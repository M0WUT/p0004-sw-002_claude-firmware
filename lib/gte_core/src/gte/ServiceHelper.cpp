#include "ServiceHelper.h"
#include <STM32FreeRTOS.h>
#include <stdio.h>
#include "Gte.h"
#include "Watchdog.h"

namespace gte {

void ServiceHelper::register_service(BaseService *s)
{
    if (_count < MAX_SERVICES)
        _services[_count++] = s;
}

void ServiceHelper::initialise_all()
{
    for (size_t i = 0; i < _count; i++)
        _services[i]->run_initialise();
}

BaseService *ServiceHelper::get(ServiceId id)
{
    for (size_t i = 0; i < _count; i++)
        if (_services[i]->id() == id)
            return _services[i];
    return nullptr;
}

BaseService *ServiceHelper::for_task(void *task)
{
    for (size_t i = 0; i < _count; i++)
        if (_services[i]->task_handle == task)
            return _services[i];
    return nullptr;
}

void ServiceHelper::task_main(void *arg)
{
    BaseService *s = (BaseService *)arg;
    const TickType_t period = pdMS_TO_TICKS(s->task_settings().tick_period_ms);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        s->run_tick();
        watchdog().checkin(s->watchdog_id);
        vTaskDelayUntil(&last, period);
    }
}

void ServiceHelper::start()
{
    for (size_t i = 0; i < _count; i++) {
        BaseService *s = _services[i];
        const auto &t = s->task_settings();
        uint32_t deadline = t.tick_period_ms * 2 > Watchdog::DEFAULT_DEADLINE_MS ? t.tick_period_ms * 2
                                                                                 : Watchdog::DEFAULT_DEADLINE_MS;
        s->watchdog_id = watchdog().register_task(s->name(), deadline);
        TaskHandle_t h = nullptr;
        xTaskCreate(task_main, s->name(), t.stack_words, s, t.priority, &h);
        s->task_handle = h;
    }
    helper_log("ServiceHelper", LogLevel::INFO, "All Services initialised; starting the scheduler");
    watchdog().start();
    vTaskStartScheduler();
    for (;;) {}
}

}  // namespace gte
