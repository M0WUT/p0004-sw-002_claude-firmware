---
id: 12
title: Config storage format
labels: [wayfinder:grilling]
status: open
assignee:
parent: 1
blocked_by: [9]
---

## Question

Does GTE Core keep sw-001's EEPROM format (`docs/eeprom.md`: "LID" header, config allocation table indexed by ServiceId, per-config length/version/CRC8) or change it? How does a Service migrate its stored config when its struct version changes; what happens when an Instrument adds ServiceIds; and how is a factory reset triggered?
