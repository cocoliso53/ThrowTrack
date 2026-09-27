# ThrowTrack

ThrowTrack uses Seeed Studio XIAO ESP32-C6 boards and Ai-Thinker BU01
(DW1000) UWB modules. One node is the initiator and two nodes are responders.
The responders are intended to be mounted a known distance apart.

## Pin connections

All three BU01 modules use the same SPI and control-pin connections. The node
role is selected by the firmware, not by changing the BU01 wiring.

### BU01 connections (all nodes)

| BU01 pin | XIAO ESP32-C6 pin | ESP32-C6 GPIO | Purpose |
|---|---:|---:|---|
| VCC / 3V3 | 3V3 | — | 3.3 V power only |
| GND | GND | — | Common ground |
| IRQ | D1 | GPIO1 | UWB interrupt |
| RSTn | D2 | GPIO2 | Active-low UWB reset |
| CSn | D3 | GPIO21 | SPI chip select |
| CLK | D8 | GPIO19 | SPI clock |
| MISO | D9 | GPIO20 | SPI data from BU01 |
| MOSI | D10 | GPIO18 | SPI data to BU01 |

The project configures the BU01/DW1000 SPI bus for `SPI_MODE2`. Keep the UWB
wires short and place the BU01 decoupling capacitors close to its 3.3 V input.

### Initiator

The initiator uses the common BU01 connections above plus the OLED and push
button below.

| Part | Signal | XIAO ESP32-C6 pin | ESP32-C6 GPIO | Connection |
|---|---|---:|---:|---|
| OLED | VCC | 3V3 | — | 3.3 V power |
| OLED | GND | GND | — | Common ground |
| OLED | SDA | D4 | GPIO22 | I2C data |
| OLED | SCL | D5 | GPIO23 | I2C clock |
| Button | Input | D6 | GPIO16 | Normally-open button between D6 and GND |
| Status LED | Output | D7 | GPIO17 | D7 -> series resistor -> LED anode; LED cathode -> GND |
| Battery monitor | ADC input | D0 / A0 | GPIO0 | Midpoint of the battery voltage divider |

The button uses the ESP32's internal pull-up, so no external button resistor is
required. Pressing the button connects D6 to ground.

### Responder A and Responder B

Both responders have identical wiring. They must have different UWB addresses
in firmware, but no hardware pin change is required between them.

| Part | Signal | XIAO ESP32-C6 pin | ESP32-C6 GPIO | Connection |
|---|---|---:|---:|---|
| Status LED | Output | D7 | GPIO17 | D7 -> series resistor -> LED anode; LED cathode -> GND |
| Battery monitor | ADC input | D0 / A0 | GPIO0 | Midpoint of the battery voltage divider |

D4, D5, and D6 remain available on each responder for future peripherals.

## Status LED

Use approximately a `1 kOhm` series resistor for a low-current indicator LED.
The planned meanings are:

- Solid on: node operating normally
- Slow blink: battery low
- Fast blink: UWB or initialization error

The LED and battery-status behavior still needs to be added to the firmware.

## Battery voltage sensing

Never connect a Li-ion/LiPo battery directly to D0. A fully charged single-cell
battery can reach 4.2 V, which is too high for an ESP32-C6 GPIO.

Use a two-resistor divider on each node:

```text
Battery + ---- 100 kOhm ----+---- D0 / A0 (GPIO0)
                            |
                          100 kOhm
                            |
Battery - ------------------+---- GND
```

This 1:1 divider presents half of the battery voltage to the ADC: 4.2 V at the
battery becomes approximately 2.1 V at D0. The calculated battery voltage is
therefore approximately twice the measured ADC voltage. A `100 nF` capacitor
from D0 to GND is recommended for steadier readings. The final low-battery
threshold should be chosen for the battery chemistry and measured under the
node's normal load.

## UWB layout

Responder A and Responder B will be placed a fixed distance apart (initially
1.9 m). The initiator will measure its distance to each responder independently
and calculate its straight-line distance to the midpoint between them. The two
responders require unique firmware addresses so their measurements are not
mixed together.
