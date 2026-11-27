# Hardware test report

| | |
|---|---|
| Result | 46 passed, 4 failed, 1 skipped |
| Parent commit | af8ca5e142ec |
| Staged changes | sha256:917380eeba29b76a |
| Unstaged changes in working tree | no |
| Firmware version | af8ca5e_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 20:58:13, 317.9 s |
| Flash / RAM used | 72,148 / 2,097,152 B, 57,272 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 3.52 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.66 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.88 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 1.74 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 9.60 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 2.79 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 10.18 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 11.75 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 2.73 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 6.59 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 19.81 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 16.42 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 16.01 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 6.02 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 5.99 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 10.61 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 14.80 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 21.09 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 8.94 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 11.88 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 6.36 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 9.08 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 1.52 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.38 s |
| `hardware_tests/test_framework.py::test_tick_periods` | failed | 11.99 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.35 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.30 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.65 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.65 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 1.75 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.50 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | failed | 11.41 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.76 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.94 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 2.34 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 4.29 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.84 s |
| `hardware_tests/test_framework.py::test_fatal_error` | failed | 12.81 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 2.57 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | failed | 21.65 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 9.11 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 13.19 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.23 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.26 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.31 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.22 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 3.32 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 0.87 s |
