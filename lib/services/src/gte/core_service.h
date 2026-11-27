// The CORE Service. Its Log level also applies to the Helpers. Board reporting
// (issue #45) and the boot-time EEPROM fatal/watchdog record (issue #43) are
// added later.
#pragma once
#include "gte/base_service.h"

namespace gte {

class CoreService : public BaseService {
 public:
  CoreService() : BaseService(ServiceId::kCore, TaskSettings{1000, 1024, 2}) {}

 protected:
  void Initialise() override {}
};

}  // namespace gte
