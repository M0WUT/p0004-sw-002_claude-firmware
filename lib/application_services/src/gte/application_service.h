// Base class for an Instrument's Application Service.
#pragma once
#include "gte/base_service.h"
#include "gte/status.h"

namespace gte {

class ApplicationService : public BaseService {
 public:
  using BaseService::BaseService;

  // The Instrument's name and version: the USB Product string and *IDN?'s
  // second and fourth fields (issues #15, #38).
  virtual const char *instrument_name() const = 0;
  virtual const char *instrument_version() const = 0;

  // Called at startup after Core has checked the Instrument Board's model
  // against the Application's list (issue #36). Override to add
  // Instrument-specific checks; false is a Fatal Error.
  virtual bool ValidateInstrumentBoard() { return true; }

  // Add a row to the OSS_LICENCES Service for each open source library the
  // Application links that Core doesn't (issue #55), with StaticText:
  // id, "<name> <version>", "<SPDX id> (https://spdx.org/licenses/<id>.html)".
  virtual void Acknowledge(StatusList *list) { (void)list; }

  // *RST: return to a safe remote state (issue #15). Runs under this
  // Service's lock, from the SCPI task.
  void RemoteReset() {
    Lock();
    OnRemoteReset();
    Unlock();
  }

 protected:
  virtual void OnRemoteReset() {}
};

}  // namespace gte
