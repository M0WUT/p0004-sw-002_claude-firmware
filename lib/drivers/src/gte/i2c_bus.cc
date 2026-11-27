#include "gte/i2c_bus.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <Wire.h>
#include <semphr.h>

namespace gte {

void I2CBus::Begin() {
  wire_ = new TwoWire(sda_, scl_);
  wire_->begin();
  wire_->setClock(hz_);
  mutex_ = xSemaphoreCreateRecursiveMutex();
}

void I2CBus::Lock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(mutex_),
                            portMAX_DELAY);
}

void I2CBus::Unlock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(mutex_));
}

static void PutReg(TwoWire *w, uint16_t reg, uint8_t reg_bytes) {
  if (reg_bytes == 2) w->write(static_cast<uint8_t>(reg >> 8));
  w->write(static_cast<uint8_t>(reg));
}

bool I2CDevice::Read(uint16_t reg, uint8_t reg_bytes, uint8_t *data,
                     size_t len) {
  bus_->Lock();
  TwoWire *w = bus_->wire();
  w->beginTransmission(address_);
  PutReg(w, reg, reg_bytes);
  bool ok = w->endTransmission(false) == 0;
  if (ok) {
    size_t got = w->requestFrom(static_cast<uint16_t>(address_),
                                static_cast<size_t>(len), true);
    ok = got == len && w->readBytes(data, len) == len;
  }
  bus_->Unlock();
  return ok;
}

bool I2CDevice::Write(uint16_t reg, uint8_t reg_bytes, const uint8_t *data,
                      size_t len) {
  bus_->Lock();
  TwoWire *w = bus_->wire();
  w->beginTransmission(address_);
  PutReg(w, reg, reg_bytes);
  size_t n = w->write(data, len);
  bool ok = w->endTransmission() == 0 && n == len;
  bus_->Unlock();
  return ok;
}

bool I2CDevice::Acked() {
  bus_->Lock();
  TwoWire *w = bus_->wire();
  w->beginTransmission(address_);
  bool ok = w->endTransmission() == 0;
  bus_->Unlock();
  return ok;
}

}  // namespace gte
