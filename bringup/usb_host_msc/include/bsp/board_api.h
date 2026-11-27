// Minimal stand-in for TinyUSB's bsp/board_api.h, just what the vendored cdc_msc example uses.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "stm32h7xx.h"

// USB serial number string from the STM32 96-bit unique ID, as UTF-16 hex digits
static inline size_t board_usb_get_serial(uint16_t desc_str1[], size_t max_chars)
{
  const uint8_t *uid = (const uint8_t *)UID_BASE;
  static const char hex[] = "0123456789ABCDEF";
  size_t n = 0;
  for (size_t i = 0; i < 12 && n + 2 <= max_chars; i++)
  {
    desc_str1[n++] = hex[uid[i] >> 4];
    desc_str1[n++] = hex[uid[i] & 0xF];
  }
  return n;
}
