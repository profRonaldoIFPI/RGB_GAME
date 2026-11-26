# Design: interface web com ranking (RGB GAME)

## Intenção

O jogo será exibido num evento para estudantes do ensino fundamental. Um notebook no estande mostra um ranking no estilo fliperama: o apelido (opcional) é digitado antes da partida, a pontuação aparece durante o jogo e o ranking volta no fim.

## Objetivos

- Página servida pelo próprio ESP32, atualizada em tempo real.
- Top 10 que sobrevive a quedas de energia.
- O jogo continua jogável pelos botões, com ou sem navegador conectado.
- Código que um aluno consiga ler e montar.

## Fora do escopo

Celulares dos visitantes, fila de jogadores, internet ou roteador, contas, controle do jogo pela web, exportação do ranking.

## Seria um fracasso se

A fita piscasse errado ou os botões engasgassem por causa do Wi-Fi; o ranking sumisse; ou uma falha da página impedisse alguém de jogar.

## Restrições

- ESP32 DOIT DevKit V1, PlatformIO, Arduino-ESP32 2.x (a instalada), FastLED.
- Sem internet: HTML, CSS e JavaScript embutidos no firmware, sem fontes ou arquivos externos.
- O loop do jogo continua sem `delay()`.

## Abordagens

| Abordagem | Prós | Contras | Decisão |
|---|---|---|---|
| **A. ESPAsyncWebServer + SSE** | Resposta imediata; servidor não depende do loop | Biblioteca extra; dados compartilhados precisam de proteção | **Escolhida por você** |
| B. WebServer + consultas a cada 0,5 s | Código simples | Atraso; risco de engasgar o jogo | Não |
| Notebook por USB, sem Wi-Fi | Sem consumo do Wi-Fi | Não é a interface no ESP32 que foi pedida | Descartada |

## Comportamento

### Rede
- O ESP32 cria a rede `RGB-GAME`, com senha WPA2 (para os visitantes não entrarem). SSID e senha ficam num bloco de configuração.
- Endereço: `http://192.168.4.1`.

### Telas (uma única página, que troca sozinha)
- **Espera:** título, campo "Apelido", botão OK, aviso "Aperte um botão para jogar" e o Top 10.
- **Jogando:** apelido, pontos em destaque, nível, vidas e o recorde.
- **Fim:** "GAME OVER", pontos, posição (ou "fora do Top 10"), Top 10 com a linha nova destacada e o campo de apelido do próximo jogador. Fica assim até a próxima partida começar ou um apelido ser enviado, mesmo depois de a fita voltar ao arco-íris.
- **Sem conexão:** faixa de aviso; a página reconecta sozinha e recebe o estado completo.
- Visual de fliperama: fundo escuro, letras grandes, cores R, G e B; legível a alguns passos de distância.

### Apelido
- Opcional, até 10 caracteres: letras sem acento, números e espaço, em maiúsculas. O ESP32 valida e limpa o texto (não confia no navegador).
- Pede "apelido", com aviso para não usar nome completo.
- Depois do OK, a tela mostra "Próximo jogador: ANA". Enviar de novo substitui; enviar vazio limpa.
- É usado **uma vez**: a partida que começar fica com ele, e depois volta a anônimo (`---`).
- Durante a partida o campo fica oculto e o ESP32 recusa envios.
- O jogo começa pelo botão, como hoje. Quem não deu OK joga como anônimo.

### Ranking
- 10 posições: apelido, pontos, nível alcançado.
- Ordem por pontos; no empate, quem chegou antes fica acima.
- Zero ponto não entra. Anônimos entram como `---`.
- Gravado na memória NVS (`Preferences`), só quando o ranking muda. Dados ausentes ou corrompidos resultam em ranking vazio.

### Organizador
- Página `/admin`: apagar uma linha ou zerar tudo, com confirmação.
- Protegida por um PIN definido na configuração, conferido no ESP32. É uma trava contra curiosos, não segurança forte.
- Recusado durante uma partida.

## Arquitetura

| Arquivo | Responsabilidade |
|---|---|
| `src/main.cpp` | Jogo, como hoje, com poucas chamadas: iniciar a web, pegar o apelido, registrar o fim, publicar o estado |
| `lib/ranking/` | Ordenar, inserir, apagar e limpar apelidos. C++ puro, testável no computador |
| `src/ranking_store.{h,cpp}` | Gravar e ler o ranking na NVS |
| `src/web.{h,cpp}` | Wi-Fi, rotas e SSE |
| `web/index.html`, `web/admin.html` | Páginas (HTML, CSS e JS), editáveis e abríveis como arquivos comuns |
| `scripts/embed_web.py` | Roda antes de cada build e gera o cabeçalho C++ com as páginas, que é compilado no firmware. Não reescreve o `main.cpp`; o arquivo gerado não vai para o git |
| `src/config_web.h` | SSID, senha e PIN |

**Rotas:** `GET /`, `GET /admin`, `GET /events` (SSE), `POST /name`, `POST /admin/delete`, `POST /admin/reset`.

**Eventos SSE:**
- `state`: tela, apelido atual, próximo apelido, pontos, nível, vidas, resultado da última partida. Enviado quando muda (no máximo a cada 100 ms) e a cada 2 s como sinal de vida.
- `ranking`: lista completa, cada linha com um identificador. Enviado ao conectar e quando muda.

Apagar usa o identificador, não a posição, para não remover a linha errada.

**Concorrência:** o servidor roda em outra tarefa. Somente o loop do jogo altera o estado e o ranking. As rotas deixam pedidos (apelido, apagar, zerar) numa caixa de entrada protegida por mutex e leem uma cópia do estado, também protegida. Os comentários explicam isso para o aluno. A tarefa de rede fica no núcleo 0; o loop do jogo, no núcleo 1.

**Falhas:** se o Wi-Fi ou o servidor não iniciar, o erro vai para a serial e o jogo segue normalmente.

**Nomes na tela:** inseridos com `textContent`, nunca como HTML.

## Alimentação (muda a montagem)

Com Wi-Fi, o consumo sobe (estimativa: 150–250 mA em média, com picos maiores). No 7805 isso é cerca de 1–1,7 W contínuos. `docs/esquema.md` e `README.md` passam a **recomendar um buck 12→5 V (≥ 1 A)**, ajustado e conferido em 5 V antes de ligar o ESP32. O 7805 permanece como alternativa, só com dissipador adequado, corrente medida e temperatura verificada num teste longo.

## Testes

- **Automáticos no computador** (`pio test -e native`): inserção, empate, lista cheia, zero ponto, apagar por identificador e limpeza do apelido.
- **Compilação** do firmware.
- **Na bancada (pelo montador, lista no README):** fita sem piscadas com a página aberta durante 10 min; botões e sons sem engasgos; ranking mantido após desligar; reconexão do notebook; PIN errado recusado; aquecimento do regulador após 1 h.

Se a fita falhar com o Wi-Fi ligado, ajusta-se o modo de envio do FastLED. Isso só pode ser verificado no hardware.

## Liberdade de quem implementa

Detalhes visuais, formato do JSON, versões das bibliotecas (forks mantidos do ESPAsyncWebServer e do AsyncTCP, compatíveis com o núcleo instalado) e nomes internos.
