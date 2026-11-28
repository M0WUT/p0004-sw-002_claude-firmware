"""The generic tree SYSTem:GTE (issue #44): Services, Config Parameters and Status
Values over SCPI, through the same checks as every other interface."""

import time

import pytest

pytestmark = pytest.mark.hardware

APP = '"APPLICATION_TEST"'


def par(s, name: str, extra: str = "") -> str:
    return s.query(f'SYST:GTE:PAR? {APP},"{name}"{extra}')


def test_service_catalog(scpi):
    """Exactly the registered Services, quoted, upper case, in ServiceId order."""
    assert scpi.query("SYST:GTE:CAT?") == (
        '"CORE","USB_DEVICE","SCPI","APPLICATION_TEST","TEST_AUX","OSS_LICENCES"'
    )


def test_parameter_catalog(scpi):
    """The worked example ("CORE" has "logLevel"), the test Application's full list,
    and -224 for an unknown Service."""
    assert scpi.query('SYST:GTE:PAR:CAT? "CORE"') == '"logLevel"'
    assert scpi.query(f"SYST:GTE:PAR:CAT? {APP}") == (
        '"logLevel","count","gain","enabled","name","mode","password",'
        '"channels","address"'
    )
    scpi.write('SYST:GTE:PAR:CAT? "NOPE"')
    assert scpi.no_reply()
    assert scpi.expect_error(-224)[1].startswith("Illegal parameter value")


def test_parameter_read(scpi):
    """One of each type, MIN/MAX/DEF, and the worked example (CORE logLevel INFO)."""
    assert par(scpi, "count") == "10"
    assert par(scpi, "gain") == "1.5"
    assert par(scpi, "enabled") == "1"
    assert par(scpi, "name") == '"GTE"'
    assert par(scpi, "mode") == "SLOW"
    assert par(scpi, "address") == "192.168.1.10"
    assert [par(scpi, "count", f",{x}") for x in ("MIN", "MAX", "DEF")] == [
        "0",
        "100",
        "10",
    ]
    assert [par(scpi, "gain", f",{x}") for x in ("MIN", "MAX", "DEF")] == [
        "0.0",
        "10.0",
        "1.5",
    ]
    assert scpi.query('SYST:GTE:PAR? "CORE","logLevel"') == "INFO"


def test_parameter_set(scpi, uart):
    """A valid set applies at once (side effect on the UART); enum names in any
    case; booleans ON/OFF/1/0; MIN/MAX/DEF as values; an IP address."""
    scpi.ok(f'SYST:GTE:PAR {APP},"count",20')
    uart.wait_for(r"\[APPLICATION_TEST\].*Config count = 20: accepted", 2)
    uart.wait_for(r"\[APPLICATION_TEST\].*Side effect for count", 2)
    assert par(scpi, "count") == "20"
    scpi.ok(f'SYST:GTE:PAR {APP},"mode",fast')
    assert par(scpi, "mode") == "FAST"
    for value, want in (("OFF", "0"), ("ON", "1"), ("0", "0"), ("1", "1")):
        scpi.ok(f'SYST:GTE:PAR {APP},"enabled",{value}')
        assert par(scpi, "enabled") == want, value
    for value, want in (("MIN", "0"), ("MAX", "100"), ("DEF", "10")):
        scpi.ok(f'SYST:GTE:PAR {APP},"count",{value}')
        assert par(scpi, "count") == want, value
    scpi.ok(f'SYST:GTE:PAR {APP},"address",10.0.0.2')
    assert par(scpi, "address") == "10.0.0.2"


@pytest.mark.parametrize(
    "name, value, code",
    [
        ("count", "105", -222),  # out of range
        ("count", "12", -224),  # off the step of 5
        ("gain", "9.0", -221),  # the Service's own rule (Enabled is ON)
        ("count", "abc", -104),  # wrong type
        ("count", '"20"', -104),  # a number must not be quoted
        ("name", "GTE2", -104),  # text must be quoted
        ("name", '"' + "x" * 17 + '"', -222),  # text too long (16 max)
        ("mode", "BOGUS", -224),  # not an allowed name
        ("mode", "MIN", -224),  # MIN on a non-numeric parameter
        ("nope", "1", -224),  # unknown parameter
    ],
)
def test_parameter_errors(scpi, name, value, code):
    """Each failure adds the agreed error (open question 7) and changes nothing."""
    before = scpi.query(f"SYST:GTE:PAR:CAT? {APP}")
    values = {p.strip('"'): par(scpi, p.strip('"')) for p in before.split(",")[:6]}
    scpi.write(f'SYST:GTE:PAR {APP},"{name}",{value}')
    err = scpi.expect_error(code)
    if code == -221:
        assert err[1] == "Settings conflict; Gain above 8 dB needs Enabled OFF"
    assert {p: par(scpi, p) for p in values} == values


