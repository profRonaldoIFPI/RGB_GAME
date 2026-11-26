// Interface web: rede Wi-Fi, servidor HTTP e envio do placar em tempo real.
//
// IMPORTANTE: o servidor roda em OUTRA TAREFA, ao mesmo tempo que o jogo.
// Para os dois não mexerem nos mesmos dados:
//   - só o loop() do jogo altera o estado e o ranking;
//   - o navegador só PEDE coisas: o pedido entra numa fila (webPollCommand);
//   - o servidor só enxerga uma CÓPIA do estado, entregue por webPublish().
#pragma once
#include <stdint.h>
#include "ranking.h"

// Qual tela a página deve mostrar
enum class Screen : uint8_t { Wait, Playing, Over };

// "Foto" do jogo, enviada para o navegador
struct GameSnapshot {
  Screen screen;
  char player[NICK_MAX + 1];      // quem está jogando (ou acabou de jogar)
  char nextPlayer[NICK_MAX + 1];  // apelido já enviado para a próxima partida
  uint32_t score;
  uint8_t level, lives;
  int8_t lastRank;                // posição da última partida no Top 10 (0 a 9) ou -1
};

// Pedido feito pelo navegador
struct WebCommand {
  enum Type : uint8_t { SetName, Delete, Reset } type;
  char name[NICK_MAX + 1];   // usado em SetName (já limpo)
  uint32_t id;               // usado em Delete
};

bool webBegin();                    // liga o Wi-Fi e o servidor; false se falhar
void webPublish(const GameSnapshot& s, const Ranking& r);  // chamar a cada volta do loop()
bool webPollCommand(WebCommand& c);  // pega o próximo pedido (não espera)
