---
id: 4
title: TinyUSB on STM32duino for the H743
labels: [wayfinder:research]
status: closed
assignee: claude-research-agent
parent: 1
blocked_by: []
---

## Question

Can TinyUSB be used from an STM32duino (PlatformIO, `framework = arduino`, `platform = ststm32`) project on the STM32H743VIT6 to run a USB device (composite CDC + MSC with more than one LUN) on one OTG port and a USB host (MSC class, a USB memory stick) on the other port at the same time? Which integration route exists (Adafruit_TinyUSB_Arduino, raw tinyusb as a PlatformIO library, other), what has to be disabled in STM32duino's own USB stack, and what are the known limitations or blockers on the H7 (cache/DMA, OTG_HS with internal FS PHY, clocking)?

## Resolution

Full findings: branch `research/tinyusb-stm32duino-h743` (commit debd39e, local only), file `docs/research/tinyusb-stm32duino-h743.md`.

- **Feasible on paper, unproven in practice.** Both H743 OTG ports are dual-role and run at full speed without an external PHY. TinyUSB numbers them rhport 0 (OTG_FS) and rhport 1 (OTG_HS, internal PHY). Upstream tests the H743 in device mode only. Host mode is tested only on an F723, and device and host together on no STM32.
- **Route**: raw TinyUSB pinned to 0.21.0 as a PlatformIO git dependency. We supply our own `tusb_config.h`, clock/pin/IRQ setup, both IRQ handlers and `tusb_time_millis_api()`, and set each port's role at runtime.
  - Adafruit_TinyUSB_Arduino is unusable: F4 only, OTG_FS only, no host.
  - Leave STM32duino's USB stack off: no `PIO_FRAMEWORK_ARDUINO_ENABLE_*`, `USBCON`, `USBD_USE_*` or `USE_USB_HS*`. This also means `Serial` is a UART. **This overrides the `-DUSE_USB_HS -DUSE_USB_HS_IN_FS` hint in the pinout ticket.**
- **Risks**:
  - Host MSC on the STM32 driver is young (since 0.18.0), and open bug #4605 shows intermittent MSC read hangs.
  - OTG_HS must be forced to full speed on its internal PHY. TinyUSB's H7 board code doesn't cover that.
  - **Clock**: both ports share one 48 MHz source, and the research says the host side needs it from HSE via PLL, not HSI48. **This conflicts with the pinout finding that the MCU has no HSE crystal.**
  - USB isn't affected by the D-cache in FIFO (non-DMA) mode.
  - Erratum ES0392 2.25.7: a wrong packet can be sent instead of a zero-length packet, with no TinyUSB workaround.
  - Pass-through LUN: there's no upstream example. It needs TinyUSB's async MSC calls (`TUD_MSC_RET_ASYNC`, then `tuh_msc_read10`/`write10`, then `tud_msc_async_io_done`).
- **Board-specific**: the upstream dual example uses device on rhport 0 and host on rhport 1. This board is the **reverse**: host on OTG_FS (rhport 0), device on OTG_HS (rhport 1).
- **For a hardware spike**: host MSC stability; device and host running at once; the clock source; CDC stalling while MSC is busy (#3934); the zero-length-packet erratum; pass-through behaviour on each OS; VBUS switching and sensing. This became [USB spike on the LCD Board](0018-usb-spike-on-the-lcd-board.md).
