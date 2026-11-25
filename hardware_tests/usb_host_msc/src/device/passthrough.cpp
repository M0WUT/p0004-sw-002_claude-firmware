// Stage 3 (SPIKE_PASSTHROUGH=1): device MSC with two LUNs.
//   LUN 0 = the 8 KB RAM disk from the vendored TinyUSB example (msc_disk.c, callbacks renamed ramdisk_*)
//   LUN 1 = the USB stick on the host port, passed through READ-ONLY using TinyUSB async MSC:
//           device read10 -> TUD_MSC_RET_ASYNC -> tuh_msc_read10 into the device buffer -> tud_msc_async_io_done
#include "tusb_config.h"
#if SPIKE_DEVICE && SPIKE_PASSTHROUGH
#include <Arduino.h>
#include "tusb.h"

extern "C" {
uint32_t ramdisk_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp, uint32_t bufsize);
bool ramdisk_test_unit_ready_cb(uint8_t lun);
void ramdisk_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size);
bool ramdisk_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject);
int32_t ramdisk_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize);
bool ramdisk_is_writable_cb(uint8_t lun);
int32_t ramdisk_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize);
int32_t ramdisk_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize);
}

// Set by main.cpp's host mount/unmount callbacks
extern volatile uint8_t mscDevAddr;
void spike_logf(const char *fmt, ...);

static constexpr uint8_t LUN_RAM = 0, LUN_STICK = 1;
static volatile uint32_t ptPending = 0;
volatile uint32_t ptReads = 0, ptErrors = 0, ptBytes = 0, ptBusy = 0;

static uint8_t stick() { return (mscDevAddr && tuh_msc_mounted(mscDevAddr)) ? mscDevAddr : 0; }

extern "C" uint8_t tud_msc_get_maxlun_cb(void) { return 2; }

extern "C" uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *r, uint32_t bufsize)
{
  if (lun == LUN_RAM)
    return ramdisk_inquiry2_cb(lun, r, bufsize);
  memcpy(r->vendor_id, "GTE     ", 8);
  memcpy(r->product_id, "USB passthrough ", 16);
  memcpy(r->product_rev, "0.1 ", 4);
  return sizeof(scsi_inquiry_resp_t);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  if (lun == LUN_RAM)
    return ramdisk_test_unit_ready_cb(lun);
  if (!stick())
    return tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00) && false; // medium not present
  return true;
}

extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
  if (lun == LUN_RAM)
    return ramdisk_capacity_cb(lun, block_count, block_size);
  uint8_t a = stick();
  *block_count = a ? tuh_msc_get_block_count(a, 0) : 0;
  *block_size = a ? (uint16_t)tuh_msc_get_block_size(a, 0) : 512;
}

extern "C" bool tud_msc_start_stop_cb(uint8_t lun, uint8_t pc, bool start, bool load_eject)
{
  if (lun == LUN_RAM)
    return ramdisk_start_stop_cb(lun, pc, start, load_eject);
  return true;
}

static bool pt_read_done(uint8_t, tuh_msc_complete_data_t const *cb)
{
  bool ok = cb->csw->status == 0;
  if (ok)
    ptBytes += ptPending;
  else
    ptErrors++;
  // Runs in the host task (tuh_task), not an ISR
  tud_msc_async_io_done(ok ? (int32_t)ptPending : -1, false);
  return true;
}

extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  if (lun == LUN_RAM)
    return ramdisk_read10_cb(lun, lba, offset, buffer, bufsize);
  uint8_t a = stick();
  uint32_t bs = a ? tuh_msc_get_block_size(a, 0) : 0;
  if (!a || offset != 0 || bs == 0 || bufsize % bs)
  {
    ptErrors++;
    return -1;
  }
  ptPending = bufsize;
  ptReads++;
  if (!tuh_msc_read10(a, 0, buffer, lba, (uint16_t)(bufsize / bs), pt_read_done, 0))
  {
    ptBusy++;
    return 0; // TUD_MSC_RET_BUSY: TinyUSB retries
  }
  return TUD_MSC_RET_ASYNC;
}

extern "C" bool tud_msc_is_writable_cb(uint8_t lun)
{
  if (lun == LUN_RAM)
    return ramdisk_is_writable_cb(lun);
  return false; // read-only pass-through: never modify the user's stick
}

extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  if (lun == LUN_RAM)
    return ramdisk_write10_cb(lun, lba, offset, buffer, bufsize);
  tud_msc_set_sense(lun, SCSI_SENSE_DATA_PROTECT, 0x27, 0x00); // write protected
  return -1;
}

extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  return ramdisk_scsi_cb(lun, scsi_cmd, buffer, bufsize);
}
#endif
