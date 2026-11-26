// ============================================================================
//  RGB GAME - "Space Invaders" de uma linha só
//  ESP32 + fita WS2811 + 3 botões (R, G, B) + buzzer
// ============================================================================
//
//  COMO O JOGO FUNCIONA
//  --------------------
//  A fita de LED é a "pista". Numa ponta fica o jogador (os primeiros pixels);
//  na outra ponta nascem os inimigos, que são LEDs coloridos e "descem" em
//  direção ao jogador, um pixel por vez.
//
//  Cada botão dispara um tiro (um pixel de luz que sobe pela fita):
//      botão R = vermelho, botão G = verde, botão B = azul.
//  Apertando botões JUNTOS, as cores se misturam:
//      R+G = amarelo   G+B = ciano   R+B = magenta   R+G+B = branco
//  O tiro só destrói o inimigo se tiver EXATAMENTE a mesma cor dele.
//
//  A cada 1 minuto o jogo muda de "nível": os inimigos ficam mais rápidos e
//  aparecem cores novas (misturas). Se um inimigo chega na base, o jogador
//  perde uma vida. Sem vidas = fim de jogo.
//
//  COMO ESTE ARQUIVO ESTÁ ORGANIZADO (leia de cima para baixo)
//  -----------------------------------------------------------
//   1. CONFIGURAÇÃO ....... pinos e números que você pode ajustar
//   2. Tipos e variáveis .. onde o jogo guarda o que está acontecendo
//   3. Som ................ toca notas sem travar o jogo
//   4. Botões ............. leitura, debounce e detecção de "combo"
//   5. Lógica do jogo ..... inimigos, tiros, colisões, níveis
//   6. Vídeo .............. desenha tudo na fita
//   7. Placar web ......... conversa entre o jogo e a página
//   8. setup() e loop() ... o programa principal do Arduino
//
//  PLACAR NA WEB: o ESP32 cria a rede Wi-Fi "RGB-GAME"; no notebook, abra
//  http://192.168.4.1. O servidor fica em web.cpp, as páginas em web/*.html,
//  o ranking em lib/ranking e a gravação dele em ranking_store.cpp.
//
//  Esquema de ligação: docs/esquema.md   |   Guia de montagem: README.md
// ============================================================================

#include <Arduino.h>
#include <FastLED.h>   // biblioteca que controla a fita de LED
#include "ranking.h"        // Top 10 (lib/ranking)
#include "ranking_store.h"  // grava o Top 10 na memória do ESP32
#include "web.h"            // Wi-Fi e página do placar

// ============================================================================
// 1. CONFIGURAÇÃO  (as constantes abaixo são as que você mais vai mexer)
// ============================================================================

// --- Fita de LED -----------------------------------------------------------
// ATENÇÃO: NUM_LEDS é o número de PIXELS (grupos que acendem juntos), não de
// LEDs físicos. Na WS2811 12 V cada CI controla 3 LEDs. Conte quantos grupos
// a sua fita tem. O jogo foi pensado para 30 ou mais.
constexpr uint16_t NUM_LEDS = 30;

#define LED_PIN 5            // pino do ESP32 ligado ao dado (DIN) da fita
#define LED_TYPE WS2811      // modelo do chip da fita
// Ordem das cores. Se você mandar "vermelho" e a fita acender outra cor, troque
// aqui por RGB, GRB, BRG ou GBR até acertar. WS2811 12 V costuma ser BRG.
#define COLOR_ORDER BRG
constexpr uint8_t BRIGHTNESS = 100;   // brilho de 0 a 255 (menos = menos corrente)

// --- Botões ----------------------------------------------------------------
// Cada botão liga o pino ao GND. O ESP32 tem um resistor interno (INPUT_PULLUP),
// então o pino lê HIGH solto e LOW apertado.
// A posição no vetor define o "bit" da cor: [0]=R (bit0), [1]=G (bit1), [2]=B (bit2)
constexpr uint8_t PIN_BTN[3] = {25, 26, 27};

