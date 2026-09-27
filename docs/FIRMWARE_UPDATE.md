# Firmware updates

## Main controller update

The validated RP production update workflow is:

1. Open the dashboard over direct Ethernet.
2. Select the `MAIN CONTROLLER` firmware file.
3. Select the RP firmware `.bin` file.
4. Click `UPDATE MAIN CONTROLLER`.
5. Keep power connected.
6. Keep Ethernet connected.
7. Allow the node to reboot.
8. Reconnect to the dashboard.
9. Confirm the new firmware version and normal status.

Use `.bin` for the dashboard RP update. **Do not upload `.uf2` through the dashboard.** UF2 is for USB/BOOTSEL recovery or initial installation.

## Wi-Fi module update

The ESP32-C5 production firmware has its own Wi-Fi-side update path. Use the C5 Wi-Fi endpoint and the matching ESP32-C5 `.bin` image only when a C5 update is specifically required. Do not use an RP image for the C5, and do not assume an update is complete until the C5 status reports the new version and normal operation.

The validated baseline is RP `V3.6F5-RP2350` and C5 `V3.6A-C5`.
