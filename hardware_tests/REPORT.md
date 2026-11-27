# Hardware test report

| | |
|---|---|
| Result | 29 passed |
| Parent commit | 44df8a990bdb |
| Staged changes | sha256:307ae7c9538db43b |
| Unstaged changes in working tree | no |
| Firmware version | 44df8a9_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 18:28:40, 106.4 s |
| Flash / RAM used | 55,144 / 2,097,152 B, 3,784 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 2.55 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.43 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.36 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 0.67 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 6.28 s |
| `hardware_tests/test_framework.py::test_tick_periods` | passed | 11.20 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 0.52 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 0.59 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 0.66 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 0.67 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 0.99 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 4.52 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | passed | 10.65 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 0.98 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.20 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 1.55 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 3.30 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 1.82 s |
| `hardware_tests/test_framework.py::test_fatal_error` | passed | 11.96 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 1.37 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | passed | 20.46 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 7.28 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 12.15 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.24 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.28 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.31 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.21 s |
