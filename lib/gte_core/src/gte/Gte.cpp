#include "Gte.h"
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <stdio.h>
#include "CoreErrors.h"
#include "CoreService.h"
#include "Fatal.h"
#include "LoggingHelper.h"
#include "ServiceHelper.h"
#include "TimeHelper.h"
#include "Watchdog.h"
#include "version.h"

namespace gte {

static TimeHelper g_time;
static LoggingHelper g_log;
static ServiceHelper g_services;
static Watchdog g_watchdog;
static CoreService g_core;

TimeHelper &time() { return g_time; }
LoggingHelper &log() { return g_log; }
ServiceHelper &services() { return g_services; }
Watchdog &watchdog() { return g_watchdog; }

void helper_log(const char *helper, LogLevel level, const char *message)
{
    BaseService *core = g_services.get(ServiceId::CORE);
    LogLevel threshold = core ? core->log_level() : LogLevel::INFO;
    if (level >= threshold)
        g_log.log(helper, level, 0, message);
}

static const char *pll_source()
{
    switch (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC) {
    case RCC_PLLCKSELR_PLLSRC_HSE: return "HSE";
    case RCC_PLLCKSELR_PLLSRC_CSI: return "CSI";
    case RCC_PLLCKSELR_PLLSRC_HSI: return "HSI";
    default: return "NONE";
    }
}

static const char *sysclk_source()
{
    switch (RCC->CFGR & RCC_CFGR_SWS) {
    case RCC_CFGR_SWS_PLL1: return "PLL1";
    case RCC_CFGR_SWS_HSE: return "HSE";
    case RCC_CFGR_SWS_CSI: return "CSI";
    default: return "HSI";
    }
}

static int voltage_scale()
{
    // VOS0 = VOS1 in PWR_D3CR plus the overdrive bit SYSCFG_PWRCR.ODEN.
    uint32_t vos = (PWR->D3CR & PWR_D3CR_VOS) >> PWR_D3CR_VOS_Pos;  // 3 = scale 1 ... 1 = scale 3
    if (vos == 3 && (SYSCFG->PWRCR & SYSCFG_PWRCR_ODEN))
        return 0;
    return 4 - (int)vos;
}

static const char *usb_source()
{
    switch ((RCC->D2CCIP2R & RCC_D2CCIP2R_USBSEL) >> RCC_D2CCIP2R_USBSEL_Pos) {
    case 1: return "PLL1Q";
    case 2: return "PLL3Q";
    case 3: return "HSI48";
    default: return "OFF";
    }
}

void start()
{
    Serial.begin(115200);
    Serial.printf("\r\nGTE boot env=%s version=%s built=%s\r\n", GTE_ENV_NAME, VERSION_STRING, BUILD_TIMESTAMP);
    PLL1_ClocksTypeDef pll1;
    HAL_RCCEx_GetPLL1ClockFreq(&pll1);
    Serial.printf("GTE clocks pllsrc=%s hserdy=%d sws=%s sysclk=%lu vos=%d usbsel=%s usbclk=%lu\r\n", pll_source(),
                  (RCC->CR & RCC_CR_HSERDY) ? 1 : 0, sysclk_source(), (unsigned long)HAL_RCC_GetSysClockFreq(),
                  voltage_scale(), usb_source(), (unsigned long)pll1.PLL1_Q_Frequency);
    Serial.printf("GTE reset reason=%s\r\n", reset_reason());

    g_log.begin(&g_time);
    g_services.register_service(&g_core);
    BaseService **extra = nullptr;
    size_t extraCount = 0;
    gte_extra_services(extra, extraCount);
    for (size_t i = 0; i < extraCount; i++)
        g_services.register_service(extra[i]);
    ApplicationService *app = gte_application();
    if (app) {
        g_services.register_service(app);
        char msg[80];
        snprintf(msg, sizeof(msg), "Application: %s", app->name());
        helper_log("ServiceHelper", LogLevel::INFO, msg);
    } else {
        helper_log("ServiceHelper", LogLevel::INFO, "No Application");
    }

    g_services.initialise_all();
    if (app) {
        if (!app->validate_instrument_board())
            fatal("CORE", errors::INSTRUMENT_BOARD_REJECTED, app->name());
        helper_log("ServiceHelper", LogLevel::INFO, "Instrument Board accepted by the Application");
    }
    g_services.start();
}

}  // namespace gte

__attribute__((weak)) gte::ApplicationService *gte_application() { return nullptr; }
__attribute__((weak)) void gte_extra_services(gte::BaseService **&list, size_t &count)
{
    list = nullptr;
    count = 0;
}

// The STM32duino FreeRTOS library defines these hooks non-weak; platformio.ini wraps them
// (-Wl,--wrap=...) so the kernel calls these instead.
extern "C" void __wrap_vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    gte::BaseService *s = gte::services().for_task(task);
    gte::fatal("CORE", gte::errors::STACK_OVERFLOW, s ? s->name() : name);
}

extern "C" void __wrap_vApplicationMallocFailedHook(void) { gte::fatal("CORE", gte::errors::OUT_OF_MEMORY); }
