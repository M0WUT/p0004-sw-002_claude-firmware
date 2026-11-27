# Hardware test report

| | |
|---|---|
| Result | 46 passed, 4 failed, 1 skipped |
| Parent commit | 4d5284189e47 |
| Staged changes | sha256:9f69bc91f005c969 |
| Unstaged changes in working tree | yes |
| Firmware version | 4d52841_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 21:21:51, 338.0 s |
| Flash / RAM used | 72,148 / 2,097,152 B, 57,276 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 3.31 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.88 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.93 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 2.05 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 10.92 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 3.52 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 12.11 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 12.58 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 3.40 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 8.91 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 21.46 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 17.23 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 17.28 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 6.90 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 6.74 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 10.73 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 14.95 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 21.32 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 11.53 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 12.63 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 6.04 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 9.37 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 2.00 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.53 s |
| `hardware_tests/test_framework.py::test_tick_periods` | failed | 12.06 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.58 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.69 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.63 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.60 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 1.87 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.66 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | failed | 11.58 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.84 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.95 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 2.38 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 4.62 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.85 s |
| `hardware_tests/test_framework.py::test_fatal_error` | failed | 12.91 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 2.30 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | failed | 21.39 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 9.41 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 13.32 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.32 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.33 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.38 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.08 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.09 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.28 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 3.47 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 0.94 s |
