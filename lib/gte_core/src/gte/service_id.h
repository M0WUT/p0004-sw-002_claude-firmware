// Copyright 2026 M0WUT
// Service identities. The numbers are fixed forever: Error Codes (100 x (id +
// 1) + n), config storage and SCPI all depend on them. Add new Services at the
// end; Instruments add their own APPLICATION_x Entry (editing this enum per
// Instrument is allowed).
#pragma once
#include <cstdint>

namespace gte {

enum class ServiceId : uint8_t {
  kCore = 0,
  kNetwork = 1,
  kWeb = 2,
  kDisplay = 3,  // the Display Service (DISPLAY is an Arduino macro)
  kUi = 4,
  kThermal = 5,
  kPower = 6,
  kStorage = 7,
  kUsbDevice = 8,
  kUsbHost = 9,
  kScpi = 10,
  kHomeAssistant = 11,
  kApplicationExample = 12,
  kApplicationTest = 13,  // hardware_tests/apps/test only
  kTestAux = 14,          // hardware_tests/apps/test only
  kTestBig = 15,          // hardware_tests/apps/test only
};

const char *ServiceName(ServiceId id);

}  // namespace gte
