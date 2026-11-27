// The debug UART log, the active Warnings, and the RGB LED's Warning indication.
//
// Line format (sw-001's, plus severity and Error Code):
//   <timestamp>\t[<source>]<padding>\t<LEVEL>\t<code or blank>\t<message>[: <supplementary>]
// Warnings are conditions (issue #19): raising an active code updates it in place, clearing
// removes it, and more than 8 active is a Fatal Error. While any is active the LED is amber.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ErrorCode.h"
#include "LogLevel.h"

namespace gte {

class TimeHelper;

constexpr size_t MAX_WARNINGS = 8;
constexpr size_t SUPPLEMENTARY_TEXT = 64;

struct Warning {
    uint16_t code;
    const char *message;
    char source[20];
    char supplementary[SUPPLEMENTARY_TEXT];
    uint64_t time_ms;
};

class LoggingHelper {
public:
    void begin(TimeHelper *time);
    void log(const char *source, LogLevel level, uint16_t code, const char *message, const char *supplementary = nullptr);

    void raise_warning(const char *source, const ErrorCode &code, const char *supplementary);
    bool clear_warning(uint16_t code);
    void clear_all_warnings();
    size_t warnings(Warning *out, size_t max);  // copy of the active list
    size_t warning_count();

    static void format_line(char *buf, size_t size, const char *timestamp, const char *source, LogLevel level,
                            uint16_t code, const char *message, const char *supplementary);

private:
    void lock();
    void unlock();
    void update_led();
    TimeHelper *_time = nullptr;
    void *_mutex = nullptr;
    Warning _warnings[MAX_WARNINGS];
    size_t _count = 0;
};

// RGB LED on the LCD Board (active low): PA10 red, PA8 green, PA9 blue.
void led_set(bool red, bool green, bool blue);

}  // namespace gte
