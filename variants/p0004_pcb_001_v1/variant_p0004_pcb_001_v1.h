// GTE LCD Board, P0004-PCB-001 v1: STM32H743VIT6 with a 25 MHz HSE crystal,
// core at 480 MHz.
//
// Built on STM32duino's generic H743V variant, used in place from the framework
// package (scripts/framework_variant.py); this header and
// variant_p0004_pcb_001_v1.cc hold everything board-specific. Pins come from
// sw-001's include/pinout.h, which is the source of truth (it reflects rework
// done after the schematic); pins it doesn't list come from the
// research/lcd-board-pinout note.
#pragma once

// Debug UART: USART2, TX PD5 / RX PD6, to CN3 and the STLINK-V3 VCP. This is
// Serial.
#define SERIAL_UART_INSTANCE 2
#define PIN_SERIAL_TX PD5
#define PIN_SERIAL_RX PD6

#include <variant_generic.h>

// ---- From sw-001 pinout.h
// ------------------------------------------------------------------ TFT (SPI4)
#define GTE_PIN_TFT_DC PE1
#define GTE_PIN_TFT_RST PE0
#define GTE_PIN_TFT_BL PE5
#define GTE_PIN_TFT_CS PE4
#define GTE_PIN_TFT_MOSI PE6
#define GTE_PIN_TFT_MISO PE13  // dummy pin
#define GTE_PIN_TFT_SCLK PE2
// W5500 (SPI2)
#define GTE_PIN_W5500_CS PB9
#define GTE_PIN_W5500_MISO PC2_C
#define GTE_PIN_W5500_MOSI PC1
#define GTE_PIN_W5500_SCLK PB13
#define GTE_PIN_W5500_RST PC6
// RGB LED
#define GTE_PIN_LED_RED PA10
#define GTE_PIN_LED_GREEN PA8
#define GTE_PIN_LED_BLUE PA9
// Internal I2C (I2C4): EEPROM 0x50, TMP102, IO Board connector
#define GTE_PIN_I2C_INT_SCL PD12
#define GTE_PIN_I2C_INT_SDA PD13
// Buzzer
#define GTE_PIN_BUZZER PB0
// Soft Keys 1-4, left to right under the LCD (issue #12). pinout.h's
// left/right/up/down names were misleading: left = SW1, right = SW2, up = SW3,
// down = SW4 (reworked to PE14).
#define GTE_PIN_SOFTKEY_1 PE10
#define GTE_PIN_SOFTKEY_2 PE12
#define GTE_PIN_SOFTKEY_3 PE9
#define GTE_PIN_SOFTKEY_4 PE14

// ---- Not in pinout.h: from the research/lcd-board-pinout note
// ------------------------------
#define GTE_PIN_W5500_INT PC7
#define GTE_PIN_IO_BOARD_ALERT \
  PD11  // TMP102 ~ALERT and the IO Expander interrupt
#define GTE_PIN_EXPANSION_FAULT PE8
#define GTE_PIN_USB_HOST_VBUS_EN PD14
#define GTE_PIN_USB_HOST_VBUS_FAULT PD15
#define GTE_PIN_SD_DETECT PB8  // SDMMC1: PC8-PC11 D0-D3, PC12 CK, PD2 CMD
