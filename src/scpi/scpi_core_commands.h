// Copyright 2026 M0WUT
// SCPI commands provided by GTE Core.
//
// Each command is documented in a /** @scpi ... */ block, which is the only source for the
// generated SCPI manual (p0004-sw-003_scpi-manual-generator). These are
// comments: none of the text reaches the firmware image. The registration code
// will sit next to each block when the SCPI Service is built; until then this
// file holds the documentation for the agreed command set ("The SCPI command
// tree", issue #15).
#pragma once

// ------------------------------------------------- IEEE 488.2 common commands

/** @scpi
 * command: *IDN
 * form: query
 * brief: Identify the instrument.
 * response: Four comma-separated fields: M0WUT,<Instrument name>,<STM32 unique ID>,<Instrument firmware version>
 * group: IEEE 488.2 common commands
 * example: *IDN?
 */

/** @scpi
 * command: *RST
 * form: event
 * brief: Return the instrument to a safe remote state.
 * details: Stops Application Logging and calls the Application Service's reset hook. Does not change any Config Parameter, write to EEPROM, or clear the error queue.
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *CLS
 * form: event
 * brief: Clear the status data structures and the error queue.
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *ESE
 * form: set
 * syntax: *ESE <mask>
 * brief: Set the Standard Event Status Enable register.
 * param: mask | integer 0-255 | Bits of the Standard Event Status register that set ESB in the status byte
 * errors: -222
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *ESE
 * form: query
 * brief: Read the Standard Event Status Enable register.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *ESR
 * form: query
 * brief: Read and clear the Standard Event Status register.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *OPC
 * form: event
 * brief: Set the Operation Complete bit when all pending operations have finished.
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *OPC
 * form: query
 * brief: Return 1 when all pending operations have finished.
 * response: 1
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *SRE
 * form: set
 * syntax: *SRE <mask>
 * brief: Set the Service Request Enable register.
 * param: mask | integer 0-255 | Status byte bits that generate a service request
 * errors: -222
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *SRE
 * form: query
 * brief: Read the Service Request Enable register.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *STB
 * form: query
 * brief: Read the status byte.
 * details: Bit 2 error queue not empty, bit 3 QUEStionable summary, bit 4 message available, bit 5 standard event summary, bit 6 master summary, bit 7 OPERation summary.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *TST
 * form: query
 * brief: Run the self-test.
 * response: 0 if the self-test passed, otherwise a non-zero failure code
 * group: IEEE 488.2 common commands
 */

/** @scpi
 * command: *WAI
 * form: event
 * brief: Wait until all pending operations have finished before running further commands.
 * group: IEEE 488.2 common commands
 */

// --------------------------------------------------------------------- SYSTem

/** @scpi
 * command: SYSTem:ERRor:NEXT
 * form: query
 * syntax: SYSTem:ERRor[:NEXT]?
 * brief: Read and remove the oldest entry in the error queue.
 * details: The queue holds SCPI-99 errors (negative) and the instrument's Error Codes (positive, including every Warning raised). When the queue overflows, its newest entry is replaced with -350.
 * response: <code>,"<message>" e.g. 0,"No error"
 * group: System
 * example: SYST:ERR?
 */

/** @scpi
 * command: SYSTem:ERRor:COUNt
 * form: query
 * brief: Number of entries in the error queue.
 * response: Integer
 * group: System
 */

/** @scpi
 * command: SYSTem:VERSion
 * form: query
 * brief: SCPI version implemented.
 * response: 1999.0
 * group: System
 */

/** @scpi
 * command: SYSTem:SECurity:IMMediate
 * form: event
 * brief: Factory Reset: return every Service to its default Config Parameters, then restart.
 * details: Rebuilds the config storage from scratch. The MAC address is not affected.
 * group: System
 */

/** @scpi
 * command: SYSTem:REBoot
 * form: event
 * brief: Restart the device.
 * details: Applies Config Parameters whose Apply Mode is Reset.
 * group: System
 */

// --------------------------------------------------------------------- STATus

/** @scpi
 * command: STATus:OPERation:EVENt
 * form: query
 * syntax: STATus:OPERation[:EVENt]?
 * brief: Read and clear the OPERation event register.
 * details: Bit 8: Application Logging.
 * response: Integer
 * group: Status
 */

