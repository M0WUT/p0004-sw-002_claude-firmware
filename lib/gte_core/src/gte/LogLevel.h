#pragma once
#include <stdint.h>

namespace gte {

// Increasing number = increasing severity (as in sw-001).
enum class LogLevel : uint8_t { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, CRITICAL = 4 };

constexpr const char *LOG_LEVEL_NAMES[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
constexpr uint8_t LOG_LEVEL_COUNT = 5;

inline const char *log_level_name(LogLevel l) { return LOG_LEVEL_NAMES[(uint8_t)l]; }

}  // namespace gte
