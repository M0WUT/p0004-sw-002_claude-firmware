// SCPI commands provided by GTE Core ("The SCPI command tree", issue #15).
//
// Each command is documented in the @scpi doc comment above its
// registration: that is the only source of the generated SCPI manual
// (p0004-sw-003_scpi-manual-generator). The blocks are comments, so none of
// their text reaches the firmware image.
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <strings.h>

#include <cstdio>
#include <cstring>

#include "gte/application_service.h"
#include "gte/core_errors.h"
#include "gte/fatal.h"
#include "gte/gte.h"
#include "gte/logging_helper.h"
#include "gte/scpi_service.h"
#include "gte/service_helper.h"

namespace gte {

namespace {

ScpiService *Scpi(ScpiCall *c) {
  return static_cast<ScpiService *>(c->context());
}
ScpiStatus &Status(ScpiCall *c) { return Scpi(c)->status(); }

void PrintUnsigned(ScpiCall *c, unsigned v) {
  char b[12];
  snprintf(b, sizeof(b), "%u", v);
  c->out()->print(b);
}

// ---------------------------------------------------- IEEE 488.2 handlers

void Idn(ScpiCall *c) {
  if (!c->ExpectParams(0)) return;
  char uid[25];
  UniqueIdString(uid, sizeof(uid));
  char b[160];
  snprintf(b, sizeof(b), "M0WUT,%s,%s,%s", InstrumentName(), uid,
           InstrumentVersion());
  c->out()->print(b);
}

void Rst(ScpiCall *c) {
  if (!c->ExpectParams(0)) return;
  if (application()) application()->RemoteReset();
}

void Cls(ScpiCall *c) {
  if (c->ExpectParams(0)) Status(c).Clear();
}

void EseSet(ScpiCall *c) {
  int32_t v;
  if (c->ExpectParams(1) && c->IntegerParam(0, 0, 255, &v))
    Status(c).Set(ScpiStatus::Enable::kEse, static_cast<uint16_t>(v));
}

void EseQuery(ScpiCall *c) {
  if (c->ExpectParams(0))
    PrintUnsigned(c, Status(c).Get(ScpiStatus::Enable::kEse));
}

void EsrQuery(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).TakeEsr());
}

void OpcSet(ScpiCall *c) {
  // Every command runs to completion before the next, so operations are
  // always complete.
  if (c->ExpectParams(0)) Status(c).SetEsr(ScpiStatus::kOpc);
}

void OpcQuery(ScpiCall *c) {
  if (c->ExpectParams(0)) c->out()->print("1");
}

void SreSet(ScpiCall *c) {
  int32_t v;
  if (c->ExpectParams(1) && c->IntegerParam(0, 0, 255, &v))
    Status(c).Set(ScpiStatus::Enable::kSre, static_cast<uint16_t>(v));
}

void SreQuery(ScpiCall *c) {
  if (c->ExpectParams(0))
    PrintUnsigned(c, Status(c).Get(ScpiStatus::Enable::kSre));
}

void StbQuery(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).Stb());
}

void TstQuery(ScpiCall *c) {
  if (c->ExpectParams(0)) c->out()->print("0");  // no self-test designed yet
}

void Wai(ScpiCall *c) { c->ExpectParams(0); }

// ------------------------------------------------------- SYSTem handlers

void ErrNext(ScpiCall *c) {
  if (!c->ExpectParams(0)) return;
  int16_t code = 0;
  char text[ScpiStatus::kTextSize];
  if (!Status(c).Pop(&code, text, sizeof(text)))
    snprintf(text, sizeof(text), "No error");
  char n[8];
  snprintf(n, sizeof(n), "%d,", code);
  c->out()->print(n);
  c->PrintQuoted(text);
}

void ErrCount(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).Count());
}

void Version(ScpiCall *c) {
  if (c->ExpectParams(0)) c->out()->print("1999.0");
}

void FactoryResetNow(ScpiCall *c) {
  if (c->ExpectParams(0)) FactoryReset("SCPI");
}

void Reboot(ScpiCall *c) {
  if (c->ExpectParams(0)) Restart("SCPI", "SYSTem:REBoot: restarting");
}

// ------------------------------------------------------- STATus handlers

void OperEvent(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).TakeOperEvent());
}

void OperCondition(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).OperCondition());
}

void OperEnableSet(ScpiCall *c) {
  int32_t v;
  if (c->ExpectParams(1) && c->IntegerParam(0, 0, 65535, &v))
    Status(c).Set(ScpiStatus::Enable::kOper, static_cast<uint16_t>(v));
}

