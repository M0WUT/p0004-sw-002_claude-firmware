#include "gte/service_id.h"

namespace gte {

const char *ServiceName(ServiceId id) {
  switch (id) {
    case ServiceId::kCore:
      return "CORE";
    case ServiceId::kNetwork:
      return "NETWORK";
    case ServiceId::kWeb:
      return "WEB";
    case ServiceId::kDisplay:
      return "DISPLAY";
    case ServiceId::kUi:
      return "UI";
    case ServiceId::kThermal:
      return "THERMAL";
    case ServiceId::kPower:
      return "POWER";
    case ServiceId::kStorage:
      return "STORAGE";
    case ServiceId::kUsbDevice:
      return "USB_DEVICE";
    case ServiceId::kUsbHost:
      return "USB_HOST";
    case ServiceId::kScpi:
      return "SCPI";
    case ServiceId::kHomeAssistant:
      return "HOME_ASSISTANT";
    case ServiceId::kApplicationExample:
      return "APPLICATION_EXAMPLE";
    case ServiceId::kApplicationTest:
      return "APPLICATION_TEST";
    case ServiceId::kTestAux:
      return "TEST_AUX";
    case ServiceId::kTestBig:
      return "TEST_BIG";
    case ServiceId::kOssLicences:
      return "OSS_LICENCES";
  }
  return "?";
}

}  // namespace gte
