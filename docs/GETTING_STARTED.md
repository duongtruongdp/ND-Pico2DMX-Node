# 🚀 Getting Started

If your controller does not already have firmware installed, follow [Installing the Firmware](INSTALLATION.md) first. A programmed controller can be configured directly.

1. Power the node.
2. Connect the lighting computer or console to the node by Ethernet.
3. Configure the computer so it can reach the node at `10.10.10.10`.
4. Open `http://10.10.10.10` in a browser.
5. Assign a universe and input protocol to each required output.
6. Connect DMX cables to the desired outputs and fixtures.
7. Configure the lighting software to send Art-Net or sACN.
8. Verify network activity and output status in the dashboard.
9. Use an output test if needed.
10. Return every tested output to `NETWORK` mode.
11. Begin normal operation.

The production configuration defaults use start universe 1, enabled outputs, and `AUTO` protocol mode. Confirm the live dashboard status before relying on defaults.

For Wi-Fi access, connect to the node's ESP32-C5 access point and use the address documented in [Network Setup](NETWORK_SETUP.md).
