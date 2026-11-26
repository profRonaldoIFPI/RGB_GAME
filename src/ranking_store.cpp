#include "ranking_store.h"
#include <Arduino.h>
#include <Preferences.h>   // biblioteca do ESP32 para salvar dados na flash

// O que é gravado: um número "mágico" (identifica o formato) + o ranking.
// Se um dia mudar a struct Ranking, mude também o número mágico.
struct StoredRanking { uint32_t magic; Ranking ranking; };
static constexpr uint32_t MAGIC = 0x52474231;   // "RGB1"

void storeLoad(Ranking& r) {
  StoredRanking s{};
  bool ok = false;
  Preferences prefs;
  if (prefs.begin("rgbgame", true)) {            // true = só leitura
    ok = prefs.getBytesLength("rank") == sizeof(s) &&
         prefs.getBytes("rank", &s, sizeof(s)) == sizeof(s);
    prefs.end();
  }
  if (ok && s.magic == MAGIC && rankingIsValid(s.ranking)) {
    r = s.ranking;
  } else {                                       // primeira vez ou dados corrompidos
    r = Ranking{};
    r.nextId = 1;
    Serial.println("Ranking: nada salvo (ou dados invalidos); comecando vazio.");
  }
}

bool storeSave(const Ranking& r) {
  StoredRanking s{MAGIC, r};
  Preferences prefs;
  if (!prefs.begin("rgbgame", false)) return false;
  bool ok = prefs.putBytes("rank", &s, sizeof(s)) == sizeof(s);
  prefs.end();
  return ok;
}
