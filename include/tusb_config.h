// TinyUSB configuration for GTE Core (lib/tinyusb). The device port is the
// OTG_HS core with its internal full-speed PHY; which rhport that is comes from
// the board's pin map (GTE_USB_DEVICE_RHPORT in the variant header).
#pragma once

#define CFG_TUSB_MCU OPT_MCU_STM32H7
#define CFG_TUSB_OS OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG 0
// STM32duino FreeRTOS 10.3.2 is kernel V10.3.1, which predates pdTICKS_TO_MS.
#ifndef pdTICKS_TO_MS
#define pdTICKS_TO_MS(t) \
  ((uint32_t)(((uint64_t)(t) * 1000u) / configTICK_RATE_HZ))
#endif

// ---- Device: CDC only for now; MSC joins with the Storage Service (#47).
#define CFG_TUD_ENABLED 1
#define CFG_TUD_MAX_SPEED OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64
#define CFG_TUD_MEM_ALIGN __attribute__((aligned(32)))
// Bound each tud_task_ext() call, so the USB_DEVICE task always gets back to
// its watchdog check-in, however busy the bus is.
#define CFG_TUD_TASK_EVENTS_PER_RUN 16
#define CFG_TUD_CDC 1
#define CFG_TUD_CDC_RX_BUFSIZE 512
#define CFG_TUD_CDC_TX_BUFSIZE 512
#define CFG_TUD_CDC_EP_BUFSIZE 64
#define CFG_TUD_MSC 0

// ---- Host: off until the USB_HOST Service (#47).
#define CFG_TUH_ENABLED 0
