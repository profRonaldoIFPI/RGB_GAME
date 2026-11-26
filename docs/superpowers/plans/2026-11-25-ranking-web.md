# Ranking Web Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O ESP32 serve uma página com ranking de fliperama, atualizada em tempo real, para o notebook do estande.

**Architecture:** A lógica do ranking é C++ puro em `lib/ranking`, testada no computador. O loop do jogo é o único dono do estado e do ranking; o servidor (outra tarefa) só enfileira comandos e envia cópias do estado por SSE.

**Tech Stack:** PlatformIO, Arduino-ESP32 2.x, FastLED, `ESP32Async/ESPAsyncWebServer@^3.7.0`, `ESP32Async/AsyncTCP@^3.3.0`, `Preferences`, Unity (env `native`).

**Spec:** `docs/superpowers/specs/2026-11-25-ranking-web-design.md`

## Global Constraints

- Sem internet: HTML, CSS e JS embutidos no firmware; nenhum recurso externo.
- O loop do jogo continua sem `delay()`; o jogo funciona sem navegador e mesmo se `webBegin()` falhar.
- Rede `RGB-GAME`, WPA2 (senha padrão `arcade2026`), PIN padrão `1234`, endereço `http://192.168.4.1`.
- Apelido: até 10 caracteres, `A-Z`, `0-9` e espaço; anônimo aparece como `---`.
- Top 10; pontuação zero não entra; no empate, o mais antigo fica acima.
- Nomes só entram na página via `textContent`.
- Comentários e textos em português, didáticos (o leitor é um aluno).

## Review Focus

1. Apelido com acentos, HTML ou longo demais → limpo e cortado, nunca interpretado (Task 1; Task 3 remove acentos antes de enviar).
2. NVS vazia ou corrompida → ranking vazio, sem travar (Task 1: `rankingIsValid`; Task 2 usa).
3. Apagar um id que já saiu da lista → nada é removido (Task 1).
4. Zerar e inserir de novo → ids não se repetem, para um `/admin` desatualizado não apagar a linha errada (Task 1).
5. Apelido ou comando de admin que chega quando a partida acabou de começar → o loop recusa, mesmo que a rota tenha aceitado (Task 4; conferido na revisão do código e na bancada).

---

### Task 1: Biblioteca do ranking

**Files:**
- Create: `lib/ranking/ranking.h`, `lib/ranking/ranking.cpp`, `test/test_ranking/test_main.cpp`
- Modify: `platformio.ini` (adicionar `[env:native]` com `platform = native`, `test_framework = unity`; e `[platformio] default_envs = esp32doit-devkit-v1`)

**Interfaces — Produces:**
```cpp
constexpr uint8_t RANKING_SIZE = 10;
constexpr uint8_t NAME_MAX = 10;
struct RankEntry { uint32_t id; char name[NAME_MAX + 1]; uint32_t score; uint8_t level; };
struct Ranking { uint32_t nextId; uint8_t count; RankEntry entries[RANKING_SIZE]; };
void rankingClear(Ranking& r);                 // count = 0; preserva nextId
int  rankingInsert(Ranking& r, const char* name, uint32_t score, uint8_t level); // posição 0..9, ou -1
bool rankingRemove(Ranking& r, uint32_t id);
bool rankingIsValid(const Ranking& r);         // count<=10, nomes terminados e limpos, pontos >0 em ordem não crescente, ids != 0 e < nextId
void sanitizeName(const char* in, char out[NAME_MAX + 1]); // maiúsculas, filtra, apara espaços, corta em 10
```
Ids começam em 1. Nome vazio é guardado como `""` (a página mostra `---`).

- [ ] **Step 1: Escrever os testes.** Cada teste parte de `Ranking r{};`.

| Teste | Asserções |
|---|---|
| `insere_em_ordem` | inserir 100, 300, 200 → posições 0, 0, 1; pontos `300,200,100` |
| `empate_antigo_fica_acima` | "A" 100, "B" 100 → B retorna 1 |
| `zero_nao_entra` | retorna -1, `count == 0` |
| `lista_cheia` | com 100..1000: inserir 50 → -1; inserir 100 → -1; inserir 550 → 5; `count == 10`; último = 200 |
| `remove_por_id` | remove o do meio, os demais sobem; repetir o mesmo id → `false`, `count` igual |
| `ids_nao_se_repetem` | inserir, `rankingClear`, inserir → id novo > antigo |
| `limpa_apelido` | `"  ana 2 "`→`"ANA 2"`; `"<b>x</b>"`→`"BXB"`; `"abcdefghijklmn"`→`"ABCDEFGHIJ"`; `"JOÃO"`→`"JOO"`; `"!!!"`→`""` |
| `valida_dados` | zerado → `true`; ranking após inserções → `true`; `count = 200` → `false`; nome sem `\0` → `false`; bytes todos `0xFF` → `false` |

