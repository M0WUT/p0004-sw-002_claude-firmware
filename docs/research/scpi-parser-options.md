# Vrekrer_scpi_parser in GTE Core: fit, limits, and PyVISA requirements

Status: research note, 2026-11-24. Ticket: "SCPI parser library options".

**Decision already made:** GTE Core uses
[Vrekrer_scpi_parser](https://github.com/Vrekrer/Vrekrer_scpi_parser). This note
covers how that library fits the GTE Core design. That design has two transports
(USB CDC and TCP 5025 on a W5500), commands registered at runtime by Services the
core does not know about at compile time, and generated commands for every Config
Parameter and Status Value. The note also lists what PyVISA expects from the
instrument.

Every claim about the library refers to tag **v0.6.1**, commit
[`d5354a0`](https://github.com/Vrekrer/Vrekrer_scpi_parser/commit/d5354a0251510decaf6a5c90b8235e37c0f885a3).
The files read are
[`Vrekrer_scpi_parser.h`](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser.h),
[`Vrekrer_scpi_parser_code.h`](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser_code.h),
[`Vrekrer_scpi_arrays_code.h`](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_arrays_code.h)
and
[`Vrekrer_scpi_parser_special_code.h`](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser_special_code.h).
Behaviour marked **(verified)** was reproduced with a host-side g++ build of those
headers against a minimal `Arduino.h`/`Stream` stub. That test harness is not
committed.

---

## TL;DR

- **Runtime registration works.** `RegisterCommand()` can be called at any time,
  and new tokens never change the hashes of commands already registered. The catch
  is that every table is a fixed-size array whose size is set at compile time.
  All the counters are `uint8_t`, so **no limit can exceed 255.** Generating
  separate set and query commands for every Config Parameter will hit that
  ceiling. Generic, parameterised commands avoid it (see §1.4).
- **Set `SCPI_HASH_TYPE=uint32_t`.** With the default 8-bit hash, even 20
  realistic commands produced colliding hashes **(verified)**. When two commands
  collide, the first one registered is dispatched for both, without any error.
- **Use one parser instance and your own line reader per transport.** Do not use
  `ProcessInput()`/`GetMessage()`: they hold one shared line buffer, a 10 ms
  inter-byte timeout and a buffer of at most 255 bytes. Instead, assemble lines
  per transport (CDC, and each `EthernetClient`) and call
  `Execute(line, stream)`. The callback receives that `Stream&`, so each response
  goes back to the transport the command came from.
- **IEEE 488.2 and SCPI-99 support is absent.** The library has no common
  commands, no status registers and no error queue. Its only error reporting is
  an `ErrorCode` enum (`UnknownCommand`, `Timeout`, `BufferOverflow`) passed to
  one error callback. A string of known tokens that is not a registered command
  is **silently ignored (verified)**. Examples are `FREQ:STOP?` when only
  `FREQ:STOP` is registered, and the relative header in `FREQ:STAR 3;STOP 5`.
  GTE Core needs a thin subclass that overrides `Execute()` (all internals are
  `protected`). The subclass would add the missing error, SCPI-99 header-path
  compounding, quote-aware splitting and a per-command context pointer.
- **Licence, footprint and maintenance:**
  - **Licence:** MIT.
  - **RAM:** about 0.3 KB per instance at the defaults. About 3.6 KB at the
    maximum (255/255/uint32) on a 32-bit target, plus heap for token strings.
  - **Flash:** small (header-only), not measured.
  - **Maintenance:** one maintainer. Little changed between Jan 2024 and Nov 2026,
    then the hashing and matching code was reworked in a burst of commits
    (Nov 19–23 2026). Pin **v0.6.1 / `d5354a0`**.
- **PyVISA expects the following.** Set `read_termination = write_termination =
  "\n"` in clients. The firmware must also accept the PyVISA default
  `write_termination` of `"\r\n"`, and it must answer every query with exactly
  one response line ending in `\n`.
  - **TCPIP SOCKET:** nothing ends a read except the termination character, so a
    missing `\n` gives a timeout (2000 ms by default).
  - **ASRL:** baud rate is ignored on CDC, but **DTR must be asserted** or
    STM32duino drops all output.
  - **Discovery:** ASRL resources are found by enumerating OS serial ports.
    SOCKET resources are found only by mDNS `_scpi-raw._tcp` (pyvisa-py with
    `zeroconf`).

---

## 1. Runtime registration and compile-time limits

### 1.1 Registration is allowed at any time

The source places no restriction on when `RegisterCommand()` is called.
`PrepareCommand_()` splits the path at `:` and calls `AddToken_()` for each
keyword. `AddToken_()` de-duplicates tokens and appends new ones, allocating each
with `new char[]`. The command hash is then computed from the *indices* of the
matched tokens, and the code, validity flag and callback are appended to parallel
arrays
([`RegisterCommand`, `AddToken_`, `GetCommandCode_`](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser_code.h)).
Tokens are only ever appended, so a token's index never changes. As a result, a
Service that registers commands late does **not** invalidate any earlier hash.

The GTE design must handle these caveats:

| Caveat | Consequence for GTE Core |
|---|---|
| **No unregister.** Token strings are heap-allocated and never freed ([open PR #44](https://github.com/Vrekrer/Vrekrer_scpi_parser/pull/44) proposes static storage). | Services should register once, at Service start. A Service that is created and destroyed repeatedly would leak memory and exhaust the slots. |
| **`RegisterCommand(const char*)` and `(const __FlashStringHelper*)` copy into `msg_buffer_` with an unbounded `strcpy`**, and `SetCommandTreeBase(const char*)` does the same ([open PR #45](https://github.com/Vrekrer/Vrekrer_scpi_parser/pull/45) adds bounds checks). | Paths must be shorter than `SCPI_BUFFER_LENGTH`. Registering while `GetMessage()` holds a partial line corrupts that line. Both problems go away if GTE does not use `GetMessage()`/`ProcessInput()` (see §2), or if it calls the `char*` overload with its own scratch buffer. Note that the `char*` overload is modified in place by `strtok`. |
| **`SetCommandTreeBase()` is hidden mutable state.** `Execute()` resets it to the root. | Services should register **full paths** and not use `SetCommandTreeBase()`. This keeps one Service's registrations from changing another's. |
| **Callbacks are bare function pointers**: `using SCPI_caller_t = void(*)(SCPI_Commands, SCPI_Parameters, Stream&)`. There is no user-data argument, and the callback is not told the index of the command that matched. | A Service object cannot be reached from its callback without extra work. Add the context in a subclass: keep a parallel `void* ctx_[SCPI_MAX_COMMANDS]` (or an `IScpiHandler*` table) at the same index as `callers_`, and dispatch from an overridden `Execute()` that knows the index. The alternative is one static trampoline per command, which does not suit generated commands. |
| **Registration failures are recorded only in `setup_errors` (protected), and are visible only through `PrintDebugInfo()`.** | The subclass should expose `setup_errors` and a hash-collision check, so that GTE Core can fail loudly at boot or at Service start. |
| **Not thread-safe.** The parser uses `strtok` (which keeps global state), and `tree_code_`/`last_error` are shared. | Call it only from the single cooperative loop, or put a mutex around `Execute()` if an RTOS is ever used. |

### 1.2 The compile-time limits

These macros are defined in the header, each inside an `#ifndef` guard
([header](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser.h)).
Note that the README and the ticket spell one of them `SCPI_ARRAY_SYZE`; the
actual macro is `SCPI_ARRAY_SIZE`.

| Macro | Default | Meaning | Hard ceiling |
|---|---|---|---|
| `SCPI_ARRAY_SIZE` | 6 | Maximum number of keywords in one command path, *and* maximum number of parameters in one message unit. | 255 (`uint8_t size_`) |
| `SCPI_MAX_TOKENS` | 15 | Number of distinct keywords across all commands. | **255** (`const uint8_t max_tokens`, `uint8_t tokens_size_`) |
| `SCPI_MAX_COMMANDS` | 20 | Number of registered commands. Set and query count separately (`X` and `X?` are two entries). | **255** (`const uint8_t max_commands`, `uint8_t codes_size_`) |
| `SCPI_MAX_SPECIAL_COMMANDS` | 0 | Number of "special" commands, which are dispatched as soon as the header arrives, for raw or large data. | 255 |
| `SCPI_BUFFER_LENGTH` | 64 | Line buffer used by `GetMessage()`, and scratch space for the `const char*` registration overloads. | **255** (`const uint8_t buffer_length`, `uint8_t message_length_`) |
| `SCPI_HASH_TYPE` | `uint8_t` | Width of the command hash. | `uint32_t` (or `uint64_t`) |

**Values above 255 truncate silently (verified).** Building with
`-DSCPI_MAX_COMMANDS=300` sets `max_commands` to `300 & 0xFF = 44`, so command
overflow starts at 44 registrations. If `SCPI_BUFFER_LENGTH` were 256,
`buffer_length` would be 0, and every byte received would be reported as an
overflow. Arrays are still sized from the macro, so RAM is spent for nothing.

### 1.3 Hash collisions

The hash is computed as follows ([`GetCommandCode_`](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser_code.h)):

1. Start with `code = hash_magic_offset` (7).
2. For each keyword, `code = code*37 + token_index`.
3. For a query, also apply `code = code*37 - 1`.

All arithmetic is modulo the width of `SCPI_HASH_TYPE`. `Execute()` dispatches the
**first** registered entry whose code matches. When two commands collide, the
second can never be reached, and its input runs the first command's callback
without any error being raised.

Results from a test that registered generated `CONFig:<svc>:<param>`,
`CONFig:<svc>:<param>?` and `STATus:<svc>:<param>?` commands **(verified)**:

| Commands | Hash type | Colliding entries flagged by `PrintDebugInfo` |
|---|---|---|
| 20 | `uint8_t` (default) | 2 |
| 40 | `uint8_t` | 4 |
| 150 | `uint8_t` | 32 |
| 150 | `uint16_t` | 0 |
| 150 | `uint32_t` | 0 |

**Use `SCPI_HASH_TYPE=uint32_t`.** For random 32-bit hashes, the chance of any
collision among 255 commands is about 255²/2³³ ≈ 8×10⁻⁶. That is small but not
zero, so the GTE subclass should still check each new code against the existing
ones at registration time. A clash should then be rejected loudly rather than
dispatched to the wrong command.

### 1.4 Command budget for generated commands

The 255-command ceiling is the main constraint on the design. Suppose *N*
Config Parameters each get a set and a query command, and *M* Status Values each
get a query. That uses `2N + M` slots, plus about 15 slots for the mandatory
common and SCPI commands (§3). Instrument Services also need slots. With these
numbers the budget runs out at around 100 parameters. The table of distinct
tokens fills up too, because every parameter name is a token.

There are two ways out:

- **(a) Recommended: parameterised generic commands.** The parameter name
  becomes a SCPI parameter rather than part of the header. Examples are
  `CONFig:VALue <service>,<param>,<value>`, `CONFig:VALue? <service>,<param>`,
  `STATus:VALue? <service>,<value>` and `CONFig:CATalog?`. That is a handful of
  slots in total, however many Config Parameters exist, and it keeps the core
  independent of which Services are present.
  Two things to watch: with `SCPI_ARRAY_SIZE` at 6, a message unit can carry at
  most 6 parameters, and parameter strings must not contain commas (§2.4).
- **(b) Patch or fork the library** to make the counters `uint16_t`. The change is
  mechanical, but it means GTE Core then maintains a fork. It is also possible to
  combine (a) for Config Parameters and Status Values with per-command headers
  for Instrument Services.

### 1.5 Overriding the limits from PlatformIO

The library is header-only. The implementation is pulled in by including
`Vrekrer_scpi_parser.h`, unless `VREKRER_SCPI_PARSER_NO_IMPL` is defined
([header comment](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser.h);
[issue #39](https://github.com/Vrekrer/Vrekrer_scpi_parser/issues/39) shows the
multiple-definition error you get otherwise). The class layout depends on the
macros, so **every translation unit must see the same values.** Otherwise the
program breaks the One Definition Rule and fails at run time without any warning
at build time. Use the global `build_flags`, not `build_src_flags`. The latter
applies only to `src_dir`
([PlatformIO `build_src_flags`](https://docs.platformio.org/en/latest/projectconf/sections/env/options/build/build_src_flags.html),
[`build_flags`](https://docs.platformio.org/en/latest/projectconf/sections/env/options/build/build_flags.html)).

```ini
[env:gte_core]
lib_deps =
    ; Registry package (owner "vrekrer", v0.6.1 released 2026-11-23)
    vrekrer/Vrekrer SCPI parser@0.6.1
    ; or pin the exact commit:
    ; https://github.com/Vrekrer/Vrekrer_scpi_parser.git#d5354a0251510decaf6a5c90b8235e37c0f885a3
build_flags =
    -D SCPI_HASH_TYPE=uint32_t
    -D SCPI_MAX_COMMANDS=128     ; <= 255
    -D SCPI_MAX_TOKENS=128       ; <= 255
    -D SCPI_ARRAY_SIZE=8         ; keywords per path and params per unit
    -D SCPI_BUFFER_LENGTH=128    ; <= 255; scratch space only if GetMessage() is not used
```

Include the header *with* the implementation in exactly one `.cpp` file. In every
other file, include it through a GTE wrapper header that defines
`VREKRER_SCPI_PARSER_NO_IMPL` first. The registry entry was confirmed through the
PlatformIO registry API: owner `vrekrer`, version 0.6.1, released
2026-11-23T17:54:55Z.

---

## 2. Two transports: one instance or two?

### 2.1 What the library keeps per instance

`GetMessage()`/`ProcessInput()` keep **one** `msg_buffer_`, one
`message_length_` and one `time_checker_` per `SCPI_Parser`
([source](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/d5354a0251510decaf6a5c90b8235e37c0f885a3/src/Vrekrer_scpi_parser_code.h)).
If CDC and TCP alternate on the same instance, their partial lines are mixed
together in that one buffer. `Execute(char*, Stream&)` has no state that lasts
between calls, apart from `tree_code_` (reset on every call) and `last_error`. Every
callback receives the `Stream&` that was passed to `Execute`, so responses
naturally go back to the transport the command came from.

Two ways to serve both transports:

- **Two `SCPI_Parser` instances**, one per transport. This works, but every
  Service has to register on both, which doubles the token heap and the tables.
  The error-queue and status state would also have to be shared outside the
  library anyway.
- **Recommended: one instance, with a GTE line reader per transport.** One
  reader for `SerialUSB`, and one for each accepted `EthernetClient`. Each reader
  calls `parser.Execute(line, stream)`. This also avoids all the `GetMessage()`
  problems listed below. The W5500 supports up to 8 sockets (`MAX_SOCK_NUM 8`)
  ([Ethernet.h](https://github.com/arduino-libraries/Ethernet/blob/master/src/Ethernet.h)),
  so the number of simultaneous clients on port 5025 should be capped at 1–2,
  each with its own line buffer.

`EthernetServer::accept()` returns each new connection once, and
`EthernetServer::available()` returns any client that has data
([Ethernet.h](https://github.com/arduino-libraries/Ethernet/blob/master/src/Ethernet.h)).
Use `accept()` so that per-client buffers can be tracked. Wrap the `EthernetClient`
in a buffering `Stream`, so that the many small `print()` calls a callback makes
are sent as one TCP write instead of many tiny segments.

### 2.2 How `GetMessage()` behaves, and why to avoid it

These points come from the source and were **(verified)** with the host build:

- **Partial lines.** Bytes are added to the buffer across calls. If no byte
  arrives for `timeout` milliseconds (**10 ms by default**), the partial line is
  thrown away and the error handler is called with `ErrorCode::Timeout`. A
  TCP stream split into separate segments, or a person typing into a terminal,
  will trip this.
- **Terminator.** The terminator test is `strstr(msg_buffer_, term_chars)`, an
  exact match on the whole string. A `"\r\n"` terminator never matches a sender
  that sends only `\n` (see
  [issue #35](https://github.com/Vrekrer/Vrekrer_scpi_parser/issues/35)). Since
  v0.6.0 ([commit 879c576](https://github.com/Vrekrer/Vrekrer_scpi_parser/commit/879c576205ab76a7c898b002e5dc6a4db704fbcc)),
  `\r` counts as a header separator, so a `"\n"` terminator now copes with
  CRLF senders such as PyVISA's default **(verified)**. However, a trailing `\r`
  stays on the last parameter (for example `"5\r"`) unless whitespace is stripped.
- **Overflow.** When the buffer fills, the handler is called with
  `BufferOverflow` and the buffer is reset, but the *rest of that line* is then
  read as a new message. In the test, a 70-byte line produced `[BufferOverflow]`
  followed by `[UnknownCommand]` from its tail. The overflow does not discard
  input up to the next terminator, and the buffer can never exceed 255 bytes.

**The GTE line reader should:**

1. Accept `\n` as the terminator and strip one trailing `\r`.
2. On overflow, discard input up to the next `\n`, then queue **-363 "Input
   buffer overrun"** once.
3. Apply no inter-byte timeout, or a long one such as seconds. An optional
   idle-close for TCP is fine.
4. Size its buffer for the largest expected message, independently of
   `SCPI_BUFFER_LENGTH`.

The `SCPI-99` error numbers (−363 "Input buffer overrun", −365 "Time out error")
are listed in
[SCPI-99 Vol 1, §21.8.2](https://www.ivifoundation.org/downloads/SCPI/scpi-99.pdf).

### 2.3 Gaps in `Execute()` itself (verified)

| Input | Registered | Result | Required by the standard |
|---|---|---|---|
| `BOGUS 1` | — | error handler, `UnknownCommand` | −113 Undefined header |
| `FREQ:STOP?` | `FREQ:STOP` only | **nothing** (no callback, no error) | −113 Undefined header. Over SOCKET, the client also waits until its read times out. |
| `STOP:FREQ 1` | `FREQ:STOP` | **nothing** | −113 |
| `POW` | `POW?` only | **nothing** | −113 |
| `FREQ:STAR 3;STOP 5` | both | only `STAR` runs; `STOP` is **silently dropped** | SCPI-99 §6.2.4 requires `STOP` to be resolved relative to the previous header level (`FREQ:`) |
| `FREQ:STAR 3;:FREQ:STOP 5` | both | both run | same |

Cause: `Execute()` calls `ResetCommandTreeBase_()` for every unit separated by `;`.
The error handler is called only when `GetCommandCode_` cannot match a token. If
every token matches but no registered code does, the loop simply ends without
dispatching anything.
[SCPI-99 Vol 1 §4.1](https://www.ivifoundation.org/downloads/SCPI/scpi-99.pdf)
requires compound command and query headers to be parsed. §6.2.4 says
"Subsequent commands … are referenced to the same tree level as the previous
command in a message unit".

### 2.4 Parameter splitting

`SCPI_Parameters` splits on every `,` using `strtok`, and the source has the
comment `// TODO: Preserve commas inside quoted string parameters`. `Execute()`
splits on every `;`. **(verified):** `POW? "a,b",c` gives three parameters, `"a`,
`b"` and `c`. Parameters are plain text, and the callback parses them itself. If
there are more than `SCPI_ARRAY_SIZE` parameters, `overflow_error` is set and the
extra ones are dropped, so callbacks must check `parameters.overflow_error`.

### 2.5 The GTE subclass

All of these gaps can be closed without forking, because every member is
`protected` and `Execute()` can be redefined in a subclass (it is not virtual, so
GTE code must call the subclass method). The subclass would be about 100 lines:

- An `Execute(char*, Stream&)` that:
  - splits on `;` and `,` outside quoted strings;
  - keeps the SCPI-99 header path between units;
  - raises −113 when no command matches;
  - dispatches through a `ctx_[]` table that is indexed in parallel with
    `callers_`.
- `RegisterCommand(path, handler, ctx)` that rejects hash collisions and slot
  overflow, and returns success or failure.
- An error handler that maps `ErrorCode` values into the GTE error queue.

---

## 3. IEEE 488.2 and SCPI-99: what the library provides and what GTE Core must build

**The library provides none of it.** The header contains no common commands, no
status registers, no error queue and no `*IDN?`. The README describes only
parsing features and "Automatic Stream communication errors handling (timeout,
buffer overflow)"
([README](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/README.md)).
The single error callback is told only `last_error ∈ {NoError, UnknownCommand,
Timeout, BufferOverflow}`.

Requirements that GTE Core must implement, from
[SCPI-99 Vol 1](https://www.ivifoundation.org/downloads/SCPI/scpi-99.pdf):

| Item | Source | Notes for implementation |
|---|---|---|
| `*CLS`, `*ESE`, `*ESE?`, `*ESR?`, `*IDN?`, `*OPC`, `*OPC?`, `*RST`, `*SRE`, `*SRE?`, `*STB?`, `*TST?`, `*WAI` | §4.1.1 "IEEE Mandated Commands" (each refers to an IEEE 488.2 §10.x) | 13 command slots. The `*` tokens use their long form only (in `AddToken_`, the short-form length counts leading upper-case letters, and `*` is not upper-case), which is correct. |
| `SYSTem:ERRor[:NEXT]?`, `SYSTem:VERSion?` | §4.2.1 "Required Commands" | The response format is `<code>,"<text>"`. When the queue is empty, the response is `0,"No error"` (§21.8). |
| `STATus:OPERation[:EVENt]?`, `:CONDition?`, `:ENABle`, `:ENABle?`; the same four for `STATus:QUEStionable`; and `STATus:PRESet` | §4.2.1 | About 9 more slots. These registers feed the summary bits of the Status Byte. |
| Error/event queue | §21.8.1 | First in, first out. Minimum depth 2. When it overflows, the *most recent* entry is replaced by `-350,"Queue overflow"` and the oldest entries are kept. Reading an entry removes it. |
| Header compounding | §4.1, §6.2.4 | See §2.3. Must be added in the subclass. |
| `*IDN?` content | §4.1.3.6 | IEEE 488.2 defines four comma-separated fields. SCPI-99 recommends that field 1 (manufacturer) is the same for every product, that field 2 (model) does not contain the word "MODEL", and that field 4 (firmware) covers every separately revisable subsystem. Field 3 is the serial number; IEEE 488.2 §10.14 itself is paywalled and was not read for this note. |
| Error-code mapping for the library's `ErrorCode` values | §21.8.2 | `UnknownCommand`→−113 "Undefined header"; `BufferOverflow`→−363 "Input buffer overrun"; `Timeout`→−365 "Time out error" (only if `GetMessage` is used). Parameter errors are −102/−108/−109/−220…−224, raised by the callbacks. |

Decision for GTE Core: are status and errors per transport or per instrument?
IEEE 488.2 models a single device. The simplest choice that conforms is **one
shared status model and one shared error queue**, with `Stream&` used only to
route responses. Response format: each query writes one `<response>\n`. If several
queries arrive in one message (`A?;B?`), IEEE 488.2 expects their responses to be
joined with `;` and terminated once. The subclass can do this by giving the
callbacks a response-collecting `Stream` and terminating once at the end of
`Execute`.

---

## 4. Licence, footprint and maintenance

**Licence.** MIT, Copyright (c) 2018 Diego González Chávez
([LICENSE](https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/LICENSE)).

**RAM**, from the header layout, for a 32-bit Cortex-M7 (4-byte pointers):

- Fixed part: `msg_buffer_[SCPI_BUFFER_LENGTH]`, plus `tokens_[MAX_TOKENS]`
  pointers, plus `registered_command_codes_[MAX_COMMANDS] × sizeof(hash)`, plus
  `registered_command_validity_[MAX_COMMANDS]`, plus
  `callers_[MAX_COMMANDS+1]` function pointers, plus about 30 bytes of scalars.
- At the defaults (64/15/20/uint8): about **280 bytes** per instance. The host
  64-bit build measured `sizeof(SCPI_Parser)` = 440 bytes.
- With 255/255/255/uint32: about **3.6 KB** per instance (the 64-bit host build
  measured 5672 bytes).
- Heap: each distinct token costs its length + 1 bytes, plus malloc overhead.
- Stack: `SCPI_Commands` and `SCPI_Parameters` are passed **by value** to every
  callback. Each is about `SCPI_ARRAY_SIZE×4 + 8` bytes on 32-bit.

All of this is negligible against the STM32H743's 1 MB of RAM.

**Flash.** Not measured, because no ARM toolchain was available in the research
environment. The code is a few hundred lines of C++ using only `strtok`,
`strncasecmp`, `strpbrk` and `strstr`. `PrintDebugInfo()` pulls in `Print`
formatting, but the linker drops it if it is never called. To measure, run
`pio run -v` before and after adding the library.

**Maintenance.** Figures from the
[GitHub API](https://api.github.com/repos/Vrekrer/Vrekrer_scpi_parser), taken
2026-11-24:

- 118 stars, 36 forks, not archived, one maintainer.
- Tags: v0.3, v0.4, 0.4.1, v0.4.2, v0.5.0, v0.6.0, v0.6.1. There are no GitHub
  Releases, only tags.
- Commit history: before the recent burst, the last commit was 2024-01-03
  (merge of PR #34). From 2026-11-19 to 2026-11-23, 14 commits reworked command
  hashing and validity tracking, numeric-suffix matching, setup error reporting,
  and CR as a header separator. They ended with v0.6.0 (`879c576`) and v0.6.1
  (`d5354a0`, a change to `library.properties` only).
- `VREKRER_SCPI_VERSION` in the header still reads `"v0.5.0"`, so it is stale.
  Do not use it to check the version.
- Open items relevant to GTE Core: PR #44 (token heap leak), PR #45 (bounds
  checks in the `strcpy` registration overloads), PR #46 (hash-to-0/1,
  setup-error reporting, CRLF), issue #37 (overflow reporting), issue #35 (CRLF).
- The internals changed a lot between v0.5.0 and v0.6.x, and the GTE subclass
  depends on `protected` members. **Pin `d5354a0`**, and re-check the subclass
  whenever the pin is moved.

**Why the alternatives were not chosen** (for the record only).
[j123b567/scpi-parser](https://github.com/j123b567/scpi-parser) (BSD-2-Clause)
provides all the 488.2 common commands, the registers and the error queue
([`ieee488.h`](https://github.com/j123b567/scpi-parser/blob/master/libscpi/inc/scpi/ieee488.h)).
It keeps per-context state in `scpi_t`, including `user_context`, and its command
table is a `const scpi_command_t*` passed to `SCPI_Init`
([`types.h`](https://github.com/j123b567/scpi-parser/blob/master/libscpi/inc/scpi/types.h),
[`parser.h`](https://github.com/j123b567/scpi-parser/blob/master/libscpi/inc/scpi/parser.h)).
The table is designed to be a static array, although it could be built in RAM
before `SCPI_Init`. The project chose Vrekrer anyway, for its simpler C++/`Stream`
API.

---

## 5. What PyVISA expects

Sources are the PyVISA `main` branch:
[`resources/messagebased.py`](https://github.com/pyvisa/pyvisa/blob/main/pyvisa/resources/messagebased.py),
[`attributes.py`](https://github.com/pyvisa/pyvisa/blob/main/pyvisa/attributes.py),
[`resources/serial.py`](https://github.com/pyvisa/pyvisa/blob/main/pyvisa/resources/serial.py),
and pyvisa-py
[`serial.py`](https://github.com/pyvisa/pyvisa-py/blob/main/pyvisa_py/serial.py)
and
[`tcpip.py`](https://github.com/pyvisa/pyvisa-py/blob/main/pyvisa_py/tcpip.py).
They are supplemented by the PyVISA docs:
[communication](https://pyvisa.readthedocs.io/en/latest/introduction/communication.html)
and [resource names](https://pyvisa.readthedocs.io/en/latest/introduction/names.html).

### 5.1 Defaults common to both resource types

| Setting | Default | Source |
|---|---|---|
| `write_termination` | `"\r\n"` (CR LF), appended to every `write()`/`query()` | `messagebased.py`: `_write_termination: str = CR + LF` |
| `read_termination` | `None`, so nothing is stripped and termination-character detection is not enabled by PyVISA | `messagebased.py`: `_read_termination: Optional[str] = None` |
| Setting `read_termination = "\n"` | Sets `VI_ATTR_TERMCHAR = 0x0A` and `VI_ATTR_TERMCHAR_EN = True`. Only the *last* character ends the read. The full string is checked afterwards, and PyVISA warns "read string doesn't end with termination characters" if it does not match. | `messagebased.py` `read_termination` setter and `read()` |
| `VI_ATTR_TERMCHAR` / `_EN` | `0x0A` / `False` | `attributes.py` |
| `timeout` (`VI_ATTR_TMO_VALUE`) | **2000 ms**. `None` means infinite. | `attributes.py` |
| `query()` | `write()`, then an optional `query_delay` (default 0.0 s), then `read()` | `messagebased.py` |
| `chunk_size` | 20 KiB | `messagebased.py` |

**Firmware requirements:**

1. Accept both `\n` and `\r\n` as the end of a command (PyVISA sends `\r\n`
   unless told otherwise).
2. Terminate every response with a single `\n`.
3. Send **exactly one** response line per query. Send nothing for commands, and
   nothing for unknown queries either (the error goes to `SYST:ERR?`).
4. Reply well within 2 s, or document that clients must raise `timeout`.

**Client snippet to document for users:**

```python
import pyvisa
rm = pyvisa.ResourceManager()          # or ResourceManager("@py") for pyvisa-py
inst = rm.open_resource("TCPIP::192.168.1.50::5025::SOCKET")   # or "ASRL/dev/ttyACM0::INSTR" / "ASRL3::INSTR"
inst.read_termination = "\n"
inst.write_termination = "\n"
inst.timeout = 2000                    # ms
print(inst.query("*IDN?"))             # "<Manufacturer>,<Model>,<Serial>,<Firmware>"
```

### 5.2 TCPIP::host::5025::SOCKET

- Syntax: `TCPIP[board]::host address::port::SOCKET`
  ([names](https://pyvisa.readthedocs.io/en/latest/introduction/names.html)).
  `TCPIPSocketSession.default_tcpip_port = 5025`
  ([pyvisa-py tcpip.py](https://github.com/pyvisa/pyvisa-py/blob/main/pyvisa_py/tcpip.py)).
- **Nothing on a raw socket marks the end of a message.** The pyvisa-py read loop
  returns only when it sees the termination character (if `termchar_en` is
  set), when `count` bytes have arrived, or when the timeout expires. So if a
  client forgets `read_termination = "\n"`, every `query()` times out. If the
  firmware forgets the `\n`, the client times out too.
- `VI_ATTR_TCPIP_NODELAY` defaults to True and `VI_ATTR_TCPIP_KEEPALIVE` to False
  (`attributes.py`). The firmware should not rely on keep-alives to notice a
  client that has disappeared; use an idle timeout on the socket if one is
  needed.
- **Discovery:** `list_resources()` in pyvisa-py finds SOCKET resources **only by
  mDNS**, browsing `_scpi-raw._tcp.local.`. It uses the advertised `port`, or
  5025 if none, and returns `TCPIP::<host>::<port>::SOCKET`. It needs the
  optional `zeroconf` package (`TCPIPSocketSession.list_resources`). Any other
  client must know the IP address. Advertising `_scpi-raw._tcp` over mDNS from
  the W5500 is an optional nice-to-have that would make `rm.list_resources()`
  find the device.

### 5.3 ASRL (USB CDC)

- Syntax: `ASRLboard[::INSTR]`
  ([names](https://pyvisa.readthedocs.io/en/latest/introduction/names.html)).
  pyvisa-py names resources `ASRL<device path>::INSTR` on Linux (for example
  `ASRL/dev/ttyACM0::INSTR`) and `ASRL<n>::INSTR` on Windows for `COM<n>`. It
  builds them from pySerial's `serial.tools.list_ports.comports()`
  ([pyvisa-py serial.py](https://github.com/pyvisa/pyvisa-py/blob/main/pyvisa_py/serial.py)).
- **The read ends on LF by default.** `VI_ATTR_ASRL_END_IN` (`end_input`)
  defaults to `termination_char`, and `VI_ATTR_TERMCHAR` to `0x0A`. Even with
  `read_termination=None`, a serial read therefore stops at `\n`, but the `\n` is
  not stripped. `VI_ATTR_ASRL_END_OUT` defaults to `none`, so only
  `write_termination` is appended (`attributes.py`). The PyVISA docs warn that
  serial termination is "controlled by a separate `end_input` attribute"
  ([communication](https://pyvisa.readthedocs.io/en/latest/introduction/communication.html)).
- **Baud rate does not matter on CDC.** PyVISA defaults `baud_rate` to 9600
  (`attributes.py`), and the host sends it in a CDC `SET_LINE_CODING` request.
  STM32duino stores that value but ignores it: `USBSerial::begin(uint32_t
  baud_count)` does `(void)baud_count;`
  ([USBSerial.cpp](https://github.com/stm32duino/Arduino_Core_STM32/blob/main/libraries/USBDevice/src/USBSerial.cpp),
  [usbd_cdc_if.c](https://github.com/stm32duino/Arduino_Core_STM32/blob/main/libraries/USBDevice/src/cdc/usbd_cdc_if.c)).
  Any baud rate works.
- **DTR matters.** STM32duino's `USBSerial::write()` loops only while
  `CDC_connected()` is true, and `CDC_connected()` requires `dtrState`. As a
  result, output is **silently dropped while the host holds DTR low**, unless
  the firmware calls `SerialUSB.dtr(false)` (which wraps `CDC_enableDTR(false)`)
  ([usbd_cdc_if.c](https://github.com/stm32duino/Arduino_Core_STM32/blob/main/libraries/USBDevice/src/cdc/usbd_cdc_if.c)).
  pySerial, and therefore pyvisa-py, asserts DTR and RTS on `open()` by default
  ([pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)).
  Clients that turn DTR off will get no responses. Consider
  `SerialUSB.dtr(false)` so the device answers whatever the host's DTR state,
  and document the choice either way.
- **Linux discovery and permissions.** pySerial lists `/dev/ttyACM*`. Users need
  to be in the group that owns the device (usually `dialout`), or a udev rule is
  needed. STM32duino's CDC interface reports `bInterfaceProtocol = 0x00` ("No
  specific protocol")
  ([usbd_cdc.c](https://github.com/stm32duino/Arduino_Core_STM32/blob/main/libraries/USBDevice/src/cdc/usbd_cdc.c)).
  ModemManager automatically allows only AT-protocol ACM ports, so it should not
  send AT probes to the device
  ([ModemManager filter docs](https://freedesktop.org/software/ModemManager/api/latest/ref-overview-modem-filter.html)).
  A `ENV{ID_MM_DEVICE_IGNORE}="1"` udev rule
  ([ModemManager udev tags](https://freedesktop.org/software/ModemManager/api/latest/ModemManager-Common-udev-tags.html))
  is still cheap insurance, because stray `AT\r` lines would otherwise reach the
  SCPI error queue.
- **NI-VISA on Linux (unverified).** NI's own Linux readme says nothing about
  enumerating USB-serial ports
  ([NI-VISA Linux 2022 Q3 readme](https://www.ni.com/pdf/manuals/ni-visa-linux-2022-q3.html)).
  NI forum reports say only `ttyS*` ports are found automatically, and that
  `ttyUSB`/`ttyACM` ports need an alias in `visaconf` (`[ASRL-RSRC-ALIAS]`) or a
  symlink ([NI forum](https://decibel.ni.com/content/message/55370)). Recommend
  pyvisa-py on Linux.
- **Windows.** `Usbser.sys` loads automatically for devices with class 02 and
  subclass 02 (`USB\Class_02&SubClass_02`), with no INF file needed
  ([Microsoft: USB Serial Driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids)).
  The device appears as `COM<n>`. Both NI-VISA and pyvisa-py expose it as
  `ASRL<n>::INSTR`.

### 5.4 `*IDN?` conventions

This is what clients and VISA tools such as NI MAX and Keysight Connection Expert
expect from `*IDN?`:

- one line;
- four comma-separated fields: `Manufacturer,Model,Serial,Firmware`;
- no commas inside a field;
- terminated by `\n`.

See SCPI-99 §4.1.3.6 for the guidance on what each field should contain
(summarised in §3). Discovery tools typically send `*IDN?` to every port they
open, so the CDC and TCP paths must both answer it **before** any Instrument
Service has started.

---

## 6. Recommendation for GTE Core

1. Pin Vrekrer_scpi_parser at `d5354a0` (v0.6.1). Set `SCPI_HASH_TYPE=uint32_t`
   and keep every limit at or below 255 in the global `build_flags`. Include the
   implementation from a single `.cpp` and use `VREKRER_SCPI_PARSER_NO_IMPL`
   everywhere else.
2. Write a `GteScpiParser : SCPI_Parser` subclass that provides:
   - context-pointer dispatch;
   - collision and overflow checks that return a result from registration;
   - −113 when no command matches;
   - SCPI-99 header-path compounding;
   - quote-aware splitting;
   - `;`-joined query responses.
3. Use one parser instance, with GTE-owned line readers for CDC and each
   `EthernetClient`. Do not use `GetMessage()` or `ProcessInput()`.
4. Implement in GTE Core:
   - the 13 IEEE 488.2 mandatory commands;
   - `SYST:ERR[:NEXT]?` and `SYST:VERS?`;
   - `STAT:OPER`, `STAT:QUES` and `STAT:PRES`;
   - a shared error queue that replaces the newest entry with −350 on overflow.

   That takes about 25 command slots.
5. Expose Config Parameters and Status Values through generic parameterised
   commands rather than one header per parameter, to stay within the 255-command
   and 255-token ceilings.
6. On the wire: accept LF or CRLF, reply with LF, send exactly one line per
   query, and reply in under 2 s. Decide on the DTR policy. Optionally advertise
   `_scpi-raw._tcp` over mDNS.
