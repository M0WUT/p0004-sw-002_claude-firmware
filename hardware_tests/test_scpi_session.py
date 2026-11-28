"""The SCPI session on USB CDC (issue #15): it belongs to the port while DTR is high.
The TCP side, and the single-session rule across both, come with #46."""

import pytest

from conftest import SESSION_RE, Scpi

pytestmark = pytest.mark.hardware


def test_needs_dtr(scpi, uart):
    """With DTR low a set isn't applied and a query gets no reply; raising DTR opens
    the session."""
    path = scpi.path
    scpi.close()
    uart.wait_for(r"\[SCPI\].*Session released: USB", 2)
    s = Scpi(path, dtr=False)
    try:
        s.write("TEST:A:VAL 9")
        s.write("*IDN?")
        assert s.no_reply(1.0)
        uart.clear()
        s.port.dtr = True
        uart.wait_for(SESSION_RE, 2)
        assert s.query("TEST:A:VAL?") == "0"
        assert s.errors() == []
    finally:
        s.close()


def test_release_and_reopen(scpi, uart):
    """Closing the port (DTR drops) releases the session; reopening works without a
    reset, and the error queue is kept."""
    path = scpi.path
    scpi.write("BOGUS")
    assert scpi.query("SYST:ERR:COUN?") == "1"
    scpi.close()
    uart.wait_for(r"\[SCPI\].*Session released: USB", 2)
    uart.clear()
    s = Scpi(path)
    try:
        uart.wait_for(SESSION_RE, 2)
        assert s.query("*OPC?") == "1"
        assert s.errors() == [(-113, "Undefined header")]
    finally:
        s.close()
