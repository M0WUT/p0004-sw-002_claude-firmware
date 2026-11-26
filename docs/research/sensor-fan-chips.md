# Sensor and fan chips under STM32duino

Research for wayfinder ticket #21, "Sensor and fan chips under STM32duino".
Gathered 2026-11-26.

**Scope** (set by the user mid-ticket):

- **INA3221** is the main focus. It is the only sensor with no driver yet, so it gets the full treatment: the chip in detail, a survey of drivers, and a sketch of an in-house driver.
- **TMP102** already has an in-house driver (sw-001 `lib/tmp102`), so there is no driver survey. This doc only notes the TMP102 features that matter for a shared Sensor abstraction.
- **EMC2302** gets a short summary of what the chip can do, to inform a Fan model. There is no driver survey.

## Sources

| Ref | Source |
|---|---|
| [INA-DS] | TI INA3221 datasheet, SBOS576C (rev. C, September 2026): https://www.ti.com/lit/ds/symlink/ina3221.pdf |
| [TMP-DS] | TI TMP102 datasheet, SBOS397I (rev. I, June 2024): https://www.ti.com/lit/ds/symlink/tmp102.pdf |
| [EMC-DS] | Microchip EMC2301/2/3/5 datasheet, DS20006532A (2021): https://ww1.microchip.com/downloads/en/DeviceDoc/EMC2301-2-3-5-Data-Sheet-DS20006532A.pdf |
| [RT] | RobTillaart/INA3221_RT, v0.4.2, cloned at `1f2bb5c` (2026-01-14): https://github.com/RobTillaart/INA3221_RT |
| [ADA] | adafruit/Adafruit_INA3221, v1.0.3, cloned at `4355880` (2024-11-12): https://github.com/adafruit/Adafruit_INA3221 |
| [BUSIO] | adafruit/Adafruit_BusIO, v1.17.4, cloned at `3b83642` (2025-09-22): https://github.com/adafruit/Adafruit_BusIO |
| [TINYU] | Tinyu-Zhao/INA3221, v0.0.2 tag, cloned at `efc09f4` (2025-05-28): https://github.com/Tinyu-Zhao/INA3221 |
| [SDL] | switchdoclabs/SDL_Arduino_INA3221, cloned at `29deed8` (2019-09-21): https://github.com/switchdoclabs/SDL_Arduino_INA3221 |
| [ZAN] | Zanduino/INA, v1.1.0, cloned at `d89299d` (2022-01-16): https://github.com/Zanduino/INA |
| [STM-WIRE] | stm32duino Arduino_Core_STM32, `libraries/Wire/src/utility/twi.c` (main branch): https://github.com/stm32duino/Arduino_Core_STM32/blob/main/libraries/Wire/src/utility/twi.c |
| [SW001] | M0WUT/p0004-sw-001_generic-arduino-firmware, `lib/tmp102` and `lib/i2c` at `d606648` |

---

## 1. INA3221: what the chip exposes

### Measurements

Each of the 3 channels gives two raw measurements: shunt voltage and bus voltage. **The chip has no current or power register.** Current is V_shunt / R_shunt and power is V_bus × I, and the host has to compute both. [INA-DS §7.3.1, Table 8-1]

| Quantity | Range | LSB / resolution | Format | Source |
|---|---|---|---|---|
| Shunt voltage (IN+ − IN−) | −163.84 mV to +163.8 mV | 40 µV (13-bit incl. sign) | bits 15..3, two's complement, bits 2..0 reserved | [INA-DS §6.5, §8.2.2] |
| Bus voltage (IN− to GND) | 0–26 V allowed at the pins (register full scale is 32.76 V) | 8 mV | bits 15..3, same format | [INA-DS §6.5, §8.2.3] |
| Shunt-voltage sum (chosen channels) | 15-bit | 40 µV | bits 15..1 | [INA-DS §8.2.14] |

- **Accuracy:** shunt offset ±40 µV typ / ±80 µV max, and shunt and bus gain error 0.1 % typ / 0.25 % max. Bus offset is ±8 mV typ / ±16 mV max. [INA-DS §6.5]
- **Input bias:** IN− draws 10 µA ‖ 670 kΩ. Series filter resistors therefore cause an offset, so keep them ≤10 Ω. [INA-DS §6.5, §7.4.3]
- **Supply:** 2.7–5.5 V, 350 µA typ. Operating range −40 to +125 °C. [INA-DS §1, §6.5]

