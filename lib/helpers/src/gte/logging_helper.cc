#include "gte/logging_helper.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <semphr.h>

#include <cstdio>
#include <cstring>

#include "gte/core_errors.h"
#include "gte/fatal.h"
#include "gte/time_helper.h"

namespace gte {

void LedSet(bool red, bool green, bool blue) {
  // Active low.
  digitalWrite(GTE_PIN_LED_RED, red ? LOW : HIGH);
  digitalWrite(GTE_PIN_LED_GREEN, green ? LOW : HIGH);
  digitalWrite(GTE_PIN_LED_BLUE, blue ? LOW : HIGH);
}

void LoggingHelper::Begin(TimeHelper *time) {
  time_ = time;
  mutex_ = xSemaphoreCreateRecursiveMutex();
  pinMode(GTE_PIN_LED_RED, OUTPUT);
  pinMode(GTE_PIN_LED_GREEN, OUTPUT);
  pinMode(GTE_PIN_LED_BLUE, OUTPUT);
  LedSet(false, false, false);
}

void LoggingHelper::Lock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(mutex_),
                            portMAX_DELAY);
}

void LoggingHelper::Unlock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(mutex_));
}

void LoggingHelper::FormatLine(char *buf, size_t size, const char *timestamp,
                               const char *source, LogLevel level,
                               uint16_t code, const char *message,
                               const char *supplementary) {
  char code_text[8] = "";
  if (code) snprintf(code_text, sizeof(code_text), "%u", code);
  char src[24];
  snprintf(src, sizeof(src), "[%s]", source);
  snprintf(buf, size, "%s\t%-18s\t%s\t%s\t%s%s%s", timestamp, src,
           LogLevelName(level), code_text, message,
           supplementary && *supplementary ? ": " : "",
           supplementary ? supplementary : "");
}

void LoggingHelper::Log(const char *source, LogLevel level, uint16_t code,
                        const char *message, const char *supplementary) {
  static const char *colours[] = {"\x1b[32m", "", "\x1b[33m", "\x1b[31m",
                                  "\x1b[31m"};
  char ts[32];
  time_->Format(ts, sizeof(ts));
  char line[256];
  FormatLine(line, sizeof(line), ts, source, level, code, message,
             supplementary);
  Lock();
  const char *c = colours[static_cast<uint8_t>(level)];
  Serial.print(c);
  Serial.print(line);
  Serial.print(*c ? "\x1b[0m\r\n" : "\r\n");
  Unlock();
}

void LoggingHelper::UpdateLed() {
  if (count_)
    LedSet(true, true, false);  // amber
  else
    LedSet(false, false, false);
}

void LoggingHelper::RaiseWarning(const char *source, const ErrorCode &code,
                                 const char *supplementary) {
  Lock();
  for (size_t i = 0; i < count_; i++) {
    if (warnings_[i].code == code.code) {
      snprintf(warnings_[i].supplementary, kSupplementaryText, "%s",
               supplementary ? supplementary : "");
      warnings_[i].time_ms = time_->NowMs();
      Log(source, LogLevel::kWarning, code.code, code.message, supplementary);
      Log(source, LogLevel::kInfo, code.code,
          "Warning updated (already active)");
      if (listener_) listener_(listener_context_, warnings_[i]);
      Unlock();
      return;
    }
  }
  Log(source, LogLevel::kWarning, code.code, code.message, supplementary);
  if (count_ >= kMaxWarnings) {
    char supp[48];
    snprintf(supp, sizeof(supp), "new Warning %u", code.code);
    Fatal("CORE", errors::kTooManyWarnings, supp);
  }
  Warning &w = warnings_[count_++];
  w.code = code.code;
  w.message = code.message;
  snprintf(w.source, sizeof(w.source), "%s", source);
  snprintf(w.supplementary, kSupplementaryText, "%s",
           supplementary ? supplementary : "");
  w.time_ms = time_->NowMs();
  UpdateLed();
  if (listener_) listener_(listener_context_, w);
  Unlock();
}

bool LoggingHelper::ClearWarning(uint16_t code) {
  Lock();
  for (size_t i = 0; i < count_; i++) {
    if (warnings_[i].code == code) {
      char source[20];
      snprintf(source, sizeof(source), "%s", warnings_[i].source);
      memmove(&warnings_[i], &warnings_[i + 1],
              (count_ - i - 1) * sizeof(Warning));
      count_--;
      UpdateLed();
      Log(source, LogLevel::kInfo, code, "Warning cleared");
      Unlock();
      return true;
    }
  }
  Unlock();
  return false;
}

void LoggingHelper::ClearAllWarnings() {
  Lock();
  while (count_) ClearWarning(warnings_[0].code);
  Unlock();
}

size_t LoggingHelper::Warnings(Warning *out, size_t max) {
  Lock();
  size_t n = count_ < max ? count_ : max;
  memcpy(out, warnings_, n * sizeof(Warning));
  Unlock();
  return n;
}

size_t LoggingHelper::WarningCount() { return count_; }

}  // namespace gte
