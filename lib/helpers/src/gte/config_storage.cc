#include "gte/config_storage.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <semphr.h>

#include <cstdio>
#include <cstring>

#include "gte/core_errors.h"
#include "gte/crc8.h"
#include "gte/eeprom_24aa256uid.h"
#include "gte/fatal.h"
#include "gte/gte.h"

namespace gte {}  // namespace gte

// The RAM copy of the config area. C linkage keeps the symbol name simple, so
// pytest can find it in the ELF and read it over SWD.
extern "C" {
uint8_t gte_config_image[gte::ConfigStorage::kImageSize];
}

namespace gte {

namespace {
uint8_t *const g_img = gte_config_image;
uint8_t g_scratch[ConfigStorage::kAreaSize];
Eeprom24AA256UID *g_chip = nullptr;

uint16_t Le16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
void SetLe16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}

// Table helpers working on a buffer laid out like the config area.
uint8_t EntryCount(const uint8_t *b) { return b[3]; }
uint16_t TableEnd(const uint8_t *b) {
  return static_cast<uint16_t>(5 + 2 * EntryCount(b));
}
uint16_t Entry(const uint8_t *b, uint8_t id) {
  return id < EntryCount(b) ? Le16(b + 4 + 2 * id) : 0xFFFF;
}
void SealTable(uint8_t *b) {
  b[4 + 2 * EntryCount(b)] = Crc8(b, 4 + 2 * EntryCount(b));
}
uint16_t UsedEnd(const uint8_t *b) {
  uint16_t end = TableEnd(b);
  for (uint8_t i = 0; i < EntryCount(b); i++) {
    uint16_t base = Entry(b, i);
    if (base != 0xFFFF && base + 2 < ConfigStorage::kAreaSize) {
      uint16_t e = base + Le16(b + base);
      if (e > end && e <= ConfigStorage::kAreaSize) end = e;
    }
  }
  return end;
}
// Shift [from, end) by delta bytes and move every block address >= from with
// it.
void Shift(uint8_t *b, uint16_t from, int delta) {
  uint16_t end = UsedEnd(b);
  memmove(b + from + delta, b + from, end - from);
  for (uint8_t i = 0; i < EntryCount(b); i++) {
    uint16_t base = Entry(b, i);
    if (base != 0xFFFF && base >= from)
      SetLe16(b + 4 + 2 * i, static_cast<uint16_t>(base + delta));
  }
}
}  // namespace

bool ConfigStorage::IsDebug() const { return !chip_; }

void ConfigStorage::Lock() {
  if (!mutex_) mutex_ = xSemaphoreCreateRecursiveMutex();
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(mutex_),
                            portMAX_DELAY);
}

void ConfigStorage::Unlock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(mutex_));
}

void ConfigStorage::Begin(I2CBus *bus) {
  g_chip = new Eeprom24AA256UID(bus, Eeprom24AA256UID::kAddress);
  if (!g_chip->Detect()) Fatal("ConfigStorage", errors::kEepromNotDetected);
#ifdef DEBUG_EEPROM
  chip_ = false;
  memset(g_img, 0xFF, kImageSize);
  Format();
  HelperLog("ConfigStorage", LogLevel::kInfo,
            "DEBUG_EEPROM: config is in RAM only, lost at every reset");
#else
  chip_ = true;
  if (!g_chip->ReadBytes(0, g_img, kImageSize))
    Fatal("ConfigStorage", errors::kI2cReadFailed, "config EEPROM");
#endif
}

TableState ConfigStorage::GetTableState() {
  if (g_img[0] == 0xFF && g_img[1] == 0xFF && g_img[2] == 0xFF)
    return TableState::kBlank;
  if (memcmp(g_img, "LID", 3) != 0 || TableEnd(g_img) > kAreaSize)
    return TableState::kCorrupt;
  if (Crc8(g_img, 4 + 2 * EntryCount(g_img)) !=
      g_img[4 + 2 * EntryCount(g_img)])
    return TableState::kCorrupt;
  return TableState::kOk;
}

void ConfigStorage::Put(uint16_t addr, const uint8_t *src, size_t len) {
  // Write only the runs of bytes that differ from the copy.
  size_t i = 0;
  while (i < len) {
    if (g_img[addr + i] == src[i]) {
      i++;
      continue;
    }
    size_t j = i;
    while (j < len && g_img[addr + j] != src[j]) j++;
    if (chip_ &&
        !g_chip->WriteBytes(static_cast<uint16_t>(addr + i), src + i, j - i))
      Fatal("ConfigStorage", errors::kI2cWriteFailed, "config EEPROM");
    memcpy(g_img + addr + i, src + i, j - i);
    i = j;
  }
}

void ConfigStorage::Format() {
  Lock();
  memcpy(g_scratch, g_img, kAreaSize);
  memcpy(g_scratch, "LID", 3);
  g_scratch[3] = 0;
  SealTable(g_scratch);
  Put(0, g_scratch, 5);
  Unlock();
}

