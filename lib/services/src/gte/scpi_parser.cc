// The one translation unit that compiles Vrekrer's implementation.
#define GTE_SCPI_PARSER_IMPL
#include "gte/scpi_parser.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gte {

// ----------------------------------------------------------------- ScpiCall

const char *ScpiErrorText(int16_t code) {
  switch (code) {
    case 0:
      return "No error";
    case scpi_errors::kCommandError:
      return "Command error";
    case scpi_errors::kDataTypeError:
      return "Data type error";
    case scpi_errors::kParameterNotAllowed:
      return "Parameter not allowed";
    case scpi_errors::kMissingParameter:
      return "Missing parameter";
    case scpi_errors::kUndefinedHeader:
      return "Undefined header";
    case scpi_errors::kHeaderSuffixOutOfRange:
      return "Header suffix out of range";
    case scpi_errors::kCommandProtected:
      return "Command protected";
    case scpi_errors::kSettingsConflict:
      return "Settings conflict";
    case scpi_errors::kDataOutOfRange:
      return "Data out of range";
    case scpi_errors::kIllegalParameterValue:
      return "Illegal parameter value";
    case scpi_errors::kQueueOverflow:
      return "Queue overflow";
    case scpi_errors::kInputBufferOverrun:
      return "Input buffer overrun";
    default:
      return "Error";
  }
}

int ScpiCall::Suffix(size_t i, int absent) const {
  const char *k = keyword(i);
  if (!k) return absent;
  size_t n = strlen(k);
  if (n && k[n - 1] == '?') n--;
  size_t start = n;
  while (start > 0 && isdigit(static_cast<unsigned char>(k[start - 1])))
    start--;
  if (start == n) return absent;
  return atoi(k + start);
}

void ScpiCall::Error(int16_t code, const char *info) {
  if (sink_) sink_(sink_context_, code, info);
}

bool ScpiCall::ExpectParams(size_t expected) {
  if (param_count_ < expected) {
    Error(scpi_errors::kMissingParameter);
    return false;
  }
  if (param_count_ > expected) {
    Error(scpi_errors::kParameterNotAllowed);
    return false;
  }
  return true;
}

bool ScpiCall::IntegerParam(size_t i, int32_t min, int32_t max,
                            int32_t *value) {
  const char *p = param(i);
  if (!p) {
    Error(scpi_errors::kMissingParameter);
    return false;
  }
  char *end = nullptr;
  errno = 0;
  long long n = strtoll(p, &end, 10);  // NOLINT(runtime/int): strtoll's type
  if (quoted(i) || !*p || *end || errno == ERANGE) {
    Error(scpi_errors::kDataTypeError);
    return false;
  }
  if (n < min || n > max) {
    Error(scpi_errors::kDataOutOfRange);
    return false;
  }
  *value = static_cast<int32_t>(n);
  return true;
}

bool ScpiCall::StringParam(size_t i, const char **value) {
  const char *p = param(i);
  if (!p) {
    Error(scpi_errors::kMissingParameter);
    return false;
  }
  if (!quoted(i)) {
    Error(scpi_errors::kDataTypeError);
    return false;
  }
  *value = p;
  return true;
}

void ScpiCall::PrintQuoted(const char *text) {
  out_->write('"');
  for (const char *c = text; *c; c++) {
    if (*c == '"') out_->write('"');
    out_->write(*c);
  }
  out_->write('"');
}

void ScpiCall::SetKeywords(char *const *k, size_t n) {
  keyword_count_ = n < kScpiMaxKeywords ? n : kScpiMaxKeywords;
  for (size_t i = 0; i < keyword_count_; i++) keywords_[i] = k[i];
}

bool ScpiCall::AddParam(char *text, bool was_quoted) {
  if (param_count_ >= kScpiMaxParams) return false;
  params_[param_count_] = text;
  quoted_[param_count_] = was_quoted;
  param_count_++;
  return true;
}

// --------------------------------------------------------------- ScpiParser

namespace {

// Responses from each query in a line, joined with ';'.
class UnitOutput : public Print {
 public:
  UnitOutput(Print *out, bool *any) : out_(out), any_(any) {}
  size_t write(uint8_t c) override {
    if (!started_) {
      if (*any_) out_->write(';');
      started_ = true;
      *any_ = true;
    }
    return out_->write(c);
  }
  size_t write(const uint8_t *buf, size_t n) override {
    if (!n) return 0;
    write(buf[0]);
    return 1 + out_->write(buf + 1, n - 1);
  }

 private:
  Print *out_;
  bool *any_;
  bool started_ = false;
};

// The first `sep` in s that is outside a quoted string, or nullptr.
char *FindUnquoted(char *s, char sep) {
  char quote = 0;
  for (; *s; s++) {
    if (quote) {
      if (*s == quote) quote = 0;  // a doubled quote reopens at once
    } else if (*s == '"' || *s == '\'') {
      quote = *s;
    } else if (*s == sep) {
      return s;
    }
  }
  return nullptr;
}

char *Trim(char *s) {
  while (isspace(static_cast<unsigned char>(*s))) s++;
  size_t n = strlen(s);
  while (n && isspace(static_cast<unsigned char>(s[n - 1]))) s[--n] = 0;
  return s;
}

// Strip a string's quotes in place, turning doubled quotes into one.
bool Unquote(char *s) {
  size_t n = strlen(s);
  char q = s[0];
  if (n < 2 || (q != '"' && q != '\'') || s[n - 1] != q) return false;
  size_t o = 0;
  for (size_t i = 1; i + 1 < n; i++) {
    s[o++] = s[i];
    if (s[i] == q && s[i + 1] == q) i++;
  }
  s[o] = 0;
  return true;
}

void IgnoreCall(SCPI_C, SCPI_P, Stream &) {}

}  // namespace

