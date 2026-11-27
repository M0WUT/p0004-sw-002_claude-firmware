// Copyright 2026 M0WUT
// GTE Core: owns startup. src/main.cpp's setup() calls gte::Start(), which
// never returns.
//
// An Instrument provides its Application through GteApplication() and,
// optionally, extra Services through GteExtraServices(). Both are weak: with
// neither, Core runs on its own.
#pragma once
#include <cstddef>

#include "gte/application_service.h"
#include "gte/log_level.h"

namespace gte {

class TimeHelper;
class LoggingHelper;
class ServiceHelper;
class Watchdog;
class ConfigStorage;
class SoftKeys;

TimeHelper &time();
LoggingHelper &log();
ServiceHelper &services();
Watchdog &watchdog();
ConfigStorage &storage();
SoftKeys &keys();

// Stop the boot until Soft Key 4 is held for 3 s, repeating `message` on the
// log every 2 s (issue #12). The LCD shows it too once the Display Service
// exists (issue #50).
void PromptReinitialise(const char *source, const char *message);

// Log from a Helper (not a Service). Helpers follow the CORE Service's Log
// level.
void HelperLog(const char *helper, LogLevel level, const char *message);

[[noreturn]] void Start();

// Services an Instrument adds besides its Application (see GteExtraServices).
struct ServiceList {
  BaseService *const *items = nullptr;
  size_t count = 0;
};

}  // namespace gte

gte::ApplicationService *GteApplication();
gte::ServiceList GteExtraServices();
// Test hook (hardware_tests only): runs once config storage is loaded, before
// the table is checked or any Service loads its config.
void GteTestBeforeConfig();
