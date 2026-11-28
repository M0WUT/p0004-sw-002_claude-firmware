"""IEEE 488.2 common commands (issue #44), as documented in their @scpi blocks."""

import re

import pytest
import pyvisa

from conftest import CORE_PRODUCT, TEST_PRODUCT, TEST_VERSION, board_uid

pytestmark = pytest.mark.hardware


def test_idn(core_scpi, scpi_board):
    """Core: M0WUT,GTE Core,<UID>,<Core version>. Test Application: its own name and
    version. The UID is the one read over SWD (and the USB serial)."""
    assert core_scpi.firmware
    core_version = core_scpi.firmware.version
    assert (
        core_scpi.query("*IDN?") == f"M0WUT,{CORE_PRODUCT},{board_uid()},{core_version}"
    )
    core_scpi.close()
    app = scpi_board()
    assert app.query("*IDN?") == f"M0WUT,{TEST_PRODUCT},{board_uid()},{TEST_VERSION}"
    assert re.fullmatch(r"[0-9A-F]{24}", board_uid())


def test_rst(scpi, uart):
    """*RST calls the Application's reset hook, and leaves Config Parameters, the
    error queue and the status enables alone."""
    scpi.ok('SYST:GTE:PAR "APPLICATION_TEST","count",20')
    scpi.ok("*ESE 60;*SRE 32;STAT:QUES:ENAB 512;STAT:OPER:ENAB 512")
    scpi.write("BOGUS")
    scpi.write("*RST")
    uart.wait_for(r"\[APPLICATION_TEST\].*Reset hook called \(\*RST\)", 2)
    assert scpi.query('SYST:GTE:PAR? "APPLICATION_TEST","count"') == "20"
    assert scpi.query("*ESE?;*SRE?;STAT:QUES:ENAB?;STAT:OPER:ENAB?") == (
        "60;32;512;512"
    )
    assert scpi.errors() == [(-113, "Undefined header")]


def test_cls(scpi_diag):
    """*CLS clears the error queue and the ESR, QUES and OPER event registers; the
    enables are kept."""
    s = scpi_diag
    s.ok("*ESE 32;STAT:QUES:ENAB 512;STAT:OPER:ENAB 512")
    s.ok("TEST:OPER 1")
    s.write('DIAG:WARN "for CLS"')
    s.write("BOGUS")
    s.write("*CLS")
    assert s.query("SYST:ERR:COUN?") == "0"
    assert s.query("*ESR?;STAT:QUES?;STAT:OPER?") == "0;0;0"
    assert s.query("*ESE?;STAT:QUES:ENAB?;STAT:OPER:ENAB?") == "32;512;512"
    s.ok("DIAG:WARN:CLE;TEST:OPER 0")


def test_esr_ese(scpi):
    """The worked examples: after an unknown command *ESR? gives 32, then 0.
    Execution errors set bit 4, device-specific errors (-350) bit 3."""
    scpi.write("BOGUS")
    assert scpi.query("*ESR?") == "32"
    assert scpi.query("*ESR?") == "0"
    scpi.write("*ESE 256")
    assert scpi.query("*ESR?") == "16"
    for _ in range(20):
        scpi.write("BOGUS")
    assert scpi.query("*ESR?") == str(32 + 8)
    scpi.write("*CLS")
    scpi.ok("*ESE 60")
    assert scpi.query("*ESE?") == "60"
    scpi.write("*ESE 256")
    assert scpi.errors() == [(-222, "Data out of range")]
    assert scpi.query("*ESE?") == "60"


def test_stb_sre(scpi):
    """*STB? is 4 while the queue holds errors (the worked example); *ESE 32 sets bit
    5; *SRE 32 then sets bit 6 (MSS); *SRE 256 gives -222."""
    assert scpi.query("*STB?") == "0"
    scpi.write("BOGUS")
    assert scpi.query("*STB?") == "4"
    scpi.write("*ESE 32")
    assert scpi.query("*STB?") == str(4 + 32)
    scpi.write("*SRE 32")
    assert scpi.query("*SRE?") == "32"
    assert scpi.query("*STB?") == str(4 + 32 + 64)
    scpi.write("*SRE 256")
    assert scpi.query("*SRE?") == "32"
    assert scpi.errors() == [(-113, "Undefined header"), (-222, "Data out of range")]
    scpi.write("*CLS")
    assert scpi.query("*STB?") == "0"


def test_opc_wai(scpi):
    """*OPC? gives 1; *OPC sets ESR bit 0; *WAI is accepted."""
    assert scpi.query("*OPC?") == "1"
    scpi.ok("*OPC")
    assert scpi.query("*ESR?") == "1"
    scpi.ok("*WAI")


def test_tst(scpi):
    """*TST? gives 0 (no self-test designed yet)."""
    assert scpi.query("*TST?") == "0"


def test_pyvisa_asrl(scpi):
    """PyVISA (pyvisa-py) opens the CDC port as an ASRL resource and gets *IDN?, with
    LF terminations both ways (issue #6)."""
    path = scpi.path
    scpi.close()
    rm = pyvisa.ResourceManager("@py")
    inst = rm.open_resource(f"ASRL{path}::INSTR")
    try:
        assert isinstance(inst, pyvisa.resources.SerialInstrument)
        inst.read_termination = "\n"
        inst.write_termination = "\n"
        inst.timeout = 2000
        assert inst.query("*IDN?") == (
            f"M0WUT,{TEST_PRODUCT},{board_uid()},{TEST_VERSION}"
        )
        inst.write_termination = "\r\n"  # PyVISA's default
        assert inst.query("SYST:ERR?") == '0,"No error"'
    finally:
        inst.close()
        rm.close()
