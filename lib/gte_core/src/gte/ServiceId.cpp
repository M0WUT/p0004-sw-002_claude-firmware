#include "ServiceId.h"

namespace gte {

const char *service_name(ServiceId id)
{
    switch (id) {
    case ServiceId::CORE: return "CORE";
    case ServiceId::NETWORK: return "NETWORK";
    case ServiceId::WEB: return "WEB";
    case ServiceId::LCD: return "DISPLAY";
    case ServiceId::UI: return "UI";
    case ServiceId::THERMAL: return "THERMAL";
    case ServiceId::POWER: return "POWER";
    case ServiceId::STORAGE: return "STORAGE";
    case ServiceId::USB_DEVICE: return "USB_DEVICE";
    case ServiceId::USB_HOST: return "USB_HOST";
    case ServiceId::SCPI: return "SCPI";
    case ServiceId::HOME_ASSISTANT: return "HOME_ASSISTANT";
    case ServiceId::APPLICATION_EXAMPLE: return "APPLICATION_EXAMPLE";
    case ServiceId::APPLICATION_TEST: return "APPLICATION_TEST";
    case ServiceId::TEST_AUX: return "TEST_AUX";
    }
    return "?";
}

}  // namespace gte
