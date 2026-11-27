// Copyright 2026 M0WUT
// IO Expander: the registers at the same address on every IO Board model.
//
// Every IO Board model has an IO Expander with this common block. Everything
// else in 0x10-0xEF is that model's own map, in
// src/io_boards/<model>/io_expander_regs.h, which the model's IO Expander
// firmware repo copies. Agreed in "IO Expander register map" (issue #28).
//
// Bus rules:
// - 7-bit address IO_EXPANDER_ADDR. 8-bit register pointer, which
// auto-increments.
// - 16-bit values are little-endian.
// - A read snapshots the whole run it covers, so a multi-byte value is never
// torn.
// - Any write to any register restarts the watchdog. If nothing is written for
//   IO_EXPANDER_WATCHDOG_MS, the IO Expander:
//     * sets every fan to 100 %;
//     * clears every LED override;
//     * sets INT_STATUS bit 7.
// - Fans also run at 100 % from power-on until the H743 first writes FAN_DUTY.
// - The open-drain interrupt (LCD Board PD11) is asserted while (INT_STATUS &
// INT_ENABLE) != 0.
// - Core takes the IO Board model from its Board Identity and requires
// PROTOCOL_VERSION to equal
//   the version in Core's definition of that model. A mismatch is a Fatal
//   Error.
#pragma once
#include <stdint.h>

namespace io_expander {

constexpr uint8_t IO_EXPANDER_ADDR = 0x30;
constexpr uint8_t ID_VALUE = 0x47;  // 'G'
constexpr uint32_t IO_EXPANDER_WATCHDOG_MS = 10000;

enum CommonReg : uint8_t {
  REG_ID = 0x00,                // RO  ID_VALUE
  REG_PROTOCOL_VERSION = 0x01,  // RO  this model's map version
  REG_FW_VERSION_MAJOR = 0x02,  // RO
  REG_FW_VERSION_MINOR = 0x03,  // RO
  REG_INT_STATUS = 0x04,        // W1C bits 0-5 model-defined, see INT_* below
  REG_INT_ENABLE = 0x05,        // RW  same bits; asserts the interrupt line
  REG_WATCHDOG_KICK = 0x06,     // W   any value
  REG_STATUS_LED = 0x07,        // RW  LED format; reads back what is showing
  REG_STATUS_LED_OVERRIDE =
      0x08,  // RW  1 = H743 drives the status LED (debug only, CORE Service)
  REG_ERROR_SEVERITY = 0x09,  // RO  ErrorSeverity
  REG_ERROR_CODE =
      0x0A,  // RO  uint16 (0x0A-0x0B), the IO Expander's own code, 0 = none
  REG_RESET = 0xF0,  // W   RESET_KEY restarts the IO Expander only (the power
                     // mux is hardware)
};

constexpr uint8_t MODEL_REG_FIRST = 0x10;
constexpr uint8_t MODEL_REG_LAST = 0xEF;
constexpr uint8_t RESET_KEY = 0xA5;

// INT_STATUS / INT_ENABLE bits common to every model. Bits 0-5 are defined per
// model.
constexpr uint8_t INT_ERROR_CHANGED =
    1u << 6;  // ERROR_SEVERITY or ERROR_CODE changed
constexpr uint8_t INT_WATCHDOG_EXPIRED = 1u << 7;

enum class ErrorSeverity : uint8_t { NONE = 0, WARNING = 1, FATAL = 2 };
// The IO Expander holds a Fatal error until it is reset, and keeps every fan at
// 100 % meanwhile. Core reports IO Expander error n (1-49) as the CORE Error
// Code 100 x (CORE ServiceId + 1) + 50 + n, with the model's message for n and
// the IO Expander's severity. Code 50 (n = 0 or n > 49) is "IO Expander
// reported an unknown error". The top 50 CORE codes (50-99) are reserved for
// this.
constexpr uint8_t CORE_ERROR_OFFSET = 50;
constexpr uint8_t MAX_ERROR_CODE = 49;

// LED register format (status LED and each model's Source LEDs).
constexpr uint8_t LED_RED = 1u << 0;
constexpr uint8_t LED_GREEN = 1u << 1;
constexpr uint8_t LED_BLUE = 1u << 2;  // colour 0 = off
constexpr uint8_t LED_MODE_SHIFT = 4;
enum class LedMode : uint8_t {
  STEADY = 0,
  SLOW_BLINK = 1,   // 1 Hz
  FAST_BLINK = 2,   // 4 Hz
  ERROR_BLINK = 3,  // 0.5 Hz, as on the LCD Board
};
// All blinking is timed by the IO Expander, and LEDs in the same mode blink in
// phase.
constexpr uint8_t led(uint8_t colour, LedMode mode) {
  return static_cast<uint8_t>((colour & 0x07) |
                              (static_cast<uint8_t>(mode) << LED_MODE_SHIFT));
}
// Status LED when the IO Expander drives it: off when healthy, amber (red +
// green) steady on a Warning, red ERROR_BLINK on a Fatal error.

}  // namespace io_expander
