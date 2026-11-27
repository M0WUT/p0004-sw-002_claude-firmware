"""Helpers shared by the USB hardware tests."""

import os
import re
import time

import serial

from conftest import wait_for_path

CDC_GLOB = "/dev/serial/by-id/usb-TinyUSB_TinyUSB_Device_*-if00"
RAMDISK_GLOB = "/dev/disk/by-id/usb-TinyUSB_Mass_Storage_*-0:0"
PASSTHROUGH_GLOB = "/dev/disk/by-id/usb-GTE_USB_passthrough_*-0:1"

# How long to allow for the device to re-enumerate after a reset and be handed to the VM
ENUM_TIMEOUT = 60

PASS_RE = re.compile(
    r"MSC: pass (\d) read (\d+) KiB in (\d+) ms = (\d+) KiB/s "
    r"\(ok=(\d+) fail=(\d+) timeouts=(\d+)\) "
    r"VBUS fault asserts so far=(\d+)"
)


def open_cdc(timeout=ENUM_TIMEOUT) -> serial.Serial:
    port = wait_for_path(CDC_GLOB, timeout)
    end = time.time() + 10
    while True:
        try:
            s = serial.Serial(port, 115200, timeout=1)
            break
        except serial.SerialException:
            if time.time() > end:
                raise
            time.sleep(0.3)
    s.dtr = True
    time.sleep(0.2)
    s.reset_input_buffer()
    return s


def cdc_echo_round(s: serial.Serial, i: int) -> bool:
    payload = (f"{i:06d}:" + os.urandom(28).hex())[: 7 + (i % 53)].encode()
    s.write(payload)
    want = b"ECHO:" + payload
    got = b""
    end = time.time() + 2
    while len(got) < len(want) and time.time() < end:
        got += s.read(len(want) - len(got))
    return got == want


def host_passes(text: str):
    return [tuple(int(g) for g in m.groups()) for m in PASS_RE.finditer(text)]


def assert_host_passes_clean(text: str, n=3, min_kib_s=900):
    passes = host_passes(text)
    assert len(passes) == n, f"expected {n} host read passes, got {passes}"
    for p, kib, ms, rate, ok, fail, tmo, faults in passes:
        assert fail == 0 and tmo == 0, f"pass {p}: fail={fail} timeouts={tmo}"
        assert rate >= min_kib_s, f"pass {p}: only {rate} KiB/s"
        assert faults == 0, f"pass {p}: VBUS fault asserted {faults} times"
    assert "FAULT ASSERTED" not in text, "host VBUS fault (PD15) asserted"
