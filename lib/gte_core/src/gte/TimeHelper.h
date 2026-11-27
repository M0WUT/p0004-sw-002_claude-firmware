// Wall-clock time for log timestamps.
//
// There is no LSE crystal or backup battery, so the clock starts at 1970-01-01T00:00:00.000 at
// power-on and counts the system tick (HSE-derived, accurate), not the LSI-driven RTC. NTP or SCPI
// set the real time later (set_time); while Application Logging is on, corrections are held back.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace gte {

class TimeHelper {
public:
    uint64_t now_ms();                            // ms since 1970-01-01 (or since boot, until set)
    uint64_t uptime_ms();                         // ms since boot, never jumps
    void set_time(uint64_t epoch_ms);
    void format(char *buf, size_t size);          // "YYYY-MM-DDTHH:MM:SS.mmm"
    static void format_ms(uint64_t epoch_ms, char *buf, size_t size);

private:
    int64_t _offset_ms = 0;
    uint32_t _last_millis = 0;
    uint64_t _high = 0;
};

}  // namespace gte
