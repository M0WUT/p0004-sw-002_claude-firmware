// Hardware test: SD card on SDMMC1 4-bit (STM32duino STM32SD + FatFs) under FreeRTOS.
// NON-DESTRUCTIVE: only creates and deletes files under /GTETEST; never formats, never writes raw blocks.
// Results are printed as "SD: ..." lines on the debug UART and mirrored into an SWD-readable RAM ring.
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <STM32SD.h>
#include "bsp_sd.h"
#include "pinmap.h"

// ---- pins (LCD Board rev 1; see hardware_tests docs / board pin map) ----
constexpr uint32_t PIN_SD_DETECT = PB8; // ~SD_CARD_DET: CN5 pin 10, 10k pull-up R12, low = card present

// ---- logging: polled USART2 + RAM ring (same scheme as usb_host_msc) ----
extern "C" {
volatile uint32_t spikeLogHead = 0;
alignas(32) char spikeLogRing[16384];
}
Uart dbg(PD_6, PD_5);

static void out(const char *s)
{
  for (; *s; s++)
  {
    spikeLogRing[spikeLogHead % sizeof(spikeLogRing)] = *s;
    spikeLogHead = spikeLogHead + 1;
    while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
    USART2->TDR = (uint8_t)*s;
  }
  while (!(USART2->ISR & USART_ISR_TC)) {}
}

static void logf(const char *fmt, ...)
{
  char buf[220];
  int n = snprintf(buf, sizeof(buf), "[%8lu] ", (unsigned long)millis());
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf + n, sizeof(buf) - n - 3, fmt, ap);
  va_end(ap);
  strcat(buf, "\r\n");
  out(buf);
}

// The LCD Board has no external pull-up on CMD (PD2) and the variant pin table leaves it floating,
// so override the weak MSP init: same pin/clock setup as STM32SD's, plus an internal pull-up on CMD.
extern "C" void BSP_SD_MspInit(SD_HandleTypeDef *hsd, void *Params)
{
  (void)Params;
  pinmap_pinout(SD_PinNames.pin_d0, PinMap_SD_DATA0);
  pinmap_pinout(SD_PinNames.pin_d1, PinMap_SD_DATA1);
  pinmap_pinout(SD_PinNames.pin_d2, PinMap_SD_DATA2);
  pinmap_pinout(SD_PinNames.pin_d3, PinMap_SD_DATA3);
  pinmap_pinout(SD_PinNames.pin_cmd, PinMap_SD_CMD);
  pinmap_pinout(SD_PinNames.pin_ck, PinMap_SD_CK);
  LL_GPIO_SetPinPull(GPIOD, LL_GPIO_PIN_2, LL_GPIO_PULL_UP); // CMD
  if (hsd->Instance == SDMMC1)
    __HAL_RCC_SDMMC1_CLK_ENABLE();
}

static uint32_t pattern(uint32_t i) { return i * 2654435761u ^ 0xA5C3F00Du; }

