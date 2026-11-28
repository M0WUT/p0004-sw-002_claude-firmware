"""SYSTem and STATus (issue #44): the error queue, the status registers, restart and
Factory Reset."""

import re

import pytest

from conftest import TEST_ENV, reopen

pytestmark = pytest.mark.hardware

APP = '"APPLICATION_TEST"'
WARN_106 = "Engineering test Warning (DIAG:WARNing)"


def test_syst_vers(scpi):
    assert scpi.query("SYST:VERS?") == "1999.0"


def test_error_queue(scpi):
    """Oldest first; SYST:ERR:COUN? counts; empty gives 0,"No error"."""
    scpi.write("BOGUS")
    scpi.write("*ESE 999")
    assert scpi.query("SYST:ERR:COUN?") == "2"
    assert scpi.error() == (-113, "Undefined header")
    assert scpi.query("SYST:ERR:COUN?") == "1"
    assert scpi.error() == (-222, "Data out of range")
    assert scpi.query("SYST:ERR?") == '0,"No error"'
    assert scpi.query("SYST:ERR:COUN?") == "0"


def test_error_queue_overflow(scpi):
    """Past 16 entries the newest becomes -350 and the oldest are kept."""
    scpi.write("*ESE 999")
    for _ in range(20):
        scpi.write("BOGUS")
    assert scpi.query("SYST:ERR:COUN?") == "16"
    errs = scpi.errors()
    assert errs[0] == (-222, "Data out of range")
    assert errs[1:15] == [(-113, "Undefined header")] * 14
    assert errs[15] == (-350, "Queue overflow")


def test_warning_in_error_queue(scpi_diag):
    """A raised Warning is queued as <code>,"<message>: <supplementary>"."""
    s = scpi_diag
    s.write('DIAG:WARN "bench check"')
    assert s.errors() == [(106, f"{WARN_106}: bench check")]
    s.ok("DIAG:WARN:CLE")


def test_ques_warning_bit(scpi_diag):
    """QUES bit 9: COND? 512 while a Warning is active and 0 once cleared; EVEN?
    latches and clears on read; STAT:QUES:ENAB 512 sets STB bit 3."""
    s = scpi_diag
    assert s.query("STAT:QUES:COND?;STAT:QUES?") == "0;0"
    s.write('DIAG:WARN "one"')
    assert s.query("STAT:QUES:COND?") == "512"
    s.write("DIAG:WARN:CLE")
    assert s.errors() == [(106, f"{WARN_106}: one")]
    assert s.query("STAT:QUES:COND?") == "0"
    assert s.query("STAT:QUES:EVEN?") == "512", "event not latched"
    assert s.query("STAT:QUES?") == "0", "event not cleared by reading"
    s.write("*CLS;STAT:QUES:ENAB 512")
    assert s.query("*STB?") == "0"
    s.write('DIAG:WARN "two"')
    assert int(s.query("*STB?")) & 8, "QUES summary (bit 3) not set"
    assert s.errors() == [(106, f"{WARN_106}: two")]
    s.ok("DIAG:WARN:CLE")


def test_oper_register(scpi):
    """An Application OPERation bit (9): COND?, EVEN? (cleared by reading) and ENAB
    routing it to STB bit 7."""
    assert scpi.query("STAT:OPER:COND?;STAT:OPER?") == "0;0"
    scpi.ok("TEST:OPER 1")
    assert scpi.query("STAT:OPER:COND?") == "512"
    assert scpi.query("STAT:OPER:EVEN?") == "512"
    assert scpi.query("STAT:OPER?") == "0"
    assert scpi.query("*STB?") == "0"
    scpi.ok("TEST:OPER 0;STAT:OPER:ENAB 512;TEST:OPER 1")
    assert scpi.query("*STB?") == "128"
    scpi.ok("TEST:OPER 0")


def test_enable_queries(scpi):
    """The enable registers read back; masks above 65535 give -222."""
    scpi.ok("STAT:OPER:ENAB 65535;STAT:QUES:ENAB 528")
    assert scpi.query("STAT:OPER:ENAB?;STAT:QUES:ENAB?") == "65535;528"
    scpi.write("STAT:OPER:ENAB 65536")
    scpi.write("STAT:QUES:ENAB 70000")
    assert scpi.errors() == [(-222, "Data out of range")] * 2
    assert scpi.query("STAT:OPER:ENAB?;STAT:QUES:ENAB?") == "65535;528"


def test_stat_pres(scpi):
    """STAT:PRES returns both enable registers to 0."""
    scpi.ok("STAT:OPER:ENAB 512;STAT:QUES:ENAB 512;STAT:PRES")
    assert scpi.query("STAT:OPER:ENAB?;STAT:QUES:ENAB?") == "0;0"


def test_syst_reboot(scpi_board, uart):
    """Real EEPROM: SYST:REB restarts (reset reason software), USB comes back, the
    pending Reset-mode change is applied and nothing is waiting any more."""
    s = scpi_board(env=TEST_ENV)
    s.ok(f'SYST:GTE:PAR {APP},"channels",6')
    assert s.query("SYST:GTE:RES:REQ?") == "1"
    uart.clear()
    s.write("SYST:REB")
    s = reopen(uart, s)
    try:
        uart.wait_for(r"^GTE reset reason=software", 1)
        assert s.query("SYST:GTE:RES:REQ?") == "0"
        assert s.query(f'SYST:GTE:PAR? {APP},"channels"') == "6"
        uart.wait_for(r"\[APPLICATION_TEST\].*Channels \(channels\): 6 \[reset\]", 1)
        assert s.errors() == []
    finally:
        s.close()


def test_factory_reset(scpi_board, uart):
    """Real EEPROM: SYST:SEC:IMM restores every Service's defaults (Log levels too)
    after a restart; the MAC is unchanged."""
    s = scpi_board(env=TEST_ENV)
    mac = uart.wait_for(r"\[CORE\].*\tMAC ([0-9A-F:]{17})$", 1).group(1)
    s.ok(f'SYST:GTE:PAR {APP},"count",20')
    s.ok('SYST:GTE:PAR "TEST_AUX","rate",9')
    s.ok('SYST:GTE:PAR "TEST_AUX","logLevel",WARNING')
    uart.clear()
    s.write("SYST:SEC:IMM")
    s = reopen(uart, s)
    try:
        assert re.search(rf"\tMAC {mac}$", uart.text, re.M), "MAC changed"
        assert s.query(f'SYST:GTE:PAR? {APP},"count"') == "10"
        assert s.query('SYST:GTE:PAR? "TEST_AUX","rate"') == "5"
        assert s.query('SYST:GTE:PAR? "TEST_AUX","logLevel"') == "INFO"
        assert s.errors() == []
    finally:
        s.close()
