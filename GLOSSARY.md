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
The board that the LCD Board connects to and draws power from (P0004-PCB-002 is the first IO Board model). It handles power-source muxing and has the Source LEDs, the fans and the IO Expander.
_Avoid_: baseboard, power board

**IO Board model**:
One of a small number of IO Board designs, identified by a model number in its Board Identity. Each model has its own IO Expander register map and its own IO Expander firmware.
_Avoid_: IO Board revision, IO Board type

**IO Expander**:
The microcontroller on an IO Board (an STM32F030 on P0004-PCB-002) that presents that board's fixed functions (power-source status, Source LEDs, fans) to the LCD Board over I2C. Every IO Board model has one with the same style of interface, but each model has its own register map. It offers no general-purpose IO: an Instrument that needs IO has it on its Instrument Board.
_Avoid_: coprocessor, slave MCU, GPIO expander

**Valid source**:
A power source on the IO Board that is present and able to power GTE.
_Avoid_: available source, present source

**Selected source**:
A Valid source that the IO Board is currently drawing power from.
_Avoid_: active source, current source

**Source LED**:
The RGB LED for one power source on the IO Board, showing that source's input state. The IO Expander drives it by itself, so it works even when the LCD Board is unpowered; the Power Service can take it over.
_Avoid_: power LED, status LED

**Board Identity**:
The write-protected record on an IO Board or Instrument Board, written in factory test, that states its type, revision and serial number.
_Avoid_: board ID, board info, ID EEPROM

**Instrument Board**:
The instrument-specific hardware fitted in the GTE enclosure's custom space. Each Instrument has its own design.
_Avoid_: daughterboard, custom hardware, add-on board

**Connected PC**:
The computer plugged into the LCD Board's USB device port.
_Avoid_: host, USB host, computer

**USB stick**:
A mass-storage device plugged into the LCD Board's USB-A port.
_Avoid_: flash drive, thumb drive, USB drive, pen drive

**Volume**:
A storage medium the firmware can log to: the SD card or the USB stick. Each Volume appears to the Connected PC as one drive.
_Avoid_: disk, drive, LUN, partition

**Logging**:
The state, set by the Application Service, in which the Instrument records data to one or more Volumes (its log destinations), none of which may have PC Access on.
_Avoid_: recording, capture, data logging

**PC Access**:
The per-Volume setting that hands the Volume to the Connected PC. While it is off (the default after every power-up), only the firmware uses the Volume and the Connected PC sees no medium; while it is on, the Connected PC owns the Volume and the firmware does not write to it.
_Avoid_: passthrough mode, USB mode, Locked, unlocked

**Soft Key**:
One of the four buttons below the LCD, whose meaning on the current screen is shown by the Soft Key Legend.
_Avoid_: hard key, arrow key, up/down/left/right button

**Soft Key Legend**:
The on-screen labels over the bottom of the page showing what each Soft Key does. It is always visible, except on the Application's own screens, where it hides after 5 s without a press.
_Avoid_: button bar, hint bar, menu bar

**Factory Reset**:
Returning every Service to its default Config Parameters by rebuilding the config storage from scratch.
_Avoid_: wipe, format, reset to defaults (that is one Service only)

**Buzzer**:
The passive sound output on the LCD Board.
_Avoid_: speaker

## Firmware

**GTE Core**:
The firmware common to every Instrument, versioned on its own and included by each Instrument's firmware.
_Avoid_: generic firmware, base firmware, framework

**Instrument Firmware**:
The firmware for one Instrument: GTE Core plus that Instrument's Application Service, any other Services it adds, and its Instrument Board definitions.
_Avoid_: application, project

**Service**:
A firmware module responsible for exactly one element of the system. A Service may or may not own hardware, and has Config Parameters and Status Values.
_Avoid_: module, task, driver

**Application Service**:
The one Service per Instrument that handles that Instrument's own behaviour, including registering the sensors on its Instrument Board.
_Avoid_: Instrument Service, custom service, instrument-specific service

**Helper**:
Shared infrastructure used by every Service (logging, time, EEPROM access, the Service registry). A Helper is not a Service and has no Config Parameters.
_Avoid_: utility, manager

**Board Helper**:
The Helper that reads the Board Identities at startup and tells Services which IO Board and Instrument Board are fitted.
_Avoid_: IO Board Service, board manager

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

**Sensor**:
One measured quantity (temperature, voltage, current, power or fan speed) from one physical source, owned by the Service that handles that quantity.
_Avoid_: probe, channel, measurement point

**Warning temperature**:
The threshold above which a temperature Sensor raises a Warning and every fan runs at full speed while the Instrument keeps running.
_Avoid_: overtemperature, thermal warning

**Critical temperature**:
The threshold above which a temperature Sensor causes a Fatal Error, with every fan at full speed and the Application shut down.
_Avoid_: overtemperature, thermal shutdown, overheat

**Warning**:
A problem the device can carry on through: a condition that pops up briefly when raised and stays listed until the user or the raising code clears it.
_Avoid_: alert, notification, alarm

**Error Code**:
The stable number identifying a Warning or Fatal Error, together with its default message: 100 × (ServiceId + 1) plus a number unique within that Service, with the lowest range reserved for problems outside any Service.
_Avoid_: fault code, error id, warning id

**Status Value**:
A read-only, live value a Service reports about itself, such as whether something is running.
_Avoid_: status field, telemetry, reading
