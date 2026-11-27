#include "TimeHelper.h"
#include <Arduino.h>
#include <stdio.h>

namespace gte {

uint64_t TimeHelper::uptime_ms()
{
    // Extend millis() past its 49-day wrap.
    uint32_t m = millis();
    if (m < _last_millis)
        _high += 1ULL << 32;
    _last_millis = m;
    return _high + m;
}

uint64_t TimeHelper::now_ms() { return (uint64_t)((int64_t)uptime_ms() + _offset_ms); }

void TimeHelper::set_time(uint64_t epoch_ms) { _offset_ms = (int64_t)epoch_ms - (int64_t)uptime_ms(); }

void TimeHelper::format(char *buf, size_t size) { format_ms(now_ms(), buf, size); }

void TimeHelper::format_ms(uint64_t epoch_ms, char *buf, size_t size)
{
    uint64_t secs = epoch_ms / 1000;
    unsigned ms = (unsigned)(epoch_ms % 1000);
    unsigned sec = secs % 60, min = (secs / 60) % 60, hour = (secs / 3600) % 24;
    // Civil-from-days (Howard Hinnant), valid for any date after 1970.
    int64_t z = (int64_t)(secs / 86400) + 719468;
    int64_t era = z / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2)
        y++;
    snprintf(buf, size, "%04d-%02u-%02uT%02u:%02u:%02u.%03u", (int)y, m, d, hour, min, sec, ms);
}

}  // namespace gte
