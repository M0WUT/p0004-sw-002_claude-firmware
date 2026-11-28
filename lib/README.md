# GTE Core libraries

GTE Core's code is split into four PlatformIO libraries, one per kind of module. Every header is included as `gte/<file>.h`, whichever folder it is in.

| Folder | Holds | Today |
|---|---|---|
| `drivers/` | Low-level drivers: talk to one piece of hardware, with no Config Parameters or Status Values. | I2C bus, 24AA256UID EEPROM, Soft Keys |
| `helpers/` | Helpers: shared infrastructure used by every Service. Not Services, and no Config Parameters. | logging, time, the Service registry, config storage, watchdog, Fatal Errors, Error Codes, CRC-8 |
| `services/` | Services: the Service base class, Config Parameters and Status Values, Service ids, Core's own Services (CORE, USB_DEVICE, SCPI, and OSS_LICENCES: open source components and their licences), and Core startup (`gte.h`). | `BaseService`, `CoreService`, `UsbDeviceService` (TinyUSB CDC), `ScpiService` (with `ScpiParser`, Core's commands and the `scpi.h` API Applications register through), `OssLicencesService`, `gte::Start()` |
| `application_services/` | The base class for an Instrument's Application Service. Each Instrument's own Application Service lives in its Instrument repo. | `ApplicationService` |

`tinyusb/` is TinyUSB 0.21.0, vendored as delivered (see its README); `include/tusb_config.h` configures it. The Vrekrer SCPI parser is fetched by PlatformIO, pinned by commit in `platformio.ini`.

The libraries include each other in both directions (for example, Helpers report Fatal Errors through Core startup, and Services use the Helpers), so `platformio.ini` sets `lib_archive = no` to link their objects directly.