// --- Buzzer ----------------------------------------------------------------
constexpr uint8_t PIN_BUZZER = 18;    // buzzer PASSIVO (faz notas diferentes)

// --- Regras do jogo (mexa aqui para ajustar a dificuldade!) ----------------
constexpr uint8_t  LIVES_MAX      = 3;       // vidas no início
constexpr uint8_t  LIVES_LEDS     = 3;       // os pixels 0,1,2 mostram as vidas e são a "base"
constexpr uint32_t LEVEL_MS       = 60000;   // a cada 60 s o nível sobe (1 minuto)
constexpr uint32_t BASE_STEP_MS   = 700;     // no nível 0, o inimigo anda 1 pixel a cada 700 ms
constexpr uint32_t MIN_STEP_MS    = 120;     // velocidade máxima (nunca passa disso)
constexpr float    SPEEDUP        = 0.75f;   // a cada nível, o tempo por passo é multiplicado por isto
constexpr uint32_t SHOT_STEP_MS   = 18;      // o tiro anda 1 pixel a cada 18 ms (bem rápido)
constexpr uint32_t COMBO_MS       = 90;      // janela para apertar botões "ao mesmo tempo"
constexpr uint32_t DEBOUNCE_MS    = 15;      // ignora o "tremor" do contato do botão
constexpr uint32_t FRAME_MS       = 10;      // atualiza a fita a cada 10 ms (100 quadros/s)
constexpr uint32_t GAMEOVER_MS    = 3000;    // tempo da tela de fim de jogo

// ============================================================================
// 2. TIPOS E VARIÁVEIS GLOBAIS
// ============================================================================

constexpr uint8_t MAX_ENEMIES = 16;   // máximo de inimigos ao mesmo tempo
constexpr uint8_t MAX_SHOTS   = 8;    // máximo de tiros ao mesmo tempo
constexpr uint8_t NOTE_QUEUE  = 12;   // máximo de notas numa melodia

// A fita: um vetor com uma cor por pixel. Mudamos o vetor e depois
// chamamos FastLED.show() para enviar tudo para a fita.
CRGB leds[NUM_LEDS];

// "Cor" dos inimigos e tiros = uma MÁSCARA de 3 bits:
//   bit0 (valor 1) = tem vermelho
//   bit1 (valor 2) = tem verde
//   bit2 (valor 4) = tem azul
// Exemplos: 1=R, 2=G, 4=B, 3=R+G (amarelo), 6=G+B (ciano), 7=branco.
// Para saber se um tiro acerta um inimigo basta comparar: tiro.mask == inimigo.mask.

struct Enemy { bool active; int16_t pos; uint8_t mask; };  // pos = pixel onde está
struct Shot  { bool active; int16_t pos; uint8_t mask; };
struct Note  { uint16_t freq; uint16_t ms; };  // frequência em Hz (0 = silêncio) e duração

// O jogo é uma "máquina de estados": em cada momento está em um destes:
enum State {
  ATTRACT,    // tela de espera: arco-íris. Qualquer botão começa o jogo.
  PLAYING,    // jogando
  GAMEOVER    // acabaram as vidas: pisca vermelho e volta para ATTRACT
};

State state = ATTRACT;
Enemy enemies[MAX_ENEMIES];
Shot  shots[MAX_SHOTS];

uint8_t  lives, level;                 // vidas restantes e nível atual (0, 1, 2...)
uint32_t score;                        // pontos
uint32_t gameStart, stateSince;        // quando o jogo / o estado atual começou (em ms)
uint32_t lastEnemyStep, lastShotStep, lastFrame;  // última vez que cada coisa andou
uint8_t  spawnCountdown;               // quantos passos faltam para nascer o próximo inimigo

// Pequenos efeitos visuais (até que instante cada um fica ligado)
int16_t  flashPos = -1;  uint32_t flashUntil = 0;     // clarão quando um inimigo é destruído
uint32_t levelFlashUntil = 0, hurtFlashUntil = 0;     // piscadas de "subiu de nível" / "perdeu vida"

