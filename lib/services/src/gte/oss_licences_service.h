// The OSS_LICENCES Service (issue #55): one Status Value per open source
// component linked into the firmware, labelled with its name and version, whose
// value is its licence (SPDX id) and a link to the licence's authoritative
// text. The first two rows are the firmware itself (MIT) and where its source
// is. The Application adds rows for its own libraries
// (ApplicationService::Acknowledge). The values never change.
#pragma once
#include "gte/application_service.h"
#include "gte/base_service.h"
#include "gte/status.h"

namespace gte {

class OssLicencesService : public BaseService {
 public:
  OssLicencesService()
      : BaseService(ServiceId::kOssLicences, TaskSettings{1000, 512, 1}) {}

  void SetApplication(ApplicationService *app) { app_ = app; }

 protected:
  void Initialise() override;
  void GetStatus(StatusList *list) override;

 private:
  ApplicationService *app_ = nullptr;
  // Labels with versions, made once in Initialise() from the components' own
  // version macros, so they always match what is linked.
  char arduino_core_[48] = {};
  char wire_[48] = {};
  char variant_[56] = {};
  char hal_[40] = {};
  char cmsis_device_[48] = {};
  char cmsis_core_[32] = {};
  char freertos_[32] = {};
  char newlib_[32] = {};
  char gcc_[48] = {};
};

}  // namespace gte
