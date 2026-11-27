"""Shared fixtures for GTE BRING-UP spikes (never part of the hardware_tests suite) (run
against a real LCD Board on an STLINK-V3).

Run everything:   hardware_tests/.venv/bin/pytest bringup -v
Skip flashing:    ... --no-flash   (use what is already on the board)

These tests are meant to be reused as production tests, so each one flashes its own
firmware,
resets the board and asserts on observable behaviour only.
"""

import glob
import os
import re
import subprocess
import threading
import time
from pathlib import Path

import pytest
import serial

HERE = Path(__file__).resolve().parent
PIO = Path.home() / ".platformio/penv/bin/pio"
OPENOCD_DIR = Path.home() / ".platformio/packages/tool-openocd"
STLINK_VCP_GLOB = "/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*-if02"


def pytest_addoption(parser):
    parser.addoption(
        "--no-flash", action="store_true", help="do not flash firmware before each test"
    )


def openocd(commands: str, timeout: float = 60) -> str:
    """Run an OpenOCD command string against the STLINK-V3 + STM32H7 and return its
    output."""
    r = subprocess.run(
        [
            str(OPENOCD_DIR / "bin/openocd"),
            "-s",
            str(OPENOCD_DIR / "openocd/scripts"),
            "-f",
            "interface/stlink.cfg",
            "-f",
            "target/stm32h7x.cfg",
            "-c",
            commands,
        ],
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    out = r.stdout + r.stderr
    if "Error: open failed" in out:
        pytest.fail("STLINK-V3 not found - is it attached to the VM?")
    return out


def reset_board():
    openocd("init; reset run; shutdown")


def wait_for_path(pattern: str, timeout: float) -> str:
    end = time.time() + timeout
    while time.time() < end:
        hits = glob.glob(pattern)
        if hits:
            return hits[0]
        time.sleep(0.2)
    pytest.fail(f"{pattern} did not appear within {timeout} s")


class UartCapture:
    """Background reader for the debug UART (STLINK-V3 VCP)."""

    def __init__(self, port: str, baud: int = 115200):
        self._s = serial.Serial(port, baud, timeout=0.1)
        self._buf = bytearray()
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
            if d:
                with self._lock:
                    self._buf += d

    @property
    def text(self) -> str:
        with self._lock:
            return self._buf.decode("ascii", "replace")

    def clear(self):
        with self._lock:
            self._buf.clear()

    def write(self, data: bytes):
        self._s.write(data)

    def wait_for(self, regex: str, timeout: float) -> re.Match:
        end = time.time() + timeout
        r = re.compile(regex, re.M)
        while time.time() < end:
            m = r.search(self.text)
            if m:
                return m
            time.sleep(0.1)
        pytest.fail(
            f"UART: /{regex}/ not seen within {timeout} s. "
            f"Last output:\n{self.text[-2000:]}"
        )

    def close(self):
        self._stop = True
        self._t.join(1)
        self._s.close()

    def save(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(self.text)


@pytest.fixture
def flash(request):
    """flash(project_dir, env=None): build and flash a PlatformIO project (skipped with
    --no-flash)."""

    def _flash(project: Path, env: str | None = None):
        if request.config.getoption("--no-flash"):
            return
        key = (str(project), env)
        if _last_flashed.get("key") == key:
            return  # already on the board from an earlier test in this session
        cmd = [str(PIO), "run", "-d", str(project), "-t", "upload"] + (
            ["-e", env] if env else []
        )
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        assert (
            r.returncode == 0 and "Verified OK" in r.stdout + r.stderr
        ), f"flash failed:\n{(r.stdout + r.stderr)[-3000:]}"
        time.sleep(1)  # the STLINK-V3 VCP can re-enumerate after programming
        _last_flashed["key"] = key

    return _flash


_last_flashed: dict = {}


@pytest.fixture
def open_uart(request):
    """open_uart() -> UartCapture on the debug UART. Call it after flashing (the VCP may
    re-enumerate)
    and before resetting the board. The capture is saved to logs/<test>.uart.log on
    teardown.
    """
    caps = []

    def _open():
        port = wait_for_path(STLINK_VCP_GLOB, 15)
        cap = UartCapture(port)
        caps.append(cap)
        return cap

    yield _open
    for cap in caps:
        cap.close()
        cap.save(HERE / "logs" / f"{request.node.name}.uart.log")


def ram_log(elf: Path) -> str:
    """Read the firmware's SWD RAM log ring (spikeLogRing / spikeLogHead)."""
    nm = (
        Path.home()
        / ".platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm"
    )
    syms = {}
    for line in subprocess.run(
        [str(nm), str(elf)], capture_output=True, text=True
    ).stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms[parts[2]] = int(parts[0], 16)
    size = 16384
    out = openocd(
        f'init; halt; echo "HEAD=[read_memory {syms["spikeLogHead"]:#x} 32 1]"; '
        f'set d [read_memory {syms["spikeLogRing"]:#x} 32 {size // 4}]; '
        'echo "RING=$d"; resume; shutdown'
    )
    head_m = re.search(r"HEAD=(\S+)", out)
    ring_m = re.search(r"RING=(.*)", out)
    assert head_m and ring_m, f"RAM log not read over SWD:\n{out[-1000:]}"
    head = int(head_m.group(1), 0)
    words = [int(w, 0) for w in ring_m.group(1).split()]
    ring = b"".join(w.to_bytes(4, "little") for w in words)
    tail = head % size
    data = ring[:head] if head <= size else ring[tail:] + ring[:tail]
    return data.decode("ascii", "replace")


def mountpoint_of(dev: str, timeout: float = 20) -> str:
    """Wait for udisks to auto-mount a block device and return its mount point."""
    dev = os.path.realpath(dev)
    end = time.time() + timeout
    while time.time() < end:
        mp = subprocess.run(
            ["lsblk", "-no", "MOUNTPOINT", dev], capture_output=True, text=True
        ).stdout.strip()
        if mp:
            return mp.splitlines()[0]
        subprocess.run(
            ["udisksctl", "mount", "-b", dev], capture_output=True, text=True
        )
        time.sleep(0.5)
    pytest.fail(f"{dev} was not mounted within {timeout} s")


def remount(dev: str) -> str:
    """Unmount then mount via udisks, dropping the page cache for that filesystem."""
    dev = os.path.realpath(dev)
    subprocess.run(["udisksctl", "unmount", "-b", dev], capture_output=True, text=True)
    return mountpoint_of(dev)
