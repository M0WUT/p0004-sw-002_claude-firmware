# GTE Firmware

Firmware for Generic Test Equipment: a common platform of boards and software from which multiple M0WUT test instruments are built.

## Hardware

**GTE**:
Generic Test Equipment, the shared platform (boards, enclosure and core firmware) that every Instrument is built on.
_Avoid_: testkit, generic board

**Instrument**:
One concrete GTE product: an LCD Board, an IO Board model and an Instrument Board, together with that Instrument's firmware.
_Avoid_: device, unit, test equipment

**LCD Board**:
The front-panel board (P0004-PCB-001) with the main MCU, TFT, buttons, buzzer, Ethernet, SD card, USB and the user IO connectors.
_Avoid_: main board, generic board, MCU board

**IO Board**:
The board (P0004-PCB-002) that the LCD Board connects to. It handles power-source muxing and also has the status LEDs, the fan controller and the IO Expander.
_Avoid_: baseboard, power board

**IO Board model**:
One of a small number of IO Board variants, identified by a model number stored in an I2C EEPROM on the IO Board.
_Avoid_: IO Board revision, IO Board type

**IO Expander**:
The STM32F030 on the IO Board, which presents IO Board functions to the LCD Board over I2C.
_Avoid_: coprocessor, slave MCU

**Instrument Board**:
The instrument-specific hardware fitted in the GTE enclosure's custom space. Each Instrument has its own design.
_Avoid_: daughterboard, custom hardware, add-on board

**Buzzer**:
The passive sound output on the LCD Board.
_Avoid_: speaker

## Firmware

**GTE Core**:
The firmware common to every Instrument, versioned on its own and included by each Instrument's firmware.
_Avoid_: generic firmware, base firmware, framework

**Instrument Firmware**:
The firmware for one Instrument: GTE Core plus that Instrument's own Services and its Instrument Board definitions.
_Avoid_: application, project

**Service**:
A firmware module responsible for exactly one element of the system. A Service may or may not own hardware, and has Config Parameters and Status Values.
_Avoid_: module, task, driver

**Instrument Service**:
The one Service per Instrument that handles that Instrument's specific behaviour, including registering the sensors on its Instrument Board.
_Avoid_: application service, custom service

**Helper**:
Shared infrastructure used by every Service (logging, time, EEPROM access, the Service registry). A Helper is not a Service and has no Config Parameters.
_Avoid_: utility, manager

**Config Parameter**:
A user-editable setting belonging to a Service. It is saved to the LCD Board EEPROM and loaded at startup.
_Avoid_: setting, option, config field

**Apply Mode**:
How a change to a Config Parameter takes effect: **Live** (the Service uses the new value straight away) or **Reset** (the change is saved and takes effect after the device restarts).
_Avoid_: update mode, reload type

**Secret Config Parameter**:
A Config Parameter that can be set but never read back, such as a password.
_Avoid_: hidden field, private setting

**Fatal Error**:
A failure, raised by any Service, after which the device stops all normal operation and shows the failure until it is restarted.
_Avoid_: crash, panic, hard fault

**Status Value**:
A read-only, live value a Service reports about itself, such as whether something is running.
_Avoid_: status field, telemetry, reading
