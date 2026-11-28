// SCPI commands as Services see them (issues #6, #15). A Service registers its
// commands in RegisterScpiCommands(), once at startup, with a handler and a
// context pointer of its own; the SCPI Service parses each line and calls the
// handler with one ScpiCall per message unit.
//
// Document every command in an @scpi doc comment next to its registration:
// that comment is the only source of the SCPI manual (p0004-sw-003).
#pragma once
#include <Print.h>

#include <cstddef>
#include <cstdint>

namespace gte {

// SCPI-99 errors used by Core (SCPI-99 Vol 1, 21.8).
namespace scpi_errors {
constexpr int16_t kCommandError = -100;
constexpr int16_t kDataTypeError = -104;
constexpr int16_t kParameterNotAllowed = -108;
constexpr int16_t kMissingParameter = -109;
constexpr int16_t kUndefinedHeader = -113;
constexpr int16_t kHeaderSuffixOutOfRange = -114;
constexpr int16_t kCommandProtected = -203;
constexpr int16_t kSettingsConflict = -221;
constexpr int16_t kDataOutOfRange = -222;
constexpr int16_t kIllegalParameterValue = -224;
constexpr int16_t kQueueOverflow = -350;
constexpr int16_t kInputBufferOverrun = -363;
}  // namespace scpi_errors

const char *ScpiErrorText(int16_t code);  // the standard text for a code

constexpr size_t kScpiMaxParams = 8;
constexpr size_t kScpiMaxKeywords = 8;

class ScpiCall {
 public:
  void *context() const { return context_; }  // as given to Register()
  Print *out() const { return out_; }         // the response (one per query)

  // Header keywords as received, e.g. "TEST", "CHAN2", "VAL?".
  size_t keyword_count() const { return keyword_count_; }
  const char *keyword(size_t i) const {
    return i < keyword_count_ ? keywords_[i] : nullptr;
  }
  // The numeric suffix of keyword i (CHANnel2 -> 2), or `absent` if it has
  // none.
  int Suffix(size_t i, int absent = 1) const;

  // Parameters, unquoted (quoted strings lose their quotes; doubled quotes
  // inside become one).
  size_t param_count() const { return param_count_; }
  const char *param(size_t i) const {
    return i < param_count_ ? params_[i] : nullptr;
  }
  bool quoted(size_t i) const { return i < param_count_ && quoted_[i]; }

  // Parameter helpers: on failure they add the SCPI error (-109 missing, -104
  // wrong type, -222 out of range) and return false.
  bool IntegerParam(size_t i, int32_t min, int32_t max, int32_t *value);
  bool StringParam(size_t i, const char **value);  // must be a quoted string
  // Exactly `expected` parameters, else -109 (too few) or -108 (too many).
  bool ExpectParams(size_t expected);

  // Response helpers.
  void PrintQuoted(const char *text);  // "text", with embedded quotes doubled

  // Add an error to the error queue. `info` is appended after a semicolon.
  void Error(int16_t code, const char *info = nullptr);

  // For the parser.
  using ErrorSink = void (*)(void *sink, int16_t code, const char *info);
  void SetErrorSink(ErrorSink sink, void *context) {
    sink_ = sink;
    sink_context_ = context;
  }
  void SetContext(void *context) { context_ = context; }
  void SetOut(Print *out) { out_ = out; }
  void SetKeywords(char *const *k, size_t n);
  bool AddParam(char *text, bool was_quoted);  // false: too many

 private:
  void *context_ = nullptr;
  Print *out_ = nullptr;
  ErrorSink sink_ = nullptr;
  void *sink_context_ = nullptr;
  char *keywords_[kScpiMaxKeywords] = {};
  size_t keyword_count_ = 0;
  char *params_[kScpiMaxParams] = {};
  bool quoted_[kScpiMaxParams] = {};
  size_t param_count_ = 0;
};

using ScpiHandler = void (*)(ScpiCall *call);

// Registration flags.
constexpr uint8_t kScpiProtected = 1;  // needs DIAG:UNLock first (-203)

class ScpiRegistry {
 public:
  // Register one header in long form with its optional parts as separate
  // registrations, e.g. "SYSTem:ERRor:NEXT?" and "SYSTem:ERRor?". A full
  // table, a hash collision or a duplicate is a Fatal Error at boot.
  virtual void Register(const char *header, ScpiHandler handler,
                        void *context = nullptr, uint8_t flags = 0) = 0;

 protected:
  ~ScpiRegistry() = default;
};

}  // namespace gte
