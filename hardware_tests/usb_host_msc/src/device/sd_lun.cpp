// MSC drive 0 = the SD card (SDMMC1 4-bit), served with raw block reads/writes via the STM32SD BSP.
// Built only for env:storage (GTE_LUN0_SD=1). The firmware never mounts the filesystem here: with nothing
// Logging, the Connected PC owns the Volume read/write (see "How USB storage is presented to a PC").
#include "tusb_config.h"
#if SPIKE_DEVICE && GTE_LUN0_SD
#include <Arduino.h>
#include <STM32SD.h>
#include "bsp_sd.h"
#include "pinmap.h"
#include "tusb.h"

void spike_logf(const char *fmt, ...);

static constexpr uint32_t PIN_SD_DETECT = PB8; // ~SD_CARD_DET, low = card present
static Sd2Card card;
static volatile bool cardReady = false;   // BSP initialised and card info read
static volatile bool mediumChanged = false; // report UNIT ATTENTION once after an insert
static uint32_t blockCount = 0, blockSize = 512;
volatile uint32_t sdReads = 0, sdWrites = 0, sdErrors = 0, sdBytesRead = 0, sdBytesWritten = 0;

// The LCD Board has no pull-up on SD CMD (PD2), and the variant pin table leaves it floating:
// same pin/clock setup as STM32SD's weak default, plus an internal pull-up on CMD.
extern "C" void BSP_SD_MspInit(SD_HandleTypeDef *hsd, void *Params)
{
  (void)Params;
  pinmap_pinout(SD_PinNames.pin_d0, PinMap_SD_DATA0);
  pinmap_pinout(SD_PinNames.pin_d1, PinMap_SD_DATA1);
  pinmap_pinout(SD_PinNames.pin_d2, PinMap_SD_DATA2);
  pinmap_pinout(SD_PinNames.pin_d3, PinMap_SD_DATA3);
  pinmap_pinout(SD_PinNames.pin_cmd, PinMap_SD_CMD);
  pinmap_pinout(SD_PinNames.pin_ck, PinMap_SD_CK);
  LL_GPIO_SetPinPull(GPIOD, LL_GPIO_PIN_2, LL_GPIO_PULL_UP);
  if (hsd->Instance == SDMMC1)
    __HAL_RCC_SDMMC1_CLK_ENABLE();
}

static bool card_present() { return digitalRead(PIN_SD_DETECT) == LOW; }

static bool sd_card_init()
{
  card.setDx(PC8, PC9, PC10, PC11);
  card.setCK(PC12);
  card.setCMD(PD2);
  uint32_t t0 = millis();
  if (!card.init(PIN_SD_DETECT, LOW))
  {
    spike_logf("SDLUN: card init FAILED (%lu ms)", (unsigned long)(millis() - t0));
    return false;
  }
  HAL_SD_CardInfoTypeDef ci;
  BSP_SD_GetCardInfo(&ci);
  blockCount = ci.LogBlockNbr;
  blockSize = ci.LogBlockSize;
  spike_logf("SDLUN: card ready in %lu ms: type=%lu, %lu blocks x %lu B = %lu MiB", (unsigned long)(millis() - t0),
             (unsigned long)ci.CardType, (unsigned long)blockCount, (unsigned long)blockSize,
             (unsigned long)(((uint64_t)blockCount * blockSize) >> 20));
  return true;
}

// Called once from the device task before USB connects
void sd_lun_begin()
{
  pinMode(PIN_SD_DETECT, INPUT_PULLUP);
  if (card_present())
    cardReady = sd_card_init();
  else
    spike_logf("SDLUN: no card at boot");
}

// Card-detect handling, from the MSC test-unit-ready path (device task context)
static void sd_poll_detect()
{
  bool present = card_present();
  if (!present && cardReady)
  {
    spike_logf("SDLUN: card removed");
    card.deinit();
    cardReady = false;
  }
  else if (present && !cardReady)
  {
    spike_logf("SDLUN: card inserted");
    cardReady = sd_card_init();
    mediumChanged = cardReady;
  }
}

extern "C" uint32_t sd_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *r, uint32_t)
{
  (void)lun;
  memcpy(r->vendor_id, "GTE     ", 8);
  memcpy(r->product_id, "SD card         ", 16);
  memcpy(r->product_rev, "0.1 ", 4);
  return sizeof(scsi_inquiry_resp_t);
}

extern "C" bool sd_test_unit_ready_cb(uint8_t lun)
{
  sd_poll_detect();
  if (!cardReady)
  {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00); // medium not present
    return false;
  }
  if (mediumChanged)
  {
    mediumChanged = false;
    tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00); // not ready to ready change, medium may have changed
    return false;
  }
  return true;
}

extern "C" void sd_capacity_cb(uint8_t, uint32_t *count, uint16_t *size)
{
  *count = cardReady ? blockCount : 0;
  *size = (uint16_t)blockSize;
}

static bool wait_card_ready(uint32_t timeoutMs)
{
  uint32_t t0 = millis();
  while (BSP_SD_GetCardState() != 0) // 0 = SD_TRANSFER_OK (private to bsp_sd.c)
    if (millis() - t0 > timeoutMs)
      return false;
  return true;
}

extern "C" int32_t sd_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  if (!cardReady || offset != 0 || bufsize % blockSize)
  {
    sdErrors++;
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return -1;
  }
  uint32_t n = bufsize / blockSize;
  if (BSP_SD_ReadBlocks((uint32_t *)buffer, lba, n, 2000) != MSD_OK || !wait_card_ready(2000))
  {
    sdErrors++;
    tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x00); // unrecovered read error
    return -1;
  }
  sdReads++;
  sdBytesRead += bufsize;
  return (int32_t)bufsize;
}

extern "C" int32_t sd_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  if (!cardReady || offset != 0 || bufsize % blockSize)
  {
    sdErrors++;
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return -1;
  }
  uint32_t n = bufsize / blockSize;
  if (BSP_SD_WriteBlocks((uint32_t *)buffer, lba, n, 2000) != MSD_OK || !wait_card_ready(2000))
  {
    sdErrors++;
    tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x00); // write error
    return -1;
  }
  sdWrites++;
  sdBytesWritten += bufsize;
  return (int32_t)bufsize;
}

// Unlocked Volume: the Connected PC owns it read/write (no Logging in this build)
extern "C" bool sd_is_writable_cb(uint8_t) { return true; }
#endif
