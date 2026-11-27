// Status Values: typed, with a stable id, a unit and a display hint. The exact value goes to
// SCPI and Home Assistant; the display text (e.g. "1.2 GiB") goes to the web page and LCD.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace gte {

enum class StatusType : uint8_t { Integer, Float, Boolean, Text };
enum class DisplayHint : uint8_t { None, Bytes };

constexpr size_t MAX_STATUS_VALUES = 24;

struct StatusValue {
    const char *id = nullptr;
    const char *label = nullptr;
    StatusType type = StatusType::Integer;
    int64_t integer = 0;
    float number = 0;
    uint8_t decimals = 0;
    bool boolean = false;
    char text[48] = {};
    const char *unit = "";
    DisplayHint hint = DisplayHint::None;

    void exact(char *buf, size_t size) const;    // full-precision value, as SCPI and HA get it
    void display(char *buf, size_t size) const;  // human-readable, with unit
};

class StatusList {
public:
    void integer(const char *id, const char *label, int64_t v, const char *unit = "", DisplayHint hint = DisplayHint::None);
    void number(const char *id, const char *label, float v, uint8_t decimals, const char *unit = "");
    void boolean(const char *id, const char *label, bool v);
    void text(const char *id, const char *label, const char *v);

    size_t count() const { return _count; }
    const StatusValue &operator[](size_t i) const { return _values[i]; }

private:
    StatusValue *add(const char *id, const char *label, StatusType type);
    StatusValue _values[MAX_STATUS_VALUES];
    size_t _count = 0;
};

void format_bytes(int64_t bytes, char *buf, size_t size);  // "1.2 GiB"

}  // namespace gte
