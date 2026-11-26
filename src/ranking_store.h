// Guarda o ranking na memória NVS do ESP32 (não se perde ao desligar).
#pragma once
#include "ranking.h"

// Lê o ranking salvo. Se não houver dados válidos, devolve um ranking vazio.
void storeLoad(Ranking& r);
// Grava o ranking. Retorna false se falhar.
bool storeSave(const Ranking& r);