- [ ] **Step 2:** `~/.platformio/penv/bin/pio test -e native` → FALHA (funções inexistentes).
- [ ] **Step 3:** Implementar as funções.
- [ ] **Step 4:** `pio test -e native` → 8 testes PASS.
- [ ] **Step 5:** Commit `feat: logica do ranking com testes`.

### Task 2: Persistência, Wi-Fi e rotas

**Files:**
- Create: `src/config_web.h`, `src/ranking_store.h`, `src/ranking_store.cpp`, `src/web.h`, `src/web.cpp`, `web/index.html`, `web/admin.html` (nesta tarefa, páginas mínimas), `scripts/embed_web.py`, `test/test_embed_web.py`
- Modify: `platformio.ini` (`lib_deps` das duas bibliotecas; `build_flags = -DCONFIG_ASYNC_TCP_RUNNING_CORE=0`; `extra_scripts = pre:scripts/embed_web.py`), `.gitignore` (`include/web_pages.h`)

**Interfaces — Consumes:** Task 1.
**Produces:**
```cpp
// config_web.h
constexpr char WIFI_SSID[] = "RGB-GAME";
constexpr char WIFI_PASS[] = "arcade2026";   // static_assert: 8 caracteres ou mais
constexpr char ADMIN_PIN[] = "1234";
// ranking_store.h  (namespace NVS "rgbgame", chave "rank", blob {magic 0x52474231, Ranking})
void storeLoad(Ranking& r);        // ausente, tamanho/magic errado ou inválido -> r zerado, nextId = 1
bool storeSave(const Ranking& r);
// web.h
enum class Screen : uint8_t { Wait, Playing, Over };
struct GameSnapshot { Screen screen; char player[NAME_MAX+1]; char nextPlayer[NAME_MAX+1];
                      uint32_t score; uint8_t level, lives; int8_t lastRank; };
struct WebCommand { enum Type : uint8_t { SetName, Delete, Reset } type; char name[NAME_MAX+1]; uint32_t id; };
bool webBegin();
void webPublish(const GameSnapshot& s, const Ranking& r);  // chamar a cada volta do loop
bool webPollCommand(WebCommand& c);                        // não bloqueia
// include/web_pages.h (GERADO): const char PAGE_INDEX[] PROGMEM, PAGE_ADMIN[] PROGMEM
# scripts/embed_web.py
def render_header(pages: dict[str, str]) -> str   # {"PAGE_INDEX": html}; raw string; erro se o HTML contiver o delimitador
def generate(root: Path) -> bool                  # lê web/*.html, grava include/web_pages.h só se mudou; True se gravou
```

Contrato HTTP/SSE:
- `POST /name` (form `name`): 200; 409 se `Playing`; 400 sem o campo; 503 fila cheia.
- `POST /admin/delete` (`pin`, `id`) e `POST /admin/reset` (`pin`): 200; 403 PIN errado; 409 se `Playing`; 400 campo ausente; 503 fila cheia.
- Evento `state`: `{"screen":"wait|playing|over","player":"","next":"","score":0,"level":0,"lives":3,"lastRank":-1}`.
- Evento `ranking`: `[{"id":1,"name":"ANA","score":120,"level":0}]`.

- [ ] **Step 0 (TDD):** `test/test_embed_web.py` (unittest): o cabeçalho contém o HTML sem alterações (inclusive aspas, `\\` e acentos); delimitador no HTML → `ValueError`; segunda geração sem mudança retorna `False`; HTML ausente → erro. `python3 -m unittest discover -s test -p 'test_*.py'`: falha antes, passa depois.
- [ ] **Step 1:** Implementar. Comandos em fila FreeRTOS (`xQueueCreate(8, sizeof(WebCommand))`, envio sem espera); cópia do snapshot e do ranking protegida por mutex (é a caixa de entrada do spec; a fila já é segura entre tarefas). `webPublish` envia `state` quando muda, no máximo a cada 100 ms, e a cada 2000 ms de qualquer forma; envia `ranking` quando muda. `onConnect` envia os dois. Se `webBegin` falhar, as outras funções não fazem nada.
- [ ] **Step 2:** `pio run` → SUCCESS. Se as versões das bibliotecas não compilarem com o núcleo 2.x, fixar uma versão compatível dos mesmos forks e registrar no relatório.
- [ ] **Step 3:** Commit `feat: wifi, rotas e persistencia do ranking`.

