# 🔌 Wiring

## Normal connections

```text
Lighting computer / console
        |
     Ethernet
        |
   ND Pico2DMX Node
     | | | |
     DMX outputs
     | | | |
   Fixtures
```

Connect the network cable to Ethernet on the node. Connect each DMX output to the first fixture in that DMX line, then daisy-chain fixtures using normal DMX practice. Terminate the end of long DMX chains according to normal DMX installation practice.

Each physical output can be assigned its own universe. Port 1 does not inherently have to use Universe 1.

Do not rely on undocumented connector pinouts. Use the labels and connector documentation supplied with the assembled hardware.

## Internal Wiring / Developer Reference

The following is proven internal production mapping. It is not required for normal operation.

### DMX outputs

| Physical port | RP2350 signal | PIO assignment |
|---|---|---|
| Port 1 | GP2 | PIO0 SM0 |
| Port 2 | GP3 | PIO0 SM1 |
| Port 3 | GP27 | PIO1 SM0 |
| Port 4 | GP28 | PIO1 SM1 |

### RP2350 ↔ ESP32-C5 SPI

| Signal | Connection |
|---|---|
| READY | RP GP11 ← C5 GPIO10 |
| MISO | RP GP12 ← C5 GPIO8 |
| CS | RP GP13 → C5 GPIO9 |
| SCK | RP GP14 → C5 GPIO25 |
| MOSI | RP GP15 → C5 GPIO26 |
| GND | RP GND ↔ C5 GND |

This is developer / internal hardware reference, not an operator wiring requirement.
