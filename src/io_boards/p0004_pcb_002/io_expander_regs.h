// IO Expander register map for IO Board model P0004-PCB-002, protocol version 1.
//
// The common block (0x00-0x0F, 0xF0) is in io_boards/io_expander_common.h. The P0004-PCB-002
// IO Expander firmware repo keeps a copy of this file; bump PROTOCOL_VERSION on any incompatible
// change. Agreed in "IO Expander register map" (issue #28).
#pragma once
#include "../io_expander_common.h"

namespace io_expander::p0004_pcb_002 {

constexpr uint8_t PROTOCOL_VERSION = 1;

// Source numbers, used for the bitmask bits, SOURCE_SELECTED and SOURCE_LED[n].
// REQ-002 POW-06: exactly one source is Selected at a time. POW-07 priority:
// barrel / Powerpole, then USB-PD, then PoE (chosen by hardware, not by the IO Expander).
enum Source : uint8_t { SOURCE_BARREL = 0, SOURCE_POWERPOLE = 1, SOURCE_USBPD = 2, SOURCE_POE = 3 };
constexpr uint8_t SOURCE_COUNT = 4;
constexpr uint8_t SOURCE_NONE = 0xFF;

constexpr uint8_t MAX_FANS = 4;  // fixed maximum; the fitted count is in this model's definition

enum Reg : uint8_t {
    REG_SOURCE_VALID        = 0x10,  // RO  bitmask by Source: present and able to power GTE
    REG_SOURCE_SELECTED     = 0x11,  // RO  Source being drawn from, or SOURCE_NONE
    REG_USBPD_STATE         = 0x12,  // RO  UsbPdState
    REG_USBPD_VOLTAGE_MV    = 0x13,  // RO  uint16 (0x13-0x14), negotiated contract, not measured
    REG_USBPD_CURRENT_MA    = 0x15,  // RO  uint16 (0x15-0x16), negotiated contract, not measured
    REG_POE_STATE           = 0x17,  // RO  PoeState
    REG_POE_CLASS           = 0x18,  // RO  0-8
    REG_SOURCE_LED          = 0x20,  // RW  SOURCE_LED[0..3] at 0x20-0x23, LED format; reads back what is showing
    REG_SOURCE_LED_OVERRIDE = 0x24,  // RW  bitmask by Source; 1 = Power Service drives that LED
    REG_FAN_DUTY            = 0x30,  // RW  FAN_DUTY[0..3] at 0x30-0x33, 0-100 %
    REG_FAN_RPM             = 0x34,  // RO  FAN_RPM[0..3] uint16 at 0x34-0x3B; 2 pulses/rev; 0 = no pulses for ~1 s
};

enum class UsbPdState : uint8_t { NO_VBUS = 0, NEGOTIATING = 1, FAILED = 2, CONTRACT = 3 };
enum class PoeState : uint8_t { NOT_PRESENT = 0, DETECTING = 1, FAILED = 2, POWERED = 3 };

// INT_STATUS / INT_ENABLE model bits.
constexpr uint8_t INT_SOURCE_CHANGED = 1u << 0;  // SOURCE_VALID or SOURCE_SELECTED changed
constexpr uint8_t INT_USBPD_CHANGED  = 1u << 1;
constexpr uint8_t INT_POE_CHANGED    = 1u << 2;

// What the IO Expander shows on a Source LED by itself (REQ-002 UX-02 / UX-03), while that
// LED's override bit is clear. Barrel and Powerpole only use off / green / green fast blink.
//   not present            off
//   negotiating            amber (red + green) SLOW_BLINK
//   negotiation failed     red steady
//   Valid                  green steady
//   Selected               green FAST_BLINK
// Overrides clear when the IO Expander resets and when the watchdog expires.

// Ownership: Source LEDs and the power registers belong to the Power Service, fans to the
// Thermal Service, the status LED override to the CORE Service. Application Services never
// access the IO Expander; Instrument IO belongs on the Instrument Board.

}  // namespace io_expander::p0004_pcb_002