void OperEnableQuery(ScpiCall *c) {
  if (c->ExpectParams(0))
    PrintUnsigned(c, Status(c).Get(ScpiStatus::Enable::kOper));
}

void QuesEvent(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).TakeQuesEvent());
}

void QuesCondition(ScpiCall *c) {
  if (c->ExpectParams(0)) PrintUnsigned(c, Status(c).QuesCondition());
}

void QuesEnableSet(ScpiCall *c) {
  int32_t v;
  if (c->ExpectParams(1) && c->IntegerParam(0, 0, 65535, &v))
    Status(c).Set(ScpiStatus::Enable::kQues, static_cast<uint16_t>(v));
}

void QuesEnableQuery(ScpiCall *c) {
  if (c->ExpectParams(0))
    PrintUnsigned(c, Status(c).Get(ScpiStatus::Enable::kQues));
}

void Preset(ScpiCall *c) {
  if (c->ExpectParams(0)) Status(c).Preset();
}

// ------------------------------------------- generic tree (SYSTem:GTE)

// The Service named by quoted parameter i, or nullptr (error added).
BaseService *ServiceParam(ScpiCall *c, size_t i) {
  const char *name;
  if (!c->StringParam(i, &name)) return nullptr;
  ServiceHelper &all = services();
  for (size_t k = 0; k < all.count(); k++)
    if (!strcasecmp(all.At(k)->name(), name)) return all.At(k);
  char info[48];
  snprintf(info, sizeof(info), "Unknown Service %s", name);
  c->Error(scpi_errors::kIllegalParameterValue, info);
  return nullptr;
}

// The Config Parameter named by quoted parameter i, or nullptr.
const ConfigParam *ConfigParamOf(ScpiCall *c, size_t i, const ConfigList &l) {
  const char *id;
  if (!c->StringParam(i, &id)) return nullptr;
  for (size_t k = 0; k < l.count(); k++)
    if (!strcasecmp(l[k].id, id)) return &l[k];
  char info[64];
  snprintf(info, sizeof(info), "Unknown Config Parameter %s", id);
  c->Error(scpi_errors::kIllegalParameterValue, info);
  return nullptr;
}

// The Service and Config Parameter named by quoted parameters 0 and 1.
struct ParamRef {
  BaseService *service = nullptr;
  ConfigList list;
  const ConfigParam *param = nullptr;
};

bool ResolveParam(ScpiCall *c, ParamRef *r) {
  r->service = ServiceParam(c, 0);
  if (!r->service) return false;
  r->service->Config(&r->list);
  r->param = ConfigParamOf(c, 1, r->list);
  return r->param != nullptr;
}

enum class Limit : uint8_t { kNone, kMin, kMax, kDef };

Limit LimitOf(ScpiCall *c, size_t i) {
  const char *t = c->param(i);
  if (!t || c->quoted(i)) return Limit::kNone;
  if (!strcasecmp(t, "MIN") || !strcasecmp(t, "MINimum")) return Limit::kMin;
  if (!strcasecmp(t, "MAX") || !strcasecmp(t, "MAXimum")) return Limit::kMax;
  if (!strcasecmp(t, "DEF") || !strcasecmp(t, "DEFault")) return Limit::kDef;
  return Limit::kNone;
}

bool Numeric(const ConfigParam &p) {
  return p.type == ParamType::kInteger || p.type == ParamType::kFloat;
}

// MIN, MAX or DEF of a numeric parameter as text; false (error added) if the
// parameter isn't numeric.
bool LimitText(ScpiCall *c, BaseService *s, const ConfigParam &p, Limit l,
               char *buf, size_t size) {
  if (!Numeric(p)) {
    c->Error(scpi_errors::kIllegalParameterValue,
             "MIN, MAX and DEF are for numeric parameters");
    return false;
  }
  if (l == Limit::kDef) return s->DefaultText(p.id, buf, size);
  const double v = l == Limit::kMin ? p.min : p.max;
  snprintf(buf, size, "%.*f", p.type == ParamType::kFloat ? p.decimals : 0, v);
  return true;
}

void ServiceCatalog(ScpiCall *c) {
  if (!c->ExpectParams(0)) return;
  ServiceHelper &all = services();
  bool first = true;
  for (int id = 0; id < 256; id++) {
    for (size_t k = 0; k < all.count(); k++) {
      if (static_cast<int>(all.At(k)->id()) != id) continue;
      if (!first) c->out()->print(",");
      first = false;
      c->PrintQuoted(all.At(k)->name());
    }
  }
}

