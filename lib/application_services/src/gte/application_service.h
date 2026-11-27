// Copyright 2026 M0WUT
// Base class for an Instrument's Application Service.
#pragma once
#include "gte/base_service.h"

namespace gte {

class ApplicationService : public BaseService {
 public:
  using BaseService::BaseService;

  // Called at startup after Core has checked the Instrument Board's model
  // against the Application's list (issue #36). Override to add
  // Instrument-specific checks; false is a Fatal Error.
  virtual bool ValidateInstrumentBoard() { return true; }
};

}  // namespace gte
