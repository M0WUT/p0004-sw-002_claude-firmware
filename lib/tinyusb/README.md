# TinyUSB (vendored)

[TinyUSB](https://github.com/hathach/tinyusb) 0.21.0 (tag `0.21.0`, commit `dae3f9a`), MIT licence (`LICENSE`). Copied as delivered from the USB spike (issue #18), keeping only the core, the class drivers and the Synopsys dwc2 port that the STM32H743 uses. Not reformatted or linted.

GTE's configuration is `include/tusb_config.h`; the USB_DEVICE Service (`lib/services/src/gte/usb_device_service.cc`) owns the stack.
