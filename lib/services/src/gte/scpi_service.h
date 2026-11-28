// The SCPI Service (issues #6, #15): one parser, one session, one error queue
// and one IEEE 488.2 / SCPI-99 status model, for every transport. Only USB CDC
// for now; TCP port 5025 comes with the Network Service (#46).
//
// The session belongs to the CDC port while DTR is high. With DTR low, input is
// thrown away unanswered. The error queue is kept when the session ends.
#pragma once
#include <cstddef>
#include <cstdint>

#include "gte/base_service.h"
#include "gte/error_code.h"
#include "gte/logging_helper.h"
#include "gte/scpi.h"
#include "gte/scpi_parser.h"

namespace gte {

GTE_ERROR_CODE(kScpiTableFull, 1101, kFatal, "SCPI command table full");
GTE_ERROR_CODE(kScpiHashCollision, 1102, kFatal,
               "SCPI command hash collision or duplicate");
GTE_ERROR_CODE(kScpiHeaderTooLong, 1103, kFatal, "SCPI header too long");
static_assert(CodeBelongsTo(kScpiTableFull.code, ServiceId::kScpi),
              "SCPI codes are 1101-1199");

// Status Byte, Standard Event Status and the two SCPI-99 registers, plus the
// error/event queue. Safe to use from any task.
class ScpiStatus {
 public:
  static constexpr size_t kQueueSize = 16;
  static constexpr size_t kTextSize = 136;

  // Standard Event Status bits.
  static constexpr uint8_t kOpc = 1 << 0;
  static constexpr uint8_t kQye = 1 << 2;
  static constexpr uint8_t kDde = 1 << 3;
  static constexpr uint8_t kExe = 1 << 4;
  static constexpr uint8_t kCme = 1 << 5;
  // QUEStionable bit 9: a Warning is active.
  static constexpr uint16_t kQuesWarning = 1 << 9;

  void Begin();

  // The queue. Negative codes set their ESR bit (SCPI-99 21.8).
  void Push(int16_t code, const char *text);
  bool Pop(int16_t *code, char *text, size_t size);  // false when empty
  size_t Count();

  void SetEsr(uint8_t bits);
  uint8_t TakeEsr();  // read and clear
  uint8_t Stb();

  // The four registers SCPI clients write: *ESE, *SRE and the two enables.
  enum class Enable : uint8_t { kEse, kSre, kOper, kQues };
  uint16_t Get(Enable which);
  void Set(Enable which, uint16_t value);

  // OPERation and QUEStionable: condition, event (latched on a 0->1 change of
  // the condition, or by Latch), enable.
  uint16_t OperCondition();
  uint16_t TakeOperEvent();
  void SetOperCondition(uint16_t mask, bool on);
  uint16_t QuesCondition();
  uint16_t TakeQuesEvent();
  void LatchQues(uint16_t bits);

  void Clear();   // *CLS: queue, ESR and the event registers
  void Preset();  // STAT:PRES: both enable registers to 0

 private:
  void Lock();
  void Unlock();
  struct Entry {
    int16_t code;
    char text[kTextSize];
  };
  Entry queue_[kQueueSize] = {};
  size_t head_ = 0;
  size_t count_ = 0;
  uint8_t esr_ = 0;
  uint16_t enables_[4] = {};  // indexed by Enable
  uint16_t oper_cond_ = 0, oper_event_ = 0;
  uint16_t ques_event_ = 0;
  void *mutex_ = nullptr;
};

class ScpiService : public BaseService,
                    public ScpiRegistry,
                    public ScpiParser::Sink {
 public:
  static constexpr size_t kMaxLine = 255;
  static constexpr const char *kDiagKey = "GTE-ENGINEERING";

  ScpiService() : BaseService(ServiceId::kScpi, TaskSettings{2, 2048, 2}) {}

  void Register(const char *header, ScpiHandler handler, void *context,
                uint8_t flags) override;
  // Core startup, after every Service is initialised: collect their commands.
  void RegisterServiceCommands();

  ScpiStatus &status() { return status_; }
  ScpiParser &parser() { return parser_; }
  bool diag_unlocked() const { return diag_unlocked_; }
  void set_diag_unlocked(bool on) { diag_unlocked_ = on; }
  // Add a SCPI-99 error with its standard text, plus optional info.
  void PushError(int16_t code, const char *info = nullptr);

 protected:
  void Initialise() override;
  void Tick() override;
  void RegisterScpiCommands(ScpiRegistry *registry) override;

 private:
  // ScpiParser::Sink
  void Error(int16_t code, const char *info) override;
  bool Allowed(uint8_t flags) override;

  static void OnWarning(void *self, const Warning &warning);
  void Feed(uint8_t c);
  void RunLine();

  ScpiParser parser_;
  ScpiStatus status_;
  bool session_ = false;
  bool diag_unlocked_ = false;
  char line_[kMaxLine + 2] = {};  // plus a CR before the LF
  size_t line_len_ = 0;
  bool overrun_ = false;
};

// Core's own commands (scpi_core_commands.cc).
void RegisterCoreScpiCommands(ScpiRegistry *registry, ScpiService *scpi);

}  // namespace gte
