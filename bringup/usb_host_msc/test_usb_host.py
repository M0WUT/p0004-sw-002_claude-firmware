"""USB host on OTG_FS (PA11/PA12): enumerate a memory stick and read 3 x 64 MiB with no
errors.

Needs a USB memory stick in the LCD Board's USB-A socket. The stick is only read, never
written.
"""

from pathlib import Path

from conftest import reset_board
from usb_common import assert_host_passes_clean

PROJECT = Path(__file__).parent


def test_host_msc_sustained_read(flash, open_uart):
    flash(PROJECT, "spike")
    uart = open_uart()
    reset_board()
    uart.wait_for(r"Host VBUS enabled \(PD14 high\); fault pin PD15=1", 5)
    uart.wait_for(
        r"HOST: MSC mounted addr=\d+", 10
    )  # fails here if no stick is inserted
    uart.wait_for(r"MSC: sector 0 signature 55 AA", 5)
    uart.wait_for(r"MSC: test complete", 240)
    assert_host_passes_clean(uart.text)
