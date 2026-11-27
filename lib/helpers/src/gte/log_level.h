// Copyright 2026 M0WUT
#pragma once
#include <cstdint>

namespace gte {

// Increasing number = increasing severity (as in sw-001).
enum class LogLevel : uint8_t {
  kDebug = 0,
  kInfo = 1,
  kWarning = 2,
  kError = 3,
  kCritical = 4
};

constexpr const char *kLogLevelNames[] = {"DEBUG", "INFO", "WARNING", "ERROR",
                                          "CRITICAL"};
constexpr uint8_t kLogLevelCount = 5;

inline const char *LogLevelName(LogLevel l) {
  return kLogLevelNames[static_cast<uint8_t>(l)];
}

}  // namespace gte
