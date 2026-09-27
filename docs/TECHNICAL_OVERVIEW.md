# ⚙️ Technical Overview

## RP2350 Main Controller

The RP2350 is the authoritative controller. It provides:

- Ethernet networking
- the browser dashboard and API
- Art-Net and sACN processing
- configuration storage
- four DMX engines
- RP2350 firmware update handling

## ESP32-C5 Wi-Fi Module

The ESP32-C5 provides:

- Wi-Fi AP/STA operation
- Wi-Fi Art-Net and sACN ingress
- C5 firmware update handling
- the gateway between Wi-Fi clients and the RP2350 controller

## RP2350 ↔ ESP32-C5 transport

The production interprocessor transport is SPI:

- 1 MHz
- SPI mode 0
- fixed 544-byte frames
- READY handshake/attention signal
- CRC-protected frames

UART experiments are historical and are retained under `archive/bringup-tests/`. UART is not the current production transport.

## DMX architecture

The RP2350 drives four independent output engines. Each output has its own universe, enable state, protocol selection, input statistics, and local test state. The normal network path passes Art-Net or sACN data to the selected output, while local FULL and BLACKOUT tests temporarily override network control.