// Estado dos botões (usado pelas funções de leitura, na seção 4)
uint8_t  btnStable = 0, btnRaw = 0;    // máscara dos botões: estável (já filtrada) e crua
uint32_t btnChangedAt[3] = {0, 0, 0};  // quando cada botão mudou pela última vez
bool     comboOpen = false;            // true = estamos esperando outros botões do combo
bool     armed = true;                 // true = pode começar um novo disparo
uint32_t comboStart = 0;
uint8_t  comboMask = 0;

// Estado do som (fila de notas tocando)
Note     noteQ[NOTE_QUEUE];
uint8_t  noteLen = 0, noteIdx = 0;
uint32_t noteEnd = 0;
bool     noteOn = false;

// Placar web e ranking (veja a seção 7)
Ranking ranking;                          // o Top 10
Screen  webScreen = Screen::Wait;         // tela da PÁGINA (pode diferir do estado da fita:
                                          // o "fim de jogo" fica na página até o próximo jogador)
char    playerName[NICK_MAX + 1] = "";    // apelido de quem está jogando ("" = anônimo)
char    nextName[NICK_MAX + 1] = "";      // apelido enviado para a próxima partida
int8_t  lastRank = -1;                    // posição da última partida no Top 10, ou -1
uint32_t lastId = 0;                      // id da linha dessa partida (0 = não entrou)
bool    rankingNeedsSave = false;         // true = gravar na memória

// ============================================================================
// Utilitário de cor
// ============================================================================
// Converte a máscara (1..7) em uma cor da fita. Ex.: máscara 3 -> vermelho+verde = amarelo.
CRGB colorOf(uint8_t mask, uint8_t v = 255) {
  return CRGB((mask & 1) ? v : 0, (mask & 2) ? v : 0, (mask & 4) ? v : 0);
}

// ============================================================================
// 3. SOM
// ============================================================================
// O buzzer passivo toca a frequência que mandarmos (como um alto-falante).
// Para não travar o jogo, NUNCA usamos delay() para tocar notas:
// guardamos a melodia numa fila e, a cada volta do loop(), soundUpdate()
// confere se já é hora de passar para a próxima nota.

// Liga o buzzer na frequência f (em Hz). f = 0 desliga.
void toneStart(uint16_t f) {
  // O ESP32 gera a onda com o periférico LEDC. A API mudou entre as versões 2.x e 3.x
  // do núcleo Arduino-ESP32, por isso há dois caminhos.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(PIN_BUZZER, f);
#else
  ledcWriteTone(0, f);
#endif
}

void soundInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BUZZER, 2000, 8);
#else
  ledcSetup(0, 2000, 8);
  ledcAttachPin(PIN_BUZZER, 0);
#endif
  toneStart(0);
}

// Começa a tocar uma sequência de notas. Se já havia algo tocando, é substituído.
void play(const Note* seq, uint8_t n) {
  if (n > NOTE_QUEUE) n = NOTE_QUEUE;
  for (uint8_t i = 0; i < n; i++) noteQ[i] = seq[i];
  noteLen = n; noteIdx = 0; noteOn = false;
  noteEnd = 0;  // faz soundUpdate() iniciar a primeira nota imediatamente
}

// Chamada em todo loop(): avança para a próxima nota quando a atual termina.
void soundUpdate() {
  uint32_t now = millis();
  if (noteOn && (int32_t)(now - noteEnd) < 0) return;   // nota ainda tocando
  if (noteIdx < noteLen) {                              // há mais notas: toca a próxima
    Note& n = noteQ[noteIdx++];
    toneStart(n.freq);
    noteEnd = now + n.ms;
    noteOn = true;
  } else if (noteOn) {                                  // acabou a melodia: silêncio
    toneStart(0);
    noteOn = false;
  }
}