static void sd_test_task(void *)
{
  bool ok = true;
  logf("SD: === SD card hardware test (SDMMC1 4-bit, STM32SD %s) ===", "1.5.0");
  pinMode(PIN_SD_DETECT, INPUT_PULLUP);
  delay(2);
  int det = digitalRead(PIN_SD_DETECT);
  logf("SD: card detect PB8=%d (%s)", det, det == LOW ? "card present" : "NO CARD");

  SD.setDx(PC8, PC9, PC10, PC11);
  SD.setCK(PC12);
  SD.setCMD(PD2);
  // --- signal-integrity check (read-only): repeat card identification under different bus settings ---
  // Identification only reads card registers (OCR/CID/CSD/status); nothing is written to the card.
  {
    SD_PinNames.pin_d0 = PC_8; SD_PinNames.pin_d1 = PC_9; SD_PinNames.pin_d2 = PC_10; SD_PinNames.pin_d3 = PC_11;
    SD_PinNames.pin_ck = PC_12; SD_PinNames.pin_cmd = PD_2;
    logf("SD: diag SDMMC kernel clock %lu Hz", (unsigned long)HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
    struct { const char *name; uint32_t edge; uint32_t div; } modes[] = {
        {"rising, div 61 (~393 kHz)", SDMMC_CLOCK_EDGE_RISING, SDMMC_NSpeed_CLK_DIV},
        {"falling, div 61 (~393 kHz)", SDMMC_CLOCK_EDGE_FALLING, SDMMC_NSpeed_CLK_DIV},
        {"rising, div 244 (~98 kHz)", SDMMC_CLOCK_EDGE_RISING, 244},
    };
    for (auto &m : modes)
    {
      uint32_t ok = 0, sdhc = 0, sdsc = 0, other = 0, locked = 0, same = 0;
      static uint32_t refBlocks = 0; // capacity from the very first identification
      uint32_t firstErr = 0;
      for (int i = 0; i < 20; i++)
      {
        static SD_HandleTypeDef h;
        memset(&h, 0, sizeof(h));
        h.Instance = SDMMC1;
        h.Init.ClockEdge = m.edge;
        h.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
        h.Init.BusWide = SDMMC_BUS_WIDE_1B;
        h.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
        h.Init.ClockDiv = m.div;
        BSP_SD_MspInit(&h, nullptr);
        __HAL_RCC_SDMMC1_FORCE_RESET();
        __HAL_RCC_SDMMC1_RELEASE_RESET();
        HAL_StatusTypeDef st = HAL_SD_Init(&h);
        if (st == HAL_OK) ok++;
        if (h.ErrorCode & HAL_SD_ERROR_LOCK_UNLOCK_FAILED) locked++;
        if (h.ErrorCode && !firstErr) firstErr = h.ErrorCode;
        if (h.SdCard.CardType == CARD_SDHC_SDXC) sdhc++; else if (h.SdCard.CardType == CARD_SDSC) sdsc++; else other++;
        if (h.SdCard.LogBlockNbr == refBlocks) same++;
        if (!refBlocks) { refBlocks = h.SdCard.LogBlockNbr; same = 1; }
        HAL_SD_DeInit(&h);
        vTaskDelay(pdMS_TO_TICKS(20));
      }
      logf("SD: diag [%s] 20 inits: HAL_OK=%lu SDHC=%lu SDSC=%lu other=%lu same-capacity=%lu 'locked'=%lu firstErr=0x%08lX",
           m.name, (unsigned long)ok, (unsigned long)sdhc, (unsigned long)sdsc, (unsigned long)other,
           (unsigned long)same, (unsigned long)locked, (unsigned long)firstErr);
    }
  }

  uint32_t t0 = millis();
  if (!SD.begin(PIN_SD_DETECT, LOW))
  {
    logf("SD: init FAILED after %lu ms (no card, unsupported card, or not FAT formatted)", (unsigned long)(millis() - t0));
    {
      static FATFS probe;
      FRESULT fr = f_mount(&probe, "0:", 1);
      logf("SD: diag f_mount=%d (13=FR_NO_FILESYSTEM e.g. exFAT/unformatted, 3=FR_NOT_READY, 1=FR_DISK_ERR)", fr);
    }
    logf("SD: RESULT FAIL");
    vTaskDelete(nullptr);
  }
  logf("SD: init OK in %lu ms", (unsigned long)(millis() - t0));

  HAL_SD_CardInfoTypeDef ci;
  BSP_SD_GetCardInfo(&ci);
  uint64_t cardBytes = (uint64_t)ci.LogBlockNbr * ci.LogBlockSize;
  logf("SD: card type=%lu (0=SDSC 1=SDHC/SDXC) version=%lu class=0x%03lX blocks=%lu x %lu B = %lu MiB",
       (unsigned long)ci.CardType, (unsigned long)ci.CardVersion, (unsigned long)ci.Class,
       (unsigned long)ci.LogBlockNbr, (unsigned long)ci.LogBlockSize, (unsigned long)(cardBytes >> 20));
  logf("SD: SDMMC1 CLKCR=0x%08lX (CLKDIV=%lu, WIDBUS=%lu)", (unsigned long)SDMMC1->CLKCR,
       (unsigned long)(SDMMC1->CLKCR & 0x3FF), (unsigned long)((SDMMC1->CLKCR >> 14) & 3));

  FATFS *fs;
  DWORD freeClusters;
  FRESULT fr = f_getfree("0:", &freeClusters, &fs);
  if (fr != FR_OK)
  {
    logf("SD: f_getfree failed (%d)", fr);
    ok = false;
  }
  else
  {
    uint64_t clusterBytes = (uint64_t)fs->csize * 512;
    uint64_t total = (uint64_t)(fs->n_fatent - 2) * clusterBytes;
    uint64_t freeB = (uint64_t)freeClusters * clusterBytes;
    // newlib-nano printf has no %llu, so report sizes in KiB (fits 32 bits up to 4 TiB)
    logf("SD: filesystem FAT type=%u cluster=%lu B total=%lu KiB free=%lu KiB used=%lu KiB", fs->fs_type,
         (unsigned long)clusterBytes, (unsigned long)(total >> 10), (unsigned long)(freeB >> 10),
         (unsigned long)((total - freeB) >> 10));
  }

  // ---- file write / read-back / verify in /GTETEST (1 MiB, 16 KiB chunks) ----
  static uint32_t buf[4096]; // 16 KiB
  const uint32_t CHUNK = sizeof(buf), TOTAL = 1024 * 1024;
  SD.mkdir("GTETEST");
  const char *path = "GTETEST/TEST1M.BIN";
  if (SD.exists(path))
    SD.remove(path);
  File f = SD.open(path, FA_CREATE_ALWAYS | FA_WRITE);
  if (!f)
  {
    logf("SD: open for write FAILED");
    ok = false;
  }
  else
  {
    t0 = millis();
    uint32_t written = 0;
    for (uint32_t off = 0; off < TOTAL; off += CHUNK)
    {
      for (uint32_t i = 0; i < CHUNK / 4; i++)
        buf[i] = pattern(off / 4 + i);
      written += f.write((const uint8_t *)buf, CHUNK);
    }
    f.close();
    uint32_t dt = millis() - t0;
    logf("SD: write %lu B in %lu ms = %lu KiB/s", (unsigned long)written, (unsigned long)dt,
         (unsigned long)(dt ? (uint64_t)written * 1000 / 1024 / dt : 0));
    if (written != TOTAL)
      ok = false;

    f = SD.open(path, FA_READ);
    t0 = millis();
    uint32_t readBytes = 0, mismatches = 0;
    for (uint32_t off = 0; off < TOTAL && f; off += CHUNK)
    {
      int n = f.read(buf, CHUNK);
      if (n != (int)CHUNK)
        break;
      readBytes += n;
      for (uint32_t i = 0; i < CHUNK / 4; i++)
        if (buf[i] != pattern(off / 4 + i))
          mismatches++;
    }
    f.close();
    dt = millis() - t0;
    logf("SD: read %lu B in %lu ms = %lu KiB/s, mismatched words=%lu", (unsigned long)readBytes, (unsigned long)dt,
         (unsigned long)(dt ? (uint64_t)readBytes * 1000 / 1024 / dt : 0), (unsigned long)mismatches);
    if (readBytes != TOTAL || mismatches)
      ok = false;
    bool removed = SD.remove(path);
    bool rmdirOk = SD.rmdir("GTETEST");
    logf("SD: cleanup remove=%d rmdir=%d", removed, rmdirOk);
  }

  // ---- raw block reads (read-only) as the MSC drive would issue them ----
  t0 = millis();
  uint32_t blocksOk = 0;
  const uint32_t RAW_BLOCKS = 2048; // 1 MiB in 32-block (16 KiB) reads
  for (uint32_t lba = 0; lba < RAW_BLOCKS; lba += 32)
  {
    if (BSP_SD_ReadBlocks(buf, lba, 32, 1000) == MSD_OK)
    {
      while (BSP_SD_GetCardState() != 0) {} // 0 = SD_TRANSFER_OK (private to bsp_sd.c)
      blocksOk += 32;
    }
    if (lba == 0)
      logf("SD: raw block 0 signature %02X %02X", ((uint8_t *)buf)[510], ((uint8_t *)buf)[511]);
  }
  uint32_t dt = millis() - t0;
  logf("SD: raw read %lu of %lu blocks in %lu ms = %lu KiB/s", (unsigned long)blocksOk, (unsigned long)RAW_BLOCKS,
       (unsigned long)dt, (unsigned long)(dt ? (uint64_t)blocksOk * 512 * 1000 / 1024 / dt : 0));
  if (blocksOk != RAW_BLOCKS)
    ok = false;

  logf("SD: RESULT %s", ok ? "PASS" : "FAIL");
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(5000));
    logf("SD: heartbeat, card detect PB8=%d", digitalRead(PIN_SD_DETECT));
  }
}

void setup()
{
  dbg.begin(115200);
  xTaskCreate(sd_test_task, "sd", 16384 / 4, nullptr, 2, nullptr);
  vTaskStartScheduler();
}

void loop() {}
