#include "gte/fatal.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <cstdio>

#include "gte/config_storage.h"
#include "gte/gte.h"
#include "gte/logging_helper.h"
#include "gte/time_helper.h"
#include "gte/watchdog.h"

namespace gte {

static void UartPut(const char *s) {
  while (*s) {
    while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {
    }
    USART2->TDR = static_cast<uint8_t>(*s++);
  }
  while (!(USART2->ISR & USART_ISR_TC)) {
  }
}

static void WaitMs(uint32_t ms) {
  // Interrupts are masked, so no SysTick: count core cycles, feeding the IWDG
  // as we go.
  const uint32_t per_ms = SystemCoreClock / 1000;
  for (uint32_t i = 0; i < ms; i++) {
    uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < per_ms) {
    }
    if (i % 100 == 0) Watchdog::Feed();
  }
}

static void LedRed(bool on) {
  // Direct register writes (active low): PA10 red, PA8 green and PA9 blue off.
  GPIOA->BSRR = (on ? (1u << (10 + 16)) : (1u << 10)) | (1u << 8) | (1u << 9);
}

void Fatal(const char *source, const ErrorCode &code,
           const char *supplementary) {
  bool in_isr = __get_IPSR() != 0;
  if (!in_isr && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    vTaskSuspendAll();
  __disable_irq();

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  char ts[32];
  TimeHelper::FormatMs(time().NowMs(), ts, sizeof(ts));
  char line[256];
  LoggingHelper::FormatLine(line, sizeof(line), ts, source, LogLevel::kError,
                            code.code, code.message, supplementary);

  char text[64];
  snprintf(text, sizeof(text), "%s%s%s", code.message,
           supplementary && *supplementary ? ": " : "",
           supplementary ? supplementary : "");
  storage().WriteRecordPolled(RecordKind::kFatal, code.code, source, text);

  UartPut("\r\n\x1b[31m");
  UartPut(line);
  UartPut("\x1b[0m\r\n");
  // A display hook (the LCD's Fatal Error screen) is added by the Display
  // Service.

  bool on = true;
  for (uint32_t n = 0;; n++) {
    LedRed(on);
    WaitMs(1000);
    on = !on;
    if (n % 2 == 1) {  // every 2 s
      UartPut("\x1b[31mFATAL ");
      UartPut(line);
      UartPut("\x1b[0m\r\n");
    }
  }
}

}  // namespace gte
