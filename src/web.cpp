// Rotas:
//   GET  /               página do ranking        (web/index.html)
//   GET  /admin          página do organizador    (web/admin.html)
//   GET  /events         "Server-Sent Events": o ESP32 empurra dados ao navegador
//   POST /name           apelido do próximo jogador (campo: name)
//   POST /admin/delete   apaga uma linha          (campos: pin, id)
//   POST /admin/reset    zera o ranking           (campo: pin)
//
// Um POST aceito responde com o NÚMERO DO PEDIDO. Aceito não quer dizer feito:
// o resultado aparece depois no evento "state", nos campos "done" e "refused".
#include "web.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config_web.h"
#include "web_pages.h"   // gerado a partir de web/*.html por scripts/embed_web.py

static constexpr uint32_t MIN_INTERVAL_MS = 100;   // no máximo 10 atualizações por segundo
static constexpr uint32_t HEARTBEAT_MS = 2000;     // "estou vivo", mesmo sem novidades

static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static QueueHandle_t commandQueue = nullptr;   // pedidos do navegador -> jogo
static SemaphoreHandle_t dataMutex = nullptr;  // protege as duas cópias abaixo
static GameSnapshot sharedState{};
static Ranking sharedRanking{};
static bool running = false;

// ---------------------------------------------------------------- JSON -----
// Os apelidos só têm A-Z, 0-9 e espaço (sanitizeName), então não precisam de
// "escape" para entrar no JSON.
static String stateJson(const GameSnapshot& s) {
  static const char* const SCREEN[] = {"wait", "playing", "over"};
  char buf[300];
  snprintf(buf, sizeof(buf),
           "{\"screen\":\"%s\",\"player\":\"%s\",\"next\":\"%s\",\"score\":%lu,"
           "\"level\":%u,\"lives\":%u,\"lastRank\":%d,\"livesMax\":%u,"
           "\"done\":%lu,\"refused\":%lu}",
           SCREEN[(uint8_t)s.screen], s.player, s.nextPlayer, (unsigned long)s.score,
           s.level, s.lives, s.lastRank, s.livesMax,
           (unsigned long)s.done, (unsigned long)s.refused);
  return String(buf);
}

static String rankingJson(const Ranking& r) {
  String out = "[";
  for (uint8_t i = 0; i < r.count; i++) {
    const RankEntry& e = r.entries[i];
    char buf[96];
    snprintf(buf, sizeof(buf), "%s{\"id\":%lu,\"name\":\"%s\",\"score\":%lu,\"level\":%u}",
             i ? "," : "", (unsigned long)e.id, e.name, (unsigned long)e.score, e.level);
    out += buf;
  }
  out += "]";
  return out;
}

// -------------------------------------------------- Rotas (outra tarefa) ---
static bool isPlaying() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  bool playing = sharedState.screen == Screen::Playing;
  xSemaphoreGive(dataMutex);
  return playing;
}

static bool field(AsyncWebServerRequest* req, const char* name, String& out) {
  if (!req->hasParam(name, true)) return false;      // true = campo do corpo do POST
  out = req->getParam(name, true)->value();
  return true;
}

// Coloca o pedido na fila e responde ao navegador.
static void enqueue(AsyncWebServerRequest* req, WebCommand& c) {
  static uint32_t lastTicket = 0;   // só a tarefa de rede usa
  if (isPlaying()) { req->send(409, "text/plain", "jogo em andamento"); return; }
  c.ticket = lastTicket + 1;
  if (xQueueSend(commandQueue, &c, 0) != pdTRUE) { req->send(503, "text/plain", "ocupado"); return; }
  lastTicket = c.ticket;
  req->send(200, "text/plain", String(c.ticket));
}

static void handleName(AsyncWebServerRequest* req) {
  String name;
  if (!field(req, "name", name)) { req->send(400, "text/plain", "falta name"); return; }
  WebCommand c{};
  c.type = WebCommand::SetName;
  sanitizeName(name.c_str(), c.name);   // nunca confie no que vem do navegador
  enqueue(req, c);
}

static bool checkPin(AsyncWebServerRequest* req) {
  String pin;
  if (!field(req, "pin", pin)) { req->send(400, "text/plain", "falta pin"); return false; }
  if (pin != ADMIN_PIN) { req->send(403, "text/plain", "PIN errado"); return false; }
  return true;
}

