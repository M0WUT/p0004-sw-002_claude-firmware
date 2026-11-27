// Status Values: typed, with a stable id, a unit and a display hint. The exact
// value goes to SCPI and Home Assistant; the display text (e.g. "1.2 GiB") goes
// to the web page and LCD.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gte {

enum class StatusType : uint8_t { kInteger, kFloat, kBoolean, kText };
enum class DisplayHint : uint8_t { kNone, kBytes };

constexpr size_t kMaxStatusValues = 24;

struct StatusValue {
  const char *id = nullptr;
  const char *label = nullptr;
  StatusType type = StatusType::kInteger;
  int64_t integer = 0;
  float number = 0;
  uint8_t decimals = 0;
  bool boolean = false;
  char text[48] = {};
  const char *unit = "";
  DisplayHint hint = DisplayHint::kNone;

  void Exact(char *buf,
             size_t size) const;  // full-precision value, as SCPI and HA get it
  void Display(char *buf, size_t size) const;  // human-readable, with unit
};

class StatusList {
 public:
  void Integer(const char *id, const char *label, int64_t v,
               const char *unit = "", DisplayHint hint = DisplayHint::kNone);
  void Number(const char *id, const char *label, float v, uint8_t decimals,
              const char *unit = "");
  void Boolean(const char *id, const char *label, bool v);
  void Text(const char *id, const char *label, const char *v);

  size_t count() const { return count_; }
  const StatusValue &operator[](size_t i) const { return values_[i]; }

 private:
  StatusValue *Add(const char *id, const char *label, StatusType type);
  StatusValue values_[kMaxStatusValues];
  size_t count_ = 0;
};

void FormatBytes(int64_t bytes, char *buf, size_t size);  // "1.2 GiB"

}  // namespace gte
