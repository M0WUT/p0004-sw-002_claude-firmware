#include "Fatal.h"
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <stdio.h>
#include "Gte.h"
#include "TimeHelper.h"
#include "LoggingHelper.h"
#include "Watchdog.h"

namespace gte {

static void uart_put(const char *s)
{
    while (*s) {
        while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
        USART2->TDR = (uint8_t)*s++;
    }
    while (!(USART2->ISR & USART_ISR_TC)) {}
}

static void wait_ms(uint32_t ms)
{
    // Interrupts are masked, so no SysTick: count core cycles, feeding the IWDG as we go.
    const uint32_t per_ms = SystemCoreClock / 1000;
    for (uint32_t i = 0; i < ms; i++) {
        uint32_t start = DWT->CYCCNT;
        while (DWT->CYCCNT - start < per_ms) {}
        if (i % 100 == 0)
            Watchdog::feed();
    }
}

static void led_red(bool on)
{
    // Direct register writes (active low): PA10 red, PA8 green and PA9 blue off.
    GPIOA->BSRR = (on ? (1u << (10 + 16)) : (1u << 10)) | (1u << 8) | (1u << 9);
}

void fatal(const char *source, const ErrorCode &code, const char *supplementary)
{
    bool inIsr = __get_IPSR() != 0;
    if (!inIsr && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskSuspendAll();
    __disable_irq();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    char ts[32];
    TimeHelper::format_ms(time().now_ms(), ts, sizeof(ts));
    char line[256];
    LoggingHelper::format_line(line, sizeof(line), ts, source, LogLevel::ERROR, code.code, code.message, supplementary);

    uart_put("\r\n\x1b[31m");
    uart_put(line);
    uart_put("\x1b[0m\r\n");
    // A display hook (the LCD's Fatal Error screen) is added by the Display Service.

    bool on = true;
    for (uint32_t n = 0;; n++) {
        led_red(on);
        wait_ms(1000);
        on = !on;
        if (n % 2 == 1) {  // every 2 s
            uart_put("\x1b[31mFATAL ");
            uart_put(line);
            uart_put("\x1b[0m\r\n");
        }
    }
}

}  // namespace gte
