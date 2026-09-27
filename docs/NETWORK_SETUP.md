# Network setup

## Ethernet

The RP2350 controller is reachable over direct Ethernet at:

```text
10.10.10.10
```

Open the address in a browser to reach the dashboard. The computer must be configured so it can communicate with that node address.

## Wi-Fi

The ESP32-C5 production access point is:

```text
SSID:      ND-DMX-C5
Password:  nddmx1234
Address:   192.168.4.1
```

Ethernet and Wi-Fi are two ways to reach and control the node. The RP2350 remains the authoritative controller and dashboard/API source.

## Art-Net and sACN

Art-Net and sACN are supported lighting-network protocols. Choose the protocol mode independently for each output:

- `AUTO` — accept the supported network protocol automatically.
- `Art-Net` — use Art-Net for that output.
- `sACN` — use sACN for that output.

If an output does not respond, check its universe and protocol mode in the dashboard.
