# 🎛️ ND Pico2DMX Node

ND Pico2DMX Node is a compact four-output network-to-DMX controller. It receives Art-Net or sACN over Ethernet or Wi-Fi and converts network lighting data into four independent DMX outputs.

```text
Lighting Console / Computer
          |
     Art-Net / sACN
          |
    Ethernet / Wi-Fi
          |
   +----------------+
   | ND Pico2DMX   |
   +----------------+
      |  |  |  |
      |  |  |  +-- DMX OUT 4
      |  |  +----- DMX OUT 3
      |  +-------- DMX OUT 2
      +----------- DMX OUT 1
```

## ✨ Features

- Four independent DMX outputs
- Art-Net and sACN over Ethernet or Wi-Fi
- Per-output universe assignment and input protocol selection
- Browser-based dashboard
- FULL, BLACKOUT, and NETWORK output testing
- Signal merge options
- Firmware update support

## 🖥️ Dashboard

![ND Pico2DMX Node Dashboard](img/Dashboard.webp)

The dashboard lets operators check controller status, assign universes, choose the network input protocol, test DMX outputs, configure Wi-Fi, and update firmware.

## 🚀 Quick Start

Follow [Getting Started](docs/GETTING_STARTED.md) for the normal setup flow. If your controller does not already have firmware installed, see [Installing the Firmware](docs/INSTALLATION.md).

## 📦 Installation & Firmware

For first installation or recovery of the Main Controller, use the RP2350 `.uf2` / BOOTSEL method. For normal updates, use the built-in web dashboard with the matching `.bin` file.

- [Installing the Firmware](docs/INSTALLATION.md)
- [Firmware Update](docs/FIRMWARE_UPDATE.md)

## 📚 Documentation

- 🚀 [Getting Started](docs/GETTING_STARTED.md)
- 📦 [Installing the Firmware](docs/INSTALLATION.md)
- 🔌 [Wiring](docs/WIRING.md)
- 🌐 [Network Setup](docs/NETWORK_SETUP.md)
- 🖥️ [Dashboard](docs/DASHBOARD.md)
- 🔄 [Firmware Update](docs/FIRMWARE_UPDATE.md)
- 🧰 [Troubleshooting](docs/TROUBLESHOOTING.md)
- ⚙️ [Technical Overview](docs/TECHNICAL_OVERVIEW.md)
- 📋 [Changelog](CHANGELOG.md)
- 🎉 [Release Notes](docs/RELEASE_NOTES_v1.0.0.md)

## 🏷️ Current Release

**ND Pico2DMX Node v1.0.0**

- Main Controller: `V3.6F5-RP2350`
- Wi-Fi Module: `V3.6A-C5`
- First GitHub release

## 🛠️ Project Status

The production baseline is hardware-validated, including successful F4 → F5 Ethernet OTA validation and real-output verification.

## 📄 License

ND Pico2DMX Node is licensed under the [GNU General Public License v3.0](LICENSE).

Third-party components remain subject to their respective licenses. See [Third-Party Licensing Notes](docs/LICENSES.md).

## Repository layout

- `firmware/` — current production source
- `docs/` — operator and technical documentation
- `hardware/` — known hardware notes
- `archive/bringup-tests/` — historical development and hardware-validation projects

Firmware binaries are intentionally not committed to the normal source tree. Versioned binaries can be published later through GitHub Releases.
