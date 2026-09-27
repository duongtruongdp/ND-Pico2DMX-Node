# 📦 Installing the Firmware

ND Pico2DMX Node contains two controllers:

- **Main Controller:** RP2350
- **Wi-Fi Module:** ESP32-C5

> ⚠️ **Important**
>
> RP2350 and ESP32-C5 firmware files are different. Always verify the target controller before starting an update.

| Task | Controller | File | Method |
|---|---|---|---|
| First install / recovery | RP2350 | `.uf2` | USB + BOOTSEL |
| Normal Main Controller update | RP2350 | `.bin` | Web dashboard |
| Wi-Fi Module update | ESP32-C5 | `.bin` | Supported C5 update workflow |

## 🔧 Main Controller — First Installation / Recovery

Use USB + BOOTSEL for first installation, recovery, or a situation where the web dashboard cannot be reached.

1. Prepare the RP2350 `.uf2` firmware file.
2. Connect the RP2350 to the computer using USB while entering BOOTSEL mode.
3. Wait for the RP bootloader USB storage volume to appear.
4. Copy the RP2350 `.uf2` file to the bootloader volume.
5. Wait for the file copy and installation process to complete.
6. Allow the controller to reboot.
7. Restore normal network and power connections if necessary.
8. Connect Ethernet.
9. Open `http://10.10.10.10`.
10. Confirm that the dashboard loads and check the installed firmware version.

> ⚠️ Keep the controller powered during firmware installation.

Do not use the RP2350 `.bin` file with BOOTSEL. The `.bin` file is for the dashboard update path.

## 🔄 Updating the Main Controller

Use this method when the node is already operating normally.

1. Connect to the node.
2. Open the dashboard.
3. Open Firmware Update.
4. Under **MAIN CONTROLLER**, select the RP2350 `.bin` file.
5. Click **UPDATE MAIN CONTROLLER**.
6. Keep power and Ethernet connected.
7. Wait for the upload and reboot.
8. Reconnect to the dashboard.
9. Verify the installed firmware version.

The RP2350 Ethernet firmware-update path was validated on physical hardware from `V3.6F4-RP2350` to `V3.6F5-RP2350`.

## 📡 Wi-Fi Module Firmware

### Initial ESP32-C5 programming

The repository does not contain a proven end-user first-flash procedure for the ESP32-C5 board. Initial Wi-Fi Module programming is currently intended as a developer/service procedure. Do not infer GPIO boot steps, USB behavior, esptool offsets, or partition offsets from this repository.

### Normal Wi-Fi Module update

The production source supports the C5 Wi-Fi-side update workflow using a matching ESP32-C5 `.bin` image. The source-supported endpoints are:

- `POST /api/ota/c5` — submit the image
- `GET /api/ota/c5/status` — read update status

Use only the matching ESP32-C5 image. Do not interchange RP2350 and ESP32-C5 files. The source does not claim automatic rollback after arbitrary runtime failure, firmware signing, Secure Boot, HTTPS, or authentication.

## 📁 Public release artifact names

Release binaries should be attached to a GitHub Release rather than committed as normal repository source files:

- `ND-Pico2DMX-Node-v1.0.0-RP2350.uf2`
- `ND-Pico2DMX-Node-v1.0.0-RP2350.bin`
- `ND-Pico2DMX-Node-v1.0.0-ESP32C5.bin`

The ESP32-C5 artifact should be published only after a valid production binary has been produced and verified.
