// Copyright 2026 M0WUT
// An I2C bus with a mutex (issue #10: one mutex per shared resource, inside the
// code that owns it) and devices on it. Every failed transfer is reported to
// the caller; callers treat a failed read as a Fatal Error (issue #13). probe()
// and the bus scan come with issue #45.
#pragma once
#include <cstddef>
#include <cstdint>

class TwoWire;

namespace gte {

class I2CBus {
 public:
  I2CBus(uint32_t sda, uint32_t scl, uint32_t hz = 400000)
      : sda_(sda), scl_(scl), hz_(hz) {}
  void Begin();
  void Lock();
  void Unlock();
  TwoWire *wire() { return wire_; }

 private:
  uint32_t sda_, scl_, hz_;
  TwoWire *wire_ = nullptr;
  void *mutex_ = nullptr;
};

class I2CDevice {
 public:
  I2CDevice(I2CBus *bus, uint8_t address) : bus_(bus), address_(address) {}
  // Register address of 1 or 2 bytes (big-endian on the wire). Return false on
  // any NACK/short transfer.
  bool Read(uint16_t reg, uint8_t reg_bytes, uint8_t *data, size_t len);
  bool Write(uint16_t reg, uint8_t reg_bytes, const uint8_t *data, size_t len);
  bool Acked();  // address-only transfer: does the device ACK right now?
  uint8_t address() const { return address_; }

 protected:
  I2CBus *bus_;
  uint8_t address_;
};

}  // namespace gte
