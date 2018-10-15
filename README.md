# LEDMATE (on XilDebug)

## What is it?

A Thumbinator that renders the LEDMATE aka ws2812 144*8 ledstrip screen.

It reads commands over USB-CDC.

- export ACM_DEVICE=/dev/ttyACM0 (or whetever)
- stty -F $ACM_DEVICE 115200 raw -clocal -echo icrnl
- Now you can communicate with it nicely.

## Protocol

TBD

# Building

Make sure you have a recent gcc-arm-none-eabi toolchain. 7.2.1 is known to work.

`make` in the root builds the firmware.

# Debugging/flashing

Install a recent version of `openocd`.

Connect a debugger (e.g. an STLink or a CMSIS-DAP compliant debugger) to the SWD pins.

`make daplink` and `make stlink` starts `openocd` with the appropriate flags to debug a XilDebug device.

`make flash` flashes the firmware to the device.

# Usage

TODO

# Contributing

TBD

# License

TBD

