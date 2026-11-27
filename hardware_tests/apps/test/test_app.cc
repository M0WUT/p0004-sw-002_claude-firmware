// Copyright 2026 M0WUT
// Test Application for the hardware suite (never shipped). Two Services
// exercise the Service framework and config storage; pytest picks a scenario by
// writing kScenarioAddr over SWD while the core is held in reset (DTCM is
// unused by the linker and untouched by startup code). Issues #42 and #43.
//
// TEST_CONFIG_VERSION (1 by default) selects the config structs, so the
// migration tests can build version 2 (with 1->2 steps) and version 3 (with
// only the 2->3 step for APPLICATION_TEST).
#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <cinttypes>
#include <cstring>

#include "gte/application_service.h"
#include "gte/config_storage.h"
#include "gte/gte.h"
#include "gte/logging_helper.h"
#include "gte/service_helper.h"

using gte::ApplicationService;
using gte::BaseService;
using gte::ConfigList;
using gte::ConfigResult;
using gte::ConfigValue;
using gte::DisplayHint;
using gte::ErrorCode;
using gte::log;
using gte::ServiceId;
using gte::services;
using gte::StatusList;
using gte::TaskSettings;

GTE_ERROR_CODE(kTestWarning1, 1401, kWarning, "Test warning 1");
GTE_ERROR_CODE(kTestWarning2, 1402, kWarning, "Test warning 2");
GTE_ERROR_CODE(kTestWarning3, 1403, kWarning, "Test warning 3");
GTE_ERROR_CODE(kTestWarning4, 1404, kWarning, "Test warning 4");
GTE_ERROR_CODE(kTestWarning5, 1405, kWarning, "Test warning 5");
GTE_ERROR_CODE(kTestWarning6, 1406, kWarning, "Test warning 6");
GTE_ERROR_CODE(kTestWarning7, 1407, kWarning, "Test warning 7");
GTE_ERROR_CODE(kTestWarning8, 1408, kWarning, "Test warning 8");
GTE_ERROR_CODE(kTestWarning9, 1409, kWarning, "Test warning 9");
GTE_ERROR_CODE(kTestFatal, 1450, kFatal, "Test fatal error");
static_assert(CodeBelongsTo(kTestWarning1.code, ServiceId::kApplicationTest),
              "codes are 1401-1499");

static const ErrorCode *const kWarnings[] = {
    &kTestWarning1, &kTestWarning2, &kTestWarning3,
    &kTestWarning4, &kTestWarning5, &kTestWarning6,
    &kTestWarning7, &kTestWarning8, &kTestWarning9};

enum Scenario : uint16_t {
  kNormal = 0,
  kWarningsCycle = 1,
  kNineWarnings = 2,
  kFatalError = 3,
  kHang = 4,
  kStackOverflowTask = 5,
  kLogLevels = 6,
  kSetValues = 7,
  kEepromFull = 8,
  kNoApp = 9
};

#ifndef TEST_CONFIG_VERSION
#define TEST_CONFIG_VERSION 1
#endif

static volatile uint32_t *const kScenarioAddr =
    reinterpret_cast<volatile uint32_t *>(0x2001FFF0);
static uint16_t g_scenario = kNormal;

// Config actions, set by pytest over SWD and applied before config loads:
// 1 = fresh (empty table, no last-reset record), 2 = blank, 3 = corrupt the
// allocation table, 4 = corrupt TEST_AUX's block.
static volatile uint32_t *const kConfigActionAddr =
    reinterpret_cast<volatile uint32_t *>(0x2001FFF8);
extern "C" uint8_t gte_config_image[];

