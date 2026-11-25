# Esquema de ligação – RGB GAME

Placa: ESP32 DOIT DevKit V1. Fita: WS2811 (12 V). Buzzer: **passivo**.

## Pinos

| Função            | GPIO ESP32 | Observação                                   |
|-------------------|-----------|----------------------------------------------|
| Dados da fita     | 5         | passa pelo level shifter 74HCT125 (3V3→5V)   |
| Botão R (vermelho)| 25        | outro lado no GND, `INPUT_PULLUP`            |
| Botão G (verde)   | 26        | idem                                         |
| Botão B (azul)    | 27        | idem                                         |
| Buzzer passivo    | 18        | via resistor de 100 Ω para o GND do buzzer   |

## Diagrama

```
                   FONTE 12 V (≥ 2 A)
                   +12V ───────┬───────────────────────────┐
                               │                           │
                         [1000 µF/25 V]                    │
                               │                           │
                   GND ────────┴──────────┬────────────┐    │
                                          │            │    │
                                          │            │    │
        ┌───────────────────┐       ┌─────┴────┐       │    │
        │  Buck 12V → 5V    │       │  GND     │       │    │
        │  (LM2596/MP1584)  │       │  comum   │       │    │
        │ IN+ ◄─────────────┼───────┼──────────┼───────┼────┘
        │ IN- ◄─────────────┼───────┘          │       │
        │ OUT+ ──► 5V ──────┼──────┐           │       │
        │ OUT- ──► GND      │      │           │       │
        └───────────────────┘      │           │       │
                                   │           │       │
              ┌────────────────────┼───────────┼───┐   │
              │   ESP32 DevKit     │           │   │   │
              │                 VIN◄┘      GND ◄┘   │   │
              │                                    │   │
              │  GPIO25 ◄── BTN R ── GND           │   │
              │  GPIO26 ◄── BTN G ── GND           │   │
              │  GPIO27 ◄── BTN B ── GND           │   │
              │  GPIO18 ──[100Ω]── BUZZER(+)       │   │
              │                    BUZZER(-) ─ GND │   │
              │  GPIO5  ─────────────┐             │   │
              │  3V3 ─┐              │             │   │
              └───────┼──────────────┼─────────────┘   │
                      │              │                 │
                      │         ┌────┴─────┐           │
                      │         │ 74HCT125 │           │
                      │         │  (1 buf) │           │
                      │         │ pino 1 (1OE) ─ GND   │
                      │         │ pino 2 (1A)  ◄ GPIO5 │
                      │         │ pino 3 (1Y) ──[330Ω]─┼──► DIN da fita
                      │         │ pino 14 (VCC) ◄ 5V   │
                      │         │ pino 7  (GND) ─ GND  │
                      │         └──────────┘           │
                      │                                │
                      │          FITA WS2811 12 V      │
                      │          +12V ◄────────────────┘
                      │          GND  ◄── GND comum
                      │          DIN  ◄── 74HCT125 (via 330 Ω)
                      └─ (3V3 só alimenta o ESP32; o 74HCT125 usa 5 V)
```

### Pontos importantes

1. **GND comum** entre fonte 12 V, buck, ESP32, 74HCT125 e fita. Sem isso a fita pisca/fica aleatória.
2. **74HCT125**: a entrada aceita 3,3 V e a saída é 5 V. Os pinos de habilitação (1OE, ativo em nível baixo) vão ao GND. Os buffers que sobrarem (2A, 3A, 4A) devem ter as entradas ligadas ao GND, nunca soltas.
3. **Resistor de 330 Ω** em série com DIN, perto da fita, e **capacitor de 1000 µF** nos terminais +12 V/GND da fita (obrigatório para evitar picos).
4. O ESP32 é alimentado pelo pino **VIN a 5 V** (do buck). **Nunca** ligue o USB e o VIN ao mesmo tempo sem diodo (USB só para gravar, com a fonte desligada).
5. **Buzzer passivo**: se o seu som sair apenas "toc", ele é ativo (tem oscilador interno); troque por um passivo.
6. Botões: arcade de 24 mm ou push-buttons, cada um com a cor correspondente. Sem resistor externo, pois o `INPUT_PULLUP` interno cuida disso.
7. Se, no teste, as cores saírem trocadas, mude `COLOR_ORDER` em `src/main.cpp` (BRG → RGB/GRB...). Se a fita não responder, tente `LED_TYPE WS2812`/velocidade de 400 kHz.

## Lista de materiais

- ESP32 DOIT DevKit V1
- Fita WS2811 12 V, até 1 m
- Fonte 12 V ≥ 2 A
- Buck 12 V → 5 V (≥ 1 A)
- 74HCT125 (ou SN74AHCT125)
- 3 botões (R, G, B)
- Buzzer passivo 5 V
- Resistores: 1× 330 Ω, 1× 100 Ω
- Capacitor eletrolítico 1000 µF / 25 V
- Protoboard ou placa perfurada, fios
