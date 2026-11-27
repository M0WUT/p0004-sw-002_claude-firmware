// The CORE Service. Its Log level also applies to the Helpers. Board reporting (issue #45) and
// the boot-time EEPROM fatal/watchdog record (issue #43) are added later.
#pragma once
#include "BaseService.h"

namespace gte {

class CoreService : public BaseService {
public:
    CoreService() : BaseService(ServiceId::CORE, TaskSettings{1000, 1024, 2}) {}

protected:
    void initialise() override {}
};

}  // namespace gte
