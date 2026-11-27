// Config storage in the 24AA256UID, in sw-001's format (docs/eeprom.md, issue
// #12):
//   "LID", entry count n, n x 16-bit LE block addresses indexed by ServiceId
//   (0xFFFF = none), CRC8; each block: 16-bit LE length (incl. its 4 overhead
//   bytes), version, data, CRC8.
// The table grows, and blocks change size, by shifting the blocks after them.
//
// A copy of the config area is kept in RAM (gte_config_image, which pytest
// reads over SWD).
//  - Real EEPROM: the copy is loaded at boot, and each change writes only the
//    bytes that differ.
//  - DEBUG_EEPROM: the copy IS the store, with no chip writes. It starts as a
//    fresh empty table and is lost at every reset or power cycle.
//
// The config area is 0x0000-0x5FBF; the page at 0x5FC0 holds the last Fatal
// Error / watchdog record. Both stay below 0x6000, clear of the chip's factory
// area (ID, EUI-48 MAC).
#pragma once
#include <cstddef>
#include <cstdint>

namespace gte {

class I2CBus;

enum class TableState : uint8_t { kOk, kBlank, kCorrupt };
enum class LoadResult : uint8_t { kMissing, kOk, kCrcFailed };
enum class RecordKind : uint8_t { kNone = 0, kFatal = 1, kWatchdog = 2 };

struct ResetRecord {
  RecordKind kind;
  uint16_t code;
  char source[17];
  char text[43];
};

class ConfigStorage {
 public:
  static constexpr uint16_t kAreaSize = 0x5FC0;
  static constexpr uint16_t kRecordAddr = 0x5FC0;
  static constexpr uint16_t kRecordSize = 64;
  static constexpr uint16_t kImageSize = kRecordAddr + kRecordSize;

  void Begin(I2CBus *bus);  // finds the chip (Fatal Error if absent) and loads
                            // the copy
  bool IsDebug() const;
  TableState GetTableState();
  void Format();  // empty table: Factory Reset, blank or corrupt table
  LoadResult Load(uint8_t id, uint8_t *version, const uint8_t **data,
                  uint16_t *len);
  void Save(uint8_t id, const char *name, uint8_t version, const uint8_t *data,
            uint16_t len);
  bool ReadMac(uint8_t mac[6]);

  // For tests and engineering use: where a Service's block starts (0xFFFF if
  // it has none), and a raw write that bypasses every check.
  uint16_t BlockAddress(uint8_t id) const;
  void Poke(uint16_t addr, const uint8_t *data, size_t len);

  bool ReadRecord(ResetRecord *r);
  void ClearRecord();
  void WriteRecord(RecordKind kind, uint16_t code, const char *source,
                   const char *text);
  // Fatal loop only (interrupts masked): polled I2C, or the RAM copy under
  // DEBUG_EEPROM.
  void WriteRecordPolled(RecordKind kind, uint16_t code, const char *source,
                         const char *text);

 private:
  void Put(uint16_t addr, const uint8_t *src,
           size_t len);  // diff-write into the copy and chip
  void Lock();
  void Unlock();
  void *mutex_ = nullptr;
  bool chip_ = false;
};

}  // namespace gte
