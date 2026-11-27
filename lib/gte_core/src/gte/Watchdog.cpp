#include "Watchdog.h"
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <stdio.h>
#include "Gte.h"

namespace gte {

int Watchdog::register_task(const char *name, uint32_t deadline_ms)
{
    if (_count >= MAX_TASKS)
        return -1;
    _tasks[_count] = Entry{name, deadline_ms, millis()};
    return (int)_count++;
}

void Watchdog::checkin(int id)
{
    if (id >= 0 && (size_t)id < _count)
        _tasks[id].last_ms = millis();
}

void Watchdog::feed() { IWDG1->KR = 0xAAAA; }

void Watchdog::start()
{
    // Freeze the IWDG while a debugger halts the core.
    DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
    // LSI ~32 kHz / 64 = 500 Hz; 2000 counts = 4 s.
    IWDG1->KR = 0xCCCC;
    IWDG1->KR = 0x5555;
    IWDG1->PR = 4;      // /64
    IWDG1->RLR = TIMEOUT_MS / 2;
    while (IWDG1->SR) {}
    feed();
    uint32_t now = millis();
    for (size_t i = 0; i < _count; i++)
        _tasks[i].last_ms = now;
    xTaskCreate(supervisor, "watchdog", 512, this, configMAX_PRIORITIES - 1, nullptr);
}

void Watchdog::supervisor(void *arg)
{
    Watchdog *self = (Watchdog *)arg;
    bool reported = false;
    uint32_t lastDebug = 0;
    for (;;) {
        uint32_t now = millis();
        const char *late = nullptr;
        for (size_t i = 0; i < self->_count; i++)
            if (now - self->_tasks[i].last_ms > self->_tasks[i].deadline_ms)
                late = self->_tasks[i].name;
        if (!late) {
            feed();
            if (now - lastDebug >= 1000) {
                lastDebug = now;
                helper_log("Watchdog", LogLevel::DEBUG, "All tasks checked in; fed");
            }
        } else if (!reported) {
            reported = true;
            char msg[64];
            snprintf(msg, sizeof(msg), "%s missed its check-in; resetting", late);
            helper_log("Watchdog", LogLevel::ERROR, msg);
            // The EEPROM fatal/watchdog record (issue #43) stores the task name here.
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

const char *reset_reason()
{
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
