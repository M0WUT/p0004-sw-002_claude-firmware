---
id: 8
title: GTE Core and Instrument repo structure
labels: [wayfinder:grilling]
status: open
assignee:
parent: 1
blocked_by: [2]
---

## Question

How is GTE Core (this repo, sw-002) laid out so an Instrument repo can include a tagged version as a git submodule? What goes in each Instrument repo's `platformio.ini`; where do the LCD Board pin map, IO Board model definitions and Instrument Board definitions live; how is `ServiceId` extended per Instrument; and how does the example Instrument under `examples/` build against Core the same way an outside repo would?
