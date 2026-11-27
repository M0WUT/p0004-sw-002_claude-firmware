#include "gte/status.h"

#include <cstdio>
#include <cstring>

namespace gte {

// newlib-nano's printf has no %lld, so 64-bit integers are formatted by hand.
static void I64ToStr(int64_t v, char *buf, size_t size) {
  char tmp[24];
  size_t n = 0;
  uint64_t u =
      v < 0 ? static_cast<uint64_t>(-(v + 1)) + 1 : static_cast<uint64_t>(v);
  do {
    tmp[n++] = static_cast<char>('0' + u % 10);
    u /= 10;
  } while (u && n < sizeof(tmp));
  size_t o = 0;
  if (v < 0 && o + 1 < size) buf[o++] = '-';
  while (n && o + 1 < size) buf[o++] = tmp[--n];
  buf[o] = 0;
}

StatusValue *StatusList::Add(const char *id, const char *label,
                             StatusType type) {
  if (count_ >= kMaxStatusValues) return nullptr;
  StatusValue &s = values_[count_++];
  s = StatusValue{};
  s.id = id;
  s.label = label;
  s.type = type;
  return &s;
}

void StatusList::Integer(const char *id, const char *label, int64_t v,
                         const char *unit, DisplayHint hint) {
  if (StatusValue *s = Add(id, label, StatusType::kInteger)) {
    s->integer = v;
    s->unit = unit;
    s->hint = hint;
  }
}

void StatusList::Number(const char *id, const char *label, float v,
                        uint8_t decimals, const char *unit) {
  if (StatusValue *s = Add(id, label, StatusType::kFloat)) {
    s->number = v;
    s->decimals = decimals;
    s->unit = unit;
  }
}

void StatusList::Boolean(const char *id, const char *label, bool v) {
  if (StatusValue *s = Add(id, label, StatusType::kBoolean)) s->boolean = v;
}

void StatusList::Text(const char *id, const char *label, const char *v) {
  if (StatusValue *s = Add(id, label, StatusType::kText))
    snprintf(s->text, sizeof(s->text), "%s", v ? v : "");
}

void FormatBytes(int64_t bytes, char *buf, size_t size) {
  static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double v = static_cast<double>(bytes);
  int u = 0;
  while (v >= 1024 && u < 4) {
    v /= 1024;
    u++;
  }
  if (u == 0) {
    I64ToStr(bytes, buf, size);
    size_t n = strlen(buf);
    snprintf(buf + n, size - n, " B");
  } else {
    snprintf(buf, size, "%.1f %s", v, units[u]);
  }
}

void StatusValue::Exact(char *buf, size_t size) const {
  switch (type) {
    case StatusType::kInteger:
      I64ToStr(integer, buf, size);
      break;
    case StatusType::kFloat:
      snprintf(buf, size, "%.*f", decimals, static_cast<double>(number));
      break;
    case StatusType::kBoolean:
      snprintf(buf, size, "%d", boolean ? 1 : 0);
      break;
    case StatusType::kText:
      snprintf(buf, size, "%s", text);
      break;
  }
}

void StatusValue::Display(char *buf, size_t size) const {
  if (type == StatusType::kInteger && hint == DisplayHint::kBytes) {
    FormatBytes(integer, buf, size);
    return;
  }
  if (type == StatusType::kBoolean) {
    snprintf(buf, size, "%s", boolean ? "Yes" : "No");
    return;
  }
  Exact(buf, size);
  if (unit && *unit) {
    size_t n = strlen(buf);
    snprintf(buf + n, size - n, " %s", unit);
  }
}

}  // namespace gte
