# ND DMX NODE 4U

ND DMX NODE 4U is a compact network-to-DMX controller with four independent DMX outputs. It receives Art-Net or sACN over Ethernet or Wi-Fi and sends standard DMX to lighting fixtures.

```text
Lighting Console / Computer
          |
     Art-Net / sACN
          |
    Ethernet / Wi-Fi
          |
   +----------------+
   | ND DMX NODE 4U |
   +----------------+
      |  |  |  |
      |  |  |  +-- DMX OUT 4
      |  |  +----- DMX OUT 3
      |  +-------- DMX OUT 2
      +----------- DMX OUT 1
```

Each output can use its own universe and protocol mode. The node includes direct Ethernet operation, Wi-Fi access through the ESP32-C5, a browser dashboard, FULL / BLACKOUT / NETWORK output testing, and dashboard firmware updates.

## Validated production baseline

- RP2350: `V3.6F5-RP2350`
- ESP32-C5: `V3.6A-C5`
- Four physical DMX outputs validated
- F4 → F5 RP Ethernet OTA validated on hardware

Licensing is currently undecided; see [LICENSE](LICENSE).

## Start here

- [Getting started](docs/GETTING_STARTED.md)
- [Wiring and connections](docs/WIRING.md)
- [Network setup](docs/NETWORK_SETUP.md)
- [Dashboard guide](docs/DASHBOARD.md)
- [Firmware updates](docs/FIRMWARE_UPDATE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Technical overview](docs/TECHNICAL_OVERVIEW.md)

## Repository layout

- `firmware/` — current production source
- `docs/` — operator and technical documentation
- `hardware/` — known hardware notes
- `archive/bringup-tests/` — historical development and hardware-validation projects

Firmware binaries are intentionally not committed to the normal source tree. Versioned binaries can be published later through GitHub Releases.
