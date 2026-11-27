# Hardware test report

| | |
|---|---|
| Result | 50 passed, 1 skipped |
| Parent commit | 9e25051f1a00 |
| Staged changes | sha256:2a1765c5b440820d |
| Unstaged changes in working tree | no |
| Firmware version | 9e25051_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 23:36:42, 302.0 s |
| Flash / RAM used | 72,148 / 2,097,152 B, 57,276 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 3.41 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.59 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.91 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 1.75 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 9.43 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 2.64 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 9.85 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 11.71 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 2.59 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 6.30 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 19.65 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 15.95 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 15.46 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 5.52 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 5.54 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 9.91 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 13.62 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 19.48 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 8.11 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 10.94 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 4.83 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 8.36 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 1.42 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.29 s |
| `hardware_tests/test_framework.py::test_tick_periods` | passed | 11.84 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.25 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.26 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.25 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.33 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 1.59 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.14 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | passed | 11.33 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.60 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.77 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 2.15 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 3.84 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.39 s |
| `hardware_tests/test_framework.py::test_fatal_error` | passed | 12.63 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 1.95 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | passed | 21.26 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 8.40 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 12.83 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.28 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.32 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.38 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.08 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.08 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.25 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 3.54 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 0.90 s |
