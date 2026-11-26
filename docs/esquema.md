# Esquema de ligação – RGB GAME

Placa: ESP32 DOIT DevKit V1. Fita: WS2811 (12 V). Buzzer: **passivo**.

## Pinos

| Função            | GPIO ESP32 | Observação                                   |
|-------------------|-----------|----------------------------------------------|
| Dados da fita     | 5         | passa pelo conversor de nível 74HCT125 (3V3→5V) |
| Botão R (vermelho)| 25        | outro lado no GND, `INPUT_PULLUP`            |
| Botão G (verde)   | 26        | idem                                         |
| Botão B (azul)    | 27        | idem                                         |
| Buzzer passivo    | 18        | com resistor de 100 Ω em série               |

## Visão geral

```
Fonte 12 V ──┬──────────────────────────────► +12V da fita
             └──► BUCK ──5 V──┬──► VIN do ESP32
                              └──► VCC do 74HCT125 (pino 14)

ESP32 GPIO5 ──► 74HCT125 (1A → 1Y) ──[330 Ω]──► DIN da fita

GND: fonte + buck + ESP32 + 74HCT125 + fita, todos juntos
```

A fita é alimentada **direto pela fonte de 12 V**. Ela **não** passa pelo regulador de 5 V.

## 1. Alimentação (buck 12 V → 5 V)

Com o Wi-Fi do placar ligado, **estimamos 150 a 250 mA em média em 5 V, com picos maiores** (meça no seu circuito). Por isso o recomendado é um **conversor buck de pelo menos 1 A** (por exemplo, módulo LM2596 ou MP1584), que esquenta bem menos que um regulador linear.

```
 12V ──► IN+ [ BUCK ] OUT+ ──► 5V (VIN do ESP32 e VCC do 74HCT125)
 GND ──► IN-          OUT- ──► GND comum
```

1. **Antes de ligar o ESP32**, ligue só a fonte e o buck.
2. Se o módulo for ajustável, gire o parafuso até o multímetro marcar **5,0 V** na saída. Muitos módulos vêm com a saída alta, o que **queima o ESP32**.
3. Desligue, ligue o restante e meça os 5 V de novo.

### Alternativa: regulador 7805

O 7805 transforma em calor a diferença entre 12 V e 5 V: com 150 a 250 mA são cerca de **1 a 1,7 W contínuos**. Use-o **somente** se fizer os três: (1) instalar um dissipador adequado, (2) medir a corrente em 5 V e (3) verificar a temperatura num teste de 1 hora, já dentro da caixa. Cuidado: ele pode queimar o dedo.

7805 visto de frente, com a face impressa virada para você:

```
 12V ──┬──────────── 1 IN  [7805]  OUT 3 ────────────┬──────── 5V
       │                     2 GND                   │
  [0,33 µF] + [10 µF]          │              [0,1 µF] + [10 µF]
       │                       │                     │
 GND ──┴───────────────────────┴─────────────────────┴──────── GND comum
```

- Capacitores cerâmicos **bem junto dos pinos**. Sem eles o regulador pode oscilar.
- Eletrolíticos: **25 V ou mais** na entrada e **10 V ou mais** na saída. Atenção à polaridade.
- A aba metálica (TO-220) é ligada ao **GND**. Não deixe encostar em outra coisa.

## 2. Dados da fita (conversor de nível 74HCT125)

O ESP32 manda os dados em 3,3 V, mas a fita é mais confiável recebendo cerca de 5 V. O 74HCT125, alimentado com 5 V, entende o sinal de 3,3 V (para ele, "1" é qualquer coisa acima de 2 V) e devolve o mesmo sinal em 5 V, rápido o bastante para a fita.

| Pino do 74HCT125 | Ligar em |
|------------------|----------|
| 1 (1OE)          | GND (habilita a saída) |
| 2 (1A, entrada)  | GPIO5 do ESP32 |
| 3 (1Y, saída)    | resistor de 330 Ω → DIN da fita |
| 5, 9, 12 (entradas não usadas) | GND (nunca deixe soltas) |
| 7 (GND)          | GND comum |
| 14 (VCC)         | 5 V (saída do buck) |

