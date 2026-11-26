// Controla QUANDO gravar o ranking: se a gravação falhar, tenta de novo mais tarde.
#pragma once
#include <stdint.h>

constexpr uint32_t SAVE_RETRY_MS = 5000;   // espera entre tentativas depois de uma falha

struct PendingSave {
  bool pending = false;    // há mudanças ainda não gravadas
  bool failed = false;     // a última tentativa falhou
  uint32_t lastTry = 0;    // quando foi a última tentativa (millis)
};

// O ranking mudou: gravar assim que possível.
inline void saveRequest(PendingSave& p) { p.pending = true; p.failed = false; }

// Está na hora de gravar?
inline bool saveDue(const PendingSave& p, uint32_t now) {
  return p.pending && (!p.failed || now - p.lastTry >= SAVE_RETRY_MS);
}

// Informa o resultado da tentativa. Só deixa de estar pendente se deu certo.
inline void saveResult(PendingSave& p, bool ok, uint32_t now) {
  if (ok) p.pending = false;
  p.failed = !ok;
  p.lastTry = now;
}
