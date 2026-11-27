// Copyright 2026 M0WUT
#include "gte/soft_keys.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <cstdio>

#include "gte/gte.h"
#include "gte/watchdog.h"

namespace gte {

static const uint32_t kPins[4] = {GTE_PIN_SOFTKEY_1, GTE_PIN_SOFTKEY_2,
                                  GTE_PIN_SOFTKEY_3, GTE_PIN_SOFTKEY_4};

void SoftKeys::Begin() {
  for (uint32_t p : kPins) pinMode(p, INPUT_PULLUP);
}

uint8_t SoftKeys::Mask() {
  uint32_t o = *reinterpret_cast<volatile uint32_t *>(kOverrideAddr);
  if ((o >> 16) == 0x4B45) return static_cast<uint8_t>(o & 0x0F);
  uint8_t m = 0;
  for (int i = 0; i < 4; i++)
    if (digitalRead(kPins[i]) == LOW) m |= 1u << i;
  return m;
}

bool SoftKeys::HeldFor(uint8_t keys, uint32_t hold_ms,
                       void (*while_waiting)()) {
  uint32_t start = millis();
  while ((Mask() & keys) == keys) {
    if (millis() - start >= hold_ms) return true;
    if (while_waiting) while_waiting();
    delay(20);
  }
  return false;
}

void SoftKeys::StartTask() { xTaskCreate(Task, "keys", 512, this, 2, nullptr); }

void SoftKeys::Task(void *arg) {
  SoftKeys *self = static_cast<SoftKeys *>(arg);
  int wd = watchdog().RegisterTask("CORE/keys");
  uint8_t last = self->Mask();
  for (;;) {
    uint8_t m = self->Mask();
    for (int i = 0; i < 4; i++) {
      uint8_t bit = 1u << i;
      if ((m ^ last) & bit) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Soft Key %d %s", i + 1,
                 (m & bit) ? "pressed" : "released");
        HelperLog("SoftKeys", LogLevel::kInfo, msg);
      }
    }
    last = m;
    watchdog().Checkin(wd);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace gte
