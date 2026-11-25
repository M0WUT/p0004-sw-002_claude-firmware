---
id: 6
title: Fitting the Vrekrer SCPI parser to GTE Core
labels: [wayfinder:research]
status: closed
assignee: claude-research-agent
parent: 1
blocked_by: []
---

## Decision (from the user)

The library is chosen: [Vrekrer_scpi_parser](https://github.com/Vrekrer/Vrekrer_scpi_parser). The research now covers how it fits the design: registering commands at runtime, serving two transports, the IEEE 488.2 and error-queue gaps, compile-time limits, and the PyVISA requirements.

## Question

Which SCPI parser libraries suit an STM32duino firmware that must take commands from both a USB CDC serial port and a TCP socket (port 5025), and be extended at runtime by Services that the core doesn't know about? Compare options (e.g. j123b567/scpi-parser, Vrekrer SCPI parser, others) on licence, memory, IEEE 488.2 mandatory commands, error queue, and runtime command registration. Also: what does PyVISA expect from an ASRL (serial) instrument and from a TCPIP SOCKET instrument (termination characters, `*IDN?`)?

## Resolution

Full findings: branch `research/scpi-parser-options` (commit 604f246, local only), file `docs/research/scpi-parser-options.md`. Checked against Vrekrer v0.6.1 (`d5354a0`); pin that commit, since the header still says v0.5.0. MIT licence, about 0.3–3.6 KB RAM per instance, one maintainer.

- **Runtime registration** works at any time, but:
  - There's no unregister, and callbacks are bare function pointers with no context.
  - Every limit is `uint8_t`, so nothing exceeds 255 (`SCPI_MAX_COMMANDS=300` silently became 44).
  - The default 8-bit hash collided at 20 commands, so use `-D SCPI_HASH_TYPE=uint32_t`.
  - A set/query pair per Config Parameter runs out at about 100 parameters. Generic commands such as `CONF:VAL <svc>,<param>,<val>` avoid that.
  - Set the limits in global `build_flags`. Include the implementation from exactly one `.cpp`; everywhere else, define `VREKRER_SCPI_PARSER_NO_IMPL` first.
- **Transports**: one parser instance, with our own line reader per transport (USB CDC, each `EthernetClient`) calling `Execute(line, stream)`. The library's own reader shares one buffer, drops partial lines after 10 ms, and runs the rest of an overflowing line as a new command.
- **No IEEE 488.2 or SCPI-99 support**:
  - Unmatched commands are silently ignored, so TCP clients wait until they time out.
  - Commas inside quoted strings split parameters.
  - Needed: a subclass of about 100 lines that overrides `Execute()` to raise error −113, resolve `;`-chained headers, split parameters with quotes respected, pass a per-command context pointer, and reject hash collisions.
  - GTE Core must implement the 13 mandatory common commands, `SYST:ERR?`, `SYST:VERS?`, `STAT:OPER`/`QUES`/`PRES`, and an error queue that reports −350 on overflow.
- **PyVISA**:
  - The firmware accepts LF or CRLF and ends every response with a single LF. Clients set both terminations to `"\n"`. The default timeout is 2 s.
  - `*IDN?` returns 4 comma-separated fields and must answer before Instrument Services start.
  - TCPIP SOCKET: discovery works only over mDNS `_scpi-raw._tcp` (pyvisa-py plus zeroconf).
  - ASRL: output is dropped while DTR is low, unless `SerialUSB.dtr(false)` is set. Use pyvisa-py on Linux.
