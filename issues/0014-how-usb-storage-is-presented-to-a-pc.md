---
id: 14
title: How USB storage is presented to a PC
labels: [wayfinder:grilling]
status: open
assignee:
parent: 1
blocked_by: [4, 5, 18]
---

## Question

Which volumes does the composite USB device expose to the PC (SD card, USB-host memory stick, both as separate LUNs?), what happens when a stick is inserted or removed while the PC is attached, and how is access shared between firmware logging and the PC?

## Input from the user (2026-11-25)

The Storage Service must have Status Values for:
- **USB connected**;
- **bytes used**, shown in human-readable form;
- **total bytes**.

Settled with the user (2026-11-25):
- **Two connection Status Values**: "PC connected" (the device port is configured by a PC, i.e. `tud_mount_cb`) and "USB stick connected" (a stick is mounted on the host port, i.e. `tuh_msc_mount_cb`).
- **One used/total pair per volume**: SD card and USB stick.
- **Bytes are typed integer Status Values with unit `B`**. Human-readable formatting (e.g. "1.2 GiB") is applied where the value is displayed (web page, LCD). Home Assistant and SCPI get the exact number. Status Values therefore need a display hint for byte formatting, for [The Service contract](0009-the-service-contract.md) implementation.