void ParameterCatalog(ScpiCall *c) {
  if (!c->ExpectParams(1)) return;
  BaseService *s = ServiceParam(c, 0);
  if (!s) return;
  ConfigList list;
  s->Config(&list);
  for (size_t k = 0; k < list.count(); k++) {
    if (k) c->out()->print(",");
    c->PrintQuoted(list[k].id);
  }
  if (!list.count()) c->PrintQuoted("");
}

int16_t ScpiErrorFor(ConfigError e) {
  switch (e) {
    case ConfigError::kType:
      return scpi_errors::kDataTypeError;
    case ConfigError::kRange:
    case ConfigError::kTooLong:
      return scpi_errors::kDataOutOfRange;
    case ConfigError::kRefused:
      return scpi_errors::kSettingsConflict;
    default:
      return scpi_errors::kIllegalParameterValue;
  }
}

void ParameterSet(ScpiCall *c) {
  ParamRef ref;
  if (!c->ExpectParams(3) || !ResolveParam(c, &ref)) return;
  BaseService *s = ref.service;
  const ConfigParam *p = ref.param;
  char value[kConfigValueText + 8];
  const Limit l = LimitOf(c, 2);
  if (l != Limit::kNone) {
    if (!LimitText(c, s, *p, l, value, sizeof(value))) return;
  } else {
    // Text must be a quoted string; nothing else may be (an IP address may be
    // either).
    const bool text = p->type == ParamType::kText;
    if (c->quoted(2) != text && p->type != ParamType::kIpAddress) {
      c->Error(scpi_errors::kDataTypeError,
               text ? "Text must be a quoted string" : "Must not be quoted");
      return;
    }
    snprintf(value, sizeof(value), "%s", c->param(2));
    if (strlen(c->param(2)) >= sizeof(value)) {
      c->Error(scpi_errors::kDataOutOfRange, "Too long");
      return;
    }
  }
  ConfigResult r = s->SetConfig(p->id, value);
  if (!r.ok) c->Error(ScpiErrorFor(r.error), r.message);
}

void ParameterQuery(ScpiCall *c) {
  if (c->param_count() < 2) {
    c->Error(scpi_errors::kMissingParameter);
    return;
  }
  if (c->param_count() > 3) {
    c->Error(scpi_errors::kParameterNotAllowed);
    return;
  }
  ParamRef ref;
  if (!ResolveParam(c, &ref)) return;
  BaseService *s = ref.service;
  const ConfigParam *p = ref.param;
  char value[kConfigValueText + 8];
  if (c->param_count() == 3) {
    const Limit l = LimitOf(c, 2);
    if (l == Limit::kNone) {
      c->Error(scpi_errors::kIllegalParameterValue, "Expected MIN, MAX or DEF");
      return;
    }
    if (LimitText(c, s, *p, l, value, sizeof(value))) c->out()->print(value);
    return;
  }
  if (p->secret) {
    c->Error(scpi_errors::kSettingsConflict, "Secret parameter: write-only");
    return;
  }
  if (!s->CurrentText(p->id, value, sizeof(value))) return;
  if (p->type == ParamType::kBoolean) {
    c->out()->print(strcmp(value, "ON") ? "0" : "1");
  } else if (p->type == ParamType::kText) {
    c->PrintQuoted(value);
  } else {
    c->out()->print(value);
  }
}

void ParameterDefaults(ScpiCall *c) {
  if (!c->ExpectParams(1)) return;
  if (BaseService *s = ServiceParam(c, 0)) s->ResetToDefaults();
}

void StatusCatalog(ScpiCall *c) {
  if (!c->ExpectParams(1)) return;
  BaseService *s = ServiceParam(c, 0);
  if (!s) return;
  StatusList list;
  s->Status(&list);
  for (size_t k = 0; k < list.count(); k++) {
    if (k) c->out()->print(",");
    c->PrintQuoted(list[k].id);
  }
  if (!list.count()) c->PrintQuoted("");
}

void StatusQuery(ScpiCall *c) {
  if (!c->ExpectParams(2)) return;
  BaseService *s = ServiceParam(c, 0);
  if (!s) return;
  const char *id;
  if (!c->StringParam(1, &id)) return;
  StatusList list;
  s->Status(&list);
  for (size_t k = 0; k < list.count(); k++) {
    if (strcasecmp(list[k].id, id)) continue;
    char value[64];
    list[k].Exact(value, sizeof(value));
    if (list[k].type == StatusType::kText)
      c->PrintQuoted(value);
    else
      c->out()->print(value);
    return;
  }
  char info[64];
  snprintf(info, sizeof(info), "Unknown Status Value %s", id);
  c->Error(scpi_errors::kIllegalParameterValue, info);
}

