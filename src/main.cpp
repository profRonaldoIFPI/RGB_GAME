// RGB GAME - "Space Invaders" de uma linha só, numa fita WS2811 + 3 botões + buzzer.
//
// Os inimigos (LEDs coloridos) descem da ponta da fita até a "base" do jogador.
// Cada botão (R, G, B) dispara um tiro; apertar botões juntos mistura as cores:
//   R+G = amarelo, G+B = ciano, R+B = magenta, R+G+B = branco.
// O tiro só destrói o inimigo se a cor for EXATAMENTE a mesma.
// A cada minuto de jogo os inimigos aceleram e novas cores aparecem.
//
// Esquema de ligação: docs/esquema.md

#include <Arduino.h>
#include <FastLED.h>

// ============================== CONFIGURAÇÃO ==============================
// --- Fita ---
constexpr uint16_t NUM_LEDS = 30;      // nº de pixels endereçáveis (CIs) da fita. AJUSTE!
#define LED_PIN 5                      // dado da fita (via level shifter 74HCT125)
#define LED_TYPE WS2811
#define COLOR_ORDER BRG                // WS2811 12V costuma ser BRG; se as cores saírem
                                       // trocadas, teste RGB, GRB, BRG, GBR...
constexpr uint8_t BRIGHTNESS = 100;    // 0-255

// --- Botões (entre o GPIO e GND, com INPUT_PULLUP) ---
constexpr uint8_t PIN_BTN[3] = {25, 26, 27};  // R, G, B  (bit0=R, bit1=G, bit2=B)

// --- Buzzer passivo ---
constexpr uint8_t PIN_BUZZER = 18;

// --- Regras do jogo ---
constexpr uint8_t  LIVES_MAX      = 3;
constexpr uint8_t  LIVES_LEDS     = 3;       // pixels 0..2 = vidas e linha de defesa
constexpr uint32_t LEVEL_MS       = 60000;   // a cada 1 min acelera
constexpr uint32_t BASE_STEP_MS   = 700;     // tempo por passo dos inimigos no nível 0
constexpr uint32_t MIN_STEP_MS    = 120;     // limite de velocidade
constexpr float    SPEEDUP        = 0.75f;   // cada nível multiplica o tempo por isto
constexpr uint32_t SHOT_STEP_MS   = 18;      // velocidade do tiro
constexpr uint32_t COMBO_MS       = 90;      // janela para apertar botões "juntos"
constexpr uint32_t DEBOUNCE_MS    = 15;
constexpr uint32_t FRAME_MS       = 10;
constexpr uint32_t GAMEOVER_MS    = 3000;
// ==========================================================================

constexpr uint8_t MAX_ENEMIES = 16;
constexpr uint8_t MAX_SHOTS   = 8;
constexpr uint8_t NOTE_QUEUE  = 12;

CRGB leds[NUM_LEDS];

// ------------------------------ Tipos -------------------------------------
struct Enemy { bool active; int16_t pos; uint8_t mask; };
struct Shot  { bool active; int16_t pos; uint8_t mask; };
struct Note  { uint16_t freq; uint16_t ms; };   // freq 0 = pausa

enum State { ATTRACT, PLAYING, GAMEOVER };

// ------------------------------ Estado ------------------------------------
State state = ATTRACT;
Enemy enemies[MAX_ENEMIES];
Shot  shots[MAX_SHOTS];

uint8_t  lives, level;
uint32_t score, gameStart, stateSince;
uint32_t lastEnemyStep, lastShotStep, lastFrame;
uint8_t  spawnCountdown;

int16_t  flashPos = -1;  uint32_t flashUntil = 0;     // efeito ao destruir inimigo
uint32_t levelFlashUntil = 0, hurtFlashUntil = 0;

// botões
uint8_t  btnStable = 0, btnRaw = 0;
uint32_t btnChangedAt[3] = {0, 0, 0};
bool     comboOpen = false, armed = true;
uint32_t comboStart = 0;
uint8_t  comboMask = 0;

// som
Note     noteQ[NOTE_QUEUE];
uint8_t  noteLen = 0, noteIdx = 0;
uint32_t noteEnd = 0;
bool     noteOn = false;

// ------------------------------ Cores -------------------------------------
CRGB colorOf(uint8_t mask, uint8_t v = 255) {
  return CRGB((mask & 1) ? v : 0, (mask & 2) ? v : 0, (mask & 4) ? v : 0);
}

