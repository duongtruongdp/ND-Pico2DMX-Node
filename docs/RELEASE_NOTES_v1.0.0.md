# ND Pico2DMX Node v1.0.0

This is the first public GitHub release of ND Pico2DMX Node, a four-output network-to-DMX controller for Art-Net and sACN lighting control.

## Highlights

- Four independent DMX outputs
- Ethernet and Wi-Fi network control
- Browser dashboard for status, configuration, testing, and updates
- Hardware-validated production behavior, including real DMX output tests
- Controlled Ethernet OTA validation completed on the RP2350 main controller

## Firmware

- RP2350 main controller: `V3.6F5-RP2350`
- ESP32-C5 Wi-Fi controller: `V3.6A-C5`

## Validation

The v1.0.0 baseline was validated on hardware with four active DMX engines, healthy RP2350 core and SPI operation, preserved configuration, and successful F4 → F5 Ethernet OTA validation.
