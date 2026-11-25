---
id: 19
title: How warnings work
labels: [wayfinder:grilling]
status: open
assignee:
parent: 1
blocked_by: []
---

## Question

Now that `_error()`/`_errorf()` raise a Fatal Error, how should `_warning()`/`_warningf()` be used? Cover:

- what counts as a warning rather than a Fatal Error or a Status Value;
- whether a warning is a one-off event or a condition that stays active until it's cleared (sw-001 returns an id "so it can be cleared later");
- who clears a warning, and how;
- how active warnings reach the user: the LCD, the RGB LED, the web page, the SCPI error queue or status registers, and Home Assistant;
- whether warnings are kept across a restart or logged to storage.

Background: sw-001's `NamedTask::_warning` returns an id from `LoggingHelper::warning`. See [The Service contract](0009-the-service-contract.md) for the Fatal Error behaviour.
