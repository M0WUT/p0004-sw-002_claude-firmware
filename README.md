# P0004-SW-002 - Claude Firmware
Experimental writing of the firmware using Claude Code

## Planning and issues

Work is planned with a wayfinder map in GitHub Issues: [#1 GTE Core firmware](https://github.com/M0WUT/p0004-sw-002_claude-firmware/issues/1). See [docs/issue-tracker.md](docs/issue-tracker.md) for how tickets, blocking and the frontier work. Domain terms are in [GLOSSARY.md](GLOSSARY.md); hardware tests are in [hardware_tests/](hardware_tests/) (`hardware_tests/.venv/bin/pytest hardware_tests -v`).

## SCPI manual

**[SCPI command manual (PDF)](https://m0wut.github.io/p0004-sw-002_claude-firmware/scpi-manual.pdf)** ([HTML](https://m0wut.github.io/p0004-sw-002_claude-firmware/))

It's generated from the `/** @scpi ... */` comments in `src/` by [p0004-sw-003_scpi-manual-generator](https://github.com/M0WUT/p0004-sw-003_scpi-manual-generator) on every push to `main` (see [.github/workflows/scpi-manual.yml](.github/workflows/scpi-manual.yml)). The comment format is described in that repo's README.