void GteTestBeforeConfig() {
  uint32_t v = *kConfigActionAddr;
  *kConfigActionAddr = 0;
  if ((v >> 16) != 0x4346) return;
  gte::ConfigStorage &s = gte::storage();
  const uint16_t action = v & 0xFFFF;
  switch (action) {
    case 1:
      s.Format();
      s.ClearRecord();
      break;
    case 2: {
      const uint8_t blank[3] = {0xFF, 0xFF, 0xFF};
      s.Poke(0, blank, sizeof(blank));
      break;
    }
    case 3: {
      const uint8_t b = gte_config_image[4] ^ 0x01;
      s.Poke(4, &b, 1);
      break;
    }
    case 4: {
      const uint16_t base =
          s.BlockAddress(static_cast<uint8_t>(ServiceId::kTestAux));
      if (base != 0xFFFF) {
        const uint8_t b = gte_config_image[base + 5] ^ 0xFF;
        s.Poke(base + 5, &b, 1);
      }
      break;
    }
    default:
      break;
  }
  log().Log("TEST", gte::LogLevel::kInfo, 0,
            action == 1 ? "Config action: fresh" : "Config action: damage");
}

static uint16_t PeekScenario() {
  uint32_t v = *kScenarioAddr;
  return (v >> 16) == 0x6765 ? static_cast<uint16_t>(v & 0xFFFF) : kNormal;
}

static uint16_t TakeScenario() {
  uint32_t v = *kScenarioAddr;
  *kScenarioAddr =
      0;  // one-shot: a later reset (e.g. by the watchdog) runs kNormal
  return (v >> 16) == 0x6765 ? static_cast<uint16_t>(v & 0xFFFF) : kNormal;
}

// TEST_AUX config: version 2 adds a 40-byte tag, so its block grows and every
// block after it has to shift.
struct AuxConfigV1 {
  int32_t rate = 5;
};
struct AuxConfigV2 {
  int32_t rate = 5;
  char tag[40] = "AUX";
};
#if TEST_CONFIG_VERSION >= 2
using AuxConfig = AuxConfigV2;
constexpr uint8_t kAuxVersion = 2;
#else
using AuxConfig = AuxConfigV1;
constexpr uint8_t kAuxVersion = 1;
#endif

class TestAux : public BaseService {
 public:
  TestAux() : BaseService(ServiceId::kTestAux, TaskSettings{250, 1024, 2}) {}

 protected:
  void Initialise() override { LogInfo("Aux initialise"); }

  gte::ConfigBlock GetConfigBlock() override {
    return {&c_, sizeof(c_), kAuxVersion};
  }

  void DeclareConfig(ConfigList *l) override {
    l->Integer("rate", "Rate", &c_.rate, 1, 100, 1);
#if TEST_CONFIG_VERSION >= 2
    l->Text("tag", "Tag", c_.tag, sizeof(c_.tag));
#endif
  }

  int MigrateConfig(uint8_t from_version, const uint8_t *in, size_t in_size,
                    uint8_t *out, size_t out_capacity) override {
    if (from_version != 1 || in_size != sizeof(AuxConfigV1) ||
        out_capacity < sizeof(AuxConfigV2))
      return -1;
    AuxConfigV1 v1;
    memcpy(&v1, in, sizeof(v1));
    AuxConfigV2 v2;
    v2.rate = v1.rate;
    snprintf(v2.tag, sizeof(v2.tag), "MIGRATED");
    memcpy(out, &v2, sizeof(v2));
    return sizeof(v2);
  }

  void Tick() override {
    n_++;
    LogDebugf("aux debug %" PRIu32, n_);
    LogInfof("aux tick %" PRIu32, n_);
    if (g_scenario == kHang && n_ == 4) {
      LogInfo("Hanging on purpose");
      for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (g_scenario == kStackOverflowTask && n_ == 4) {
      LogInfo("Overflowing the stack on purpose");
      Recurse(0);
    }
  }

 private:
  static uint32_t Recurse(uint32_t depth) {
    volatile uint8_t pad[256];
    for (auto &b : pad) b = static_cast<uint8_t>(depth);
    vTaskDelay(1);  // context switch: lets FreeRTOS check the stack
    return Recurse(depth + 1) + pad[depth % sizeof(pad)];
  }
  AuxConfig c_;
  uint32_t n_ = 0;
};

// Big enough not to fit in the config area: registered only for kEepromFull.
class TestBig : public BaseService {
 public:
  TestBig() : BaseService(ServiceId::kTestBig, TaskSettings{1000, 512, 2}) {}

