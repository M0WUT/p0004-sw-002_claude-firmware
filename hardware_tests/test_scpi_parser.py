"""Parsing and the CDC line reader (issue #44): Vrekrer plus GTE's Execute() layer
(issue #6) and the PyVISA requirements."""

import time

import pytest

pytestmark = pytest.mark.hardware


def test_long_short_case(scpi):
    """Long and short forms, any case, and the optional NEXT, are one command."""
    for q in ("SYSTem:ERRor:NEXT?", "SYST:ERR?", "syst:err?", "SYST:ERR:NEXT?"):
        assert scpi.query(q) == '0,"No error"', q


def test_compound_commands(scpi):
    """Responses from one line are joined by ';'; headers chain absolutely after
    ':' and relatively without it (SCPI-99 6.2.4)."""
    scpi.write("BOGUS")
    assert scpi.query("*CLS;*ESR?") == "0"
    assert scpi.query("SYST:VERS?;:SYST:ERR:COUN?") == "1999.0;0"
    scpi.write("BOGUS")
    assert scpi.query("SYST:ERR:COUN?;NEXT?") == '1;-113,"Undefined header"'


def test_quoted_strings(scpi):
    """A comma inside a quoted parameter stays in it; a doubled quote is one quote."""
    scpi.ok('SYST:GTE:PAR "APPLICATION_TEST","name","A,B"')
    assert scpi.query('SYST:GTE:PAR? "APPLICATION_TEST","name"') == '"A,B"'
    scpi.ok('SYST:GTE:PAR "APPLICATION_TEST","name","x""y"')
    assert scpi.query('SYST:GTE:PAR? "APPLICATION_TEST","name"') == '"x""y"'


def test_terminators(scpi):
    """LF and CRLF both work, empty lines are ignored, and every response ends with
    exactly one LF."""
    scpi.write("*OPC?", end="\r\n")
    scpi.port.timeout = 2
    assert scpi.port.read_until(b"\n") == b"1\n"
    scpi.write("", end="\n")
    scpi.write("", end="\r\n")
    scpi.write("*OPC?", end="\n")
    assert scpi.port.read_until(b"\n") == b"1\n"
    assert scpi.no_reply(0.3), "an empty line produced output"


def test_pipelined_lines(scpi):
    """Several commands in one USB write all run, in order."""
    scpi.port.write(b"TEST:A:VAL 1\nTEST:A:VAL?\nTEST:A:VAL 2\nTEST:A:VAL?\n*OPC?\n")
    assert [scpi.read_line() for _ in range(3)] == ["1", "2", "1"]


def test_split_line(scpi):
    """A command split across writes 500 ms apart still runs as one line."""
    for part in (b"TEST:A:", b"VAL 4", b"2\n"):
        scpi.port.write(part)
        scpi.port.flush()
        time.sleep(0.5)
    assert scpi.query("TEST:A:VAL?") == "42"


def test_undefined_header(scpi):
    """An unknown command or query adds -113, gives no reply, and the next command
    works."""
    scpi.write("NOT:A:COMMand 3")
    scpi.write("NOT:A:QUERy?")
    scpi.write("*IDN")  # only the query exists
    assert scpi.no_reply()
    assert scpi.errors() == [(-113, "Undefined header")] * 3
    assert scpi.query("*OPC?") == "1"


def test_overlong_line(scpi):
    """A line past the 255-character buffer gives one -363, none of it runs, and the
    next line works."""
    scpi.ok("TEST:A:VAL 5")
    scpi.write("TEST:A:VAL 6;" + "*WAI;" * 60)
    assert scpi.errors() == [(-363, "Input buffer overrun")]
    assert scpi.query("TEST:A:VAL?") == "5"
    scpi.write("TEST:A:VAL 7;" + "*WAI;" * 48)  # 253 characters: fits
    assert scpi.query("TEST:A:VAL?") == "7"
    longest = "TEST:A:VAL 8;" + "*WAI;" * 48 + "  "
    assert len(longest) == 255
    scpi.write(longest, end="\r\n")  # exactly 255 with CRLF still runs
    assert scpi.query("TEST:A:VAL?") == "8"
    scpi.write("TEST:A:VAL 9;" + "*WAI;" * 48 + "   ")  # 256: one too many
    assert scpi.errors() == [(-363, "Input buffer overrun")]
    assert scpi.query("TEST:A:VAL?") == "8"
