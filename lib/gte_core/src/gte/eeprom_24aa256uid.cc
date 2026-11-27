// Copyright 2026 M0WUT
#include "gte/eeprom_24aa256uid.h"

#include <Arduino.h>

namespace gte {

bool Eeprom24AA256UID::Detect() {
  uint8_t id[2];
  return ReadBytes(0x7FFA, id, 2) && id[0] == 0x29 && id[1] == 0x48;
}

bool Eeprom24AA256UID::ReadBytes(uint16_t addr, uint8_t *data, size_t len) {
  while (len) {
    size_t n = len > 64 ? 64 : len;  // sequential reads may cross pages; 64
                                     // keeps transfers short
    if (!Read(addr, 2, data, n)) return false;
    addr += n;
    data += n;
    len -= n;
  }
  return true;
}

bool Eeprom24AA256UID::WaitWriteCycle() {
  // tWC is 5 ms max; the chip NACKs its address until the write is done.
  uint32_t start = millis();
  do {
    if (Acked()) return true;
  } while (millis() - start < 20);
  return false;
}

bool Eeprom24AA256UID::WriteBytes(uint16_t addr, const uint8_t *data,
                                  size_t len) {
  while (len) {
    size_t room =
        kPage -
        (addr % kPage);  // never cross a page: the chip would wrap within it
    size_t n = len < room ? len : room;
    if (!Write(addr, 2, data, n) || !WaitWriteCycle()) return false;
    addr += n;
    data += n;
    len -= n;
  }
  return true;
}

bool Eeprom24AA256UID::PolledWritePage(uint16_t addr, const uint8_t *data,
                                       size_t len) {
  I2C_TypeDef *i2c = I2C4;
  if (len == 0 || len > kPage || (addr % kPage) + len > kPage) return false;
  uint32_t guard = 0;
  while (i2c->ISR & I2C_ISR_BUSY)
    if (++guard > 2000000) return false;
  i2c->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;
  i2c->CR2 = (static_cast<uint32_t>(kAddress) << 1) |
             (static_cast<uint32_t>(len + 2) << I2C_CR2_NBYTES_Pos) |
             I2C_CR2_AUTOEND | I2C_CR2_START;
  uint8_t header[2] = {static_cast<uint8_t>(addr >> 8),
                       static_cast<uint8_t>(addr)};
  for (size_t i = 0; i < len + 2; i++) {
    guard = 0;
    while (!(i2c->ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF)))
      if (++guard > 2000000) return false;
    if (i2c->ISR & I2C_ISR_NACKF) {
      i2c->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
      return false;
    }
    i2c->TXDR = i < 2 ? header[i] : data[i - 2];
  }
  guard = 0;
  while (!(i2c->ISR & I2C_ISR_STOPF))
    if (++guard > 2000000) return false;
  i2c->ICR = I2C_ICR_STOPCF;
  return true;
}

}  // namespace gte