// Efeitos sonoros do jogo: {frequência em Hz, duração em ms}.
// Quer mudar um som? Basta editar os números!
void sndShot()   { static const Note s[] = {{1800, 15}, {1200, 15}}; play(s, 2); }            // "piu" curto
void sndHit()    { static const Note s[] = {{880, 30}, {1320, 40}, {1760, 50}}; play(s, 3); } // acertou (sobe)
void sndMiss()   { static const Note s[] = {{200, 60}}; play(s, 1); }                         // cor errada (grave)
void sndHurt()   { static const Note s[] = {{400, 120}, {250, 120}, {150, 220}}; play(s, 3); }// perdeu vida (desce)
void sndLevel()  { static const Note s[] = {{523, 90}, {659, 90}, {784, 90}, {1047, 220}}; play(s, 4); } // subiu de nível
void sndStart()  { static const Note s[] = {{523, 100}, {784, 100}, {1047, 200}}; play(s, 3); }         // início
void sndOver()   {
  static const Note s[] = {{523, 200}, {466, 200}, {392, 200}, {262, 500}};                   // fim de jogo
  play(s, 4);
}

// ============================================================================
// 4. BOTÕES
// ============================================================================

// Lê os 3 botões e filtra o "tremor" (debounce): um botão só é considerado
// apertado/solto depois de ficar no mesmo valor por DEBOUNCE_MS.
// Retorna uma máscara: bit0=R, bit1=G, bit2=B (1 = apertado).
uint8_t readButtons() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    bool pressed = digitalRead(PIN_BTN[i]) == LOW;      // LOW = apertado (pull-up)
    bool cur = (btnRaw >> i) & 1;
    if (pressed != cur) {                               // o valor mudou: reinicia o relógio
      btnRaw ^= (1 << i);
      btnChangedAt[i] = now;
    } else if (((btnStable >> i) & 1) != pressed && now - btnChangedAt[i] >= DEBOUNCE_MS) {
      btnStable ^= (1 << i);                            // ficou estável: aceita a mudança
    }
  }
  return btnStable;
}

// Detecta o DISPARO. Ninguém aperta dois botões no mesmo milissegundo, então:
//   1) quando o primeiro botão é apertado, abrimos uma "janela" de COMBO_MS;
//   2) durante a janela, juntamos todos os botões apertados;
//   3) quando a janela fecha, devolvemos a máscara (ex.: R+G = 3) -> um tiro.
// Depois do tiro, só aceitamos novo disparo quando TODOS os botões forem soltos
// (isso evita tiros repetidos se o jogador segurar o botão).
// Retorna 0 se não há disparo agora, ou a máscara do tiro.
uint8_t pollFire() {
  uint8_t now = readButtons();
  if (!armed) {                          // esperando soltar tudo
    if (now == 0) armed = true;
    return 0;
  }
  if (!comboOpen) {                      // nenhuma janela aberta
    if (now) { comboOpen = true; comboStart = millis(); comboMask = now; }
    return 0;
  }
  comboMask |= now;                      // janela aberta: acumula botões
  if (millis() - comboStart >= COMBO_MS) {
    comboOpen = false;
    armed = (now == 0);
    return comboMask;                    // janela fechou: dispara!
  }
  return 0;
}

// ============================================================================
// 5. LÓGICA DO JOGO
// ============================================================================

// Quanto tempo o inimigo leva para andar 1 pixel no nível atual.
// Nível 0: 700 ms; nível 1: 525 ms; nível 2: 394 ms... (x0.75 a cada nível), mínimo 120 ms.
uint32_t enemyStepMs() {
  float ms = BASE_STEP_MS;
  for (uint8_t i = 0; i < level; i++) ms *= SPEEDUP;
  return ms < MIN_STEP_MS ? MIN_STEP_MS : (uint32_t)ms;
}

// Sorteia a cor de um novo inimigo conforme o nível:
//   nível 0: só R, G, B       nível 1: + misturas de 2 cores       nível 2+: + branco
uint8_t randomMask() {
  if (level == 0) return 1 << random(3);              // 1, 2 ou 4
  static const uint8_t pool2[] = {1, 2, 4, 3, 5, 6};
  static const uint8_t pool3[] = {1, 2, 4, 3, 5, 6, 7};
  if (level == 1) return pool2[random(6)];
  return pool3[random(7)];
}