### Conversion time and averaging

- **Conversion time** is set separately for shunt (VSHCT) and bus (VBUSCT) and applies to all channels. The options are 140 µs, 204 µs, 332 µs, 588 µs, 1.1 ms (default), 2.116 ms, 4.156 ms and 8.244 ms; the max values are about 10 % higher. [INA-DS §6.5, Table 8-4]
- **Averaging:** AVG = 1 (default), 4, 16, 64, 128, 256, 512 or 1024, one setting for the whole chip. [INA-DS Table 8-4]
- **The averaging is a running filter, not a block average.** Each new sample moves the output register by (new − old)/N. The result registers therefore update after *every* conversion, and N sets how slowly they settle. This is an exponential moving average. [INA-DS §7.4.1, Fig 7-5] It does **not** reduce the update rate, but the step response gets slower as N grows.
- **Sequencing:** the chip converts shunt then bus for CH1, then CH2, then CH3, skipping disabled channels. A full cycle takes (t_shunt + t_bus) × enabled channels. [INA-DS §7.3.1, §7.4.2.1] Defaults (1.1 ms + 1.1 ms, 3 channels) give about **6.6 ms per full cycle**. TI's example: data every 2 ms on all 3 channels needs 332 µs for both. [INA-DS §7.4.2.2]
- **Modes** (MODE bits): power-down; shunt, bus, or shunt+bus single-shot (triggered); and shunt, bus, or shunt+bus continuous (default). [INA-DS Table 8-4]
  - Single-shot converts every enabled channel once, then powers down. Writing Config again triggers the next one.
  - Coming out of power-down takes 40 µs.
  - Registers can always be read and hold the last result. [INA-DS §7.3.1]
- **CVRF** (Mask/Enable bit 0) sets when all conversions are done. It clears on a Config write or on **any read of Mask/Enable**. [INA-DS §7.3.1, §8.2.16]
- **Config writes:** a write to Config stops the conversion in progress and starts a new one with the new settings. [INA-DS §8.2.1]

### Alerts and limits

There are four open-drain alert pins: **Critical, Warning, PV, TC**. [INA-DS §7.3.2]

| Alert | Compares | Limit regs (reset value) | Flag bits (Mask/Enable 0Fh) | Notes |
|---|---|---|---|---|
| **Critical** | each *single* shunt conversion against a per-channel limit; also the shunt-voltage sum against the sum limit | 07h/09h/0Bh (7FF8h = full scale, so in effect off); sum limit 0Eh (7FFEh) | CF1–3 (bits 9–7), SF (bit 6) | Catches fast overcurrent. SCC1–3 pick which channels feed the sum. The sum only means a total current if all the summed shunts have the same value. Latch with CEN. [INA-DS §7.3.2.1, §8.2.8, §8.2.15] |
| **Warning** | each channel's *averaged* shunt value against a per-channel limit | 08h/0Ah/0Ch (7FF8h) | WF1–3 (bits 5–3) | Catches slow overcurrent. Latch with WEN. [INA-DS §7.3.2.2] |
| **Power-valid (PV)** | all bus voltages against an upper and lower threshold, with hysteresis | 10h upper (2710h = 10.000 V), 11h lower (2328h = 9.000 V), 8 mV LSB | PVF (bit 2), shows the pin state, not latched | PV goes high once **all three** bus voltages are above the upper limit, and drops when any falls below the lower limit. An unused channel's IN− must be tied to a live rail or PV never asserts. Needs bus conversions running. [INA-DS §7.3.2.3, §8.2.17–18] |
| **Timing control (TC)** | at power-up only: after CH1 bus reaches 1.2 V, CH2 must reach 1.2 V within 4 full cycles (about 28.6 ms with defaults) | none | TCF (bit 1), reset value 1 | Any write to Config before the sequence finishes **disables TC** until a power cycle or a RST. [INA-DS §7.3.2.4] |

Flag behaviour that shapes the driver:

