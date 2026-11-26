// Lógica do ranking (Top 10). É C++ "puro", sem nada do Arduino,
// por isso pode ser testada no computador (test/test_ranking).
#pragma once
#include <stdint.h>

constexpr uint8_t RANKING_SIZE = 10;   // quantas posições o ranking tem
constexpr uint8_t NICK_MAX = 10;       // tamanho máximo do apelido

struct RankEntry {
  uint32_t id;                 // número único da linha (usado para apagar a linha certa)
  char name[NICK_MAX + 1];     // apelido; "" = anônimo. O +1 é o '\0' do final
  uint32_t score;
  uint8_t level;
};

struct Ranking {
  uint32_t nextId;             // contador de ids: só cresce, nunca se repete
  uint8_t count;               // quantas posições estão ocupadas
  RankEntry entries[RANKING_SIZE];   // ordenadas: [0] é o 1º lugar
};

// Apaga todas as linhas (mantém o contador de ids).
void rankingClear(Ranking& r);
// Insere um resultado. Retorna a posição (0 a 9) ou -1 se não entrou.
int rankingInsert(Ranking& r, const char* name, uint32_t score, uint8_t level);
// Apaga a linha com este id. Retorna false se ela não existe.
bool rankingRemove(Ranking& r, uint32_t id);
// Posição atual (0 a 9) da linha com este id, ou -1 se ela não está no ranking.
int rankingPositionOf(const Ranking& r, uint32_t id);
// Confere se os dados fazem sentido (usado ao ler a memória do ESP32).
bool rankingIsValid(const Ranking& r);
// Limpa um apelido: maiúsculas, só A-Z, 0-9 e espaço, no máximo NICK_MAX.
void sanitizeName(const char* in, char out[NICK_MAX + 1]);
