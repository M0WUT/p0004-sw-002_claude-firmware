// Clock tree for the GTE LCD Board, P0004-PCB-001 v1.
//
// 25 MHz HSE crystal -> PLL1: /M5 = 5 MHz, xN192 = 960 MHz VCO (wide range),
//   P /2 = 480 MHz SYSCLK (VOS0), Q /20 = 48 MHz for USB, SDMMC and QSPI.
// PLL2 and PLL3 (25 MHz /15 x96 /2 = 80 MHz) clock the ADC, UARTs, SPIs and I2C.
// Overrides the weak SystemClock_Config() in the STM32duino core.
#include "pins_arduino.h"

#if defined(ARDUINO_P0004_PCB_001_V1)
#ifdef __cplusplus
extern "C" {
#endif

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {};
  RCC_ClkInitTypeDef clk = {};
  RCC_PeriphCLKInitTypeDef periph = {};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);  // VOS0, needed for 480 MHz
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM = 5;
  osc.PLL.PLLN = 192;
  osc.PLL.PLLP = 2;
  osc.PLL.PLLQ = 20;
  osc.PLL.PLLR = 2;
  osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;     // 5 MHz input: 4-8 MHz range
  osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  osc.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler();
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                  | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.SYSCLKDivider = RCC_SYSCLK_DIV1;     // 480 MHz core
  clk.AHBCLKDivider = RCC_HCLK_DIV2;       // 240 MHz AXI/AHB
  clk.APB3CLKDivider = RCC_APB3_DIV2;      // 120 MHz
  clk.APB1CLKDivider = RCC_APB1_DIV2;
  clk.APB2CLKDivider = RCC_APB2_DIV2;
  clk.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }

  periph.PeriphClockSelection = RCC_PERIPHCLK_USB | RCC_PERIPHCLK_QSPI | RCC_PERIPHCLK_SDMMC
                                | RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_LPUART1 | RCC_PERIPHCLK_USART16
                                | RCC_PERIPHCLK_USART234578 | RCC_PERIPHCLK_I2C123 | RCC_PERIPHCLK_I2C4
                                | RCC_PERIPHCLK_SPI123 | RCC_PERIPHCLK_SPI45 | RCC_PERIPHCLK_SPI6;
  periph.PLL2.PLL2M = 15;
  periph.PLL2.PLL2N = 96;
  periph.PLL2.PLL2P = 2;
  periph.PLL2.PLL2Q = 2;
  periph.PLL2.PLL2R = 2;
  periph.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
  periph.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  periph.PLL2.PLL2FRACN = 0;
  periph.PLL3.PLL3M = 15;
  periph.PLL3.PLL3N = 96;
  periph.PLL3.PLL3P = 2;
  periph.PLL3.PLL3Q = 2;
  periph.PLL3.PLL3R = 2;
  periph.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
  periph.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
  periph.PLL3.PLL3FRACN = 0;
  periph.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  periph.UsbClockSelection = RCC_USBCLKSOURCE_PLL;          // PLL1Q, 48 MHz
  periph.QspiClockSelection = RCC_QSPICLKSOURCE_PLL;
  periph.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
  periph.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PLL2;
  periph.Usart16ClockSelection = RCC_USART16CLKSOURCE_PLL2;
  periph.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL2;
  periph.I2c123ClockSelection = RCC_I2C123CLKSOURCE_PLL3;
  periph.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PLL3;
  periph.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
  periph.Spi45ClockSelection = RCC_SPI45CLKSOURCE_PLL2;
  periph.Spi6ClockSelection = RCC_SPI6CLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
    Error_Handler();
  }
}

#ifdef __cplusplus
}
#endif
#endif /* ARDUINO_P0004_PCB_001_V1 */
