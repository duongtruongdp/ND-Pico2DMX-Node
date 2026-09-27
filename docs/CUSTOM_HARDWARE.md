# 🔧 Custom Hardware & GPIO Mapping

This is an **advanced** guide for users designing their own PCB, adapting the
firmware to a different enclosure, changing RP2350-to-DMX-transceiver wiring,
or creating another hardware revision.

The official v1.0.0 hardware remains the validated reference configuration.
Changing the wiring or firmware GPIO assignments creates a custom hardware
configuration outside that validated reference result.

## 📍 Reference DMX Output Mapping

The production RP2350 source defines the validated reference mapping as:

| DMX Output | RP2350 GPIO | PIO State Machine |
|---|---|---|
| Output 1 | GP2 | PIO0 SM0 |
| Output 2 | GP3 | PIO0 SM1 |
| Output 3 | GP27 | PIO1 SM0 |
| Output 4 | GP28 | PIO1 SM1 |

This is a validated reference mapping, not the only possible mapping.

The public source identifies outputs by zero-based internal index and exposes
them as output/port numbers at the dashboard and API boundary. The validated
source does not provide independent evidence for a particular assembled
connector label order; connector numbering must therefore be verified against
the actual PCB and transceiver wiring.

## 🧭 Production customization point

The four DMX GPIO values are defined in:

firmware/rp2350/ND_Pico2DMX.cpp

The symbol is the constant array RS485_PINS[4]:

    const uint8_t RS485_PINS[4] = {2, 3, 27, 28};

The array positions correspond to Output 1 through Output 4 respectively.
During setup(), the production source calls:

    dmxOutputs[i].begin(i < 2 ? pio0 : pio1, i % 2, RS485_PINS[i]);

This associates outputs 1 and 2 with PIO0 state machines 0 and 1, and outputs
3 and 4 with PIO1 state machines 0 and 1. PIODMX::begin() receives the
selected pin and configures the PIO output and sideset pin from that argument.

For a custom PCB, a technically capable developer may substitute the four
values in RS485_PINS so they match the physical DMX transceiver inputs. This
is a source-code customization and requires rebuilding the RP2350 firmware.
It is not configurable from the web dashboard.

The current source does not use GPIO numbers for configuration storage,
universe routing, dashboard output numbering, or the network protocol. Those
systems continue to use the output index. The GPIO array is also used by the
DMX transmit service for the corresponding output pin. No other pin-specific
PIO change is required for a simple relocation when the same PIO program,
timing, and state-machine allocation remain suitable.

Do not modify the PIO instructions, DMX baud rate, break timing, MAB timing,
FIFO behavior, or state-machine allocation for a simple GPIO relocation.

## ⚠️ Custom hardware warning

Do not choose a GPIO only because it appears unused in the firmware.

Before assigning a different DMX pin, check:

- the RP2350 board schematic
- Waveshare W5500-EVB-Pico2 pin usage
- the W5500 interface
- the ESP32-C5 interface
- boot and debug functions
- external RS485 / DMX transceiver wiring
- all PCB-specific connections

The GPIO number in firmware must match the physical RP2350 connection to the
DMX transceiver. Changing firmware alone does not physically reroute a DMX
connector.

## 🔌 Reference GPIO usage

The following table is derived from the production source and existing
hardware documentation. “Needs hardware review” does not mean a pin is safe;
it means this source review does not authorize using it for a custom DMX
connection.

