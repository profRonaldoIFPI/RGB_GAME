# RGB GAME

Um jogo no estilo *Space Invaders*, mas numa **única linha**, feito com um ESP32, uma fita de LED WS2811, 3 botões (R, G, B) e um buzzer.

Luzes coloridas "descem" pela fita em direção ao jogador. Com os botões, o jogador atira luz da **mesma cor** para apagar os inimigos. Em níveis mais altos aparecem cores misturadas (amarelo, ciano, magenta, branco), e é preciso apertar vários botões ao mesmo tempo. A cada 1 minuto o jogo acelera.

## Como se joga

| Ação | O que acontece |
|------|----------------|
| Apertar **R**, **G** ou **B** | Dispara um tiro vermelho, verde ou azul |
| Apertar **R+G** juntos | Tiro amarelo |
| Apertar **G+B** juntos | Tiro ciano |
| Apertar **R+B** juntos | Tiro magenta |
| Apertar **R+G+B** juntos | Tiro branco |

- O tiro só destrói o inimigo se a cor for **exatamente** igual. Cor errada = o tiro se perde (som grave).
- Os 3 primeiros LEDs da fita são suas **vidas** (verdes). Se um inimigo chega lá, você perde uma vida.
- **Nível 0** (1º minuto): só R, G e B. **Nível 1**: surgem misturas de 2 cores. **Nível 2 em diante**: surge o branco.
- Todo minuto o nível sobe e os inimigos ficam mais rápidos.
- Tela de espera = arco-íris. **Qualquer botão** inicia a partida.
- Um notebook no estande mostra o **placar na web**: apelido, pontos ao vivo e o Top 10.

## Placar na web

O ESP32 cria a própria rede Wi-Fi. **Não precisa de internet nem de roteador.**

1. No notebook, conecte-se à rede **`RGB-GAME`** (senha padrão: `arcade2026`). Se o sistema avisar que a rede não tem internet, escolha continuar conectado.
2. No navegador, abra **http://192.168.4.1** (com `http`, não `https`). Aperte **F11** para tela cheia.
3. Quem quiser digita um **apelido** (até 10 letras ou números, nunca o nome completo) e clica em **OK**. Depois, é só apertar um botão do jogo.
   - O apelido vale para **uma partida**. Sem apelido, o jogador aparece como `---`.
   - O campo some durante a partida.
4. Durante o jogo a página mostra pontos, nível, vidas e o recorde. No fim, mostra o Top 10 com a posição do jogador.

**Organizador:** em **http://192.168.4.1/admin**, com o PIN (padrão `1234`), dá para apagar um apelido impróprio ou zerar o ranking (fora de uma partida). O PIN só barra curiosos; não é segurança forte, pois a rede não usa HTTPS.

O Top 10 fica gravado na memória do ESP32 e continua lá depois de desligar. Partidas com zero ponto não entram, e no empate quem chegou antes fica acima.

> **Antes do evento**, troque a senha e o PIN em [src/config_web.h](src/config_web.h). A senha precisa de 8 caracteres ou mais.

## O que tem neste repositório

| Caminho | Para que serve |
|---------|----------------|
| [src/main.cpp](src/main.cpp) | **O código do jogo**, com comentários explicando cada parte |
| [web/index.html](web/index.html), [web/admin.html](web/admin.html) | **Páginas** do placar e do organizador. Edite estes arquivos |
| [scripts/embed_web.py](scripts/embed_web.py) | Roda antes de cada build e transforma as páginas em `include/web_pages.h` (**gerado**, não edite), que vai dentro do firmware |
| [src/web.cpp](src/web.cpp), [src/web.h](src/web.h) | Wi-Fi, servidor e envio do placar em tempo real |
| [src/config_web.h](src/config_web.h) | Nome da rede, senha e PIN |
| [lib/ranking/](lib/ranking/) | Regras do Top 10 (C++ puro) |
| [src/ranking_store.cpp](src/ranking_store.cpp) | Grava e lê o Top 10 na memória do ESP32 |
| [test/](test/) | Testes que rodam no computador |
| [docs/esquema.md](docs/esquema.md) | **Esquema de ligação**, tabela de pinos e lista de materiais |
| [platformio.ini](platformio.ini) | Configuração do PlatformIO (placa e biblioteca FastLED) |

## Materiais

