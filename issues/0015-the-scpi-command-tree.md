---
id: 15
title: The SCPI command tree
labels: [wayfinder:grilling]
status: open
assignee:
parent: 1
blocked_by: [6, 9]
---

## Question

What does the SCPI command tree look like: the IEEE 488.2 mandatory commands (`*IDN?`, `*RST`, ...), how every Config Parameter and Status Value is reachable through commands generated from each Service, how an Instrument Service adds its own commands, and how errors are reported? The same parser serves USB CDC and TCP port 5025.
