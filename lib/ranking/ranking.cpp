#include "ranking.h"
#include <string.h>

void rankingClear(Ranking& r) {
  r.count = 0;
  memset(r.entries, 0, sizeof(r.entries));
  // nextId não zera: assim um id antigo nunca aponta para uma linha nova.
}

int rankingInsert(Ranking& r, const char* name, uint32_t score, uint8_t level) {
  if (score == 0) return -1;

  // Procura a primeira linha com MENOS pontos (no empate, a antiga fica acima).
  uint8_t pos = 0;
  while (pos < r.count && r.entries[pos].score >= score) pos++;
  if (pos >= RANKING_SIZE) return -1;                  // não entrou no Top 10

  // Empurra as linhas de baixo para abrir espaço (a última cai se estiver cheio).
  if (r.count < RANKING_SIZE) r.count++;
  for (uint8_t i = r.count - 1; i > pos; i--) r.entries[i] = r.entries[i - 1];

  if (r.nextId == 0) r.nextId = 1;                     // id 0 não é usado
  RankEntry& e = r.entries[pos];
  memset(&e, 0, sizeof(e));
  e.id = r.nextId++;
  sanitizeName(name, e.name);
  e.score = score;
  e.level = level;
  return pos;
}

int rankingPositionOf(const Ranking& r, uint32_t id) {
  for (uint8_t i = 0; i < r.count; i++)
    if (r.entries[i].id == id) return i;
  return -1;
}

bool rankingRemove(Ranking& r, uint32_t id) {
  for (uint8_t i = 0; i < r.count; i++) {
    if (r.entries[i].id != id) continue;
    for (uint8_t j = i; j + 1 < r.count; j++) r.entries[j] = r.entries[j + 1];
    r.count--;
    memset(&r.entries[r.count], 0, sizeof(RankEntry));
    return true;
  }
  return false;
}

bool rankingIsValid(const Ranking& r) {
  if (r.count > RANKING_SIZE) return false;
  for (uint8_t i = 0; i < r.count; i++) {
    const RankEntry& e = r.entries[i];
    if (!memchr(e.name, '\0', sizeof(e.name))) return false;   // texto sem fim
    char clean[NICK_MAX + 1];
    sanitizeName(e.name, clean);
    if (strcmp(clean, e.name) != 0) return false;              // caractere proibido
    if (e.id == 0 || e.id >= r.nextId) return false;
    if (rankingPositionOf(r, e.id) != i) return false;         // id repetido
    if (e.score == 0) return false;
    if (i > 0 && e.score > r.entries[i - 1].score) return false;  // fora de ordem
  }
  return true;
}

void sanitizeName(const char* in, char out[NICK_MAX + 1]) {
  uint8_t n = 0;
  for (; *in && n < NICK_MAX; in++) {
    char c = *in;
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == ' ' && n > 0);
    if (ok) out[n++] = c;                  // espaços no começo são ignorados
  }
  while (n > 0 && out[n - 1] == ' ') n--;  // tira espaços do final
  out[n] = '\0';
}
