# SCPI-99 conventions for a configurable instrument

Status: research note, 2026-11-26. Ticket: "SCPI-99 conventions for a configurable
instrument" (GitHub issue #22).

**Question:** what conventions should the GTE Core SCPI command tree follow? This
covers mandatory commands and the status model, generic access to settings,
parameter types, reset, restart and factory reset, the error queue, network
settings, naming, and read-only and write-only values.

**Constraint carried in from
[`scpi-parser-options.md`](https://github.com/M0WUT/p0004-sw-002_claude-firmware/blob/research/scpi-parser-options/docs/research/scpi-parser-options.md)**
(branch `research/scpi-parser-options`): Vrekrer_scpi_parser caps the number of
registered commands at 255. So GTE Core exposes Config Parameters and Status
Values through a few generic, parameterised commands, not one command per value.

## Sources

| Tag | Document | Where |
|---|---|---|
| **[S1]** | SCPI Consortium, *SCPI-99* (all 4 volumes in one PDF: Vol 1 Syntax & Style, Vol 2 Command Reference, Vol 3 Data Interchange, Vol 4 Instrument Classes) | <https://www.ivifoundation.org/downloads/SCPI/scpi-99.pdf> |
| **[K1]** | Keysight *Truevolt Series DMM Operating and Service Guide* (34460A/61A/65A/70A), p/n 34460-90901, Edition 13, Sep 2025 | <https://www.keysight.com/content/dam/keysight/en/doc/gate/service-manuals/9018-03876.pdf> |
| **[K2]** | Keysight *E36300 Series Programming Guide*, p/n E36311-90008, Edition 3, Oct 2017 (mirror of the Keysight PDF) | <https://xdevs.com/doc/HP_Agilent_Keysight/E36300A/E36311-90008_programming_guide.pdf> |
| **[K3]** | Keysight M9384B VXG help, "Power On/Standby" (`SYSTem:REBoot`) | <https://helpfiles.keysight.com/csg/m9384/Content/GPSS/Power%20On%20Standby.htm> |
| **[R1]** | Rigol *DP800 Programming Guide*, PGH03102-1110 (2013) (mirror of the Rigol PDF) | <https://www.batronix.com/pdf/Rigol/DP800_ProgrammingGuide_EN.pdf> |
| **[G1]** | Siglent *SPD3303X/X-E Quick Start*, EN_02A (holds the programming commands) | <https://siglentna.com/download/2512> |

IEEE 488.2 itself is paywalled and was **not read directly**. Claims about it come
from the places where [S1] (which restates and cites 488.2 by section) and [K1]
(which documents the 488.2 registers) describe it. The EDU36311A guide could not
be fetched (the mirror returned HTML). [K2] covers the same E363xx command set.

---

## TL;DR

1. **Mandatory set:** SCPI-99 requires the 13 IEEE 488.2 mandatory common
   commands plus `SYSTem:ERRor[:NEXT]?`, `SYSTem:VERSion?`,
   `STATus:OPERation…`, `STATus:QUEStionable…` and `STATus:PRESet`
   ([S1] Vol 1 §4.1.1, §4.2.1). For GTE Core, that is about 25 registered
   commands.
2. **Do not root the generic settings tree at `CONFigure`.** `CONFigure` is
   a SCPI-defined measurement subsystem (`CONFigure:<function>`, [S1] Vol 1 §3.1,
   Vol 2 ch 3). Its short form is `CONF`, so a `CONFig:…` node would be the same
   keyword. An Instrument built on GTE Core (for example a DMM) may need the real
   `CONF:VOLT:DC`. A complete SCPI path must mean the same thing in every
   instrument ([S1] Vol 1 §6.2.2 rule 6). Use a device-specific root of our own.
3. **The generic access pattern has precedent:** SCPI's own naming facility is
   `:DEFine <name>,<data>` / `:DEFine? <name>` / `:CATalog?` ([S1] Vol 1 ch 11),
   with names returned as quoted strings. Vendors also take the target as a
   parameter (`SYST:COMM:ENABle ON,GPIB` [K1]; `OUTP CH1,ON` [R1]). SCPI-99 style
   rule 10 does discourage one parameter changing the type of another, so the
   generic command is a documented device-specific exception.
4. **Three levels of "reset":** `*RST` sets a known, safe *remote* state and
   leaves non-volatile I/O settings alone. `SYSTem:PRESet` gives the
   front-panel-friendly state. `SYSTem:SECurity:IMMediate` wipes all user settings
   and reboots, and is the closest SCPI-defined match to GTE's Factory Reset
   ([S1] Vol 1 ch 10, Vol 2 §21.16–21.17; [K1]; [K2]). SCPI-99 has no restart
   command. `SYSTem:REBoot` is the vendor convention [K3].
5. **Errors:** the response is `<code>,"<text>[;<detail>]"`. `0,"No error"`
   means the queue is empty. The negative codes are SCPI's: -1xx command, -2xx
   execution, -3xx device, -4xx query. Positive codes are ours. Overflow replaces
   the newest entry with `-350,"Queue overflow"` ([S1] Vol 2 §21.8).
   Keysight keeps **one error queue per I/O session** [K1], which suits USB plus
   TCP.
6. **Booleans:** accept `ON|OFF|1|0` and **always return `1|0`** ([S1] Vol 1
   §7.3). Rigol returns `ON`/`OFF` [R1], which breaks this rule. Do not copy
   it.
7. **Secrets:** SCPI-99 `SYSTem:PASSword` commands are all `[event; no query]`,
   and a wrong password gives `-221` ([S1] Vol 2 §21.15). Keysight's
   `CAL:SEC:CODE` also has no query form [K1]. A Secret Config Parameter is
   write-only, and a query of it should raise an error.

---

## 1. Mandatory and conventional commands

### 1.1 IEEE 488.2 common commands (mandatory)

SCPI-99 Vol 1 §4.1.1 says: "All SCPI instruments shall implement all the common
commands declared mandatory by IEEE 488.2". The table lists `*CLS`,
`*ESE`, `*ESE?`, `*ESR?`, `*IDN?`, `*OPC`, `*OPC?`, `*RST`, `*SRE`, `*SRE?`,
`*STB?`, `*TST?` and `*WAI`. The optional 488.2 commands (`*SAV`, `*RCL`,
`*PSC`, `*LRN?`, …) are "not required by SCPI" (§4.1.2).

What each one means in practice. The sources are [S1] Vol 1 §4.1.3 and the
[K1] Keysight descriptions:

| Command | Behaviour | Notes for GTE |
|---|---|---|
| `*IDN?` | 4 comma-separated fields: `Manufacturer,Model,Serial,Firmware` [K1]. Per [S1] §4.1.3.6: the manufacturer field should be identical across a company's products, the model "should NOT contain the word MODEL", and the firmware field "should contain information about all separately revisable subsystems". | Field 4 should carry **both** the Instrument Firmware version and the GTE Core version, e.g. `M0WUT,GTE-PSU1,0001,1.3.0-core0.4.1`. |
| `*RST` | Sets a known configuration. See §4. | Does not touch status registers or the error queue ([S1] Vol 1 ch 10). |
| `*CLS` | Clears the SESR, the OPERation and QUEStionable *event* registers and the error queue. Enable registers are unaffected ([S1] §4.1.3.2). | |
| `*OPC` / `*OPC?` / `*WAI` | With only sequential commands, "`*OPC`… simply sets the OPC bit", "`*WAI` is a no-operation" and `*OPC?` "simply places a '1' in the Output Queue" ([S1] §4.1.3.3–4). All three are required "even if none of the device's commands are overlapped". | GTE Core can make all commands sequential and implement these trivially. |
| `*ESE <n>` / `*ESE?` / `*ESR?` | Standard Event Status enable mask and event register. Reading `*ESR?` clears it. | |
| `*SRE <n>` / `*SRE?` / `*STB?` | Service Request enable and Status Byte. `*STB?` is non-destructive; "the Master Summary bit (bit 6) is not cleared by `*STB?`" [K1]. | Over USB CDC and raw TCP there is no hardware SRQ line. `*STB?` polling is still useful. |
| `*TST?` | Returns `+0` (pass) or `+1` (fail), and on failure "stores an error in the error queue". The instrument returns to its prior state afterwards [K1]. | Could run each Service's self-check. |

### 1.2 SCPI-required commands

[S1] Vol 1 §4.2.1 "Required Commands":

```
:SYSTem
  :ERRor
    [:NEXT]?
  :VERSion?
:STATus
  :OPERation
    [:EVENt]?
    :CONDition?
    :ENABle
    :ENABle?
  :QUEStionable
    [:EVENt]?
    :CONDition?
    :ENABle
    :ENABle?
  :PRESet
```

- `SYSTem:VERSion?` returns `<NR2>` of the form `YYYY.V`, i.e. `1999.0`
  ([S1] Vol 2 §21.21).
- A SCPI instrument must also parse compound headers (`FREQ:STAR 3;STOP 5`
  walks relative to the last node; [S1] Vol 1 §4.1, §6.2.4). The parser note
  found that Vrekrer does not do this yet.
- The documentation shall state the SCPI version complied with ([S1] Vol 1
  §4.2.3).

The STATus commands take `<NRf>` and not `<numeric_value>`, so `MIN`/`MAX` are
**not** accepted. Queries always return `<NR1>`. The range is 0–32767, because
bit 15 is always 0 ([S1] Vol 2 ch 20 intro, §20.1.2–20.1.3).

### 1.3 Conventional extras worth having

From [S1] Vol 2 §21.8: `SYSTem:ERRor:COUNt?`, `SYSTem:ERRor:ALL?`, and
`SYSTem:ERRor:CODE[:NEXT]?`. Keysight [K1] adds read-only `SYSTem` queries
such as `SYST:TEMP?` (internal temperature), `SYST:UPTime?`, and `SYST:LABel
"<string>"` (a user string, not affected by `*RST`).

## 2. Status model

### 2.1 Status Byte and Standard Event Status Register (IEEE 488.2)

Bit meanings, as documented by [K1] (p. 260–261) and drawn in [S1] Vol 4's
status figures:

**Status Byte (`*STB?`, enable `*SRE`)**

| Bit | Value | Meaning |
|---|---|---|
| 0, 1 | 1, 2 | Available to designer |
| 2 | 4 | Error/event queue not empty ([S1] Vol 1 §9.1 mandates bit 2 for this) |
| 3 | 8 | QUEStionable summary |
| 4 | 16 | MAV, message available |
| 5 | 32 | ESB, Standard Event summary |
| 6 | 64 | MSS/RQS, master summary |
| 7 | 128 | OPERation summary |

**Standard Event Status Register (`*ESR?`, enable `*ESE`)**

| Bit | Value | Meaning | Set by |
|---|---|---|---|
| 0 | 1 | Operation Complete | `*OPC` |
| 1 | 2 | Request Control | (unused on non-GPIB) |
| 2 | 4 | Query Error | any -4xx |
| 3 | 8 | Device-Dependent Error | any -3xx **or any positive error** [K1] |
| 4 | 16 | Execution Error | any -2xx |
| 5 | 32 | Command Error | any -1xx ([S1] Vol 2 §21.8.9: "shall cause the command error bit (bit 5)… to be set") |
| 6 | 64 | User Request | |
| 7 | 128 | Power On | power cycled since last read |

### 2.2 OPERation and QUEStionable

Each register is a 16-bit set: CONDition (live), PTRansition/NTRansition
filters, EVENt (latched; "reading the event register clears it"), and ENABle,
which feeds the summary bit in the Status Byte ([S1] Vol 1 ch 9, Vol 2 §20.1).
`STATus:PRESet` sets the enables to 0, PTR to all 1s and NTR to 0. It clears
neither the events nor the queue ([S1] Vol 2 §20.2). The recommended init
sequence is `*CLS; *SRE 0; *ESE 0; STATus:PRESet` (§20.2).

**STATus:OPERation** reports "conditions which are part of the instrument's
normal operation" ([S1] Vol 1 §9.3):

| Bit | Name |
|---|---|
| 0 | CALibrating |
| 1 | SETTling |
| 2 | RANGing |
| 3 | SWEeping |
| 4 | MEASuring |
| 5 | Waiting for TRIGger |
| 6 | Waiting for ARM |
| 7 | CORRecting |
| 8–12 | **available to designer** (`STAT:OPER:BIT<n>`, n = 8..12) |
| 13 | INSTrument summary |
| 14 | PROGram running |
| 15 | always 0 |

**STATus:QUEStionable** indicates that "the data currently being acquired or
generated is of questionable quality" ([S1] Vol 1 §9.4). The bit names below
are from the [S1] Vol 4 status figures:

| Bit | Name |
|---|---|
| 0 | VOLTage |
| 1 | CURRent |
| 2 | TIME |
| 3 | POWer |
| 4 | TEMPerature |
| 5 | FREQuency |
| 6 | PHASe |
| 7 | MODulation |
| 8 | CALibration |
| 9–12 | **available to designer** (`STAT:QUES:BIT<n>`, n = 9..12) |
| 13 | INSTrument summary |
| 14 | Command Warning (a parameter was ignored) |
| 15 | always 0 |

Real instruments only partly follow this:

- **Keysight 34461A** QUES ([K1] p. 383): 0 voltage overload, 1 current
  overload, 4 temperature overload, 5 frequency overload, 8 "Calibration
  Corrupt", and designer bits 9–12 used for resistance/capacitance overload and
  limit fail. Bit 14 is reused as "Memory Overflow".
- **Keysight E36300** ([K2] p. 31–32) uses only QUES bit 13. That bit fans out
  to `STAT:QUES:INST:ISUM<n>` (one register per output; bit 0 voltage
  unregulated, bit 1 current unregulated). This is the SCPI
  multiple-logical-instrument fan-out of [S1] Vol 1 §9.5. It is the pattern for
  per-channel or per-Service status.

**For GTE Core:** implement all five registers, even if most bits stay 0. The
firmware already has some candidates:

- QUES bit 4 TEMPerature for overtemperature.
- QUES bit 8 CALibration for a corrupt calibration or config.
- OPER designer bits 8–12 for core conditions such as "config saved, restart
  pending" (Apply Mode = Reset).

Application Services can fan out further under `STATus:QUEStionable:<node>`.
The "Status Values" of the glossary are a different thing (§6).

## 3. Parameter types and generic settings access

### 3.1 Types

The type grammar is IEEE 488.2 §7.7 (program data) and §8.7 (response data),
with the SCPI restrictions from [S1] Vol 1 ch 7.

- **`<NRf>`** is the flexible numeric input form: any of NR1 (integer,
  `42`), NR2 (fixed-point, `4.20`) or NR3 (exponent, `4.2E+1`). Responses
  use the specific forms. Keysight returns, for example, `+2.85000000E+01` (NR3)
  for `SYST:TEMP?` and `+22` (NR1) for `SYST:SEC:COUNt?` [K1].
- **`<numeric_value>`** (SCPI) is `<NRf>` plus character forms. **`MINimum`
  and `MAXimum` are required** on every command that takes one. `DEFault`, `UP`,
  `DOWN`, `INFinity`, `NINF` and `NAN` are optional and must be documented
  ([S1] §7.2.1). `<header>? MIN|MAX` returns the limits (§7.2.1.2), as in
  [K2]'s `CURR? MAX; CURR? MIN`. Values beyond ±9.9E37 → `-222,"Data out of
  range"`. Other values are "rounded to the closest 'correct' value" (§7.2).
  Numerics "shall not be used for selecting functions on a 'One of N' position
  switch" (§7.2). Use character data for enumerations.
- **Units:** if a parameter has units, it "shall accept a suffix" (e.g. `100
  mV`), with multipliers (§7.2.2).
- **`<Boolean>`** is `ON|OFF|<NRf>`. A nonzero value after rounding is 1, and
  "Queries shall return 1 or 0, never ON or OFF" (§7.3). [K2] does this
  (`DISP {OFF|ON}`, query returns 0/1). [R1] does not (`:OUTP? CH1` returns
  `ON`).
- **Character data** (enumerations) accepts long and short forms, and the
  query returns "the shortform value" ([S1] §6.2.3). [K2]: "Query responses
  will always return the short form in all upper-case letters".
- **String** is quoted with `"` or `'`, and the quote is doubled to embed it
  ([K2] p. 15). Keysight's IP address, hostname and label values are all
  quoted strings ([K1]: `SYST:COMM:LAN:IPAD "169.254.149.35"`).

### 3.2 Dedicated commands vs generic paths

**The SCPI-99 ideal is dedicated, typed leaf commands.** Every command has a
query form unless marked otherwise, and a query has "no side effects"
([S1] §6.2.3). Parameters sit only at leaves (§6.2.2 rule 11). Rule 10 says
"The value or syntax of a parameter shall not change the type or number of other
parameters in that command. If such a condition is encountered, additional nodes
should be added to the tree." The example it gives is `SYST:BEEP:FREQ` and
`SYST:BEEP:TIME`, *not* `SYST:BEEP 1,<freq>`.

**Precedent for generic or parameter-addressed access:**

- **SCPI's own naming facility** ([S1] Vol 1 ch 11): `:DEFine <name>,<data>`,
  `:DEFine? <name>`, `:DELete[:NAME] <name>` and `:CATalog?`. "The type of
  <data> is dependent on the subsystem". `CATalog?` returns "a list of one or
  more strings… If no names are defined, a single null string is returned".
  Names are sent as character data (≤12 chars, no colons). GTE's
  `service.param` names may be longer, so quoted strings are safer.
- **Rule 14 exception** ([S1] §6.2.2): commands may take a parameter naming
  an object when "these objects cannot be enumerated" at standard time, for
  example `<file_name>`. Services registered at runtime are in the same
  position.
- **Vendors:** Keysight `SYSTem:COMMunicate:ENABle {ON|1|OFF|0},GPIB`, with the
  interface as a parameter [K1]. Keysight E36300 `OUTPut[:STATe] ON,(@1)` uses a
  channel list [K2]. Rigol `:OUTP CH1,ON` and `:PRESet:USER[n]:SET:VOLTage`
  [R1]. Siglent `OUTPut CH1,ON` but also `CH1:VOLTage 25` [G1].
- **[S1] §6.2.2** also warns against adding filler nodes "simply to make the tree
  have a uniform depth… (for instance, VALue)". A bare leaf is better than
  `…:VALue`.

**Conclusion:** the generic pattern breaks a *style* rule (rule 10). That rule
governs commands proposed for the SCPI standard. Device-dependent commands are
explicitly the manufacturer's to define ([S1] Vol 1 §3.3). Given the 255-command
limit, a generic, documented device-specific subsystem is the defensible
choice. Keep it as SCPI-like as possible:

- Address by quoted-string names.
- Have a `CATalog?` that returns quoted strings.
- Return each value in its natural response type: NR1/NR3, `0|1` for booleans,
  short-form character data, or a quoted string.
- Accept `MIN|MAX|DEF` for numerics.
- Report `-224,"Illegal parameter value"` (unknown name or enum),
  `-222,"Data out of range"`, or `-104,"Data type error"`.

For the few settings that SCPI already defines, also add **dedicated aliases**.
Network settings are the main case: `SYST:COMM:LAN:…` (§5). Others are the
beeper (`SYST:BEEP:STAT`, [S1] §21.2.3) and the clock (`SYST:DATE`/`SYST:TIME`,
§21.7/§21.19). A user who knows SCPI then finds them where expected.

## 4. Reset, preset, factory reset, restart

| Command | Source definition | Practice |
|---|---|---|
| `*RST` | "All instruments shall return to a known configuration". Trigger idle. "Source instruments should not be sourcing power". Special modes disabled. "Optimized for remote operation". Status structures "are not affected by `*RST`" ([S1] Vol 1 ch 10, Vol 2 table §20.2). | Keysight [K1]: does not affect stored states. LAN settings are "non-volatile; it is not changed by power cycling, a Factory Reset (`*RST`), or an Instrument Preset (`SYSTem:PRESet`)". Keysight calls `*RST` "Factory Reset". Rigol [R1]: "Restore the power supply to factory state… and clear the error queue" (the queue clear contradicts [S1]). |
| `SYSTem:PRESet` | "Sets the device to the same state as the front-panel reset key… optimized for front panel operation". `PRESet2` etc. may select other presets. "The state of the device after PRESet is not defined by SCPI". No query ([S1] Vol 2 §21.16, Vol 1 ch 10). | [K1]: "*RST turns the histogram and statistics off, and SYSTem:PRESet turns them on". Rigol uses its own `:PRESet[:APPLy]` + `:PRESet:KEY DEFAULT\|USER1…` [R1]. |
| `SYSTem:SECurity:IMMediate` | "Immediately destroys all measurement data and instrument settings. Current settings are initialized to their `*RST` values… Critical calibration constants are retained". May be under `SYSTem:PASSword` control; if protected → `-203,"Command protected"` ([S1] Vol 2 §21.17.1). | [K1]: "Sanitizes all user-accessible instrument memory except for the calibration constants and reboots"; it resets user I/O settings "such as the IP address". [K2]: "clears all user memory and reboots… Identification data (… serial number, MAC address and calibration data) is not erased". |
| `SYSTem:REBoot` | Not in SCPI-99. | Keysight VXG: `:SYSTem:REBoot [NORMal]\|CPOWer`, "provides the ability to reboot the instrument" [K3]. |
| LAN-only resets | Not in SCPI-99. | Keysight `LXI:RESet` (LAN to DHCP, clears web password) and `LXI:RESTart` (apply LAN settings) [K1]. |

**Mapping to GTE vocabulary (GLOSSARY.md):**

- `*RST`: put every Service into its safe remote state, with outputs off. It
  must **not** change Config Parameters stored in EEPROM, and it leaves the status
  registers and the error queue alone.
- **Factory Reset** (rebuild config storage, all Services to default Config
  Parameters): `SYSTem:SECurity:IMMediate`. This is the SCPI-defined command
  with exactly that meaning. It reboots afterwards, as both Keysight products do.
  Calibration and identity data are kept.
- "Reset to defaults" for one Service: a generic-tree command, e.g.
  `<root>:PARameter:DEFault "<service>"`.
- **Restart** (needed for Apply Mode = Reset): `SYSTem:REBoot`. Document that
  the host must reconnect, because USB CDC re-enumerates and the TCP socket drops,
  and that `*OPC?` cannot synchronise across it [K3].

## 5. Error queue and network settings

### 5.1 `SYSTem:ERRor[:NEXT]?` ([S1] Vol 2 §21.8)

- Response: `<number>,"<description>[;<device-dependent info>]"`, e.g.
  `-131,"Invalid suffix;FREQuency:CENT 2.0E+5 dBuV"`. The string is at most 255
  characters. For standard codes, the description "shall be sent exactly as
  indicated in this document including case".
- The number is in [-32768, 32767]. "All positive numbers are
  instrument-dependent. All negative numbers are reserved by the SCPI standard…
  zero… indicate[s] that no error". When empty, the query returns `0,"No error"`
  (Keysight prints `+0,"No error"` [K1]).
- The queue is FIFO, with a minimum length of 2. On overflow, the last entry is
  replaced with `-350,"Queue overflow"` and the newest error is discarded. The
  queue is cleared by `*CLS`, by reading the last item, and at power-on.
- Classes and the ESR bit each one sets:

  | Range | Class | ESR bit |
  |---|---|---|
  | -100…-199 | Command (syntax) error | bit 5 |
  | -200…-299 | Execution error | bit 4 |
  | -300…-399 | Device-specific error | bit 3 |
  | -400…-499 | Query error | bit 2 |
  | positive | device-defined | bit 3 per [K1] |

  "More specific error/event codes are preferred, and the generic error/event
  is used only if the others are inappropriate" (§21.8.2).
- Codes GTE will need:
  - **Command errors:**
    - `-102,"Syntax error"`
    - `-104,"Data type error"`
    - `-108,"Parameter not allowed"`
    - `-109,"Missing parameter"`
    - `-113,"Undefined header"` (Vrekrer's `UnknownCommand`)
    - `-114,"Header suffix out of range"`
  - **Execution errors:**
    - `-200,"Execution error"`
    - `-203,"Command protected"`
    - `-221,"Settings conflict"`
    - `-222,"Data out of range"`
    - `-224,"Illegal parameter value"`
    - `-240,"Hardware error"`
    - `-241,"Hardware missing"`
  - **Device-specific errors:**
    - `-310,"System error"`
    - `-315,"Configuration memory lost"`. Keysight uses `-315,"Configuration
      memory lost; memory corruption detected"` [K1], which is exactly the GTE
      EEPROM-CRC-failure case.
    - `-330,"Self-test failed"`
    - `-350,"Queue overflow"`
    - `-363,"Input buffer overrun"` (Vrekrer's `BufferOverflow`)
  - **Query errors:** `-410,"Query INTERRUPTED"` and `-420,"Query UNTERMINATED"`.
- **Positive codes:** Keysight groups them by area, e.g. `+100,"Network
  Error"`, `+110,"LXI mDNS Error"` (+1xx network) and `+263,"Not able to
  execute while instrument is measuring"` (+2xx instrument) [K1]. GTE could give
  each Service a block of codes.
- **Per-session queues:** "Each remote interface I/O session (GPIB, USB, VXI-11,
  Telnet/Sockets) has its own interface-specific error queue. Errors appear in the
  error queue of the I/O session that caused the error… Errors generated by
  instrument hardware are broadcast to all I/O sessions" [K1]. This is the model
  to copy for USB CDC plus TCP 5025.
- Non-conformant example: Siglent returns `0 No Error`, with no comma and no
  quotes [G1]. PyVISA-style parsers expect `0,"No error"`.

### 5.2 Network settings

SCPI-99's `SYSTem:COMMunicate` has `GPIB`, `SERial` and (1999) `SOCKet`
nodes, but **no `LAN` node** ([S1] Vol 2 §21.4). `SYST:COMM:LAN:…` is a de
facto vendor convention that Keysight and Rigol share:

| Function | Keysight Truevolt [K1] | Rigol DP800 [R1] | Siglent SPD3303X [G1] |
|---|---|---|---|
| DHCP | `SYST:COMM:LAN:DHCP {ON\|1\|OFF\|0}` | `:SYST:COMM:LAN:DHCP[:STATe]` | `DHCP {ON\|OFF}` |
| IP | `SYST:COMM:LAN:IPADdress "<addr>"` | `:SYST:COMM:LAN:IPADdress <ip>` | `IPaddr <ip>` |
| Mask | `SYST:COMM:LAN:SMASk "<mask>"` | `:SYST:COMM:LAN:SMASK` | `MASKaddr` |
| Gateway | `SYST:COMM:LAN:GATeway "<addr>"` | `:SYST:COMM:LAN:GATEway` (short form differs from Keysight's) | `GATEaddr` |
| DNS | `SYST:COMM:LAN:DNS[{1\|2}] "<addr>"` | `:SYST:COMM:LAN:DNS` | — |
| Hostname | `SYST:COMM:LAN:HOSTname "<name>"` | — | — |
| MAC (read-only) | `SYST:COMM:LAN:MAC?` → 12 hex chars in quotes | `:SYST:COMM:LAN:MAC?` | — |
| Apply | `SYST:COMM:LAN:UPDate` ("you must send … UPDate to activate the new setting") | `:SYST:COMM:LAN:APPLy` ("the new setting will only take into effect when" APPLy runs) | — |

Two Keysight details map directly onto GTE concepts:

- **Stored vs active value:** `SYST:COMM:LAN:IPAD? [{CURRent|STATic}]`.
  `CURRent` (the default) is "the value currently being used". `STATic` is "the
  value currently stored in nonvolatile memory" [K1]. This is GTE's Apply Mode =
  Reset problem, solved with a query argument.
- **Explicit apply step** (`UPDate`/`APPLy`) instead of each setting taking
  effect immediately and dropping the TCP link mid-sequence.

## 6. Naming rules

From [S1] Vol 1 ch 6:

- **Long/short form:** "A SCPI instrument shall accept only the exact short and
  the exact long forms", and anything else is an error. The short form is the
  first 4 characters, or 3 if the 4th is a vowel and the long form is longer than
  4 (`SWEep` → `SWE`, `FREE` → `FREE`). A phrase becomes initials plus the last
  word (`RVELocity`). Headers are at most 12 characters, including any numeric
  suffix (IEEE 488.2 limit). Matching is case-insensitive.
- **Numeric suffixes:** any header may carry one, e.g. `TRIGger1`, `OUTP5:MOD3:FM2`.
  "A numeric suffix of 1 is implied", so `TRIG` = `TRIG1`. Multi-instance devices
  "shall recognize nodes which do not have numeric suffixes and treat them as
  channel/capability 1". Single-instance devices need not accept suffixes
  (§6.2.1, §6.2.5.2). Hence "a mnemonic should contain no digits" (§6.2.1). The
  out-of-range error is `-114,"Header suffix out of range"`.
- **Channels:** vendors use either a numeric suffix (`SENSe2`, `OUTPut1`,
  `STAT:QUES:INST:ISUM<n>`), a channel list `(@1,2)` [K2], or a discrete
  parameter `CH1` [R1][G1]. SCPI's own forms are the suffix and the channel list.
- **Tree shape:** keep it "as shallow as possible, usually three levels or
  less". Use the broad noun first (`FREQ:CENT`, not `CENT:FREQ`). Optional
  default nodes go in `[ ]`, e.g. `[:STATe]`, `[:IMMediate]`, `[:NEXT]`
  (§6.2.2 rules 1–4). Avoid near-identical siblings such as `TIME`/`TIMer`.
- **Events:** commands with no query form "do not have any parameters", except
  ones that act on non-enumerable named objects (§6.2.2 rule 14). They are marked
  `[event; no query]`, and query-only commands are marked `[query only]`
  (§6.2.3).
- **Vendor subsystems:** a manufacturer "may elect to build instruments with
  capabilities that are not covered" and may keep those commands even if SCPI
  later standardises something different (Vol 1 §3.3). There is no reserved
  prefix. Vendors add roots such as Keysight's `LXI:` [K1] or Rigol's
  `:PRESet:USER[n]:…` [R1]. SCPI reserves `DIAGnostic` as "a tree node for all
  of the instrument service and diagnostic routines… considered
  instrument-specific" (Vol 2 ch 7). The rule to respect is that a full path
  already defined by SCPI must keep its SCPI meaning (§6.2.2 rule 6). That is why
  `CONFigure`, `STATus:OPERation`, `MEASure`, `SENSe`, `SOURce`, `OUTPut` and so
  on should not be reused for GTE's own purposes.

## 7. Read-only values and write-only secrets

**Read-only:** these are query-only nodes, marked `[query only]` ([S1]
§6.2.3). Sending the command form is then an error (`-113` for an
unregistered header). Examples:

- `SYST:COMM:LAN:MAC?`, `SYST:TEMP?`, `SYST:UPTime?`, `SYST:SEC:COUNt?` and
  `SYST:LFRequency?` [K1]
- `SYSTem:STATus?` [G1]. This returns a hex bitfield, `0x0224`. It is a
  non-SCPI shortcut for the status model, and STATus registers are the
  conformant way to do the same thing.
- SCPI's own `STATus:…:CONDition?` and `SYSTem:ERRor:COUNt?` [S1].

Value-type Status Values belong in the generic tree as query-only. Boolean
"health" conditions are better *also* mirrored into OPERation/QUEStionable
bits, so that `*STB?` and service requests see them.

**Write-only secrets:**

- `SYSTem:PASSword` ([S1] Vol 2 §21.15). `:CDISable <password>`, `[:CENable]
  <password>` and `:NEW <current>,<new>` are all `[event; no query]`, and
  passwords are case-sensitive strings. An "invalid password generates error
  -221, (settings conflict)". Only `:CENable:STATe?` (0/1) can be queried. A
  disabled protected command gives `-203,"Command protected"`. Neither the
  password nor the protection state "shall be affected by `*RST`". The password
  "shall be locally defeatable", meaning there is a front-panel way to reset it
  to `"DEFAULT"`.
- Keysight `CALibration:SECure:CODE <new_code>` has a typical return of
  "(none)", so there is no query. The secured state is readable through
  `CAL:SEC:STAT?` → 0/1 [K1]. The pattern is that the **secret is write-only,
  but whether it is set or enabled can be read.**

For GTE Core, a Secret Config Parameter accepts a set, but its query raises
`-224,"Illegal parameter value;write-only"` (or a dedicated positive code). It
never echoes the value. A separate boolean query such as "is set" can be offered
if useful. A password that gates other commands should use the SCPI
`SYSTem:PASSword` shape.

---

## 8. Recommended command-tree shape for GTE Core

These are proposals. Names marked *(proposed)* are not taken from any source. The
`SERVice` root is one possible choice of device-specific root. Its job is to keep
the generic tree away from `CONFigure`/`STATus` semantics.

```
# IEEE 488.2 mandatory (13 headers)
*IDN?  *RST  *CLS  *ESE <n>  *ESE?  *ESR?  *OPC  *OPC?  *SRE <n>  *SRE?  *STB?  *TST?  *WAI

# SCPI-99 required
SYSTem:ERRor[:NEXT]?                      -> -113,"Undefined header"
SYSTem:ERRor:COUNt?                       (optional, cheap)
SYSTem:VERSion?                           -> 1999.0
STATus:OPERation[:EVENt]? | :CONDition? | :ENABle <NRf> | :ENABle?
STATus:QUEStionable[:EVENt]? | :CONDition? | :ENABle <NRf> | :ENABle?
STATus:PRESet

# Reset / lifecycle
SYSTem:PRESet                             (optional: front-panel-style state)
SYSTem:SECurity:IMMediate                 Factory Reset: rebuild config storage, keep cal/ID, reboot
SYSTem:REBoot                             restart (vendor convention, [K3])

# SCPI-conventional aliases for core-owned Config Parameters
SYSTem:COMMunicate:LAN:DHCP <Boolean>
SYSTem:COMMunicate:LAN:IPADdress "<a.b.c.d>"   ? [CURRent|STATic]
SYSTem:COMMunicate:LAN:SMASk "<mask>"          ? [CURRent|STATic]
SYSTem:COMMunicate:LAN:GATeway "<addr>"        ? [CURRent|STATic]
SYSTem:COMMunicate:LAN:MAC?                    [query only]
SYSTem:COMMunicate:LAN:UPDate                  [event] apply LAN changes
SYSTem:BEEPer:STATe <Boolean>                  (if the Buzzer has a Config Parameter)

# Generic Service access (proposed, device-specific root)
SERVice:CATalog?                                         -> "net","display","psu",...
SERVice:PARameter:CATalog? "<svc>"                       -> "ip","dhcp",...
SERVice:PARameter "<svc>","<param>",<value>|MIN|MAX|DEF
SERVice:PARameter? "<svc>","<param>"[,MIN|MAX|DEF]       -> natural type; secrets -> error
SERVice:PARameter:DEFault "<svc>"                        [event] one Service to defaults
SERVice:STATus:CATalog? "<svc>"
SERVice:STATus? "<svc>","<value>"                        [query only]

# Instrument-specific (Application Service), registered by each Instrument
SOURce/SENSe/OUTPut/MEASure/...<n>:...   SCPI-standard subsystems with numeric suffixes
```

This costs about 30–35 registered commands for the core, leaving more than 200
for Application Services. Before committing to the root name, check that every
generic-tree keyword obeys the short-form rule (`SERVice`→`SERV`,
`PARameter`→`PAR`, `CATalog`→`CAT`) and does not collide with SCPI paths the
Instruments will use.

## Open points for the design ticket

- Choose the root name for the generic tree (`SERVice` is a proposal).
  `SYSTem:…` is also defensible for core Services ("functions not related to
  instrument performance", [S1] Vol 2 ch 21), but not for Application Services.
- Decide whether `*RST` also resets live (non-EEPROM) state of every Service, or
  only of the Application Service.
- Decide whether Services get their own status fan-out registers (E36300 ISUM
  style) or only use OPER/QUES designer bits.
- Choose the error-code block allocation for positive (device-defined) codes per
  Service.
