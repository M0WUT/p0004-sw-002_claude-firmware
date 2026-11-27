// Error Codes for the Helpers (1-99) and the CORE Service (101-199).
#pragma once
#include "ErrorCode.h"

namespace gte::errors {

// CORE Service
GTE_ERROR_CODE(TOO_MANY_WARNINGS, 101, FATAL, "More than 8 Warnings active");
GTE_ERROR_CODE(STACK_OVERFLOW, 102, FATAL, "Task stack overflow");
GTE_ERROR_CODE(OUT_OF_MEMORY, 103, FATAL, "Out of memory");
GTE_ERROR_CODE(INSTRUMENT_BOARD_REJECTED, 104, FATAL, "Instrument Board rejected by the Application");
static_assert(code_belongs_to(TOO_MANY_WARNINGS.code, ServiceId::CORE), "CORE codes are 101-199");

}  // namespace gte::errors
