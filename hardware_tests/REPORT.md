# Hardware test report

| | |
|---|---|
| Result | 50 passed, 1 skipped |
| Parent commit | f198ab876e47 |
| Staged changes | sha256:3675673b95e0990f |
| Unstaged changes in working tree | no |
| Firmware version | f198ab8_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 19:38:30, 298.8 s |
| Flash / RAM used | 72,148 / 2,097,152 B, 57,272 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 2.66 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.53 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.98 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 1.81 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 8.58 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 2.76 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 9.02 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 11.76 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 2.59 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 6.35 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 18.83 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 16.12 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 15.80 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 5.80 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 5.65 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 10.12 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 13.71 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 19.63 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 7.13 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 10.86 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 4.84 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 8.33 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 1.37 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.32 s |
| `hardware_tests/test_framework.py::test_tick_periods` | passed | 11.81 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.21 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.23 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.23 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.27 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 1.54 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.07 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | passed | 11.33 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.63 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.80 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 2.19 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 3.86 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.38 s |
| `hardware_tests/test_framework.py::test_fatal_error` | passed | 12.57 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 1.96 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | passed | 21.25 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 8.39 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 12.85 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.27 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.30 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.34 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.08 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.26 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 3.40 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 0.90 s |
