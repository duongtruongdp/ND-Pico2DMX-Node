# Dashboard guide

The browser dashboard provides:

- System Overview
- Output Configuration
- Wi-Fi Configuration
- Output Test
- Firmware Update

## Output tests

- `FULL` sets the selected output to full for testing.
- `BLACKOUT` sets the selected output to zero.
- `NETWORK` returns control to Art-Net or sACN.

**After testing an output, return it to `NETWORK` for normal operation.**

## Signal Merge

- `Latest Signal (LTP)` uses the most recently received source.
- `Highest Value (HTP)` uses the highest channel value.
- `First Source (BLOCK)` keeps the first active source until it releases control.

The output cards show signal activity, protocol, sender, input rate, DMX output rate, and test state. Use the status information before troubleshooting cables or fixtures.