def test_unknown_service(scpi):
    scpi.write('SYST:GTE:PAR "NOPE","count",1')
    assert scpi.expect_error(-224)[1] == "Illegal parameter value; Unknown Service NOPE"


def test_secret_parameter(scpi, uart):
    """A secret can be written but not read (-221), and its value never appears in
    a response or on the UART."""
    scpi.ok(f'SYST:GTE:PAR {APP},"password","s3cretValue"')
    scpi.write(f'SYST:GTE:PAR? {APP},"password"')
    assert scpi.no_reply()
    assert scpi.expect_error(-221)[0] == -221
    uart.wait_for(r"Config password = \*\*\*\*: accepted", 2)
    replies = [
        scpi.query(f"SYST:GTE:PAR:CAT? {APP}"),
        scpi.query(f"SYST:GTE:STAT:CAT? {APP}"),
    ]
    scpi.write(f'SYST:GTE:PAR? {APP},"password",DEF')  # not numeric: -224
    replies += [f"{c},{m}" for c, m in scpi.errors()]
    for secret in ("s3cretValue", "hunter2"):
        assert not [r for r in replies if secret in r], "secret in a response"
        assert secret not in uart.text, "secret on the UART"


def test_parameter_defaults(scpi):
    """PAR:DEF restores every parameter of one Service, including its Log level;
    other Services keep theirs."""
    scpi.ok(f'SYST:GTE:PAR {APP},"count",20')
    scpi.ok(f'SYST:GTE:PAR {APP},"name","CHANGED"')
    scpi.ok(f'SYST:GTE:PAR {APP},"logLevel",WARNING')
    scpi.ok('SYST:GTE:PAR "TEST_AUX","rate",9')
    scpi.ok(f"SYST:GTE:PAR:DEF {APP}")
    assert par(scpi, "count") == "10"
    assert par(scpi, "name") == '"GTE"'
    assert par(scpi, "logLevel") == "INFO"
    assert scpi.query('SYST:GTE:PAR? "TEST_AUX","rate"') == "9"


def test_restart_required(scpi, uart):
    """RES:REQ? is 0, and 1 after a change to a Reset-mode parameter, which is saved
    but not applied live (no side effect)."""
    assert scpi.query("SYST:GTE:RES:REQ?") == "0"
    scpi.ok(f'SYST:GTE:PAR {APP},"channels",6')
    assert scpi.query("SYST:GTE:RES:REQ?") == "1"
    assert par(scpi, "channels") == "6"
    uart.wait_for(r"Config channels = 6: accepted \(restart required\)", 2)
    assert "Side effect for channels" not in uart.text


def test_status_values(scpi):
    """STAT:CAT? and the exact values (integer bytes, float, 1/0, quoted text); an
    unknown id gives -224."""
    assert scpi.query(f"SYST:GTE:STAT:CAT? {APP}") == (
        '"dataUsed","temperature","ready","label"'
    )
    q = [f'SYST:GTE:STAT? {APP},"{v}"' for v in ("dataUsed", "temperature")]
    q += [f'SYST:GTE:STAT? {APP},"{v}"' for v in ("ready", "label")]
    assert scpi.query(";".join(q)) == '1288490188;23.5;1;"TEST"'
    assert scpi.query('SYST:GTE:STAT:CAT? "CORE"') == '""'
    scpi.write(f'SYST:GTE:STAT? {APP},"nope"')
    assert scpi.no_reply()
    scpi.expect_error(-224)


def test_log_level_over_scpi(scpi, uart):
    """The worked example: CORE's Log level to DEBUG makes its debug lines appear
    on the UART at once."""
    debug = r"\[CORE\]\s*\tDEBUG\t"
    time.sleep(1.5)
    assert not uart.lines_matching(debug), "CORE debug lines before the change"
    scpi.ok('SYST:GTE:PAR "CORE","logLevel",DEBUG')
    uart.wait_for(debug, 1.5)
    assert scpi.query('SYST:GTE:PAR? "CORE","logLevel"') == "DEBUG"