// ------------------------------ Som ---------------------------------------
void toneStart(uint16_t f) {
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

// Toca uma sequência sem travar o jogo (substitui a que estiver tocando).
void play(const Note* seq, uint8_t n) {
  if (n > NOTE_QUEUE) n = NOTE_QUEUE;
  for (uint8_t i = 0; i < n; i++) noteQ[i] = seq[i];
  noteLen = n; noteIdx = 0; noteOn = false;
  noteEnd = 0;  // força início imediato em soundUpdate()
}

void soundUpdate() {
  uint32_t now = millis();
  if (noteOn && (int32_t)(now - noteEnd) < 0) return;
  if (noteIdx < noteLen) {
    Note& n = noteQ[noteIdx++];
    toneStart(n.freq);
    noteEnd = now + n.ms;
    noteOn = true;
  } else if (noteOn) {
    toneStart(0);
    noteOn = false;
  }
}

// Efeitos sonoros
void sndShot()   { static const Note s[] = {{1800, 15}, {1200, 15}}; play(s, 2); }
void sndHit()    { static const Note s[] = {{880, 30}, {1320, 40}, {1760, 50}}; play(s, 3); }
void sndMiss()   { static const Note s[] = {{200, 60}}; play(s, 1); }
void sndHurt()   { static const Note s[] = {{400, 120}, {250, 120}, {150, 220}}; play(s, 3); }
void sndLevel()  { static const Note s[] = {{523, 90}, {659, 90}, {784, 90}, {1047, 220}}; play(s, 4); }
void sndStart()  { static const Note s[] = {{523, 100}, {784, 100}, {1047, 200}}; play(s, 3); }
void sndOver()   {
  static const Note s[] = {{523, 200}, {466, 200}, {392, 200}, {262, 500}};
  play(s, 4);
}

// ------------------------------ Botões ------------------------------------
// Lê os 3 botões com debounce. Retorna máscara (bit0=R, bit1=G, bit2=B).
uint8_t readButtons() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    bool pressed = digitalRead(PIN_BTN[i]) == LOW;
    bool cur = (btnRaw >> i) & 1;
    if (pressed != cur) {
      btnRaw ^= (1 << i);
      btnChangedAt[i] = now;
    } else if (((btnStable >> i) & 1) != pressed && now - btnChangedAt[i] >= DEBOUNCE_MS) {
      btnStable ^= (1 << i);
    }
  }
  return btnStable;
}

// Retorna a máscara do disparo quando a janela de combo fecha; senão 0.
uint8_t pollFire() {
  uint8_t now = readButtons();
  if (!armed) {                     // espera soltar tudo antes de novo disparo
    if (now == 0) armed = true;
    return 0;
  }
  if (!comboOpen) {
    if (now) { comboOpen = true; comboStart = millis(); comboMask = now; }
    return 0;
  }
  comboMask |= now;
  if (millis() - comboStart >= COMBO_MS) {
    comboOpen = false;
    armed = (now == 0);
    return comboMask;
  }
  return 0;
}

// ------------------------------ Jogo --------------------------------------
uint32_t enemyStepMs() {
  float ms = BASE_STEP_MS;
  for (uint8_t i = 0; i < level; i++) ms *= SPEEDUP;
  return ms < MIN_STEP_MS ? MIN_STEP_MS : (uint32_t)ms;
}

uint8_t randomMask() {
  if (level == 0) return 1 << random(3);           // R, G ou B
  static const uint8_t pool2[] = {1, 2, 4, 3, 5, 6};
  static const uint8_t pool3[] = {1, 2, 4, 3, 5, 6, 7};
  if (level == 1) return pool2[random(6)];         // + combinações de 2 cores
  return pool3[random(7)];                         // + branco (R+G+B)
}

void resetState(State s) { state = s; stateSince = millis(); }

void startGame() {
  memset(enemies, 0, sizeof(enemies));
  memset(shots, 0, sizeof(shots));
  lives = LIVES_MAX; level = 0; score = 0;
  gameStart = lastEnemyStep = lastShotStep = millis();
  spawnCountdown = 3;
  flashPos = -1; levelFlashUntil = hurtFlashUntil = 0;
  sndStart();
  Serial.println("== Novo jogo ==");
  resetState(PLAYING);
}

void fireShot(uint8_t mask) {
  for (auto& s : shots) {
    if (!s.active) {
      s = {true, (int16_t)(LIVES_LEDS), mask};
      sndShot();
      return;
    }
  }
}

void spawnEnemy() {
  for (auto& e : enemies) {
    if (!e.active) { e = {true, (int16_t)(NUM_LEDS - 1), randomMask()}; return; }
  }
}

void loseLife() {
  if (lives) lives--;
  hurtFlashUntil = millis() + 250;
  sndHurt();
  Serial.printf("Vida perdida! Restam %u\n", lives);
  if (lives == 0) {
    Serial.printf("GAME OVER - pontos: %lu (nivel %u)\n", (unsigned long)score, level);
    sndOver();
    resetState(GAMEOVER);
  }
}

