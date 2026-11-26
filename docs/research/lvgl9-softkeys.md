# LVGL 9 with four Soft Keys on a 320x170 TFT

Research for wayfinder ticket [#23](https://github.com/M0WUT/p0004-sw-002_claude-firmware/issues/23).
Target: STM32H743VIT6 @ 480 MHz, STM32duino on PlatformIO, FreeRTOS, **LVGL 9.6.0 (pinned)**,
ST7789 320x170 over SPI4 with DMA, partial buffers. Input is **only** the four Soft Keys in a row
under the LCD (see `GLOSSARY.md`: *Soft Key*, *Soft Key Legend*).

Sources are the LVGL `v9.6.0` tag itself (source, `lv_conf_template.h`, and the docs sources under
`docs/src/`, which are what docs.lvgl.io renders; docs.lvgl.io now redirects to
`lvgl.io/docs/open/9.x/...`). Claims marked *(inference)* are my own reasoning, not taken from a source.

---

## 1. Recommendation in one screen

1. **One `LV_INDEV_TYPE_KEYPAD` indev with no group.** The read callback reports one of four
   custom key codes (for example `GTE_KEY_F1..F4`, outside the `LV_KEY_*` range). Since there is no
   group, LVGL sends `PRESSED / PRESSING / LONG_PRESSED / LONG_PRESSED_REPEAT / RELEASED /
   SHORT_CLICKED / CLICKED` **to the indev's own event callbacks** for every key, so one
   `lv_indev_add_event_cb()` handler in a `SoftKeys` module receives everything
   ([lv_indev.c#L821-L980](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L821-L980),
   [`send_event` #L1975](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L1975-L2003)).
2. **`SoftKeys` owns hold timing.** It records `lv_tick_get()` on `PRESSED`, checks the elapsed
   time on every `PRESSING`, and raises "held 3 s" and "held 5 s" once each, plus auto-repeat
   where a screen asks for it. LVGL's own long press supports only one threshold per indev and
   only one repeat period. After a hold action fires, call `lv_indev_wait_release()` so that the
   release doesn't also count as a click.
3. **Per-screen key maps.** Each screen declares a table of 4 entries `{label, on_click,
   on_hold_3s, on_hold_5s, repeat}` plus a legend policy (`ALWAYS` for screens that need input,
   `AUTO_HIDE` for information screens). Screens never see raw keys.
4. **The Soft Key Legend is a single widget on `lv_layer_top()`**: a 320 x ~22 px flex row of four
   80 px cells along the bottom edge, created once, then relabelled whenever a screen loads.
   The top layer persists across `lv_screen_load()` ([screen_layers.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/screen_layers.mdx)).
   On `AUTO_HIDE` screens, an `lv_timer` checks `lv_display_get_inactive_time()` and after 10 s
   runs `lv_obj_fade_out()`. Any key press fades it back in.
5. **Exactly one LVGL task.** It runs `lv_timer_handler()` and nothing else touches `lv_*`.
   Other tasks send messages through a FreeRTOS queue. The UI task blocks on that queue, using
   the value returned by `lv_timer_handler()` as the timeout, and applies each message
   (preferably by setting `lv_subject`s that the labels are bound to). Don't use `lv_async_call()`
   from other tasks, because LVGL documents that it still needs the LVGL mutex.
6. **Display**: the built-in `lv_st7789_create()` driver (generic MIPI). Two partial buffers of
   1/10 screen (17 lines, 10,880 B each) in AXI SRAM, 32-byte aligned. Clean the D-cache before
   every DMA transfer. `LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565_SWAPPED` means no byte
   swap is needed. SPI around 60 MHz gives roughly a 15 ms wire time for a full frame.
7. **Fonts**: Montserrat 14 or 16 for the legend and body text, plus one large font
   for readouts. Generate the large one as a digits-only subset with `lv_font_conv`, because the
   full built-in Montserrat 48 costs 97 KB.

---

## 2. Input device type for four discrete buttons

### What 9.6.0 offers

| Type | How it works | Relevant behaviour (from v9.6.0 source) |
|---|---|---|
| `LV_INDEV_TYPE_KEYPAD` + **group** | `read_cb` reports a `key` and a state. Events go to the group's focused widget. | `LV_KEY_NEXT/PREV` move the focus. `LONG_PRESSED` and `LONG_PRESSED_REPEAT` are sent **only for `LV_KEY_ENTER`**. Holding any other key just resends it to the focused widget at the repeat rate, which gives auto-repeat ([#L913-L957](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L913-L957)). |
| `LV_INDEV_TYPE_KEYPAD`, **no group** | Same `read_cb`. | When `g == NULL`, every press, pressing, long press, repeat, release and click is sent to the indev for **every key** (`if(g == NULL \|\| data->key == LV_KEY_ENTER)`), and `lv_indev_get_key()` says which key it was. `LV_EVENT_KEY` fires on each key or state change ([#L851-L855](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L851-L855)). |
| `LV_INDEV_TYPE_BUTTON` | `read_cb` reports `btn_id`. `lv_indev_set_button_points()` maps each id to a screen point, and LVGL simulates a touch there ([button.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/button.mdx)). | Reuses the pointer path, including `LONG_PRESSED(_REPEAT)` on the widget under the point ([#L1217-L1260](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L1217-L1260), [#L1460-L1485](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L1460-L1485)). |
| `LV_INDEV_TYPE_ENCODER` | Rotation plus push. | Not applicable. |

Defaults, set per indev in `lv_indev_create()`: `LV_INDEV_DEF_LONG_PRESS_TIME 400` ms and
`LV_INDEV_DEF_LONG_PRESS_REP_TIME 100` ms ([lv_conf_template.h#L685-L688](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h#L685-L688)).
Both can be changed at runtime with `lv_indev_set_long_press_time()` and
`lv_indev_set_long_press_repeat_time()`. The read period is `LV_DEF_REFR_PERIOD`, 33 ms by default
([indev/overview.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/overview.mdx)).
Any pressed state updates the display's activity time, so `lv_display_get_inactive_time()`
measures time since the last Soft Key press for free
([#L238-L245](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L238-L245),
[inactivity.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/inactivity.mdx)).

### Assessment

- **KEYPAD + group** suits Tab-style navigation, where a cursor moves between widgets. A soft-key
  UI doesn't work that way: each key has a fixed position and a meaning that depends on the
  screen. A group also kills long press for every key except ENTER, so a 3 s hold on F2 is
  impossible. Groups are still worth using **inside** a screen that has a list or a spinbox: the
  `SoftKeys` handler can translate, for example, F1 to `lv_group_focus_prev()` and F2 to
  `lv_group_send_data(g, LV_KEY_UP)` itself.
- **BUTTON + points** sounds like a natural fit ("press the legend cell above the key"), and it
  gives pressed-state styling for free. But it ties key behaviour to geometry. When the legend is
  faded or hidden, a press lands on whatever widget is under the point (hidden objects aren't
  hit-tested, and `lv_layer_top` itself isn't clickable). It also brings in pointer scroll and
  gesture logic that does nothing useful here. *(inference)*
- **KEYPAD without a group** gives one event stream with key identity, LVGL-maintained timestamps
  and inactivity tracking, and it works the same whether or not the legend is visible.
  **Recommended.**
- A fully custom approach (no indev, key events straight from a queue) is possible, but you lose
  inactivity tracking and the `PRESSING` heartbeat for free. There's no reason to go that way.

### Limits to plan around

- **One key at a time per indev.** The keypad keeps a single `last_key` and `last_state`. If the
  product ever needs a chord (two keys held together, for example for Factory Reset), create
  **four keypad indevs, one per key**. Each has its own press, long-press and repeat state, and
  they can share one event callback that tells them apart with `lv_indev_get_user_data()`.
  *(inference from the source structure)*
- **Short presses can be lost in polling mode.** The key task or ISR should debounce and queue
  edge events. `read_cb` then pops one event per call and sets `data->continue_reading = true`
  while more are queued (the "Buffered Reading" section of
  [indev/overview.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/overview.mdx)).
  Use 20-30 ms debounce in the key task, not in LVGL.
- Custom key codes: LVGL only interprets the special `LV_KEY_*` values (17-127,
  [keypad.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/keypad.mdx)).
  Without a group they aren't acted on anyway, but using values like `0x1000+n` avoids confusion
  if a group is ever attached. A `key_remap_cb` (`lv_indev_set_key_remap_cb`) exists too, but
  isn't needed.

### 3 s and 5 s holds

LVGL's model has one `long_press_time` and then a repeat every `long_press_repeat_time`, measured
from the long press ([#L920-L937](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c#L920-L937)).
You could set 3000 / 2000 ms so that `LONG_PRESSED` fires at 3 s and the first
`LONG_PRESSED_REPEAT` at 5 s, but that hack prevents ordinary hold-to-repeat on the same indev.
Do the timing yourself instead, in the `PRESSING` handler. It runs every read period (~33 ms),
which is more than enough resolution:

```c
/* softkeys.c: runs only in the LVGL task */
typedef struct {
    const char *label;                 /* NULL = key unused on this screen */
    void (*on_click)(void);
    void (*on_hold_3s)(void);          /* NULL = not offered */
    void (*on_hold_5s)(void);
    uint16_t repeat_ms;                /* 0 = no auto-repeat; else repeat on_click while held */
} softkey_t;

typedef struct { softkey_t key[4]; enum { LEGEND_ALWAYS, LEGEND_AUTO_HIDE } legend; } softkey_map_t;

static uint32_t t_press, t_last_rep; static uint8_t fired; /* bit0=3s, bit1=5s */

static void indev_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_target(e);
    uint32_t k = lv_indev_get_key(indev) - GTE_KEY_F1;         /* 0..3 */
    const softkey_t *sk = &cur_map->key[k];
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        t_press = t_last_rep = lv_tick_get(); fired = 0;
        legend_wake();                                         /* fade in, restart 10 s */
        break;
    case LV_EVENT_PRESSING: {
        uint32_t held = lv_tick_elaps(t_press);
        if (sk->on_hold_5s && held >= 5000 && !(fired & 2)) { fired |= 2; sk->on_hold_5s(); lv_indev_wait_release(indev); }
        else if (sk->on_hold_3s && held >= 3000 && !(fired & 1)) { fired |= 1; sk->on_hold_3s();
                 if (!sk->on_hold_5s) lv_indev_wait_release(indev); }
        else if (sk->repeat_ms && held >= 500 && lv_tick_elaps(t_last_rep) >= sk->repeat_ms) {
                 t_last_rep = lv_tick_get(); sk->on_click(); }
        break; }
    case LV_EVENT_RELEASED:
        if (!fired && sk->on_click && !(sk->repeat_ms && lv_tick_elaps(t_press) >= 500)) sk->on_click();
        break;
    default: break;
    }
}
/* lv_indev_add_event_cb(kp, indev_cb, LV_EVENT_ALL, NULL); */
```

Product decisions this exposes, to settle in the UI spec rather than here:

- If a key offers both 3 s and 5 s holds, should 3 s fire at 3 s (so 5 s means "3 s action, then
  5 s action"), or only on release between 3 and 5 s? The sketch fires at 3 s.
- Should the legend show hold progress (for example a cell fill bar during a 5 s Factory Reset
  hold)? That's cheap to add: it's a bar widget updated from `PRESSING`.
- Does the first press on a faded legend only wake it, or also act? Pagers and phones usually
  just wake. The sketch acts.

---

## 3. Soft-key UI pattern and how to build it in LVGL

**Pattern.** Screen-labelled soft keys (ATMs, fuel pumps, feature phones, and the bezel keys of
bench test equipment) put a changing label next to each fixed physical button. The label always
describes what that key does *now* ([Wikipedia: Soft key](https://en.wikipedia.org/wiki/Soft_key)).
The conventions that follow from this *(industry practice, inference)*:

- Fixed geometry: cell *n* is always directly above key *n*. On this panel that's 4 x 80 px.
  Check the mechanical drawing for the key centres and align the cells to them, not to the
  pixel grid.
- An unused key shows an **empty** cell, not a hidden one, so the other labels don't shift.
- Keep roles consistent across screens (for example, F4 is always Back/Exit and F1 is always the
  primary action). This matters more than the label wording.
- Short labels: about 6-9 characters at 14-16 px across 80 px. Use icons (`LV_SYMBOL_*`, already
  included in the built-in Montserrat fonts) for up/down/back.
- Visual feedback on press: invert the cell while its key is held.

**LVGL construction.**

```c
static lv_obj_t *legend, *cell[4], *cell_lbl[4];
#define LEGEND_H 22

void legend_create(void)                          /* once, after display init */
{
    legend = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(legend);
    lv_obj_set_size(legend, 320, LEGEND_H);
    lv_obj_align(legend, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_add_style(legend, &st_legend_bg, 0);           /* opaque bg, top border */
    for (int i = 0; i < 4; i++) {
        cell[i] = lv_obj_create(legend);
        lv_obj_remove_style_all(cell[i]);
        lv_obj_set_size(cell[i], 80, LEGEND_H);
        lv_obj_add_style(cell[i], &st_cell, 0);
        lv_obj_add_style(cell[i], &st_cell_pressed, LV_STATE_PRESSED); /* set by SoftKeys */
        cell_lbl[i] = lv_label_create(cell[i]);
        lv_obj_center(cell_lbl[i]);
        lv_label_set_long_mode(cell_lbl[i], LV_LABEL_LONG_MODE_DOTS);
    }
}

void softkeys_set_map(const softkey_map_t *m)     /* call from each screen's LV_EVENT_SCREEN_LOADED */
{
    cur_map = m;
    for (int i = 0; i < 4; i++) lv_label_set_text(cell_lbl[i], m->key[i].label ? m->key[i].label : "");
    legend_wake();
}

static void legend_tick(lv_timer_t *t)            /* lv_timer_create(legend_tick, 250, NULL) */
{
    if (cur_map->legend == LEGEND_AUTO_HIDE && legend_visible &&
        lv_display_get_inactive_time(NULL) >= 10000) {
        lv_obj_fade_out(legend, 300, 0); legend_visible = false;
    }
}
```

Notes:

- The top layer is above *every* screen, and it persists across `lv_screen_load()`
  ([screen_layers.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/screen_layers.mdx)).
  The legend covers the bottom `LEGEND_H` px of the active screen. Recommended layout rule
  *(inference)*: input screens lay out their content in 320 x (170 - `LEGEND_H`). Information
  screens may use the full height, provided nothing essential sits in the strip the legend
  covers when it wakes.
- `lv_obj_fade_in/out()` ([lv_obj_style.h#L279-L287](https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/core/lv_obj_style.h#L279-L287))
  animate `opa`. While `opa < 255`, LVGL renders the widget through a "simple layer" in chunks of
  `LV_DRAW_LAYER_SIMPLE_BUF_SIZE` (24 KB by default,
  [lv_conf_template.h#L176-L180](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h#L176-L180)).
  That's cheap for a 320x22 strip, but if RAM is tight, a hard cut with `LV_OBJ_FLAG_HIDDEN` is
  the zero-cost alternative.
- Use `lv_display_trigger_activity()` if something other than a key press should restart the
  10 s window.
- Optionally wire the legend with the Observer module (`LV_USE_OBSERVER`, on by default). Note
  that 9.6 changed subjects to `lv_subject_create()` and `lv_subject_delete()`
  ([migration-v9-6.mdx, Observer](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/changelog/migration-v9-6.mdx)).

---

## 4. Fonts, sizes and theme

Built-in Montserrat fonts are 4 bpp, covering ASCII 0x20-0x7F, `°`, `•` and about 60 FontAwesome
symbols (header of [lv_font_montserrat_14.c](https://github.com/lvgl/lvgl/blob/v9.6.0/src/font/lv_font_montserrat_14.c)).
Only `LV_FONT_MONTSERRAT_14` is enabled by default
([lv_conf_template.h#L1000-L1067](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h#L1000-L1067)).

**Flash cost, measured** here by compiling each v9.6.0 font file with the PlatformIO
`arm-none-eabi-gcc` (`-mcpu=cortex-m7 -mthumb -Os`) and reading `.text+.rodata` with `size`:

| Font | Flash (B) | Line height (px) |
|---|---:|---:|
| Montserrat 10 | 9,369 | |
| Montserrat 12 | 11,491 | 15 |
| **Montserrat 14** | **13,652** | 16 |
| **Montserrat 16** | **15,910** | 18 |
| Montserrat 18 | 19,033 | |
| Montserrat 20 | 21,806 | 22 |
| Montserrat 22 | 25,459 | |
| Montserrat 24 | 28,704 | 27 |
| Montserrat 28 | 37,304 | |
| Montserrat 32 | 45,916 | |
| Montserrat 48 | 96,732 | |
| UNSCII 8 (1 bpp, bitmap) | 1,350 | |
| UNSCII 16 (1 bpp, bitmap) | 2,591 | |

(Line heights are taken from the `.line_height` fields of the font files.)

Guidance *(inference; check against the actual panel, likely a 1.9" 320x170 at about 190 ppi)*:

- 170 px tall gives about 7 lines of 16 px text at most, and about 6 above a 22 px legend.
- **Legend**: Montserrat 14 (16 px line, 22 px cell), or 16 if the panel is the small 1.9" one
  and labels look cramped. The 4 px of anti-aliased edge per glyph at 4 bpp reads well on IPS.
- **Body and menus**: the same font, so there's one font in RAM caches and one style.
- **Readouts** (frequency, voltage): one big face at 32-48 px, generated with
  [lv_font_conv](https://github.com/lvgl/lv_font_conv) for only `0-9 . - + : space` and the unit
  letters you need. That costs a few KB instead of 46-97 KB. Flash is 2 MB, so the full fonts
  are affordable, just wasteful.
- Set `LV_FONT_DEFAULT` to the chosen body font. 9.6 picks it with `LV_FONT_DEFAULT_MONTSERRAT_xx`,
  or with `LV_USE_CUSTOM_FONT_DEFAULT` for your own
  ([lv_conf_template.h#L1096-L1135](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h#L1096-L1135)).

**Theme.** 9.6 ships `default` (full, designed for touch, with grow-on-press and transitions),
`simple` ("minimal styling, a good starting point for a custom theme") and `mono` (1-bit)
([lv_conf_template.h#L845-L870](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h#L845-L870)).
Recommendation: use `lv_theme_simple_init(disp)`, or no theme, plus a handful of project styles:
legend background, legend cell and pressed cell, body text, readout text, and a focus style for
lists. The default theme's DPI-scaled paddings and press animations are sized for fingers,
which wastes pixels at 320x170. Set `LV_THEME_DEFAULT_GROW 0` if you keep it. Lower
`LV_DPI_DEF` (130 by default, used to derive default paddings) if widgets look padded.
Use a dark background with high-contrast text: it suits a bench instrument, and black pixels
cost nothing on an IPS backlight *(inference)*.

---

## 5. Performance and memory on the H743 with SPI DMA

**Wire time is the bottleneck, not rendering.** A full frame is 320 x 170 x 2 = 108,800 B,
which is 870,400 bits.

- The ST7789's serial write clock cycle (TSCYCW) is **16 ns minimum, about 62.5 MHz**
  (Sitronix ST7789V3 datasheet, serial interface AC timing:
  [Waveshare mirror](https://files.waveshare.com/wiki/common/ST7789V3_V0.1.pdf)).
- SPI4's kernel clock defaults to APB2 (120 MHz at 480 MHz SYSCLK with the usual dividers), and
  the prescaler divides by powers of two, so 60 MHz is the natural setting. Otherwise clock SPI4
  from PLL2/PLL3 to get closer to the panel limit *(inference from RM0433 clock tree; confirm in
  CubeMX)*.
- At 60 MHz, a full frame takes **about 14.5 ms (about 69 fps)**. At 30 MHz it takes about 29 ms.
  Typical updates (a readout, a legend cell) are a few percent of the screen and take well under
  a millisecond. The default 33 ms refresh period (`LV_DEF_REFR_PERIOD`) is fine.

**Buffers.** LVGL recommends partial buffers of at least 1/10 of the screen, and two buffers so
that rendering overlaps the DMA
([display/setup.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/setup.mdx)).
For 320x170 RGB565, 1/10 is 17 lines, or 10,880 B, so **2 x 10,880 B is about 21 KB**. RAM is
plentiful (1 MB), so 2 x 1/4 screen (2 x 27 KB) is also reasonable and reduces flush overhead.
Keep each buffer at 65,535 B or less, because both an H7 DMA stream's NDTR and the SPI TSIZE
counter are 16-bit *(RM0433)*. A full-frame single buffer (108,800 B) would have to be split
into two transfers.

**H7 specifics** *(ST AN4839 "Level 1 cache on STM32F7/H7"; RM0433 bus matrix)*:

- STM32duino's H743 linker script puts `.data`/`.bss` and the heap in **AXI SRAM (RAM_D1,
  0x24000000)** (`variants/STM32H7xx/H742V…_H743V…/ldscript.ld`), which DMA1 and DMA2 can reach.
  DTCM (0x20000000) cannot be reached by DMA1 or DMA2. Don't place draw buffers there.
- STM32duino enables the D-cache in `main.cpp` (`SCB_EnableDCache()` unless `D_CACHE_DISABLED`).
  Either call `SCB_CleanDCache_by_Addr(px_map, len)` in `send_color` before starting the DMA, or
  put the draw buffers in an MPU non-cacheable region. Align the buffers to 32 B (the cache line
  size): `__attribute__((aligned(32)))`, or `LV_DRAW_BUF_ALIGN 32`.
- `lv_display_flush_ready()` may be called from the DMA-complete ISR. It's one of the two
  functions that are safe from any context
  ([integration/overview.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/overview.mdx)).
  With `lv_display_set_flush_wait_cb()`, the UI task can block on a semaphore given by that ISR
  instead of spinning.

**Driver.** Use LVGL's own `lv_st7789_create(320, 170, flags, send_cmd_cb, send_color_cb)`
(`LV_USE_ST7789 1`, which pulls in `LV_USE_GENERIC_MIPI`). You implement a blocking
`send_cmd` and a DMA `send_color` that calls `lv_display_flush_ready()` on completion. Use
`lv_st7789_set_gap()` for the 170-line panel's offset (often 35 rows on 1.9" 170x320 glass,
which must be checked on the real part), plus `set_invert` and rotation
([st7789.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/external_display_controllers/st7789.mdx),
[lv_lcd_generic_mipi.h](https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/drivers/display/lv_lcd_generic_mipi.h)).
SPI sends MSB first, but RGB565 in memory is little-endian. Set
`LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565_SWAPPED` so LVGL renders big-endian directly,
with no swap loop ([color_format.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/color_format.mdx)).
Alternatively, use a 16-bit SPI data size with the normal RGB565 format.

**DMA2D (Chrom-ART).** 9.6 has a DMA2D draw unit (`LV_USE_DRAW_DMA2D 1`,
`LV_DRAW_DMA2D_HAL_INCLUDE "stm32h7xx_hal.h"`). It accelerates fills and unscaled images only,
and drives the peripheral by register, not through HAL
([dma2d_gpu.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/chip_vendors/stm32/dma2d_gpu.mdx)).
At 54k pixels on a 480 MHz M7 it isn't needed. Leave it off at first and measure with
`LV_USE_SYSMON`/the perf monitor if rendering ever shows up.

**LVGL heap.** `LV_MEM_SIZE` defaults to 64 KB (in bytes in 9.6. The `_KILOBYTES` option was
removed). That's plenty for a few screens of labels. Watch it with `lv_mem_monitor()`.

---

## 6. Thread-safety with FreeRTOS

LVGL "is **not thread-safe**". Event, timer and animation callbacks are safe because they run
inside `lv_timer_handler()`. Only `lv_tick_inc()` and `lv_display_flush_ready()` may be called
from anywhere ([integration/overview.mdx, Operating Systems and Threads](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/overview.mdx)).
`lv_async_call()` defers a call to the next `lv_timer_handler()`, but "if
`lv_async_call()` is called from a thread other than the one that normally makes LVGL calls, you
are still obligated to protect the LVGL data structure using a MUTEX"
([timer.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/timer.mdx)).
So it **doesn't** remove the need for locking.

Given the project rule (LVGL is called from one task only):

- `LV_USE_OS LV_OS_NONE` is the simplest option *(inference)*. Nothing else calls LVGL, so you
  don't need `lv_lock()`, and you avoid the separate software draw thread that LVGL creates when
  an OS is configured (`LV_DRAW_THREAD_STACK_SIZE` 8 KB,
  [lv_conf_template.h#L188-L205](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h#L188-L205)).
  Provide the tick with `lv_tick_set_cb()` returning `xTaskGetTickCount()` in ms (or Arduino
  `millis()`), and sleep with `vTaskDelay()` or the queue wait below. With `LV_OS_FREERTOS`, the
  rule becomes "only take `lv_lock()` in the UI task". It also works, but only buys you a
  parallel draw thread on a single core.
- **The UI task loop**, which is event driven with no polling sleep:

```c
for (;;) {
    uint32_t wait = lv_timer_handler();                  /* ms until LVGL needs us again */
    if (wait == LV_NO_TIMER_READY || wait > 50) wait = 50;
    ui_msg_t m;
    while (xQueueReceive(ui_q, &m, pdMS_TO_TICKS(wait)) == pdTRUE) {
        ui_apply(&m);                                    /* lv_subject_set_*(), screen changes */
        wait = 0;                                        /* drain the rest without blocking */
    }
}
```

- Other services post **small value messages** (`{id, value}`) with `xQueueSend()`, without
  blocking or with a short timeout. For high-rate telemetry, have the producer overwrite a
  "latest value" slot (`xQueueOverwrite` on a 1-deep queue, or a seqlock'd struct) so the UI
  samples at frame rate instead of queuing every sample.
- The key task or ISR feeds a separate key-edge queue that only `read_cb` consumes (see §2).
- Inside `ui_apply`, setting `lv_subject`s that the labels are bound to
  (`lv_label_bind_text`) keeps services ignorant of widgets.

---

## 7. LVGL 8 to 9 (and 9.5 to 9.6) changes that matter here

From the v9.0 changelog ([release/v9.0 CHANGELOG.rst](https://github.com/lvgl/lvgl/blob/release/v9.0/docs/CHANGELOG.rst))
and [migration-v9-6.mdx](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/changelog/migration-v9-6.mdx):

- **Drivers**: `lv_disp_drv_t`, `lv_disp_draw_buf_t` and `lv_indev_drv_t` are gone. Use
  `lv_display_create()` + `lv_display_set_flush_cb()` + `lv_display_set_buffers(disp, b1, b2,
  size_in_bytes, mode)`; the size is **in bytes**, not pixels. For input, use
  `lv_indev_create()` + `lv_indev_set_type()` + `lv_indev_set_read_cb()`. The flush callback takes
  `uint8_t *px_map`, and the read callback takes `(lv_indev_t *, lv_indev_data_t *)`.
  `feedback_cb` is gone, and indev events (`lv_indev_add_event_cb`) replace it. **This is what
  makes the "keypad without group, listen on the indev" design clean.** Most v8 tutorials for
  keypads predate it.
- **Renames**: `disp` became `display`, `scr_act` became `screen_active`, `btn` became `button`,
  `img` became `image`, `del` became `delete`, `zoom` became `scale`, and `lv_obj_clear_flag`
  became `lv_obj_remove_flag`. `lv_coord_t` was replaced by `int32_t`. Most v8 forum code needs
  these renames applied.
- **Colour**: `lv_color_t` is always RGB888. `LV_COLOR_16_SWAP` was removed in 9.0 (with a
  warning shim in 9.6). In 9.6, `LV_COLOR_DEPTH` is replaced by
  `LV_COLOR_FORMAT_DEFAULT`, which can be `LV_COLOR_FORMAT_RGB565_SWAPPED`, the right setting for
  an SPI ST7789.
- **9.6 configuration**: `lv_conf_template.h` is now generated from Kconfig. Start from the 9.6
  template rather than an older `lv_conf.h`. `LV_CONF_MINIMAL` and `LV_MEM_SIZE_KILOBYTES` were
  removed.
- **9.6 headers**: the public API moved to `include/lvgl/`. Include `<lvgl.h>`/`lvgl/lvgl.h`,
  because headers under `src/` are now private and warn.
- **9.6 Observer**: use `lv_subject_create(type)` and `lv_subject_delete()` instead of
  `lv_subject_init_*()`.
- **v9.6 is the last v9 release.** Symbols marked `LV_DEPRECATED` are removed in v10, so build
  with `LV_DISABLE_API_MAPPING` and treat deprecation warnings as errors to stay v10-ready.
- Present in 9.6.0 and relevant: per-indev `lv_indev_set_long_press_time()` and
  `lv_indev_set_long_press_repeat_time()`, `lv_indev_set_key_remap_cb()`,
  `lv_display_set_flush_wait_cb()`, `lv_lcd_generic_mipi`/`lv_st7789` drivers, and the DMA2D
  draw unit.

---

## 8. Open points for the UI spec / prototype ticket

1. The hold semantics when one key offers both 3 s and 5 s (see §2), and whether hold progress
   is shown.
2. Whether a press on a faded legend wakes only, or wakes and acts.
3. Whether any key chord exists. If one does, use four indevs.
4. The exact panel: diagonal, ST7789 row/column gap, IPS or not, and key-centre positions
   relative to the active area. These decide the legend cell geometry and the font size.
5. Whether information screens may draw under the legend strip.

## Sources

- LVGL v9.6.0 source: <https://github.com/lvgl/lvgl/tree/v9.6.0>.
  [src/indev/lv_indev.c](https://github.com/lvgl/lvgl/blob/v9.6.0/src/indev/lv_indev.c),
  [lv_conf_template.h](https://github.com/lvgl/lvgl/blob/v9.6.0/lv_conf_template.h),
  [include/lvgl/drivers/display/](https://github.com/lvgl/lvgl/tree/v9.6.0/include/lvgl/drivers/display),
  [include/lvgl/core/lv_obj_style.h](https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/core/lv_obj_style.h)
- LVGL v9.6.0 docs sources (rendered at docs.lvgl.io / lvgl.io/docs/open/9.6):
  [indev/overview](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/overview.mdx),
  [indev/keypad](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/keypad.mdx),
  [indev/button](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/indev/button.mdx),
  [display/setup](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/setup.mdx),
  [display/screen_layers](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/screen_layers.mdx),
  [display/inactivity](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/inactivity.mdx),
  [display/color_format](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/display/color_format.mdx),
  [timer (lv_async_call)](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/main-modules/timer.mdx),
  [integration/overview (threads)](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/overview.mdx),
  [ST7789 driver](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/external_display_controllers/st7789.mdx),
  [STM32 DMA2D](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/integration/chip_vendors/stm32/dma2d_gpu.mdx),
  [migration v9.5 to v9.6](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/changelog/migration-v9-6.mdx)
- LVGL v9.0 changelog / migration: <https://github.com/lvgl/lvgl/blob/release/v9.0/docs/CHANGELOG.rst>
- Sitronix ST7789V3 datasheet (serial AC timing, TSCYCW 16 ns): <https://files.waveshare.com/wiki/common/ST7789V3_V0.1.pdf>
- STM32duino core (installed `framework-arduinoststm32`): `cores/arduino/main.cpp` (cache enable),
  `variants/STM32H7xx/H742V(G-I)(H-T)_H743V(G-I)(H-T)_H750VBT_H753VI(H-T)/ldscript.ld` (RAM placement)
- ST RM0433 (STM32H742/743/750/753 reference manual) and AN4839 (L1 cache on STM32F7/H7), cited
  for the DMA reachability, 16-bit transfer counters and cache maintenance. They weren't
  re-fetched for this note.
- Soft key concept: <https://en.wikipedia.org/wiki/Soft_key>
