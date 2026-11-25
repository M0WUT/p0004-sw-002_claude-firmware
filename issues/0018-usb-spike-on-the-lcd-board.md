---
id: 18
title: USB spike on the LCD Board
labels: [wayfinder:task]
status: closed
assignee: M0WUT
parent: 1
blocked_by: [2]
---

## Question

Can raw TinyUSB (0.21.0) on this LCD Board run a USB device (CDC plus at least one MSC LUN) on OTG_HS in internal-PHY full-speed mode (rhport 1, PB14/PB15) **and** a USB host reading a memory stick over MSC on OTG_FS (rhport 0, PA11/PA12) at the same time, under STM32duino with its own USB stack disabled? Find out:

- which 48 MHz clock source to use. An HSE (25 MHz) is present and drives PLL1, so a PLL from HSE is available as well as HSI48 with CRS;
- how the host port's VBUS enable (PD14) and fault (PD15) behave;
- whether a pass-through LUN (async MSC from host to device) works at all;
- that it all works with TinyUSB built for FreeRTOS (`CFG_TUSB_OS = OPT_OS_FREERTOS`), with `tud_task()` and `tuh_task()` in separate tasks (see [Execution model](0010-execution-model.md)).

Per the map's Notes, the code that exercises hardware lands in `hardware_tests/` (for later production testing), not on a throwaway branch. Record what worked, the clock configuration, and every failure seen. See [TinyUSB on STM32duino for the H743](0004-tinyusb-on-stm32duino-for-the-h743.md) for the risks to probe.

## Progress log (2026-11-25)

**Status**: claimed by M0WUT and in progress. Stage 1 (host only) is built and run. It is blocked on three answers from the user (see "Waiting on the user" below).

**Code**: [hardware_tests/usb_host_msc/](../hardware_tests/usb_host_msc/). This is a PlatformIO project: FreeRTOS (STM32duino FreeRTOS 10.3.2, with stack-overflow checking on via `include/STM32FreeRTOSConfig.h`) plus TinyUSB 0.21.0, vendored and trimmed in `lib/tinyusb` with its own `library.json`. Host runs on OTG_FS (rhport 0). Logging goes to USART2 **and** to a RAM ring (`spikeLogRing`/`spikeLogHead`) that is read over SWD with `python3 hardware_tests/tools/swd_ram_log.py <firmware.elf>`. Build and flash with `~/.platformio/penv/bin/pio run -d hardware_tests/usb_host_msc -t upload`.

**Confirmed on hardware**:
- 25 MHz HSE, PLL1 gives a 480 MHz system clock, and USB takes 48 MHz from PLL1Q (USBSEL = 1). No clock work is needed.
- FreeRTOS and TinyUSB host build and run: 36 KB RAM, 54 KB flash.
- STM32duino core 3.0.0 (`framework-arduinoststm32` 4.30000.0, platform ststm32 20.0.0) names the serial class `Uart(PinName rx, PinName tx)`, e.g. `Uart dbg(PD_6, PD_5)`.

**Finding: host VBUS fault.** As soon as PD14 (U7 EN) goes high, PD15 (U7 ~FAULT) goes low and stays low, and the stick never enumerates. Log: [logs/2026-11-25-vbus-fault-ramlog.txt](../hardware_tests/usb_host_msc/logs/2026-11-25-vbus-fault-ramlog.txt). VBUS was switched off over SWD and the CPU **left halted**, so the spike can't re-enable it. Re-flashing or resetting runs the spike again, which re-enables VBUS.
- U7: TPS2553DBVR-1, IN = 5V via F4, ILIM R50 = 47k (roughly 550 mA, by my reading), output 66 µF.
- Candidates: no 5V at U7's input on the bench; the stick drawing more than the limit, or a short on 5V_VBUS; or the "-1" variant latching off after one overcurrent event.

**Waiting on the user** (asked 2026-11-25; question 2 answered yes and done, see below):
1. How is the LCD Board powered on the bench? Is 5V present at U7/F4?
2. May the spike toggle EN to test for latch-off? What stick is it, and does its LED light?
3. (Answered: the debug UART goes through the STLINK-V3 VCP.)

