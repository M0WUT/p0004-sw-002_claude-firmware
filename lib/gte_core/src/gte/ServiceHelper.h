// Registers Services, initialises them in order, then gives each its own FreeRTOS task.
#pragma once
#include <stddef.h>
#include "BaseService.h"
#include "ServiceId.h"

namespace gte {

class ServiceHelper {
public:
    static constexpr size_t MAX_SERVICES = 24;

    void register_service(BaseService *s);
    void initialise_all();
    [[noreturn]] void start();  // creates the tasks, starts the watchdog and the scheduler

    BaseService *get(ServiceId id);
    size_t count() const { return _count; }
    BaseService *at(size_t i) { return _services[i]; }
    BaseService *for_task(void *task);

    void set_restart_required() { _restartRequired = true; }
    bool restart_required() const { return _restartRequired; }

private:
    static void task_main(void *service);
    BaseService *_services[MAX_SERVICES] = {};
    size_t _count = 0;
    bool _restartRequired = false;
};

}  // namespace gte
