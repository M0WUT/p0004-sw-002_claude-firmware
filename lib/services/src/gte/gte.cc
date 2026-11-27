#include "gte/gte.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <cinttypes>
#include <cstdio>

#include "generated/version.h"
#include "gte/config_storage.h"
#include "gte/core_errors.h"
#include "gte/core_service.h"
#include "gte/fatal.h"
#include "gte/i2c_bus.h"
#include "gte/logging_helper.h"
#include "gte/service_helper.h"
#include "gte/soft_keys.h"
#include "gte/time_helper.h"
#include "gte/watchdog.h"

namespace gte {

static TimeHelper g_time;
static LoggingHelper g_log;
static ServiceHelper g_services;
static Watchdog g_watchdog;
static CoreService g_core;
static I2CBus g_i2c(GTE_PIN_I2C_INT_SDA, GTE_PIN_I2C_INT_SCL);
static ConfigStorage g_storage;
static SoftKeys g_keys;

TimeHelper &time() { return g_time; }
LoggingHelper &log() { return g_log; }
ServiceHelper &services() { return g_services; }
Watchdog &watchdog() { return g_watchdog; }
ConfigStorage &storage() { return g_storage; }
SoftKeys &keys() { return g_keys; }

void PromptReinitialise(const char *source, const char *message) {
  uint32_t last = 0;
  bool first = true;
  for (;;) {
    if (first || millis() - last >= 2000) {
      first = false;
      last = millis();
      g_log.Log(source, LogLevel::kError, 0, message);
    }
    if (g_keys.IsDown(4) && g_keys.HeldFor(1u << 3, 3000)) {
      g_log.Log(source, LogLevel::kInfo, 0,
                "Soft Key 4 held for 3 s: re-initialising");
      return;
    }
    delay(20);
  }
}

static void CheckFactoryReset() {
  // Runs before any Service loads its config, so a board stuck on a migration
  // Fatal Error can always be recovered (issue #12).
  const uint8_t keys14 = (1u << 0) | (1u << 3);
  if ((g_keys.Mask() & keys14) != keys14) return;
  g_log.Log(
      "CORE", LogLevel::kInfo, 0,
      "Soft Keys 1+4 held at power-on: keep holding for 5 s to Factory Reset");
  if (!g_keys.HeldFor(keys14, 5000)) {
    g_log.Log("CORE", LogLevel::kInfo, 0,
              "Factory Reset cancelled (keys released)");
    return;
  }
  g_storage.Format();
  g_storage.ClearRecord();
  g_log.Log("CORE", LogLevel::kInfo, 0, "Factory Reset done; restarting");
  Serial.flush();
  delay(20);
  NVIC_SystemReset();
}

static void CheckTable() {
  switch (g_storage.GetTableState()) {
    case TableState::kOk:
      return;
    case TableState::kBlank:
      PromptReinitialise("ConfigStorage",
                         "Blank EEPROM detected. Initialise with defaults? "
                         "Hold Soft Key 4 for 3 s to confirm.");
      break;
    case TableState::kCorrupt:
      PromptReinitialise("ConfigStorage",
                         "Config allocation table corrupt (CRC failed). Hold "
                         "Soft Key 4 for 3 s "
                         "to re-initialise the whole table to defaults.");
      break;
  }
  g_storage.Format();
  g_log.Log("ConfigStorage", LogLevel::kInfo, 0,
            "Config table initialised; every Service starts at defaults");
}

static void ReportLastReset() {
  ResetRecord rec;
  if (!g_storage.ReadRecord(&rec)) return;
  char msg[128];
  if (rec.kind == RecordKind::kFatal) {
    snprintf(msg, sizeof(msg), "Last Fatal Error: %u %s (%s)", rec.code,
             rec.text, rec.source);
    g_log.Log("CORE", LogLevel::kInfo, 0, msg);
  } else if (rec.kind == RecordKind::kWatchdog) {
    snprintf(msg, sizeof(msg), "Last reset: watchdog, Service %s", rec.source);
    g_log.Log("CORE", LogLevel::kInfo, 0, msg);
    char supp[40];
    snprintf(supp, sizeof(supp), "Service %s", rec.source);
    g_log.RaiseWarning("CORE", errors::kRecoveredWatchdog, supp);
  }
  g_storage.ClearRecord();
}

void HelperLog(const char *helper, LogLevel level, const char *message) {
  BaseService *core = g_services.Get(ServiceId::kCore);
  LogLevel threshold = core ? core->log_level() : LogLevel::kInfo;
  if (level >= threshold) g_log.Log(helper, level, 0, message);
}

static const char *PllSource() {
  switch (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC) {
    case RCC_PLLCKSELR_PLLSRC_HSE:
      return "HSE";
    case RCC_PLLCKSELR_PLLSRC_CSI:
      return "CSI";
    case RCC_PLLCKSELR_PLLSRC_HSI:
      return "HSI";
    default:
      return "NONE";
  }
}

static const char *SysclkSource() {
  switch (RCC->CFGR & RCC_CFGR_SWS) {
    case RCC_CFGR_SWS_PLL1:
      return "PLL1";
    case RCC_CFGR_SWS_HSE:
      return "HSE";
    case RCC_CFGR_SWS_CSI:
      return "CSI";
    default:
      return "HSI";
  }
}

static int VoltageScale() {
  // VOS0 = VOS1 in PWR_D3CR plus the overdrive bit SYSCFG_PWRCR.ODEN.
  uint32_t vos = (PWR->D3CR & PWR_D3CR_VOS) >>
                 PWR_D3CR_VOS_Pos;  // 3 = scale 1 ... 1 = scale 3
  if (vos == 3 && (SYSCFG->PWRCR & SYSCFG_PWRCR_ODEN)) return 0;
  return 4 - static_cast<int>(vos);
}

static const char *UsbSource() {
  switch ((RCC->D2CCIP2R & RCC_D2CCIP2R_USBSEL) >> RCC_D2CCIP2R_USBSEL_Pos) {
    case 1:
      return "PLL1Q";
    case 2:
      return "PLL3Q";
    case 3:
      return "HSI48";
    default:
      return "OFF";
  }
}

void Start() {
  Serial.begin(115200);
  Serial.printf("\r\nGTE boot env=%s version=%s built=%s\r\n", GTE_ENV_NAME,
                VERSION_STRING, BUILD_TIMESTAMP);
  PLL1_ClocksTypeDef pll1;
  HAL_RCCEx_GetPLL1ClockFreq(&pll1);
  Serial.printf("GTE clocks pllsrc=%s hserdy=%d sws=%s sysclk=%" PRIu32
                " vos=%d usbsel=%s "
                "usbclk=%" PRIu32 "\r\n",
                PllSource(), (RCC->CR & RCC_CR_HSERDY) ? 1 : 0, SysclkSource(),
                HAL_RCC_GetSysClockFreq(), VoltageScale(), UsbSource(),
                pll1.PLL1_Q_Frequency);
  Serial.printf("GTE reset reason=%s\r\n", ResetReason());

  g_log.Begin(&g_time);
  g_keys.Begin();
  g_i2c.Begin();
  g_storage.Begin(&g_i2c);
  uint8_t mac[6];
  if (g_storage.ReadMac(mac)) {
    char m[40];
    snprintf(m, sizeof(m), "MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);
    g_log.Log("CORE", LogLevel::kInfo, 0, m);
  }
  CheckFactoryReset();
  GteTestBeforeConfig();
  CheckTable();
  ReportLastReset();

  g_services.RegisterService(&g_core);
  ServiceList extra = GteExtraServices();
  for (size_t i = 0; i < extra.count; i++)
    g_services.RegisterService(extra.items[i]);
  ApplicationService *app = GteApplication();
  if (app) {
    g_services.RegisterService(app);
    char msg[80];
    snprintf(msg, sizeof(msg), "Application: %s", app->name());
    HelperLog("ServiceHelper", LogLevel::kInfo, msg);
  } else {
    HelperLog("ServiceHelper", LogLevel::kInfo, "No Application");
  }

  g_services.InitialiseAll();
  if (app) {
    if (!app->ValidateInstrumentBoard())
      Fatal("CORE", errors::kInstrumentBoardRejected, app->name());
    HelperLog("ServiceHelper", LogLevel::kInfo,
              "Instrument Board accepted by the Application");
  }
  g_keys.StartTask();
  g_services.Start();
}

}  // namespace gte

__attribute__((weak)) gte::ApplicationService *GteApplication() {
  return nullptr;
}
__attribute__((weak)) gte::ServiceList GteExtraServices() { return {}; }
__attribute__((weak)) void GteTestBeforeConfig() {}

// The STM32duino FreeRTOS library defines these hooks non-weak; platformio.ini
// wraps them
// (-Wl,--wrap=...) so the kernel calls these instead.
extern "C" void __wrap_vApplicationStackOverflowHook(TaskHandle_t task,
                                                     char *name) {
  gte::BaseService *s = gte::services().ForTask(task);
  gte::Fatal("CORE", gte::errors::kStackOverflow, s ? s->name() : name);
}

extern "C" void __wrap_vApplicationMallocFailedHook(void) {
  gte::Fatal("CORE", gte::errors::kOutOfMemory);
}
