// Copyright 2026 M0WUT
// Error Codes for the Helpers (1-99) and the CORE Service (101-199).
#pragma once
#include "gte/error_code.h"

namespace gte::errors {

// Helpers (1-99)
GTE_ERROR_CODE(kI2cReadFailed, 1, kFatal, "I2C read failed");
GTE_ERROR_CODE(kI2cWriteFailed, 2, kFatal, "I2C write failed");
GTE_ERROR_CODE(kEepromNotDetected, 10, kFatal, "Config EEPROM not detected");
GTE_ERROR_CODE(kEepromFull, 20, kFatal, "Config EEPROM full");
GTE_ERROR_CODE(kConfigMigrationMissing, 21, kFatal,
               "No config migration (hold Soft Keys 1+4 for 5 s at power-on to "
               "Factory Reset)");

// CORE Service
GTE_ERROR_CODE(kTooManyWarnings, 101, kFatal, "More than 8 Warnings active");
GTE_ERROR_CODE(kStackOverflow, 102, kFatal, "Task stack overflow");
GTE_ERROR_CODE(kOutOfMemory, 103, kFatal, "Out of memory");
GTE_ERROR_CODE(kInstrumentBoardRejected, 104, kFatal,
               "Instrument Board rejected by the Application");
GTE_ERROR_CODE(kRecoveredWatchdog, 105, kWarning,
               "Recovered from a watchdog reset");
static_assert(CodeBelongsTo(kTooManyWarnings.code, ServiceId::kCore),
              "CORE codes are 101-199");

}  // namespace gte::errors
