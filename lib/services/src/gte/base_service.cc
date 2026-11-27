// Copyright 2026 M0WUT
#include "gte/base_service.h"

#include <STM32FreeRTOS.h>
#include <semphr.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gte/config_storage.h"
#include "gte/core_errors.h"
#include "gte/fatal.h"
#include "gte/gte.h"
#include "gte/logging_helper.h"
#include "gte/service_helper.h"

namespace gte {

void BaseService::Lock() {
  if (!mutex_) mutex_ = xSemaphoreCreateRecursiveMutex();
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(mutex_),
                            portMAX_DELAY);
}

void BaseService::Unlock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(mutex_));
}

void BaseService::Config(ConfigList *list) {
  Lock();
  list->Enumeration("logLevel", "Log level", &log_level_, kLogLevelNames,
                    kLogLevelCount);
  DeclareConfig(list);
  Unlock();
}

ConfigResult BaseService::SetConfig(const char *id, const char *text) {
  ConfigResult r;
  Lock();
  ConfigList list;
  Config(&list);
  const ConfigParam *p = list.Find(id);
  ConfigValue v;
  if (!p) {
    snprintf(r.message, sizeof(r.message), "Unknown parameter %s", id);
  } else if (!ParseConfigValue(*p, text, &v, r.message, sizeof(r.message))) {
    // message set by the parser
  } else if (!ValidateConfigField(id, v, r.message, sizeof(r.message))) {
    if (!r.message[0])
      snprintf(r.message, sizeof(r.message), "Refused by %s", name());
  } else {
    WriteConfigValue(*p, v);
    SaveConfig();
    r.ok = true;
    if (p->apply == ApplyMode::kReset) {
      r.restart_required = true;
      services().SetRestartRequired();
      snprintf(r.message, sizeof(r.message), "Applied after restart");
    } else {
      snprintf(r.message, sizeof(r.message), "Applied");
      OnConfigChanged(id);
    }
  }
  Unlock();
  char shown[kConfigValueText + 8] = "?";
  if (p) p->Display(shown, sizeof(shown));
  char msg[200];
  snprintf(
      msg, sizeof(msg), "Config %s = %s: %s%s", id,
      (p && p->secret) ? "****" : text, r.ok ? "accepted" : "rejected: ",
      r.ok ? (r.restart_required ? " (restart required)" : "") : r.message);
  LogInfo(msg);
  return r;
}

void BaseService::Status(StatusList *list) {
  Lock();
  GetStatus(list);
  Unlock();
}

void BaseService::RunInitialise() {
  LogInfo("Initialising");
  LoadConfig();
  Initialise();
  PrintConfig();
}

void BaseService::CaptureDefaults() {
  ConfigBlock b = GetConfigBlock();
  if (b.size && !defaults_) {
    defaults_ = static_cast<uint8_t *>(malloc(b.size));
    memcpy(defaults_, b.data, b.size);
  }
}

// Stored data: format byte (1), Log level, then the Service's struct.
static constexpr uint8_t kStoredFormat = 1;
static constexpr size_t kMaxMigration = 2048;

void BaseService::SaveConfig() {
  ConfigBlock b = GetConfigBlock();
  uint8_t *tmp = static_cast<uint8_t *>(malloc(b.size + 2));
  tmp[0] = kStoredFormat;
  tmp[1] = log_level_;
  if (b.size) memcpy(tmp + 2, b.data, b.size);
  storage().Save(static_cast<uint8_t>(id_), name(), b.version, tmp,
                 static_cast<uint16_t>(b.size + 2));
  free(tmp);
}

void BaseService::ResetToDefaults() {
  Lock();
  ConfigBlock b = GetConfigBlock();
  if (b.size && defaults_) memcpy(b.data, defaults_, b.size);
  log_level_ = static_cast<uint8_t>(LogLevel::kInfo);
  SaveConfig();
  Unlock();
  LogInfo("Config reset to defaults");
}

