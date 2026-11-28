#include "gte/scpi_service.h"

#include <STM32FreeRTOS.h>
#include <semphr.h>

#include <cstdio>
#include <cstring>

#include "gte/fatal.h"
#include "gte/gte.h"
#include "gte/logging_helper.h"
#include "gte/service_helper.h"
#include "gte/usb_device_service.h"

namespace gte {

// --------------------------------------------------------------- ScpiStatus

void ScpiStatus::Begin() {
  if (!mutex_) mutex_ = xSemaphoreCreateRecursiveMutex();
}

void ScpiStatus::Lock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(mutex_),
                            portMAX_DELAY);
  }
}

void ScpiStatus::Unlock() {
  if (mutex_ && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(mutex_));
}

static uint8_t EsrBitFor(int16_t code) {
  if (code <= -100 && code > -200) return ScpiStatus::kCme;
  if (code <= -200 && code > -300) return ScpiStatus::kExe;
  if (code <= -300 && code > -400) return ScpiStatus::kDde;
  if (code <= -400 && code > -500) return ScpiStatus::kQye;
  return 0;  // positive codes (Warnings) use QUEStionable bit 9 instead
}

void ScpiStatus::Push(int16_t code, const char *text) {
  Lock();
  if (count_ >= kQueueSize) {
    // SCPI-99 21.8.1: the newest entry becomes -350; the oldest are kept.
    Entry &last = queue_[(head_ + kQueueSize - 1) % kQueueSize];
    last.code = scpi_errors::kQueueOverflow;
    snprintf(last.text, sizeof(last.text), "%s",
             ScpiErrorText(scpi_errors::kQueueOverflow));
    esr_ |= kDde;
  } else {
    Entry &e = queue_[(head_ + count_) % kQueueSize];
    e.code = code;
    snprintf(e.text, sizeof(e.text), "%s", text);
    count_++;
    esr_ |= EsrBitFor(code);
  }
  Unlock();
}

bool ScpiStatus::Pop(int16_t *code, char *text, size_t size) {
  Lock();
  bool any = count_ > 0;
  if (any) {
    *code = queue_[head_].code;
    snprintf(text, size, "%s", queue_[head_].text);
    head_ = (head_ + 1) % kQueueSize;
    count_--;
  }
  Unlock();
  return any;
}

size_t ScpiStatus::Count() {
  Lock();
  size_t n = count_;
  Unlock();
  return n;
}

void ScpiStatus::SetEsr(uint8_t bits) {
  Lock();
  esr_ |= bits;
  Unlock();
}

uint8_t ScpiStatus::TakeEsr() {
  Lock();
  uint8_t v = esr_;
  esr_ = 0;
  Unlock();
  return v;
}

uint8_t ScpiStatus::Stb() {
  Lock();
  uint8_t stb = 0;
  if (count_) stb |= 1 << 2;
  const uint16_t *e = enables_;
  if (ques_event_ & e[static_cast<int>(Enable::kQues)]) stb |= 1 << 3;
  if (esr_ & e[static_cast<int>(Enable::kEse)]) stb |= 1 << 5;
  if (oper_event_ & e[static_cast<int>(Enable::kOper)]) stb |= 1 << 7;
  if (stb & e[static_cast<int>(Enable::kSre)] & ~(1 << 6)) stb |= 1 << 6;
  Unlock();
  return stb;
}

uint16_t ScpiStatus::Get(Enable which) {
  Lock();
  uint16_t v = enables_[static_cast<int>(which)];
  Unlock();
  return v;
}

void ScpiStatus::Set(Enable which, uint16_t value) {
  Lock();
  enables_[static_cast<int>(which)] = value;
  Unlock();
}

uint16_t ScpiStatus::OperCondition() {
  Lock();
  uint16_t v = oper_cond_;
  Unlock();
  return v;
}

uint16_t ScpiStatus::TakeOperEvent() {
  Lock();
  uint16_t v = oper_event_;
  oper_event_ = 0;
  Unlock();
  return v;
}

void ScpiStatus::SetOperCondition(uint16_t mask, bool on) {
  Lock();
  uint16_t was = oper_cond_;
  oper_cond_ = on ? (oper_cond_ | mask) : (oper_cond_ & ~mask);
  oper_event_ |= oper_cond_ & ~was;
  Unlock();
}

uint16_t ScpiStatus::QuesCondition() {
  return log().WarningCount() ? kQuesWarning : 0;
}

uint16_t ScpiStatus::TakeQuesEvent() {
  Lock();
  uint16_t v = ques_event_;
  ques_event_ = 0;
  Unlock();
  return v;
}

void ScpiStatus::LatchQues(uint16_t bits) {
  Lock();
  ques_event_ |= bits;
  Unlock();
}

