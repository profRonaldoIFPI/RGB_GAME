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

## O que tem neste repositório

| Caminho | Para que serve |
|---------|----------------|
| [src/main.cpp](src/main.cpp) | **Todo o código do jogo**, com comentários explicando cada parte |
| [docs/esquema.md](docs/esquema.md) | **Esquema de ligação**, tabela de pinos e lista de materiais |
| [platformio.ini](platformio.ini) | Configuração do PlatformIO (placa e biblioteca FastLED) |
| `include/`, `lib/`, `test/` | Pastas padrão do PlatformIO (vazias neste projeto) |

## Materiais

Veja a lista completa em [docs/esquema.md](docs/esquema.md#lista-de-materiais). Resumo: ESP32 DOIT DevKit V1, fita WS2811 12 V (até 1 m), fonte 12 V ≥ 2 A, regulador 7805 com dissipador, 74HCT125, 3 botões, buzzer **passivo**, resistores (330 Ω e 100 Ω) e capacitores (1000 µF para a fita e os do 7805).

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
- **Todos os GND juntos** (fonte, 7805, ESP32, 74HCT125 e fita).
- A fita liga **direto nos 12 V** da fonte. O 7805 reduz para 5 V só para o ESP32 e o 74HCT125. Coloque os capacitores junto dele, use um dissipador e **confira os 5 V na saída antes de ligar o ESP32**.
- O 74HCT125 converte o sinal de 3,3 V do ESP32 para 5 V, que é o que a fita precisa para entender os dados.
- ⚠️ **Não substitua o 74HCT125 por plaquinhas de "conversor de nível lógico" com BSS138 ou TXS0108E.** Elas costumam deformar o sinal da fita (cores erradas ou LEDs piscando). Veja o [aviso no esquema](docs/esquema.md#2-dados-da-fita-conversor-de-nível-74hct125).
- O **capacitor de 1000 µF** vai na alimentação da fita e o **resistor de 330 Ω** vai em série no fio de dados.
- Não ligue o USB e a alimentação por VIN ao mesmo tempo. Para gravar o programa, desligue a fonte de 12 V.
- Teste os botões e o buzzer **antes** de ligar a fita.

### 2. Instale as ferramentas
1. Instale o [VS Code](https://code.visualstudio.com/).
2. Instale a extensão **PlatformIO IDE**.
3. Abra esta pasta no VS Code (*File → Open Folder*). O PlatformIO baixa a placa e a biblioteca FastLED sozinho na primeira vez.

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

## Se algo der errado

| Problema | O que verificar |
|----------|-----------------|
| A fita não acende | GND comum, fio de dados no 74HCT125, alimentação de 12 V da fita, pino 1 (1OE) do 74HCT125 no GND, 5 V na saída do 7805 |
| Cores trocadas | `COLOR_ORDER` no `main.cpp` |
| 7805 muito quente | Dissipador instalado? Meça a corrente em 5 V e procure curto-circuito |
| LEDs aleatórios ou piscando | Falta GND comum, capacitor ou resistor de 330 Ω; fio de dados muito longo, ou conversor de nível inadequado (BSS138/TXS0108E) |
| Só 1 LED acende (ou só alguns) | `NUM_LEDS` errado |
| O buzzer faz só "toc" | O buzzer é ativo; troque por um passivo |
| Botão não responde | Um lado no GPIO e o outro no GND, nada de resistor externo |
| Não consigo gravar | Cabo USB só de carga; segure o botão **BOOT** durante o upload; desligue o VIN |

## Ideias para evoluir

- Placar com os LEDs, ou display de 7 segmentos para a pontuação.
- Guardar o recorde na memória do ESP32 (`Preferences`).
- Modo de 2 jogadores.
- Mais efeitos sonoros e músicas.
- Uma caixa/gabinete com botões arcade grandes.