// Muda de estado e anota a hora (usada para contar o tempo no estado).
void resetState(State s) { state = s; stateSince = millis(); }

// Zera tudo e começa uma partida nova.
void startGame() {
  memset(enemies, 0, sizeof(enemies));
  memset(shots, 0, sizeof(shots));
  lives = LIVES_MAX; level = 0; score = 0;
  gameStart = lastEnemyStep = lastShotStep = millis();
  spawnCountdown = 3;                         // o primeiro inimigo nasce após 3 passos
  flashPos = -1; levelFlashUntil = hurtFlashUntil = 0;
  // O apelido enviado pela página vale só para ESTA partida.
  strcpy(playerName, nextName);
  nextName[0] = '\0';
  lastRank = -1;
  lastId = 0;
  webScreen = Screen::Playing;
  sndStart();
  Serial.println("== Novo jogo ==");
  resetState(PLAYING);
}

// Cria um tiro na base (primeiro pixel depois das vidas), subindo pela fita.
void fireShot(uint8_t mask) {
  for (auto& s : shots) {
    if (!s.active) {                // usa o primeiro "espaço" livre do vetor
      s = {true, (int16_t)(LIVES_LEDS), mask};
      sndShot();
      return;
    }
  }
}

// Cria um inimigo na ponta final da fita, com cor sorteada.
void spawnEnemy() {
  for (auto& e : enemies) {
    if (!e.active) { e = {true, (int16_t)(NUM_LEDS - 1), randomMask()}; return; }
  }
}

// Fim de partida: coloca o resultado no ranking (se couber).
void finishGame() {
  Serial.printf("GAME OVER - pontos: %lu (nivel %u)\n", (unsigned long)score, level);
  lastRank = rankingInsert(ranking, playerName, score, level);
  lastId = 0;
  if (lastRank >= 0) {
    lastId = ranking.entries[lastRank].id;
    rankingNeedsSave = true;                    // grava depois, no loop()
  }
  webScreen = Screen::Over;
  sndOver();
  resetState(GAMEOVER);
}

// Chamada quando um inimigo chega na base.
void loseLife() {
  if (lives) lives--;
  hurtFlashUntil = millis() + 250;    // pisca as vidas em vermelho por 250 ms
  sndHurt();
  Serial.printf("Vida perdida! Restam %u\n", lives);
  if (lives == 0) finishGame();
}

// Faz os inimigos andarem 1 pixel (quando chega a hora) e cria novos.
void updateEnemies(uint32_t now) {
  if (now - lastEnemyStep < enemyStepMs()) return;   // ainda não é hora de andar
  lastEnemyStep = now;

  for (auto& e : enemies) {
    if (!e.active) continue;
    e.pos--;                                   // desce 1 pixel (em direção à base)
    if (e.pos < (int16_t)LIVES_LEDS) {         // passou da linha de defesa
      e.active = false;
      loseLife();
      if (state != PLAYING) return;            // se o jogo acabou, para por aqui
    }
  }

  // Contagem regressiva para o próximo inimigo. O intervalo (em passos) fica
  // menor em níveis altos, então os inimigos vêm mais juntos.
  if (spawnCountdown > 0) spawnCountdown--;
  if (spawnCountdown == 0) {
    spawnEnemy();
    uint8_t lo = level >= 2 ? 2 : 4 - level;   // mínimo de passos até o próximo
    uint8_t hi = level >= 4 ? 3 : 6 - level;   // máximo de passos até o próximo
    spawnCountdown = random(lo, hi + 1);
  }
}