void ScpiStatus::Clear() {
  Lock();
  count_ = 0;
  head_ = 0;
  esr_ = 0;
  oper_event_ = 0;
  ques_event_ = 0;
  Unlock();
}

void ScpiStatus::Preset() {
  Lock();
  enables_[static_cast<int>(Enable::kOper)] = 0;
  enables_[static_cast<int>(Enable::kQues)] = 0;
  Unlock();
}

// -------------------------------------------------------------- ScpiService

void ScpiService::Initialise() {
  status_.Begin();
  // Warnings raised before SCPI existed (e.g. "Recovered from a watchdog
  // reset") are queued first; every later raise arrives through the listener.
  Warning active[kMaxWarnings];
  size_t n = log().Warnings(active, kMaxWarnings);
  for (size_t i = 0; i < n; i++) OnWarning(this, active[i]);
  log().SetWarningListener(OnWarning, this);
}

void ScpiService::OnWarning(void *self, const Warning &w) {
  ScpiService *s = static_cast<ScpiService *>(self);
  char text[ScpiStatus::kTextSize];
  snprintf(text, sizeof(text), "%s%s%s", w.message,
           w.supplementary[0] ? ": " : "", w.supplementary);
  s->status_.Push(static_cast<int16_t>(w.code), text);
  s->status_.LatchQues(ScpiStatus::kQuesWarning);
}

void ScpiService::Register(const char *header, ScpiHandler handler,
                           void *context, uint8_t flags) {
  switch (parser_.Add(header, handler, context, flags)) {
    case ScpiParser::AddResult::kOk:
      return;
    case ScpiParser::AddResult::kFull:
      FatalError(kScpiTableFull, header);
    case ScpiParser::AddResult::kCollision:
      FatalError(kScpiHashCollision, header);
    case ScpiParser::AddResult::kTooLong:
      FatalError(kScpiHeaderTooLong, header);
  }
}

void ScpiService::RegisterScpiCommands(ScpiRegistry *registry) {
  RegisterCoreScpiCommands(registry, this);
}

void ScpiService::RegisterServiceCommands() {
  ServiceHelper &all = services();
  for (size_t i = 0; i < all.count(); i++)
    all.At(i)->RegisterScpiCommands(this);
  LogInfof("%u SCPI commands, %u keywords",
           static_cast<unsigned>(parser_.count()),
           static_cast<unsigned>(parser_.token_count()));
}

void ScpiService::PushError(int16_t code, const char *info) {
  char text[ScpiStatus::kTextSize];
  snprintf(text, sizeof(text), "%s%s%s", ScpiErrorText(code),
           info && *info ? "; " : "", info ? info : "");
  status_.Push(code, text);
}

void ScpiService::Error(int16_t code, const char *info) {
  PushError(code, info);
}

bool ScpiService::Allowed(uint8_t flags) {
  return !(flags & kScpiProtected) || diag_unlocked_;
}

namespace {

// Responses go straight to the CDC port.
class CdcOutput : public Print {
 public:
  size_t write(uint8_t c) override { return usb_device().CdcWrite(&c, 1); }
  size_t write(const uint8_t *buf, size_t n) override {
    return usb_device().CdcWrite(buf, n);
  }
};

}  // namespace

void ScpiService::RunLine() {
  CdcOutput out;
  parser_.Execute(line_, &out, this);
  usb_device().CdcFlush();
}

void ScpiService::Feed(uint8_t c) {
  if (c == '\n') {
    if (line_len_ && line_[line_len_ - 1] == '\r') line_len_--;
    if (overrun_ || line_len_ > kMaxLine) {
      PushError(scpi_errors::kInputBufferOverrun);
    } else {
      line_[line_len_] = 0;
      RunLine();
    }
    line_len_ = 0;
    overrun_ = false;
    return;
  }
  if (overrun_) return;        // discard up to the end of the line
  if (line_len_ > kMaxLine) {  // room for kMaxLine characters and a CR
    overrun_ = true;
    return;
  }
  line_[line_len_++] = static_cast<char>(c);
}

void ScpiService::Tick() {
  UsbDeviceService &usb = usb_device();
  const bool open = usb.CdcSessionOpen();
  if (open != session_) {
    session_ = open;
    line_len_ = 0;
    overrun_ = false;
    LogInfo(open ? "Session opened: USB" : "Session released: USB");
  }
  if (!open) {
    usb.CdcDiscardInput();
    return;
  }
  uint8_t buf[64];
  size_t n;
  while ((n = usb.CdcRead(buf, sizeof(buf))) > 0)
    for (size_t i = 0; i < n; i++) Feed(buf[i]);
}

}  // namespace gte
