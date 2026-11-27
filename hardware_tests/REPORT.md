# Hardware test report

| | |
|---|---|
| Result | 47 passed, 3 failed, 1 skipped |
| Parent commit | 7afee46f8891 |
| Staged changes | sha256:07f6c7ad8add3f6c |
| Unstaged changes in working tree | no |
| Firmware version | 7afee46_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 21:14:23, 317.0 s |
| Flash / RAM used | 72,148 / 2,097,152 B, 57,276 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 3.15 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.68 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.97 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 1.87 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 9.81 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 2.91 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 9.34 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 11.85 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 2.87 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 6.91 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 19.73 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 16.56 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 16.30 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 6.22 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 6.13 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 10.63 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 14.71 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 20.74 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 8.12 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 11.90 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 5.29 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 9.04 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 1.42 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.46 s |
| `hardware_tests/test_framework.py::test_tick_periods` | failed | 12.01 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.35 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.26 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.32 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.40 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 2.10 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.53 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | failed | 11.89 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.71 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.84 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 2.35 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 4.26 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.55 s |
| `hardware_tests/test_framework.py::test_fatal_error` | passed | 12.73 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 2.63 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | failed | 21.33 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 9.12 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 12.93 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.30 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.35 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.35 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.26 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 4.64 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 0.94 s |