void RestartRequired(ScpiCall *c) {
  if (c->ExpectParams(0))
    c->out()->print(services().restart_required() ? "1" : "0");
}

// ------------------------------------------------------------- DIAGnostic

void DiagUnlock(ScpiCall *c) {
  const char *key;
  if (!c->ExpectParams(1) || !c->StringParam(0, &key)) return;
  if (strcmp(key, ScpiService::kDiagKey)) {
    c->Error(scpi_errors::kIllegalParameterValue, "Wrong key");
    return;
  }
  Scpi(c)->set_diag_unlocked(true);
  log().Log("SCPI", LogLevel::kInfo, 0, "DIAG commands unlocked");
}

void DiagWarning(ScpiCall *c) {
  const char *text;
  if (c->ExpectParams(1) && c->StringParam(0, &text))
    log().RaiseWarning("CORE", errors::kDiagTestWarning, text);
}

void DiagWarningClear(ScpiCall *c) {
  if (c->ExpectParams(0)) log().ClearWarning(errors::kDiagTestWarning.code);
}

void DiagFatal(ScpiCall *c) {
  const char *text;
  if (c->ExpectParams(1) && c->StringParam(0, &text))
    Fatal("CORE", errors::kDiagTestFatal, text);
}

void DiagHang(ScpiCall *c) {
  if (!c->ExpectParams(0)) return;
  log().Log("SCPI", LogLevel::kInfo, 0,
            "DIAG:HANG: the SCPI task stops checking in");
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void DiagCommandCatalog(ScpiCall *c) {
  if (!c->ExpectParams(0)) return;
  ScpiParser &p = Scpi(c)->parser();
  for (size_t k = 0; k < p.count(); k++) {
    if (k) c->out()->print(",");
    c->PrintQuoted(p.header(k));
  }
}

}  // namespace

