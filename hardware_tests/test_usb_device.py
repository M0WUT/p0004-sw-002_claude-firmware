"""The USB_DEVICE Service (Build: USB Device CDC and SCPI, issue #44): TinyUSB CDC on
the LCD Board's device port, with the identity from Release USB identity (issue #38).
"""

import subprocess
import time
from pathlib import Path

import pytest

from conftest import (
    CORE_PRODUCT,
    ENV,
    REPO,
    TEST_PRODUCT,
    TEST_RAM_ENV,
    TEST_VERSION,
    USB_PID,
    USB_VID,
    board_uid,
    build_cached,
    open_scpi,
    put_on_board,
    reset_board,
    wait_gone,
    cdc_path,
)

pytestmark = pytest.mark.hardware


def core_bcd() -> int:
    """bcdDevice expected from the latest tag: major and minor as two BCD digits
    each (v1.2 is 0x0102), 0x0000 with no tag."""
    r = subprocess.run(
        ["git", "-C", str(REPO), "describe", "--tags", "--abbrev=0"],
        capture_output=True,
        text=True,
    )
    if r.returncode != 0:
        return 0
    parts = [int(p) for p in r.stdout.strip().lstrip("v").split(".")[:2]]
    major, minor = (parts + [0, 0])[:2]
    return int(f"{major:02d}{minor:02d}", 16)


def sysfs_device(serial: str) -> Path:
    for dev in Path("/sys/bus/usb/devices").iterdir():
        f = dev / "serial"
        if f.exists() and f.read_text().strip() == serial:
            return dev
    pytest.fail(f"no USB device with serial {serial} in sysfs")


def read(dev: Path, name: str) -> str:
    return (dev / name).read_text().strip()


def check_identity(product: str):
    dev = sysfs_device(board_uid())
    assert int(read(dev, "idVendor"), 16) == USB_VID
    assert int(read(dev, "idProduct"), 16) == USB_PID
    assert read(dev, "manufacturer") == "M0WUT"
    assert read(dev, "product") == product
    assert int(read(dev, "bcdDevice"), 16) == core_bcd()
    classes = sorted(
        (read(i, "bInterfaceClass"), read(i, "bInterfaceSubClass"))
        for i in dev.glob(f"{dev.name}:*")
    )
    assert classes == [("02", "02"), ("0a", "00")], f"not one CDC ACM: {classes}"


def boot_core(request, uart):
    put_on_board(request, build_cached(ENV))
    uart.clear()
    reset_board()
    wait_gone(cdc_path(CORE_PRODUCT, board_uid()))
    return open_scpi(uart, CORE_PRODUCT)


def test_enumerates_core(request, uart):
    """Core firmware: CAFE:4004, M0WUT, "GTE Core", serial = the UID read over SWD,
    bcdDevice = Core version, one CDC ACM interface; within the timeout of a reset."""
    s = boot_core(request, uart)
    s.close()
    check_identity(CORE_PRODUCT)


def test_enumerates_application(scpi):
    """The test Application: Product = its Instrument name; the rest as for Core."""
    check_identity(TEST_PRODUCT)


def test_reenumerates_after_reset(request, uart):
    """Three SWD resets in a row: each time the CDC port comes back and answers."""
    put_on_board(request, build_cached(TEST_RAM_ENV))
    for i in range(3):
        uart.clear()
        reset_board()
        wait_gone(cdc_path(TEST_PRODUCT, board_uid()))
        s = open_scpi(uart, TEST_PRODUCT)
        try:
            assert s.query("*IDN?").startswith("M0WUT,"), f"reset {i + 1}"
        finally:
            s.close()


def test_cdc_soak(scpi):
    """2,000 back-to-back queries: every reply complete and correct."""
    idn = f"M0WUT,{TEST_PRODUCT},{board_uid()},{TEST_VERSION}"
    bad = []
    t0 = time.time()
    for i in range(2000):
        q, want = ("*IDN?", idn) if i % 2 else ("SYST:ERR:COUN?", "0")
        reply = scpi.query(q)
        if reply != want:
            bad.append((i, reply))
    assert not bad, f"{len(bad)} bad replies, first {bad[:3]}"
    print(f"2000 queries in {time.time() - t0:.1f} s")