static void handleDelete(AsyncWebServerRequest* req) {
  if (!checkPin(req)) return;
  String id;
  WebCommand c{};
  c.type = WebCommand::Delete;
  if (field(req, "id", id)) c.id = strtoul(id.c_str(), nullptr, 10);
  if (c.id == 0) { req->send(400, "text/plain", "falta id"); return; }
  enqueue(req, c);
}

static void handleReset(AsyncWebServerRequest* req) {
  if (!checkPin(req)) return;
  WebCommand c{};
  c.type = WebCommand::Reset;
  enqueue(req, c);
}

// Navegador acabou de conectar (ou reconectar): manda tudo.
static void handleConnect(AsyncEventSourceClient* client) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  GameSnapshot s = sharedState;
  Ranking r = sharedRanking;
  xSemaphoreGive(dataMutex);
  client->send(rankingJson(r).c_str(), "ranking");
  client->send(stateJson(s).c_str(), "state", 0, 1000);   // 1000 ms = tempo p/ reconectar
}

// Envia uma página DIRETO da flash. Passar o tamanho e o tipo uint8_t* faz a
// biblioteca ler aos pedaços, sem copiar a página inteira para a memória RAM.
static void sendPage(AsyncWebServerRequest* req, const char* page, size_t len) {
  req->send(200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(page), len);
}

// -------------------------------------------------- Chamadas pelo jogo -----
bool webBegin() {
  commandQueue = xQueueCreate(8, sizeof(WebCommand));
  dataMutex = xSemaphoreCreateMutex();
  if (!commandQueue || !dataMutex) { Serial.println("Web: sem memoria"); return false; }

  WiFi.mode(WIFI_AP);                              // o ESP32 CRIA a rede
  if (!WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
    Serial.println("Web: falha ao criar a rede Wi-Fi");
    return false;
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    sendPage(req, PAGE_INDEX, sizeof(PAGE_INDEX) - 1);
  });
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest* req) {
    sendPage(req, PAGE_ADMIN, sizeof(PAGE_ADMIN) - 1);
  });
  server.on("/name", HTTP_POST, handleName);
  server.on("/admin/delete", HTTP_POST, handleDelete);
  server.on("/admin/reset", HTTP_POST, handleReset);
  events.onConnect(handleConnect);
  server.addHandler(&events);
  server.begin();

  running = true;
  Serial.printf("Web: rede \"%s\", abra http://%s\n", WIFI_SSID,
                WiFi.softAPIP().toString().c_str());
  return true;
}

// Compara campo a campo (memcmp não serve: a struct pode ter bytes "de enchimento").
static bool sameState(const GameSnapshot& a, const GameSnapshot& b) {
  return a.screen == b.screen && a.score == b.score && a.level == b.level &&
         a.lives == b.lives && a.lastRank == b.lastRank &&
         a.livesMax == b.livesMax && a.done == b.done && a.refused == b.refused &&
         strcmp(a.player, b.player) == 0 && strcmp(a.nextPlayer, b.nextPlayer) == 0;
}

void webPublish(const GameSnapshot& s, const Ranking& r) {
  if (!running) return;
  static bool stateDirty = true, rankingDirty = true;
  static uint32_t lastSend = 0;

  // Só este loop() ESCREVE as cópias, então pode compará-las sem o mutex.
  if (!sameState(s, sharedState)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    sharedState = s;
    xSemaphoreGive(dataMutex);
    stateDirty = true;
  }
  // Aqui o memcmp serve, apesar dos bytes de enchimento: a cópia é feita com
  // memcpy, que copia TODOS os bytes (inclusive esses). Se o ranking não mudou,
  // os dois blocos de memória continuam idênticos.
  if (memcmp(&r, &sharedRanking, sizeof(Ranking)) != 0) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    memcpy(&sharedRanking, &r, sizeof(Ranking));
    xSemaphoreGive(dataMutex);
    rankingDirty = true;
  }

  uint32_t now = millis();
  uint32_t elapsed = now - lastSend;
  if (elapsed < MIN_INTERVAL_MS) return;
  if (!stateDirty && !rankingDirty && elapsed < HEARTBEAT_MS) return;

  lastSend = now;
  if (rankingDirty) events.send(rankingJson(sharedRanking).c_str(), "ranking");
  events.send(stateJson(sharedState).c_str(), "state");
  stateDirty = rankingDirty = false;
}

bool webPollCommand(WebCommand& c) {
  return running && xQueueReceive(commandQueue, &c, 0) == pdTRUE;
}
