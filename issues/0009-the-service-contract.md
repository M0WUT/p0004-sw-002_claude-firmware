---
id: 9
title: The Service contract
labels: [wayfinder:grilling]
status: closed
assignee: M0WUT
parent: 1
blocked_by: []
---

## Question

What is the contract every Service implements? Cover: the full validate → apply → save path for a Config Parameter (sw-001 validates but never writes to `_config.data`); how a change takes effect (immediately, or after restart); the order Services start in and which depend on which (e.g. Web needs Network); how Services find each other; and how an Instrument Service registers itself and its additions without editing core code beyond `ServiceId`.

## Resolution

**Lifecycle**
- **Register**: GTE Core owns `setup()`/`loop()`. It registers the core Services, then the Instrument's list, which the Instrument-specific code provides (as with sw-001's `extraServiceList`). There is exactly one instance per `ServiceId`, and editing the enum per Instrument is allowed.
- **Initialise**: each Service's `initialise()` loads its config (or creates defaults and saves them if the EEPROM block is missing or invalid), then does its own setup. **No calls between Services during `initialise()`.** There is no dependency graph or derived start order.
- **Run**: `tick()` starts only after every Service has initialised. It is the only place Services may call each other. They find each other with ServiceHelper by `ServiceId`, plus a cast.
- **Execution model**: whether `tick()` runs in a cooperative loop or under STM32duino FreeRTOS is decided in [Execution model](0010-execution-model.md). The user prefers FreeRTOS, started once every Service has initialised.

**Config Parameters**
- **Declaration**: in `get_config_fields`, with:
  - a stable id (the web page, SCPI and Home Assistant name things by it);
  - a label, a type, and **a pointer to where the value lives in `_config.data`**;
  - range, step and unit;
  - an **Apply Mode** enum (`Live` or `Reset`);
  - an optional **secret** flag.
- **Update path**, for the web page or SCPI:
  1. Generic code parses the value and checks type, range and step.
  2. The Service's `validate_config_field` applies any custom rule (e.g. refusing a setpoint change while running).
  3. Generic code writes the value into `_config.data`.
  4. Generic code saves the whole config block, with its CRC, using the existing `write_service_config`.
  5. `Live` takes effect immediately. `Reset` sets a "restart required" flag, shown on the web page and over SCPI, which also offer a restart command. That way several Reset changes need only one restart.
  6. An optional per-Service hook handles side effects.
- **Secret Config Parameters** are write-only: masked on the web page, and left out of Home Assistant, SCPI queries and log printouts.

**Status Values**
- sw-001's `get_status_fields(list)` style, with typed add calls that take a stable id and a unit, e.g. `addFloat("boardTemp", "Board temperature", 23.5, "°C")`.

**Fatal Errors**
- `_error()`/`_errorf()` are fatal. The device stops normal operation and shows the failing Service and the message on the LCD. The red LED blinks at 0.5 Hz.
- **The error is printed on the debug UART every 2 s**, so a cable attached at any time afterwards still sees it. The device never reboots by itself.
- Any Service may raise a Fatal Error. Mostly it's for hardware that should be fitted but isn't detected. Things that are normally absent (no SD card or USB stick, a Service disabled in config, no IO Board fitted for now) are Status Values, not errors.
- How `_warning()` is used is redefined in [How warnings work](0019-how-warnings-work.md).

**Passed on to**: the EEPROM layout and versioning go to [Config storage format](0012-config-storage-format.md); adding sensors to [The sensor model](0011-the-sensor-model.md); SCPI commands to [The SCPI command tree](0015-the-scpi-command-tree.md), which should use generic `CONF:` commands; Home Assistant entities to [Mapping Services to Home Assistant](0016-mapping-services-to-home-assistant.md).
