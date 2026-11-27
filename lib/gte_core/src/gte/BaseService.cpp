#include "BaseService.h"
#include <STM32FreeRTOS.h>
#include <semphr.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "Fatal.h"
#include "Gte.h"
#include "LoggingHelper.h"
#include "ServiceHelper.h"

namespace gte {

void BaseService::lock()
{
    if (!_mutex)
        _mutex = xSemaphoreCreateRecursiveMutex();
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        xSemaphoreTakeRecursive((SemaphoreHandle_t)_mutex, portMAX_DELAY);
}

void BaseService::unlock()
{
    if (_mutex && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        xSemaphoreGiveRecursive((SemaphoreHandle_t)_mutex);
}

void BaseService::config(ConfigList &list)
{
    lock();
    list.enumeration("logLevel", "Log level", &_logLevel, LOG_LEVEL_NAMES, LOG_LEVEL_COUNT);
    declare_config(list);
    unlock();
}

ConfigResult BaseService::set_config(const char *id, const char *text)
{
    ConfigResult r;
    lock();
    ConfigList list;
    config(list);
    const ConfigParam *p = list.find(id);
    ConfigValue v;
    if (!p) {
        snprintf(r.message, sizeof(r.message), "Unknown parameter %s", id);
    } else if (!parse_config_value(*p, text, v, r.message, sizeof(r.message))) {
        // message set by the parser
    } else if (!validate_config_field(id, v, r.message, sizeof(r.message))) {
        if (!r.message[0])
            snprintf(r.message, sizeof(r.message), "Refused by %s", name());
    } else {
        write_config_value(*p, v);
        save_config();
        r.ok = true;
        if (p->apply == ApplyMode::Reset) {
            r.restartRequired = true;
            services().set_restart_required();
            snprintf(r.message, sizeof(r.message), "Applied after restart");
        } else {
            snprintf(r.message, sizeof(r.message), "Applied");
            on_config_changed(id);
        }
    }
    unlock();
    char shown[CONFIG_VALUE_TEXT + 8] = "?";
    if (p)
        p->display(shown, sizeof(shown));
    char msg[200];
    snprintf(msg, sizeof(msg), "Config %s = %s: %s%s", id, (p && p->secret) ? "****" : text,
             r.ok ? "accepted" : "rejected: ", r.ok ? (r.restartRequired ? " (restart required)" : "") : r.message);
    _info(msg);
    return r;
}

void BaseService::status(StatusList &list)
{
    lock();
    get_status(list);
    unlock();
}

void BaseService::run_initialise()
{
    _info("Initialising");
    initialise();
    print_config();
}

void BaseService::print_config()
{
    ConfigList list;
    config(list);
    _info("Config:");
    for (size_t i = 0; i < list.count(); i++) {
        char value[CONFIG_VALUE_TEXT + 16];
        list[i].display(value, sizeof(value));
        char line[128];
        snprintf(line, sizeof(line), "  %s (%s): %s%s", list[i].label, list[i].id, value,
                 list[i].apply == ApplyMode::Reset ? " [reset]" : "");
        _info(line);
    }
}

void BaseService::_log(LogLevel level, const char *msg)
{
    if (level >= log_level())
        log().log(name(), level, 0, msg);
}

void BaseService::_debugf(const char *fmt, ...)
{
    char buf[160];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    _debug(buf);
}

void BaseService::_infof(const char *fmt, ...)
{
    char buf[160];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    _info(buf);
}

void BaseService::_warning(const ErrorCode &code, const char *supplementary)
{
    // Warnings are always recorded; the Log level only filters debug/info chatter.
    log().raise_warning(name(), code, supplementary);
}

void BaseService::_warningf(const ErrorCode &code, const char *fmt, ...)
{
    char buf[SUPPLEMENTARY_TEXT];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    _warning(code, buf);
}

void BaseService::_clear_warning(const ErrorCode &code) { log().clear_warning(code.code); }

void BaseService::_error(const ErrorCode &code, const char *supplementary) { fatal(name(), code, supplementary); }

void BaseService::_errorf(const ErrorCode &code, const char *fmt, ...)
{
    char buf[SUPPLEMENTARY_TEXT];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    fatal(name(), code, buf);
}

}  // namespace gte
