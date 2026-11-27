// CRC8 as sw-001's code computes it (RobTillaart CRC8(7, 0, 0x55)): polynomial
// 0x07, initial value 0x00, no reflection, final XOR 0x55. sw-001's
// docs/eeprom.md said XOR 0; the code (and this repo's docs/eeprom.md) use 0x55
// so an all-zero block doesn't get a CRC of 0.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gte {

inline uint8_t Crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                         : static_cast<uint8_t>(crc << 1);
  }
  return crc ^ 0x55;
}

}  // namespace gte