Veja a lista completa em [docs/esquema.md](docs/esquema.md#lista-de-materiais). Resumo: ESP32 DOIT DevKit V1, fita WS2811 12 V (até 1 m), fonte 12 V ≥ 2 A, conversor buck 12→5 V (≥ 1 A), 74HCT125, 3 botões, buzzer **passivo**, resistores (330 Ω e 100 Ω) e um capacitor de 1000 µF.

## Passo a passo

### 1. Monte o circuito
Siga o diagrama de [docs/esquema.md](docs/esquema.md). Pinos usados no ESP32:

| Função | GPIO |
|--------|------|
| Dados da fita (via 74HCT125) | 5 |
| Botão R | 25 |
| Botão G | 26 |
| Botão B | 27 |
| Buzzer | 18 |

Cuidados importantes:
- **Todos os GND juntos** (fonte, buck, ESP32, 74HCT125 e fita).
- A fita liga **direto nos 12 V** da fonte. O **buck** reduz para 5 V só para o ESP32 e o 74HCT125. **Ajuste e confira os 5 V na saída antes de ligar o ESP32.**
- ⚠️ Com o Wi-Fi ligado, o ESP32 consome bem mais. Por isso **o buck é o recomendado**. O 7805 só serve como alternativa com [os cuidados do esquema](docs/esquema.md#alternativa-regulador-7805).
- O 74HCT125 converte o sinal de 3,3 V do ESP32 para 5 V, que é o que a fita precisa para entender os dados.
- ⚠️ **Não substitua o 74HCT125 por plaquinhas de "conversor de nível lógico" com BSS138 ou TXS0108E.** Elas costumam deformar o sinal da fita (cores erradas ou LEDs piscando). Veja o [aviso no esquema](docs/esquema.md#2-dados-da-fita-conversor-de-nível-74hct125).
- O **capacitor de 1000 µF** vai na alimentação da fita e o **resistor de 330 Ω** vai em série no fio de dados.
- Não ligue o USB e a alimentação por VIN ao mesmo tempo. Para gravar o programa, desligue a fonte de 12 V.
- Teste os botões e o buzzer **antes** de ligar a fita.

### 2. Instale as ferramentas
1. Instale o [VS Code](https://code.visualstudio.com/).
2. Instale a extensão **PlatformIO IDE**.
3. Abra esta pasta no VS Code (*File → Open Folder*). O PlatformIO baixa a placa e as bibliotecas (FastLED e servidor web) sozinho na primeira vez, **com internet**.

### 3. Ajuste a configuração
Abra [src/main.cpp](src/main.cpp) e olhe a seção **1. CONFIGURAÇÃO**:
- `NUM_LEDS`: conte quantos **pixels** (grupos de LEDs que acendem juntos, normalmente 1 CI a cada 3 LEDs) a sua fita tem e coloque o número.
- `COLOR_ORDER`: se na hora de testar o vermelho sair de outra cor, troque `BRG` por `RGB`, `GRB`, `GBR`...
- Dificuldade: `BASE_STEP_MS`, `SPEEDUP`, `LIVES_MAX`.

### 4. Grave e teste
1. Conecte o ESP32 por USB (com a fonte de 12 V **desligada**).
2. No PlatformIO, clique em **Upload** (seta →) na barra inferior, ou use `pio run -t upload` no terminal.
3. Abra o **Serial Monitor** (115200) para ver as mensagens (início de jogo, vidas, nível, pontos).
4. Desligue o USB, ligue a fonte de 12 V e jogue.

### 5. Rode os testes (opcional)

Eles rodam no computador, sem o ESP32:

```bash
pio test -e native                                  # regras do ranking (precisa de um compilador C++, como o g++)
python3 -m unittest discover -s test -p 'test_*.py'  # script que embute as páginas
```

### 6. Confira na bancada, antes do evento

- [ ] Com a página aberta, jogar por 10 minutos sem a fita piscar errado
- [ ] Botões e sons sem engasgos com a página aberta
- [ ] O ranking continua lá depois de desligar e ligar
- [ ] Desligar e religar o Wi-Fi do notebook: a página avisa "Sem conexão" e volta sozinha
- [ ] O `/admin` recusa um PIN errado
- [ ] Depois de 1 hora ligado, o regulador de 5 V não está quente demais

## Se algo der errado

| Problema | O que verificar |
|----------|-----------------|
| A fita não acende | GND comum, fio de dados no 74HCT125, alimentação de 12 V da fita, pino 1 (1OE) do 74HCT125 no GND, 5 V na saída do regulador |
| Cores trocadas | `COLOR_ORDER` no `main.cpp` |
| Regulador muito quente | Com 7805, troque por um buck. Meça a corrente em 5 V e procure curto-circuito |
| A rede `RGB-GAME` não aparece | Veja o Serial Monitor; confira se a senha tem 8 caracteres ou mais |
| A página não abre | O notebook está na rede `RGB-GAME`? Use `http://192.168.4.1`; desligue VPN e, se preciso, os dados móveis ou o cabo de rede |
| "Sem conexão com o jogo" | O ESP32 reiniciou ou o notebook trocou de rede. Se o ESP32 reinicia sozinho, verifique a alimentação de 5 V |
| A fita pisca errado só com o Wi-Fi | Confira GND comum, 74HCT125 e alimentação; se continuar, avise o professor (é preciso ajustar o envio de dados do FastLED) |
| Mudei o HTML e nada mudou | Compile e grave de novo; as páginas vão dentro do firmware |
| LEDs aleatórios ou piscando | Falta GND comum, capacitor ou resistor de 330 Ω; fio de dados muito longo, ou conversor de nível inadequado (BSS138/TXS0108E) |
| Só 1 LED acende (ou só alguns) | `NUM_LEDS` errado |
| O buzzer faz só "toc" | O buzzer é ativo; troque por um passivo |
| Botão não responde | Um lado no GPIO e o outro no GND, nada de resistor externo |
| Não consigo gravar | Cabo USB só de carga; segure o botão **BOOT** durante o upload; desligue o VIN |

## Ideias para evoluir

- Display de 7 segmentos para a pontuação.
- Mais efeitos e animações na página do placar.
- Modo de 2 jogadores.
- Mais efeitos sonoros e músicas.
- Uma caixa/gabinete com botões arcade grandes.
