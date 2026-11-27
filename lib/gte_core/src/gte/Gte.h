// GTE Core: owns startup. src/main.cpp's setup() calls gte::start(), which never returns.
//
// An Instrument provides its Application through gte_application() and, optionally, extra
// Services through gte_extra_services(). Both are weak: with neither, Core runs on its own.
#pragma once
#include <stddef.h>
#include "ApplicationService.h"
#include "LogLevel.h"

namespace gte {

class TimeHelper;
class LoggingHelper;
class ServiceHelper;
class Watchdog;

TimeHelper &time();
LoggingHelper &log();
ServiceHelper &services();
Watchdog &watchdog();

// Log from a Helper (not a Service). Helpers follow the CORE Service's Log level.
void helper_log(const char *helper, LogLevel level, const char *message);

[[noreturn]] void start();

}  // namespace gte

gte::ApplicationService *gte_application();
void gte_extra_services(gte::BaseService **&list, size_t &count);