/** @scpi
 * command: STATus:OPERation:CONDition
 * form: query
 * brief: Read the OPERation condition register.
 * response: Integer
 * group: Status
 */

/** @scpi
 * command: STATus:OPERation:ENABle
 * form: set
 * syntax: STATus:OPERation:ENABle <mask>
 * brief: Set which OPERation bits are summarised in the status byte.
 * param: mask | integer 0-65535 | Enable mask
 * errors: -222
 * group: Status
 */

/** @scpi
 * command: STATus:QUEStionable:EVENt
 * form: query
 * syntax: STATus:QUEStionable[:EVENt]?
 * brief: Read and clear the QUEStionable event register.
 * details: Bit 4: warning temperature. Bit 9: a Warning is active.
 * response: Integer
 * group: Status
 */

/** @scpi
 * command: STATus:QUEStionable:CONDition
 * form: query
 * brief: Read the QUEStionable condition register.
 * response: Integer
 * group: Status
 */

/** @scpi
 * command: STATus:QUEStionable:ENABle
 * form: set
 * syntax: STATus:QUEStionable:ENABle <mask>
 * brief: Set which QUEStionable bits are summarised in the status byte.
 * param: mask | integer 0-65535 | Enable mask
 * errors: -222
 * group: Status
 */

/** @scpi
 * command: STATus:PRESet
 * form: event
 * brief: Reset the OPERation and QUEStionable enable registers to their defaults.
 * group: Status
 */

// ---------------------------------------------- generic settings (SYSTem:GTE)

/** @scpi
 * command: SYSTem:GTE:CATalog
 * form: query
 * brief: List the Services.
 * response: Comma-separated quoted Service names, e.g. "Core","Network","Thermal"
 * group: Generic settings and status
 */

/** @scpi
 * command: SYSTem:GTE:PARameter:CATalog
 * form: query
 * syntax: SYSTem:GTE:PARameter:CATalog? "<service>"
 * brief: List a Service's Config Parameter ids.
 * param: service | string | Service name, as listed by SYSTem:GTE:CATalog?
 * response: Comma-separated quoted Config Parameter ids
 * errors: -224
 * group: Generic settings and status
 */

/** @scpi
 * command: SYSTem:GTE:PARameter
 * form: set
 * syntax: SYSTem:GTE:PARameter "<service>","<parameter>",<value>|MIN|MAX|DEF
 * brief: Set a Config Parameter.
 * details: Runs the same checks as the web page and the front panel, then saves to EEPROM. Live parameters take effect at once; Reset parameters after SYSTem:REBoot.
 * param: service | string | Service name
 * param: parameter | string | Config Parameter id
 * param: value | number, boolean (ON/OFF/1/0) or quoted string | New value, or MIN, MAX or DEF for numeric parameters
 * errors: -224, -222, -221
 * group: Generic settings and status
 * example: SYST:GTE:PAR "Thermal","warn",62.5
 */

/** @scpi
 * command: SYSTem:GTE:PARameter
 * form: query
 * syntax: SYSTem:GTE:PARameter? "<service>","<parameter>"[,MIN|MAX|DEF]
 * brief: Read a Config Parameter, or its minimum, maximum or default.
 * details: Secret Config Parameters (e.g. passwords) cannot be read and return an error.
 * param: service | string | Service name
 * param: parameter | string | Config Parameter id
 * response: The value; booleans as 1 or 0, text as a quoted string
 * errors: -224, -221
 * group: Generic settings and status
 */

/** @scpi
 * command: SYSTem:GTE:PARameter:DEFault
 * form: set
 * syntax: SYSTem:GTE:PARameter:DEFault "<service>"
 * brief: Reset every Config Parameter of one Service to its default.
 * param: service | string | Service name
 * errors: -224
 * group: Generic settings and status
 */

/** @scpi
 * command: SYSTem:GTE:STATus:CATalog
 * form: query
 * syntax: SYSTem:GTE:STATus:CATalog? "<service>"
 * brief: List a Service's Status Value ids.
 * param: service | string | Service name
 * response: Comma-separated quoted Status Value ids
 * errors: -224
 * group: Generic settings and status
 */

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

/** @scpi
 * command: SYSTem:GTE:RESet:REQuired
 * form: query
 * brief: Whether a Reset-mode Config Parameter change is waiting for a restart.
 * response: 1 if SYSTem:REBoot is needed to apply a change, otherwise 0
 * group: Generic settings and status
 */
