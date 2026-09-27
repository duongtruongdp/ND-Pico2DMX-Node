# 🧰 Troubleshooting

## Dashboard does not open

- Check power.
- Check the Ethernet cable.
- Confirm the computer can reach `10.10.10.10`.
- Try the direct Ethernet connection rather than Wi-Fi.
- Check whether the node status LED and network link are active.

## No DMX output

- Confirm the fixture is powered.
- Confirm the DMX cable and line termination.
- Check the selected universe.
- Check the selected protocol mode.
- Confirm the output is in `NETWORK`, not `BLACKOUT`.
- Use `FULL` briefly as a local output test, then return to `NETWORK`.

## Wrong universe

Check the universe assigned to that physical port and the universe configured in the lighting software. Port 1 is not required to use Universe 1.

## Output stuck FULL or BLACKOUT

Open the output test controls and select `NETWORK`. A local test overrides network input until it is stopped.

## Art-Net works on the wrong or no port

Check the port universe, port enable state, and protocol mode. Use `AUTO` for a simple first test, or select `Art-Net` explicitly.

## sACN is not received

Check the sACN universe, port enable state, and protocol mode. Select `sACN` explicitly if automatic selection is not appropriate.

## Wi-Fi unavailable

Confirm the ESP32-C5 access point is visible as `ND-DMX-C5`, then verify the documented password and use `192.168.4.1`. Direct Ethernet remains the primary recovery and setup path.

## Firmware update fails

- Confirm the correct `.bin` file is selected for the target controller.
- Do not upload UF2 through the dashboard.
- Keep power and Ethernet connected during the update.
- Wait for the node to reboot before trying again.
- Reconnect to the dashboard and check the reported firmware version.

> 🔧 **Main Controller recovery**
>
> If the Main Controller cannot boot or the dashboard cannot be reached after a firmware problem, follow [Main Controller — First Installation / Recovery](INSTALLATION.md#-main-controller--first-installation--recovery). Network connectivity problems should be investigated first when the controller is otherwise running.

For advanced diagnosis, consult the source and archived bring-up tests. Operators should not need to debug internal transport details for normal use.
