# 🔄 Firmware Update

For first installation or recovery, see [Installing the Firmware](INSTALLATION.md). This page covers normal updates while the node is already operating.

## Main Controller update

The validated RP2350 production update workflow is:

1. Open the dashboard over direct Ethernet.
2. Select the `MAIN CONTROLLER` firmware file.
3. Select the matching RP2350 `.bin` file.
4. Click `UPDATE MAIN CONTROLLER`.
5. Keep power and Ethernet connected.
6. Allow the node to upload and reboot.
7. Reconnect to the dashboard.
8. Confirm the new firmware version and normal status.

> ⚠️ **File format**
>
> Use `.bin` for the dashboard Main Controller update. Do not upload `.uf2` through the dashboard. UF2 is for USB/BOOTSEL installation or recovery.

The RP2350 Ethernet firmware-update path was validated on physical hardware from `V3.6F4-RP2350` to `V3.6F5-RP2350`.

## Wi-Fi Module update

The ESP32-C5 production firmware has its own Wi-Fi-side update path. Use the supported C5 workflow and the matching ESP32-C5 `.bin` image only when a Wi-Fi Module update is specifically required. Do not use an RP2350 image for the C5, and do not assume an update is complete until the C5 status reports the new version and normal operation.

The source-supported C5 endpoints are:

- `POST /api/ota/c5` for the update
- `GET /api/ota/c5/status` for status

The validated baseline is RP2350 `V3.6F5-RP2350` and C5 `V3.6A-C5`.