- **Reading Mask/Enable clears CF1–3, SF, WF1–3 and CVRF together** (PVF and TCF follow their pins). Writing Mask/Enable does not clear them. [INA-DS §8.2.16] Every Mask/Enable read must therefore keep *all* the flags it returns. If a driver polls CVRF and throws the other bits away, it silently loses alert flags.
- Critical and Warning are *transparent* by default (CEN = WEN = 0). SMBus Alert Response (ARA, 0001 100) "requires alerts in Latch mode to function". [INA-DS §7.3.2, §7.5.3, rev-C change list]
- **Limits are shunt-voltage limits, not current limits.** Only shunt voltage is compared against the per-channel Critical and Warning limits. The chip has **no bus-voltage over or under limit** apart from the shared PV window. [INA-DS §7.3.2]
- All registers are volatile. RST (Config bit 15) restores defaults; the PV *output* state is held through the reset. [INA-DS §7.3.2.5, §7.3.3]

### I2C

- **Address:** one A0 pin, 4 options. GND → 0x40, VS → 0x41, SDA → 0x42, SCL → 0x43. **Up to 4 INA3221 per bus.** A0 is sampled on every transaction. [INA-DS Table 7-1]
  - 0x40–0x43 is also the default range of the INA219/INA226/INA260 family. Any other chip in that range on a shared bus takes away a slot.
  - The LCD Board TMP102 (0x48) and the EMC2302 (0x2E or 0x2F) do not clash.
- **Speed:** Fast mode up to 400 kHz, and HS mode up to 2.44 MHz. Bytes go MSB first, and every register is 16-bit. [INA-DS §7.5.1.2, §8.2]
- **Pointer:** every write starts with a pointer byte, and a read returns the register the pointer last addressed. [INA-DS §7.5.2]
- **SMBus timeout:** if SCL is held low for more than 28 ms (35 ms max), the INA3221 resets its interface. [INA-DS §6.5, §7.5.1]
- **ID registers:** 0xFE Manufacturer ID = 0x5449 ("TI") and 0xFF Die ID = 0x3220. [INA-DS §8.2.19–20] Checking both is a cheap way to confirm the chip is present and is what we expect, beyond a bare address ACK.

---

## 2. INA3221: driver survey

A cross-cutting constraint applies to every Arduino driver. On STM32duino, `TwoWire` transfers are interrupt-driven HAL calls, but `twi.c` **busy-waits** for completion with `HAL_GetTick()` polling, up to `I2C_TIMEOUT_TICK = 100` ms. It never yields to FreeRTOS. [STM-WIRE lines 50–51, 1058–1094]

- A healthy 16-bit register read at 400 kHz is about 100 µs, so this cost is small.
- A stuck bus burns up to 100 ms of CPU per call, at the calling task's priority.
- Any driver built on `Wire` inherits this. Whether a driver blocks is decided by the `TwoWire` layer, not the driver. None of the drivers surveyed add `delay()` calls of their own. [RT, ADA, TINYU, checked by grep]

### Comparison

