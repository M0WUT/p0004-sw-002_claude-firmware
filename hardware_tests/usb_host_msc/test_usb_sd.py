"""SD card presented to the Connected PC as USB drive 0 (raw blocks), next to the stick pass-through on drive 1.

Needs: the USB device cable to this PC (passed through to the VM if virtualised, VID:PID CAFE:4005) and a
FAT-formatted microSD card. NON-DESTRUCTIVE: only creates and deletes /GTETEST/ on the card.
"""
import os
import re
import shutil
import time
from pathlib import Path

from conftest import reset_board, wait_for_path, mountpoint_of, remount
from usb_common import ENUM_TIMEOUT

PROJECT = Path(__file__).parent
SD_DISK_GLOB = '/dev/disk/by-id/usb-GTE_SD_card_*-0:0'
SIZE = 4 * 1024 * 1024


def test_sd_card_as_usb_drive(flash, open_uart):
    flash(PROJECT, 'storage')
    uart = open_uart()
    reset_board()
    m = uart.wait_for(r'SDLUN: card ready in \d+ ms: type=\d+, (\d+) blocks x (\d+) B', 10)  # fails if no card
    blocks, bsize = int(m.group(1)), int(m.group(2))
    uart.wait_for(r'DEVICE: configured by the PC', ENUM_TIMEOUT)

    disk = os.path.realpath(wait_for_path(SD_DISK_GLOB, ENUM_TIMEOUT))
    name = Path(disk).name
    assert int(Path(f'/sys/block/{name}/size').read_text()) * 512 == blocks * bsize, 'drive 0 size != card size'
    assert Path(f'/sys/block/{name}/ro').read_text().strip() == '0', 'drive 0 should be writable (PC owns it)'
    parts = sorted(p for p in os.listdir(f'/sys/block/{name}') if p.startswith(name))
    target = f'/dev/{parts[0]}' if parts else disk

    mp = mountpoint_of(target)
    testdir = Path(mp, 'GTETEST')
    shutil.rmtree(testdir, ignore_errors=True)
    testdir.mkdir()
    data = os.urandom(SIZE)
    t0 = time.time()
    with open(testdir / 'USBSD4M.BIN', 'wb') as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    t_write = time.time() - t0

    mp = remount(target)  # drop the page cache so the read really comes from the card over USB
    t0 = time.time()
    back = Path(mp, 'GTETEST', 'USBSD4M.BIN').read_bytes()
    t_read = time.time() - t0
    print(f'SD over USB: write {SIZE / 1024 / t_write:.0f} KiB/s, read {SIZE / 1024 / t_read:.0f} KiB/s')
    assert back == data, 'data read back over USB differs from what was written'

    shutil.rmtree(Path(mp, 'GTETEST'))
    os.sync()
    time.sleep(6)  # next heartbeat prints the SD counters
    counters = re.findall(r'sdlun: reads=(\d+) \((\d+) B\) writes=(\d+) \((\d+) B\) errors=(\d+)', uart.text)
    assert counters, 'no SD drive counters in the log'
    reads, rbytes, writes, wbytes, errors = map(int, counters[-1])
    print(f'firmware SD counters: reads={reads} ({rbytes} B) writes={writes} ({wbytes} B) errors={errors}')
    assert errors == 0 and wbytes >= SIZE and rbytes >= SIZE
