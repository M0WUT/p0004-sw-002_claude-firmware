---
id: 2
title: Toolchain proven: sw-001 flashed to the LCD Board
labels: [wayfinder:task]
status: closed
assignee: M0WUT
parent: 1
blocked_by: []
---

## Question

Install PlatformIO, build the existing sw-001 firmware (`M0WUT/p0004-sw-001_generic-arduino-firmware`), flash it to the LCD Board over the attached STLINK-V3, and capture its debug log (does the debug UART on PD5/PD6 reach the STLINK-V3 virtual COM port?). Record versions installed, the exact build/flash/monitor commands, and anything that had to be changed to make it work on Linux (e.g. `monitor_port = COM3`).

## Resolution

**Done: sw-001 builds, flashes and boots on the LCD Board from this Linux machine.**

**One-time setup** (needed `sudo`, done by the user):
- `python3-venv`;
- PlatformIO's `99-platformio-udev.rules` (gives the STLINK-V3 and `/dev/ttyACM0` mode 0666);
- `usermod -aG dialout`.

PlatformIO Core 6.2.0 is installed in `~/.platformio/penv`. Its packages pulled in xPack OpenOCD 0.12.0+dev and the GCC ARM toolchain.

**Commands** (run from the sw-001 checkout):
```sh
PLATFORMIO_BUILD_FLAGS="-UM0WUT_HA_SETTINGS" ~/.platformio/penv/bin/pio run -e gte_lcd             # build
PLATFORMIO_BUILD_FLAGS="-UM0WUT_HA_SETTINGS" ~/.platformio/penv/bin/pio run -e gte_lcd -t upload   # flash over ST-Link (OpenOCD)
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200                                # debug log
```
- `-UM0WUT_HA_SETTINGS`: sw-001 sets `-D M0WUT_HA_SETTINGS`, which includes the gitignored `m0wut_ha_login.h` (the user's MQTT credentials). Without it the build falls back to Home Assistant disabled with empty credentials.
- On Linux, `monitor_port = COM3` must be `/dev/ttyACM0` or left out.
- A bare `cat /dev/ttyACM0` showed nothing. pyserial (which `pio device monitor` uses) asserts DTR and works.

**Results**:
- Build: RAM 71,200 B (13.6%), flash 393,616 B (18.8%). The only warnings come from libraries (PubSubClient `boolean`, RTC BCD).
- Flash: programmed and verified OK. SWD halt/resume and memory reads work, so OpenOCD can be used as a debugger.
- Boot log over the STLINK-V3 VCP at 115200: the CAT table, the Ethernet config, "W5500 detected" (SPI2), and Home Assistant and Webpage defaults created. Halting afterwards found the CPU in `spi_transfer` (W5500 polling), with CFSR and HFSR both 0: no fault.
- `sw-001` builds with `-D DEBUG_EEPROM`, which uses a 32 KB **RAM fake EEPROM**. So config never persists across a restart in that build, and the real 24AA256UID wasn't touched.

**Facts for other tickets**:
- **There is an HSE clock, contrary to [LCD Board pinout from the schematic](0003-lcd-board-pinout-from-the-schematic.md).** `RCC_CR` = 0x3f03c025 (HSEON and HSERDY set), `RCC_CFGR` SWS = PLL1, `RCC_PLLCKSELR` = 0x00f0f052 (PLLSRC = HSE, DIVM1 = 5, which fits a 25 MHz HSE). This eases the 48 MHz USB clock question in [USB spike on the LCD Board](0018-usb-spike-on-the-lcd-board.md).
- USART2 runs from an 80 MHz kernel clock (BRR = 694 for 115200). PD5/PD6 are in AF7.
- The STDC14 connector (CN4) carries the VCP through 100R resistors to PD6 (pin 13, target RX) and PD5 (pin 14, target TX).
- For [GTE Core and Instrument repo structure](0008-gte-core-and-instrument-repo-structure.md): developer-only secrets such as `m0wut_ha_login.h`, and debug-only build flags (`DEBUG_EEPROM`), need a home.

### Note from the user

The HSE is expected: every fix from the GitHub issues on the LCD Board repo has been applied to this board. `DEBUG_EEPROM` is deliberate for now, to make debugging easier without persistent state.

### Later finding: VM USB controller

The dev machine is a VirtualBox guest. On the emulated **EHCI** controller the STLINK-V3 VCP silently lost most of its serial stream, while SWD and flashing still worked. On **xHCI** the debug_uart hardware test passes cleanly. Keep the VM on USB 3.0 (xHCI), with a USB filter for the STLINK-V3.