void updateEnemies(uint32_t now) {
  if (now - lastEnemyStep < enemyStepMs()) return;
  lastEnemyStep = now;

  for (auto& e : enemies) {
    if (!e.active) continue;
    e.pos--;
    if (e.pos < (int16_t)LIVES_LEDS) {   // chegou na base
      e.active = false;
      loseLife();
      if (state != PLAYING) return;
    }
  }

  if (spawnCountdown > 0) spawnCountdown--;
  if (spawnCountdown == 0) {
    spawnEnemy();
    uint8_t lo = level >= 2 ? 2 : 4 - level;
    uint8_t hi = level >= 4 ? 3 : 6 - level;
    spawnCountdown = random(lo, hi + 1);
  }
}

void updateShots(uint32_t now) {
  if (now - lastShotStep >= SHOT_STEP_MS) {
    lastShotStep = now;
    for (auto& s : shots) {
      if (!s.active) continue;
      s.pos++;
      if (s.pos >= (int16_t)NUM_LEDS) s.active = false;  // saiu da fita
    }
  }
  // colisões (a cada quadro, pois inimigos também se movem)
  for (auto& s : shots) {
    if (!s.active) continue;
    Enemy* target = nullptr;
    for (auto& e : enemies)
      if (e.active && (!target || e.pos < target->pos)) target = &e;
    if (target && s.pos >= target->pos) {
      s.active = false;
      if (s.mask == target->mask) {
        uint8_t bits = (s.mask & 1) + ((s.mask >> 1) & 1) + ((s.mask >> 2) & 1);
        score += 10u * bits * (level + 1);
        flashPos = target->pos; flashUntil = now + 80;
        target->active = false;
        sndHit();
      } else {
        sndMiss();  // cor errada: o tiro se perde
      }
    }
  }
}

void updateLevel(uint32_t now) {
  uint8_t lv = (now - gameStart) / LEVEL_MS;
  if (lv > level) {
    level = lv;
    levelFlashUntil = now + 300;
    sndLevel();
    Serial.printf("NIVEL %u! passo=%lums\n", level, (unsigned long)enemyStepMs());
  }
}

// ------------------------------ Vídeo -------------------------------------
void renderLives(uint32_t now) {
  bool hurt = now < hurtFlashUntil;
  for (uint8_t i = 0; i < LIVES_LEDS; i++) {
    if (hurt)            leds[i] = CRGB(255, 0, 0);
    else if (i < lives)  leds[i] = CRGB(0, 40, 0);
    else                 leds[i] = CRGB(15, 0, 0);
  }
}

void renderPlaying(uint32_t now) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  renderLives(now);

  if (now < levelFlashUntil && ((now / 60) & 1))
    for (uint16_t i = LIVES_LEDS; i < NUM_LEDS; i++) leds[i] = CRGB(40, 40, 40);

  for (auto& e : enemies)
    if (e.active) leds[e.pos] = colorOf(e.mask);

  if (flashPos >= 0 && now < flashUntil) leds[flashPos] = CRGB::White;

  for (auto& s : shots)
    if (s.active && s.pos < (int16_t)NUM_LEDS) leds[s.pos] = colorOf(s.mask, 255);
}

void renderAttract(uint32_t now) {
  // arco-íris suave + um pixel "pulsando" no início para chamar atenção
  fill_rainbow(leds, NUM_LEDS, (now / 20) & 0xFF, 255 / NUM_LEDS);
  uint8_t pulse = beatsin8(60, 20, 255);
  leds[0] = leds[1] = leds[2] = CRGB(pulse, pulse, pulse);
}

void renderGameOver(uint32_t now) {
  bool on = ((now - stateSince) / 250) & 1;
  fill_solid(leds, NUM_LEDS, on ? CRGB(120, 0, 0) : CRGB::Black);
}

// ------------------------------ Arduino -----------------------------------
void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 3; i++) pinMode(PIN_BTN[i], INPUT_PULLUP);
  soundInit();
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  randomSeed(esp_random());
  Serial.println("RGB GAME pronto. Aperte qualquer botao para jogar.");
  resetState(ATTRACT);
}

void loop() {
  uint32_t now = millis();
  soundUpdate();

  uint8_t fire = pollFire();

  switch (state) {
    case ATTRACT:
      if (fire) startGame();
      break;

    case PLAYING:
      if (fire) fireShot(fire);
      updateLevel(now);
      updateEnemies(now);
      if (state == PLAYING) updateShots(now);
      break;

    case GAMEOVER:
      if (now - stateSince >= GAMEOVER_MS) resetState(ATTRACT);
      break;
  }

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    switch (state) {
      case ATTRACT:  renderAttract(now);  break;
      case PLAYING:  renderPlaying(now);  break;
      case GAMEOVER: renderGameOver(now); break;
    }
    FastLED.show();
  }
}
