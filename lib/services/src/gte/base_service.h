// A Service (issue #9, execution model issue #10).
//
// Lifecycle: ServiceHelper calls Initialise() on every Service in registration
// order before the scheduler starts (no calls between Services there), then
// runs each Service's Tick() in its own FreeRTOS task every tick_period_ms.
// Tick() is the only place Services call each other.
//
// Every Service gets a "Log level" Config Parameter automatically. Generic code
// owns the config update path (parse, range/step, validate_config_field, write,
// save, Live/Reset, on_config_changed).
#pragma once
#include <cstddef>
#include <cstdint>

#include "gte/config.h"
#include "gte/error_code.h"
#include "gte/log_level.h"
#include "gte/service_id.h"
#include "gte/status.h"

namespace gte {

class ScpiRegistry;

// A Service's stored config: its struct (its values on construction are the
// defaults) and the struct's version. Bump the version, and add a
// migrate_config step, on every change once released.
struct ConfigBlock {
  void *data = nullptr;
  uint16_t size = 0;
  uint8_t version = 1;
};

struct TaskSettings {
  uint32_t tick_period_ms = 1000;
  uint32_t stack_words = 1024;
  uint8_t priority =
      2;  // USB Services highest, then Display/UI, then everything else
};

class BaseService {
 public:
  explicit BaseService(ServiceId id, TaskSettings task = {})
      : id_(id), task_(task) {}
  virtual ~BaseService() = default;

  ServiceId id() const { return id_; }
  const char *name() const { return ServiceName(id_); }
  const TaskSettings &task_settings() const { return task_; }
  LogLevel log_level() const { return static_cast<LogLevel>(log_level_); }

  // Generic paths, used by the web page, SCPI, the UI and Home Assistant. They
  // take the lock.
  void Config(ConfigList *list);
  ConfigResult SetConfig(const char *id, const char *text);
  void Status(StatusList *list);

  // Restore every Config Parameter (and the Log level) to its default, and
  // save.
  void ResetToDefaults();
  // A Config Parameter's built-in default, as text (Format()); false if there
  // is no such parameter.
  bool DefaultText(const char *id, char *buf, size_t size);
  // Its current value as text (Format(): secrets unmasked), read under the
  // lock; false if there is no such parameter.
  bool CurrentText(const char *id, char *buf, size_t size);

  // Register this Service's SCPI commands (once, at startup, after every
  // Service is initialised). Document each in an @scpi doc comment.
  virtual void RegisterScpiCommands(ScpiRegistry *registry) { (void)registry; }

  // For ServiceHelper.
  void CaptureDefaults();
  void RunInitialise();
  void RunTick() { Tick(); }
  void *task_handle = nullptr;
  int watchdog_id = -1;

 protected:
  // What a Service implements.
  virtual void Initialise() = 0;
  virtual void Tick() {}
  virtual void DeclareConfig(ConfigList *) {}
  virtual bool ValidateConfigField(const char *id, const ConfigValue &v,
                                   char *reason, size_t reason_size) {
    (void)id;
    (void)v;
    (void)reason;
    (void)reason_size;
    return true;
  }
  virtual void OnConfigChanged(const char *id) { (void)id; }
  virtual void GetStatus(StatusList *) {}
  virtual ConfigBlock GetConfigBlock() { return {}; }
  // One step, from_version -> from_version + 1: fill `out` and return its size,
  // or -1 if this step doesn't exist (a Fatal Error). Core chains the steps.
  virtual int MigrateConfig(uint8_t from_version, const uint8_t *in,
                            size_t in_size, uint8_t *out, size_t out_capacity) {
    (void)from_version;
    (void)in;
    (void)in_size;
    (void)out;
    (void)out_capacity;
    return -1;
  }
  void SaveConfig();

  // Logging. Messages below this Service's Log level are dropped.
  void LogDebug(const char *msg) { LogAt(LogLevel::kDebug, msg); }
  void LogInfo(const char *msg) { LogAt(LogLevel::kInfo, msg); }
  void LogDebugf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void LogInfof(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void Warn(const ErrorCode &code, const char *supplementary = nullptr);
  void Warnf(const ErrorCode &code, const char *fmt, ...)
      __attribute__((format(printf, 3, 4)));
  void ClearWarning(const ErrorCode &code);
  [[noreturn]] void FatalError(const ErrorCode &code,
                               const char *supplementary = nullptr);
  [[noreturn]] void FatalErrorf(const ErrorCode &code, const char *fmt, ...)
      __attribute__((format(printf, 3, 4)));

  void Lock();
  void Unlock();

 private:
  void LogAt(LogLevel level, const char *msg);
  void PrintConfig();
  void LoadConfig();
  uint8_t *defaults_ = nullptr;
  ServiceId id_;
  TaskSettings task_;
  uint8_t log_level_ = static_cast<uint8_t>(LogLevel::kInfo);
  void *mutex_ = nullptr;
};

}  // namespace gte
