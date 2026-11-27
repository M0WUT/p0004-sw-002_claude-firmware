// Copyright 2026 M0WUT
// Wall-clock time for log timestamps.
//
// There is no LSE crystal or backup battery, so the clock starts at
// 1970-01-01T00:00:00.000 at power-on and counts the system Tick (HSE-derived,
// accurate), not the LSI-driven RTC. NTP or SCPI set the real time later
// (set_time); while Application Logging is on, corrections are held back.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gte {

class TimeHelper {
 public:
  uint64_t NowMs();     // ms since 1970-01-01 (or since boot, until set)
  uint64_t UptimeMs();  // ms since boot, never jumps
  void SetTime(uint64_t epoch_ms);
  void Format(char *buf, size_t size);  // "YYYY-MM-DDTHH:MM:SS.mmm"
  static void FormatMs(uint64_t epoch_ms, char *buf, size_t size);

 private:
  int64_t offset_ms_ = 0;
  uint32_t last_millis_ = 0;
  uint64_t high_ = 0;
};

}  // namespace gte
