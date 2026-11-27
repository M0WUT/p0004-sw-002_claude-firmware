// Copyright 2026 M0WUT
#include "gte/watchdog.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <cstdio>

#include "gte/config_storage.h"
#include "gte/gte.h"

namespace gte {

int Watchdog::RegisterTask(const char *name, uint32_t deadline_ms) {
  if (count_ >= kMaxTasks) return -1;
  tasks_[count_] = Entry{name, deadline_ms, millis()};
  return static_cast<int>(count_++);
}

void Watchdog::Checkin(int id) {
  if (id >= 0 && static_cast<size_t>(id) < count_)
    tasks_[id].last_ms = millis();
}

void Watchdog::Feed() { IWDG1->KR = 0xAAAA; }

void Watchdog::Start() {
  // Freeze the IWDG while a debugger halts the core.
  DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
  // LSI ~32 kHz / 64 = 500 Hz; 2000 counts = 4 s.
  IWDG1->KR = 0xCCCC;
  IWDG1->KR = 0x5555;
  IWDG1->PR = 4;  // /64
  IWDG1->RLR = kTimeoutMs / 2;
  while (IWDG1->SR) {
  }
  Feed();
  uint32_t now = millis();
  for (size_t i = 0; i < count_; i++) tasks_[i].last_ms = now;
  xTaskCreate(Supervisor, "watchdog", 512, this, configMAX_PRIORITIES - 1,
              nullptr);
}

void Watchdog::Supervisor(void *arg) {
  Watchdog *self = static_cast<Watchdog *>(arg);
  bool reported = false;
  uint32_t last_debug = 0;
  for (;;) {
    uint32_t now = millis();
    const char *late = nullptr;
    for (size_t i = 0; i < self->count_; i++)
      if (now - self->tasks_[i].last_ms > self->tasks_[i].deadline_ms)
        late = self->tasks_[i].name;
    if (!late) {
      Feed();
      if (now - last_debug >= 1000) {
        last_debug = now;
        HelperLog("Watchdog", LogLevel::kDebug, "All tasks checked in; fed");
      }
    } else if (!reported) {
      reported = true;
      storage().WriteRecord(RecordKind::kWatchdog, 0, late, "");
      char msg[64];
      snprintf(msg, sizeof(msg), "%s missed its check-in; resetting", late);
      HelperLog("Watchdog", LogLevel::kError, msg);
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

const char *ResetReason() {
  static const char *reason = nullptr;
  if (!reason) {
    uint32_t rsr = RCC->RSR;
    if (rsr & (RCC_RSR_IWDG1RSTF | RCC_RSR_WWDG1RSTF))
      reason = "watchdog";
    else if (rsr & RCC_RSR_BORRSTF)
      reason = "power-on";
    else if (rsr & RCC_RSR_SFTRSTF)
      reason = "software";
    else if (rsr & RCC_RSR_PINRSTF)
      reason = "pin";
    else
      reason = "unknown";
    RCC->RSR |= RCC_RSR_RMVF;
  }
  return reason;
}

}  // namespace gte
