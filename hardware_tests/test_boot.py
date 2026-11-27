"""Boot and clocks on the LCD Board, running the Core firmware with no Application
(issue #41).

Scheduler timing is covered by test_framework.py::test_tick_periods."""

import configparser
import subprocess
import time

import pytest

from conftest import BOOT_RE, ENV, REPO, answer_blank_prompt, reset_board

pytestmark = pytest.mark.hardware

BOARD = (
    "p0004_pcb_001_v1"  # board id must be a C identifier; the env keeps the PCB name
)


def board_of(ini: configparser.ConfigParser, section: str) -> str | None:
    """The board an environment uses, following PlatformIO's "extends"."""
    while section in ini:
        if "board" in ini[section]:
            return ini[section]["board"]
        parent = ini[section].get("extends")
        if not parent:
            return None
        section = parent.strip()
    return None


def test_build_fits(firmware):
    """The firmware builds for this board's own definition and fits in flash and RAM."""
    assert firmware.flash_used < firmware.flash_total
    assert firmware.ram_used < firmware.ram_total
    assert firmware.flash_total == 2 * 1024 * 1024
    ini = configparser.ConfigParser()
    ini.read(REPO / "platformio.ini")
    envs = [s for s in ini.sections() if s.startswith("env:")]
    assert f"env:{ENV}" in envs
    for s in envs:
        assert board_of(ini, s) == BOARD, f"[{s}] must use board = {BOARD}"
    # No third-party dev board anywhere in the build configuration.
    hits = subprocess.run(
        [
            "git",
            "-C",
            str(REPO),
            "grep",
            "-il",
            "devebox",
            "--",
            "platformio.ini",
            "boards",
            "variants",
            "src",
            "include",
            "scripts",
        ],
        capture_output=True,
        text=True,
    ).stdout
    assert hits == "", f"third-party board referenced in: {hits}"


def test_boot_line(flashed, uart):
    """After an SWD reset the boot line appears within 5 s, naming this build."""
    uart.clear()
    t0 = time.time()
    reset_board()
    m = uart.wait_for(BOOT_RE, 5)
    assert time.time() - t0 < 5
    assert m.group(1) == ENV
    assert m.group(2) == flashed.version


def test_clocks(booted):
    """The 25 MHz crystal drives PLL1: SYSCLK 480 MHz at VOS0, USB kernel clock 48 MHz
    from PLL1Q."""
    m = booted.wait_for(r"^GTE clocks (.*)\n", 2)  # \n: only a complete line
    kv = dict(p.split("=", 1) for p in m.group(1).split())
    assert kv["pllsrc"] == "HSE", "PLL is not running from the crystal"
    assert kv["hserdy"] == "1", "HSE crystal not ready"
    assert kv["sws"] == "PLL1", "SYSCLK is not PLL1P"
    assert int(kv["sysclk"]) == 480_000_000
    assert kv["vos"] == "0"
    assert kv["usbsel"] == "PLL1Q"
    assert int(kv["usbclk"]) == 48_000_000


def test_reset_reboots(flashed, uart):
    """Two SWD resets in a row each give a fresh boot: boot line, reset reason, then
    CORE initialising."""
    for _ in range(2):
        uart.clear()
        reset_board()
        uart.wait_for(BOOT_RE, 5)
        uart.wait_for(r"^GTE reset reason=\S+$", 2)
        answer_blank_prompt(uart)
        uart.wait_for(r"\[CORE\]\s+\tINFO\t\tInitialising$", 3)
        uart.wait_for(r"starting the scheduler$", 3)
