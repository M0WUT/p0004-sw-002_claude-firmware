// Service identities. The numbers are fixed forever: Error Codes (100 x (id + 1) + n), config
// storage and SCPI all depend on them. Add new Services at the end; Instruments add their own
// APPLICATION_x entry (editing this enum per Instrument is allowed).
#pragma once
#include <stdint.h>

namespace gte {

enum class ServiceId : uint8_t {
    CORE = 0,
    NETWORK = 1,
    WEB = 2,
    LCD = 3,  // the Display Service (DISPLAY is an Arduino macro)
    UI = 4,
    THERMAL = 5,
    POWER = 6,
    STORAGE = 7,
    USB_DEVICE = 8,
    USB_HOST = 9,
    SCPI = 10,
    HOME_ASSISTANT = 11,
    APPLICATION_EXAMPLE = 12,
    APPLICATION_TEST = 13,  // hardware_tests/apps/test only
    TEST_AUX = 14,          // hardware_tests/apps/test only
};

const char *service_name(ServiceId id);

}  // namespace gte
