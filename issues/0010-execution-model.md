---
id: 10
title: Execution model
labels: [wayfinder:grilling]
status: closed
assignee: M0WUT
parent: 1
blocked_by: [4, 5]
---

## Question

Does the firmware keep sw-001's cooperative `tick()` loop or move to FreeRTOS (or a hybrid), given that TinyUSB device and host, LVGL, the W5500, SD/MSC file IO and SCPI all need regular servicing, and blocking calls such as DHCP must not stall the UI?

## Resolution

**FreeRTOS, one task per Service**, using the `STM32duino FreeRTOS` library.

- **Startup**: `setup()` registers every Service and calls each `initialise()` in turn, before the scheduler runs. It then creates one task per Service and starts the scheduler. `loop()` is unused.
- **Tasks**:
  - Each Service's task runs `tick()` and then waits with `vTaskDelayUntil`.
  - Each Service declares a stack size, priority and tick period, and `BaseService` provides defaults.
  - Priorities: USB Device and USB Host highest, then Display/UI, then everything else.
  - No busy-waiting. Blocking calls such as DHCP block only their own task.
  - Stack-overflow checking is on. An overflow raises a Fatal Error naming the Service.
- **Calls between Services**: still only from `tick()`, looked up through ServiceHelper by `ServiceId`.
  - Every Service has a lock, provided by `BaseService`. Its public methods take it, and so do the generic config and status paths (including config updates arriving from Web or SCPI).
  - **Rule: never hold your own lock while calling another Service.**
- **LVGL**: only the display-owning Service's task calls `lv_*`. Other Services pass data through its public methods, and it reads Status Values on its own tick.
- **Shared hardware**: one mutex per shared resource, inside the code that owns it.
  - I2C4: in `I2CBus` (EEPROM, TMP102, IO Board, EMC2302, INA3221).
  - W5500 sockets: in the Network Service, taken around each socket operation (Web, HA, SCPI over TCP, NTP).
  - Debug UART: in `LoggingHelper`.
  - Services never handle these locks directly.
- **Fatal Errors**: the raising task calls `vTaskSuspendAll()` and masks the other interrupts, then runs the fatal loop with busy-waits. The loop shows the error on the LCD once, blinks the red LED at 0.5 Hz and repeats the error on the UART every 2 s (as decided in [The Service contract](0009-the-service-contract.md)).
- **Watchdog**: none for now (in the fog). If one is added, the fatal loop feeds it.
- **USB**: TinyUSB is built with `CFG_TUSB_OS = OPT_OS_FREERTOS`. Each USB Service runs `tud_task()` or `tuh_task()` in its own task. [USB spike on the LCD Board](0018-usb-spike-on-the-lcd-board.md) proves this setup.
