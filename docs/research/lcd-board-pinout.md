# LCD Board (P0004-PCB-001) STM32H743VIT6 pinout

Research note, 2026-11-24. Question: what is the complete pin allocation of U4
(STM32H743VIT6, LQFP-100) on the LCD Board, which peripheral instance serves
each function, and where does sw-001's `include/pinout.h` disagree?

## Sources

| Tag | Source | Revision |
|---|---|---|
| **PCB** | `p0004-pcb-001_lcd-board/P0004-PCB-001.kicad_pcb`, per-pad `net` and `pinfunction` fields | `1a4811d` "draft v1 release" |
| **SCH** | `p0004-pcb-001_lcd-board/*.kicad_sch`: net labels, hierarchical labels, text annotations | `1a4811d` |
| **PH** | `p0004-sw-001_generic-arduino-firmware/include/pinout.h` | `a91a8ad` |
| **PP** | STM32duino `variants/STM32H7xx/H742V(G-I)(H-T)_H743V(G-I)(H-T)_H750VBT_H753VI(H-T)/PeripheralPins.c` (generated from ST's STM32CubeMX pin database), plus `libraries/SrcWrapper/src/stm32/pinmap.c` and `libraries/USBDevice/src/usbd_conf.c` | Arduino_Core_STM32 `main` snapshot in the scratchpad |
| **DS** | ST DS12110 (STM32H742xI/G, STM32H743xI/G datasheet), alternate-function table | Not fetched in this session. Items tagged DS come from recall and are marked "verify". |

Method: I parsed every footprint pad in the PCB file to get (ref, pad,
pinfunction, net). Then I traced each connector pin through its series
resistor (one hop through any 2-terminal R/F/D/L) back to a U4 pad. The PCB
was updated from the schematic: every schematic label is either a PCB net
name or a hierarchical-sheet-pin alias of one (for example, `USB_DEVICE_P/N`
in microcontroller.kicad_sch becomes the top-level `/USB_P`/`/USB_N`, and
`~{ETH_PHY_RESET}` becomes `~{ETH_PHY_RST}`). No kicad-cli was available, so
I did not run ERC or a netlist export. The scripts are in the session
scratchpad (`work/pads.py`, `work/trace.py`, `work/af.py`).

## Summary answers

- **SD card is on SDMMC1, 4-bit** (PC8-PC11 D0-D3, PC12 CLK, PD2 CMD). Card
  detect is on PB8. SPI is not used. [PCB; SCH annotation "SDMMC1"; PP SD_* AF12_SDIO1]
- **USB host (USB-A, CN2) = USB_OTG_FS on PA11/PA12** (HAL `USB_OTG_FS` =
  USB2_OTG_FS, AF10). It goes through a USBLC6-2P6 (D3) ESD array. VBUS comes
  from a TPS2553 load switch (U7): EN on PD14, ~FAULT on PD15.
  [PCB; SCH "USB_OTG_FS"; PP PinMap_USB_OTG_FS]
- **USB device = USB_OTG_HS in FS mode on PB14 (D-)/PB15 (D+)**, using the
  internal FS PHY (AF12). Its only route is to the IO Board connector CN1
  (pins 23/21); the LCD Board has no USB device receptacle. The schematic
  note reads "USB only runs at FS with internal PHY". No VBUS-sense pin is
  wired. With STM32duino this needs `-DUSE_USB_HS -DUSE_USB_HS_IN_FS`.
  [PCB; SCH "USB_OTG_HS"; PP PinMap_USB_OTG_HS; usbd_conf.c]
- **EEPROM (U3 24AA256UID, 0x50), TMP102 (U2, 0x48) and the IO Board connector
  all share one bus: I2C4 on PD12 (SCL)/PD13 (SDA)**. The schematic net is
  `INT_SCL`/`INT_SDA`, annotated "I2C4". TMP102 ~ALERT is on PD11 (net
  `~{INT_ALERT}`, also on CN1.27), the I2C4_SMBA pin. **The LCD Board has no
  pull-up resistors on this bus or on the user I2C buses.** They must come
  from the IO Board or off-board, or from MCU weak pull-ups (too weak for
  real use). [PCB nets; SCH "I2C4", "Address = 0x50", "Address = 0x48"]
- **User IO connectors: 1x UART, 2x I2C, 1x SPI, 2x 6-pin GPIO.** The ticket
  says "2x UART, I2C"; the board actually has one 3-pin UART (CN6, UART4) and
  two 4-pin I2C (CN7 I2C1, CN8 I2C2), each with an SMBus alert line. Pin 1 of
  every user connector is 0V. **None of them carries a supply pin.**
- No MCU HSE crystal is fitted: PH0/PH1 are unconnected, and the 25 MHz XT1
  belongs to the W5500. No LSE either: PC14/PC15 are routed out as GPIO. The
  clock tree must run from HSI/CSI, and USB needs HSI48 (with CRS synced to
  USB SOF for accuracy). [PCB: XT1 to U1 XI/XO, U4.12/13 unconnected]

## Full U4 pin allocation

Every pad that is not power, ground or VCAP. "AF" lists the function this
board uses, then other useful alternates from PP.

| Pad | Pin | Net (PCB) | Function on board | Peripheral / AF used | Other notable AFs (PP) |
|---|---|---|---|---|---|
| 1 | PE2 | LCD_SCLK | TFT SCL via R18 33R | SPI4_SCK AF5 | QUADSPI_BK1_IO2 |
| 2 | PE3 | LCD_BL | TFT BLK via R23 33R | GPIO (**no timer AF**) | none in PP (only SAI/TRACE in DS) |
| 3 | PE4 | ~{LCD_CS} | TFT ~CS via R22 | SPI4_NSS AF5 (used as GPIO CS) | TIM15_CH1N |
| 4 | PE5 | LCD_MISO | **MCU pad only, not connected to display** | SPI4_MISO AF5 | TIM15_CH1 |
| 5 | PE6 | LCD_MOSI | TFT SDA via R19 | SPI4_MOSI AF5 | TIM15_CH2 |
| 7 | PC13 | PC13 | CN10.2 GPIO via R38 | GPIO (RTC domain, low drive) | none |
| 8 | PC14 | PC14 | CN11.6 GPIO via R47 | GPIO (OSC32_IN, low drive) | none |
| 9 | PC15 | PC15 | CN11.5 GPIO via R46 | GPIO (OSC32_OUT, low drive) | none |
| 12/13 | PH0/PH1 | unconnected | no HSE | | |
| 14 | NRST | ~{SWD_NRST} | CN4.12, 10k pull-up | | |
| 15 | PC0 | PC0 | CN11.4 GPIO via R45 | GPIO / ADC123_INP10 | OTG_HS_ULPI_STP |
| 16 | PC1 | /Ethernet/MOSI | W5500 MOSI | SPI2_MOSI AF5 | ADC123_INP11, SDMMC2_CK |
| 17 | PC2_C | /Ethernet/MISO | W5500 MISO | SPI2_MISO AF5 (dual pad; core closes the analog switch, pinmap.c) | ADC3_INP0 |
| 18 | PC3_C | PC3 | CN10.5 GPIO via R41 | GPIO / ADC3_INP1 (dual pad) | SPI2_MOSI |
| 22 | PA0 | UART4_TX | CN6.2 via R26 | UART4_TX AF8 | TIM2_CH1, TIM5_CH1, ADC1_INP16, USART2_CTS |
| 23 | PA1 | UART4_RX | CN6.3 via R27 | UART4_RX AF8 | TIM2_CH2, TIM5_CH2, TIM15_CH1N, ADC1_INP17 |
| 24 | PA2 | PA2 | CN10.3 GPIO via R39 | GPIO | USART2_TX AF7, TIM2_CH3, TIM5_CH3, TIM15_CH1, ADC12_INP14 |
| 25 | PA3 | PA3 | CN11.2 GPIO via R43 | GPIO | USART2_RX AF7, TIM2_CH4, TIM5_CH4, TIM15_CH2, ADC12_INP15 |
| 28 | PA4 | ~{SPI6_CS} | CN9.2 via R34 | SPI6_NSS AF8 | SPI1/SPI3_NSS, DAC1_OUT1, ADC12_INP18 |
| 29 | PA5 | SPI6_SCLK | CN9.3 via R35 | SPI6_SCK AF8 | SPI1_SCK, TIM2_CH1, DAC1_OUT2 |
| 30 | PA6 | SPI6_MISO | CN9.4 via R36 | SPI6_MISO AF8 | SPI1_MISO, TIM3_CH1, TIM13_CH1, ADC12_INP3 |
| 31 | PA7 | SPI6_MOSI | CN9.5 via R37 | SPI6_MOSI AF8 | SPI1_MOSI, TIM3_CH2, TIM14_CH1, ADC12_INP7 |
| 32 | PC4 | unconnected | | | |
| 33 | PC5 | PC5 | CN11.3 GPIO via R44 | GPIO / ADC12_INP8 | |
| 34 | PB0 | SPEAKER | buzzer: R48 1k to Q1 BC847 base, LS1 to 3V3 (flyback D15) | GPIO/PWM: TIM1_CH2N, TIM3_CH3, TIM8_CH2N | ADC12_INP9 |
| 35 | PB1 | PB1 | CN10.4 GPIO via R40 | GPIO | TIM1_CH3N, TIM3_CH4, TIM8_CH3N, ADC12_INP5 |
| 36 | PB2 | PB2 | CN10.6 GPIO via R42 | GPIO | SPI3_MOSI AF7, QUADSPI_CLK |
| 37 | PE7 | unconnected | | | |
| 38 | PE8 | ~{3V3_EXPANSION_FAULT} | U8 TPS2553 ~FAULT (3V3_EXPANSION to CN1.29), 10k pull-up | GPIO in | |
| 39 | PE9 | ~{SW3} | button SW3 to 0V | GPIO in, active low | TIM1_CH1 |
| 40 | PE10 | ~{SW1} | button SW1 to 0V | GPIO in, active low | TIM1_CH2N |
| 41 | PE11 | unconnected | | | |
| 42 | PE12 | ~{SW2} | button SW2 to 0V | GPIO in, active low | TIM1_CH3N |
| 43 | PE13 | ~{SW4} | button SW4 to 0V | GPIO in, active low | TIM1_CH3, SPI4_MISO |
| 44/45 | PE14/PE15 | unconnected | | | |
| 46 | PB10 | I2C2_SCL | CN8.2 via R31 | I2C2_SCL AF4 | USART3_TX, SPI2_SCK, TIM2_CH3 |
| 47 | PB11 | I2C2_SDA | CN8.3 via R32 | I2C2_SDA AF4 | USART3_RX, TIM2_CH4 |
| 51 | PB12 | ~{I2C2_ALERT} | CN8.4 via R33 | I2C2_SMBA AF4 (DS, verify) | SPI2_NSS, UART5_RX, FDCAN2_RX |
| 52 | PB13 | /Ethernet/SCLK | W5500 SCLK | SPI2_SCK AF5 | |
| 53 | PB14 | /USB_N | USB device D-, to CN1.23 | OTG_HS_DM (FS PHY) AF12 | |
| 54 | PB15 | /USB_P | USB device D+, to CN1.21 | OTG_HS_DP (FS PHY) AF12 | |
| 55-57 | PD8-PD10 | unconnected | | | |
| 58 | PD11 | ~{INT_ALERT} | TMP102 ~ALERT + CN1.27 | I2C4_SMBA AF4 (DS, verify) / GPIO in | |
| 59 | PD12 | INT_SCL | U2, U3, CN1.19 | I2C4_SCL AF4 | |
| 60 | PD13 | INT_SDA | U2, U3, CN1.17 | I2C4_SDA AF4 | |
| 61 | PD14 | USB_VBUS_EN | U7 EN (host VBUS), 10k pull-down | GPIO out, active high | |
| 62 | PD15 | ~{USB_VBUS_FAULT} | U7 ~FAULT, 10k pull-up | GPIO in | |
| 63 | PC6 | ~{ETH_PHY_RST} | W5500 ~RST, 10k pull-up | GPIO out | |
| 64 | PC7 | ~{ETH_PHY_INT} | W5500 ~INT, 10k pull-up | GPIO in | |
| 65 | PC8 | SDMMC_D0 | CN5 DAT0 | SDMMC1_D0 AF12 | |
| 66 | PC9 | SDMMC_D1 | CN5 DAT1 | SDMMC1_D1 AF12 | |
| 67 | PA8 | RGB_G | LD1 G via R16 1k | TIM1_CH1 (SCH note) | |
| 68 | PA9 | RGB_B | LD1 B via R17 1k | TIM1_CH2 (SCH note) | |
| 69 | PA10 | RGB_R | LD1 R via R15 1k | TIM1_CH3 (SCH note) | |
| 70 | PA11 | USB_HOST_D_N | D3 to CN2 D- | OTG_FS_DM AF10 | |
| 71 | PA12 | USB_HOST_D_P | D3 to CN2 D+ | OTG_FS_DP AF10 | |
| 72 | PA13 | SWD_SWDIO | CN4.4 | SWD | |
| 76 | PA14 | SWD_SWCLK | CN4.6 | SWD | |
| 77 | PA15 | unconnected | | | |
| 78 | PC10 | SDMMC_D2 | CN5 DAT2 | SDMMC1_D2 AF12 | |
| 79 | PC11 | SDMMC_D3 | CN5 DAT3/CD | SDMMC1_D3 AF12 | |
| 80 | PC12 | SDMMC_CLK | CN5 CLK | SDMMC1_CK AF12 | |
| 81/82 | PD0/PD1 | unconnected | | | |
| 83 | PD2 | SDMMC_CMD | CN5 CMD | SDMMC1_CMD AF12 | |
| 84/85 | PD3/PD4 | unconnected | | | |
| 86 | PD5 | DEBUG_UART_TX | CN3.2 (R10 100R) and CN4.14 T_VCP_TX (R13 100R) | USART2_TX AF7 | |
| 87 | PD6 | DEBUG_UART_RX | CN3.1 (R9 100R) and CN4.13 T_VCP_RX (R14 100R) | USART2_RX AF7 | |
| 88 | PD7 | unconnected | | | |
| 89 | PB3 | SWD_SWO | CN4.8 | TRACESWO | |
| 90 | PB4 | unconnected | | | |
| 91 | PB5 | ~{I2C1_ALERT} | CN7.4 via R30 | I2C1_SMBA AF4 (DS, verify; SCH "Using proper SMBus pin") | SPI1/3/6_MOSI, TIM3_CH2, FDCAN2_RX |
| 92 | PB6 | I2C1_SCL | CN7.2 via R28 | I2C1_SCL AF4 | USART1_TX, TIM4_CH1, I2C4_SCL AF6 |
| 93 | PB7 | I2C1_SDA | CN7.3 via R29 | I2C1_SDA AF4 | USART1_RX, TIM4_CH2, I2C4_SDA AF6 |
| 94 | BOOT0 | DFU_EN | 10k pull-down + 100n, driven from CN1.25 | | |
| 95 | PB8 | ~{SD_CARD_DET} | CN5 DET_A, 10k pull-up (DET_B = 0V) | GPIO in | |
| 96 | PB9 | /Ethernet/~{CS} | W5500 ~SCS | GPIO CS (SPI2_NSS AF5 capable) | |
| 97 | PE0 | LCD_RST | TFT RES via R20 | GPIO out | UART8_RX |
| 98 | PE1 | LCD_DC | TFT DC via R21 | GPIO out | UART8_TX |

## Connectors

### User IO (JST SH, pin 1 = 0V on all, each signal has 33R series resistor)

| Ref | Part | Role | Pin 2 | Pin 3 | Pin 4 | Pin 5 | Pin 6 |
|---|---|---|---|---|---|---|---|
| CN6 | BM03B | UART4 | PA0 TX | PA1 RX | | | |
| CN7 | BM04B | I2C1 | PB6 SCL | PB7 SDA | PB5 ~ALERT (SMBA) | | |
| CN8 | BM04B | I2C2 | PB10 SCL | PB11 SDA | PB12 ~ALERT (SMBA) | | |
| CN9 | BM05B | SPI6 | PA4 ~CS | PA5 SCK | PA6 MISO | PA7 MOSI | |
| CN10 | BM06B | GPIO | PC13 | PA2 | PB1 | PC3_C | PB2 |
| CN11 | BM06B | GPIO | PA3 | PC5 | PC0 | PC15 | PC14 |

Notes:
- PA2 (CN10.3) and PA3 (CN11.2) together form USART2 TX/RX (AF7). USART2 is
  already the debug UART on PD5/PD6, so only one of those pin pairs can carry
  USART2 at a time. The GPIO pins have no other second UART. PWM-capable
  pins: PA2, PA3 (TIM2/5/15) and PB1 (TIM3_CH4, TIM1/8_CH3N). ADC-capable
  pins: PA2, PA3, PB1, PC0, PC3_C, PC5.
- PC13/PC14/PC15 sit in the backup domain. In DS they have no AF and are
  limited to low drive and low speed (verify the exact limits in DS12110).
  Treat them as slow GPIO only.

### IO Board connector CN1 (Liansheng PH--00603, 30-pin; all even pins 0V)

| Pin | Net | MCU |
|---|---|---|
| 1 | VIN (to F2) | |
| 5/7 | ETH_RX_P/N (W5500 RXP/RXN) | |
| 9/11 | ETH_TX_P/N | |
| 13 | ~ETH_LINK_OD (W5500 LINKLED) | |
| 15 | ~ETH_ACT_OD (W5500 ACTLED) | |
| 17 | INT_SDA | PD13 (I2C4) |
| 19 | INT_SCL | PD12 (I2C4) |
| 21 | USB_P | PB15 (OTG_HS FS D+) |
| 23 | USB_N | PB14 (OTG_HS FS D-) |
| 25 | DFU_EN | BOOT0 |
| 27 | ~INT_ALERT | PD11 (shared with TMP102) |
| 29 | 3V3_EXPANSION (U8 load switch, fault on PE8) | |

### Others
- **SD (CN5, Molex 104031-0811 microSD):** SDMMC1 4-bit as above. VDD comes
  from 3V3 via F1 and is always on (no power-enable pin). The SD lines have
  no series or pull-up resistors on the board, so enable SDMMC internal
  pull-ups (or GPIO pull-ups) on CMD/D0-D3.
- **Debug UART:** USART2 PD5/PD6, available on both CN3 (Harwin M20 3-pin)
  and CN4 (Samtec 14-pin, STLINK-V3 pinout, VCP). Both sources feed PD6
  through 100R each, so connect only one adapter at a time.
- **SWD:** CN4 carries SWDIO PA13, SWCLK PA14, SWO PB3 and NRST.
- **TFT (ZF1, East Rising ER-TFTM019-1-SPI):** SPI4. SCL=PE2, SDA=PE6,
  RES=PE0, DC=PE1, ~CS=PE4, BLK=PE3. The display has no MISO pin. PE5 is
  labelled LCD_MISO but goes only to the MCU pad.
- **W5500 (U1):** SPI2. SCLK=PB13, MISO=PC2_C, MOSI=PC1, ~SCS=PB9, ~RST=PC6,
  ~INT=PC7. PMODE0-2 are tied high (all-capable auto-negotiation).
- **RGB LED (LD1):** common anode to 3V3, so **active low**. R=PA10, G=PA8,
  B=PA9 (TIM1_CH3/CH1/CH2 per SCH annotation and PP).
- **Buzzer (LS1):** PB0 through 1k into an NPN low-side switch, so active
  high. PB0 is PWM-capable (TIM3_CH3, TIM1_CH2N, TIM8_CH2N).
- **Buttons SW1-SW4:** each goes to 0V with only a TVS diode on the net and
  **no external pull-up**, so use `INPUT_PULLUP`. Physical order: SW1-SW4
  and ZF1 are on B.Cu at y=119.75, x = 195.5 / 180.5 / 165.5 / 150.5 mm.
  Mirrored for viewing from the display side, that puts them in a single row
  in the order **SW1, SW2, SW3, SW4** from left to right (inferred; confirm on
  hardware). The schematic does not name them left/right/up/down.

## Disagreements with sw-001 `pinout.h`

| # | pinout.h | Board | Impact |
|---|---|---|---|
| 1 | `gpio_tft_bl = PE5` | Backlight is **PE3** (LCD_BL). PE5 is the unconnected LCD_MISO pad | Backlight never turns on via PE5. Also **PE3 has no timer channel**, so the hardware PWM implied by `TFT_PWM_FREQUENCY = 25000` cannot run on the real pin; use on/off or software PWM |
| 2 | `gpio_tft_miso = PE13 // Dummy pin` | PE13 is **button SW4**. The board's (unused) SPI4 MISO is PE5 | PE13 can mux as SPI4_MISO AF5 (PP), so SPI init would steal the SW4 button. Use PE5 as the dummy MISO |
| 3 | `gpio_button_down = PE14` | PE14 is **unconnected**. The fourth button is SW4 on **PE13** | Down button dead |
| 4 | Buttons named left/right/up/down | Schematic only has SW1-SW4 in one row | The semantic mapping is a firmware choice, not something the hardware defines |
| 5 | No TMP102 address | TMP102 at **0x48** (ADD0 = GND) | Missing constant |
| 6 | W5500 has no INT pin | ~INT on **PC7** | Missing |
| 7 | Missing entirely | SDMMC1 (PC8-PC12, PD2), SD detect PB8; USB host PA11/PA12 + VBUS_EN PD14 + ~FAULT PD15; USB device PB14/PB15 (OTG_HS-in-FS); 3V3 expansion ~FAULT PE8; IO-board ~INT_ALERT PD11; all six user connectors (UART4, I2C1+SMBA, I2C2+SMBA, SPI6, 10 GPIO) | Missing |

Matches: debug UART PD5/PD6; TFT DC PE1, RST PE0, CS PE4, MOSI PE6, SCLK PE2
on SPI4; W5500 CS PB9, MISO PC2_C, MOSI PC1, SCLK PB13, RSTn PC6 (SPI2); RGB
R PA10, G PA8, B PA9; internal I2C PD12/PD13 with EEPROM 0x50; buzzer PB0;
buttons PE10, PE12, PE9. pinout.h does not say that the RGB LED is active low
or that the buttons need internal pull-ups.

## Open questions / not determined

- **ST system bootloader DFU port (verify against AN2606).** As I recall,
  the STM32H74x ROM bootloader offers USB DFU only on OTG_FS PA11/PA12. On
  this board those pins are the *host* USB-A port. DFU_EN (BOOT0) is driven
  from the IO Board, while the device port is on PB14/PB15 (OTG_HS). If my
  recall is right, ROM DFU through the IO Board's USB port will not
  enumerate. I did not verify this in this session.
- **SMBA AF numbers** for PB5/PB12/PD11 come from recall of DS12110
  (the STM32duino PeripheralPins.c does not list SMBA). Verify them in
  DS12110 Table "Port x alternate functions".
- Whether the IO Board fits I2C4 pull-ups and VBUS sensing for the device
  port. That is outside this board; see P0004-PCB-002.
- Button direction semantics, and confirmation of the physical left-to-right
  order (inferred from mirrored B.Cu coordinates).
- No ERC/DRC run (kicad-cli not installed). Schematic-to-PCB sync was checked
  by matching label names only.
