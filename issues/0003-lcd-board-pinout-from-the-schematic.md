---
id: 3
title: LCD Board pinout from the schematic
labels: [wayfinder:research]
status: closed
assignee: claude-research-agent
parent: 1
blocked_by: []
---

## Question

What is the complete pin allocation of the STM32H743VIT6 on the LCD Board (P0004-PCB-001, KiCad repo `M0WUT/p0004-pcb-001_lcd-board`)? In particular: is the SD card on SDMMC or SPI; which pins/peripherals serve the USB device port and the USB host port (OTG_FS vs OTG_HS-in-FS); which I2C bus(es) serve the on-board EEPROM, TMP102 and the IO Board; the pins and peripherals of the 6 user IO connectors (2x UART, I2C, SPI, 2x 6-pin GPIO) and of the IO Board connector. Cross-check against `include/pinout.h` in sw-001 and flag any disagreements.

## Resolution

Full findings: branch `research/lcd-board-pinout` (commit 7578fdd, local only), file `docs/research/lcd-board-pinout.md`.

- **SD card**: SDMMC1, 4-bit (D0–D3 on PC8–PC11, CLK PC12, CMD PD2, card detect PB8). The data lines have no pull-ups, so use the internal ones.
- **USB host** (USB-A socket): OTG_FS on PA11/PA12, VBUS enable PD14, VBUS fault PD15.
- **USB device**: OTG_HS in FS mode on PB14/PB15. It goes only to the IO Board connector, with no VBUS sense pin. Build flags: `-DUSE_USB_HS -DUSE_USB_HS_IN_FS`.
- **On-board I2C**: I2C4 on PD12 (SCL) / PD13 (SDA) carries the EEPROM (0x50), the TMP102 (0x48) and the IO Board. The TMP102 alert is PD11, shared with the IO Board. The LCD Board has no I2C pull-ups.
- **User connectors**: CN6 UART4 (PA0/PA1), CN7 I2C1 (PB6/PB7, alert PB5), CN8 I2C2 (PB10/PB11, alert PB12), CN9 SPI6 (PA4–PA7), CN10 GPIO (PC13, PA2, PB1, PC3_C, PB2), CN11 GPIO (PA3, PC5, PC0, PC15, PC14). That is **1 UART and 2 I2C**, not 2 UARTs and 1 I2C. No user connector has a power pin.
- **Other**:
  - Buzzer PB0 (driven through a transistor, PWM-capable). Buttons PE10, PE12, PE9, PE13 (`INPUT_PULLUP`). RGB LED active-low on PA10/PA8/PA9.
  - TFT on SPI4. W5500 on SPI2, interrupt on PC7. Debug UART USART2 on PD5/PD6.
  - The MCU has no HSE or LSE crystal, so it runs on HSI, and USB needs HSI48 with CRS.
- **Where `pinout.h` is wrong**:
  - The backlight is **PE3**, not PE5. PE3 has no timer channel, so hardware PWM isn't possible.
  - The TFT dummy MISO is set to **PE13**, which is button SW4. It should be PE5, which is unused.
  - The down button is set to **PE14**, which is unconnected. It should be PE13.
  - Which button is left, right, up or down is a guess from the layout.
- **Not verified**:
  - The ROM DFU bootloader may only work on PA11/PA12 (the host socket); check ST's AN2606. This matters for the out-of-scope firmware-update work.
  - The SMBus alert alternate-function numbers are from memory.

### Correction from the user

`pinout.h` (in sw-001) is the source of truth for pins. Where it disagrees with the schematic, `pinout.h` wins: the backlight is PE5, the TFT dummy MISO is PE13, the down button is PE14, and button directions come from its names (left PE10, right PE12, up PE9, down PE14). The schematic findings above apply only to pins `pinout.h` doesn't define: SD, both USB ports, the user connectors, the W5500 interrupt, the TMP102 address and alert, and the IO Board connector.

### Correction from measurement

The board **does have an HSE clock**: it was read from the RCC registers on the running board (see [Toolchain proven: sw-001 flashed to the LCD Board](0002-toolchain-proven-sw-001-flashed-to-the-lcd-board.md)). The "HSI only" finding above is wrong.

### Fixes applied to the bench board (from the GitHub issues on `M0WUT/p0004-pcb-001_lcd-board`)

All of these are fitted to the board on the bench, according to the user, **except #5**.

- **#1 "MCU requires crystal"**: an HSE crystal was added, the same part as the PHY's (25 MHz). This matches the RCC readings.
- **#3 "i2c missing pullups"**: I2C pull-ups were added on **every** I2C bus: internal I2C4 and the user ports CN7 and CN8 (confirmed by the user).
- **#4 "beef up speaker transistor"**: the Buzzer driver is now a BS170. No firmware impact.
- **#5 "3v3 expansion load switch EN should go back to MCU"**: **not applied** on this board. The firmware can't control the 3V3 expansion enable, so it can't clear a fault. The fault output is PE8 (from the schematic) and can only be read.
- **#6 "LCD Backlight pin is not on a timer output"**: the backlight moves to **PE5** (a timer output), the TFT dummy MISO to **PE13**, and SW4 moves off PE13 (to PE14, per `pinout.h`).
- **#7 "Add 1k pulldown on LCD BL pin"**: keeps the backlight off at power-on. No firmware impact.
- **#8 "add pulldown to miso on W5500"**: no firmware impact.
- **#2 "internal i2c not connected to header"**: closed. It wasn't a fault (the author was reading an old schematic).

The user confirmed the user connector mix is **1 UART (CN6) and 2 I2C (CN7, CN8)**, as the schematic shows. No issue touches SD, USB, the debug UART or the IO Board connector, so the schematic findings above still apply to those.