ScpiParser::AddResult ScpiParser::Add(const char *header, ScpiHandler handler,
                                      void *context, uint8_t flags) {
  if (strlen(header) > kMaxHeader) return AddResult::kTooLong;
  if (codes_size_ >= max_commands) return AddResult::kFull;
  snprintf(scratch_, sizeof(scratch_), "%s", header);
  ResetCommandTreeBase_();
  const CommandCode c = PrepareCommand_(scratch_);
  if (c.invalid || setup_errors.token_overflow) return AddResult::kFull;
  if (IndexOf(c.code) >= 0) return AddResult::kCollision;
  registered_command_codes_[codes_size_] = c.code;
  registered_command_validity_[codes_size_] = true;
  callers_[codes_size_] = IgnoreCall;
  headers_[codes_size_] = strdup(header);
  handlers_[codes_size_] = handler;
  contexts_[codes_size_] = context;
  flags_[codes_size_] = flags;
  codes_size_++;
  return AddResult::kOk;
}

int ScpiParser::Find(const char *path) {
  if (strlen(path) > kMaxHeader) return -1;
  snprintf(scratch_, sizeof(scratch_), "%s", path);
  SCPI_Commands keywords(scratch_);
  if (keywords.overflow_error) return -1;
  ResetCommandTreeBase_();
  const CommandCode c = GetCommandCode_(keywords);
  if (c.invalid) return -1;
  return IndexOf(c.code);
}

int ScpiParser::IndexOf(scpi_hash_t code) const {
  for (uint8_t i = 0; i < codes_size_; i++) {
    if (registered_command_validity_[i] &&
        registered_command_codes_[i] == code) {
      return i;
    }
  }
  return -1;
}

void ScpiParser::Execute(char *line, Print *out, Sink *sink) {
  char prefix[kMaxHeader + 2] = "";
  bool any_output = false;
  char *unit = line;
  while (unit) {
    char *next = FindUnquoted(unit, ';');
    if (next) *next++ = 0;
    RunUnit(unit, prefix, sizeof(prefix), out, &any_output, sink);
    unit = next;
  }
  if (any_output) out->write('\n');
}

void ScpiParser::RunUnit(char *unit, char *prefix, size_t prefix_size,
                         Print *out, bool *any_output, Sink *sink) {
  while (isspace(static_cast<unsigned char>(*unit))) unit++;
  if (!*unit) return;
  char *header = unit;
  char *rest = header;
  while (*rest && !isspace(static_cast<unsigned char>(*rest))) rest++;
  if (*rest) *rest++ = 0;
  const bool absolute = *header == ':';
  if (absolute) header++;
  const bool common = *header == '*';

  char path[kMaxHeader + 2];
  int index = -1;
  if (!absolute && !common && prefix[0]) {
    // SCPI-99 6.2.4: relative to the previous header's level; then, to be
    // forgiving, as an absolute header.
    snprintf(path, sizeof(path), "%s%s", prefix, header);
    index = Find(path);
  }
  if (index < 0) {
    snprintf(path, sizeof(path), "%s", header);
    index = Find(path);
  }
  if (index < 0) {
    sink->Error(scpi_errors::kUndefinedHeader, nullptr);
    return;
  }
  if (!common) {
    const char *colon = strrchr(path, ':');
    size_t n = colon ? static_cast<size_t>(colon - path) + 1 : 0;
    if (n >= prefix_size) n = 0;
    memcpy(prefix, path, n);
    prefix[n] = 0;
  }

  ScpiCall call;
  call.SetErrorSink(
      [](void *s, int16_t code, const char *info) {
        static_cast<Sink *>(s)->Error(code, info);
      },
      sink);
  // Keywords as received (path is ours now: split it in place).
  char *keywords[kScpiMaxKeywords];
  size_t nk = 0;
  for (char *k = path; *k && nk < kScpiMaxKeywords;) {
    keywords[nk++] = k;
    char *colon = strchr(k, ':');
    if (!colon) break;
    *colon = 0;
    k = colon + 1;
  }
  call.SetKeywords(keywords, nk);

  rest = Trim(rest);
  if (*rest) {
    for (char *p = rest; p;) {
      char *comma = FindUnquoted(p, ',');
      if (comma) *comma++ = 0;
      char *text = Trim(p);
      bool was_quoted = Unquote(text);
      if (!call.AddParam(text, was_quoted)) {
        sink->Error(scpi_errors::kParameterNotAllowed, nullptr);
        return;
      }
      p = comma;
    }
  }

  if (!sink->Allowed(flags_[index])) {
    sink->Error(scpi_errors::kCommandProtected, nullptr);
    return;
  }
  UnitOutput unit_out(out, any_output);
  call.SetOut(&unit_out);
  call.SetContext(contexts_[index]);
  handlers_[index](&call);
}

}  // namespace gte
