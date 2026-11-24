# TinyUSB on STM32duino for the STM32H743

**Question.** Can TinyUSB run, from an STM32duino/PlatformIO project on the STM32H743VIT6, a USB **device**
(composite CDC + MSC with several LUNs: an SD card and a pass-through of a USB memory stick) on one OTG port
and a USB **host** (MSC, the memory stick) on the other port, both at once? Which integration route works, what
must be switched off in STM32duino, and what are the H7-specific risks?

**Researched:** 2026-11-24. Sources pinned to these revisions:

| Source | Revision read |
|---|---|
| hathach/tinyusb | `master` @ [`93aa62b`](https://github.com/hathach/tinyusb/tree/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d) (2026-11-23). Latest release is **0.21.0** (tag commit `dae3f9a`, 2026-06-30) |
| adafruit/Adafruit_TinyUSB_Arduino | `master` @ [`f88ae55`](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/tree/f88ae552a0a1faf1a24034df768adb2cde28769e), release 3.8.1 (2026-11-20) |
| stm32duino/Arduino_Core_STM32 | `main` @ [`0bac153`](https://github.com/stm32duino/Arduino_Core_STM32/tree/0bac153da08281138042177087090a120abcc971), latest release 3.1.0 (2026-11-12) |
| platformio/platform-ststm32 | release v20.0.0 pins `framework-arduinoststm32 ~4.30000.0`, and `develop` pins `~4.30100.0` ([platform.json](https://github.com/platformio/platform-ststm32/blob/develop/platform.json)) |
| ST | DS12110 Rev 11 (datasheet), RM0433 Rev 9 (reference manual), ES0392 Rev 16 (errata) |

Abbreviations used for links below:
`T/` = `https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/`,
`A/` = `https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/f88ae552a0a1faf1a24034df768adb2cde28769e/`,
`C/` = `https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/`.
In the text they are written out in full as links.

---

## 1. Answer in brief

- **It looks feasible, but nobody has shown this exact setup working on an H743.** The silicon can do it: both
  OTG controllers are full dual-role controllers. TinyUSB has a dwc2 device driver and a dwc2 host driver, and
  it supports one rhport as device and the other as host in the same image. The upstream example
  `examples/dual/host_info_to_device_cdc` uses exactly that split and lists `mcu:STM32H7` as a supported target.
  No public source shows **H743 host mode**, or **device plus host at the same time on any STM32**, passing
  upstream hardware-in-the-loop (HIL) tests. A hardware spike is required.
- **Recommended route: raw TinyUSB, pinned to tag `0.21.0` (or a reviewed later commit), added as a PlatformIO
  `lib_deps` git dependency.** We supply our own `tusb_config.h`, USB clock/GPIO/NVIC init, the two IRQ handlers
  and `tusb_time_millis_api()`. STM32duino's USB stack stays disabled (it is disabled by default under
  PlatformIO).
- **Adafruit_TinyUSB_Arduino is not usable for this.** Its STM32 port is hard-coded to the F4: it includes
  `stm32f4xx_hal.h`, sets `CFG_TUSB_MCU OPT_MCU_STM32F4`, handles only the OTG_FS IRQ, sets
  `CFG_TUH_ENABLED 0`, and is documented as "Still WIP, only support/tested with F4, only support OTG_FS".
  The PRs that would add H7 (#587, #569) are open and unmerged, and they cover the device side only.
- **Main H7 risks:**
  1. Host MSC on dwc2 is young (host driver since 0.18.0, Dec 2024) and has open bugs, for example an
     intermittent MSC read hang ([#4605](https://github.com/hathach/tinyusb/issues/4605), seen on ESP32-S3).
  2. The OTG_HS port must be forced to its internal FS PHY. Set `CFG_TUH_MAX_SPEED` (or `CFG_TUD_MAX_SPEED`)
     to full speed and do **not** enable the ULPI clock.
  3. Host mode needs a crystal-accurate 48 MHz clock (PLL from HSE). HSI48 is not acceptable for the host.
  4. D-cache only matters if USB DMA is enabled. Slave (FIFO) mode is the default and has no coherency issue,
     but the SD card's SDMMC IDMA does need cache care.
  5. The MSC pass-through has no upstream example and must use TinyUSB's async MSC API.

---

## 2. Hardware facts (H743)

### 2.1 Two OTG controllers, both dual-role

- DS12110 §3.40: "The devices embed two USB OTG high-speed (up to 480 Mbit/s) **device/host/OTG** peripheral.
  OTG_HS1 supports both full-speed and high-speed operations, while OTG_HS2 supports only full-speed
  operations. They both integrate the transceivers for full-speed operation (12 Mbit/s) and are able to operate
  from the internal HSI48 oscillator. OTG_HS1 features a UTMI low-pin interface (ULPI) … When using the USB
  OTG_HS1 in HS mode, an external PHY device connected to the ULPI is required." Each controller has 9
  bidirectional endpoints (including EP0), 16 host channels, a 4 KB FIFO and internal USB DMA. Also: "For
  OTG/Host modes, a power switch is needed in case bus-powered devices are connected."
  ([DS12110](https://www.st.com/resource/en/datasheet/stm32h743vi.pdf), p. 52)
- RM0433 §57.1: "The USB OTG is a dual-role device (DRD) controller that supports both device and host
  functions … It can also be configured as a host-only or device-only controller." Table 517 gives both
  instances the same feature set (9 EPs, 16 host channels, 4 KB SRAM, integrated FS PHY). Only OTG_HS1 has ULPI,
  and scatter/gather DMA exists only on OTG_HS1 and only on rev V silicon.
  ([RM0433](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf), pp. 2587–2590)
- **So yes: each port can be host or device.** With no external ULPI PHY, both ports run at full speed
  (12 Mbit/s). That caps MSC throughput to roughly 1 MB/s or less per port.

### 2.2 Naming map (this causes confusion)

| ST datasheet / RM | CMSIS (`stm32h743xx.h`) | Pins (LQFP100) | TinyUSB rhport | IRQ handler |
|---|---|---|---|---|
| OTG_FS = OTG_HS2 = "USB2" | `USB2_OTG_FS` @ 0x4008_0000, alias `USB_OTG_FS` | PA11 DM / PA12 DP (AF10), PA9 VBUS, PA10 ID | **0** | `OTG_FS_IRQHandler` |
| OTG_HS = OTG_HS1 = "USB1" | `USB1_OTG_HS` @ 0x4004_0000, alias `USB_OTG_HS` | PB14 DM / PB15 DP (AF12, internal FS PHY), PB13 VBUS, PB12 ID; or ULPI | **1** | `OTG_HS_IRQHandler` |

Sources: CMSIS header in the core
([stm32h743xx.h](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/system/Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h743xx.h),
lines 2132–2133 and 2699–2702). TinyUSB maps "Port0 to OTG_FS, and Port1 to OTG_HS"
([dwc2_stm32.h L128](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/portable/synopsys/dwc2/dwc2_stm32.h#L128)),
and its H7 BSP forwards `OTG_FS_IRQHandler` to `tusb_int_handler(0, true)` and `OTG_HS_IRQHandler` to
`tusb_int_handler(1, true)`
([hw/bsp/stm32h7/family.c](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/hw/bsp/stm32h7/family.c#L86-L96)).
The DevEBox pin map lists PA11/PA12 for OTG_FS and PB15 for OTG_HS-in-FS
([PeripheralPins_DevEBoxH7xx.c](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/variants/STM32H7xx/H742V%28G-I%29%28H-T%29_H743V%28G-I%29%28H-T%29_H750VBT_H753VI%28H-T%29/PeripheralPins_DevEBoxH7xx.c#L470-L504)).
Note that PB14 is commented out there. That map only matters for STM32duino's own USB stack, but it means our
init code must configure PB14 and PB15 itself. PB14 also carries SDMMC2_D0 (DS12110 pinout), so the SD card
must be on SDMMC1 if OTG_HS is used.

### 2.3 48 MHz clock

- One kernel-clock mux feeds **both** controllers. RCC_D2CCIP2R `USBSEL[1:0]` "USBOTG 1 and 2 kernel clock
  source selection": 01 = `pll1_q_ck`, 10 = `pll3_q_ck`, 11 = `hsi48_ck` (RM0433 §8.7.21 RCC_D2CCIP2R, p. 416).
  The two ports cannot have different 48 MHz sources.
- HSI48 accuracy is −4.5 %/+3.5 % over temperature (DS12110 Table 36). USB full speed needs ±0.25 %, so HSI48
  only works when the CRS trims it. The CRS sync sources are USB2 (OTG_FS) SOF, LSE, or OTG_HS1 SOF (RM0433
  §9.4.2). SOF sync only exists while some port is a **device** receiving SOFs from a PC.
- ES0392 §2.2.3 "Clock recovery system synchronization with USB SOF does not work" applies to **rev Y only**.
  It does not apply to rev X/V
  ([ES0392 Rev 16](https://www.st.com/resource/en/errata_sheet/es0392-stm32h742xig-and-stm32h743xig-device-errata-stmicroelectronics.pdf), Table 3).
- **Conclusion:** because one port is a host (it generates SOF itself and needs an accurate clock even when no
  PC is attached), feed USB from **PLL1Q or PLL3Q off the HSE crystal**. The DevEBox variant's
  `SystemClock_Config()` already does this: HSE 25 MHz, /5 ×96 /10 = 48 MHz on PLL1Q, with
  `UsbClockSelection = RCC_USBCLKSOURCE_PLL`
  ([variant_DevEBoxH7xx.cpp](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/variants/STM32H7xx/H742V%28G-I%29%28H-T%29_H743V%28G-I%29%28H-T%29_H750VBT_H753VI%28H-T%29/variant_DevEBoxH7xx.cpp)).
  PLL1Q also clocks SDMMC and QSPI in that variant, which constrains later clock changes. TinyUSB's own BSPs
  for H743 Nucleo/Eval use PLL3. Its WeAct H743 BSP uses HSI48 *without* CRS, which is device-only and out of
  spec, so it is not a model to follow.
- The USB voltage detector / 3.3 V USB regulator must be enabled (`HAL_PWREx_EnableUSBVoltageDetector()`), as
  the TinyUSB H7 BSP does.

### 2.4 Bus/DMA reachability (only matters with USB DMA)

RM0433 Table 3 (bus-master-to-slave interconnect, p. 104): DTCM (0x2000_0000) is reachable only by the M7 and
MDMA. AXI SRAM (0x2400_0000) and SRAM1–3 (0x3000_0000) are reachable by the D2 masters, including both USB
OTG. TinyUSB's dcache table makes the same point: "DTCM (although USB DMA can't transfer to/from DTCM)"
([dwc2_stm32.h L379](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/portable/synopsys/dwc2/dwc2_stm32.h#L379)).
STM32duino's H743 linker script puts `.data`/`.bss`/heap/stack in `RAM_D1` = AXI SRAM 0x2400_0000
([ldscript.ld](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/variants/STM32H7xx/H742V%28G-I%29%28H-T%29_H743V%28G-I%29%28H-T%29_H750VBT_H753VI%28H-T%29/ldscript.ld#L39-L52)),
so default static buffers are DMA-reachable. SDMMC1's IDMA can reach only AXI SRAM, QSPI and FMC: SRAM1–3 are
not in its column. Pass-through buffers shared between SD and USB therefore belong in AXI SRAM.

### 2.5 Relevant errata (ES0392 Rev 16, OTG_HS section §2.25)

Rev V (REV_ID 0x2003) is what current H743VIT6 parts ship as, and it sits in the last status column of the
errata table.

| Erratum | Rev V status | Relevance |
|---|---|---|
| 2.25.2 TX FIFO corrupted if a FIFO write is interrupted by OTG register access (device mode) | not applicable (rev Y only) | Would matter for slave mode on old silicon |
| 2.25.3 Host TX may hang with FS interface → hub → **low-speed** device | applies, no workaround | Not hit by a stick plugged in directly |
| 2.25.6 False `GINTSTS.CIDSCHG` after reset | applies, workaround | Minor |
| 2.25.7 Possibly wrong data sent instead of a **ZLP**, in slave and DMA modes | applies, workaround (SNAK/CNAK timing) | CDC and MSC both send ZLPs. **The TinyUSB dwc2 DCD has no ES0392 workaround** (grep finds none). Unknown real-world impact; test it |
| 2.25.8 False chirp-K when an FS device connects to a host programmed in HS mode | applies, workaround | Avoided by running the host at FS |
| 2.25.1 USB PHY pull-up drift (after ~86 M resets) | rev Y only | — |

---

## 3. Integration routes

### 3.1 Adafruit_TinyUSB_Arduino: not viable for H7 dual-role

- README: stm32duino is listed under "Cores without built-in support … Still WIP, only support/tested with F4,
  only support OTG_FS". You must call `TinyUSB_Device_Init()` yourself and use `SerialTinyUSB`
  ([README](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/f88ae552a0a1faf1a24034df768adb2cde28769e/README.md)).
- The port files hard-code the F4:
  [tusb_config_stm32.h](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/f88ae552a0a1faf1a24034df768adb2cde28769e/src/arduino/ports/stm32/tusb_config_stm32.h)
  sets `#define CFG_TUSB_MCU OPT_MCU_STM32F4` and `#define CFG_TUH_ENABLED 0 // disable for now`.
  [Adafruit_TinyUSB_stm32.cpp](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/f88ae552a0a1faf1a24034df768adb2cde28769e/src/arduino/ports/stm32/Adafruit_TinyUSB_stm32.cpp)
  includes `stm32f4xx_hal.h`, defines only `OTG_FS_IRQHandler`, overrides `yield()`, and reads the UID from
  the F411 address `0x1FFF7A10`.
  The library's `library.properties` describes the library as supporting "nRF5x, SAMD21, SAMD51, RP2040,
  ESP32-S2/S3, CH32V", so STM32 is not on that list.
- Pending work: [PR #587](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/pull/587) "Support for
  STM32F4xx, STM32F7xx, STM32H7xx, STM32WBxx" (open since 2026-07-08; the author says it was tested on an
  STM32H743VIT6 with audio + CDC, device only) and
  [PR #569](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/pull/569) (F1/F4/G4/WB55, open). On the core
  side, [stm32duino PR #2906](https://github.com/stm32duino/Arduino_Core_STM32/pull/2906) adds a "TinyUSB" USB
  menu option for GenF4 only (open).
- Even with #587 merged, the library's Arduino layer assumes one device port. Its `Adafruit_USBH_Host` targets
  MAX3421E or PIO-USB/native rp2040 host
  ([Adafruit_USBH_Host.cpp](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/f88ae552a0a1faf1a24034df768adb2cde28769e/src/arduino/Adafruit_USBH_Host.cpp)).
  We would end up patching its STM32 port anyway. PR #587 is still a useful **reference for H7 device clock
  and GPIO init**.

### 3.2 Raw TinyUSB as a PlatformIO library: recommended

- The TinyUSB repo ships a `library.json` (name "TinyUSB", version 0.21.0, `"frameworks": "*"`) at its root
  ([library.json](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/library.json)),
  so `lib_deps = https://github.com/hathach/tinyusb.git#0.21.0` works. PlatformIO builds `src/`. Every
  portable driver is guarded by `CFG_TUSB_MCU`/`TUP_USBIP_*`, so only dwc2 compiles. The PlatformIO registry
  has no official hathach package; the only "tinyusb" entry is a third-party 0.17.0 upload, so pin the git tag.
- What we write (TinyUSB's integration guide lists the same steps:
  [docs/integration.rst](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/docs/integration.rst)):
  1. `tusb_config.h` on the include path (for example in `include/`, plus `-I include` for the lib build).
  2. Clock/GPIO/NVIC init for both controllers. `__HAL_RCC_USB2_OTG_FS_CLK_ENABLE()`,
     `__HAL_RCC_USB1_OTG_HS_CLK_ENABLE()` and **not** `..._ULPI_CLK_ENABLE()`. PA11/12 AF10, PB14/15 AF12,
     `HAL_PWREx_EnableUSBVoltageDetector()`. The TinyUSB H7 BSP is the template
     ([family.c L190–277](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/hw/bsp/stm32h7/family.c#L190-L277)).
     That BSP configures port 1 for **ULPI** only, so the PB14/15 internal-PHY variant is ours to write.
     STM32duino's own `USE_USB_HS_IN_FS` path shows the same rule: it skips the ULPI clock
     ([usbd_conf.c L173–177](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/libraries/USBDevice/src/usbd_conf.c#L173-L177)).
  3. `extern "C" void OTG_FS_IRQHandler(void){ tusb_int_handler(0, true); }` and the same for
     `OTG_HS_IRQHandler` with rhport 1. These override the weak startup vectors. STM32duino defines them only
     when `USBCON` is set (see §4).
  4. `uint32_t tusb_time_millis_api(void){ return millis(); }`. This is required with `OPT_OS_NONE`
     ([osal_none.h](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/osal/osal_none.h)).
     Adafruit does the same.
  5. Call `tusb_init(0, &dev_init)` and `tusb_init(1, &host_init)`, then call `tud_task()` and `tuh_task()`
     from `loop()`, exactly as in the integration guide's minimal example, which itself shows "device on port
     0, host on port 1".
  6. Descriptors and class callbacks: CDC+MSC composite descriptor, `tud_msc_get_maxlun_cb()` returning 2, and
     the `tuh_msc_mount_cb`/`umount_cb` callbacks.
- **Reference examples (upstream, not Arduino):**
  [examples/dual/host_info_to_device_cdc](https://github.com/hathach/tinyusb/tree/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/examples/dual/host_info_to_device_cdc)
  (device rhport 0, host rhport 1; `only.txt` includes `mcu:STM32H7`).
  [examples/device/cdc_msc](https://github.com/hathach/tinyusb/tree/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/examples/device/cdc_msc),
  [examples/device/msc_dual_lun](https://github.com/hathach/tinyusb/tree/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/examples/device/msc_dual_lun),
  [examples/host/msc_file_explorer](https://github.com/hathach/tinyusb/tree/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/examples/host/msc_file_explorer).
  None of these is a device-MSC to host-MSC bridge; no such upstream example exists.

### 3.3 Other routes (not researched in depth)

- ST's own USB Device + Host middleware via HAL PCD/HCD. STM32duino bundles both HAL modules
  (`HAL_PCD_MODULE_ENABLED`, `HAL_HCD_MODULE_ENABLED` in
  [stm32h7xx_hal_conf_default.h](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/system/STM32H7xx/stm32h7xx_hal_conf_default.h#L63-L81)),
  but its Arduino USB layer is device-only (CDC/HID). The "USB Host + MSC" PR
  [#1196](https://github.com/stm32duino/Arduino_Core_STM32/pull/1196) has been open since 2020. A hand-rolled
  ST-middleware composite CDC+multi-LUN-MSC plus host would be much more work than TinyUSB.
- [stm32duino PR #2919](https://github.com/stm32duino/Arduino_Core_STM32/pull/2919) "PluggableUSB-based USB
  stack" is open and device-only.

---

## 4. What to disable in STM32duino

- STM32duino's USB device stack (`libraries/USBDevice`) compiles only when `USBCON` is defined
  ([usbd_conf.c L19](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/libraries/USBDevice/src/usbd_conf.c#L19)).
  When `USBCON` is defined, it defines `OTG_FS_IRQHandler`, or `OTG_HS_IRQHandler` with `USE_USB_HS`
  ([usbd_conf.c ~L397](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/libraries/USBDevice/src/usbd_conf.c#L395-L405)),
  which would clash with ours at link time.
- Under PlatformIO, the framework builder adds `USBCON`, `USBD_USE_CDC`/`USBD_USE_HID_COMPOSITE`,
  `HAL_PCD_MODULE_ENABLED` and the USBDevice sources **only** if one of `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC`,
  `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC_WITHOUT_SERIAL` or `PIO_FRAMEWORK_ARDUINO_ENABLE_HID` is in `build_flags`.
  `PIO_FRAMEWORK_ARDUINO_USB_HIGHSPEED[_FULLMODE]` adds `USE_USB_HS[_IN_FS]`
  ([tools/platformio/platformio-build.py L78–126](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/tools/platformio/platformio-build.py#L78-L126)).
  The `devebox_h743vitx` board JSON adds no USB flags
  ([board json](https://github.com/platformio/platform-ststm32/blob/develop/boards/devebox_h743vitx.json)).
  **So: do not set any `PIO_FRAMEWORK_ARDUINO_ENABLE_*`/`USB_HIGHSPEED*` flag, and do not define `USBCON`,
  `USBD_USE_CDC`, `USBD_USE_HID_COMPOSITE`, `USE_USB_HS` or `USE_USB_HS_IN_FS`.** In the Arduino IDE the
  equivalent is Tools → USB support → "None".
- Without `USBCON`, `Serial` is a U(S)ART, not USB CDC
  ([WSerial.h](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/cores/arduino/WSerial.h)).
  Our CDC goes through `tud_cdc_*`, or through a thin `Stream` wrapper of our own.
- HAL PCD/HCD modules stay compiled by default; that is harmless, because TinyUSB does not use them.
- `premain()` enables I-cache and D-cache on M7 unless `I_CACHE_DISABLED`/`D_CACHE_DISABLED` is defined
  ([main.cpp L38–54](https://github.com/stm32duino/Arduino_Core_STM32/blob/0bac153da08281138042177087090a120abcc971/cores/arduino/main.cpp#L38-L54)).
  Keep D-cache on; see §5.1.

---

## 5. Known limitations and risks on the H7

### 5.1 D-cache / DMA coherency

- TinyUSB dwc2 runs in **slave (FIFO, CPU-copy) mode by default**: `CFG_TUD_DWC2_DMA_ENABLE_DEFAULT 0` and
  `CFG_TUH_DWC2_DMA_ENABLE_DEFAULT 0`
  ([tusb_option.h L343–404](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/tusb_option.h#L343-L404)).
  In slave mode the USB core never touches RAM, so the D-cache is irrelevant to USB.
- If DMA is enabled (`CFG_TUD/TUH_DWC2_DMA_ENABLE=1`), the H7 port turns on cache maintenance automatically
  (`CFG_TUD/TUH_MEM_DCACHE_ENABLE_DEFAULT = ..._DMA_ENABLE`) with a 32-byte line size
  ([tusb_mcu.h L309–320](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/common/tusb_mcu.h#L309-L320)).
  STM32 dwc2 cache support with alignment arrived in 0.19.0
  ([changelog 0.19.0](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/docs/changelog/0.19.0.md),
  PR [#2963](https://github.com/hathach/tinyusb/pull/2963)). Buffers handed to the stack must then be 32-byte
  aligned, a multiple of 32 bytes in size, and outside DTCM.
- **Recommendation:** start in slave mode, because full speed does not need DMA. The cache problem that does
  need solving is **SDMMC IDMA ↔ D-cache** for the SD card. Its buffers belong in AXI SRAM, with
  clean/invalidate calls or an MPU non-cacheable region.
- Slave-mode caveat: upstream issue [#3934](https://github.com/hathach/tinyusb/issues/3934) (open) reports CDC IN
  almost stopping while MSC IN is busy in dwc2 slave mode. It was seen on ESP32-P4 at HS; the S3 at FS shared
  bandwidth fairly. This is relevant to our CDC+MSC composite.

### 5.2 OTG_HS with the internal FS PHY

- TinyUSB picks the PHY at init from the compile-time max speed. `dwc2_core_is_highspeed_phy(dwc2,
  prefer_hs_phy)` returns false (FS PHY) when HS is not preferred and the core has an FS PHY
  ([dwc2_common.c L167–177](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/portable/synopsys/dwc2/dwc2_common.c#L167-L177)).
  The device side uses `TUD_OPT_HIGH_SPEED` and the host side uses `_tuh_cfg.use_hs_phy = TUH_OPT_HIGH_SPEED`
  ([hcd_dwc2.c L298](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/portable/synopsys/dwc2/hcd_dwc2.c#L298)).
  On H7, `TUP_RHPORT_HIGHSPEED` is not defined, so it defaults to 0
  ([tusb_mcu.h L876](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/common/tusb_mcu.h#L876)).
  FS is therefore the default, but **set `CFG_TUD_MAX_SPEED` and `CFG_TUH_MAX_SPEED` to `OPT_MODE_FULL_SPEED`
  explicitly**. If HS were selected on OTG_HS without a ULPI PHY, the core would be switched to ULPI and would
  not work.
- When the FS PHY is selected, `dwc2_phy_init()` powers it up (`GCCFG.PWRDWN`) and clears
  `RCC_AHB1LPENR.USB1OTGHSULPILPEN`. The H7 issue behind that: "USB won't work when mcu executing WFI/WFE"
  with the ULPI clock still enabled in sleep
  ([dwc2_stm32.h L174–197](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/portable/synopsys/dwc2/dwc2_stm32.h#L174-L197);
  history in PR [#541](https://github.com/hathach/tinyusb/pull/541) "Allow use of internal FS PHY on OTG_HS"
  and [#2408](https://github.com/hathach/tinyusb/pull/2408)).
- 0.21.0 added "Support full-speed host operation on a high-speed PHY" and "HCD: correct HFIR for low-speed
  devices via internal FS PHY"
  ([changelog 0.21.0](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/docs/changelog/0.21.0.md),
  PR [#3731](https://github.com/hathach/tinyusb/pull/3731)). The internal-FS-PHY host path is maintained but
  recent.
- No upstream H743 BSP uses OTG_HS + internal FS PHY. The H743 Eval and OpenH743I BSPs default to
  `RHPORT_DEVICE=1` (HS via ULPI) and `RHPORT_HOST=0`.

### 5.3 48 MHz clocking

See §2.3. Use PLL1Q (already 48 MHz in the DevEBox variant) or PLL3Q from HSE. Do not use HSI48 for a
host port. The rev Y CRS erratum only matters if someone later tries HSI48+CRS for a device-only build.

### 5.4 Dual-port support in TinyUSB (rhport numbering, `CFG_TUSB_RHPORTx_MODE`)

- In the current API, roles and ports are chosen **at runtime**:
  `tusb_init(rhport, &(tusb_rhport_init_t){.role = TUSB_ROLE_DEVICE/HOST, .speed = …})`. The config only needs
  `CFG_TUD_ENABLED 1` and `CFG_TUH_ENABLED 1`, plus the max speeds.
  `CFG_TUSB_RHPORT0_MODE`/`CFG_TUSB_RHPORT1_MODE` are a legacy fallback that only derives
  `CFG_TUD_ENABLED`/`CFG_TUH_ENABLED` and the max speed when those are not set
  ([tusb_option.h L510–562](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/tusb_option.h#L510-L562)).
- Only one device-stack instance and one host-stack instance exist. "Device on port 0 + host on port 1" (or the
  reverse) is supported; "two devices" is not.
- On STM32 the dwc2 controller table holds both OTG_FS and OTG_HS whenever the CMSIS header defines both, which
  is true for the H743
  ([dwc2_stm32.h L38–52, L128–137](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/portable/synopsys/dwc2/dwc2_stm32.h#L38-L52)).
  `EP_MAX_FS = EP_MAX_HS = 9` and `DFIFO_DEPTH = 1024` words (4 KB), matching RM0433 Table 517.
- Upstream HIL (hardware-in-the-loop) coverage
  ([docs/reference/hil_boards.md](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/docs/reference/hil_boards.md),
  [test/hil/tinyusb.json](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/test/hil/tinyusb.json)):
  - `stm32h743nucleo`: device only (slave and DMA variants); host false, dual false.
  - `stm32f723disco`: device + host ("Device port0 FS (slave only), Host port1 HS with DMA"), dual **false**.
  - The dual tests run only on non-STM32 boards (rp2040, LPC4088, i.MX RT1064, SAMD51, Fruit Jam).
  - **The H743 has HIL coverage for device mode only.** Host is verified only on the F7 (same dwc2 IP family,
    different core revision and PHY), and dual-role at runtime is not verified on any STM32.

### 5.5 Host MSC maturity on dwc2

- dwc2 host support (control/bulk/interrupt, FS/HS, split transactions, hubs) and DMA support both arrived in
  **0.18.0, released 2024-12-25**
  ([changelog 0.18.0](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/docs/changelog/0.18.0.md)).
  0.19.0 and 0.21.0 added "MSC Host: continue async I/O improvements", a fully asynchronous host core, and many
  dwc2 HCD fixes. The README support table lists STM32 "F2, F4, F7, H7, H7RS" as Device ✅ Host ✅ Highspeed ✅
  on dwc2
  ([README.rst](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/README.rst)).
- The host MSC class supports multiple LUNs (`CFG_TUH_MSC_MAXLUN`, default 4) and async
  `tuh_msc_read10`/`write10` with completion callbacks
  ([msc_host.h](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/class/msc/msc_host.h)).
  Block count and size are cached at enumeration for LUN 0 only, per the header comment.
- Open dwc2-host issues of note as of 2026-11-24 (57 open issues match "dwc2 host"):
  - [#4605](https://github.com/hathach/tinyusb/issues/4605): MSC read intermittently hangs mid-transfer (dwc2
    host, ESP32-S3, slave mode 3/8 runs at `93aa62b`). The suspected cause is a timing-sensitive lost channel
    interrupt in the dwc2 HCD, so it is not specific to ESP32.
  - [#4203](https://github.com/hathach/tinyusb/issues/4203): STM32F723 non-DMA host CDC echo intermittently
    reads nothing.
  - [#3931](https://github.com/hathach/tinyusb/issues/3931): aborting fails to suppress transfer-complete IRQs
    in slave mode.
  - [#4528](https://github.com/hathach/tinyusb/issues/4528) and
    [#4541](https://github.com/hathach/tinyusb/issues/4541): 0.21.0 audit follow-ups, including the dwc2 DMA
    disconnect sequence and "hardware validation owed".
  - Project activity is very high (daily commits). Pin a known-good commit and re-test before any bump.

### 5.6 Device MSC with several LUNs and the pass-through LUN

- Device MSC supports multiple LUNs via `tud_msc_get_maxlun_cb()` ("required for multiple LUNs
  implementation") and supports **async I/O**. `tud_msc_read10_cb`/`write10_cb` may return
  `TUD_MSC_RET_ASYNC` and finish later with `tud_msc_async_io_done(bytes, in_isr)`
  ([msc_device.h L45–93, L133–134](https://github.com/hathach/tinyusb/blob/93aa62bce0f2708c9d0794b1947f50f9f1a0ac0d/src/class/msc/msc_device.h#L45-L93)).
  This async path arrived in 0.19.0.
- The pass-through LUN therefore maps naturally onto TinyUSB's API:
  1. `tud_msc_read10_cb(lun=1, …)` calls `tuh_msc_read10(dev_addr, 0, buf, lba, n, cb)` and returns
     `TUD_MSC_RET_ASYNC`.
  2. The host completion callback calls `tud_msc_async_io_done()`.
  3. Both stacks run from the same `loop()`.

  Things to design for:
  - **Block size:** `CFG_TUD_MSC_EP_BUFSIZE` must be at least the stick's block size, and LBA/size are
    reported 1:1 to the PC.
  - **Hot-plug:** report "not ready" (`tud_msc_test_unit_ready_cb` → false plus sense data) when no stick is
    mounted, and signal media change on attach.
  - **Bus resets:** a device bus reset abandons a pending async I/O, but the I/O still owns the buffer until
    `async_io_done` is called (header comment).
  - **Throughput:** FS on both sides with a store-and-forward copy roughly halves throughput.
- No upstream or Adafruit example implements this bridge. Adafruit's `examples/DualRole/MassStorage` only does
  a host-side file explorer and a data logger.

---

## 6. Sketch of the configuration (untested)

```ini
; platformio.ini (excerpt) - no PIO_FRAMEWORK_ARDUINO_ENABLE_* / USB_HIGHSPEED* flags
[env:gte_core]
platform = ststm32
framework = arduino
board = devebox_h743vitx
lib_deps = https://github.com/hathach/tinyusb.git#0.21.0   ; or a reviewed commit SHA
build_flags = -I include -DCFG_TUSB_CONFIG_FILE=\"tusb_config.h\"
```

```c
// include/tusb_config.h (essentials)
#define CFG_TUSB_MCU            OPT_MCU_STM32H7
#define CFG_TUSB_OS             OPT_OS_NONE
#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_FULL_SPEED
#define CFG_TUH_ENABLED         1
#define CFG_TUH_MAX_SPEED       OPT_MODE_FULL_SPEED   // keeps OTG_HS on its internal FS PHY
// DMA off (default) -> slave mode, no dcache maintenance needed for USB
#define CFG_TUD_CDC             1
#define CFG_TUD_MSC             1
#define CFG_TUD_MSC_EP_BUFSIZE  512
#define CFG_TUH_MSC             1
#define CFG_TUH_DEVICE_MAX      1                     // stick directly attached, no hub
#define CFG_TUH_ENUMERATION_BUFSIZE 256
```

Ports: device on rhport 0 (OTG_FS, PA11/12) and host on rhport 1 (OTG_HS internal PHY, PB14/15), or the
reverse. Either works in principle. The choice depends on the PCB (which connector gets the VBUS power switch)
and on the fact that PB14 is also SDMMC2_D0. `CFG_TUSB_MCU` must be visible to the TinyUSB library build; a
`build_flags` `-D` or the config file on the `-I` path both work.

---

## 7. Open unknowns that only a hardware spike can settle

1. Host MSC on an H743 dwc2 core (rev V, slave mode, OTG_HS internal FS PHY) enumerating and sustaining reads
   and writes on real sticks (several vendors, including ones that report 4 KB blocks or several LUNs). Watch
   for the #4605-type hang.
2. Device and host running at the same time on one H743 (rhport 0 device + rhport 1 host). Nobody upstream
   tests this on STM32.
3. CDC responsiveness while MSC is busy in slave mode (#3934 pattern), and whether DMA mode (with the cache
   rules) is needed.
4. Whether ES0392 2.25.7 (wrong data instead of a ZLP) shows up with CDC/MSC traffic, given that TinyUSB has no
   explicit workaround.
5. Pass-through LUN behaviour under Windows/macOS/Linux: timeouts while the host side is slow, stick hot-plug
   and media change, bus reset during a pending async I/O.
6. Coexistence with STM32duino: SysTick/`millis()`, IRQ priorities relative to SDMMC, and no hidden user of
   the OTG IRQ vectors once `USBCON` is off. Also whether the variant's PLL1Q 48 MHz stays valid if the
   SDMMC/QSPI clocks are changed.
7. VBUS: how the host port switches and limits VBUS on our board (DS12110 says a power switch is required), and
   whether the device port uses VBUS sensing (PA9/PB13). The WeAct BSP notes a replug enumeration problem with
   sensing off.
