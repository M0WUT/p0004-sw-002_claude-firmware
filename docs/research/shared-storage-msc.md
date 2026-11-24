# Sharing storage between the firmware and a connected PC

Research ticket: the GTE Core firmware (STM32H743VIT6, STM32duino on PlatformIO, TinyUSB) logs to an
SD card on SDMMC (4-bit) and to a USB memory stick on the USB host port. It also exposes the same media
to a PC as USB MSC LUNs on the USB device port. How do we avoid FAT corruption, and which SD/FAT stack
should we use on the H743?

Researched 2026-11-24. Each claim cites its source. Claims marked **(inference)** are my reasoning and
are not stated by a source.

---

## 1. Why sharing corrupts FAT

USB MSC exports *blocks*, not files. The PC runs its own FAT driver over those blocks, and it caches the
FAT, the directory entries and (with write-back caching) the data. The firmware's FatFs does the same in
its `FATFS` window and its `FIL` buffers. Two FAT drivers on one volume each believe they own the
allocation table. Nothing in USB MSC or SCSI tells a mounted host that *the file system* changed under
it. SCSI can only say "the medium is gone" or "the medium changed".

- CircuitPython: the MSC interface "works underneath the file system at the block level so using it
  excludes other types of workflows from manipulating the file system at the same time."
  ([CircuitPython workflows](https://docs.circuitpython.org/en/latest/docs/workflows.html))
- Adafruit: "CircuitPython cannot write to the filesystem at the same time as your computer."
  ([CircuitPython Essentials: Storage](https://learn.adafruit.com/circuitpython-essentials/circuitpython-storage))
- FatFs on one volume, even inside one program: opening a file for write twice, or renaming or deleting
  an open file, "can cause data collaption". `f_mount`/`f_mkfs` "are always not thread-safe to the
  volume processed". ([FatFs application note](https://elm-chan.org/fsw/ff/doc/appnote.html))
- Windows 10 1809 and later default removable drives to *Quick removal*, which turns off write-back
  caching. *Better performance* caches writes and needs Safely Remove.
  ([Microsoft Learn: default media removal policy](https://learn.microsoft.com/windows/client-management/change-default-removal-policy-external-storage-media))
  **(inference)** Quick removal limits how stale the PC's *writes* can get. It does not stop the PC
  caching *reads* of the FAT and directories, so firmware writes while the PC has the volume mounted are
  still unsafe.

**The rule every project below follows: at any moment, at most one side may write a volume. A side that
caches must re-read the volume after the other side has written to it.**

---

## 2. Strategies used by established projects

### 2.1 Single writer, other side read-only (CircuitPython model)

CircuitPython gives each LUN exactly one writer:

- CIRCUITPY: "writable by the host by default and read-only to CircuitPython". The SD card (`/sd`):
  "writable from CircuitPython by default and read-only to the host."
  ([workflows](https://docs.circuitpython.org/en/latest/docs/workflows.html))
- `storage.remount(readonly=...)` swaps the writer. After boot it raises `RuntimeError` if the host has
  write access, and remounting while USB is active "ejects" the drive from the host.
  `disable_concurrent_write_protection=True` turns the check off, with "the risk that the filesystem will
  be corrupted." ([storage module](https://docs.circuitpython.org/en/latest/shared-bindings/storage/index.html))
- How it is enforced, from [`supervisor/shared/usb/usb_msc_flash.c`](https://github.com/adafruit/circuitpython/blob/main/supervisor/shared/usb/usb_msc_flash.c):
  - `tud_msc_is_writable_cb()` returns false unless `filesystem_is_writable_by_usb()`. Once it reports
    writable, it takes a per-volume `blockdev_lock`. If the lock is held by Python code or by another
    workflow, the LUN is reported read-only.
  - TinyUSB turns a false `is_writable` into the MODE SENSE write-protect bit, and fails WRITE10 with
    DATA PROTECT / ASC 0x27. The host then mounts the LUN read-only.
    ([`msc_device.c`](https://github.com/hathach/tinyusb/blob/master/src/class/msc/msc_device.c))

Trade-offs for a logger:

- Pro: logging never stops. The PC can browse and copy logs live. No corruption is possible, as long as
  the PC really mounts the LUN read-only.
- Con: the PC's view is **stale**. It cached the FAT and directories at mount time and does not see
  files the firmware writes afterwards. **(inference)** It may even read sectors that belong to clusters
  the firmware has since reallocated. Users have to re-plug or eject and re-mount to see new data.
  CircuitPython gets around this with the eject/remount signalling in 2.3.
- Con: the PC cannot delete old logs or drop in config files unless the writer is swapped.

### 2.2 Exclusive handover (firmware unmounts while the PC has the volume)

The firmware gives up the whole volume while the PC owns it. It flushes and closes files, then calls
`f_unmount()`. FatFs invalidates any open file or directory objects
([f_mount](https://elm-chan.org/fsw/ff/doc/mount.html)). The LUN then reports ready and writable. When
the PC ejects, or USB is unplugged, the firmware re-mounts. FatFs only re-reads the volume when the
filesystem object is uninitialised or the drive reports `STA_NOINIT`
([f_mount](https://elm-chan.org/fsw/ff/doc/mount.html); `mount_volume()` in `ff.c` R0.15 returns early
when `fs_type != 0` and `disk_status()` lacks `STA_NOINIT`). So after a PC session the firmware must
either `f_mount()` again or have `disk_status()` return `STA_NOINIT` once.

- Adafruit's TinyUSB/SdFat example does the "firmware re-reads after host writes" half. Its write-complete
  callback calls `sd.card()->syncDevice(); sd.cacheClear(); // clear file system's cache to force
  refresh`. Its `loop()` does nothing, so the firmware never writes concurrently.
  ([msc_sdfat.ino](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/master/examples/MassStorage/msc_sdfat/msc_sdfat.ino))
- MicroPython pyboard: "Remember to eject (on Linux, unmount) the USB drive before you reset your
  pyboard". If corrupted, reformat via boot mode 3.
  ([pyboard general](https://docs.micropython.org/en/latest/pyboard/general.html))

Trade-offs:

- Pro: the PC gets full read/write, including deleting logs and writing config.
- Con: **logging to that medium stops** for the whole PC session. A test-equipment logger must then
  buffer in RAM, redirect to the other medium (SD vs. USB stick), or drop data.
- Con: there is no reliable signal that the PC has finished. A START STOP UNIT eject is sent by
  "Eject"/"Safely remove"/`eject` but not by a plain unplug or a crashed host. The firmware needs a
  fallback: USB suspend/unplug (VBUS), a timeout, or a front-panel "take back storage" action.

### 2.3 Media-change signalling: "medium not present" and eject handling

SCSI has no "file system changed" event. What projects do instead is make the medium *disappear*
briefly, so the host drops its cache and re-mounts.

- TinyUSB: if `tud_msc_test_unit_ready_cb()` returns false and no sense is set, TinyUSB sets NOT READY /
  MEDIUM NOT PRESENT (ASC 0x3A) by default. INQUIRY reports `is_removable = 1`. There is no built-in
  UNIT ATTENTION (ASC 0x28) handling.
  ([`msc_device.c`](https://github.com/hathach/tinyusb/blob/master/src/class/msc/msc_device.c),
  [`msc_device.h`](https://github.com/hathach/tinyusb/blob/master/src/class/msc/msc_device.h))
  The dual-LUN example simulates SD removal by returning false from TEST UNIT READY while a button is
  held. ([msc_disk_dual.c](https://github.com/hathach/tinyusb/blob/master/examples/device/msc_dual_lun/src/msc_disk_dual.c))
- CircuitPython (from `usb_msc_flash.c`, linked above):
  - Eject: on START STOP UNIT with `load_eject=1, start=0`, it runs `disk_ioctl(CTRL_SYNC)`, releases
    its lock and sets `ejected[lun]`. A later load succeeds only "if it hasn't been ejected". A bus
    reset un-ejects.
  - Remount: `usb_msc_remount()` sets `eject_once[lun]`. The next TEST UNIT READY then fails once with
    `tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00)`, and the host re-discovers the medium.
  - PREVENT/ALLOW MEDIUM REMOVAL is rejected with ILLEGAL REQUEST, so the OS cannot lock the medium in.
- TinyUSB passes SYNCHRONIZE CACHE (0x35) to `tud_msc_scsi_cb()`, so the application must answer it. A
  no-op success is fine if writes are synchronous. (`msc_device.c`)

Trade-offs:

- Pro: this is the only in-band way to force a host to see firmware-written data, or to take a volume
  back without unplugging.
- Con: **(inference)** host behaviour varies (Windows, macOS and Linux each poll TEST UNIT READY on
  removable media differently). A short "not present" blip can be missed, or can make the OS show a
  "disk not ejected properly" warning. Signalling "medium not present" while the host is mid-write
  causes the very corruption we want to avoid. Treat it as part of a handover protocol. It is not a
  substitute for single-writer.

### 2.4 Something else: file-level protocols (MTP) or a synthesised read-only view

- **MTP** works at the file level. The device keeps the only FAT driver and serves objects, so there is
  no shared-block-cache problem by construction, and the firmware can keep logging while the PC
  browses. TinyUSB now has a device MTP class (`src/class/mtp/mtp_device.{c,h}`) and an example
  (`examples/device/mtp`). ([tinyusb src/class/mtp](https://github.com/hathach/tinyusb/tree/master/src/class/mtp))
  Cons: it is newer and less battle-tested than MSC. macOS needs a third-party MTP client
  **(inference, well known)**. Throughput is lower than MSC. It costs more firmware work (object
  handles, partial transfers).
- **Synthesised read-only FAT** (the UF2/DAPLink "virtual disk" style): the firmware fabricates a
  read-only FAT image whose files map onto real log data. It is mentioned for completeness. It is
  complex for growing logs and still suffers host read caching. Not recommended here.

### 2.5 Summary for a test-equipment logger

| Strategy | Logging during PC session | PC can write | Corruption risk | PC sees new data | Effort |
|---|---|---|---|---|---|
| PC read-only, firmware writes (2.1) | Yes | No | None if PC honours WP | Only after re-mount (use 2.3 blip) | Low |
| Exclusive handover (2.2) | No (buffer or redirect) | Yes | None if handover is strict | Yes | Medium |
| Firmware read-only while USB attached (CircuitPython default for CIRCUITPY) | No | Yes | None | Yes | Low |
| Concurrent RW (no protection) | Yes | Yes | **High** | Unreliable | n/a, do not use |
| MTP instead of MSC (2.4) | Yes | Yes (file-level) | None at FS level | Yes | High |

**Suggested shape (inference):** default each LUN to *PC read-only, firmware writes* (2.1), so logging
never stops and the PC can pull data. Add an explicit, user-initiated *handover* (2.2) for when the PC
needs write access: front-panel action or vendor command → firmware closes files and `f_unmount()` →
TEST UNIT READY "not present" blip (2.3) → LUN reported writable. Return ownership on eject, USB
disconnect or timeout, then `f_mount()` again. The two media make handover cheap: while the PC owns
one, log to the other. Keep MTP as a later option if live read-write browsing becomes a requirement.

---

## 3. USB stick on the host port, re-exported as a device LUN

TinyUSB host MSC is asynchronous: `tuh_msc_read10(dev_addr, lun, buffer, lba, block_count,
complete_cb, arg)` / `tuh_msc_write10(...)`, plus `tuh_msc_mount_cb`/`tuh_msc_umount_cb`
([`msc_host.h`](https://github.com/hathach/tinyusb/blob/master/src/class/msc/msc_host.h)). The device
side's read10/write10 callbacks can return `TUD_MSC_RET_ASYNC` and later call `tud_msc_async_io_done()`
([`msc_device.h`](https://github.com/hathach/tinyusb/blob/master/src/class/msc/msc_device.h)). So a
pass-through LUN (PC → device MSC → host MSC → stick) maps naturally. Notes:

- The ownership rules in section 2 apply unchanged. The firmware's FatFs volume on the stick and the PC
  must not both write.
- With async I/O, the buffer belongs to the I/O until `tud_msc_async_io_done()`, even after an MSC
  reset (`msc_device.h`).
- Stick hot-unplug should be reported to the PC as NOT READY / 0x3A (the default when
  `test_unit_ready` returns false).
- **(inference)** Report the stick's real block size and count in `tud_msc_capacity_cb`. A stick with
  4 KiB logical sectors would break the default `FF_MAX_SS 512` in STM32SD's ffconf. This is unlikely,
  but it should be checked at mount.

---

## 4. SD / FAT libraries usable on STM32duino with H743 SDMMC

### 4.1 stm32duino STM32SD + stm32duino FatFs (recommended starting point)

- Targets SDIO/SDMMC hardware. 4-bit is the default (`SD_BUS_WIDE_4B`). SDMMC1 is the default instance,
  SDMMC2 can be selected. It also has card detect, a transceiver option, `SD_CLK_DIV` and
  `SD_HW_FLOW_CTRL` (default *disabled*). It depends on the stm32duino FatFs port.
  ([STM32SD README](https://github.com/stm32duino/STM32SD); `src/bsp_sd.c`, v1.5.0)
- FatFs port: elm-chan FatFs R0.15 with ST's `ff_gen_drv` layer, v4.0.4.
  ([stm32duino/FatFs](https://github.com/stm32duino/FatFs))
- **Raw block access: yes.** `bsp_sd.c` exposes `BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr,
  uint32_t NumOfBlocks, uint32_t Timeout)` and `BSP_SD_WriteBlocks(...)`, plus
  `BSP_SD_GetCardState()`, `BSP_SD_GetCardInfo()` (`LogBlockNbr`, `LogBlockSize`) and
  `BSP_SD_IsDetected()`. The FatFs glue `drivers/sd_diskio.c` calls exactly these. A TinyUSB
  `read10`/`write10` callback can call `BSP_SD_*Blocks` (or `disk_read`/`disk_write` on the pdrv)
  directly with `lba`, and `capacity_cb` can come from `BSP_SD_GetCardInfo`. `Sd2Card` itself has no
  public block API. (Source: `src/bsp_sd.c`, `src/Sd2Card.h`, FatFs `src/drivers/sd_diskio.c`.)
- **Transfer mode: polling, CPU-driven FIFO, not IDMA.** `BSP_SD_ReadBlocks` wraps `HAL_SD_ReadBlocks`
  ("in polling mode"). In the H7 HAL, polling reads move data through the FIFO by CPU on `RXFIFOHF` and
  `TXFIFOHE`. Only the `_DMA` variants program `IDMABASE0`/`IDMACTRL`.
  ([stm32h7xx_hal_sd.c](https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Src/stm32h7xx_hal_sd.c))
  - **D-cache consequence:** no cache maintenance is needed, and any RAM works, DTCM included, because
    the CPU does the copying.
  - **Costs:** it blocks the CPU for the whole transfer. **(inference)** With polling, an interrupt
    (for example USB) that delays FIFO service can cause RX overrun or TX underrun at high SDMMC clocks.
    NuttX's H7 SDMMC driver warns that "Large Non-DMA transfer may result in RX overrun failures"
    ([NuttX stm32_sdmmc.c](https://apache.googlesource.com/nuttx/+/71d7028c4a33e4c7fee5f1c62aad7890478e941c/arch/arm/src/stm32h7/stm32_sdmmc.c)).
    The mitigations to evaluate are enabling `SD_HW_FLOW_CTRL`, lowering `SD_CLK_DIV`, or switching to
    IDMA (4.3).
- **Configuration limits:** the default `ffconf` has `FF_VOLUMES 1`, `FF_FS_LOCK 0` and `FF_MAX_SS 512`
  (`src/ffconf_default_80286.h`). Mounting both SD and USB stick needs `FF_VOLUMES ≥ 2` through
  `ffconf_custom.h` (picked up by `ffconf.h` via `__has_include`), plus a second `Diskio_drvTypeDef`
  for the USB stick, linked with `FATFS_LinkDriver` (`ff_gen_drv.h`). STM32SD's `SdFatFs::init()`
  hard-wires `FATFS_LinkDriver(&SD_Driver, ...)` and `f_mount(..., 1)`, and `SDClass::end()` unmounts and
  unlinks. Those are the hooks for handover.

### 4.2 greiman SdFat

- SdFat 2.3.1 has SDIO drivers only for Teensy (`HAS_TEENSY_SDIO`) and RP2040 PIO (`HAS_PIO_SDIO`). On
  every other target `HAS_SDIO_CLASS` defaults to 0. On `STM32_CORE_VERSION` it only provides a custom
  **SPI** driver. ([SdFat `src/SdFatConfig.h`](https://github.com/greiman/SdFat/blob/master/src/SdFatConfig.h))
- It has a clean block API (`readSectors`/`writeSectors`/`syncDevice`, used directly by Adafruit's MSC
  example) and a `USE_BLOCK_DEVICE_INTERFACE` hook for an external `FsBlockDevice`. Using it on H743
  SDMMC therefore means **writing our own SDMMC block driver** (wrapping the HAL) under SdFat.
- Verdict: usable only if we want SdFat's FAT/exFAT layer badly enough to write the driver. It gives no
  advantage for MSC.

### 4.3 ST's STM32Cube FatFs middleware DMA drivers (reference for an IDMA upgrade)

The stm32duino FatFs package ships only the polling `sd_diskio.c`. ST's upstream middleware
([stm32_mw_fatfs](https://github.com/STMicroelectronics/stm32_mw_fatfs), `source/drivers/sd/`) also
has `sd_diskio_dma_standalone.c` and `sd_diskio_dma_rtos.c`. They show the established H7 IDMA pattern:

- With `ENABLE_SD_DMA_CACHE_MAINTENANCE == 1`, a 32-byte-aligned buffer is DMA'd directly, with
  `SCB_CleanDCache_by_Addr` before a write and `SCB_InvalidateDCache_by_Addr` after a read. A buffer
  that is not 32-byte aligned is bounced one block at a time through a
  `scratch[BLOCKSIZE] __attribute__((aligned(32)))` buffer.
- This could be copied into a custom `Diskio_drvTypeDef` linked instead of `SD_Driver`, or wrapped by a
  modified `bsp_sd.c`.

### 4.4 H7 D-cache and IDMA placement facts (if IDMA is used)

- **The stm32duino core enables the D-cache by default on Cortex-M7.** `premain()` in
  `cores/arduino/main.cpp` calls `SCB_EnableDCache()` unless `D_CACHE_DISABLED` is defined
  ([main.cpp](https://github.com/stm32duino/Arduino_Core_STM32/blob/main/cores/arduino/main.cpp)).
- **Default RAM placement is AXI SRAM.** In the H743V variant linker script, `.data`, `.bss`, heap and
  stack all go to `RAM_D1` (0x24000000). DTCM (`RAM`, 0x20000000, 128K) is declared but unused, and
  `RAM_D2` (0x30000000, 288K) and `RAM_D3` are available.
  ([ldscript.ld](https://github.com/stm32duino/Arduino_Core_STM32/blob/main/variants/STM32H7xx/H742V(G-I)(H-T)_H743V(G-I)(H-T)_H750VBT_H753VI(H-T)/ldscript.ld))
  Ordinary globals are therefore reachable by SDMMC1 IDMA, but they are **cacheable**, so maintenance or
  an MPU non-cacheable region is mandatory.
- **IDMA reach:** SDMMC1's master port sits on the 64-bit AXI matrix (D1) and reaches AXI SRAM, Flash,
  QSPI and FMC. SDMMC2 is on the D2 AHB matrix and also reaches SRAM1/2. Neither reaches DTCM.
  ([AN5200, Getting started with STM32H7 SDMMC](https://www.st.com/resource/en/application_note/an5200-getting-started-with-stm32h7-series-sdmmc-host-controller-stmicroelectronics.pdf);
  [AN4891, H7 system architecture](https://www.st.com/resource/en/application_note/dm00306681-stm32h72x-stm32h73x-and-singlecore-stm32h74x75x-system-architecture-and-performance-stmicroelectronics.pdf)).
  I could not extract the PDF tables in this session. Confirm the bus-master table against RM0433 before
  relying on SDMMC2 + SRAM1/2.
- FatFs itself warns that `disk_read`/`disk_write` buffers may be unaligned (`BYTE*`) and "may be out of
  reach in DMA", for example TCM used for the stack. Its remedies are a bounce buffer or keeping
  FATFS/FIL/buffers off the stack. ([disk_read](https://elm-chan.org/fsw/ff/doc/dread.html))
- TinyUSB side: the MSC endpoint buffer is `TUD_EPBUF_DEF(buf, CFG_TUD_MSC_EP_BUFSIZE)`. Its size and
  alignment pad to a cache line when `CFG_TUD_MEM_DCACHE_ENABLE` is set (default 0,
  `CFG_TUSB_MEM_DCACHE_LINE_SIZE` default 1). Set it to 1 with line size 32 on H7 if DWC2 DMA is used
  ([tusb_types.h](https://github.com/hathach/tinyusb/blob/master/src/common/tusb_types.h),
  [tusb_option.h](https://github.com/hathach/tinyusb/blob/master/src/tusb_option.h)). That buffer is what
  `read10`/`write10` receive. Sizing `CFG_TUD_MSC_EP_BUFSIZE` as a multiple of 512, and ≥ 4 KiB, gives
  multi-block SD transfers and lets the buffer be 32-byte aligned, which avoids the scratch-bounce path.
  **(inference)**

### 4.5 Concurrency between TinyUSB and firmware FatFs

- Both the MSC callbacks and the logger's FatFs calls end up in `BSP_SD_*`, which uses one global
  `uSdHandle`. Serialise them: run `tud_task()` and the logger in the same loop, or put a mutex around
  the block layer. FatFs locks only at the volume level (`FF_FS_REENTRANT`), and only its own API calls
  ([appnote](https://elm-chan.org/fsw/ff/doc/appnote.html)). **(inference)**
- TinyUSB read10/write10 may return `TUD_MSC_RET_BUSY` (0) to be retried later, which is useful while
  the logger holds the card ([`msc_device.h`](https://github.com/hathach/tinyusb/blob/master/src/class/msc/msc_device.h)).

---

## 5. Recommendation

1. **Library:** use **stm32duino STM32SD + FatFs**. It is the only option with H7 SDMMC support out of
   the box. Use `BSP_SD_ReadBlocks`/`BSP_SD_WriteBlocks` (or `disk_read`/`disk_write`) as the TinyUSB
   MSC backend. Start with polling, which is cache-safe and works from any RAM. Add a custom IDMA
   `Diskio_drvTypeDef` modelled on ST's `sd_diskio_dma_standalone.c` only if throughput or CPU load
   demands it.
2. **Sharing policy:** single writer per LUN. By default the firmware writes and the PC is read-only
   (`tud_msc_is_writable_cb` → false). Add an explicit handover for PC write access with
   unmount/remount, a TEST UNIT READY 0x3A blip and START STOP UNIT eject handling, in the same way as
   CircuitPython's `usb_msc_flash.c`. Never allow concurrent read-write.

## 6. Open unknowns

- How Windows, macOS and Linux behave on a short TEST UNIT READY "not present" blip while a read-only
  mount is open. Does each OS reliably drop its cache and re-mount, and how long must the blip last?
  Needs bench testing.
- Whether a polling STM32SD on H743 at 4-bit high clock overruns when USB interrupts are active, and
  whether `SD_HW_FLOW_CTRL_ENABLE` fixes it on H7. ST community reports are mixed.
- Exact RM0433 bus-master reachability for SDMMC2 (D2) versus SDMMC1, which depends on which instance
  the hardware uses. The AN tables were not extracted in this session.
- TinyUSB dual-role on H743: device on one DWC2 port and host on the other, through STM32duino. Covered
  by the TinyUSB integration ticket, not here. Adafruit_TinyUSB_Arduino's stm32duino support is "Still
  WIP, only support/tested with F4, only support OTG_FS"
  ([Adafruit_TinyUSB_Arduino README](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)), so plain
  TinyUSB integration is likely needed.
- How the firmware detects "PC finished" when the host never sends an eject (plain unplug or host
  crash). Options are VBUS sensing, a suspend timeout or a manual action. This is a product decision.
- MTP maturity in TinyUSB and its macOS support, if live read-write browsing becomes a requirement.
