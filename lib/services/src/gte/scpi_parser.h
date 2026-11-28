// GTE's layer over the Vrekrer SCPI parser (pinned at d5354a0; issue #6).
// Vrekrer keeps the token table and the command hashes (long/short forms, case,
// numeric suffixes); this class replaces its Execute() to add what IEEE 488.2
// and SCPI-99 need:
//  - message units split on ';' outside quoted strings, with headers resolved
//    relative to the previous one (SCPI-99 6.2.4), or absolute after ':';
//  - parameters split on ',' outside quoted strings;
//  - -113 for any header that is not registered;
//  - a context pointer per command, and flags (e.g. DIAG protection);
//  - query responses joined with ';' and terminated once with LF.
// Registration rejects a full table and hash collisions.
#pragma once
#include <Print.h>

#include <cstddef>
#include <cstdint>

#include "gte/scpi.h"

#ifndef GTE_SCPI_PARSER_IMPL
#define VREKRER_SCPI_PARSER_NO_IMPL
#endif
#include <Vrekrer_scpi_parser.h>

namespace gte {

class ScpiParser : protected SCPI_Parser {
 public:
  enum class AddResult : uint8_t { kOk, kFull, kCollision, kTooLong };

  class Sink {
   public:
    virtual void Error(int16_t code, const char *info) = 0;
    virtual bool Allowed(uint8_t flags) = 0;  // false: -203

   protected:
    ~Sink() = default;
  };

  static constexpr size_t kMaxHeader = 96;

  AddResult Add(const char *header, ScpiHandler handler, void *context,
                uint8_t flags);
  // Run every message unit in `line` (changed in place).
  void Execute(char *line, Print *out, Sink *sink);

  size_t count() const { return codes_size_; }
  const char *header(size_t i) const { return headers_[i]; }
  size_t token_count() const { return tokens_size_; }
  const char *token(size_t i) const { return tokens_[i]; }

 private:
  int Find(const char *path);           // registered index, or -1
  int IndexOf(scpi_hash_t code) const;  // the valid entry with this hash, or -1
  void RunUnit(char *unit, char *prefix, size_t prefix_size, Print *out,
               bool *any_output, Sink *sink);

  char scratch_[kMaxHeader + 2] = {};
  const char *headers_[SCPI_MAX_COMMANDS] = {};
  ScpiHandler handlers_[SCPI_MAX_COMMANDS] = {};
  void *contexts_[SCPI_MAX_COMMANDS] = {};
  uint8_t flags_[SCPI_MAX_COMMANDS] = {};
};

}  // namespace gte
