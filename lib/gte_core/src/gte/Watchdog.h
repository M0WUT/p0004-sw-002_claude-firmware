// The independent watchdog (issue #37): 4 s timeout, fed by a supervisor task only while every
// registered task has checked in within its deadline. Frozen while a debugger halts the core.
// The fatal loop feeds it directly, so the Fatal Error screen stays up.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace gte {

class Watchdog {
public:
    static constexpr uint32_t TIMEOUT_MS = 4000;
    static constexpr uint32_t DEFAULT_DEADLINE_MS = 2000;
    static constexpr size_t MAX_TASKS = 32;

    int register_task(const char *name, uint32_t deadline_ms = DEFAULT_DEADLINE_MS);
    void checkin(int id);
    void start();          // starts the IWDG and the supervisor task (just before the scheduler)
    static void feed();

private:
    static void supervisor(void *self);
    struct Entry {
        const char *name;
        uint32_t deadline_ms;
        volatile uint32_t last_ms;
    };
    Entry _tasks[MAX_TASKS];
    size_t _count = 0;
};

const char *reset_reason();  // why the last reset happened, read once from the RCC flags at boot

}  // namespace gte