| GPIO | Current function | Custom DMX guidance |
|---:|---|---|
| GP2 | Reference DMX Output 1 | Reference DMX pin |
| GP3 | Reference DMX Output 2 | Reference DMX pin |
| GP11 | ESP32-C5 READY input | Reserved by ESP32-C5 interface |
| GP12 | ESP32-C5 SPI MISO | Reserved by ESP32-C5 interface |
| GP13 | ESP32-C5 SPI CS | Reserved by ESP32-C5 interface |
| GP14 | ESP32-C5 SPI SCK | Reserved by ESP32-C5 interface |
| GP15 | ESP32-C5 SPI MOSI | Reserved by ESP32-C5 interface |
| GP16 | W5500 MISO | Reserved by Ethernet |
| GP17 | W5500 CS | Reserved by Ethernet |
| GP18 | W5500 SCK | Reserved by Ethernet |
| GP19 | W5500 MOSI | Reserved by Ethernet |
| GP20 | W5500 reset | Reserved by Ethernet |
| GP21 | W5500 INT/reserved board connection | Reserved by Ethernet |
| GP25 | W5500-EVB-Pico2 hardware LED | Needs hardware review |
| GP27 | Reference DMX Output 3 | Reference DMX pin |
| GP28 | Reference DMX Output 4 | Reference DMX pin |
| Other GPIOs | Not assigned by this production source review | Needs board-schematic and boot/debug review |

The production source uses SPI1 GP12–GP15 for the ESP32-C5 link, with GP11 as
READY. It uses the integrated W5500 allocation GP16–GP21 and the board LED on
GP25. A custom design must also check pins that are not explicitly assigned
in this firmware.

## ⚙️ PIO and state-machine rules

The validated allocation is:

- Output 1: PIO0 SM0
- Output 2: PIO0 SM1
- Output 3: PIO1 SM0
- Output 4: PIO1 SM1

The current PIODMX::begin() implementation accepts the GPIO as an argument,
sets the PIO output and sideset pin from that argument, and does not encode the
reference GPIO numbers in the PIO instructions. Therefore a simple GPIO-only
relocation can retain the validated PIO blocks and state machines, provided the
new pins are valid for the RP2350 PIO and the custom hardware is wired
accordingly.

## 🛠️ Build requirement

Changing a DMX GPIO is a source-code customization. It requires rebuilding the
RP2350 firmware and is not a runtime dashboard setting.

The validated production environment used for this project is:

- Arduino-Pico 6.1.1
- FQBN: rp2040:rp2040:rpipico2:flash=4194304_2097152
- Target hardware: Waveshare W5500-EVB-Pico2 / RP2350
- Flash layout: 4 MB total, approximately 2 MB application and 2 MB filesystem

The repository does not contain a fully reproducible build script or manifest;
do not infer a build command from this guide alone. Match the production board,
core, FQBN, and flash settings in the project’s validated build environment.

## 🚀 Advanced custom build workflow

1. Clone the repository.
2. Read this guide and inspect the custom PCB schematic.
3. Locate RS485_PINS in firmware/rp2350/ND_Pico2DMX.cpp.
4. Select GPIOs appropriate for the custom PCB.
5. Check conflicts with Ethernet, ESP32-C5, boot/debug, and other hardware.
6. Update only the required GPIO array values.
7. Build the RP2350 firmware using the documented production environment.
8. For the first custom hardware test, install using USB/BOOTSEL.
9. Test each DMX output independently.
10. Verify Ethernet and ESP32-C5 communication.
11. Verify Art-Net and sACN operation.
12. Only after validation should you consider dashboard-based .bin updates for
    that custom hardware.

## 🏷️ Custom build identification

For troubleshooting, a custom build may be identified with a name such as
V3.6F5-RP2350-custom. This is only a recommendation for custom builds.
Do not change the official production firmware version in the validated
reference source. Clear custom identification helps prevent confusion with the
official release.

## ✅ Custom hardware validation checklist

- [ ] Firmware boots normally.
- [ ] Dashboard opens.
- [ ] Ethernet remains operational.
- [ ] ESP32-C5 communication remains healthy.
- [ ] Output 1 produces DMX.
- [ ] Output 2 produces DMX.
- [ ] Output 3 produces DMX.
- [ ] Output 4 produces DMX.
- [ ] FULL test works.
- [ ] BLACKOUT test works.
- [ ] NETWORK mode works.
- [ ] Art-Net works where applicable.
- [ ] sACN works where applicable.
- [ ] No custom GPIO conflicts with Ethernet.
- [ ] No custom GPIO conflicts with ESP32-C5 communication.

A custom GPIO mapping is not covered by the project’s validated reference
hardware result until it has been validated on the user’s own hardware.
