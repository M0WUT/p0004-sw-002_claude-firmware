#include "gte/config.h"

#include <strings.h>

#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gte {

ConfigParam *ConfigList::Add(const char *id, const char *label, ParamType type,
                             void *value) {
  if (count_ >= kMaxConfigParams)
    return &overflow_;  // ignored; ServiceHelper reports it at startup
  ConfigParam *p = &params_[count_++];
  *p = ConfigParam{};
  p->id = id;
  p->label = label;
  p->type = type;
  p->value = value;
  return p;
}

ConfigParam *ConfigList::Integer(const char *id, const char *label, int32_t *v,
                                 int32_t min, int32_t max, int32_t step,
                                 const char *unit) {
  ConfigParam *p = Add(id, label, ParamType::kInteger, v);
  p->min = min;
  p->max = max;
  p->step = step;
  p->unit = unit;
  return p;
}

ConfigParam *ConfigList::Number(const char *id, const char *label, float *v,
                                float min, float max, float step,
                                uint8_t decimals, const char *unit) {
  ConfigParam *p = Add(id, label, ParamType::kFloat, v);
  p->min = min;
  p->max = max;
  p->step = step;
  p->decimals = decimals;
  p->unit = unit;
  return p;
}

ConfigParam *ConfigList::Boolean(const char *id, const char *label, bool *v) {
  return Add(id, label, ParamType::kBoolean, v);
}

ConfigParam *ConfigList::Text(const char *id, const char *label, char *v,
                              size_t capacity) {
  ConfigParam *p = Add(id, label, ParamType::kText, v);
  p->text_capacity = capacity;
  return p;
}

ConfigParam *ConfigList::Enumeration(const char *id, const char *label,
                                     uint8_t *v, const char *const *names,
                                     uint8_t count) {
  ConfigParam *p = Add(id, label, ParamType::kEnum, v);
  p->enum_names = names;
  p->enum_count = count;
  return p;
}

ConfigParam *ConfigList::Ip(const char *id, const char *label, uint8_t *v) {
  return Add(id, label, ParamType::kIpAddress, v);
}

const ConfigParam *ConfigList::Find(const char *id) const {
  for (size_t i = 0; i < count_; i++)
    if (!strcmp(params_[i].id, id)) return &params_[i];
  return nullptr;
}

