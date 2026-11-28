// Config Parameters: declared by each Service in DeclareConfig(), each pointing
// at the place in the Service's config struct where its value lives. Generic
// code parses, range/step-checks, writes and saves them; the Service only adds
// custom rules (validate_config_field) and side effects (on_config_changed).
// Decided in "The Service contract" (issue #9).
#pragma once
#include <cstddef>
#include <cstdint>

namespace gte {

enum class ParamType : uint8_t {
  kInteger,
  kFloat,
  kBoolean,
  kText,
  kEnum,
  kIpAddress
};
enum class ApplyMode : uint8_t { kLive, kReset };

constexpr size_t kMaxConfigParams = 24;
constexpr size_t kConfigValueText = 48;  // longest display or text value

struct ConfigParam {
  const char *id =
      nullptr;  // stable: the web page, SCPI and Home Assistant use it
  const char *label = nullptr;
  ParamType type = ParamType::kInteger;
  void *value = nullptr;  // int32_t / float / bool / char[text_capacity] /
                          // uint8_t / uint8_t[4]
  double min = 0, max = 0, step = 0;
  uint8_t decimals = 0;  // Float display
  const char *unit = "";
  ApplyMode apply = ApplyMode::kLive;
  bool secret = false;  // write-only: masked everywhere
  const char *const *enum_names = nullptr;
  uint8_t enum_count = 0;
  size_t text_capacity = 0;  // Text: buffer size including the terminator

  void Format(char *buf, size_t size)
      const;  // current value as text (secrets: real value)
  void Display(char *buf, size_t size)
      const;  // value + unit for printing (secrets masked)
};

// A parsed, range-checked candidate value, handed to validate_config_field
// before it's written.
struct ConfigValue {
  ParamType type = ParamType::kInteger;
  int32_t integer = 0;
  float number = 0;
  bool boolean = false;
  uint8_t enum_index = 0;
  uint8_t ip[4] = {};
  char text[kConfigValueText] = {};
};

class ConfigList {
 public:
  ConfigParam *Integer(const char *id, const char *label, int32_t *v,
                       int32_t min, int32_t max, int32_t step = 1,
                       const char *unit = "");
  ConfigParam *Number(const char *id, const char *label, float *v, float min,
                      float max, float step, uint8_t decimals,
                      const char *unit = "");
  ConfigParam *Boolean(const char *id, const char *label, bool *v);
  ConfigParam *Text(const char *id, const char *label, char *v,
                    size_t capacity);
  ConfigParam *Enumeration(const char *id, const char *label, uint8_t *v,
                           const char *const *names, uint8_t count);
  ConfigParam *Ip(const char *id, const char *label, uint8_t *v);

  size_t count() const { return count_; }
  const ConfigParam &operator[](size_t i) const { return params_[i]; }
  const ConfigParam *Find(const char *id) const;

 private:
  ConfigParam *Add(const char *id, const char *label, ParamType type,
                   void *value);
  ConfigParam params_[kMaxConfigParams];
  ConfigParam overflow_;
  size_t count_ = 0;
};

// Modifiers for a declared parameter, e.g. ResetMode(list->Integer(...)),
// Secret(list->Text(...)).
inline ConfigParam *ResetMode(ConfigParam *p) {
  p->apply = ApplyMode::kReset;
  return p;
}
inline ConfigParam *Secret(ConfigParam *p) {
  p->secret = true;
  return p;
}

// Why a value was refused, for interfaces that report errors by number (SCPI).
enum class ConfigError : uint8_t {
  kNone,
  kUnknown,     // no such parameter
  kType,        // not a value of the parameter's type
  kRange,       // outside min-max
  kStep,        // not on the step
  kTooLong,     // text longer than its capacity
  kNotAllowed,  // not one of an enumeration's names
  kRefused,     // the Service's own rule (validate_config_field)
};

struct ConfigResult {
  bool ok = false;
  ConfigError error = ConfigError::kNone;
  bool restart_required = false;
  char message[96] = {};
};

bool ParseConfigValue(const ConfigParam &p, const char *text, ConfigValue *out,
                      char *error, size_t error_size,
                      ConfigError *kind = nullptr);
void WriteConfigValue(const ConfigParam &p, const ConfigValue &v);

}  // namespace gte
