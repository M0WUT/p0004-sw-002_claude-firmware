"""Shared fixtures for the GTE Core hardware test suite.

Every test runs the real firmware on a real LCD Board through an STLINK-V3:
  build (PlatformIO) -> flash and reset (OpenOCD over SWD) -> observe (debug UART via the STLINK VCP).

Run:          hardware_tests/.venv/bin/pytest hardware_tests -v
Skip flash:   ... --no-flash   (test whatever is already on the board)

Bring-up spikes live in bringup/ and are never part of this suite.
"""
import glob
import re
import subprocess
import threading
import time
from dataclasses import dataclass
from pathlib import Path

import pytest
import serial

pytest_plugins = ['gte_report']


def meta() -> dict:
    """The report plugin's metadata (imported lazily so pytest can load it as a plugin first)."""
    import gte_report
    return gte_report.META

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
ENV = 'p0004-pcb-001_v1'
PIO = Path.home() / '.platformio/penv/bin/pio'
OPENOCD_DIR = Path.home() / '.platformio/packages/tool-openocd'
STLINK_VCP_GLOB = '/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*-if02'
STM32H7_UID_ADDR = 0x1FF1E800


def pytest_addoption(parser):
    parser.addoption('--no-flash', action='store_true', help='do not flash; test what is already on the board')


def pytest_configure(config):
    config.addinivalue_line('markers', 'hardware: needs the LCD Board on the STLINK-V3')
    meta().setdefault('io_board', 'not fitted')
    meta().setdefault('instrument_board', 'not fitted')


def stlink_present() -> bool:
    return bool(glob.glob(STLINK_VCP_GLOB))


def openocd(commands: str, timeout: float = 60) -> str:
    r = subprocess.run([str(OPENOCD_DIR / 'bin/openocd'), '-s', str(OPENOCD_DIR / 'openocd/scripts'),
                        '-f', 'interface/stlink.cfg', '-f', 'target/stm32h7x.cfg', '-c', commands],
                       capture_output=True, text=True, timeout=timeout)
    out = r.stdout + r.stderr
    if 'Error: open failed' in out:
        pytest.fail('STLINK-V3 not found - is it attached?')
    return out


def reset_board():
    openocd('init; reset run; shutdown')


def read_uid() -> str:
    out = openocd(f'init; echo "UID=[read_memory {STM32H7_UID_ADDR:#x} 32 3]"; shutdown')
    m = re.search(r'UID=(\S+) (\S+) (\S+)', out)
    # Bytes in memory order, the same string TinyUSB uses as the USB serial number.
    return ''.join(int(w, 0).to_bytes(4, 'little').hex().upper() for w in m.groups()) if m else 'unknown'


@dataclass
class Firmware:
    env: str
    elf: Path
    version: str
    flash_used: int
    flash_total: int
    ram_used: int
    ram_total: int


def build(env: str) -> Firmware:
    r = subprocess.run([str(PIO), 'run', '-d', str(REPO), '-e', env], capture_output=True, text=True, timeout=900)
    out = r.stdout + r.stderr
    assert r.returncode == 0, f'build of {env} failed:\n{out[-4000:]}'
    ram = re.search(r'RAM:.*used (\d+) bytes from (\d+) bytes', out)
    flash = re.search(r'Flash:.*used (\d+) bytes from (\d+) bytes', out)
    bdir = REPO / '.pio/build' / env
    ver = re.search(r'#define VERSION_STRING "([^"]+)"', (bdir / 'generated/version.h').read_text()).group(1)
    return Firmware(env, bdir / 'firmware.elf', ver, int(flash.group(1)), int(flash.group(2)),
                    int(ram.group(1)), int(ram.group(2)))


@pytest.fixture(scope='session')
def firmware() -> Firmware:
    """The Core firmware for this LCD Board, built once per session."""
    fw = build(ENV)
    meta()['firmware_version'] = fw.version
    meta().setdefault('extra', {})['Flash / RAM used'] = \
        f'{fw.flash_used:,} / {fw.flash_total:,} B, {fw.ram_used:,} / {fw.ram_total:,} B'
    return fw


@pytest.fixture(scope='session')
def flashed(request, firmware) -> Firmware:
    """firmware, programmed onto the board once per session, and the board serial recorded."""
    if not stlink_present():
        pytest.fail('STLINK-V3 not found - is the LCD Board attached?')
    if not request.config.getoption('--no-flash'):
        out = openocd(f'program {firmware.elf} verify reset exit', timeout=180)
        assert 'Verified OK' in out, f'flash failed:\n{out[-3000:]}'
        time.sleep(1)  # the STLINK-V3 VCP can re-enumerate after programming
    meta()['board_serial'] = read_uid()
    return firmware


class UartCapture:
    """Background reader for the debug UART. Keeps every complete line with its host arrival time."""

    def __init__(self, port: str, baud: int = 115200):
        self._s = serial.Serial(port, baud, timeout=0.05)
        self._raw = bytearray()
        self._partial = b''
        self.lines: list[tuple[float, str]] = []
        self._lock = threading.Lock()
        self._stop = False
        self._t = threading.Thread(target=self._run, daemon=True)
        self._t.start()

    def _run(self):
        while not self._stop:
            try:
                d = self._s.read(4096)
            except serial.SerialException:
                break
            if not d:
                continue
            now = time.time()
            with self._lock:
                self._raw += d
                parts = (self._partial + d).split(b'\n')
                self._partial = parts.pop()
                for p in parts:
                    self.lines.append((now, p.rstrip(b'\r').decode('ascii', 'replace')))

    @property
    def text(self) -> str:
        with self._lock:
            return self._raw.decode('ascii', 'replace').replace('\r\n', '\n')

    def clear(self):
        with self._lock:
            self._raw.clear()
            self.lines.clear()
            self._partial = b''

    def wait_for(self, regex: str, timeout: float) -> re.Match:
        end = time.time() + timeout
        r = re.compile(regex, re.M)
        while time.time() < end:
            m = r.search(self.text)
            if m:
                return m
            time.sleep(0.05)
        pytest.fail(f'UART: /{regex}/ not seen within {timeout} s. Last output:\n{self.text[-2000:]}')

    def close(self):
        self._stop = True
        self._t.join(1)
        self._s.close()


@pytest.fixture
def uart(request):
    """The debug UART (STLINK VCP). Saved to logs/<test>.uart.log afterwards."""
    end = time.time() + 15
    while not stlink_present() and time.time() < end:
        time.sleep(0.2)
    ports = glob.glob(STLINK_VCP_GLOB)
    if not ports:
        pytest.fail('STLINK-V3 VCP not found')
    cap = UartCapture(ports[0])
    yield cap
    cap.close()
    log = HERE / 'logs' / f'{request.node.name}.uart.log'
    log.parent.mkdir(exist_ok=True)
    log.write_text(cap.text)


BOOT_RE = r'^GTE boot env=(\S+) version=(\S+)'


@pytest.fixture
def booted(flashed, uart):
    """Reset the board and wait for its boot line. Returns the UART capture."""
    uart.clear()
    reset_board()
    uart.wait_for(BOOT_RE, 5)
    return uart
