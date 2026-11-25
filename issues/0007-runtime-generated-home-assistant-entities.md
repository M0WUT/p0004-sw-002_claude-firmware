---
id: 7
title: Runtime-generated Home Assistant entities
labels: [wayfinder:research]
status: closed
assignee: claude-research-agent
parent: 1
blocked_by: []
---

## Question

Can `dawidchyrzynski/arduino-home-assistant` (as used in sw-001) create entities at runtime from a list of fields that is only known after the Services have started, rather than as compile-time globals? What are its limits on entity count, memory per entity, and MQTT buffer size, and does it support read-only entities for each Config Parameter type (text, integer, float, boolean, IP address) as well as sensors for Status Values? If it can't, what are the alternatives (raw MQTT discovery with PubSubClient, etc.)?

## Resolution

Full findings: branch `research/runtime-ha-entities` (commit 40f282a, local only, authored as "Claude"), file `docs/research/runtime-ha-entities.md`.

- **Version**: ArduinoHA 2.1.0 (`1d333ab`, as pinned in sw-001) is still the latest release. Upstream has been inactive since July 2025.
- **Runtime entities**: yes. An entity registers itself when it's constructed, so it can be created with `new` after `mqtt.begin()` (confirmed by the author in issue #103). The limits:
  - The registry is a fixed array sized by `maxDevicesTypesNb` (default 24, maximum 255). Entities past the limit are **dropped silently**.
  - Entities can't be removed.
  - Strings are stored as pointers, so the caller must keep its own copies.
  - Discovery is published on connect, so create every entity before the first connect.
- **Memory**: about 200 bytes per entity including strings. About 100 entities is roughly 20 KB.
- **Buffer**: discovery messages are streamed past PubSubClient's 256-byte buffer, but at roughly 50–80 small W5500 writes per entity on each reconnect. This needs measuring on hardware.
- **Read-only config**: HA rejects `entity_category: config` on sensor and binary_sensor. So config values become sensor or binary_sensor entities with `entity_category: diagnostic` and no command topic, which makes them unwritable in HA.
- **Gap**: the library has no `entity_category` support (issue #280 and PRs #238/#276 are unmerged).
- **Recommendation**:
  - Keep ArduinoHA and add an in-repo subclass of about 60 lines, `HAReadOnlyEntity`, to supply `entity_category`.
  - Construct `HAMqtt` with an explicit limit (for example 128) and call `enableExtendedUniqueIds()`.
  - Fallback: write the discovery JSON ourselves via `HAMqtt::publish`, about 200–400 lines.
- **Flags for other tickets**:
  - Config Parameters need a **secret / don't-export** flag: the MQTT password would otherwise be published. (The Service contract.)
  - Status Values need an id, type and unit. (The Service contract.)
  - The sw-001 device id is 64 hex characters of padded name buffer. (Mapping Services to Home Assistant.)
