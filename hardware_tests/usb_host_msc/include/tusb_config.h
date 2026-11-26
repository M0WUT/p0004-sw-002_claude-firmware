#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_STM32H7
#endif
#define CFG_TUSB_OS            OPT_OS_FREERTOS
// STM32duino FreeRTOS 10.3.2 is kernel V10.3.1, which predates pdTICKS_TO_MS
#ifndef pdTICKS_TO_MS
#define pdTICKS_TO_MS(t) ((uint32_t)(((uint64_t)(t) * 1000u) / configTICK_RATE_HZ))
#endif
#define CFG_TUSB_DEBUG         0

// Board wiring: host on OTG_FS (PA11/PA12, rhport 0), device on OTG_HS internal FS PHY (PB14/PB15, rhport 1)
#define BOARD_TUH_RHPORT       0
#define BOARD_TUD_RHPORT       1

//------------- Host -------------//
#define CFG_TUH_ENABLED        1
#define CFG_TUH_MAX_SPEED      OPT_MODE_FULL_SPEED
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB            0
#define CFG_TUH_DEVICE_MAX     1
#define CFG_TUH_MSC            1
#define CFG_TUH_MSC_MAXLUN     4
#define CFG_TUH_MEM_ALIGN      __attribute__((aligned(32)))

//------------- Device (off for the host-only stage) -------------//
#ifndef SPIKE_DEVICE
#define SPIKE_DEVICE 0
#endif
#ifndef SPIKE_PASSTHROUGH
#define SPIKE_PASSTHROUGH 0
#endif
#ifndef GTE_LUN0_SD
#define GTE_LUN0_SD 0
#endif
#define CFG_TUD_ENABLED        SPIKE_DEVICE
#define CFG_TUD_MAX_SPEED      OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64
#define CFG_TUD_MEM_ALIGN      __attribute__((aligned(32)))
#define CFG_TUD_CDC            1
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256
#define CFG_TUD_CDC_EP_BUFSIZE 64
#define CFG_TUD_MSC            1
#define CFG_TUD_MSC_EP_BUFSIZE 4096 // 8 sectors per pass-through transfer

#endif
