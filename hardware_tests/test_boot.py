"""Boot, clocks and scheduler on the LCD Board (Build: skeleton and test harness, issue #41)."""
import configparser
import re
import subprocess
import time

import pytest

from conftest import BOOT_RE, ENV, REPO, reset_board

pytestmark = pytest.mark.hardware

BOARD = 'p0004_pcb_001_v1'  # board id must be a C identifier; the env keeps the PCB name


def test_build_fits(firmware):
    """The firmware builds for this board's own definition and fits in flash and RAM."""
    assert firmware.flash_used < firmware.flash_total
    assert firmware.ram_used < firmware.ram_total
    assert firmware.flash_total == 2 * 1024 * 1024
    ini = configparser.ConfigParser()
    ini.read(REPO / 'platformio.ini')
    envs = [s for s in ini.sections() if s.startswith('env:')]
    assert f'env:{ENV}' in envs
    for s in envs:
        assert ini[s].get('board') == BOARD, f'[{s}] must use board = {BOARD}'
    # No third-party dev board anywhere in the build configuration.
    hits = subprocess.run(['git', '-C', str(REPO), 'grep', '-il', 'devebox', '--', 'platformio.ini', 'boards',
                           'variants', 'src', 'include', 'scripts'], capture_output=True, text=True).stdout
    assert hits == '', f'third-party board referenced in: {hits}'


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
    """The 25 MHz crystal drives PLL1: SYSCLK 480 MHz at VOS0, USB kernel clock 48 MHz from PLL1Q."""
    m = booted.wait_for(r'^GTE clocks (.*)\n', 2)  # \n: only a complete line
    kv = dict(p.split('=', 1) for p in m.group(1).split())
    assert kv['pllsrc'] == 'HSE', 'PLL is not running from the crystal'
    assert kv['hserdy'] == '1', 'HSE crystal not ready'
    assert kv['sws'] == 'PLL1', 'SYSCLK is not PLL1P'
    assert int(kv['sysclk']) == 480_000_000
    assert kv['vos'] == '0'
    assert kv['usbsel'] == 'PLL1Q'
    assert int(kv['usbclk']) == 48_000_000


def test_scheduler_timing(booted):
    """A FreeRTOS task ticks every 100 ms; over 10 s it matches the host clock within 0.5 %."""
    booted.wait_for(r'^GTE tick 1$', 2)
    time.sleep(10.5)
    lines = list(booted.lines)
    boot_idx = max(i for i, (_, line) in enumerate(lines) if re.match(BOOT_RE, line))
    ticks = [(t, int(m.group(1))) for t, line in lines[boot_idx:]
             if (m := re.match(r'GTE tick (\d+)$', line))]
    assert len(ticks) >= 100, f'only {len(ticks)} ticks in 10.5 s'
    ns = [n for _, n in ticks]
    assert ns == list(range(ns[0], ns[0] + len(ns))), 'tick numbers are not consecutive'
    # Least-squares slope of host time against tick number = seconds per tick.
    xs = ns
    ys = [t for t, _ in ticks]
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    slope = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    assert abs(slope - 0.1) / 0.1 < 0.005, f'tick period {slope * 1000:.2f} ms, expected 100 ms ±0.5 %'


def test_reset_reboots(flashed, uart):
    """Two SWD resets in a row each give a fresh boot, with the tick count restarting from 0."""
    for _ in range(2):
        uart.clear()
        reset_board()
        uart.wait_for(BOOT_RE, 5)
        uart.wait_for(r'^GTE tick 0$', 2)
        lines = [line for _, line in uart.lines]
        boot_idx = max(i for i, line in enumerate(lines) if re.match(BOOT_RE, line))
        first_tick = next(line for line in lines[boot_idx:] if line.startswith('GTE tick '))
        assert first_tick == 'GTE tick 0', f'first tick after the boot line was "{first_tick}"'
