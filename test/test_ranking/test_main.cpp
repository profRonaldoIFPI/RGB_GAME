// Testes da lógica do ranking. Rodam no COMPUTADOR: pio test -e native
#include <string.h>
#include <unity.h>
#include "ranking.h"

void setUp() {}
void tearDown() {}

void insere_em_ordem() {
  Ranking r{};
  TEST_ASSERT_EQUAL(0, rankingInsert(r, "A", 100, 0));
  TEST_ASSERT_EQUAL(0, rankingInsert(r, "B", 300, 0));
  TEST_ASSERT_EQUAL(1, rankingInsert(r, "C", 200, 0));
  TEST_ASSERT_EQUAL(300, r.entries[0].score);
  TEST_ASSERT_EQUAL(200, r.entries[1].score);
  TEST_ASSERT_EQUAL(100, r.entries[2].score);
}

void empate_antigo_fica_acima() {
  Ranking r{};
  rankingInsert(r, "A", 100, 0);
  TEST_ASSERT_EQUAL(1, rankingInsert(r, "B", 100, 0));
  TEST_ASSERT_EQUAL_STRING("A", r.entries[0].name);
}

void zero_nao_entra() {
  Ranking r{};
  TEST_ASSERT_EQUAL(-1, rankingInsert(r, "A", 0, 0));
  TEST_ASSERT_EQUAL(0, r.count);
}

void lista_cheia() {
  Ranking r{};
  for (uint32_t i = 1; i <= 10; i++) rankingInsert(r, "X", i * 100, 0);
  TEST_ASSERT_EQUAL(-1, rankingInsert(r, "A", 50, 0));
  TEST_ASSERT_EQUAL(-1, rankingInsert(r, "A", 100, 0));
  TEST_ASSERT_EQUAL(5, rankingInsert(r, "A", 550, 0));
  TEST_ASSERT_EQUAL(10, r.count);
  TEST_ASSERT_EQUAL(200, r.entries[9].score);
}

void remove_por_id() {
  Ranking r{};
  rankingInsert(r, "A", 300, 0);
  rankingInsert(r, "B", 200, 0);
  rankingInsert(r, "C", 100, 0);
  uint32_t id = r.entries[1].id;
  TEST_ASSERT_TRUE(rankingRemove(r, id));
  TEST_ASSERT_EQUAL(2, r.count);
  TEST_ASSERT_EQUAL_STRING("C", r.entries[1].name);
  TEST_ASSERT_FALSE(rankingRemove(r, id));
  TEST_ASSERT_EQUAL(2, r.count);
}

void ids_nao_se_repetem() {
  Ranking r{};
  rankingInsert(r, "A", 100, 0);
  uint32_t antigo = r.entries[0].id;
  TEST_ASSERT_NOT_EQUAL(0, antigo);
  rankingClear(r);
  TEST_ASSERT_EQUAL(0, r.count);
  rankingInsert(r, "B", 100, 0);
  TEST_ASSERT_GREATER_THAN(antigo, r.entries[0].id);
}

static void confere(const char* entrada, const char* esperado) {
  char out[NICK_MAX + 1];
  sanitizeName(entrada, out);
  TEST_ASSERT_EQUAL_STRING(esperado, out);
}

void limpa_apelido() {
  confere("  ana 2 ", "ANA 2");
  confere("<b>x</b>", "BXB");
  confere("abcdefghijklmn", "ABCDEFGHIJ");
  confere("JOÃO", "JOO");
  confere("!!!", "");
}

void valida_dados() {
  Ranking r{};
  TEST_ASSERT_TRUE(rankingIsValid(r));
  rankingInsert(r, "ANA", 100, 1);
  rankingInsert(r, "", 200, 2);
  TEST_ASSERT_TRUE(rankingIsValid(r));
  Ranking ruim = r;
  ruim.count = 200;
  TEST_ASSERT_FALSE(rankingIsValid(ruim));
  ruim = r;
  memset(ruim.entries[0].name, 'A', sizeof(ruim.entries[0].name));
  TEST_ASSERT_FALSE(rankingIsValid(ruim));
  memset(&ruim, 0xFF, sizeof(ruim));
  TEST_ASSERT_FALSE(rankingIsValid(ruim));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(insere_em_ordem);
  RUN_TEST(empate_antigo_fica_acima);
  RUN_TEST(zero_nao_entra);
  RUN_TEST(lista_cheia);
  RUN_TEST(remove_por_id);
  RUN_TEST(ids_nao_se_repetem);
  RUN_TEST(limpa_apelido);
  RUN_TEST(valida_dados);
  return UNITY_END();
}
