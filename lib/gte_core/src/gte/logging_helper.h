// Copyright 2026 M0WUT
// The debug UART log, the active Warnings, and the RGB LED's Warning
// indication.
//
// Line format (sw-001's, plus severity and Error Code):
//   <timestamp>\t[<source>]<padding>\t<LEVEL>\t<code or blank>\t<message>[:
//   <supplementary>]
// Warnings are conditions (issue #19): raising an active code updates it in
// place, clearing removes it, and more than 8 active is a Fatal Error. While
// any is active the LED is amber.
#pragma once
#include <cstddef>
#include <cstdint>

#include "gte/error_code.h"
#include "gte/log_level.h"

namespace gte {

class TimeHelper;

constexpr size_t kMaxWarnings = 8;
constexpr size_t kSupplementaryText = 64;

struct Warning {
  uint16_t code;
  const char *message;
  char source[20];
  char supplementary[kSupplementaryText];
  uint64_t time_ms;
};

class LoggingHelper {
 public:
  void Begin(TimeHelper *time);
  void Log(const char *source, LogLevel level, uint16_t code,
           const char *message, const char *supplementary = nullptr);

  void RaiseWarning(const char *source, const ErrorCode &code,
                    const char *supplementary);
  bool ClearWarning(uint16_t code);
  void ClearAllWarnings();
  size_t Warnings(Warning *out, size_t max);  // copy of the active list
  size_t WarningCount();

  static void FormatLine(char *buf, size_t size, const char *timestamp,
                         const char *source, LogLevel level, uint16_t code,
                         const char *message, const char *supplementary);

 private:
  void Lock();
  void Unlock();
  void UpdateLed();
  TimeHelper *time_ = nullptr;
  void *mutex_ = nullptr;
  Warning warnings_[kMaxWarnings];
  size_t count_ = 0;
};

// RGB LED on the LCD Board (active low): PA10 red, PA8 green, PA9 blue.
void LedSet(bool red, bool green, bool blue);

}  // namespace gte