| Library | Licence | Last release / commit | Bus handle | Error reporting | Coverage | Verdict |
|---|---|---|---|---|---|---|
| **RobTillaart/INA3221_RT** [RT] | MIT | 0.4.2, 2026-01-14 (0.4.1, 2025-03-06 added error handling) | `INA3221(addr, TwoWire* = &Wire)`; one object per chip, so several chips are easy | `getLastError()` latches −1 (write/pointer NACK) or −2 (short read); reads return 0 on error | Bus/shunt V, I (float R), P (host-computed); critical and warning limits (µV or mA, positive only); sum and sum limit; config fields; PV limits; raw `getMaskEnable()`; IDs | Best of the bunch, but its README marks it **"Experimental, USE WITH CARE"**, "only tested partially with an Arduino UNO", and alerts "(not tested)" |
| **adafruit/Adafruit_INA3221** [ADA] | MIT (lib), MIT (BusIO) | 1.0.3, 2024-11-12; no commits since | `begin(addr, TwoWire* = &Wire)` via Adafruit_BusIO | Silent. A failed read returns `0xFFFFFFFF`, which comes back as −40 µV or −8 mV [BUSIO `Register::read`] | Bus/shunt V, I; config enums; critical and warning (A); PV limits; summation channels; `getFlags()`; checks IDs in `begin()` | Usable, but see the defects below. It also pulls in BusIO and `new`s an `Adafruit_I2CDevice` |
| **Tinyu-Zhao/INA3221** [TINYU] (fork of Beast Devices' 2020 library) | MIT | v0.0.2; last commit 2025-05-28; low activity | `begin(TwoWire* = &Wire)` | None: `_read`/`_write` return `void`, and a failed read leaves the old value | Wide: all limits, latch enables, flag getters, optional compensation for input-bias offset through filter resistors | Do not use: no error path, and a byte-order bug in `_read` (below) |
| **switchdoclabs/SDL_Arduino_INA3221** [SDL] | GPL-2.0 (plus a BSD file) | 2019-09-21 | hard-wired `Wire` | none | basic V/I | Stale, and GPL. Not a candidate |
| **Zanduino/INA** [ZAN] | GPL-3.0 | v1.1.0, 2022-01-16 | global `Wire` | some | multi-INA family including INA3221 | GPL-3 and inactive. Not a candidate |

### Defects found by reading the source

**Adafruit_INA3221 / BusIO** [ADA, BUSIO]

- **A failed read during read-modify-write can reset the chip.**
  - Every setter (`setMode`, `setAveragingMode`, `enableChannel`, …) uses `Adafruit_BusIO_RegisterBits::write()`. That function reads the register, masks in the field, and writes the result back.
  - If the read fails, `read()` returns `0xFFFFFFFF`. All the other bits, **including RST (bit 15)**, go back as 1, so one bus glitch during a config change becomes a full device reset that nobody sees. [BUSIO `Adafruit_BusIO_Register.cpp` `read()`, `RegisterBits::write()`]
- **`begin()` re-initialises the bus.** `Adafruit_I2CDevice::begin()` calls `_wire->begin()` [BUSIO `Adafruit_I2CDevice.cpp:31`]. On a shared `TwoWire` that re-runs `i2c_init` underneath any other user. This is harmless only if every driver is set up under the bus mutex before other tasks run.
- **Silent failures.** A failed read shows up as a plausible small negative value.

**INA3221_RT** [RT]

- The `getDieID()` comment says 0x2260, but the datasheet says 0x3220 [INA-DS §8.2.20]. The README admits the author's chip returns 0x3220. Cosmetic.
- Config setters do a read then a write as two separate bus transactions, so they are only atomic under an external mutex.
  - If the read fails, it returns 0 with `_error` set, but the setter still writes `0 | field`. That disables every channel (CHxen = 0) instead of resetting the chip. It is less harmful than Adafruit's failure, but still silent unless the caller checks `getLastError()`.
- Critical and warning setters take `uint32_t` µV, so negative (reverse-current) limits are impossible. That is fine for our use.
- `getPower()` does 2 separate reads (bus, then shunt), so the pair may come from different conversions. That is fine for slow monitoring.
- `isConnected()` only checks the address ACK. It never reads the IDs.

**Tinyu-Zhao/INA3221** [TINYU]

- `*val = ((_i2c->read() << 8) | _i2c->read());` puts two `read()` calls in one expression. C++ does not fix their order, so the compiler may swap the bytes.
- `_read` also leaves `*val` untouched if `available()` is 0. A missing chip therefore reads as 0 V / 0 A.

### Fit with a mutex-protected `I2CBus`

All three candidates take a `TwoWire*`, so any of them can share a bus. sw-001's `I2CBus` keeps the `TwoWire` as a private member and hands it only to the `I2CDevice` friend class [SW001 `lib/i2c/i2c.h`], so we would have to expose it. **But none of the libraries can take the mutex themselves.** Every caller would need to wrap every library call in a lock: `lock(); ina.getBusVoltage(1); unlock();`. That is workable, but in the end the driver adds little except register constants.

### Recommendation: thin in-house driver for INA3221

A thin in-house driver on top of the GTE `I2CBus`/`I2CDevice` costs little and fixes everything above. It is about 150–200 lines. What it needs:

1. **Two primitives.** `bool read16(reg, uint16_t&)` and `bool write16(reg, uint16_t)`, both big-endian with the error returned. The bus wrapper takes the mutex, so each register access is one locked transaction.
   - sw-001's `I2CDevice::read16` defaults to `littleEndian = true` and returns no error, so it needs fixing or a BE variant (see §3).
2. **`probe()`** reads 0xFE == 0x5449 and 0xFF == 0x3220. This gives absent-chip and wrong-chip detection.
3. **`configure(const Ina3221Config&)`** writes the whole Config register (channel enables, AVG, VBUSCT, VSHCT, MODE) **in one write**, with no read-modify-write. The same applies to Mask/Enable (SCC, WEN, CEN).
   - The driver keeps a shadow copy of both registers, so `configure()` can be re-run after a detected reset or a hot-plug.
4. **`readChannel(ch, Reading&)`** reads the shunt then the bus register, converts with `(int16_t)raw >> 3` × 40 µV / 8 mV, and works out I = V_shunt / R_shunt from a per-channel milliohm value. It returns false on any bus error. Values are never faked.
5. **Limits:**
   - `setCriticalLimit(ch, amps)` and `setWarningLimit(ch, amps)` convert through R_shunt to `(uV/40) << 3`, clamped to 0x7FF8.
   - `setSumLimit` (bits 14..1).
   - `setPowerValid(upper_mV, lower_mV)`, where the raw value equals mV because LSB 8 mV << 3 is 1 mV per count.
6. **`readFlags()`** reads Mask/Enable once and returns *all* the flags (CF1–3, SF, WF1–3, PVF, TCF, CVRF). Callers must not read Mask/Enable anywhere else, because each read clears the flags.
7. **Optional single-shot.** Write Config with MODE = 011, then wait for CVRF, or wait (t_sh + t_bus) × enabled channels. Do the wait with `vTaskDelay`, never a busy-wait.
8. **Per-channel metadata** comes from board config, not the driver: name, shunt resistance, whether the channel is fitted.

If the team would rather vendor a library, **INA3221_RT** (MIT, small, `TwoWire*`, has `getLastError()`) is the one to start from. Plan to (a) check `getLastError()` after every call, (b) never call its config setters except under the bus mutex, and (c) do ID-based probing outside it.

---

## 3. TMP102: features relevant to a shared Sensor model

Driver survey skipped by request. The facts below are from [TMP-DS].

| Item | Value | Source |
|---|---|---|
| Range / accuracy | −40 to +125 °C; ±0.5 °C typ / ±2 °C max (−25…85 °C); ±1 / ±3 °C (−40…125 °C) | §5.5 |
| Resolution | 12-bit, 0.0625 °C/LSB, left-justified (bits 15..4), two's complement; EM = 1 gives 13-bit for readings above 128 °C | §6.3.1, §6.4.2 |
| Conversion | 10 ms typ (15 ms max). Continuous at 0.25, 1, **4 (default)** or 8 Hz. One-shot from shutdown (OS bit) allows 80+ conversions/s | §5.5, §6.4.1, §6.4.4 |
| Limits | T_HIGH (reset 80 °C) and T_LOW (reset 75 °C), same format as the temperature register | §6.5.4 |
| Alert | Comparator mode (TM = 0): active at ≥ T_HIGH, clears below T_LOW, so there is hysteresis. Interrupt mode (TM = 1): latched until any register read or SMBus ARA. Polarity via POL (default active-low). Fault queue of 1/2/4/6 consecutive faults. AL bit gives the comparator state in software | §6.4.5, §6.5.3.3–8 |
| Address | 0x48 (A0 = GND), 0x49 (V+), 0x4A (SDA), 0x4B (SCL): up to 4 per bus | Table 6-4 |
| ID | **No ID register**, so the only presence check is an address ACK plus a sane config read-back (power-up config is 0x60A0) | §6.5 |
| Timeout | resets its interface if SCL is low for 30 ms typ | §6.3.10 |

For GTE:

- The PD11 alert works as a hardware over-temperature line. Comparator mode with a fault queue of 4 or 6 gives a level signal with hysteresis and no software latching to manage.
- Reading at 1 Hz or slower easily keeps up with the 4 Hz default.

**Things to check in the existing sw-001 driver** [SW001]:

- `TMP102::read_temperature()` calls `read16(REG_TEMPERATURE)`, and the default `littleEndian = true` builds `data[1] << 8 | data[0]`. The TMP102 sends MSB first [TMP-DS §6.5.2, Table 6-13 note]. As written, the bytes look **swapped**: 25 °C (0x19 0x00) would read as 0x0019 >> 4, which is 0.0625 °C.
  - No caller of `TMP102` exists in sw-001, so the driver may never have run on hardware.
  - Check this when porting: pass `littleEndian = false`.
- `I2CDevice::read16` returns no error. A NACK leaves `data` as zeros, which reads as 0 °C, a plausible value. The port should return a status instead.
- `_read_bytes` calls `endTransmission()` again after `requestFrom()`, with no `beginTransmission()`. On STM32duino a zero-length transmit becomes an address probe [STM-WIRE `i2c_master_write` size == 0 path → `i2c_IsDeviceReady`]. The result is an extra, harmless but wasted bus transaction on every read.

---

## 4. EMC2302: capabilities summary for a Fan model

Driver survey skipped by request. The facts below are from [EMC-DS].

**Addresses** (§5.2):

- EMC2302-1: 0x2E (0101_110b). EMC2302-2: 0x2F (0101_111b). The address is fixed by the part variant, with no pin.
- Product ID (0xFD) = 0x36, Manufacturer ID (0xFE) = 0x5D, Revision (0xFF) = 0x80 (Table 6-1). These make a proper presence and identity check.

**Bus** (§1.3, §5.6–5.7, Table 6-1):

- SMBus at 10–400 kHz. The device never stretches the clock.
- There is a 30 ms SMBus timeout, but the Config POR value 40h has DIS_TO = 1, so it is disabled by default.
- It supports block read and write with auto-increment.

**Drive:**

- Each channel has a PWM output, open-drain (default) or push-pull, with selectable polarity (§4.2, Reg 6-8).
- Base frequency 26 kHz (default), 19.531 kHz, 4.882 kHz or 2.441 kHz, plus an 8-bit divider, which reaches down to about 9.5 Hz.
- 256 duty steps (§1.2, §4.2).

**Two modes per fan** (§4.1):

1. **Direct Setting:** write Fan Setting (0x30/0x40), where duty = value/255.
2. **FSC (closed-loop RPM):**
   - Set ENAGx and write a **TACH Target** (13-bit, 0x3C/0x3D and 0x4C/0x4D). The target is a period count, not an RPM.
   - The chip runs a PID loop with gain ×1/2/4/8, an update time of 100 ms to 1.6 s (default 400 ms), an error window of 0/50/100/200 RPM, a minimum drive (default 40 %) and a max step (§4.3, Reg 6-13…6-18).

**Tach** (§4.4, Eq. 4-3, Reg 6-13):

- TACH Reading (0x3E/0x3F, 0x4E/0x4F) is a 13-bit count of 32.768 kHz clocks per revolution.
- RPM = 3 932 160 × m / COUNT for a 2-pole fan sampled on 5 edges, where m = 1/2/4/8 from RANGE (500/1000/2000/4000 RPM minimum).
- The measurable range is 480–16 000 RPM.
- Accuracy is ±1 % typ (±2 % max) with the internal oscillator, or ±0.5 % with an external 32.768 kHz clock (§1.2).
- Reading the high byte latches the low byte, so a two-byte read is coherent (§6.1).

**Spin-up** (§4.8, Table 4-10/4-11):

- Runs on 0 → non-zero in Direct mode, or when the target leaves FFh or a stall is seen in FSC.
- The fan gets 100 % for ¼ of the spin-up time (the kick, which NKCK can disable), then 30–65 % (default 60 %).
- Total spin-up time is 250 ms, 500 ms (default), 1 s or 2 s.
- If the TACH count is still above Valid TACH Count at the end, FNSPIN sets and the routine retries.

**Fault detection** (§4.4.3–4.4.4, §6.3–6.7):

- **Stall:** TACH count above Valid TACH Count (default F5h) sets FxSTL. In FSC mode the chip retries spin-up by itself. FSC checks only at each update time.
- **Spin-up failure:** FxSPIN.
- **Drive fail / aging fan:** the target is not reached at full drive within a band, checked over 16/32/64 update periods (off by default). Sets DRVFx.
- All status bits clear on read. The summary register 0x24 has WATCH, DVFAIL, FNSPIN and FNSTL.
- **ALERT#** is asserted per fan if FxITEN is set (off by default), and can be masked globally with Config.MASK. SMBus ARA is supported.

**Watchdog** (§4.11): after power-up, if 4 s pass with no Fan Setting write and no ENAG set, every fan goes to **100 %** and WATCH plus ALERT are set. Continuous watchdog mode (WD_EN) can stay armed during operation, and any SMBus access re-arms it. This is a good safety net for firmware lockup: fans go to full speed if the H743 stops talking to it.

**Software lock** (§6.1, 0xEF): setting LOCK makes the SWL registers (gains, spin-up, min drive, valid TACH and others) read-only until power-cycle.

Implications for a Fan model:

- The natural control input is **target RPM** (FSC), with **duty %** as a fallback mode.
- Reported values are **measured RPM** (from the count, using poles, edges and range from config) plus **status flags**: stalled, spin-up failed, drive failed, watchdog fired.
- Per-fan constants (poles, minimum RPM/range, minimum drive, valid-TACH threshold) are board or fan configuration, not runtime state.
- Firmware must write a fan setting or enable FSC within **4 s of EMC2302 power-up**, or the fans go to full speed. That is safe, but the fans will be loud during a slow boot.

---

## 5. What this means for a shared "Sensor" abstraction

| Concern | TMP102 | INA3221 | EMC2302 (as a sensor) |
|---|---|---|---|
| Quantities | temperature (°C) | per channel: bus V, shunt V, and derived current (A) and power (W) | per fan: speed (RPM), plus drive % read back |
| Raw resolution | 0.0625 °C | 8 mV bus, 40 µV shunt (so current LSB = 40 µV / R_shunt) | about 1 % RPM; 13-bit count |
| Native update rate | 4 Hz default (0.25–8 Hz) | one full cycle about 6.6 ms by default; configurable from about 0.84 ms (3 ch × 2 × 140 µs) to about 50 ms (3 ch × 2 × 8.244 ms), ×AVG settling | FSC/tach update 100 ms to 1.6 s |
| Hardware limits | T_HIGH/T_LOW with hysteresis | critical (instant) and warning (averaged) per channel on **shunt V only**; sum limit; PV window on bus V across all channels | Valid TACH Count (stall), drive-fail band |
| Alert output | ALERT pin (PD11 on LCD Board) | 4 pins (Critical, Warning, PV, TC); read-to-clear flags | ALERT# (per-fan enable); read-to-clear status |
| Presence / identity | ACK only (no ID register) | ACK + 0x5449 / 0x3220 | ACK + 0x36 / 0x5D |
| Addresses | 0x48–0x4B (4 per bus) | 0x40–0x43 (4 per bus) | 0x2E or 0x2F (by part variant) |

Recommendations for the model:

1. **One Sensor per physical quantity per channel, named from board config.**
   - Examples: "LCD temperature", "IO 5V rail voltage", "IO 5V rail current", "Fan 1 speed".
   - A single INA3221 provides up to 6 directly measured Sensors (3 × V_bus, 3 × I), plus optional derived power.
   - The board description holds the names, shunt values and which channels are fitted. Driver code does not.
2. **SI base units in `float`:** °C, V, A, W, RPM.
   - The mV/mA scaling belongs in the display layer.
   - Each Sensor should state its own resolution, so the UI can choose sensible decimal places. For example, current resolution depends on the shunt.
3. **Reading = value + validity, never a sentinel.**
   - Every surveyed library reports a failed read as 0 or a small negative value, which looks plausible. The driver layer must return a status, such as `ok`, `bus_error`, `absent` or `stale`.
   - The Sensor keeps the last good value plus a timestamp, so consumers can tell "0 A" apart from "no data".
4. **Absent-chip detection at probe time, using ID registers where they exist** (INA3221, EMC2302). TMP102 can only use the address ACK.
   - Probe at boot and after a bus error.
   - Re-run `configure()` from the shadow registers when a chip comes back. INA3221 and EMC2302 registers are volatile, and an INA3221 RST or brown-out silently restores defaults.
5. **Polling, owned by one task per bus, paced to the chip.**
   - TMP102 at 1–4 Hz. INA3221 at 10–100 Hz is plenty with AVG ≥ 16, given its running-average filter. EMC2302 RPM at the FSC update rate (default 400 ms).
   - Because STM32duino `Wire` busy-waits up to 100 ms on a fault [STM-WIRE], the bus task should run at modest priority and own the mutex for each transaction only, not each sweep.
6. **Limits are partly hardware and partly software. Do not force one shape onto them.**
   - TMP102 and the INA3221 critical/warning limits are real hardware comparators worth using for protection (PD11, INA Critical).
   - INA3221 limits exist only on current, never on bus voltage (apart from the PV window).
   - A generic "Sensor has min/max thresholds" feature therefore needs a software comparator anyway. Hardware limits can be an optional extra on top of the generic interface.
7. **Read-to-clear status needs a single owner.**
   - INA3221 Mask/Enable and the EMC2302 status registers clear when read.
   - Only the driver's `readFlags()` and `readStatus()` should touch them, and they should hand the flags out as events or latched faults rather than leave them to be read again.
8. **Fans are actuators that also report.** Model the EMC2302 as a `Fan` with a target (RPM or duty) and a status. Its speed reading can additionally be published as an ordinary Sensor (RPM) for logging and display.