// Faz os tiros subirem e verifica se acertaram algum inimigo.
void updateShots(uint32_t now) {
  // 1) Movimento: cada tiro sobe 1 pixel a cada SHOT_STEP_MS.
  if (now - lastShotStep >= SHOT_STEP_MS) {
    lastShotStep = now;
    for (auto& s : shots) {
      if (!s.active) continue;
      s.pos++;
      if (s.pos >= (int16_t)NUM_LEDS) s.active = false;  // saiu da fita: perdeu
    }
  }

  // 2) Colisão (a cada volta, pois os inimigos também se mexem).
  // Cada tiro só pode atingir o inimigo MAIS PRÓXIMO da base (o de menor posição).
  for (auto& s : shots) {
    if (!s.active) continue;
    Enemy* target = nullptr;
    for (auto& e : enemies)
      if (e.active && (!target || e.pos < target->pos)) target = &e;

    // O tiro sobe e o inimigo desce: se o tiro alcançou ou passou a posição dele, bateu.
    if (target && s.pos >= target->pos) {
      s.active = false;                            // o tiro some em qualquer caso
      if (s.mask == target->mask) {                // MESMA cor: acertou!
        uint8_t bits = (s.mask & 1) + ((s.mask >> 1) & 1) + ((s.mask >> 2) & 1);
        score += 10u * bits * (level + 1);         // mais cores e nível maior = mais pontos
        flashPos = target->pos; flashUntil = now + 80;
        target->active = false;
        sndHit();
      } else {                                     // cor diferente: o tiro se perde
        sndMiss();
      }
    }
  }
}

// Verifica se passou 1 minuto e sobe de nível.
void updateLevel(uint32_t now) {
  uint8_t lv = (now - gameStart) / LEVEL_MS;       // quantos minutos já se passaram
  if (lv > level) {
    level = lv;
    levelFlashUntil = now + 300;
    sndLevel();
    Serial.printf("NIVEL %u! passo=%lums\n", level, (unsigned long)enemyStepMs());
  }
}

// ============================================================================
// 6. VÍDEO  (desenhar na fita)
// ============================================================================
// Regra: cada função apenas PREENCHE o vetor leds[]. Quem envia para a fita
// de verdade é o FastLED.show(), chamado uma vez por quadro no loop().

// Desenha as vidas nos primeiros pixels: verde = vida, vermelho fraco = perdida.
void renderLives(uint32_t now) {
  bool hurt = now < hurtFlashUntil;
  for (uint8_t i = 0; i < LIVES_LEDS; i++) {
    if (hurt)            leds[i] = CRGB(255, 0, 0);   // acabou de perder vida: vermelho forte
    else if (i < lives)  leds[i] = CRGB(0, 40, 0);
    else                 leds[i] = CRGB(15, 0, 0);
  }
}

void renderPlaying(uint32_t now) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);   // apaga tudo
  renderLives(now);

  // Piscada branca fraca na pista ao subir de nível
  if (now < levelFlashUntil && ((now / 60) & 1))
    for (uint16_t i = LIVES_LEDS; i < NUM_LEDS; i++) leds[i] = CRGB(40, 40, 40);

  // Inimigos (a ordem importa: o que for desenhado depois fica por cima)
  for (auto& e : enemies)
    if (e.active) leds[e.pos] = colorOf(e.mask);

  // Clarão branco onde um inimigo acabou de ser destruído
  if (flashPos >= 0 && now < flashUntil) leds[flashPos] = CRGB::White;

  // Tiros por último, para aparecerem na frente
  for (auto& s : shots)
    if (s.active && s.pos < (int16_t)NUM_LEDS) leds[s.pos] = colorOf(s.mask, 255);
}

// Tela de espera: arco-íris que se move e 3 pixels brancos pulsando na base.
void renderAttract(uint32_t now) {
  fill_rainbow(leds, NUM_LEDS, (now / 20) & 0xFF, 255 / NUM_LEDS);
  uint8_t pulse = beatsin8(60, 20, 255);
  leds[0] = leds[1] = leds[2] = CRGB(pulse, pulse, pulse);
}

// Fim de jogo: a fita inteira pisca em vermelho.
void renderGameOver(uint32_t now) {
  bool on = ((now - stateSince) / 250) & 1;
  fill_solid(leds, NUM_LEDS, on ? CRGB(120, 0, 0) : CRGB::Black);
}

