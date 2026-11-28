# Hardware test report

| | |
|---|---|
| Result | 119 passed, 1 failed, 1 skipped |
| Parent commit | e0878fc242ef |
| Staged changes | sha256:123ae83113495d5a |
| Unstaged changes in working tree | no |
| Firmware version | e0878fc_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-28 14:11:37, 743.5 s |
| Flash / RAM used | 111,016 / 2,097,152 B, 69,292 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 2.99 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.98 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.97 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 2.44 s |
| `hardware_tests/test_config_storage.py::test_values_persist` | passed | 12.97 s |
| `hardware_tests/test_config_storage.py::test_format_matches_sw001` | passed | 3.55 s |
| `hardware_tests/test_config_storage.py::test_block_grows_and_shifts` | passed | 11.48 s |
| `hardware_tests/test_config_storage.py::test_eeprom_full_fatal` | passed | 13.48 s |
| `hardware_tests/test_config_storage.py::test_stale_application_block_ignored` | passed | 7.26 s |
| `hardware_tests/test_config_storage.py::test_migration_chain` | passed | 7.63 s |
| `hardware_tests/test_config_storage.py::test_missing_migration_fatal` | passed | 22.16 s |
| `hardware_tests/test_config_storage.py::test_downgrade_fatal` | passed | 21.58 s |
| `hardware_tests/test_config_storage.py::test_blank_prompt` | passed | 18.58 s |
| `hardware_tests/test_config_storage.py::test_corrupt_block_prompt` | passed | 6.95 s |
| `hardware_tests/test_config_storage.py::test_corrupt_table_prompt` | passed | 6.29 s |
| `hardware_tests/test_config_storage.py::test_hold_key4_reinitialises` | passed | 11.63 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_keys_at_power_on` | passed | 15.72 s |
| `hardware_tests/test_config_storage.py::test_factory_reset_before_config_load` | passed | 20.13 s |
| `hardware_tests/test_config_storage.py::test_debug_eeprom_is_ram_only` | passed | 7.90 s |
| `hardware_tests/test_config_storage.py::test_mac_untouched` | passed | 11.48 s |
| `hardware_tests/test_config_storage.py::test_fatal_record_logged_next_boot` | passed | 5.63 s |
| `hardware_tests/test_config_storage.py::test_watchdog_record_and_warning` | passed | 8.55 s |
| `hardware_tests/test_config_storage.py::test_soft_key_pins` | passed | 1.66 s |
| `hardware_tests/test_config_storage.py::test_real_soft_keys` | skipped | 0.00 s |
| `hardware_tests/test_framework.py::test_initialise_order` | passed | 1.45 s |
| `hardware_tests/test_framework.py::test_tick_periods` | passed | 12.09 s |
| `hardware_tests/test_framework.py::test_application_base` | passed | 1.34 s |
| `hardware_tests/test_framework.py::test_config_printed_at_boot` | passed | 1.33 s |
| `hardware_tests/test_framework.py::test_config_update_path` | passed | 1.51 s |
| `hardware_tests/test_framework.py::test_status_values` | passed | 1.54 s |
| `hardware_tests/test_framework.py::test_log_line_format` | passed | 1.86 s |
| `hardware_tests/test_framework.py::test_log_levels` | passed | 5.66 s |
| `hardware_tests/test_framework.py::test_time_from_boot` | passed | 11.90 s |
| `hardware_tests/test_framework.py::test_error_code_numbering` | passed | 1.82 s |
| `hardware_tests/test_framework.py::test_warning_raise` | failed | 2.03 s |
| `hardware_tests/test_framework.py::test_warning_update_in_place` | passed | 3.38 s |
| `hardware_tests/test_framework.py::test_warning_clear` | passed | 4.52 s |
| `hardware_tests/test_framework.py::test_nine_warnings_fatal` | passed | 2.75 s |
| `hardware_tests/test_framework.py::test_fatal_error` | passed | 13.02 s |
| `hardware_tests/test_framework.py::test_stack_overflow_fatal` | passed | 2.84 s |
| `hardware_tests/test_framework.py::test_watchdog_quiet_when_healthy` | passed | 21.57 s |
| `hardware_tests/test_framework.py::test_watchdog_resets_hung_task` | passed | 8.64 s |
| `hardware_tests/test_framework.py::test_watchdog_frozen_under_debugger` | passed | 14.40 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.20 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.23 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.27 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.06 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.06 s |
| `hardware_tests/test_harness.py::test_hook_waits_for_bench` | passed | 3.01 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.28 s |
| `hardware_tests/test_harness.py::test_python_style` | passed | 2.88 s |
| `hardware_tests/test_harness.py::test_cpp_style` | passed | 1.13 s |
| `hardware_tests/test_oss_licences.py::test_service_registered` | passed | 10.31 s |
| `hardware_tests/test_oss_licences.py::test_firmware_and_source` | passed | 2.28 s |
| `hardware_tests/test_oss_licences.py::test_core_rows` | passed | 1.97 s |
| `hardware_tests/test_oss_licences.py::test_every_linked_component_listed` | passed | 1.82 s |
| `hardware_tests/test_oss_licences.py::test_licence_values` | passed | 1.75 s |
| `hardware_tests/test_oss_licences.py::test_application_acknowledgements` | passed | 1.79 s |
| `hardware_tests/test_scpi_application.py::test_application_commands` | passed | 8.41 s |
| `hardware_tests/test_scpi_application.py::test_numeric_suffix` | passed | 4.98 s |
| `hardware_tests/test_scpi_application.py::test_command_table_limits[scpi_table_full-SCPI command table full: TFULl:N\\d+]` | passed | 0.95 s |
| `hardware_tests/test_scpi_application.py::test_command_table_limits[scpi_collision-SCPI command hash collision or duplicate: TCOLlisionA:]` | passed | 1.12 s |
| `hardware_tests/test_scpi_common.py::test_idn` | passed | 15.76 s |
| `hardware_tests/test_scpi_common.py::test_rst` | passed | 4.39 s |
| `hardware_tests/test_scpi_common.py::test_cls` | passed | 3.98 s |
| `hardware_tests/test_scpi_common.py::test_esr_ese` | passed | 3.98 s |
| `hardware_tests/test_scpi_common.py::test_stb_sre` | passed | 4.04 s |
| `hardware_tests/test_scpi_common.py::test_opc_wai` | passed | 4.20 s |
| `hardware_tests/test_scpi_common.py::test_tst` | passed | 3.99 s |
| `hardware_tests/test_scpi_common.py::test_pyvisa_asrl` | passed | 3.94 s |
| `hardware_tests/test_scpi_diag.py::test_diag_locked` | passed | 19.76 s |
| `hardware_tests/test_scpi_diag.py::test_diag_warning` | passed | 4.19 s |
| `hardware_tests/test_scpi_diag.py::test_diag_fatal` | passed | 17.84 s |
| `hardware_tests/test_scpi_diag.py::test_diag_hang_watchdog` | passed | 17.69 s |
| `hardware_tests/test_scpi_docs.py::test_every_command_documented` | passed | 4.44 s |
| `hardware_tests/test_scpi_docs.py::test_application_commands_documented` | passed | 9.17 s |
| `hardware_tests/test_scpi_docs.py::test_worked_examples_accepted` | passed | 11.50 s |
| `hardware_tests/test_scpi_gte.py::test_service_catalog` | passed | 8.19 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_catalog` | passed | 4.42 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_read` | passed | 4.66 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_set` | passed | 4.24 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[count-105--222]` | passed | 4.37 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[count-12--224]` | passed | 4.10 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[gain-9.0--221]` | passed | 3.99 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[count-abc--104]` | passed | 4.26 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[count-"20"--104]` | passed | 4.01 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[name-GTE2--104]` | passed | 5.36 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[name-"xxxxxxxxxxxxxxxxx"--222]` | passed | 4.86 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[mode-BOGUS--224]` | passed | 3.93 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[mode-MIN--224]` | passed | 4.09 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_errors[nope-1--224]` | passed | 3.95 s |
| `hardware_tests/test_scpi_gte.py::test_unknown_service` | passed | 3.81 s |
| `hardware_tests/test_scpi_gte.py::test_secret_parameter` | passed | 5.28 s |
| `hardware_tests/test_scpi_gte.py::test_parameter_defaults` | passed | 4.01 s |
| `hardware_tests/test_scpi_gte.py::test_restart_required` | passed | 4.13 s |
| `hardware_tests/test_scpi_gte.py::test_status_values` | passed | 5.51 s |
| `hardware_tests/test_scpi_gte.py::test_log_level_over_scpi` | passed | 5.90 s |
| `hardware_tests/test_scpi_parser.py::test_long_short_case` | passed | 4.02 s |
| `hardware_tests/test_scpi_parser.py::test_compound_commands` | passed | 3.82 s |
| `hardware_tests/test_scpi_parser.py::test_quoted_strings` | passed | 3.96 s |
| `hardware_tests/test_scpi_parser.py::test_terminators` | passed | 4.61 s |
| `hardware_tests/test_scpi_parser.py::test_pipelined_lines` | passed | 3.98 s |
| `hardware_tests/test_scpi_parser.py::test_split_line` | passed | 6.17 s |
| `hardware_tests/test_scpi_parser.py::test_undefined_header` | passed | 4.56 s |
| `hardware_tests/test_scpi_parser.py::test_overlong_line` | passed | 4.06 s |
| `hardware_tests/test_scpi_session.py::test_needs_dtr` | passed | 5.87 s |
| `hardware_tests/test_scpi_session.py::test_release_and_reopen` | passed | 5.54 s |
| `hardware_tests/test_scpi_system.py::test_syst_vers` | passed | 4.41 s |
| `hardware_tests/test_scpi_system.py::test_error_queue` | passed | 4.11 s |
| `hardware_tests/test_scpi_system.py::test_error_queue_overflow` | passed | 4.05 s |
| `hardware_tests/test_scpi_system.py::test_warning_in_error_queue` | passed | 4.03 s |
| `hardware_tests/test_scpi_system.py::test_ques_warning_bit` | passed | 4.89 s |
| `hardware_tests/test_scpi_system.py::test_oper_register` | passed | 5.01 s |
| `hardware_tests/test_scpi_system.py::test_enable_queries` | passed | 3.93 s |
| `hardware_tests/test_scpi_system.py::test_stat_pres` | passed | 3.93 s |
| `hardware_tests/test_scpi_system.py::test_syst_reboot` | passed | 12.45 s |
| `hardware_tests/test_scpi_system.py::test_factory_reset` | passed | 8.72 s |
| `hardware_tests/test_usb_device.py::test_enumerates_core` | passed | 8.02 s |
| `hardware_tests/test_usb_device.py::test_enumerates_application` | passed | 7.93 s |
| `hardware_tests/test_usb_device.py::test_reenumerates_after_reset` | passed | 12.04 s |
| `hardware_tests/test_usb_device.py::test_cdc_soak` | passed | 9.06 s |
