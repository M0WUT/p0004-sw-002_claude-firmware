---
id: 1
title: GTE Core firmware
labels: [wayfinder:map]
status: open
assignee:
parent:
blocked_by: []
---

## Destination

A PlatformIO (STM32duino) project in this repo (P0004-SW-002, GTE Core) that builds and runs on the LCD Board over the ST-Link. Every core Service works: System, Network, Web, Display, UI, Thermal, Power (including INA3221), Storage, USB Device (CDC/SCPI and mass-storage pass-through of SD and USB stick) and USB Host, SCPI (over USB CDC and TCP 5025), and Home Assistant. Every Config Parameter is validated, applied and saved to EEPROM. Every Config Parameter and Status Value appears on the web page, in Home Assistant (read-only) and over SCPI. IO Board features run against a stand-in until the board exists. One example Instrument under `examples/` shows that an Instrument Service can add its own sensors without touching core code, apart from one new `ServiceId` entry.

## Notes

- **Execution is carried into this map.** The destination is working firmware, not just a spec. Once the decision tickets that a piece of the build depends on are closed, graduate build tickets (`wayfinder:task`) out of the fog and work them here.
- Domain: embedded firmware for the STM32H743VIT6, STM32duino on PlatformIO, LVGL 9, TinyUSB, W5500. Use the terms in [GLOSSARY.md](../GLOSSARY.md); consult the `domain-modeling` skill when terms shift.
- Reference implementation to port from (not to copy blindly): `M0WUT/p0004-sw-001_generic-arduino-firmware`. It already has BaseService, typed config fields, an EEPROM config allocation table, a web page, Ethernet/NTP, Home Assistant and LVGL on the ST7789. No Service in it is finished. In particular, config updates are validated but never written to `_config.data`.
- Hardware: LCD Board (`M0WUT/p0004-pcb-001_lcd-board`) is built and attached through an STLINK-V3. IO Board (`M0WUT/p0004-pcb-002_io-board`) is not built yet. Requirements: `M0WUT/p0004_testkit-generic` → `requirements/` (REQ-001 LCD Board, REQ-002 system).
- GTE Core must be usable by each Instrument repo as a git submodule pinned to a tag. Editing the `ServiceId` enum per Instrument is acceptable; anything else an Instrument needs must be added, not changed, in core.
- Home Assistant is read-only. Config is edited only from the web page and SCPI.
- USB is built on TinyUSB.
- SCPI uses [Vrekrer_scpi_parser](https://github.com/Vrekrer/Vrekrer_scpi_parser) (the user's choice).
- The user prefers to be asked, not assumed, on hardware facts.
- **Every hardware test goes in a `hardware_tests/` folder in this repo**, kept and maintained so it can be reused for production testing of boards. Spikes that exercise hardware are tidied into `hardware_tests/` rather than thrown away. (User rule, 2026-11-25.) **All hardware tests must run under pytest**: a `test_*.py` per test, with shared fixtures (flash firmware, serial port, SWD) in `hardware_tests/conftest.py`.
- **Dev machine is a VirtualBox VM** (Linux guest on a Windows host), with the STLINK-V3 passed through by USB. SWD and flashing work. The ST-Link VCP lost data on the emulated EHCI controller. **The VM must use the USB 3.0 (xHCI) controller**, and then the debug UART is clean.
- The LCD Board on the bench has **every fix from the GitHub issues on `M0WUT/p0004-pcb-001_lcd-board` applied**, so the board differs from the schematic in that repo. Check those issues before trusting a schematic-only finding.
- sw-001's `-D DEBUG_EEPROM` (config in a RAM fake EEPROM, nothing persists) is deliberate for now, to make debugging easier without persistent state.
- Pins: sw-001's `include/pinout.h` is the source of truth. The schematic is used only for pins it doesn't define (see [LCD Board pinout from the schematic](0003-lcd-board-pinout-from-the-schematic.md)).

## Decisions so far

<!-- one line per closed ticket: - [<title>](file): <gist> -->

- [LCD Board pinout from the schematic](0003-lcd-board-pinout-from-the-schematic.md): SD on SDMMC1 4-bit, USB host on OTG_FS, USB device on OTG_HS-in-FS via the IO Board, a single I2C4 bus, 1 UART and 2 I2C user ports (the "HSI only" finding was wrong: an HSE is present). `pinout.h` is the source of truth where it disagrees with the schematic.
- [Runtime-generated Home Assistant entities](0007-runtime-generated-home-assistant-entities.md): ArduinoHA 2.1.0 can create entities at runtime (fixed-size registry, silent overflow, no removal); config becomes diagnostic sensors via a small in-repo subclass that adds `entity_category`.
- [Sharing storage between the firmware and a connected PC](0005-sharing-storage-between-the-firmware-and-a-connected-pc.md): one writer per FAT volume; PC read-only by default plus a user-initiated handover is the suggested route; use STM32SD and FatFs (CPU transfers, block functions map onto TinyUSB MSC).
- [Fitting the Vrekrer SCPI parser to GTE Core](0006-fitting-the-vrekrer-scpi-parser-to-gte-core.md): pin v0.6.1; one parser instance, with our own line reader per transport; limits of 255 and a 32-bit hash; needs a ~100-line `Execute()` subclass, and Core supplies all IEEE 488.2 and SCPI-99 commands, status registers and the error queue; generic `CONF:` commands avoid running out of command slots.
- [TinyUSB on STM32duino for the H743](0004-tinyusb-on-stm32duino-for-the-h743.md): raw TinyUSB 0.21.0 with STM32duino's USB stack disabled; feasible on paper but dual-role is unproven on any STM32, the ports are reversed from the upstream example, and the 48 MHz source conflicts with the missing HSE; a hardware spike is needed.
- [The Service contract](0009-the-service-contract.md): register, then `initialise()` (no calls between Services), then `tick()` (the only place for calls between Services, looked up by `ServiceId`); Config Parameters declare a value pointer, an Apply Mode (Live or Reset) and a secret flag, and generic code checks, writes and saves them; Status Values are typed with units; `_error()` is fatal (LCD message, red LED at 0.5 Hz, UART repeat every 2 s, no reboot).
- [Toolchain proven: sw-001 flashed to the LCD Board](0002-toolchain-proven-sw-001-flashed-to-the-lcd-board.md): PlatformIO 6.2.0 builds, flashes (OpenOCD) and debugs; the log reads on `/dev/ttyACM0` at 115200; the board has a 25 MHz HSE driving PLL1; sw-001 uses a RAM fake EEPROM and a private HA-credentials header.
- [Execution model](0010-execution-model.md): STM32duino FreeRTOS started after every `initialise()`; one task per Service (declared stack, priority and period); a lock per Service, never held while calling another; one task owns LVGL; mutexes inside `I2CBus`, Network (W5500) and `LoggingHelper`; Fatal Errors suspend the scheduler.
- [USB spike on the LCD Board](0018-usb-spike-on-the-lcd-board.md): proven on hardware: TinyUSB 0.21.0 under FreeRTOS runs host MSC on OTG_FS and a CDC+MSC device on OTG_HS at the same time, with no errors under load; a read-only pass-through LUN works at 500 KiB/s. These are now reusable pytest hardware tests.

## Not yet specified

- **Logging**: what each Service logs, the file format and rotation on SD/USB stick, and how logging relates to the debug UART and the LoggingHelper.
- **Power-source indication**: how USB-PD and PoE negotiation status (REQ-002 UX-02/UX-03) is detected and shown. The IO Board hardware for this isn't designed yet.
- **Testing strategy**: what is unit-tested natively on the PC versus only on hardware, and how the stand-in IO Board is used in tests.
- **The web page beyond sw-001**: layout once there are many Services, grouping, and Sensors.
- **Which Instrument is the example**, and what its Instrument Service and Instrument Board stand-in do.
- **Watchdog**: whether an IWDG is added to catch hung tasks, once there's a real hang to justify it. The fatal loop would feed it.
- **Build order**: how the implementation is sliced into build tickets once the decisions above close.

## Out of scope

- Firmware update over USB or Ethernet (REQ-002 CONN-08): a later effort. USB mass storage is designed so it doesn't rule this out.
- Prometheus export (CONN-12).
- WiFi (CONN-09/CONN-10).
- The IO Expander (STM32F030) firmware. Only its I2C interface is in scope.
