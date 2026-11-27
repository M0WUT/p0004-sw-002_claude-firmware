#include "Status.h"
#include <stdio.h>
#include <string.h>

namespace gte {

// newlib-nano's printf has no %lld, so 64-bit integers are formatted by hand.
static void i64_to_str(int64_t v, char *buf, size_t size)
{
    char tmp[24];
    size_t n = 0;
    uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && n < sizeof(tmp));
    size_t o = 0;
    if (v < 0 && o + 1 < size)
        buf[o++] = '-';
    while (n && o + 1 < size)
        buf[o++] = tmp[--n];
    buf[o] = 0;
}

StatusValue *StatusList::add(const char *id, const char *label, StatusType type)
{
    if (_count >= MAX_STATUS_VALUES)
        return nullptr;
    StatusValue &s = _values[_count++];
    s = StatusValue{};
    s.id = id;
    s.label = label;
    s.type = type;
    return &s;
}

void StatusList::integer(const char *id, const char *label, int64_t v, const char *unit, DisplayHint hint)
{
    if (StatusValue *s = add(id, label, StatusType::Integer)) {
        s->integer = v;
        s->unit = unit;
        s->hint = hint;
    }
}

void StatusList::number(const char *id, const char *label, float v, uint8_t decimals, const char *unit)
{
    if (StatusValue *s = add(id, label, StatusType::Float)) {
        s->number = v;
        s->decimals = decimals;
        s->unit = unit;
    }
}

void StatusList::boolean(const char *id, const char *label, bool v)
{
    if (StatusValue *s = add(id, label, StatusType::Boolean))
        s->boolean = v;
}

void StatusList::text(const char *id, const char *label, const char *v)
{
    if (StatusValue *s = add(id, label, StatusType::Text))
        snprintf(s->text, sizeof(s->text), "%s", v ? v : "");
}

void format_bytes(int64_t bytes, char *buf, size_t size)
{
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024 && u < 4) {
        v /= 1024;
        u++;
    }
    if (u == 0) {
        i64_to_str(bytes, buf, size);
        size_t n = strlen(buf);
        snprintf(buf + n, size - n, " B");
    } else
        snprintf(buf, size, "%.1f %s", v, units[u]);
}

void StatusValue::exact(char *buf, size_t size) const
{
    switch (type) {
    case StatusType::Integer: i64_to_str(integer, buf, size); break;
    case StatusType::Float: snprintf(buf, size, "%.*f", decimals, (double)number); break;
    case StatusType::Boolean: snprintf(buf, size, "%d", boolean ? 1 : 0); break;
    case StatusType::Text: snprintf(buf, size, "%s", text); break;
    }
}

void StatusValue::display(char *buf, size_t size) const
{
    if (type == StatusType::Integer && hint == DisplayHint::Bytes) {
        format_bytes(integer, buf, size);
        return;
    }
    if (type == StatusType::Boolean) {
        snprintf(buf, size, "%s", boolean ? "Yes" : "No");
        return;
    }
    exact(buf, size);
    if (unit && *unit) {
        size_t n = strlen(buf);
        snprintf(buf + n, size - n, " %s", unit);
    }
}

}  // namespace gte