 protected:
  void Initialise() override {}
  gte::ConfigBlock GetConfigBlock() override {
    return {blob_, sizeof(blob_), 1};
  }

 private:
  uint8_t blob_[30000] = {};
};

// APPLICATION_TEST config. Version 2 adds an offset (1->2 sets it to twice the
// count); version 3 adds an extra field but has no 1->2 step, so version 1
// data is a missing-migration Fatal Error.
struct AppConfigV1 {
  int32_t count = 10;
  float gain = 1.5f;
  bool enabled = true;
  char name[17] = "GTE";
  uint8_t mode = 1;
  char password[17] = "hunter2";
  int32_t channels = 4;
};
struct AppConfigV2 : AppConfigV1 {
  int32_t offset = 7;
};
struct AppConfigV3 : AppConfigV2 {
  int32_t extra = 3;
};
#if TEST_CONFIG_VERSION >= 3
using AppConfig = AppConfigV3;
#elif TEST_CONFIG_VERSION == 2
using AppConfig = AppConfigV2;
#else
using AppConfig = AppConfigV1;
#endif

class TestApp : public ApplicationService {
 public:
  TestApp()
      : ApplicationService(ServiceId::kApplicationTest,
                           TaskSettings{100, 2048, 2}) {}

  bool ValidateInstrumentBoard() override {
    LogInfo("validate_instrument_board called");
    return true;
  }

 protected:
  void Initialise() override {
    g_scenario = TakeScenario();
    LogInfof("Test scenario %u", g_scenario);
  }

  void DeclareConfig(ConfigList *l) override {
    static const char *const modes[] = {"OFF", "SLOW", "FAST"};
    l->Integer("count", "Count", &c_.count, 0, 100, 5);
    l->Number("gain", "Gain", &c_.gain, 0.0f, 10.0f, 0.5f, 1, "dB");
    l->Boolean("enabled", "Enabled", &c_.enabled);
    l->Text("name", "Name", c_.name, sizeof(c_.name));
    l->Enumeration("mode", "Mode", &c_.mode, modes, 3);
    Secret(l->Text("password", "Password", c_.password, sizeof(c_.password)));
    ResetMode(l->Integer("channels", "Channels", &c_.channels, 1, 8, 1));
#if TEST_CONFIG_VERSION >= 2
    l->Integer("offset", "Offset", &c_.offset, 0, 1000, 1);
#endif
#if TEST_CONFIG_VERSION >= 3
    l->Integer("extra", "Extra", &c_.extra, 0, 1000, 1);
#endif
  }

  gte::ConfigBlock GetConfigBlock() override {
    return {&c_, sizeof(c_), TEST_CONFIG_VERSION};
  }

  int MigrateConfig(uint8_t from_version, const uint8_t *in, size_t in_size,
                    uint8_t *out, size_t out_capacity) override {
#if TEST_CONFIG_VERSION == 2
    if (from_version == 1 && in_size == sizeof(AppConfigV1) &&
        out_capacity >= sizeof(AppConfigV2)) {
      AppConfigV2 v2;
      memcpy(static_cast<AppConfigV1 *>(&v2), in, sizeof(AppConfigV1));
      v2.offset = v2.count * 2;
      memcpy(out, &v2, sizeof(v2));
      return sizeof(v2);
    }
#elif TEST_CONFIG_VERSION >= 3
    if (from_version == 2 && in_size == sizeof(AppConfigV2) &&
        out_capacity >= sizeof(AppConfigV3)) {
      AppConfigV3 v3;
      memcpy(static_cast<AppConfigV2 *>(&v3), in, sizeof(AppConfigV2));
      memcpy(out, &v3, sizeof(v3));
      return sizeof(v3);
    }
#endif
    (void)from_version;
    (void)in;
    (void)in_size;
    (void)out;
    (void)out_capacity;
    return -1;  // no such step
  }

  bool ValidateConfigField(const char *id, const ConfigValue &v, char *reason,
                           size_t size) override {
    if (!strcmp(id, "gain") && v.number > 8.0f && c_.enabled) {
      snprintf(reason, size, "Gain above 8 dB needs Enabled OFF");
      return false;
    }
    return true;
  }