### Task 3: Páginas

**Files:** Modify `web/index.html`, `web/admin.html`. Criar um simulador temporário **fora do repositório** (scratchpad).

**Interfaces — Consumes:** o contrato HTTP/SSE da Task 2.

- [ ] **Step 1:** `PAGE_INDEX`, com as telas do spec ("Espera", "Jogando", "Fim", faixa "Sem conexão"). Textos: "Apelido", "OK", "Aperte um botão para jogar", "Não use seu nome completo", "Próximo jogador: X", "GAME OVER", "fora do Top 10", "Recorde". Nível exibido como o do firmware + 1. Recorde = maior entre o 1º do ranking e os pontos atuais. Antes de enviar: `normalize('NFD')` e remoção das marcas (`JOÃO` → `JOAO`); `maxlength="10"`; mensagem se o POST falhar. O campo não pode ser apagado nem perder o foco a cada evento `state`. Faixa "Sem conexão" em `onerror` ou após 5 s sem `state`. Pontos zero em "Fim": mostrar "fora do Top 10".
- [ ] **Step 2:** `PAGE_ADMIN`: campo de PIN, botão "Apagar" por linha, "Zerar ranking", `confirm()` nas duas ações, mensagem para 403 e 409.
- [ ] **Step 3:** Simulador em Python (stdlib) que serve os arquivos de `web/` e imita o contrato; conferir as três telas, a falta de conexão e o `/admin` num navegador, se houver um disponível. Sem navegador, rodar `node --check` no JS extraído e registrar que o visual não foi conferido.
- [ ] **Step 4:** `pio run` → SUCCESS. Commit `feat: paginas do ranking e do organizador`.

### Task 4: Integração com o jogo

**Files:** Modify `src/main.cpp` (`startGame`, `loseLife`, `setup`, `loop`, variáveis e mapa do cabeçalho)

**Interfaces — Consumes:** Tasks 1 e 2.

- [ ] **Step 1:** Novas variáveis: `Ranking ranking; Screen webScreen; char playerName[11], nextName[11]; int8_t lastRank;`.
  - `setup`: `storeLoad`, depois `webBegin` (falha só vai para a serial).
  - `startGame`: `playerName = nextName`, limpar `nextName`, `webScreen = Playing`, `lastRank = -1`.
  - Fim de jogo (em `loseLife`): `lastRank = rankingInsert(...)`; `webScreen = Over`; se `lastRank >= 0`, marcar para salvar. O `storeSave` roda no `loop`, depois do `FastLED.show()`, uma vez.
  - `loop`: consumir `webPollCommand`. Com `state == PLAYING`, descartar todo comando. `SetName`: `sanitizeName` em `nextName`; `Over` vira `Wait`. `Delete`: se remover, salvar e recalcular `lastRank` pela linha do jogador (`rankingPositionOf`). `Reset`: `rankingClear`, salvar, `lastRank = -1`. Em seguida, `webPublish`.
  - `webScreen` é independente do estado da fita: continua `Over` quando a fita volta ao arco-íris.
- [ ] **Step 2:** `pio run` → SUCCESS; `pio test -e native` → PASS.
- [ ] **Step 3:** Commit `feat: integra ranking web ao jogo`.

### Task 5: Documentação

**Files:** Modify `README.md`, `docs/esquema.md`

- [ ] **Step 1:** README: como conectar o notebook, usar a página e o `/admin`, trocar senha e PIN (`src/config_web.h`), tabela de arquivos, `pio test -e native` (requer compilador C++ no computador), problemas comuns de Wi-Fi e a lista "Na bancada" do spec.
- [ ] **Step 2:** Alimentação, nos dois arquivos (diagramas, pontos importantes e materiais): **buck 12→5 V, ≥ 1 A, recomendado**, ajustado e conferido em 5 V antes de ligar o ESP32; 7805 como alternativa só com dissipador adequado, corrente medida e temperatura verificada num teste de 1 h. Estimativa: 150–250 mA em média, 1–1,7 W no 7805.
- [ ] **Step 3:** `grep -n 7805 README.md docs/esquema.md` → nenhuma menção o apresenta como a opção principal nem cita "menos de ~100 mA". Commit `docs: interface web e alimentacao`.