Confira a pinagem no datasheet do CI que você comprou. O 74AHCT125 também serve.

> ### ⚠️ Aviso: não use módulos prontos de "conversor de nível lógico" comuns
>
> | Módulo | Serve? |
> |--------|--------|
> | **74HCT125**, 74AHCT125 ou 74HCT245 (CI) | ✅ Recomendado |
> | Plaquinha de 4 canais com transistores **BSS138** (a mais comum, feita para I2C) | ❌ Evite. Depende de resistores de pull-up (geralmente 10 kΩ); o sinal sobe devagar e os pulsos da fita se deformam |
> | Módulo com **TXS0108E** | ❌ Evite. Saída fraca e detecção automática de direção, que costuma falhar com fitas de LED |
>
> O sintoma de um conversor inadequado são **cores erradas, LEDs piscando ao acaso ou fita sem resposta**.

## 3. ESP32, botões e buzzer

```
        ┌──────────────────────┐
 5V ───►│ VIN                  │
        │               GPIO25 │◄── botão R ── GND
        │    ESP32      GPIO26 │◄── botão G ── GND
        │    DevKit     GPIO27 │◄── botão B ── GND
        │               GPIO18 │──[100 Ω]── (+) buzzer (-) ── GND
        │                GPIO5 │──► pino 2 do 74HCT125
GND ───►│ GND                  │
        └──────────────────────┘
```

## 4. Fita

```
 +12V da fonte ──┬──────────────► +12V da fita
                 │
           [1000 µF / 25 V]     (junto da entrada da fita; atenção à polaridade)
                 │
 GND da fonte  ──┴──────────────► GND da fita

 74HCT125 pino 3 ──[330 Ω]──────► DIN da fita
```

Ligue o GND da fita **direto na fonte**, com um fio próprio, para a corrente da fita não passar pelo GND do ESP32.

### Pontos importantes

1. **GND comum** entre fonte 12 V, buck, ESP32, 74HCT125 e fita. Sem isso a fita pisca ou fica aleatória.
2. **Resistor de 330 Ω** em série com DIN, perto da fita, e **capacitor de 1000 µF** nos terminais +12 V/GND da fita (para evitar picos).
3. **Nunca** ligue o USB e o VIN (5 V do regulador) ao mesmo tempo. Use o USB só para gravar, com a fonte de 12 V desligada.
4. **Nunca** coloque 12 V no ESP32, no 74HCT125 ou no fio DIN.
5. **Buzzer passivo**: se o som sair apenas como um "toc", ele é ativo (tem oscilador interno); troque por um passivo.
6. Botões: arcade de 24 mm ou push-buttons, cada um com a cor correspondente. Sem resistor externo, pois o `INPUT_PULLUP` interno cuida disso.
7. Se as cores saírem trocadas, mude `COLOR_ORDER` em `src/main.cpp` (BRG → RGB, GRB...). Se a fita estiver ligada corretamente e mesmo assim se comportar mal, ela pode usar a velocidade de 400 kHz: troque `LED_TYPE WS2811` por `WS2811_400`.

## Lista de materiais

- ESP32 DOIT DevKit V1
- Fita WS2811 12 V, até 1 m
- Fonte 12 V ≥ 2 A (confira o consumo da sua fita)
- Conversor **buck 12 V → 5 V, ≥ 1 A**
- **74HCT125** (ou 74AHCT125)
- 3 botões (R, G, B)
- Buzzer passivo 5 V
- Resistores: 1× 330 Ω, 1× 100 Ω
- Capacitor eletrolítico: 1× 1000 µF/25 V
- *Só para a alternativa com 7805:* dissipador, cerâmicos de 0,33 µF e 0,1 µF, eletrolíticos de 10 µF/25 V e 10 µF/10 V (ou mais)
- Protoboard ou placa perfurada, fios
