#include "gte/core_service.h"

#include <STM32FreeRTOS.h>

#include "gte/gte.h"
#include "gte/logging_helper.h"

namespace gte {

void CoreService::Tick() {
  LogDebugf("Heartbeat: %u B heap free, %u Warnings active",
            static_cast<unsigned>(xPortGetFreeHeapSize()),
            static_cast<unsigned>(log().WarningCount()));
}

}  // namespace gte