void BaseService::LoadConfig() {
  ConfigBlock b = GetConfigBlock();
  uint8_t version = 0;
  const uint8_t *data = nullptr;
  uint16_t len = 0;
  char msg[160];
  LoadResult r =
      storage().Load(static_cast<uint8_t>(id_), &version, &data, &len);
  if (r == LoadResult::kMissing) {
    LogInfo("No stored config: saving defaults");
    SaveConfig();
    return;
  }
  bool bad = r == LoadResult::kCrcFailed || len < 2 ||
             data[0] != kStoredFormat || data[1] >= kLogLevelCount;
  if (!bad && version == b.version && len - 2u != b.size) {
    snprintf(msg, sizeof(msg),
             "Config for %s has the wrong size for version %u (changed without "
             "a version bump). "
             "Hold Soft Key 4 for 3 s to reset %s to defaults.",
             name(), version, name());
    PromptReinitialise("ConfigStorage", msg);
    ResetToDefaults();
    return;
  }
  if (bad) {
    snprintf(msg, sizeof(msg),
             "Config for %s corrupt (CRC failed). Hold Soft Key 4 for 3 s to "
             "reset %s to defaults.",
             name(), name());
    PromptReinitialise("ConfigStorage", msg);
    ResetToDefaults();
    return;
  }
  log_level_ = data[1];
  const uint8_t *s = data + 2;
  size_t slen = len - 2;
  if (version == b.version) {
    if (b.size) memcpy(b.data, s, b.size);
    return;
  }
  if (version > b.version) {
    snprintf(msg, sizeof(msg), "%s stored v%u, firmware v%u", name(), version,
             b.version);
    Fatal(name(), errors::kConfigMigrationMissing, msg);
  }
  static uint8_t a[kMaxMigration], c[kMaxMigration];
  memcpy(a, s, slen);
  for (uint8_t v = version; v < b.version; v++) {
    int n = MigrateConfig(v, a, slen, c, sizeof(c));
    if (n < 0) {
      snprintf(msg, sizeof(msg), "%s v%u->v%u", name(), v, v + 1);
      Fatal(name(), errors::kConfigMigrationMissing, msg);
    }
    memcpy(a, c, n);
    slen = static_cast<size_t>(n);
  }
  if (slen != b.size) {
    snprintf(msg, sizeof(msg), "%s migration to v%u gave %u bytes, expected %u",
             name(), b.version, static_cast<unsigned>(slen), b.size);
    Fatal(name(), errors::kConfigMigrationMissing, msg);
  }
  memcpy(b.data, a, slen);
  LogInfof("Config migrated v%u -> v%u", version, b.version);
  SaveConfig();
}

void BaseService::PrintConfig() {
  ConfigList list;
  Config(&list);
  LogInfo("Config:");
  for (size_t i = 0; i < list.count(); i++) {
    char value[kConfigValueText + 16];
    list[i].Display(value, sizeof(value));
    char line[128];
    snprintf(line, sizeof(line), "  %s (%s): %s%s", list[i].label, list[i].id,
             value, list[i].apply == ApplyMode::kReset ? " [reset]" : "");
    LogInfo(line);
  }
}

void BaseService::LogAt(LogLevel level, const char *msg) {
  if (level >= log_level()) log().Log(name(), level, 0, msg);
}

void BaseService::LogDebugf(const char *fmt, ...) {
  char buf[160];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  LogDebug(buf);
}

void BaseService::LogInfof(const char *fmt, ...) {
  char buf[160];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  LogInfo(buf);
}

void BaseService::Warn(const ErrorCode &code, const char *supplementary) {
  // Warnings are always recorded; the Log level only filters debug/info
  // chatter.
  log().RaiseWarning(name(), code, supplementary);
}

void BaseService::Warnf(const ErrorCode &code, const char *fmt, ...) {
  char buf[kSupplementaryText];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  Warn(code, buf);
}

void BaseService::ClearWarning(const ErrorCode &code) {
  log().ClearWarning(code.code);
}

void BaseService::FatalError(const ErrorCode &code, const char *supplementary) {
  Fatal(name(), code, supplementary);
}

void BaseService::FatalErrorf(const ErrorCode &code, const char *fmt, ...) {
  char buf[kSupplementaryText];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  Fatal(name(), code, buf);
}

}  // namespace gte
