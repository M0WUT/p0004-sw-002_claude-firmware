"""Every SCPI command is documented in a /** @scpi */ block, and every block is a
registered command (issue #44). The registered list comes from the firmware itself
(DIAG:COMM:CAT?)."""

import re
from pathlib import Path

import pytest

from conftest import ENV, REPO

pytestmark = pytest.mark.hardware

BLOCK = re.compile(r"/\*\*\s*@scpi\b(.*?)\*/", re.S)
KEY = re.compile(r"^\s*\*?\s*([a-z]+):\s*(.*)$")
# Examples that would stop or restart the device.
DESTRUCTIVE = ("DIAG:FAT", "DIAG:HANG", "SYST:REB", "SYST:SEC")


def blocks(paths: list[Path]) -> list[dict[str, list[str]]]:
    out = []
    for p in paths:
        for m in BLOCK.finditer(p.read_text()):
            fields: dict[str, list[str]] = {}
            for line in m.group(1).splitlines():
                k = KEY.match(line)
                if k:
                    fields.setdefault(k.group(1), []).append(k.group(2).strip())
            out.append(fields)
    return out


def expand(header: str) -> set[str]:
    """SYSTem:ERRor[:NEXT]? -> {SYSTem:ERRor?, SYSTem:ERRor:NEXT?}"""
    m = re.search(r"\[([^\]]*)\]", header)
    if not m:
        return {header}
    start, end = m.span()
    head, tail = header[:start], header[end:]
    return expand(head + tail) | expand(head + m.group(1) + tail)


def documented(paths: list[Path]) -> set[str]:
    headers: set[str] = set()
    for b in blocks(paths):
        query = b.get("form", ["set"])[0] == "query"
        h = (b.get("syntax") or b["command"])[0].split()[0]
        if query and not h.endswith("?"):
            h += "?"
        headers |= expand(h)
    return headers


def core_sources() -> list[Path]:
    files = [p for d in ("lib", "src") for p in (REPO / d).rglob("*") if p.is_file()]
    return [p for p in files if p.suffix in (".h", ".hpp", ".c", ".cc", ".cpp")]


def app_sources() -> list[Path]:
    return sorted((REPO / "hardware_tests/apps/test").glob("*.cc"))


def registered(s) -> set[str]:
    reply = s.query("DIAG:COMM:CAT?")
    headers = re.findall(r'"([^"]*)"', reply)
    assert len(headers) == len(set(headers)), "a header registered twice"
    return set(headers)


def test_every_command_documented(scpi_board):
    """Core firmware: the registered headers match Core's @scpi blocks, both ways."""
    s = scpi_board(env=ENV, unlock=True)
    reg, doc = registered(s), documented(core_sources())
    assert not reg - doc, f"registered but not documented: {sorted(reg - doc)}"
    assert not doc - reg, f"documented but not registered: {sorted(doc - reg)}"


def test_application_commands_documented(scpi_diag):
    """Test Application: registered = Core's blocks + the Application's blocks."""
    reg = registered(scpi_diag)
    doc = documented(core_sources()) | documented(app_sources())
    assert not reg - doc, f"registered but not documented: {sorted(reg - doc)}"
    assert not doc - reg, f"documented but not registered: {sorted(doc - reg)}"


def test_worked_examples_accepted(scpi_board):
    """Every example in Core's blocks runs without a command (-1xx) error. Values
    aren't compared: they describe another Instrument."""
    s = scpi_board(env=ENV, unlock=True)
    examples = [e for b in blocks(core_sources()) for e in b.get("example", [])]
    assert len(examples) > 20
    for e in examples:
        cmd = e.split("|")[0].strip()
        if cmd.startswith(DESTRUCTIVE):
            continue
        if "?" in cmd:
            s.query(cmd)
        else:
            s.write(cmd)
        bad = [x for x in s.errors() if -200 < x[0] <= -100]
        assert not bad, f"{cmd!r}: {bad}"
    s.write('DIAG:WARN:CLE;SYST:GTE:PAR:DEF "CORE";*CLS')
