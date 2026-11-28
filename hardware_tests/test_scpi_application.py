"""Application commands (issue #44): an Application registers its own SCPI subtree,
with context pointers (no globals in the parser) and numeric suffixes (issues #6,
#15)."""

import pytest

from conftest import (
    BOOT_RE,
    SCENARIOS,
    TEST_RAM_ENV,
    build_cached,
    put_on_board,
    reset_with,
)

pytestmark = pytest.mark.hardware


def test_application_commands(scpi):
    """TEST:A and TEST:B share one handler with different contexts; each reads back
    its own value."""
    scpi.ok("TEST:A:VAL 5;TEST:B:VAL 7")
    assert scpi.query("TEST:A:VAL?;TEST:B:VAL?") == "5;7"
    assert scpi.query("TEST:A:VALue?;:TEST:B:VALUE?") == "5;7"


def test_numeric_suffix(scpi):
    """TEST:CHANnel2:VAL reaches the handler as channel 2; no suffix is channel 1;
    CHAN9 is out of range (-114)."""
    scpi.ok("TEST:CHAN2:VAL 3;TEST:CHANNEL4:VAL 8;TEST:CHAN:VAL 1")
    got = [scpi.query(f"TEST:CHAN{n}:VAL?") for n in (1, 2, 3, 4)]
    assert got == ["1", "3", "0", "8"]
    scpi.write("TEST:CHAN9:VAL 1")
    scpi.write("TEST:CHAN9:VAL?")
    assert scpi.no_reply()
    assert [e[0] for e in scpi.errors()] == [-114, -114]


@pytest.mark.parametrize(
    "scenario, message",
    [
        ("scpi_table_full", r"SCPI command table full: TFULl:N\d+"),
        ("scpi_collision", r"SCPI command hash collision or duplicate: TCOLlisionA:"),
    ],
)
def test_command_table_limits(request, uart, scenario, message):
    """Registering past the table's limit, or a pair whose 32-bit hashes collide,
    is a Fatal Error at boot naming the header (open question 10)."""
    put_on_board(request, build_cached(TEST_RAM_ENV))
    uart.clear()
    reset_with(SCENARIOS[scenario], config_action="fresh")
    uart.wait_for(BOOT_RE, 5)
    uart.wait_for(rf"\[SCPI\]\s*\tERROR\t110[12]\t{message}", 5)
