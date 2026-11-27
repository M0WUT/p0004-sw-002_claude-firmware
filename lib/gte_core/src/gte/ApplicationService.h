// Base class for an Instrument's Application Service.
#pragma once
#include "BaseService.h"

namespace gte {

class ApplicationService : public BaseService {
public:
    using BaseService::BaseService;

    // Called at startup after Core has checked the Instrument Board's model against the
    // Application's list (issue #36). Override to add Instrument-specific checks; false is a
    // Fatal Error.
    virtual bool validate_instrument_board() { return true; }
};

}  // namespace gte
