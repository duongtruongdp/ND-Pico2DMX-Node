# ND DMX NODE hardware v1

Headless firmware target: W5500-EVB-Pico2 / RP2350 with integrated W5500,
four MAX485 DMX transmitters, and an ESP32-C5 Wi-Fi coprocessor.

OLED, encoder, Confirm, and Back hardware are removed from hardware v1 and
are not compiled into the RP2350 firmware. The existing web dashboard remains
the node control/status interface.

## RP2350 pin map

| Function | GPIO | Notes |
|---|---:|---|
| DMX 1 TX / MAX485 DI | GP2 | PIO0 SM0 |
| DMX 2 TX / MAX485 DI | GP3 | PIO0 SM1 |
| ESP32-C5 TX link | GP4 | RP2350 TX → ESP32 RX GPIO12 |
| ESP32-C5 RX link | GP5 | RP2350 RX ← ESP32 TX GPIO11 |
| W5500 MISO | GP16 | Integrated board allocation; do not reuse |
| W5500 CS | GP17 | Integrated board allocation; do not reuse |
| W5500 SCK | GP18 | Integrated board allocation; do not reuse |
| W5500 MOSI | GP19 | Integrated board allocation; do not reuse |
| W5500 RESET | GP20 | Integrated board allocation; do not reuse |
| W5500 INT | GP21 | Reserved; firmware uses polling |
| Status LED | GP25 | Onboard W5500-EVB-Pico2 LED |
| DMX 3 TX / MAX485 DI | GP27 | PIO1 SM0 |
| DMX 4 TX / MAX485 DI | GP28 | PIO1 SM1 |

MAX485 DE and `/RE` are not MCU-controlled by this firmware. DMX remains
output-only, with the existing PIO timing, 513-byte frames, 250 kbaud, and
approximately 40 Hz schedule.

## ESP32-C5 pin map

| Function | GPIO | Notes |
|---|---:|---|
| RP2350 link TX | GPIO4 | ESP32-C5 RX ← RP2350 GP4 |
| RP2350 link RX | GPIO5 | ESP32-C5 TX → RP2350 GP5 |

The RP2350 sketch uses `Serial2` (hardware UART1) for this link at 460800 baud;
the ESP32 sketch uses `HardwareSerial(1)`.
`Serial` remains the board/core debug console; GPIO11/12 are not shared with
that console by the sketch.

## Unified dashboard and UART protocol

The RP2350 is the authoritative dashboard/API, configuration store, Art-Net,
sACN, and DMX engine. Ethernet serves it directly at `10.10.10.10`; the
ESP32-C5 serves HTTP on Wi-Fi and proxies each request to the RP2350, so there
is only one dashboard implementation.

The link uses bounded newline-framed ASCII records:

```text
ND1|HELLO|0|1
ND1|HB|0
ND1|READY|1|<base64 status>
ND1|WIFI|1|<base64 status>
ND1|REQ|<id>|<base64 HTTP request line>
ND1|RESP|<id>|<byte length>
ND1|DATA|<id>|<base64 response chunk>
ND1|END|<id>
ND1|WIFI_SET|<id>|<base64 GET query>
```

Response chunks are bounded and correlated by ID. Passwords are accepted only
in an explicit `WIFI_SET` request and are never included in status records or
dashboard rendering. The ESP32 sends periodic Wi-Fi status/heartbeat records;
neither board requires a response from the other to boot.

## Arduino assumptions

RP2350 target:

- Arduino-Pico core 6.0.0 or newer
- Generic RP2350/Pico 2 board entry if a dedicated W5500-EVB-Pico2 entry is unavailable
- `EthernetCompat.h` with `ArduinoWiznet5500lwIP`
- U8g2, Wire, and OLED libraries are no longer required

ESP32-C5 target:

- `ESP32C5_UART_WiFi/ESP32C5_UART_WiFi.ino`
- Espressif Arduino-ESP32 core with ESP32-C5 support; exact board FQBN is
  hardware-module dependent and was not present in the local installation.
- USB/JTAG `Serial` availability depends on that board/core variant; the ESP32
  UART1 link is reserved for GPIO12/11 and is never used as the debug console.

## Physical test order

1. Flash RP2350 and verify USB serial boot diagnostics and GP25 LED.
2. Flash ESP32-C5 and verify its debug console plus `ESP_READY` on the link.
3. Verify RP2350 reports the ESP32 READY/heartbeat lines.
4. Connect Ethernet and verify static IP `10.10.10.10` and HTTP dashboard.
5. Test Art-Net and sACN reception.
6. Test DMX output ports 1–4 individually.
# Legacy engineering note

The note below is preserved from the original bring-up project for historical reference. It predates the hardware-validated production SPI architecture in this repository. For current behavior, use [TECHNICAL_OVERVIEW.md](TECHNICAL_OVERVIEW.md) and the production source under `firmware/`.
