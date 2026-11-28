// GTE Core: owns startup. src/main.cpp's setup() calls gte::Start(), which
// never returns.
//
// An Instrument provides its Application through GteApplication() and,
// optionally, extra Services through GteExtraServices(). Both are weak: with
// neither, Core runs on its own.
#pragma once
#include <cstddef>
#include <cstdint>

#include "gte/application_service.h"
#include "gte/log_level.h"

namespace gte {

class TimeHelper;
class LoggingHelper;
class ServiceHelper;
class Watchdog;
class ConfigStorage;
class SoftKeys;
class UsbDeviceService;
class ScpiService;

TimeHelper &time();
LoggingHelper &log();
ServiceHelper &services();
Watchdog &watchdog();
ConfigStorage &storage();
SoftKeys &keys();
UsbDeviceService &usb_device();
ScpiService &scpi();

// The Application Service, or nullptr when Core runs on its own.
ApplicationService *application();
// The Instrument's name and version: the Application's, or "GTE Core" and the
// Core version with no Application.
const char *InstrumentName();
const char *InstrumentVersion();
// The STM32 96-bit unique ID as 24 upper-case hex digits, bytes in memory
// order: the USB serial number and *IDN?'s third field.
void UniqueIdString(char *buf, size_t size);

// Set or clear bits of the SCPI OPERation condition register (issue #15): bit
// 8 is Application Logging; bits 9-12 belong to the Application Service.
void SetOperationCondition(uint16_t mask, bool on);

// Restart now (reset reason: software), logging why.
[[noreturn]] void Restart(const char *source, const char *why);
// Factory Reset: rebuild config storage from scratch, then restart. The MAC is
// not touched.
[[noreturn]] void FactoryReset(const char *source);

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
