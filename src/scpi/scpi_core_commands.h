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
 * example: *IDN? | M0WUT,P0004-SW-002,2F0045001851333035383533,v1.2.0 | Made by M0WUT; Instrument name P0004-SW-002; STM32 unique ID 2F0045001851333035383533; firmware version v1.2.0.
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
 * example: *ESE 60 | Enables bits 2 to 5 (Query, Device-dependent, Execution and Command Error), so any error sets the ESB bit (bit 5) in the status byte.
 */

/** @scpi
 * command: *ESE
 * form: query
 * brief: Read the Standard Event Status Enable register.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 * example: *ESE? | 60 | Bits 2 to 5 are enabled: any Query, Device-dependent, Execution or Command Error sets the ESB bit in the status byte.
 */

/** @scpi
 * command: *ESR
 * form: query
 * brief: Read and clear the Standard Event Status register.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 * example: *ESR? | 32 | Bit 5 (Command Error) is set: a command was not understood since the register was last read. Reading it has cleared it to 0.
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
 * example: *OPC? | 1 | All pending operations have finished.
 */

/** @scpi
 * command: *SRE
 * form: set
 * syntax: *SRE <mask>
 * brief: Set the Service Request Enable register.
 * param: mask | integer 0-255 | Status byte bits that generate a service request
 * errors: -222
 * group: IEEE 488.2 common commands
 * example: *SRE 32 | Requests service when the ESB bit (bit 5, Standard Event Status summary) of the status byte is set.
 */

/** @scpi
 * command: *SRE
 * form: query
 * brief: Read the Service Request Enable register.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 * example: *SRE? | 32 | A service request is generated only when the ESB bit (bit 5) of the status byte is set.
 */

/** @scpi
 * command: *STB
 * form: query
 * brief: Read the status byte.
 * details: Bit 2 error queue not empty, bit 3 QUEStionable summary, bit 4 message available, bit 5 standard event summary, bit 6 master summary, bit 7 OPERation summary.
 * response: Integer 0-255
 * group: IEEE 488.2 common commands
 * example: *STB? | 4 | Only bit 2 is set: the error queue holds at least one entry. Read it with SYSTem:ERRor?.
 */

/** @scpi
 * command: *TST
 * form: query
 * brief: Run the self-test.
 * response: 0 if the self-test passed, otherwise a non-zero failure code
 * group: IEEE 488.2 common commands
 * example: *TST? | 0 | The self-test passed.
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
 * example: SYST:ERR? | -113,"Undefined header" | The oldest entry is SCPI-99 error -113: a command that the instrument does not recognise was sent.
 * example: SYST:ERR? | 105,"Recovered from a watchdog reset" | The oldest entry is the instrument's Error Code 105, a Warning raised after a watchdog reset.
 * example: SYST:ERR? | 0,"No error" | The error queue is empty.
 */

/** @scpi
 * command: SYSTem:ERRor:COUNt
 * form: query
 * brief: Number of entries in the error queue.
 * response: Integer
 * group: System
 * example: SYST:ERR:COUN? | 2 | The error queue holds two entries.
 */

/** @scpi
 * command: SYSTem:VERSion
 * form: query
 * brief: SCPI version implemented.
 * response: 1999.0
 * group: System
 * example: SYST:VERS? | 1999.0 | The instrument implements SCPI version 1999.0.
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
 * example: STAT:OPER? | 256 | Bit 8 (Application Logging) has been set since the register was last read. Reading it has cleared it to 0.
 */

/** @scpi
 * command: STATus:OPERation:CONDition
 * form: query
 * brief: Read the OPERation condition register.
 * response: Integer
 * group: Status
 * example: STAT:OPER:COND? | 256 | Bit 8 is set: Application Logging is running now.
 */

/** @scpi
 * command: STATus:OPERation:ENABle
 * form: set
 * syntax: STATus:OPERation:ENABle <mask>
 * brief: Set which OPERation bits are summarised in the status byte.
 * param: mask | integer 0-65535 | Enable mask
 * errors: -222
 * group: Status
 * example: STAT:OPER:ENAB 256 | Enables bit 8 only, so Application Logging sets the OPERation summary bit (bit 7) in the status byte.
 */

