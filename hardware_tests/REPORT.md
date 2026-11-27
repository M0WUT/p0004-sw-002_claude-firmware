# Hardware test report

| | |
|---|---|
| Result | 10 passed, 1 failed |
| Parent commit | 06d2d4996129 |
| Staged changes | sha256:208e15064dabe93d |
| Unstaged changes in working tree | no |
| Firmware version | 06d2d49_dirty |
| Board serial (STM32 UID) | 44001F000751343031333736 |
| IO Board | not fitted |
| Instrument Board | not fitted |
| Run | 2026-11-27 17:46:05, 18.9 s |
| Flash / RAM used | 36,928 / 2,097,152 B, 1,992 / 524,288 B |

## Tests

| Test | Result | Time |
|---|---|---|
| `hardware_tests/test_boot.py::test_build_fits` | passed | 2.45 s |
| `hardware_tests/test_boot.py::test_boot_line` | passed | 3.36 s |
| `hardware_tests/test_boot.py::test_clocks` | failed | 0.32 s |
| `hardware_tests/test_boot.py::test_scheduler_timing` | passed | 10.92 s |
| `hardware_tests/test_boot.py::test_reset_reboots` | passed | 0.65 s |
| `hardware_tests/test_harness.py::test_report_contents` | passed | 0.22 s |
| `hardware_tests/test_harness.py::test_hook_never_blocks` | passed | 0.28 s |
| `hardware_tests/test_harness.py::test_hook_skips_docs_only` | passed | 0.31 s |
| `hardware_tests/test_harness.py::test_hook_no_hardware` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_hook_skip_env` | passed | 0.07 s |
| `hardware_tests/test_harness.py::test_spikes_not_collected` | passed | 0.21 s |