void RegisterCoreScpiCommands(ScpiRegistry *r, ScpiService *scpi) {
  // ------------------------------------------- IEEE 488.2 common commands

  /** @scpi
   * command: *IDN
   * form: query
   * brief: Identify the instrument.
   * response: Four comma-separated fields: M0WUT,<Instrument name>,<STM32 unique ID>,<Instrument firmware version>
   * group: IEEE 488.2 common commands
   * example: *IDN? | M0WUT,P0004-SW-002,2F0045001851333035383533,v1.2.0 | Made by M0WUT; Instrument name P0004-SW-002; STM32 unique ID 2F0045001851333035383533; firmware version v1.2.0.
   */
  r->Register("*IDN?", Idn, scpi);

  /** @scpi
   * command: *RST
   * form: event
   * brief: Return the instrument to a safe remote state.
   * details: Stops Application Logging and calls the Application Service's reset hook. Does not change any Config Parameter, write to EEPROM, or clear the error queue.
   * group: IEEE 488.2 common commands
   */
  r->Register("*RST", Rst, scpi);

  /** @scpi
   * command: *CLS
   * form: event
   * brief: Clear the status data structures and the error queue.
   * group: IEEE 488.2 common commands
   */
  r->Register("*CLS", Cls, scpi);

  /** @scpi
   * command: *ESE
   * form: set
   * syntax: *ESE <mask>
   * brief: Set the Standard Event Status Enable register.
   * param: mask | integer 0-255 | Bits of the Standard Event Status register that set ESB in the status byte
   * errors: -222
   * group: IEEE 488.2 common commands
   * example: *ESE 60 | Enables bits 2 to 5 (Query, Device-dependent, Execution and Command Error), so any error sets the ESB bit (bit 5) in the status byte.
   */
  r->Register("*ESE", EseSet, scpi);

  /** @scpi
   * command: *ESE
   * form: query
   * brief: Read the Standard Event Status Enable register.
   * response: Integer 0-255
   * group: IEEE 488.2 common commands
   * example: *ESE? | 60 | Bits 2 to 5 are enabled: any Query, Device-dependent, Execution or Command Error sets the ESB bit in the status byte.
   */
  r->Register("*ESE?", EseQuery, scpi);

  /** @scpi
   * command: *ESR
   * form: query
   * brief: Read and clear the Standard Event Status register.
   * details: Bit 0 Operation Complete, bit 2 Query Error, bit 3 Device-dependent Error (e.g. -350 Queue overflow), bit 4 Execution Error (-200 to -299), bit 5 Command Error (-100 to -199). Warnings do not set any bit here: they set QUEStionable bit 9.
   * response: Integer 0-255
   * group: IEEE 488.2 common commands
   * example: *ESR? | 32 | Bit 5 (Command Error) is set: a command was not understood since the register was last read. Reading it has cleared it to 0.
   */
  r->Register("*ESR?", EsrQuery, scpi);

  /** @scpi
   * command: *OPC
   * form: event
   * brief: Set the Operation Complete bit when all pending operations have finished.
   * group: IEEE 488.2 common commands
   */
  r->Register("*OPC", OpcSet, scpi);

  /** @scpi
   * command: *OPC
   * form: query
   * brief: Return 1 when all pending operations have finished.
   * response: 1
   * group: IEEE 488.2 common commands
   * example: *OPC? | 1 | All pending operations have finished.
   */
  r->Register("*OPC?", OpcQuery, scpi);

  /** @scpi
   * command: *SRE
   * form: set
   * syntax: *SRE <mask>
   * brief: Set the Service Request Enable register.
   * param: mask | integer 0-255 | Status byte bits that generate a service request
   * errors: -222
   * group: IEEE 488.2 common commands
   * example: *SRE 32 | Requests service when the ESB bit (bit 5, Standard Event Status summary) of the status byte is set.
   */
  r->Register("*SRE", SreSet, scpi);

  /** @scpi
   * command: *SRE
   * form: query
   * brief: Read the Service Request Enable register.
   * response: Integer 0-255
   * group: IEEE 488.2 common commands
   * example: *SRE? | 32 | A service request is generated only when the ESB bit (bit 5) of the status byte is set.
   */
  r->Register("*SRE?", SreQuery, scpi);

  /** @scpi
   * command: *STB
   * form: query
   * brief: Read the status byte.
   * details: Bit 2 error queue not empty, bit 3 QUEStionable summary, bit 4 message available, bit 5 standard event summary, bit 6 master summary, bit 7 OPERation summary.
   * response: Integer 0-255
   * group: IEEE 488.2 common commands
   * example: *STB? | 4 | Only bit 2 is set: the error queue holds at least one entry. Read it with SYSTem:ERRor?.
   */
  r->Register("*STB?", StbQuery, scpi);

  /** @scpi
   * command: *TST
   * form: query
   * brief: Run the self-test.
   * response: 0 if the self-test passed, otherwise a non-zero failure code
   * group: IEEE 488.2 common commands
   * example: *TST? | 0 | The self-test passed.
   */
  r->Register("*TST?", TstQuery, scpi);

  /** @scpi
   * command: *WAI
   * form: event
   * brief: Wait until all pending operations have finished before running further commands.
   * group: IEEE 488.2 common commands
   */
  r->Register("*WAI", Wai, scpi);

  // ---------------------------------------------------------------- SYSTem

  /** @scpi
   * command: SYSTem:ERRor:NEXT
   * form: query
   * syntax: SYSTem:ERRor[:NEXT]?
   * brief: Read and remove the oldest entry in the error queue.
   * details: The queue holds 16 entries: SCPI-99 errors (negative) and the instrument's Error Codes (positive, including every Warning raised, with its supplementary text after a colon). When the queue overflows, its newest entry is replaced with -350.
   * response: <code>,"<message>" e.g. 0,"No error"
   * group: System
   * example: SYST:ERR? | -113,"Undefined header" | The oldest entry is SCPI-99 error -113: a command that the instrument does not recognise was sent.
   * example: SYST:ERR? | 105,"Recovered from a watchdog reset: Service SCPI" | The oldest entry is the instrument's Error Code 105, a Warning raised after the SCPI Service stopped responding and the watchdog restarted the Instrument.
   * example: SYST:ERR? | 0,"No error" | The error queue is empty.
   */
  r->Register("SYSTem:ERRor?", ErrNext, scpi);
  r->Register("SYSTem:ERRor:NEXT?", ErrNext, scpi);

  /** @scpi
   * command: SYSTem:ERRor:COUNt
   * form: query
   * brief: Number of entries in the error queue.
   * response: Integer
   * group: System
   * example: SYST:ERR:COUN? | 2 | The error queue holds two entries.
   */
  r->Register("SYSTem:ERRor:COUNt?", ErrCount, scpi);

  /** @scpi
   * command: SYSTem:VERSion
   * form: query
   * brief: SCPI version implemented.
   * response: 1999.0
   * group: System
   * example: SYST:VERS? | 1999.0 | The instrument implements SCPI version 1999.0.
   */
  r->Register("SYSTem:VERSion?", Version, scpi);

  /** @scpi
   * command: SYSTem:SECurity:IMMediate
   * form: event
   * brief: Factory Reset: return every Service to its default Config Parameters, then restart.
   * details: Rebuilds the config storage from scratch. The MAC address is not affected.
   * group: System
   */
  r->Register("SYSTem:SECurity:IMMediate", FactoryResetNow, scpi);

  /** @scpi
   * command: SYSTem:REBoot
   * form: event
   * brief: Restart the Instrument.
   * details: Applies Config Parameters whose Apply Mode is Reset.
   * group: System
   */
  r->Register("SYSTem:REBoot", Reboot, scpi);

  // ---------------------------------------------------------------- STATus

  /** @scpi
   * command: STATus:OPERation:EVENt
   * form: query
   * syntax: STATus:OPERation[:EVENt]?
   * brief: Read and clear the OPERation event register.
   * details: Bit 8: Application Logging. Bits 9 to 12 belong to the Application Service.
   * response: Integer
   * group: Status
   * example: STAT:OPER? | 256 | Bit 8 (Application Logging) has been set since the register was last read. Reading it has cleared it to 0.
   */
  r->Register("STATus:OPERation?", OperEvent, scpi);
  r->Register("STATus:OPERation:EVENt?", OperEvent, scpi);

  /** @scpi
   * command: STATus:OPERation:CONDition
   * form: query
   * brief: Read the OPERation condition register.
   * response: Integer
   * group: Status
   * example: STAT:OPER:COND? | 256 | Bit 8 is set: Application Logging is running now.
   */
  r->Register("STATus:OPERation:CONDition?", OperCondition, scpi);

  /** @scpi
   * command: STATus:OPERation:ENABle
   * form: set
   * syntax: STATus:OPERation:ENABle <mask>
   * brief: Set which OPERation bits are summarised in the status byte.
   * param: mask | integer 0-65535 | Enable mask
   * errors: -222
   * group: Status
   * example: STAT:OPER:ENAB 256 | Enables bit 8 only, so Application Logging sets the OPERation summary bit (bit 7) in the status byte.
   */
  r->Register("STATus:OPERation:ENABle", OperEnableSet, scpi);

  /** @scpi
   * command: STATus:OPERation:ENABle
   * form: query
   * brief: Read the OPERation enable register.
   * response: Integer 0-65535
   * group: Status
   * example: STAT:OPER:ENAB? | 256 | Only bit 8 (Application Logging) is summarised in the status byte.
   */
  r->Register("STATus:OPERation:ENABle?", OperEnableQuery, scpi);

  /** @scpi
   * command: STATus:QUEStionable:EVENt
   * form: query
   * syntax: STATus:QUEStionable[:EVENt]?
   * brief: Read and clear the QUEStionable event register.
   * details: Bit 4: warning temperature. Bit 9: a Warning was raised.
   * response: Integer
   * group: Status
   * example: STAT:QUES? | 512 | Bit 9 has been set since the register was last read: a Warning was raised. Reading it has cleared it to 0.
   */
  r->Register("STATus:QUEStionable?", QuesEvent, scpi);
  r->Register("STATus:QUEStionable:EVENt?", QuesEvent, scpi);

  /** @scpi
   * command: STATus:QUEStionable:CONDition
   * form: query
   * brief: Read the QUEStionable condition register.
   * response: Integer
   * group: Status
   * example: STAT:QUES:COND? | 528 | Bits 4 and 9 are set (16 + 512): a temperature is above its Warning temperature, and a Warning is active.
   */
  r->Register("STATus:QUEStionable:CONDition?", QuesCondition, scpi);

  /** @scpi
   * command: STATus:QUEStionable:ENABle
   * form: set
   * syntax: STATus:QUEStionable:ENABle <mask>
   * brief: Set which QUEStionable bits are summarised in the status byte.
   * param: mask | integer 0-65535 | Enable mask
   * errors: -222
   * group: Status
   * example: STAT:QUES:ENAB 528 | Enables bits 4 (Warning temperature) and 9 (Warning active), so either sets the QUEStionable summary bit (bit 3) in the status byte.
   */
  r->Register("STATus:QUEStionable:ENABle", QuesEnableSet, scpi);

  /** @scpi
   * command: STATus:QUEStionable:ENABle
   * form: query
   * brief: Read the QUEStionable enable register.
   * response: Integer 0-65535
   * group: Status
   * example: STAT:QUES:ENAB? | 512 | Only bit 9 (a Warning was raised) is summarised in the status byte.
   */
  r->Register("STATus:QUEStionable:ENABle?", QuesEnableQuery, scpi);

  /** @scpi
   * command: STATus:PRESet
   * form: event
   * brief: Reset the OPERation and QUEStionable enable registers to their defaults (0).
   * group: Status
   */
  r->Register("STATus:PRESet", Preset, scpi);

  // ------------------------------------------ generic settings (SYSTem:GTE)

  /** @scpi
   * command: SYSTem:GTE:CATalog
   * form: query
   * brief: List the Services, in Service id order.
   * response: Comma-separated quoted Service names, e.g. "CORE","NETWORK","THERMAL"
   * group: Generic settings and status
   * example: SYST:GTE:CAT? | "CORE","NETWORK","WEB","DISPLAY","UI","THERMAL","POWER","STORAGE","USB_DEVICE","USB_HOST","SCPI","HOME_ASSISTANT","APPLICATION_EXAMPLE" | The instrument has these thirteen Services: the GTE Core Services and its Application Service, APPLICATION_EXAMPLE.
   */
  r->Register("SYSTem:GTE:CATalog?", ServiceCatalog, scpi);

  /** @scpi
   * command: SYSTem:GTE:PARameter:CATalog
   * form: query
   * syntax: SYSTem:GTE:PARameter:CATalog? "<service>"
   * brief: List a Service's Config Parameter ids.
   * param: service | string | Service name, as listed by SYSTem:GTE:CATalog?
   * response: Comma-separated quoted Config Parameter ids
   * errors: -224
   * group: Generic settings and status
   * example: SYST:GTE:PAR:CAT? "CORE" | "logLevel" | The CORE Service has one Config Parameter, logLevel (its Log level).
   */
  r->Register("SYSTem:GTE:PARameter:CATalog?", ParameterCatalog, scpi);

  /** @scpi
   * command: SYSTem:GTE:PARameter
   * form: set
   * syntax: SYSTem:GTE:PARameter "<service>","<parameter>",<value>|MIN|MAX|DEF
   * brief: Set a Config Parameter.
   * details: Runs the same checks as the web page and the front panel, then saves to EEPROM. Live parameters take effect at once; Reset parameters after SYSTem:REBoot. Errors: -224 for an unknown Service or parameter, an enumeration name that isn't allowed, a value off the parameter's step, or MIN/MAX/DEF on a non-numeric parameter; -104 for a value of the wrong type (text must be quoted, nothing else may be); -222 for a value out of range or text that is too long; -221 when the Service's own rule refuses it, with the reason after a semicolon.
   * param: service | string | Service name
   * param: parameter | string | Config Parameter id
   * param: value | number, boolean (ON/OFF/1/0), enumeration name (unquoted), IP address or quoted string | New value, or MIN, MAX or DEF for numeric parameters
   * errors: -224, -222, -221, -104
   * group: Generic settings and status
   * example: SYST:GTE:PAR "CORE","logLevel",DEBUG | Sets the CORE Service's Log level to DEBUG, so its debug messages are printed and saved too.
   */
  r->Register("SYSTem:GTE:PARameter", ParameterSet, scpi);

  /** @scpi
   * command: SYSTem:GTE:PARameter
   * form: query
   * syntax: SYSTem:GTE:PARameter? "<service>","<parameter>"[,MIN|MAX|DEF]
   * brief: Read a Config Parameter, or its minimum, maximum or default.
   * details: Secret Config Parameters (e.g. passwords) cannot be read and return -221. MIN, MAX and DEF are for numeric parameters only (-224 otherwise).
   * param: service | string | Service name
   * param: parameter | string | Config Parameter id
   * response: The value; booleans as 1 or 0, enumerations as their name (unquoted), IP addresses unquoted, text as a quoted string
   * errors: -224, -221
   * group: Generic settings and status
   * example: SYST:GTE:PAR? "CORE","logLevel" | INFO | The CORE Service's Log level is INFO.
   */
  r->Register("SYSTem:GTE:PARameter?", ParameterQuery, scpi);

  /** @scpi
   * command: SYSTem:GTE:PARameter:DEFault
   * form: set
   * syntax: SYSTem:GTE:PARameter:DEFault "<service>"
   * brief: Reset every Config Parameter of one Service to its default.
   * param: service | string | Service name
   * errors: -224
   * group: Generic settings and status
   * example: SYST:GTE:PAR:DEF "CORE" | Resets every Config Parameter of the CORE Service to its default, so its Log level returns to INFO.
   */
  r->Register("SYSTem:GTE:PARameter:DEFault", ParameterDefaults, scpi);

  /** @scpi
   * command: SYSTem:GTE:STATus:CATalog
   * form: query
   * syntax: SYSTem:GTE:STATus:CATalog? "<service>"
   * brief: List a Service's Status Value ids.
   * param: service | string | Service name
   * response: Comma-separated quoted Status Value ids ("" if it has none)
   * errors: -224
   * group: Generic settings and status
   */
  r->Register("SYSTem:GTE:STATus:CATalog?", StatusCatalog, scpi);

  /** @scpi
   * command: SYSTem:GTE:STATus
   * form: query
   * syntax: SYSTem:GTE:STATus? "<service>","<status value>"
   * brief: Read a Status Value.
   * param: service | string | Service name
   * param: status value | string | Status Value id
   * response: The value in its SI unit (numbers), 1/0 (booleans) or a quoted string
   * errors: -224
   * group: Generic settings and status
   */
  r->Register("SYSTem:GTE:STATus?", StatusQuery, scpi);

  /** @scpi
   * command: SYSTem:GTE:RESet:REQuired
   * form: query
   * brief: Whether a Reset-mode Config Parameter change is waiting for a restart.
   * response: 1 if SYSTem:REBoot is needed to apply a change, otherwise 0
   * group: Generic settings and status
   * example: SYST:GTE:RES:REQ? | 0 | No change is waiting for a restart.
   */
  r->Register("SYSTem:GTE:RESet:REQuired?", RestartRequired, scpi);

  // ------------------------------------------------------------ Engineering

  /** @scpi
   * command: DIAGnostic:UNLock
   * form: set
   * syntax: DIAGnostic:UNLock "<key>"
   * brief: Unlock the other DIAGnostic commands until the next restart.
   * details: The key is "GTE-ENGINEERING". It guards against accidents, not against an attacker. Until it is given, every other DIAGnostic command gives -203 and does nothing.
   * param: key | string | The engineering key
   * errors: -224
   * group: Engineering
   * example: DIAG:UNL "GTE-ENGINEERING" | Unlocks the engineering commands until the Instrument restarts.
   */
  r->Register("DIAGnostic:UNLock", DiagUnlock, scpi);

  /** @scpi
   * command: DIAGnostic:WARNing
   * form: set
   * syntax: DIAGnostic:WARNing "<text>"
   * brief: Raise the CORE test Warning (Error Code 106), with text as its supplementary text.
   * param: text | string | Supplementary text
   * errors: -203
   * group: Engineering
   * example: DIAG:WARN "bench check" | Raises Warning 106, so the LED turns amber and 106,"Engineering test Warning (DIAG:WARNing): bench check" is queued.
   */
  r->Register("DIAGnostic:WARNing", DiagWarning, scpi, kScpiProtected);

  /** @scpi
   * command: DIAGnostic:WARNing:CLEar
   * form: event
   * brief: Clear the CORE test Warning.
   * errors: -203
   * group: Engineering
   */
  r->Register("DIAGnostic:WARNing:CLEar", DiagWarningClear, scpi,
              kScpiProtected);

  /** @scpi
   * command: DIAGnostic:FATal
   * form: set
   * syntax: DIAGnostic:FATal "<text>"
   * brief: Stop the Instrument with the CORE test Fatal Error (Error Code 107).
   * details: Normal operation stops for good: the error is shown on the debug UART and the red LED blinks. Only a reset recovers.
   * param: text | string | Supplementary text
   * errors: -203
   * group: Engineering
   * example: DIAG:FAT "bench check" | Stops the Instrument with Fatal Error 107, "Engineering test Fatal Error (DIAG:FATal): bench check".
   */
  r->Register("DIAGnostic:FATal", DiagFatal, scpi, kScpiProtected);

  /** @scpi
   * command: DIAGnostic:HANG
   * form: event
   * brief: Hang the SCPI task, to test the watchdog.
   * details: The SCPI task stops checking in, so the watchdog restarts the Instrument within about 6 s, and Warning 105 is queued after the restart.
   * errors: -203
   * group: Engineering
   */
  r->Register("DIAGnostic:HANG", DiagHang, scpi, kScpiProtected);

  /** @scpi
   * command: DIAGnostic:COMMand:CATalog
   * form: query
   * brief: List every registered command header.
   * details: Long form, one entry per registration: an optional keyword appears both with and without it.
   * response: Comma-separated quoted headers
   * errors: -203
   * group: Engineering
   * example: DIAG:COMM:CAT? | "*IDN?","*RST","*CLS" | The first three registered headers (the full list is much longer).
   */
  r->Register("DIAGnostic:COMMand:CATalog?", DiagCommandCatalog, scpi,
              kScpiProtected);
}

}  // namespace gte