/** @scpi
 * command: STATus:QUEStionable:EVENt
 * form: query
 * syntax: STATus:QUEStionable[:EVENt]?
 * brief: Read and clear the QUEStionable event register.
 * details: Bit 4: warning temperature. Bit 9: a Warning is active.
 * response: Integer
 * group: Status
 * example: STAT:QUES? | 512 | Bit 9 has been set since the register was last read: a Warning was raised. Reading it has cleared it to 0.
 */

/** @scpi
 * command: STATus:QUEStionable:CONDition
 * form: query
 * brief: Read the QUEStionable condition register.
 * response: Integer
 * group: Status
 * example: STAT:QUES:COND? | 528 | Bits 4 and 9 are set (16 + 512): a temperature is above its Warning temperature, and a Warning is active.
 */

/** @scpi
 * command: STATus:QUEStionable:ENABle
 * form: set
 * syntax: STATus:QUEStionable:ENABle <mask>
 * brief: Set which QUEStionable bits are summarised in the status byte.
 * param: mask | integer 0-65535 | Enable mask
 * errors: -222
 * group: Status
 * example: STAT:QUES:ENAB 528 | Enables bits 4 (Warning temperature) and 9 (Warning active), so either sets the QUEStionable summary bit (bit 3) in the status byte.
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
 * response: Comma-separated quoted Service names, e.g. "CORE","NETWORK","THERMAL"
 * group: Generic settings and status
 * example: SYST:GTE:CAT? | "CORE","NETWORK","WEB","DISPLAY","UI","THERMAL","POWER","STORAGE","USB_DEVICE","USB_HOST","SCPI","HOME_ASSISTANT","APPLICATION_EXAMPLE" | The instrument has these thirteen Services: the GTE Core Services and its Application Service, APPLICATION_EXAMPLE.
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
 * example: SYST:GTE:PAR:CAT? "CORE" | "logLevel" | The CORE Service has one Config Parameter, logLevel (its Log level).
 */

/** @scpi
 * command: SYSTem:GTE:PARameter
 * form: set
 * syntax: SYSTem:GTE:PARameter "<service>","<parameter>",<value>|MIN|MAX|DEF
 * brief: Set a Config Parameter.
 * details: Runs the same checks as the web page and the front panel, then saves to EEPROM. Live parameters take effect at once; Reset parameters after SYSTem:REBoot.
 * param: service | string | Service name
 * param: parameter | string | Config Parameter id
 * param: value | number, boolean (ON/OFF/1/0), enumeration name (unquoted) or quoted string | New value, or MIN, MAX or DEF for numeric parameters
 * errors: -224, -222, -221
 * group: Generic settings and status
 * example: SYST:GTE:PAR "CORE","logLevel",DEBUG | Sets the CORE Service's Log level to DEBUG, so its debug messages are printed and saved too.
 */

/** @scpi
 * command: SYSTem:GTE:PARameter
 * form: query
 * syntax: SYSTem:GTE:PARameter? "<service>","<parameter>"[,MIN|MAX|DEF]
 * brief: Read a Config Parameter, or its minimum, maximum or default.
 * details: Secret Config Parameters (e.g. passwords) cannot be read and return an error.
 * param: service | string | Service name
 * param: parameter | string | Config Parameter id
 * response: The value; booleans as 1 or 0, enumerations as their name (unquoted), text as a quoted string
 * errors: -224, -221
 * group: Generic settings and status
 * example: SYST:GTE:PAR? "CORE","logLevel" | INFO | The CORE Service's Log level is INFO.
 */

/** @scpi
 * command: SYSTem:GTE:PARameter:DEFault
 * form: set
 * syntax: SYSTem:GTE:PARameter:DEFault "<service>"
 * brief: Reset every Config Parameter of one Service to its default.
 * param: service | string | Service name
 * errors: -224
 * group: Generic settings and status
 * example: SYST:GTE:PAR:DEF "CORE" | Resets every Config Parameter of the CORE Service to its default, so its Log level returns to INFO.
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
 * example: SYST:GTE:RES:REQ? | 0 | No change is waiting for a restart.
 */
