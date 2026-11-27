// Fatal Errors (issues #9, #10, #19): the device stops normal operation for good.
// The raising code suspends the scheduler and masks interrupts, then loops forever:
// the error is printed on the debug UART at once and every 2 s after (polled, no interrupts),
// the red LED blinks at 0.5 Hz, and the IWDG keeps being fed so the board never reboots itself.
#pragma once
#include "ErrorCode.h"

namespace gte {

[[noreturn]] void fatal(const char *source, const ErrorCode &code, const char *supplementary = nullptr);

}  // namespace gte
