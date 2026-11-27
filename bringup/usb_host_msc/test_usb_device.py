"""USB device on OTG_HS internal FS PHY (PB14/PB15 via the IO Board connector), running alongside the host port.

Needs: a USB cable from IO Board connector D+/D-/GND to this PC (passed through to the VM if virtualised),
and a USB memory stick in the USB-A socket (read only).
"""
import os
import threading
import time
from pathlib import Path

from conftest import reset_board, mountpoint_of, remount, wait_for_path
from usb_common import (RAMDISK_GLOB, ENUM_TIMEOUT, open_cdc, cdc_echo_round, assert_host_passes_clean)

PROJECT = Path(__file__).parent


def _boot_stage2(flash, open_uart):
    flash(PROJECT, 'stage2')
    uart = open_uart()
    reset_board()
    uart.wait_for(r'DEVICE: configured by the PC', ENUM_TIMEOUT)
    return uart


def test_cdc_echo(flash, open_uart):
    _boot_stage2(flash, open_uart)
    s = open_cdc()
    bad = [i for i in range(500) if not cdc_echo_round(s, i)]
    assert not bad, f'{len(bad)} of 500 CDC echoes wrong, first at {bad[:5]}'


def test_ramdisk_write_survives_remount(flash, open_uart):
    _boot_stage2(flash, open_uart)
    dev = wait_for_path(RAMDISK_GLOB, ENUM_TIMEOUT)
    mp = mountpoint_of(dev)
    data = os.urandom(1024)
    Path(mp, 'GTETEST.BIN').write_bytes(data)
    os.sync()
    mp = remount(dev)
    assert Path(mp, 'GTETEST.BIN').read_bytes() == data
    assert 'tinyusb' in Path(mp, 'README.TXT').read_text().lower()


def test_concurrent_load(flash, open_uart):
    """Host stick reads (3 x 64 MiB) while the PC keeps the device's CDC port and RAM disk busy."""
    uart = _boot_stage2(flash, open_uart)
    dev = wait_for_path(RAMDISK_GLOB, ENUM_TIMEOUT)
    stats = {'cdc_ok': 0, 'cdc_bad': 0, 'disk_ok': 0, 'disk_bad': 0}
    done = threading.Event()

    def cdc_load():
        s = open_cdc()
        i = 0
        while not done.is_set():
            stats['cdc_ok' if cdc_echo_round(s, i) else 'cdc_bad'] += 1
            i += 1

    def disk_load():
        mp = mountpoint_of(dev)
        i = 0
        while not done.is_set():
            data = os.urandom(2048)
            Path(mp, 'LOAD.BIN').write_bytes(data)
            os.sync()
            mp = remount(dev)  # forces the read to come from the device, not the page cache
            stats['disk_ok' if Path(mp, 'LOAD.BIN').read_bytes() == data else 'disk_bad'] += 1
            i += 1

    threads = [threading.Thread(target=cdc_load, daemon=True), threading.Thread(target=disk_load, daemon=True)]
    for t in threads:
        t.start()
    try:
        uart.wait_for(r'MSC: test complete', 240)
    finally:
        done.set()
        for t in threads:
            t.join(15)
    print('concurrent load stats:', stats)
    assert_host_passes_clean(uart.text)
    assert stats['cdc_bad'] == 0 and stats['cdc_ok'] > 1000, stats
    assert stats['disk_bad'] == 0 and stats['disk_ok'] > 10, stats
