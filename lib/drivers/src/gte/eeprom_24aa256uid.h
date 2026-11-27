// Microchip 24AA256UID on I2C4 at 0x50: 32 KiB, 64-byte pages, factory ID and
// EUI-48 (MAC) at the top of the array. Writes are split at page boundaries and
// each page waits for the chip's write cycle by acknowledge polling. (sw-001's
// driver never split pages: see issue #43.)
#pragma once
#include "gte/i2c_bus.h"

namespace gte {

class Eeprom24AA256UID : public I2CDevice {
 public:
  static constexpr uint8_t kAddress = 0x50;
  static constexpr size_t kPage = 64;
  static constexpr uint16_t kMacAddr = 0x7F7A;

  using I2CDevice::I2CDevice;
  bool Detect();  // manufacturer 0x29, device 0x48
  bool ReadBytes(uint16_t addr, uint8_t *data, size_t len);
  bool WriteBytes(uint16_t addr, const uint8_t *data, size_t len);
  bool ReadMac(uint8_t mac[6]) { return ReadBytes(kMacAddr, mac, 6); }

  // For the fatal loop only: interrupts are masked, so this drives I2C4's
  // registers directly with loop-count timeouts. One page at most; no wait for
  // the write cycle afterwards.
  static bool PolledWritePage(uint16_t addr, const uint8_t *data, size_t len);

 private:
  bool WaitWriteCycle();
};

}  // namespace gte
