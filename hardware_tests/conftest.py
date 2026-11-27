"""Shared fixtures for the GTE Core hardware test suite.

Every test runs the real firmware on a real LCD Board through an STLINK-V3:
  build (PlatformIO) -> flash and reset (OpenOCD over SWD) -> observe (debug UART via
  the STLINK VCP).

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

pytest_plugins = ["gte_report"]


def meta() -> dict:
    """The report plugin's metadata (imported lazily so pytest can load it as a plugin
    first)."""
    import gte_report

    return gte_report.META


HERE = Path(__file__).resolve().parent
REPO = HERE.parent
ENV = "p0004-pcb-001_v1"
PIO = Path.home() / ".platformio/penv/bin/pio"
OPENOCD_DIR = Path.home() / ".platformio/packages/tool-openocd"
STLINK_VCP_GLOB = "/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*-if02"
STM32H7_UID_ADDR = 0x1FF1E800


def pytest_addoption(parser):
    parser.addoption(
        "--manual",
        action="store_true",
        help="also run tests that need a person (e.g. pressing the Soft Keys)",
    )
    parser.addoption(
        "--no-flash",
        action="store_true",
        help="do not flash; test what is already on the board",
    )


def pytest_configure(config):
    config.addinivalue_line("markers", "hardware: needs the LCD Board on the STLINK-V3")
    config.addinivalue_line("markers", "manual: needs a person; run with --manual")
    meta().setdefault("io_board", "not fitted")
    meta().setdefault("instrument_board", "not fitted")


def pytest_collection_modifyitems(config, items):
    if config.getoption("--manual"):
        return
    skip = pytest.mark.skip(reason="needs a person: run with --manual")
    for item in items:
        if "manual" in item.keywords:
            item.add_marker(skip)


def stlink_present() -> bool:
    return bool(glob.glob(STLINK_VCP_GLOB))


def openocd(commands: str, timeout: float = 60) -> str:
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
        pytest.fail("STLINK-V3 not found - is it attached?")
    return out


def reset_board():
    openocd("init; reset run; shutdown")


def read_uid() -> str:
    out = openocd(
        f'init; echo "UID=[read_memory {STM32H7_UID_ADDR:#x} 32 3]"; shutdown'
    )
    m = re.search(r"UID=(\S+) (\S+) (\S+)", out)
    # Bytes in memory order, the same string TinyUSB uses as the USB serial number.
    return (
        "".join(int(w, 0).to_bytes(4, "little").hex().upper() for w in m.groups())
        if m
        else "unknown"
    )


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
    r = subprocess.run(
        [str(PIO), "run", "-d", str(REPO), "-e", env],
        capture_output=True,
        text=True,
        timeout=900,
    )
    out = r.stdout + r.stderr
    assert r.returncode == 0, f"build of {env} failed:\n{out[-4000:]}"
    ram = re.search(r"RAM:.*used (\d+) bytes from (\d+) bytes", out)
    flash = re.search(r"Flash:.*used (\d+) bytes from (\d+) bytes", out)
    assert ram and flash, f"no size summary in the {env} build output"
    bdir = REPO / ".pio/build" / env
    ver = re.search(
        r'#define VERSION_STRING "([^"]+)"', (bdir / "generated/version.h").read_text()
    )
    assert ver, f"no VERSION_STRING in {env}'s generated/version.h"
    return Firmware(
        env,
        bdir / "firmware.elf",
        ver.group(1),
        int(flash.group(1)),
        int(flash.group(2)),
        int(ram.group(1)),
        int(ram.group(2)),
    )


_built: dict = {}
_on_board: dict = {}


def build_cached(env: str) -> Firmware:
    if env not in _built:
        _built[env] = build(env)
    return _built[env]


def put_on_board(request, fw: Firmware) -> Firmware:
    """Program fw unless it's already the image on the board (or --no-flash); record the
    board serial."""
    if not stlink_present():
        pytest.fail("STLINK-V3 not found - is the LCD Board attached?")
    if not request.config.getoption("--no-flash") and _on_board.get("env") != fw.env:
        out = openocd(f"program {fw.elf} verify reset exit", timeout=180)
        assert "Verified OK" in out, f"flash failed:\n{out[-3000:]}"
        time.sleep(1)  # the STLINK-V3 VCP can re-enumerate after programming
        _on_board["env"] = fw.env
    if "board_serial" not in meta():
        meta()["board_serial"] = read_uid()
    return fw


@pytest.fixture(scope="session")
def firmware() -> Firmware:
    """The Core firmware for this LCD Board (no Application), built once per session."""
    fw = build_cached(ENV)
    meta()["firmware_version"] = fw.version
    used = (
        f"{fw.flash_used:,} / {fw.flash_total:,} B, "
        f"{fw.ram_used:,} / {fw.ram_total:,} B"
    )
    meta().setdefault("extra", {})["Flash / RAM used"] = used
    return fw


@pytest.fixture
def flashed(request, firmware) -> Firmware:
    """The Core firmware, on the board."""
    return put_on_board(request, firmware)


TEST_ENV = ENV + "-test"  # real EEPROM
TEST_RAM_ENV = ENV + "-test-ram"  # DEBUG_EEPROM: config in RAM, lost at reset
TEST_V2_ENV = ENV + "-test-v2"
TEST_V3GAP_ENV = ENV + "-test-v3gap"
KEYS_OVERRIDE_ADDR = 0x2001FFF4
CONFIG_ACTION_ADDR = 0x2001FFF8
CONFIG_ACTIONS = {"fresh": 1, "blank": 2, "corrupt_table": 3, "corrupt_aux": 4}
CONFIG_IMAGE_SIZE = 0x6000  # config area 0x0000-0x5FBF, record page 0x5FC0
PROMPT_RE = r"Hold Soft Key 4 for 3 s"
SCENARIO_ADDR = 0x2001FFF0
SCENARIOS = {
    "normal": 0,
    "warnings": 1,
    "nine_warnings": 2,
    "fatal": 3,
    "hang": 4,
    "stack_overflow": 5,
    "log_levels": 6,
    "set_values": 7,
    "eeprom_full": 8,
    "no_app": 9,
    "print_status": 20,  # every Service's Status Values (#55)
}


def keys_value(mask: int) -> int:
    return (0x4B45 << 16) | mask if mask else 0


def hold_keys(mask: int):
    """Simulate Soft Keys held down (bit 0 = Soft Key 1); 0 releases them."""
    openocd(f"init; mww {KEYS_OVERRIDE_ADDR:#x} {keys_value(mask):#x}; shutdown")


def symbol_address(elf: Path, name: str) -> int:
    nm = Path.home() / ".platformio/packages/toolchain-gccarmnoneeabi/bin"
    out = subprocess.run(
        [str(nm / "arm-none-eabi-nm"), str(elf)], capture_output=True, text=True
    ).stdout
    m = re.search(rf"^([0-9a-f]+) \w {name}$", out, re.M)
    assert m, f"{name} not in {elf}"
    return int(m.group(1), 16)


def read_config_image(fw: "Firmware") -> bytes:
    """The firmware's RAM copy of the config area (loaded from the EEPROM at boot,
    and kept in step with it)."""
    addr = symbol_address(fw.elf, "gte_config_image")
    path = Path(HERE / "logs" / "config_image.bin")
    path.parent.mkdir(exist_ok=True)
    openocd(f"init; dump_image {path} {addr:#x} {CONFIG_IMAGE_SIZE}; shutdown")
    return path.read_bytes()


def reset_with(
    scenario: int | None = None, keys: int = 0, config_action: str | None = None
) -> None:
    """Reset with the core held while the scenario word, the Soft Key override
    and a config action (see the test Application) are set up over SWD."""
    cmds = ["init", "reset halt"]
    if scenario is not None:
        cmds.append(f"mww {SCENARIO_ADDR:#x} {(0x6765 << 16) | scenario:#x}")
    cmds.append(f"mww {KEYS_OVERRIDE_ADDR:#x} {keys_value(keys):#x}")
    if config_action:
        value = (0x4346 << 16) | CONFIG_ACTIONS[config_action]
        cmds.append(f"mww {CONFIG_ACTION_ADDR:#x} {value:#x}")
    cmds += ["resume", "shutdown"]
    openocd("; ".join(cmds))


def answer_blank_prompt(uart, timeout: float = 3) -> bool:
    """If the firmware stops at the blank/corrupt config prompt, hold Soft Key 4
    until it re-initialises. True if a prompt was answered."""
    end = time.time() + timeout
    while time.time() < end:
        if re.search(PROMPT_RE, uart.text):
            hold_keys(1 << 3)
            uart.wait_for(r"Soft Key 4 held for 3 s: re-initialising", 6)
            hold_keys(0)
            return True
        if re.search(r"starting the scheduler", uart.text):
            return False
        time.sleep(0.1)
    return False


@pytest.fixture
def app_firmware(request) -> Firmware:
    """The test Application (hardware_tests/apps/test), on the board."""
    return put_on_board(request, build_cached(TEST_ENV))


@pytest.fixture
def run_scenario(request, uart):
    """run_scenario(name, env=TEST_ENV, config="fresh") -> UartCapture.

    Puts the test Application (in env) on the board, then resets it into a
    scenario and waits for boot. The scenario word is written over SWD while the
    core is held in reset; the test Application reads and clears it in
    Initialise(). config is a config action applied before config loads
    ("fresh" = empty table), or None to keep what's stored."""

    def _run(name: str, env: str = TEST_ENV, config: str | None = "fresh"):
        put_on_board(request, build_cached(env))
        uart.clear()
        reset_with(SCENARIOS[name], config_action=config)
        uart.wait_for(BOOT_RE, 5)
        answer_blank_prompt(uart)
        if name != "no_app":
            uart.wait_for(rf"Test scenario {SCENARIOS[name]}\b", 5)
        uart.trim_before_last(BOOT_RE)
        return uart

    return _run


