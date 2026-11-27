// Base class for an Instrument's Application Service.
#pragma once
#include "gte/base_service.h"
#include "gte/status.h"

namespace gte {

class ApplicationService : public BaseService {
 public:
  using BaseService::BaseService;

  // Called at startup after Core has checked the Instrument Board's model
  // against the Application's list (issue #36). Override to add
  // Instrument-specific checks; false is a Fatal Error.
  virtual bool ValidateInstrumentBoard() { return true; }

  // Add a row to the OSS_LICENCES Service for each open source library the
  // Application links that Core doesn't (issue #55), with StaticText:
  // id, "<name> <version>", "<SPDX id> (https://spdx.org/licenses/<id>.html)".
  virtual void Acknowledge(StatusList *list) { (void)list; }
};

}  // namespace gte
