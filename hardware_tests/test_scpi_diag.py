"""DIAGnostic engineering commands (Testing strategy, issue #32): locked until
DIAG:UNLock at every boot."""

import re
import time

import pytest
import serial

from conftest import (
    BOOT_RE,
    CORE_PRODUCT,
    DIAG_KEY,
    ENV,
    TEST_PRODUCT,
    ScpiError,
    open_scpi,
    reopen,
    reset_board,
)

pytestmark = pytest.mark.hardware

LOCKED = [
    'DIAG:WARN "x"',
    "DIAG:WARN:CLE",
    'DIAG:FAT "x"',
    "DIAG:HANG",
    "DIAG:COMM:CAT?",
]


def test_diag_locked(scpi, uart):
    """Before DIAG:UNL every DIAG command gives -203 and does nothing; a wrong key
    gives -224 and doesn't unlock; after SYST:REB it is locked again."""
    for cmd in LOCKED:
        scpi.write(cmd)
        assert scpi.no_reply(0.3), cmd
        assert scpi.errors() == [(-203, "Command protected")], cmd
    assert scpi.query("STAT:QUES:COND?") == "0", "a locked DIAG:WARN raised"
    scpi.write('DIAG:UNL "wrong"')
    assert scpi.expect_error(-224)[1] == "Illegal parameter value; Wrong key"
    scpi.write("DIAG:COMM:CAT?")
    assert scpi.errors() == [(-203, "Command protected")]
    scpi.ok(f'DIAG:UNL "{DIAG_KEY}"')
    assert scpi.query("DIAG:COMM:CAT?").startswith('"*IDN?"')
    uart.clear()
    scpi.write("SYST:REB")
    s = reopen(uart, scpi)
    try:
        s.write("DIAG:COMM:CAT?")
        assert s.no_reply(0.3)
        assert s.errors() == [(-203, "Command protected")]
    finally:
        s.close()


def test_diag_warning(scpi_diag, uart):
    """DIAG:WARN raises the CORE test Warning (UART and queue); DIAG:WARN:CLE clears
    it."""
    s = scpi_diag
    s.write('DIAG:WARN "bench check"')
    uart.wait_for(
        r"\[CORE\]\s*\tWARNING\t106\tEngineering test Warning \(DIAG:WARNing\): "
        r"bench check$",
        2,
    )
    assert s.query("STAT:QUES:COND?") == "512"
    s.write("DIAG:WARN:CLE")
    uart.wait_for(r"\[CORE\]\s*\tINFO\t106\tWarning cleared$", 2)
    assert s.query("STAT:QUES:COND?") == "0"
    assert [e[0] for e in s.errors()] == [106]


def test_diag_fatal(scpi_diag, uart):
    """DIAG:FAT stops the device with Fatal Error 107 from CORE (on the UART), USB
    stops answering, and an SWD reset recovers."""
    s = scpi_diag
    s.check_queue_at_end = False
    s.write('DIAG:FAT "bench check"')
    uart.wait_for(
        r"\[CORE\]\s*\tERROR\t107\tEngineering test Fatal Error \(DIAG:FATal\): "
        r"bench check$",
        2,
    )
    with pytest.raises((ScpiError, serial.SerialException)):
        s.query("*OPC?", timeout=1)
    s.close()
    uart.clear()
    reset_board()
    uart.wait_for(BOOT_RE, 5)
    s2 = open_scpi(uart, TEST_PRODUCT)
    try:
        assert s2.query("*OPC?") == "1"
    finally:
        s2.close()


def test_diag_hang_watchdog(scpi_board, uart):
    """Core firmware (the reset record needs the EEPROM, not DEBUG_EEPROM): DIAG:HANG
    stops the SCPI task checking in; the watchdog resets the board, and the first
    SYST:ERR? afterwards names the Service."""
    s = scpi_board(env=ENV, unlock=True)
    uart.clear()
    t0 = time.time()
    s.write("DIAG:HANG")
    uart.wait_for(r"\[Watchdog\].*SCPI missed its check-in; resetting", 5)
    s = reopen(uart, s, CORE_PRODUCT)
    try:
        assert time.time() - t0 < 15
        assert re.search(r"^GTE reset reason=watchdog", uart.text, re.M)
        assert s.query("SYST:ERR?") == (
            '105,"Recovered from a watchdog reset: Service SCPI"'
        )
        s.ok("*CLS")
    finally:
        s.close()
