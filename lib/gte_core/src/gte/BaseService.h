// A Service (issue #9, execution model issue #10).
//
// Lifecycle: ServiceHelper calls initialise() on every Service in registration order before the
// scheduler starts (no calls between Services there), then runs each Service's tick() in its own
// FreeRTOS task every tick_period_ms. tick() is the only place Services call each other.
//
// Every Service gets a "Log level" Config Parameter automatically. Generic code owns the config
// update path (parse, range/step, validate_config_field, write, save, Live/Reset, on_config_changed).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Config.h"
#include "ErrorCode.h"
#include "LogLevel.h"
#include "ServiceId.h"
#include "Status.h"

namespace gte {

struct TaskSettings {
    uint32_t tick_period_ms = 1000;
    uint32_t stack_words = 1024;
    uint8_t priority = 2;  // USB Services highest, then Display/UI, then everything else
};

class BaseService {
public:
    explicit BaseService(ServiceId id, TaskSettings task = {}) : _id(id), _task(task) {}
    virtual ~BaseService() = default;

    ServiceId id() const { return _id; }
    const char *name() const { return service_name(_id); }
    const TaskSettings &task_settings() const { return _task; }
    LogLevel log_level() const { return (LogLevel)_logLevel; }

    // Generic paths, used by the web page, SCPI, the UI and Home Assistant. They take the lock.
    void config(ConfigList &list);
    ConfigResult set_config(const char *id, const char *text);
    void status(StatusList &list);

    // For ServiceHelper.
    void run_initialise();
    void run_tick() { tick(); }
    void *task_handle = nullptr;
    int watchdog_id = -1;

protected:
    // What a Service implements.
    virtual void initialise() = 0;
    virtual void tick() {}
    virtual void declare_config(ConfigList &) {}
    virtual bool validate_config_field(const char *id, const ConfigValue &v, char *reason, size_t reasonSize)
    {
        (void)id; (void)v; (void)reason; (void)reasonSize;
        return true;
    }
    virtual void on_config_changed(const char *id) { (void)id; }
    virtual void get_status(StatusList &) {}
    virtual void save_config() {}  // config storage arrives with issue #43

    // Logging. Messages below this Service's Log level are dropped.
    void _debug(const char *msg) { _log(LogLevel::DEBUG, msg); }
    void _info(const char *msg) { _log(LogLevel::INFO, msg); }
    void _debugf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void _infof(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void _warning(const ErrorCode &code, const char *supplementary = nullptr);
    void _warningf(const ErrorCode &code, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
    void _clear_warning(const ErrorCode &code);
    [[noreturn]] void _error(const ErrorCode &code, const char *supplementary = nullptr);
    [[noreturn]] void _errorf(const ErrorCode &code, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

    void lock();
    void unlock();

private:
    void _log(LogLevel level, const char *msg);
    void print_config();
    ServiceId _id;
    TaskSettings _task;
    uint8_t _logLevel = (uint8_t)LogLevel::INFO;
    void *_mutex = nullptr;
};

}  // namespace gte
