"""SD card on SDMMC1 (4-bit) with STM32SD + FatFs under FreeRTOS.

NON-DESTRUCTIVE: the firmware only creates and deletes files in /GTETEST; it never
formats the card or
writes raw blocks. Needs a FAT-formatted (FAT32/exFAT) microSD card in the LCD Board's
slot.
"""

import re
from pathlib import Path

from conftest import reset_board

PROJECT = Path(__file__).parent


def test_sd_card(flash, open_uart):
    flash(PROJECT, "sd_card")
    uart = open_uart()
    reset_board()
    uart.wait_for(
        r"SD: card detect PB8=0 \(card present\)", 5
    )  # fails here if no card is inserted
    uart.wait_for(r"SD: RESULT (PASS|FAIL)", 120)
    text = uart.text
    # Signal integrity: every one of the 60 read-only identifications must agree
    for m in re.finditer(
        r"SD: diag \[(.*?)\] 20 inits: HAL_OK=(\d+) SDHC=(\d+) SDSC=(\d+) other=(\d+) "
        r"same-capacity=(\d+) 'locked'=(\d+)",
        text,
    ):
        mode = m.group(1)
        ok, sdhc, sdsc, _other, same, locked = map(int, m.groups()[1:])
        assert (
            ok == 20 and same == 20 and locked == 0
        ), f"unstable card identification at {mode}: {m.group(0)}"
        assert (
            sdhc == 20 or sdsc == 20
        ), f"card type flipped between inits at {mode}: {m.group(0)}"
    assert re.search(
        r"SD: filesystem FAT type=\d+ .* total=\d+ KiB free=\d+ KiB", text
    ), "filesystem not mounted"
    assert "mismatched words=0" in text, "read-back verify failed"
    assert re.search(r"SD: raw read (\d+) of \1 blocks", text), "raw block reads failed"
    assert "SD: RESULT PASS" in text
