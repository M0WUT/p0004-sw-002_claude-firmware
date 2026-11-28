# P0004-SW-002 - Claude Firmware
Experimental writing of the firmware using Claude Code

## Planning and issues

Work is planned with a wayfinder map in GitHub Issues: [#1 GTE Core firmware](https://github.com/M0WUT/p0004-sw-002_claude-firmware/issues/1). See [docs/issue-tracker.md](docs/issue-tracker.md) for how tickets, blocking and the frontier work. Domain terms are in [GLOSSARY.md](GLOSSARY.md).

## Building

PlatformIO, one environment per LCD Board, named after the PCB: `p0004-pcb-001_v1`. The board definition is in [boards/](boards/) and [variants/](variants/) (STM32H743VIT6, 25 MHz HSE, 480 MHz core).

```sh
pio run -e p0004-pcb-001_v1 -t upload
```

## Testing

All testing is pytest, against the real firmware on a real LCD Board connected through an STLINK-V3 ([hardware_tests/](hardware_tests/)). One-time setup per clone:

```sh
python3 -m venv hardware_tests/.venv
hardware_tests/.venv/bin/pip install -r hardware_tests/requirements.txt
git config core.hooksPath .githooks
```

**Every commit runs the suite** through [.githooks/pre-commit](.githooks/pre-commit) and commits the results as [hardware_tests/REPORT.md](hardware_tests/REPORT.md) (plus `report.xml`). A failing test never blocks a commit: **read the report before pushing**. The hook runs nothing for docs-only commits, records "not run" when no STLINK-V3 is attached, and can be skipped with `GTE_SKIP_TESTS=1 git commit ...`. The latest report is also published with the SCPI manual.

Run the suite by hand with `flock -w 900 /tmp/gte-bench.lock hardware_tests/.venv/bin/pytest hardware_tests -v`. The lock keeps two checkouts from using the bench at once; the hook takes it too. Bring-up spikes live in [bringup/](bringup/) and are never part of the suite.

From the SCPI build on, tests drive the firmware over SCPI on the LCD Board's USB device port, so the bench also needs a USB cable from the IO Board connector (D+/D-/GND) to this PC. In a VM, add a USB filter for `CAFE:4004`.

## Code style

Enforced by `hardware_tests/test_harness.py` on every commit (see Testing):

- **Python:** formatted with [black](https://black.readthedocs.io/), clean [flake8](https://flake8.pycqa.org/) (defaults, `max-line-length = 88`, see `.flake8`), and clean [pyright](https://github.com/microsoft/pyright) in `standard` mode, as Pylance's "Standard" type checking (`pyrightconfig.json`).
- **C++:** the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html): formatted with clang-format (`.clang-format`, Google style) and a clean [cpplint](https://github.com/cpplint/cpplint) run. Third-party code (STM32duino's generic variant files, the FreeRTOS config copy, vendored TinyUSB in `lib/tinyusb`, and the Vrekrer SCPI parser fetched by PlatformIO) is left as delivered.
- **Exception:** anything inside a `/** @scpi ... */` block is exempt from linting, because the SCPI manual generator reads it as written.

The tools are pinned in `hardware_tests/requirements.txt`.

## SCPI manual

**[SCPI command manual (PDF)](https://m0wut.github.io/p0004-sw-002_claude-firmware/scpi-manual.pdf)** ([HTML](https://m0wut.github.io/p0004-sw-002_claude-firmware/)) · [latest hardware test report](https://m0wut.github.io/p0004-sw-002_claude-firmware/report.html)

It's generated from the `/** @scpi ... */` comments next to each command's registration (Core's are in `lib/services/src/gte/scpi_core_commands.cc`) by [p0004-sw-003_scpi-manual-generator](https://github.com/M0WUT/p0004-sw-003_scpi-manual-generator) on every push to `main` (see [.github/workflows/scpi-manual.yml](.github/workflows/scpi-manual.yml)). The comment format is described in that repo's README.
