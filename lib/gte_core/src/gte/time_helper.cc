// Copyright 2026 M0WUT
#include "gte/time_helper.h"

#include <Arduino.h>

#include <cstdio>

namespace gte {

uint64_t TimeHelper::UptimeMs() {
  // Extend millis() past its 49-day wrap.
  uint32_t m = millis();
  if (m < last_millis_) high_ += 1ULL << 32;
  last_millis_ = m;
  return high_ + m;
}

uint64_t TimeHelper::NowMs() {
  return static_cast<uint64_t>(static_cast<int64_t>(UptimeMs()) + offset_ms_);
}

void TimeHelper::SetTime(uint64_t epoch_ms) {
  offset_ms_ =
      static_cast<int64_t>(epoch_ms) - static_cast<int64_t>(UptimeMs());
}

void TimeHelper::Format(char *buf, size_t size) {
  FormatMs(NowMs(), buf, size);
}

void TimeHelper::FormatMs(uint64_t epoch_ms, char *buf, size_t size) {
  uint64_t secs = epoch_ms / 1000;
  unsigned ms = static_cast<unsigned>(epoch_ms % 1000);
  unsigned sec = secs % 60, min = (secs / 60) % 60, hour = (secs / 3600) % 24;
  // Civil-from-days (Howard Hinnant), valid for any date after 1970.
  int64_t z = static_cast<int64_t>(secs / 86400) + 719468;
  int64_t era = z / 146097;
  unsigned doe = static_cast<unsigned>(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = static_cast<int64_t>(yoe) + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1;
  unsigned m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) y++;
  snprintf(buf, size, "%04d-%02u-%02uT%02u:%02u:%02u.%03u", static_cast<int>(y),
           m, d, hour, min, sec, ms);
}

}  // namespace gte