void ConfigParam::Format(char *buf, size_t size) const {
  switch (type) {
    case ParamType::kInteger:
      snprintf(buf, size, "%" PRId32, *static_cast<int32_t *>(value));
      break;
    case ParamType::kFloat:
      snprintf(buf, size, "%.*f", decimals,
               static_cast<double>(*static_cast<float *>(value)));
      break;
    case ParamType::kBoolean:
      snprintf(buf, size, "%s", *static_cast<bool *>(value) ? "ON" : "OFF");
      break;
    case ParamType::kText:
      snprintf(buf, size, "%s", static_cast<const char *>(value));
      break;
    case ParamType::kEnum: {
      uint8_t i = *static_cast<uint8_t *>(value);
      snprintf(buf, size, "%s", i < enum_count ? enum_names[i] : "?");
      break;
    }
    case ParamType::kIpAddress: {
      const uint8_t *a = static_cast<const uint8_t *>(value);
      snprintf(buf, size, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
      break;
    }
  }
}

void ConfigParam::Display(char *buf, size_t size) const {
  if (secret) {
    snprintf(buf, size, "****");
    return;
  }
  Format(buf, size);
  if (unit && *unit) {
    size_t n = strlen(buf);
    snprintf(buf + n, size - n, " %s", unit);
  }
}

static bool OnStep(double x, double min, double step) {
  if (step <= 0) return true;
  double k = (x - min) / step;
  return fabs(k - round(k)) <= 1e-6 * fmax(1.0, fabs(k));
}

bool ParseConfigValue(const ConfigParam &p, const char *text, ConfigValue *out,
                      char *error, size_t error_size, ConfigError *kind) {
  ConfigError ignored;
  if (!kind) kind = &ignored;
  *kind = ConfigError::kType;
  *out = ConfigValue{};
  out->type = p.type;
  if (!text) {
    snprintf(error, error_size, "Missing value");
    return false;
  }
  switch (p.type) {
    case ParamType::kInteger: {
      char *end = nullptr;
      errno = 0;
      int64_t n = strtoll(text, &end, 10);
      if (!*text || *end || errno == ERANGE) {
        snprintf(error, error_size, "Not an integer");
        return false;
      }
      if (n < p.min || n > p.max) {
        *kind = ConfigError::kRange;
        snprintf(error, error_size, "Out of range %.0f to %.0f", p.min, p.max);
        return false;
      }
      if (!OnStep(static_cast<double>(n), p.min, p.step)) {
        *kind = ConfigError::kStep;
        snprintf(error, error_size, "Must be a multiple of %.0f from %.0f",
                 p.step, p.min);
        return false;
      }
      out->integer = static_cast<int32_t>(n);
      return true;
    }
    case ParamType::kFloat: {
      char *end = nullptr;
      errno = 0;
      double x = strtod(text, &end);
      if (!*text || *end || errno == ERANGE || !std::isfinite(x)) {
        snprintf(error, error_size, "Not a number");
        return false;
      }
      if (x < p.min || x > p.max) {
        *kind = ConfigError::kRange;
        snprintf(error, error_size, "Out of range %.*f to %.*f", p.decimals,
                 p.min, p.decimals, p.max);
        return false;
      }
      if (!OnStep(x, p.min, p.step)) {
        *kind = ConfigError::kStep;
        snprintf(error, error_size, "Must be a multiple of %g from %g", p.step,
                 p.min);
        return false;
      }
      out->number = static_cast<float>(x);
      return true;
    }
    case ParamType::kBoolean:
      if (!strcasecmp(text, "ON") || !strcmp(text, "1") ||
          !strcasecmp(text, "true")) {
        out->boolean = true;
        return true;
      }
      if (!strcasecmp(text, "OFF") || !strcmp(text, "0") ||
          !strcasecmp(text, "false")) {
        out->boolean = false;
        return true;
      }
      snprintf(error, error_size, "Not ON or OFF");
      return false;
    case ParamType::kText:
      if (strlen(text) + 1 > p.text_capacity ||
          strlen(text) >= sizeof(out->text)) {
        *kind = ConfigError::kTooLong;
        snprintf(error, error_size, "Longer than %u characters",
                 static_cast<unsigned>(p.text_capacity - 1));
        return false;
      }
      snprintf(out->text, sizeof(out->text), "%s", text);
      return true;
    case ParamType::kEnum:
      for (uint8_t i = 0; i < p.enum_count; i++)
        if (!strcasecmp(text, p.enum_names[i])) {
          out->enum_index = i;
          return true;
        }
      *kind = ConfigError::kNotAllowed;
      snprintf(error, error_size, "Not one of the allowed values");
      return false;
    case ParamType::kIpAddress: {
      unsigned a, b, c, d;
      char extra;
      if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4 ||
          a > 255 || b > 255 || c > 255 || d > 255) {
        snprintf(error, error_size, "Not an IPv4 address");
        return false;
      }
      out->ip[0] = a;
      out->ip[1] = b;
      out->ip[2] = c;
      out->ip[3] = d;
      return true;
    }
  }
  snprintf(error, error_size, "Unknown type");
  return false;
}

void WriteConfigValue(const ConfigParam &p, const ConfigValue &v) {
  switch (p.type) {
    case ParamType::kInteger:
      *static_cast<int32_t *>(p.value) = v.integer;
      break;
    case ParamType::kFloat:
      *static_cast<float *>(p.value) = v.number;
      break;
    case ParamType::kBoolean:
      *static_cast<bool *>(p.value) = v.boolean;
      break;
    case ParamType::kText:
      snprintf(static_cast<char *>(p.value), p.text_capacity, "%s", v.text);
      break;
    case ParamType::kEnum:
      *static_cast<uint8_t *>(p.value) = v.enum_index;
      break;
    case ParamType::kIpAddress:
      memcpy(p.value, v.ip, 4);
      break;
  }
}

}  // namespace gte