  void OnConfigChanged(const char *id) override {
    LogInfof("Side effect for %s", id);
  }

  void GetStatus(StatusList *s) override {
    s->Integer("dataUsed", "Data used", 1288490188LL, "B", DisplayHint::kBytes);
    s->Number("temperature", "Temperature", 23.5f, 1, "°C");
    s->Boolean("ready", "Ready", true);
    s->Text("label", "Label", "TEST");
  }

  void Tick() override {
    n_++;
    LogInfof("tick %" PRIu32, n_);
    switch (g_scenario) {
      case kNormal:
        if (n_ == 1) ExerciseConfigAndStatus();
        break;
      case kWarningsCycle:
        if (n_ == 5) Warn(kTestWarning1, "first");
        if (n_ == 10) Warn(kTestWarning1, "second");
        if (n_ == 15) Warn(kTestWarning2, "other");
        if (n_ == 20) ClearWarning(kTestWarning1);
        if (n_ == 25) ClearWarning(kTestWarning2);
        if (n_ == 5 || n_ == 10 || n_ == 15 || n_ == 20 || n_ == 25)
          LogInfof("active warnings=%u",
                   static_cast<unsigned>(log().WarningCount()));
        break;
      case kNineWarnings:
        if (n_ >= 5 && n_ < 14)
          Warnf(*kWarnings[n_ - 5], "number %" PRIu32, n_ - 4);
        break;
      case kFatalError:
        if (n_ == 10) FatalError(kTestFatal, "boom");
        break;
      case kSetValues:
        if (n_ == 1) {
          SetConfig("count", "20");
          SetConfig("gain", "3.0");
          SetConfig("name", "PERSIST");
          BaseService *aux = services().Get(ServiceId::kTestAux);
          aux->SetConfig("rate", "9");
          aux->SetConfig("logLevel", "WARNING");
          LogInfo("values set");
        }
        break;
      case kLogLevels:
        if (n_ == 10)
          services().Get(ServiceId::kCore)->SetConfig("logLevel", "DEBUG");
        if (n_ == 20) {
          // TEST_AUX's own confirmation is INFO, which its new level hides, so
          // report it here.
          ConfigResult r = services()
                               .Get(ServiceId::kTestAux)
                               ->SetConfig("logLevel", "WARNING");
          LogInfof("TEST_AUX logLevel -> WARNING: %s",
                   r.ok ? "accepted" : r.message);
        }
        break;
      default:
        break;
    }
  }

 private:
  void ExerciseConfigAndStatus() {
    SetConfig("count", "105");  // out of range
    SetConfig("count", "12");   // off step
    SetConfig("gain", "9.0");  // refused by ValidateConfigField (Enabled is ON)
    SetConfig("gain", "2.5");  // Live: applied, side effect runs
    SetConfig("channels", "6");  // Reset: restart required
    SetConfig("password", "newsecret");
    LogInfof("restart required=%d", services().restart_required() ? 1 : 0);
    StatusList list;
    Status(&list);
    for (size_t i = 0; i < list.count(); i++) {
      char shown[64], exact[64];
      list[i].Display(shown, sizeof(shown));
      list[i].Exact(exact, sizeof(exact));
      LogInfof("Status %s (%s): %s [exact %s]", list[i].id, list[i].label,
               shown, exact);
    }
  }

  AppConfig c_;
  uint32_t n_ = 0;
};

static TestAux g_aux;
static TestApp g_app;
static TestBig g_big;
static BaseService *g_extra[] = {&g_aux, &g_big};

ApplicationService *GteApplication() {
  if (PeekScenario() == kNoApp) {
    TakeScenario();  // nothing else will consume it
    return nullptr;
  }
  return &g_app;
}

gte::ServiceList GteExtraServices() {
  switch (PeekScenario()) {
    case kNoApp:
      return {};
    case kEepromFull:
      return {g_extra, 2};
    default:
      return {g_extra, 1};
  }
}