ANSI = re.compile(r"\x1b\[[0-9;]*m")


class UartCapture:
    """Background reader for the debug UART. Keeps every complete line with its host
    arrival time."""

    def __init__(self, port: str, baud: int = 115200):
        self._s = serial.Serial(port, baud, timeout=0.05)
        self._raw = bytearray()
        self._partial = b""
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
                parts = (self._partial + d).split(b"\n")
                self._partial = parts.pop()
                for p in parts:
                    self.lines.append(
                        (now, ANSI.sub("", p.rstrip(b"\r").decode("utf-8", "replace")))
                    )

    @property
    def text(self) -> str:
        with self._lock:
            return ANSI.sub(
                "", self._raw.decode("utf-8", "replace").replace("\r\n", "\n")
            )

    def clear(self):
        with self._lock:
            self._raw.clear()
            self.lines.clear()
            self._partial = b""

    def wait_for(self, regex: str, timeout: float) -> re.Match:
        end = time.time() + timeout
        r = re.compile(regex, re.M)
        while time.time() < end:
            m = r.search(self.text)
            if m:
                return m
            time.sleep(0.05)
        pytest.fail(
            f"UART: /{regex}/ not seen within {timeout} s. "
            f"Last output:\n{self.text[-2000:]}"
        )

    def trim_before_last(self, regex: str):
        """Drop everything before the last line matching regex (e.g. stale output from
        before a reset,
        still buffered in the STLINK VCP when the capture was cleared)."""
        r = re.compile(regex)
        with self._lock:
            idx = max(
                (i for i, (_, line) in enumerate(self.lines) if r.search(line)),
                default=None,
            )
            if idx is None:
                return
            keep = self.lines[idx:]
            self.lines[:] = keep
            self._raw = bytearray(
                "\n".join(line for _, line in keep).encode() + b"\n" + self._partial
            )

    def lines_matching(self, regex: str) -> list[tuple[float, re.Match]]:
        r = re.compile(regex)
        with self._lock:
            lines = list(self.lines)
        return [(t, m) for t, line in lines if (m := r.search(line))]

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
        pytest.fail("STLINK-V3 VCP not found")
    cap = UartCapture(ports[0])
    yield cap
    cap.close()
    log = HERE / "logs" / f"{request.node.name}.uart.log"
    log.parent.mkdir(exist_ok=True)
    log.write_text(cap.text)


BOOT_RE = r"^GTE boot env=(\S+) version=(\S+)"


@pytest.fixture
def booted(flashed, uart):
    """Reset the board and wait for its boot line. Returns the UART capture."""
    uart.clear()
    reset_board()
    uart.wait_for(BOOT_RE, 5)
    answer_blank_prompt(uart)
    return uart
