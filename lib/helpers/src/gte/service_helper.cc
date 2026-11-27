#include "gte/service_helper.h"

#include <STM32FreeRTOS.h>

#include <cstdio>

#include "gte/gte.h"
#include "gte/watchdog.h"

namespace gte {

void ServiceHelper::RegisterService(BaseService *s) {
  if (count_ < kMaxServices) {
    s->CaptureDefaults();
    services_[count_++] = s;
  }
}

void ServiceHelper::InitialiseAll() {
  for (size_t i = 0; i < count_; i++) services_[i]->RunInitialise();
}

BaseService *ServiceHelper::Get(ServiceId id) {
  for (size_t i = 0; i < count_; i++)
    if (services_[i]->id() == id) return services_[i];
  return nullptr;
}

BaseService *ServiceHelper::ForTask(void *task) {
  for (size_t i = 0; i < count_; i++)
    if (services_[i]->task_handle == task) return services_[i];
  return nullptr;
}

void ServiceHelper::TaskMain(void *arg) {
  BaseService *s = static_cast<BaseService *>(arg);
  const TickType_t period = pdMS_TO_TICKS(s->task_settings().tick_period_ms);
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    s->RunTick();
    watchdog().Checkin(s->watchdog_id);
    vTaskDelayUntil(&last, period);
  }
}

void ServiceHelper::Start() {
  for (size_t i = 0; i < count_; i++) {
    BaseService *s = services_[i];
    const auto &t = s->task_settings();
    uint32_t deadline = t.tick_period_ms * 2 > Watchdog::kDefaultDeadlineMs
                            ? t.tick_period_ms * 2
                            : Watchdog::kDefaultDeadlineMs;
    s->watchdog_id = watchdog().RegisterTask(s->name(), deadline);
    TaskHandle_t h = nullptr;
    xTaskCreate(TaskMain, s->name(), t.stack_words, s, t.priority, &h);
    s->task_handle = h;
  }
  HelperLog("ServiceHelper", LogLevel::kInfo,
            "All Services initialised; starting the scheduler");
  watchdog().Start();
  vTaskStartScheduler();
  for (;;) {
  }
}

}  // namespace gte