// ============================================================================
// 7. PLACAR WEB
// ============================================================================
// O navegador só PEDE; quem decide e altera os dados é sempre o jogo, aqui.

// Atende os pedidos que chegaram pela página.
void handleWebCommands() {
  WebCommand c;
  while (webPollCommand(c)) {
    // Durante a partida nada muda. O servidor já recusa, mas o pedido pode ter
    // entrado na fila um instante antes de a partida começar.
    if (state == PLAYING) continue;

    switch (c.type) {
      case WebCommand::SetName:
        sanitizeName(c.name, nextName);
        if (webScreen == Screen::Over) webScreen = Screen::Wait;
        break;
      case WebCommand::Delete:
        if (rankingRemove(ranking, c.id)) {
          rankingNeedsSave = true;
          lastRank = rankingPositionOf(ranking, lastId);  // a linha do jogador pode ter subido
        }
        break;
      case WebCommand::Reset:
        rankingClear(ranking);
        rankingNeedsSave = true;
        lastRank = -1;
        break;
    }
  }
}

// Entrega à parte web uma "foto" do jogo. Ela só envia quando algo muda.
void publishToWeb() {
  GameSnapshot s{};
  s.screen = webScreen;
  strcpy(s.player, playerName);
  strcpy(s.nextPlayer, nextName);
  s.score = score;
  s.level = level;
  s.lives = lives;
  s.lastRank = lastRank;
  webPublish(s, ranking);
}

// ============================================================================
// 8. PROGRAMA PRINCIPAL
// ============================================================================

// setup() roda UMA vez quando o ESP32 liga.
void setup() {
  Serial.begin(115200);                       // abra o Monitor Serial a 115200 para ver os logs
  for (uint8_t i = 0; i < 3; i++) pinMode(PIN_BTN[i], INPUT_PULLUP);
  soundInit();
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  randomSeed(esp_random());                   // para os inimigos não saírem sempre iguais
  storeLoad(ranking);                         // recupera o Top 10 salvo
  if (!webBegin()) Serial.println("Sem placar web; o jogo funciona normalmente.");
  Serial.println("RGB GAME pronto. Aperte qualquer botao para jogar.");
  resetState(ATTRACT);
}

// loop() roda para sempre, milhares de vezes por segundo.
// Ele NÃO usa delay(): cada coisa (inimigos, tiros, som, vídeo) confere se já é
// a hora dela agir comparando com millis(). Assim tudo acontece "ao mesmo tempo".
void loop() {
  uint32_t now = millis();
  soundUpdate();                  // mantém a melodia andando

  uint8_t fire = pollFire();      // 0 = nenhum disparo; senão, a máscara de cor do tiro

  // Antes da lógica: assim um apelido recém-enviado vale para a partida que começar agora.
  handleWebCommands();

  // Lógica de cada estado
  switch (state) {
    case ATTRACT:
      if (fire) startGame();      // qualquer botão começa a partida
      break;

    case PLAYING:
      if (fire) fireShot(fire);
      updateLevel(now);
      updateEnemies(now);
      if (state == PLAYING) updateShots(now);   // updateEnemies pode ter terminado o jogo
      break;

    case GAMEOVER:
      if (now - stateSince >= GAMEOVER_MS) resetState(ATTRACT);
      break;
  }

  // Desenho: a cada FRAME_MS desenha o quadro e envia para a fita.
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    switch (state) {
      case ATTRACT:  renderAttract(now);  break;
      case PLAYING:  renderPlaying(now);  break;
      case GAMEOVER: renderGameOver(now); break;
    }
    FastLED.show();

    // Gravar na memória demora alguns milissegundos, por isso só quando o
    // ranking muda (fim de partida ou organizador), nunca durante o jogo.
    if (rankingNeedsSave && state != PLAYING) {
      rankingNeedsSave = false;
      if (!storeSave(ranking)) Serial.println("ERRO: nao foi possivel salvar o ranking");
    }
  }

  publishToWeb();
}
