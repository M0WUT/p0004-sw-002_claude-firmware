---
id: 13
title: The IO Board interface
labels: [wayfinder:grilling]
status: open
assignee:
parent: 1
blocked_by: [3]
---

## Question

How does the LCD Board identify and talk to the IO Board: what is the format of the IO Board's identity EEPROM (model number and anything else), what is the I2C register map of the IO Expander (power-source status, LEDs, GPIO), and what stand-in does the firmware use until the IO Board exists? The EMC2302 is driven directly by the H743 on the I2C bus.