**Side finding: debug UART loss is the VirtualBox VM's fault.** This Linux machine is a VirtualBox guest. The STLINK-V3 is passed through on the emulated **EHCI** controller, and the VCP stream loses large chunks there. SWD and flashing are unaffected. The same firmware prints perfectly in minicom on the Windows host. The user is switching the VM's USB controller to **USB 3.0 (xHCI)**. After that, rerun `python3 hardware_tests/debug_uart/run_test.py` to confirm (it checks sequence gaps and the RX echo). If xHCI doesn't fix it, the fallbacks are: read the port on the Windows host and forward it over TCP (`tcp_serial_redirect.py`, then `socket://host:port` in pyserial/pio), or keep using the SWD RAM log.

**Update 2026-11-25: UART fixed.** After switching the VM to USB 3.0 (xHCI) and re-attaching the STLINK-V3, `hardware_tests/debug_uart/run_test.py --seconds 15` **passes**: 599 lines (about 600 expected), 0 gaps, 0 corrupt, RX echo OK. The debug UART can be trusted again, so the SWD RAM log is now only a fallback. The board is currently running the debug_uart test firmware, which doesn't drive PD14.

**Update 2026-11-25: the VBUS fault is on the board side.** Using `hardware_tests/tools/vbus_toggle.tcl` (OpenOCD drives PD14 and samples PD15 while the debug_uart firmware runs), with the user's OK:
- **With the stick**: 3 of 3 tries showed the fault first at 10–12 ms after EN high, asserted for the rest of the 300 ms, and clear 1 s after EN low.
- **With no stick**: identical, 3 of 3, fault at 10–11 ms and held.
- So U7 reports a continuous overcurrent with **no load fitted**. The ~10 ms matches the overcurrent deglitch time. **This is a board-side fault.**
- Checks handed to the user: (1) 5V present at U7 pin 1 / after F4; (2) unpowered resistance from 5V_VBUS to 0V (a short, or D3 USBLC6-2P6 rotated, or C40/C41/C47); (3) R50 = 47k and fitted on U7 pin 5 (ILIM); (4) watch 5V_VBUS while EN is pulsed, to tell a real overload from a false FAULT.
- Results: (1) **5V is present at U7 pin 1** (user measured). The 0.35 V on 5V_VBUS with EN off is just the output caps discharging. (2), (3) and (4) are still to do.
- 2-second pulses (`hardware_tests/tools/vbus_pulse.tcl`): the fault was held for the whole of every pulse.
- **RESOLVED 2026-11-25: the user found and fixed a soldering defect** on the board. After the rework, `vbus_toggle.tcl` with no stick gave 3 of 3 tries with **0 fault samples**, and PD15 stayed 1. Host VBUS now switches cleanly. It was an assembly defect on this board, not a design issue, so no PCB GitHub issue is needed.
- Note: unplugging the STLINK-V3 (e.g. for rework) hands it back to Windows. Re-attach it in VirtualBox via Devices → USB.

