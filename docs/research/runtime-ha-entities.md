# Runtime-generated Home Assistant entities

Research ticket: can `dawidchyrzynski/arduino-home-assistant` (ArduinoHA) build Home
Assistant (HA) entities at runtime from the Config Parameter and Status Value lists that
exist only once the Services have started, instead of as compile-time globals? Which
entity types fit each field type, and how do we show config read-only in HA?

Researched 2026-11-24. Sources:

- ArduinoHA at the commit sw-001 pins, `1d333ab229b2dcefedf1b51e033f2177b74fca8c`
  (abbreviated `AHA@1d333ab` below).
- PubSubClient v2.8.
- HA core `dev` at `39842000c541b78604d301012590d1d70e8c8ebe` (abbreviated `HA@3984200`).
- The HA MQTT integration docs.
- The sw-001 sources under `lib/`.

## TL;DR

- **Yes, with constraints.** ArduinoHA entities are ordinary C++ objects. They register
  themselves with the `HAMqtt` singleton when constructed, so they can be created with
  `new` at any time, including after `mqtt.begin()`. The library's author confirms that
  this is supported as long as each entity "remains in the memory for a lifetime"
  ([issue #103](https://github.com/dawidchyrzynski/arduino-home-assistant/issues/103)).
- **Hard constraints:**
  - The registry is a fixed array sized by a constructor argument. The default is 24,
    the type is `uint8_t` so the maximum is 255, and entities past the limit are dropped
    without any error.
  - Entities cannot be removed: there is no deregistration call and no virtual
    destructor.
  - Every string is stored as a pointer to memory the caller owns.
  - Discovery is published only when MQTT connects, so the cleanest pattern is to create
    every entity *before the first connection*.
- **The main gap is `entity_category`.** ArduinoHA 2.1.0 cannot emit `entity_category`
  ([issue #280](https://github.com/dawidchyrzynski/arduino-home-assistant/issues/280),
  and PRs #238 and #276 are still unmerged). We need it so that config values appear
  under "Diagnostic" and not as primary sensors. It can be added *without a fork* by
  subclassing `HABaseDeviceType`, whose serializer and publish hooks are `protected`.
- **Read-only config in HA:** use `sensor` and `binary_sensor` entities only, never
  `text`, `number`, `switch` or `select`, because those create HA-side controls that
  publish to a command topic. HA's MQTT `sensor` and `binary_sensor` schemas **reject
  `entity_category: config`**. Read-only config values must therefore use
  `entity_category: diagnostic` (or no category).
- **Recommendation:**
  - Keep ArduinoHA (already a dependency, pinned, 2.1.0 is also the latest release) as the
    MQTT transport and entity framework.
  - Add one small in-repo subclass, `HAReadOnlyEntity`, that emits
    `ent_cat`/`unit_of_meas`/`dev_cla`/`stat_t`. Allocate one entity per exported field
    after the Services initialise and before `begin()`.
  - Construct `HAMqtt` with an explicit `maxDevicesTypesNb`.
- **Fallback:** hand-written discovery JSON published through `HAMqtt::publish` or
  PubSubClient. It costs roughly 200–400 lines of our own JSON and topic code, but it
  allows device-based discovery and full control.

## 1. How HAMqtt stores entities ("device types")

| Fact | Source |
|---|---|
| The constructor is `HAMqtt(Client&, HADevice&, const uint8_t maxDevicesTypesNb = HAMQTT_DEFAULT_DEVICES_LIMIT)` | [HAMqtt.h L76-80](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.h#L76-L80) |
| `HAMQTT_DEFAULT_DEVICES_LIMIT` is 24, or 6 on ATmega328P/168 | [HAMqtt.h L20-24](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.h#L20-L24) |
| The registry is `new HABaseDeviceType*[maxDevicesTypesNb]`, allocated once in the constructor and never grown | [HAMqtt.cpp L25](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L25) |
| `addDeviceType()` **does nothing** once the array is full and reports no error | [HAMqtt.cpp L208-215](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L208-L215) |
| The count and limit are `uint8_t`, so there are at most 255 entities per `HAMqtt` | [HAMqtt.h, private members](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.h) |
| `HAMqtt` is a singleton: the constructor sets the static `_instance = this`, and entities find it through `HAMqtt::instance()` | [HAMqtt.cpp L34, L57-66](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L57-L66) |
| An entity registers itself **in its base constructor** if `HAMqtt::instance()` is non-null. The `HAMqtt` must therefore exist before any entity is constructed. | [HABaseDeviceType.cpp L7-21](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/device-types/HABaseDeviceType.cpp#L7-L21) |
| There is no `removeDeviceType`. `HABaseDeviceType` has **no virtual destructor**: a grep for `~HA` finds destructors only on HAMqtt, HADevice, HASelect, HAHVAC, HADeviceTrigger and the serializer classes. | source grep |
| Discovery config is published only from `onMqttConnected()`, which runs for each registered entity in `HAMqtt::onConnectedLogic()` when the state changes to connected | [HAMqtt.cpp L322-333](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L322-L333) |

Consequences for the GTE Core:

1. **Construct `HAMqtt` with an explicit limit.** sw-001 constructs it with the default of
   24 ([HomeAssistantService.cpp L3](#7-observations-on-the-current-sw-001-code)). That is
   too few once every Service exports up to 16 config fields (`MAX_CONFIG_FIELDS`) plus up
   to 16 status fields (`StatusFieldList::MAX_FIELDS`). Choose a compile-time cap such as
   128 (the array costs 4 bytes per slot). Count the entities you create and log when the
   cap is hit, because the library drops extras without warning.
2. **Create entities after the Services initialise and before the first `begin()`/connect.**
   Entities created then are published at connect time with no further work. sw-001
   already calls `begin()` lazily from `tick()` once the link is up, which leaves a natural
   window for this.
3. **Entities created after the connection exists** do not publish their config until the
   next reconnect, because `onMqttConnected()` is `protected`
   ([issue #103 follow-up](https://github.com/dawidchyrzynski/arduino-home-assistant/issues/103),
   and the [unmerged PR #263](https://github.com/dawidchyrzynski/arduino-home-assistant/pull/263)
   that would add a public `setConfig()`). There are two workarounds:
   - A subclass can call the protected `publishConfig()` itself.
   - `mqtt.disconnect()` followed by `begin()` again: `disconnect()` resets
     `_lastConnectionAttemptAt` to 0, so the reconnect happens immediately
     ([HAMqtt.cpp L162-175](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L162-L175)).
4. **Never `delete` an entity.** `HAMqtt` keeps the raw pointer, so a deleted entity
   leaves a dangling pointer that `loop()` and `processMessage()` will dereference. The
   entity set must be fixed for the lifetime of the firmware run. That matches our case,
   because the field lists are fixed once the Services have started. If a field ever
   disappears, it has to be cleared from HA with an empty retained config payload, which
   ArduinoHA does not do for you.

## 2. Memory per entity

The values below are `sizeof` results from compiling the ArduinoHA headers with
`g++ -m32`. That is an ILP32 target like Cortex-M7, but the ARM EABI aligns `int64_t`
(inside `HANumeric`) to 8 bytes rather than 4, so add up to about 8 bytes on target.

| Object | Bytes (i386) | Notes |
|---|---|---|
| `HABaseDeviceType` | 28 | vptr plus component, uniqueId, name and objectId pointers, the serializer pointer, and the availability flag |
| `HASensor` | 68 | adds device_class, state_class, icon and unit pointers, force_update, and the `expire_after` `HANumeric` |
| `HASensorNumber` | 88 | adds precision and a cached `HANumeric` value |
| `HABinarySensor` | 56 | |
| `HANumeric` | 16 | `int64_t` value, precision, isSet |
| `HASerializer` + entries | 12 + 16 × N | **transient:** built in `publishConfig()` and destroyed straight after ([HABaseDeviceType.cpp L78-107](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/device-types/HABaseDeviceType.cpp#L78-L107)). N is 13 for a sensor and 9 for a binary sensor. |

Each entity also needs:

- About 8 bytes of malloc overhead.
- 4 bytes for its registry slot.
- **Caller-owned strings.** ArduinoHA stores only the pointer for `uniqueId`, name, unit,
  device class and icon ([HASensor.h setters](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/device-types/HASensor.h)).
  Issue #103 shows the classic bug: several entities named from one reused buffer all end
  up with the same name.

sw-001's `ConfigFieldList` is a by-value snapshot (`ConfigField` is about 170 bytes × 16),
so the HA service must copy `id` and `label` into its own long-lived storage: about
32 + 48 bytes per field, plus a buffer for the latest value. Each entity therefore costs
about **200 bytes in total**, so 100 entities take about 20 KB. That is easy on the
STM32H743's 1 MB of SRAM, but put the storage in one pool allocated at startup rather than
in many small `new` calls, to avoid fragmentation.

## 3. MQTT buffer size and discovery payloads

- **The PubSubClient buffer** defaults to `MQTT_MAX_PACKET_SIZE` = 256 and can be changed
  at runtime with `setBufferSize()`, which `HAMqtt::setBufferSize` passes through
  ([PubSubClient.h L24-26](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.h#L24-L26),
  [PubSubClient.cpp L740-757](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.cpp#L740-L757)).
- **ArduinoHA streams discovery and state publishes, so the payload never passes through
  that buffer.** `publishConfig()` calls `beginPublish(topic, calculateSize(), true)`, and
  the serializer then writes each JSON fragment with `writePayload()`.
  - PubSubClient's `beginPublish` puts only the fixed header and topic in its buffer.
  - `write()` goes straight to `Client::write`.
  - `endPublish()` is a no-op that returns 1.

  Sources: [PubSubClient.cpp L526-555](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.cpp#L526-L555)
  and [HAMqtt.cpp L217-265](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L217-L265).

  The 256-byte buffer therefore limits only **topic length plus header**, and **incoming**
  messages, which we do not need because the HA side is read-only. PubSubClient
  calculates the remaining length as `uint16_t`, so one publish can carry at most about
  64 KB.
- **Streaming costs packets on the W5500.** In the HeAtNet Ethernet fork that sw-001
  pins, every `EthernetClient::write()` calls `socketSend()`. That copies the data into
  the TX buffer, issues `Sock_SEND`, and **spins until `SEND_OK`**
  ([EthernetClient.cpp L83-89](https://github.com/HeAtNet/Ethernet/blob/eb14bb73589d3a09033b23a1d760ef38b1d4f4a4/src/EthernetClient.cpp#L83-L89),
  [socket.cpp `socketSend`](https://github.com/HeAtNet/Ethernet/blob/eb14bb73589d3a09033b23a1d760ef38b1d4f4a4/src/socket.cpp)).
  - ArduinoHA's serializer makes several `write()` calls per JSON property (quote, key,
    `":`, quote, value, quote, comma), so one sensor config is roughly 50–80 tiny TCP
    segments.
  - With about 100 entities that is several thousand blocking SENDs on every (re)connect.
    It is correct, but slow and bursty. I have not measured it; **measure on hardware.**
  - Mitigation: wrap `EthernetClient` in a `Client` that buffers about 1–2 KB and is
    flushed explicitly by the HA service after `loop()` and after each batch of
    `setValue()` calls, because PubSubClient never calls `Client::flush()`. The
    hand-written JSON alternative avoids the problem by rendering into a RAM buffer and
    writing it once.
- **HA side:** the MQTT docs give no payload size limit. They recommend device-based
  discovery to "reduce the number of messages", and a random delay before sending
  discovery to avoid broker load
  ([HA MQTT docs, discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)).
  HA truncates and rejects entity *states* longer than 255 characters
  (`MAX_LENGTH_STATE_STATE`, `check_state_too_long` in
  [mqtt/util.py L422](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/util.py#L422)).
  sw-001 display values are at most 47 characters, so this does not matter for us.

## 4. unique_id and topics

- The config topic is `<discovery_prefix>/<component>/<deviceUniqueId>/<entityUniqueId>/config`
  ([HASerializer.cpp L40-72](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/utils/HASerializer.cpp#L40-L72)).
  The state topic is `aha/<deviceUniqueId>/<entityUniqueId>/stat_t`
  ([L101-131](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/utils/HASerializer.cpp#L101-L131)).
- HA accepts only `[a-zA-Z0-9_-]+` for the node id and the object id in the discovery topic
  ([mqtt/discovery.py L74-77](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/discovery.py#L74-L77)).
  ArduinoHA does not validate this, so build entity ids such as `<serviceId>_<fieldId>`
  from that character set only. sw-001 field ids such as `mqttBrokerAddress` already
  comply.
- The JSON `uniq_id` is the entity's `uniqueId`. If
  `HADevice::enableExtendedUniqueIds()` has been called, it becomes
  `<deviceUniqueId>_<uniqueId>`
  ([HASerializer.cpp L351-366, L540-560](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/utils/HASerializer.cpp#L540-L560)).
  **Enable extended unique ids.** Without them, two GTE Core units on the same HA would
  both publish `uniq_id: "ha_mqttBrokerAddress"` and collide in the entity registry.
- The device unique id cannot be changed once it is set
  ([HADevice.cpp L50-60](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HADevice.cpp#L50-L60)).
  It is also the MQTT client id
  ([HAMqtt.cpp L304-313](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L304-L313)).
- ArduinoHA writes string values into the JSON **without escaping**
  ([HASerializer.cpp L431-450](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/utils/HASerializer.cpp#L431-L450)).
  A label containing `"` or `\` breaks the discovery payload. Field labels are compile-time
  constants, so this is a code-review rule. The device name, however, comes from user
  config and needs sanitising.
- `setObjectId()` emits `obj_id`, which HA core `dev` no longer recognises. The
  abbreviation table now has `def_ent_id` → `default_entity_id`
  ([abbreviations.py L46](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/abbreviations.py#L46))
  and no longer has `obj_id`. The sensor discovery schema uses `REMOVE_EXTRA`
  ([mqtt/sensor.py L169](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/sensor.py#L169)),
  so HA drops the key without complaint. Don't rely on `setObjectId()`.

## 5. Entity types for each field type, and keeping config read-only

HA's MQTT `sensor` and `binary_sensor` platforms validate `entity_category` with
`entity_category_without_config`. That function raises an error for `config` and accepts
only `diagnostic`
([mqtt/util.py L353-365](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/util.py#L353-L365),
[mqtt/sensor.py L90](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/sensor.py#L90),
[mqtt/binary_sensor.py L63](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/binary_sensor.py#L63)).
`entity_category: config` is intended for *controls* such as `text`, `number`, `switch`
and `select`. Those platforms send commands: MQTT `text` publishes to its `command_topic`
([mqtt/text.py L183](https://github.com/home-assistant/core/blob/39842000c541b78604d301012590d1d70e8c8ebe/homeassistant/components/mqtt/text.py#L183)),
so HA's UI would offer an edit box. This is the opposite of what we want.

**Conclusion:** every config value becomes a `sensor` or `binary_sensor` with
`entity_category: diagnostic`. HA then lists it under the device's "Diagnostic" section,
and it cannot be edited because the entity has no command topic. The firmware does not
need to subscribe to anything.

| sw-001 type | HA entity | ArduinoHA class | Notes |
|---|---|---|---|
| Config `Text` | `sensor` (string state) | `HASensor`, `setValue(const char*)` | Exclude secrets. See section 7: the MQTT password field. |
| Config `Integer` | `sensor` | `HASensor` with the display string, or `HASensorNumber` (P0) | Set `unit_of_measurement` if there is one. With a unit, HA requires the state to be numeric. |
| Config `Float` | `sensor` | `HASensor` with `displayValue` | `HASensorNumber` supports only precisions P0–P3 ([HABaseDeviceType.h `NumberPrecision`](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/device-types/HABaseDeviceType.h)). sw-001 allows up to 8 decimals, so publishing the already formatted `displayValue` string is simpler. `suggested_display_precision` (`sug_dsp_prc`) is not supported by ArduinoHA. |
| Config `Boolean` | `binary_sensor` | `HABinarySensor`, `setState(bool)` | Publishes `ON`/`OFF`. |
| Config `IpAddressLike` | `sensor` (string) | `HASensor` | No HA device class exists for IP addresses. Icon `mdi:ip-network`. |
| Status value (numeric) | `sensor` | `HASensor`/`HASensorNumber` | Unit, `device_class` (e.g. `temperature`, `voltage`) and `state_class: measurement` are all supported by ArduinoHA. |
| Status value (on/off) | `binary_sensor` | `HABinarySensor` | `device_class` (e.g. `connectivity`) is supported. |

What ArduinoHA 2.1.0 can emit:

- **Sensor:** `name`, `uniq_id`, `dev_cla`, `stat_cla`, `ic`, `unit_of_meas`, `frc_upd`,
  `exp_aft`, `json_attr_t`, `dev` and `avty_t`
  ([HASensor.cpp `buildSerializer`](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/device-types/HASensor.cpp)).
- **Binary sensor:** the same set without the unit and state class.
- **Not supported:** `entity_category`, `enabled_by_default`, `suggested_display_precision`
  and device-based discovery (`cmps`). A grep of `src/` for `categ` and `ent_cat` finds
  nothing.

The relevant upstream work is still open: issue
[#280](https://github.com/dawidchyrzynski/arduino-home-assistant/issues/280), and PRs
[#238](https://github.com/dawidchyrzynski/arduino-home-assistant/pull/238) and
[#276](https://github.com/dawidchyrzynski/arduino-home-assistant/pull/276). The repo was
last pushed on 2025-07-07, has 46 open issues, and 2.1.0 (2024-02-09) is still the latest
release. The sw-001 pin `1d333ab` *is* the 2.1.0 tag and the current `master` HEAD, so no
newer upstream code exists.

**Data-model gap in sw-001:**

- `StatusField` is only `{label[32], value[32]}`, with no stable id, no type and no unit
  ([ServiceStatus.h](#7-observations-on-the-current-sw-001-code)). An HA entity needs:
  - a stable `unique_id`, because the label may change;
  - a type, to choose between `sensor` and `binary_sensor`;
  - optionally a unit or device class.
- `ConfigField` has an id and a type but no unit, and no "sensitive / do not export" flag.

Whichever approach we choose, the Status and Config field descriptors need:

- an `id`;
- a type;
- an optional unit;
- an optional HA device class;
- an `exportToHa` or `secret` flag.

## 6. Options

### A. ArduinoHA as-is

- It works for sensor and binary_sensor with units and device class.
- **It cannot set `entity_category`**, so config values would appear as primary sensors
  beside the real measurements, which clutters the HA device page and dashboards.
- Cost: lowest.
- Not recommended on its own.

### B. ArduinoHA with an in-repo subclass (recommended)

`HABaseDeviceType` exposes the extension points we need as `protected`:

- `buildSerializer()` (virtual)
- `onMqttConnected()` (virtual)
- `publishConfig()`
- `publishOnDataTopic()`
- `_serializer`
- `_name`
- `_uniqueId`

Source: [HABaseDeviceType.h](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/device-types/HABaseDeviceType.h).

`HASerializer::set(property, value)` accepts any property name, and properties with a
`nullptr` value are skipped
([HASerializer.cpp L173-188](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/utils/HASerializer.cpp#L173-L188)).
A subclass of about 60 lines, `HAReadOnlyEntity(component = sensor|binary_sensor)`, can
therefore:

- build a serializer with name, `uniq_id`, `ent_cat` (`diagnostic` or none),
  `dev_cla`, `unit_of_meas`, `stat_cla`, `ic`, `dev`, `avty_t` and `stat_t`;
- publish config and retained state on connect;
- expose a public `republishConfig()` that calls `publishConfig()`.

This approach:

- needs no fork, and keeps the pinned library and its reconnect and LWT handling;
- costs about 200 bytes per entity, as in section 2;
- has two risks: it depends on `protected` internals, which could change if the library is
  ever bumped (it has not moved since February 2024), and it keeps the streaming
  small-writes cost from section 3.

Pair it with:

- `HAMqtt(client, device, 128)`;
- `device.enableExtendedUniqueIds()`;
- a buffered `Client` wrapper if the reconnect burst proves slow on hardware.

### C. Hand-written discovery JSON on top of HAMqtt or PubSubClient

Use `HAMqtt::publish()`/`beginPublish()`/`writePayload()` (public,
[HAMqtt.h L284-329](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.h#L284-L329))
from an `onConnected` callback, or use PubSubClient directly.

What you gain:

- Every HA option: `ent_cat`, `en`, `sug_dsp_prc` and `origin`.
- **Device-based discovery**: one retained payload per device with a `cmps` map. `dev` and
  `o` are mandatory at the root. Single components are removed with an empty retained
  payload.
- One RAM render followed by a single `write()`, so the W5500 sends a few segments
  instead of hundreds.
- No registry cap and no lifetime coupling.

Source: [HA MQTT docs](https://www.home-assistant.io/integrations/mqtt/#device-discovery-payload).

What it costs:

- About 200–400 lines of our own code: JSON writer with escaping, topic builder,
  availability/LWT, birth-message (`homeassistant/status`) handling, and state publishing.
- Unit tests for that code.
- A device payload of about 8–15 KB for about 100 components.
  - PubSubClient handles that through `beginPublish` streaming (≤64 KB) and an 8–16 KB
    render buffer on the H743.
  - Alternatively, publish per component and keep each message at about 300 bytes.

### D. Replace PubSubClient with 256dpi/arduino-mqtt

[arduino-mqtt](https://github.com/256dpi/arduino-mqtt) v2.6.0, released 2026-11-18:

- supports QoS 1/2;
- since 2.5.2 "the message payload is sent directly during publishing", so its write
  buffer only needs to hold the header and topic ([README notes](https://github.com/256dpi/arduino-mqtt#notes));
- **but** ArduinoHA is hard-wired to PubSubClient
  ([HAMqtt.cpp L4, L62](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/HAMqtt.cpp#L57-L66)),
  so choosing it means option C plus a library swap;
- v2.6.0 is less than a week old, so it has had little field exposure.

There is no benefit for a read-only, QoS 0 use case. Not recommended.

## 7. Observations on the current sw-001 code

These are side findings, not part of the question. The source is
`lib/HomeAssistantService/` and `lib/ConfigData/ConfigData.h` in sw-001.

- **The MQTT password is a config field.** `get_config_fields` adds `mqttPasword` (sic)
  as a Text field. A generic "export every config field to HA" loop would publish the
  broker password as a retained sensor. A `secret` or `noExport` flag is mandatory.
- **The device id is 64 hex characters.**
  `_haDevice.setUniqueId((const byte*)haDeviceName, sizeof(haDeviceName))` hex-encodes all
  32 bytes of the name buffer, trailing NULs included
  ([HAUtils.cpp `byteArrayToStr`](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/1d333ab229b2dcefedf1b51e033f2177b74fca8c/src/utils/HAUtils.cpp)).
  The result is a 64-character device id that is used in every topic and as the MQTT
  client id. MQTT 3.1.1 only guarantees that brokers accept 1–23 byte client ids; Mosquitto
  accepts longer ones. Prefer the MAC address (6 bytes → 12 hex characters) or
  `strlen(name)`.
- The `HAMqtt` member uses the default limit of 24 devices types. See section 1.
- `_haMqtt(HAMqtt(_client, _haDevice))` relies on C++17 guaranteed copy elision.
  Otherwise `HAMqtt::_instance` would point at a destroyed temporary, because `HAMqtt` has
  no copy constructor that fixes `_instance`. It is fine under C++17, and GCC elides the
  copy in practice anyway, but it is fragile. I did not check which `-std` the STM32duino
  build uses. Prefer `_haMqtt(_client, _haDevice)`.
