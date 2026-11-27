# Hardware test report

| | |
|---|---|
| Result | 56 passed, 1 skipped |
| Parent commit | aa87640e85a2 |
| Staged changes | sha256:8fa15157711321dd |
| Unstaged changes in working tree | yes |
| Firmware version | aa87640_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-28 00:11:53, 318.3 s |
| Flash / RAM used | 74,196 / 2,097,152 B, 57,700 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 3.63 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.62 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.92 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 1.83 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 9.06 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 2.78 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 9.72 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 11.74 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 2.72 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 6.41 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 19.31 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 16.32 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 15.75 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 5.84 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 5.71 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 10.18 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 13.80 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 19.81 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 7.90 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 10.95 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 4.82 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 8.31 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 1.37 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.28 s |
| `hardware_tests/test_framework.py::test_tick_periods` | passed | 11.80 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.18 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.17 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.28 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.28 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 1.58 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.08 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | passed | 11.25 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.57 s |
| `hardware_tests/test_framework.py::test_warning_raise` | passed | 1.79 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 2.17 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 3.87 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.38 s |
| `hardware_tests/test_framework.py::test_fatal_error` | passed | 12.50 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 1.92 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | passed | 21.13 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 8.39 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 12.73 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.26 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.31 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.36 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.08 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.08 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.24 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 3.19 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 1.02 s |
| `hardware_tests/test_oss_licences.py::test_service_registered` | passed | 7.67 s |
| `hardware_tests/test_oss_licences.py::test_firmware_and_source` | passed | 1.63 s |
| `hardware_tests/test_oss_licences.py::test_core_rows` | passed | 1.57 s |
| `hardware_tests/test_oss_licences.py::test_every_linked_component_listed` | passed | 1.74 s |
| `hardware_tests/test_oss_licences.py::test_licence_values` | passed | 1.58 s |
| `hardware_tests/test_oss_licences.py::test_application_acknowledgements` | passed | 1.62 s |