LoadResult ConfigStorage::Load(uint8_t id, uint8_t *version,
                               const uint8_t **data, uint16_t *len) {
  uint16_t base = Entry(g_img, id);
  if (base == 0xFFFF) return LoadResult::kMissing;
  if (base + 4 > kAreaSize) return LoadResult::kCrcFailed;
  uint16_t total = Le16(g_img + base);
  if (total < 4 || base + total > kAreaSize ||
      Crc8(g_img + base, total - 1) != g_img[base + total - 1])
    return LoadResult::kCrcFailed;
  *version = g_img[base + 2];
  *data = g_img + base + 3;
  *len = total - 4;
  return LoadResult::kOk;
}

void ConfigStorage::Save(uint8_t id, const char *name, uint8_t version,
                         const uint8_t *data, uint16_t len) {
  Lock();
  uint8_t *b = g_scratch;
  memcpy(b, g_img, kAreaSize);
  const uint16_t total = len + 4;
  if (id >= EntryCount(b)) {  // grow the table, shifting every block up
    uint8_t n = EntryCount(b), grow = static_cast<uint8_t>(id + 1 - n);
    if (UsedEnd(b) + 2 * grow > kAreaSize)
      Fatal("ConfigStorage", errors::kEepromFull, name);
    Shift(b, TableEnd(b), 2 * grow);
    b[3] = static_cast<uint8_t>(id + 1);
    for (uint8_t i = n; i <= id; i++) SetLe16(b + 4 + 2 * i, 0xFFFF);
  }
  uint16_t base = Entry(b, id);
  if (base != 0xFFFF && base + 2 <= kAreaSize) {
    uint16_t old = Le16(b + base);
    if (old != total) {  // resize: shift everything after this block
      if (UsedEnd(b) - old + total > kAreaSize)
        Fatal("ConfigStorage", errors::kEepromFull, name);
      if (base + old < UsedEnd(b))
        Shift(b, base + old, static_cast<int>(total) - static_cast<int>(old));
    }
  } else {
    base = UsedEnd(b);
  }
  if (base + total > kAreaSize)
    Fatal("ConfigStorage", errors::kEepromFull, name);
  SetLe16(b + 4 + 2 * id, base);
  SetLe16(b + base, total);
  b[base + 2] = version;
  memcpy(b + base + 3, data, len);
  b[base + total - 1] = Crc8(b + base, total - 1);
  SealTable(b);
  Put(0, b, UsedEnd(b));
  Unlock();
}

uint16_t ConfigStorage::BlockAddress(uint8_t id) const {
  return Entry(g_img, id);
}

void ConfigStorage::Poke(uint16_t addr, const uint8_t *data, size_t len) {
  Lock();
  Put(addr, data, len);
  Unlock();
}

bool ConfigStorage::ReadMac(uint8_t mac[6]) {
  return g_chip && g_chip->ReadMac(mac);
}

static void BuildRecord(uint8_t *page, RecordKind kind, uint16_t code,
                        const char *source, const char *text) {
  memset(page, 0, ConfigStorage::kRecordSize);
  page[0] = 'F';
  page[1] = 'R';
  page[2] = static_cast<uint8_t>(kind);
  SetLe16(page + 3, code);
  strncpy(reinterpret_cast<char *>(page) + 5, source ? source : "", 16);
  strncpy(reinterpret_cast<char *>(page) + 21, text ? text : "", 42);
  page[63] = Crc8(page, 63);
}

bool ConfigStorage::ReadRecord(ResetRecord *r) {
  const uint8_t *p = g_img + kRecordAddr;
  if (p[0] != 'F' || p[1] != 'R' || Crc8(p, 63) != p[63]) return false;
  r->kind = static_cast<RecordKind>(p[2]);
  r->code = Le16(p + 3);
  memcpy(r->source, p + 5, 16);
  r->source[16] = 0;
  memcpy(r->text, p + 21, 42);
  r->text[42] = 0;
  return true;
}

void ConfigStorage::ClearRecord() {
  Lock();
  uint8_t ff = 0xFF;
  Put(kRecordAddr, &ff, 1);
  Unlock();
}

void ConfigStorage::WriteRecord(RecordKind kind, uint16_t code,
                                const char *source, const char *text) {
  uint8_t page[kRecordSize];
  BuildRecord(page, kind, code, source, text);
  Lock();
  Put(kRecordAddr, page, kRecordSize);
  Unlock();
}

void ConfigStorage::WriteRecordPolled(RecordKind kind, uint16_t code,
                                      const char *source, const char *text) {
  uint8_t page[kRecordSize];
  BuildRecord(page, kind, code, source, text);
  if (chip_) Eeprom24AA256UID::PolledWritePage(kRecordAddr, page, kRecordSize);
  memcpy(g_img + kRecordAddr, page, kRecordSize);
}

}  // namespace gte
