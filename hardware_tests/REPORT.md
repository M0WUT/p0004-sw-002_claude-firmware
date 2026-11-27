# Hardware test report

| | |
|---|---|
| Result | 11 passed |
| Parent commit | ff247501cd20 |
| Staged changes | sha256:21f275151f1be5d7 |
| Unstaged changes in working tree | no |
| Firmware version | ff24750_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 17:46:38, 18.9 s |
| Flash / RAM used | 36,928 / 2,097,152 B, 1,992 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 2.51 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.27 s |
| `hardware_tests/test_boot.py::test_clocks` | passed | 0.31 s |
| `hardware_tests/test_boot.py::test_scheduler_timing` | passed | 10.93 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 0.64 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.24 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.27 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.32 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.21 s |
