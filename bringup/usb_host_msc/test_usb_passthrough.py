"""USB pass-through: the host-port memory stick exposed to the PC as read-only LUN 1 of
the device.

Needs the same setup as test_usb_device.py. The stick is never written (LUN 1 is
write-protected).
"""

import hashlib
import os
import re
import time
from pathlib import Path

from conftest import reset_board, wait_for_path, mountpoint_of, remount
from usb_common import PASSTHROUGH_GLOB, ENUM_TIMEOUT

PROJECT = Path(__file__).parent


MAX_BYTES = 32 * 1024 * 1024  # full-speed USB reads ~1 MB/s, so keep the test bounded


def _files_digest(mp: str) -> dict:
    """sha256 of files on the stick, in a stable order, up to MAX_BYTES in total."""
    out, total = {}, 0
    for root, dirs, files in sorted(os.walk(mp)):
        dirs.sort()
        for f in sorted(files):
            p = Path(root, f)
            try:
                size = p.stat().st_size
                if total + size > MAX_BYTES:
                    continue
                out[str(p.relative_to(mp))] = hashlib.sha256(p.read_bytes()).hexdigest()
                total += size
            except OSError as e:
                out[str(p.relative_to(mp))] = f"ERROR {e}"
    return out


def test_passthrough_lun_reads_stick(flash, open_uart):
    flash(PROJECT, "stage3")
    uart = open_uart()
    reset_board()
    uart.wait_for(r"HOST: MSC mounted", 10)
    uart.wait_for(r"DEVICE: configured by the PC", ENUM_TIMEOUT)
    lun = os.path.realpath(wait_for_path(PASSTHROUGH_GLOB, ENUM_TIMEOUT))
    # Size must match what the host side read from the stick
    m = uart.wait_for(r"MSC: (\d+) blocks x (\d+) bytes", 5)
    blocks, bsize = int(m.group(1)), int(m.group(2))
    size = int(Path(f"/sys/block/{Path(lun).name}/size").read_text()) * 512
    assert size == blocks * bsize, f"LUN 1 is {size} bytes, stick is {blocks * bsize}"
    assert (
        Path(f"/sys/block/{Path(lun).name}/ro").read_text().strip() == "1"
    ), "LUN 1 should be read-only"

    # Mount the stick's first partition (or the whole LUN if unpartitioned) through the
    # pass-through
    parts = sorted(
        p
        for p in os.listdir(f"/sys/block/{Path(lun).name}")
        if p.startswith(Path(lun).name)
    )
    target = f"/dev/{parts[0]}" if parts else lun
    t0 = time.time()
    mp = mountpoint_of(target, 30)
    first = _files_digest(mp)
    t1 = time.time()
    total = sum(
        Path(mp, f).stat().st_size for f in first if not first[f].startswith("ERROR")
    )
    print(f"pass-through: {len(first)} files, {total} bytes read in {t1 - t0:.1f} s")
    assert first, "no files seen on the stick through the pass-through LUN"
    assert not [f for f, h in first.items() if h.startswith("ERROR")], first
    # Re-read after a remount (drops the page cache): content must be identical
    mp = remount(target)
    assert _files_digest(mp) == first, "pass-through reads were not repeatable"
    time.sleep(6)  # let the next heartbeat print the pass-through counters
    counters = re.findall(
        r"passthrough: reads=(\d+) bytes=(\d+) errors=(\d+) busy=(\d+)", uart.text
    )
    assert counters, "no pass-through counters in the log"
    reads, nbytes, errors, busy = map(int, counters[-1])
    print(
        f"pass-through counters: reads={reads} bytes={nbytes} "
        f"errors={errors} busy={busy}"
    )
    assert reads > 0 and errors == 0
