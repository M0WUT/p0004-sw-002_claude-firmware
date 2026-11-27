// GTE Core firmware: skeleton (Build: skeleton and test harness, issue #41).
// Prints a boot line and the clock configuration on the debug UART, then runs FreeRTOS with one
// task that ticks every 100 ms. The Service framework replaces this in issue #42.
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "version.h"

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
  uint32_t vos = (PWR->D3CR & PWR_D3CR_VOS) >> PWR_D3CR_VOS_Pos;  // 3 = scale 1, 2 = scale 2, 1 = scale 3
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

static void tick_task(void *)
{
  TickType_t last = xTaskGetTickCount();
  for (uint32_t n = 0;; n++) {
    Serial.printf("GTE tick %lu\r\n", (unsigned long)n);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.printf("\r\nGTE boot env=%s version=%s built=%s\r\n", GTE_ENV_NAME, VERSION_STRING, BUILD_TIMESTAMP);

  PLL1_ClocksTypeDef pll1;
  HAL_RCCEx_GetPLL1ClockFreq(&pll1);
  Serial.printf("GTE clocks pllsrc=%s hserdy=%d sws=%s sysclk=%lu vos=%d usbsel=%s usbclk=%lu\r\n",
                pll_source(), (RCC->CR & RCC_CR_HSERDY) ? 1 : 0, sysclk_source(),
                (unsigned long)HAL_RCC_GetSysClockFreq(), voltage_scale(), usb_source(),
                (unsigned long)pll1.PLL1_Q_Frequency);

  xTaskCreate(tick_task, "tick", 1024, nullptr, 2, nullptr);
  vTaskStartScheduler();
}

void loop()
{
}
