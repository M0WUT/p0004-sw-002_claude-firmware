---
id: 5
title: Sharing storage between the firmware and a connected PC
labels: [wayfinder:research]
status: closed
assignee: claude-research-agent
parent: 1
blocked_by: []
---

## Question

When the firmware logs to an SD card (and a USB-host-attached memory stick) while also exposing the same media to a PC as USB MSC, how do established embedded projects avoid FAT corruption: exclusive handover, read-only exposure, media-change/eject signalling, or something else? Which FAT/SD libraries are usable on STM32duino with the H743's SDMMC (STM32duino STM32SD/FatFs, SdFat, others) and do they support sharing the block device with a TinyUSB MSC callback?

## Resolution

Full findings: branch `research/shared-storage-msc` (commit ee325dd, local only), file `docs/research/shared-storage-msc.md`.

- **The rule**: no established project lets the PC and the firmware write the same FAT volume at once. Use one writer per volume, and the other side re-reads after a change.
- **Strategies**:
  - **PC read-only, firmware keeps logging** (CircuitPython's approach for SD): safe, and logging never stops. The PC's view goes stale until it re-mounts, and the PC can't delete or add files.
  - **Exclusive handover**: the firmware unmounts and the PC gets read/write, but logging to that volume pauses. There's no reliable "PC is done" signal: an eject arrives only if the user ejects, and a plain unplug sends nothing. So it needs cable-unplug detection, a timeout or a front-panel action.
  - **Firmware read-only while USB is attached**: a poor fit for a logger.
  - **"Medium not present" blip and eject handling**: briefly reporting the medium as missing forces the PC to re-mount. This helps a handover but can't replace single-writer.
  - **MTP instead of MSC**: safe for live read/write, but more work, slower, and weak on macOS.
  - **Suggested**: PC read-only by default, plus a user-initiated handover. While the PC owns one volume, log to the other.
- **Library**: stm32duino STM32SD with the stm32duino FatFs port. It supports SDMMC 4-bit, and its `BSP_SD_ReadBlocks`/`BSP_SD_WriteBlocks` map directly onto TinyUSB's read10/write10 callbacks.
  - It copies by CPU, not DMA, so the D-cache isn't a concern. The core enables the D-cache with RAM in AXI SRAM.
  - For a later move to DMA: follow ST's FatFs DMA driver (32-byte-aligned buffers, cache clean/invalidate, nothing in DTCM).
  - Mounting both SD and stick needs a custom FatFs config file and a second disk driver for the stick.
  - greiman SdFat has no STM32 SDIO driver, so it's out.
- **Open**:
  - How reliably each OS re-mounts after a "not present" blip, and how long the blip must be.
  - Whether CPU-driven transfers overrun at high SD clock while USB interrupts are active.
  - How the firmware decides the PC has finished when no eject arrives. This is for **How USB storage is presented to a PC**.