**STAGE 1 PASSED (2026-11-25), host MSC on OTG_FS under FreeRTOS.** This followed a second user rework (a soldering fault had held D+ and D− low; diagnosed by reading PA11/PA12 as plain GPIO inputs: both 0 even with no pull, so the stick's 1.5k pull-up wasn't seen). Log: [logs/2026-11-25-stage1-uart-after-rework.log](../hardware_tests/usb_host_msc/logs/2026-11-25-stage1-uart-after-rework.log).
- Enumeration 455 ms after boot: VID ABCD / PID 1234, "General UDisk" rev 5.00, 1 LUN, 983,040 × 512 B (480 MiB), sector 0 signature 55 AA.
- Sustained read: 3 passes × 64 MiB in 32 KiB chunks. 6144 reads, **0 failures, 0 timeouts, no hang** (tinyusb#4605 not reproduced). Steady **1166 KiB/s** (~9.5 Mbit/s, close to the full-speed ceiling).
- VBUS fault (PD15) monitored throughout, via an edge ISR plus a 5 ms poll: **0 edges**.
- Confirmed on the board: TinyUSB `CFG_TUSB_OS = OPT_OS_FREERTOS` with `tuh_task()` in its own task works; host on rhport 0 (OTG_FS, PA11/PA12, AF10, `__HAL_RCC_USB2_OTG_FS_CLK_ENABLE()`); USB clock from PLL1Q; NVIC priority at `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`.

**STAGE 2: device and host running together (2026-11-25).** `pio run -e stage2` (`SPIKE_DEVICE=1`) adds a CDC + MSC device (TinyUSB cdc_msc example descriptors and RAM disk, vendored in `src/device/`; VID:PID CAFE:4003) on OTG_HS internal FS PHY: PB14/PB15 AF12 (`GPIO_AF12_OTG1_FS`), `__HAL_RCC_USB1_OTG_HS_CLK_ENABLE()` with the ULPI clock disabled (also in sleep), and `tud_task()` in its own task. The user wired a USB cable to the IO Board connector's D+/D−/GND (no VBUS). Log: [logs/2026-11-25-stage2-boot.log](../hardware_tests/usb_host_msc/logs/2026-11-25-stage2-boot.log).
- **Windows configured the device at 351 ms** (CDC line state set at 374 ms) and showed the TinyUSB MSC drive. **At the same time** the host stick mounted at 458 ms, with inquiry, capacity and sector 0 correct. No VBUS faults; free heap 413 KB.
- So **device on OTG_HS and host on OTG_FS run together on one H743 under FreeRTOS**, which is what the research couldn't confirm.
- **PC-side checks PASSED** (CAFE:4003 passed through to the VM by the user):
  - CDC (`/dev/serial/by-id/usb-TinyUSB_TinyUSB_Device_<UID>-if00`): **500 of 500 echoes byte-exact**, 15.7 KB in 0.15 s.
  - MSC (8 KB RAM disk): auto-mounted by Linux; README.TXT read OK; a file written, then **read back intact after unmount and remount**. Windows had also written to it earlier (`System Volume Information`).
- Still to do: sustained traffic on both ports **at once** (host stick reads during CDC and MSC traffic), then stage 3 (pass-through LUN).
- Note: after flashing, the STLINK-V3 VCP can re-enumerate as `/dev/ttyACM1`. Open it as `/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*-if02`.

**STAGE 3 PASSED (2026-11-25): stick pass-through as read-only LUN 1.** `pio run -e stage3` (`SPIKE_PASSTHROUGH=1`). The 8 KB RAM disk is LUN 0. LUN 1 is the host-port stick: `src/device/passthrough.cpp` dispatches the `tud_msc_*` callbacks by LUN. For LUN 1, read10 returns `TUD_MSC_RET_ASYNC` and calls `tuh_msc_read10` straight into the device's endpoint buffer (`CFG_TUD_MSC_EP_BUFSIZE` 4096 = 8 sectors), and the host completion callback (in the host task) calls `tud_msc_async_io_done`. Calling the host API from the device task worked. LUN 1 is write-protected (sense DATA PROTECT 27h), not-ready until the stick mounts, and takes its capacity from the stick.
- The PC saw `GTE USB passthrough` at 480 MB, `ro=1`, with the stick's FAT partition auto-mounted.
- 32 MiB of files read at **500 KiB/s**, re-read after a remount **byte-identical**, and a write was refused (read-only filesystem).
- 500 KiB/s is about half of direct host reads, because each 4 KB chunk is read from the stick and then sent to the PC, one after the other. Double-buffering or queueing would overlap them; that's for the Storage Service design.

**pytest suite** (user rule): `hardware_tests/conftest.py` holds the shared fixtures (flash with per-session caching, SWD reset, a UART capture with `wait_for`, the SWD RAM log, udisks mount/remount). Tests: `debug_uart/test_debug_uart.py` (TX gaps, RX echo); `usb_host_msc/test_usb_host.py` (3 × 64 MiB); `test_usb_device.py` (CDC echo, RAM-disk write survives remount, **concurrent load**: host passes while CDC and RAM disk are hammered); `test_usb_passthrough.py`. Run with `hardware_tests/.venv/bin/pytest hardware_tests -v` (venv from `hardware_tests/requirements.txt`). The VM clock must be NTP-synced or pip fails TLS.

**Next steps once unblocked**:
2. ~~Resolve the VBUS fault~~ (done). ~~Finish stage 1~~ (PASSED, see above): enumeration, inquiry, capacity, sector 0, and 3 passes of the sustained-read test looking for hangs (tinyusb#4605), with the fault pin monitored throughout (the user asked for this).
3. ~~Stage 2: device on OTG_HS~~ (device enumerates alongside host; PC-side tests pending): (PB14/PB15, rhport 1, AF12 OTG1_FS, `__HAL_RCC_USB1_OTG_HS_CLK_ENABLE()` with the ULPI clock **disabled**, `SPIKE_DEVICE=1`). **The device port only reaches the IO Board connector (pins 23/21), and the IO Board doesn't exist.** The user needs to wire a USB breakout to those pins first. Ask them.
4. Stage 3: pass-through LUN (async MSC), then CDC and MSC alongside host.
5. Convert hardware_tests to **pytest** (user rule, see the map's Notes) and record the resolution.

## Resolution

**Yes: the whole USB design works on this LCD Board.** The test is the pytest suite in [hardware_tests/](../hardware_tests/): `hardware_tests/.venv/bin/pytest hardware_tests -v`. **7 of 7 passed** on 2026-11-25 (9 min 8 s, log in `hardware_tests/logs/pytest-run-2026-11-25.txt`).

- **Host on OTG_FS** (rhport 0, PA11/PA12 AF10, VBUS switch U7 on PD14, fault on PD15): a stick enumerates, and 3 × 64 MiB reads at **1166 KiB/s** with 0 errors, 0 timeouts and no hang (tinyusb#4605 not seen). PD15 stays clear.
- **Device on OTG_HS internal FS PHY** (rhport 1, PB14/PB15 AF12, ULPI clock off, no VBUS sensing) runs **at the same time as the host**. A CDC + MSC composite device (CAFE:4003 test IDs) enumerates on Windows and Linux.
- **Concurrent load**: 599,126 CDC echoes and 187 RAM-disk write/remount/read cycles, all with 0 errors, during the full host read test. CDC doesn't stall while MSC is busy (#3934 not seen).
- **Pass-through LUN**: the stick appears to the PC as a read-only LUN via TinyUSB async MSC (`TUD_MSC_RET_ASYNC`, then `tuh_msc_read10`, then `tud_msc_async_io_done`). 32 MiB read identically twice, 17,806 reads with 0 errors, at **500 KiB/s**: half the direct rate, because each read is read-then-send with no overlap.
- **Clock**: 25 MHz HSE, PLL1Q gives 48 MHz for USB. There's no clock issue.
- **Stack**: raw TinyUSB 0.21.0 (vendored, trimmed `lib/tinyusb` with its own `library.json`), `CFG_TUSB_OS = OPT_OS_FREERTOS`, STM32duino FreeRTOS 10.3.2 (needs a `pdTICKS_TO_MS` shim). STM32duino's own USB stack stays disabled. `tud_task()` and `tuh_task()` run in separate tasks, and calling `tuh_msc_read10` from the device task works.
- **Hardware fixes made along the way** (by the user): a solder fault that made U7 report a continuous overcurrent, and a solder fault holding D+ and D− low on the host port. Both were assembly defects on this board, not design issues.
- **Dev environment facts**:
  - The VirtualBox VM must use xHCI, or the ST-Link VCP drops data.
  - The device holds itself disconnected for 1.5 s after boot (`tud_disconnect()`/`tud_connect()`), so a VM USB filter re-captures it after a reset.
  - Open serial ports via `/dev/serial/by-id/`.
- **Not tested here**: SD card behind MSC (SDMMC), pass-through **writes**, macOS, stick hot-plug while the PC is reading. These are for [How USB storage is presented to a PC](0014-how-usb-storage-is-presented-to-a-pc.md) and the Storage build.
