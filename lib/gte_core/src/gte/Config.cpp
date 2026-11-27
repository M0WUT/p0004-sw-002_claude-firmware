#include "Config.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace gte {

ConfigParam &ConfigList::add(const char *id, const char *label, ParamType type, void *value)
{
    if (_count >= MAX_CONFIG_PARAMS)
        return _overflow;  // ignored; ServiceHelper reports it at startup
    ConfigParam &p = _params[_count++];
    p = ConfigParam{};
    p.id = id;
    p.label = label;
    p.type = type;
    p.value = value;
    return p;
}

ConfigParam &ConfigList::integer(const char *id, const char *label, int32_t *v, int32_t min, int32_t max, int32_t step,
                                 const char *unit)
{
    ConfigParam &p = add(id, label, ParamType::Integer, v);
    p.min = min; p.max = max; p.step = step; p.unit = unit;
    return p;
}

ConfigParam &ConfigList::number(const char *id, const char *label, float *v, float min, float max, float step,
                                uint8_t decimals, const char *unit)
{
    ConfigParam &p = add(id, label, ParamType::Float, v);
    p.min = min; p.max = max; p.step = step; p.decimals = decimals; p.unit = unit;
    return p;
}

ConfigParam &ConfigList::boolean(const char *id, const char *label, bool *v) { return add(id, label, ParamType::Boolean, v); }

ConfigParam &ConfigList::text(const char *id, const char *label, char *v, size_t capacity)
{
    ConfigParam &p = add(id, label, ParamType::Text, v);
    p.textCapacity = capacity;
    return p;
}

ConfigParam &ConfigList::enumeration(const char *id, const char *label, uint8_t *v, const char *const *names, uint8_t count)
{
    ConfigParam &p = add(id, label, ParamType::Enum, v);
    p.enumNames = names;
    p.enumCount = count;
    return p;
}

ConfigParam &ConfigList::ip(const char *id, const char *label, uint8_t *v) { return add(id, label, ParamType::IpAddress, v); }

const ConfigParam *ConfigList::find(const char *id) const
{
    for (size_t i = 0; i < _count; i++)
        if (!strcmp(_params[i].id, id))
            return &_params[i];
    return nullptr;
}

void ConfigParam::format(char *buf, size_t size) const
{
    switch (type) {
    case ParamType::Integer: snprintf(buf, size, "%ld", (long)*(int32_t *)value); break;
    case ParamType::Float: snprintf(buf, size, "%.*f", decimals, (double)*(float *)value); break;
    case ParamType::Boolean: snprintf(buf, size, "%s", *(bool *)value ? "ON" : "OFF"); break;
    case ParamType::Text: snprintf(buf, size, "%s", (const char *)value); break;
    case ParamType::Enum: {
        uint8_t i = *(uint8_t *)value;
        snprintf(buf, size, "%s", i < enumCount ? enumNames[i] : "?");
        break;
    }
    case ParamType::IpAddress: {
        const uint8_t *a = (const uint8_t *)value;
        snprintf(buf, size, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
        break;
    }
    }
}

void ConfigParam::display(char *buf, size_t size) const
{
    if (secret) {
        snprintf(buf, size, "****");
        return;
    }
    format(buf, size);
    if (unit && *unit) {
        size_t n = strlen(buf);
        snprintf(buf + n, size - n, " %s", unit);
    }
}

static bool on_step(double x, double min, double step)
{
    if (step <= 0)
        return true;
    double k = (x - min) / step;
    return fabs(k - round(k)) <= 1e-6 * fmax(1.0, fabs(k));
}

bool parse_config_value(const ConfigParam &p, const char *text, ConfigValue &out, char *error, size_t errorSize)
{
    out = ConfigValue{};
    out.type = p.type;
    if (!text) {
        snprintf(error, errorSize, "Missing value");
        return false;
    }
    switch (p.type) {
    case ParamType::Integer: {
        char *end = nullptr;
        errno = 0;
        long long n = strtoll(text, &end, 10);
        if (!*text || *end || errno == ERANGE) {
            snprintf(error, errorSize, "Not an integer");
            return false;
        }
        if (n < p.min || n > p.max) {
            snprintf(error, errorSize, "Out of range %.0f to %.0f", p.min, p.max);
            return false;
        }
        if (!on_step((double)n, p.min, p.step)) {
            snprintf(error, errorSize, "Must be a multiple of %.0f from %.0f", p.step, p.min);
            return false;
        }
        out.integer = (int32_t)n;
        return true;
    }
    case ParamType::Float: {
        char *end = nullptr;
        errno = 0;
        double x = strtod(text, &end);
        if (!*text || *end || errno == ERANGE || !isfinite(x)) {
            snprintf(error, errorSize, "Not a number");
            return false;
        }
        if (x < p.min || x > p.max) {
            snprintf(error, errorSize, "Out of range %.*f to %.*f", p.decimals, p.min, p.decimals, p.max);
            return false;
        }
        if (!on_step(x, p.min, p.step)) {
            snprintf(error, errorSize, "Must be a multiple of %g from %g", p.step, p.min);
            return false;
        }
        out.number = (float)x;
        return true;
    }
    case ParamType::Boolean:
        if (!strcasecmp(text, "ON") || !strcmp(text, "1") || !strcasecmp(text, "true")) {
            out.boolean = true;
            return true;
        }
        if (!strcasecmp(text, "OFF") || !strcmp(text, "0") || !strcasecmp(text, "false")) {
            out.boolean = false;
            return true;
        }
        snprintf(error, errorSize, "Not ON or OFF");
        return false;
    case ParamType::Text:
        if (strlen(text) + 1 > p.textCapacity || strlen(text) >= sizeof(out.text)) {
            snprintf(error, errorSize, "Longer than %u characters", (unsigned)(p.textCapacity - 1));
            return false;
        }
        snprintf(out.text, sizeof(out.text), "%s", text);
        return true;
    case ParamType::Enum:
        for (uint8_t i = 0; i < p.enumCount; i++)
            if (!strcasecmp(text, p.enumNames[i])) {
                out.enumIndex = i;
                return true;
            }
        snprintf(error, errorSize, "Not one of the allowed values");
        return false;
    case ParamType::IpAddress: {
        unsigned a, b, c, d;
        char extra;
        if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4 || a > 255 || b > 255 || c > 255 || d > 255) {
            snprintf(error, errorSize, "Not an IPv4 address");
            return false;
        }
        out.ip[0] = a; out.ip[1] = b; out.ip[2] = c; out.ip[3] = d;
        return true;
    }
    }
    snprintf(error, errorSize, "Unknown type");
    return false;
}

void write_config_value(const ConfigParam &p, const ConfigValue &v)
{
    switch (p.type) {
    case ParamType::Integer: *(int32_t *)p.value = v.integer; break;
    case ParamType::Float: *(float *)p.value = v.number; break;
    case ParamType::Boolean: *(bool *)p.value = v.boolean; break;
    case ParamType::Text: snprintf((char *)p.value, p.textCapacity, "%s", v.text); break;
    case ParamType::Enum: *(uint8_t *)p.value = v.enumIndex; break;
    case ParamType::IpAddress: memcpy(p.value, v.ip, 4); break;
    }
}

}  // namespace gte
