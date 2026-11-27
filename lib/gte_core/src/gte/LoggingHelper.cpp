#include "LoggingHelper.h"
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <semphr.h>
#include <stdio.h>
#include <string.h>
#include "CoreErrors.h"
#include "Fatal.h"
#include "TimeHelper.h"

namespace gte {

void led_set(bool red, bool green, bool blue)
{
    // Active low.
    digitalWrite(GTE_PIN_LED_RED, red ? LOW : HIGH);
    digitalWrite(GTE_PIN_LED_GREEN, green ? LOW : HIGH);
    digitalWrite(GTE_PIN_LED_BLUE, blue ? LOW : HIGH);
}

void LoggingHelper::begin(TimeHelper *time)
{
    _time = time;
    _mutex = xSemaphoreCreateRecursiveMutex();
    pinMode(GTE_PIN_LED_RED, OUTPUT);
    pinMode(GTE_PIN_LED_GREEN, OUTPUT);
    pinMode(GTE_PIN_LED_BLUE, OUTPUT);
    led_set(false, false, false);
}

void LoggingHelper::lock()
{
    if (_mutex && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        xSemaphoreTakeRecursive((SemaphoreHandle_t)_mutex, portMAX_DELAY);
}

void LoggingHelper::unlock()
{
    if (_mutex && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        xSemaphoreGiveRecursive((SemaphoreHandle_t)_mutex);
}

void LoggingHelper::format_line(char *buf, size_t size, const char *timestamp, const char *source, LogLevel level,
                                uint16_t code, const char *message, const char *supplementary)
{
    char codeText[8] = "";
    if (code)
        snprintf(codeText, sizeof(codeText), "%u", code);
    char src[24];
    snprintf(src, sizeof(src), "[%s]", source);
    snprintf(buf, size, "%s\t%-18s\t%s\t%s\t%s%s%s", timestamp, src, log_level_name(level), codeText, message,
             supplementary && *supplementary ? ": " : "", supplementary ? supplementary : "");
}

void LoggingHelper::log(const char *source, LogLevel level, uint16_t code, const char *message, const char *supplementary)
{
    static const char *colours[] = {"\x1b[32m", "", "\x1b[33m", "\x1b[31m", "\x1b[31m"};
    char ts[32];
    _time->format(ts, sizeof(ts));
    char line[256];
    format_line(line, sizeof(line), ts, source, level, code, message, supplementary);
    lock();
    const char *c = colours[(uint8_t)level];
    Serial.print(c);
    Serial.print(line);
    Serial.print(*c ? "\x1b[0m\r\n" : "\r\n");
    unlock();
}

void LoggingHelper::update_led()
{
    if (_count)
        led_set(true, true, false);  // amber
    else
        led_set(false, false, false);
}

void LoggingHelper::raise_warning(const char *source, const ErrorCode &code, const char *supplementary)
{
    lock();
    for (size_t i = 0; i < _count; i++) {
        if (_warnings[i].code == code.code) {
            snprintf(_warnings[i].supplementary, SUPPLEMENTARY_TEXT, "%s", supplementary ? supplementary : "");
            _warnings[i].time_ms = _time->now_ms();
            log(source, LogLevel::WARNING, code.code, code.message, supplementary);
            log(source, LogLevel::INFO, code.code, "Warning updated (already active)");
            unlock();
            return;
        }
    }
    log(source, LogLevel::WARNING, code.code, code.message, supplementary);
    if (_count >= MAX_WARNINGS) {
        char supp[48];
        snprintf(supp, sizeof(supp), "new Warning %u", code.code);
        fatal("CORE", errors::TOO_MANY_WARNINGS, supp);
    }
    Warning &w = _warnings[_count++];
    w.code = code.code;
    w.message = code.message;
    snprintf(w.source, sizeof(w.source), "%s", source);
    snprintf(w.supplementary, SUPPLEMENTARY_TEXT, "%s", supplementary ? supplementary : "");
    w.time_ms = _time->now_ms();
    update_led();
    unlock();
}

bool LoggingHelper::clear_warning(uint16_t code)
{
    lock();
    for (size_t i = 0; i < _count; i++) {
        if (_warnings[i].code == code) {
            char source[20];
            snprintf(source, sizeof(source), "%s", _warnings[i].source);
            memmove(&_warnings[i], &_warnings[i + 1], (_count - i - 1) * sizeof(Warning));
            _count--;
            update_led();
            log(source, LogLevel::INFO, code, "Warning cleared");
            unlock();
            return true;
        }
    }
    unlock();
    return false;
}

void LoggingHelper::clear_all_warnings()
{
    lock();
    while (_count)
        clear_warning(_warnings[0].code);
    unlock();
}

size_t LoggingHelper::warnings(Warning *out, size_t max)
{
    lock();
    size_t n = _count < max ? _count : max;
    memcpy(out, _warnings, n * sizeof(Warning));
    unlock();
    return n;
}

size_t LoggingHelper::warning_count() { return _count; }

}  // namespace gte
