// Config Parameters: declared by each Service in declare_config(), each pointing at the place in
// the Service's config struct where its value lives. Generic code parses, range/step-checks,
// writes and saves them; the Service only adds custom rules (validate_config_field) and side
// effects (on_config_changed). Decided in "The Service contract" (issue #9).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace gte {

enum class ParamType : uint8_t { Integer, Float, Boolean, Text, Enum, IpAddress };
enum class ApplyMode : uint8_t { Live, Reset };

constexpr size_t MAX_CONFIG_PARAMS = 24;
constexpr size_t CONFIG_VALUE_TEXT = 48;   // longest display or text value

struct ConfigParam {
    const char *id = nullptr;       // stable: the web page, SCPI and Home Assistant use it
    const char *label = nullptr;
    ParamType type = ParamType::Integer;
    void *value = nullptr;          // int32_t / float / bool / char[textCapacity] / uint8_t / uint8_t[4]
    double min = 0, max = 0, step = 0;
    uint8_t decimals = 0;           // Float display
    const char *unit = "";
    ApplyMode apply = ApplyMode::Live;
    bool secret = false;            // write-only: masked everywhere
    const char *const *enumNames = nullptr;
    uint8_t enumCount = 0;
    size_t textCapacity = 0;        // Text: buffer size including the terminator

    void format(char *buf, size_t size) const;   // current value as text (secrets: real value)
    void display(char *buf, size_t size) const;  // value + unit for printing (secrets masked)
};

// A parsed, range-checked candidate value, handed to validate_config_field before it's written.
struct ConfigValue {
    ParamType type = ParamType::Integer;
    int32_t integer = 0;
    float number = 0;
    bool boolean = false;
    uint8_t enumIndex = 0;
    uint8_t ip[4] = {};
    char text[CONFIG_VALUE_TEXT] = {};
};

class ConfigList {
public:
    ConfigParam &integer(const char *id, const char *label, int32_t *v, int32_t min, int32_t max, int32_t step = 1,
                         const char *unit = "");
    ConfigParam &number(const char *id, const char *label, float *v, float min, float max, float step,
                        uint8_t decimals, const char *unit = "");
    ConfigParam &boolean(const char *id, const char *label, bool *v);
    ConfigParam &text(const char *id, const char *label, char *v, size_t capacity);
    ConfigParam &enumeration(const char *id, const char *label, uint8_t *v, const char *const *names, uint8_t count);
    ConfigParam &ip(const char *id, const char *label, uint8_t *v);

    size_t count() const { return _count; }
    const ConfigParam &operator[](size_t i) const { return _params[i]; }
    const ConfigParam *find(const char *id) const;

private:
    ConfigParam &add(const char *id, const char *label, ParamType type, void *value);
    ConfigParam _params[MAX_CONFIG_PARAMS];
    ConfigParam _overflow;
    size_t _count = 0;
};

// Modifiers for a declared parameter, e.g. reset_mode(list.integer(...)), secret(list.text(...)).
inline ConfigParam &reset_mode(ConfigParam &p) { p.apply = ApplyMode::Reset; return p; }
inline ConfigParam &secret(ConfigParam &p) { p.secret = true; return p; }

struct ConfigResult {
    bool ok = false;
    bool restartRequired = false;
    char message[96] = {};
};

bool parse_config_value(const ConfigParam &p, const char *text, ConfigValue &out, char *error, size_t errorSize);
void write_config_value(const ConfigParam &p, const ConfigValue &v);

}  // namespace gte
