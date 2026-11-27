// Error Codes: one number per problem, used for both Warnings and Fatal Errors.
//
// Code = 100 x (ServiceId + 1) + n, n = 0..99 within the Service. 0 = "No error"; 1-99 belong to
// Helpers. Each Service defines its codes in its own header with GTE_ERROR_CODE, giving the
// absolute code (so the SCPI manual generator can read it), a fixed severity and a default
// message. A raise adds optional supplementary text.
#pragma once
#include <stdint.h>
#include "ServiceId.h"

namespace gte {

enum class Severity : uint8_t { WARNING, FATAL };

struct ErrorCode {
    uint16_t code;
    Severity severity;
    const char *message;
};

constexpr uint16_t service_code_base(ServiceId id) { return (uint16_t)(100 * ((uint16_t)id + 1)); }
constexpr bool code_belongs_to(uint16_t code, ServiceId id)
{
    return code > service_code_base(id) && code < service_code_base(id) + 100;
}

}  // namespace gte

#define GTE_ERROR_CODE(NAME, CODE, SEVERITY, MESSAGE) \
    constexpr ::gte::ErrorCode NAME{(CODE), ::gte::Severity::SEVERITY, (MESSAGE)}
