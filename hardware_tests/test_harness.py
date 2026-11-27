"""The test machinery itself: the report plugin and the pre-commit hook (issue #41).

Each test builds a throwaway git repo containing the real hook and report plugin plus a
dummy
suite (one pass, one fail, one skip, one xfail), so no board is needed.
"""

import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

from conftest import HERE, REPO

DUMMY_CONFTEST = """
import gte_report
pytest_plugins = ['gte_report']
gte_report.META.update(firmware_version='TESTVER', board_serial='TESTSERIAL',
                       io_board='not fitted', instrument_board='not fitted')
"""
DUMMY_TESTS = """
import pytest
def test_pass(): pass
def test_fail(): assert False
def test_skip(): pytest.skip('dummy')
@pytest.mark.xfail(reason='dummy')
def test_xfail(): assert False
"""


def clean_env(**extra) -> dict:
    """Environment for git/pytest in the temp repo, without the outer commit's GIT_*
    variables."""
    env = {
        k: v
        for k, v in os.environ.items()
        if not k.startswith("GIT_") and not k.startswith("PYTEST")
    }
    env.update(
        GIT_AUTHOR_NAME="t",
        GIT_AUTHOR_EMAIL="t@t",
        GIT_COMMITTER_NAME="t",
        GIT_COMMITTER_EMAIL="t@t",
    )
    env.update(extra)
    return env


def git(repo: Path, *args, env=None) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        capture_output=True,
        text=True,
        env=env or clean_env(),
    )


@pytest.fixture
def repo(tmp_path) -> Path:
    r = tmp_path / "repo"
    (r / "hardware_tests").mkdir(parents=True)
    (r / ".githooks").mkdir()
    shutil.copy(HERE / "gte_report.py", r / "hardware_tests")
    shutil.copy(REPO / ".githooks/pre-commit", r / ".githooks")
    (r / "hardware_tests/conftest.py").write_text(DUMMY_CONFTEST)
    (r / "hardware_tests/test_dummy.py").write_text(DUMMY_TESTS)
    (r / "docs").mkdir()
    (r / "docs/notes.md").write_text("notes\n")
    git(r, "init", "-q", "-b", "main")
    git(r, "add", ".")
    assert git(r, "commit", "-qm", "initial").returncode == 0
    git(r, "config", "core.hooksPath", ".githooks")
    return r


def hook_env(**extra) -> dict:
    return clean_env(GTE_HOOK_PYTHON=sys.executable, **extra)


def commit(repo: Path, msg: str, env: dict) -> subprocess.CompletedProcess:
    return git(repo, "commit", "-qm", msg, env=env)


def committed_files(repo: Path) -> list[str]:
    return git(repo, "show", "--name-only", "--format=", "HEAD").stdout.split()


def touch_test_file(repo: Path):
    with open(repo / "hardware_tests/test_dummy.py", "a") as f:
        f.write("# changed\n")
    git(repo, "add", "hardware_tests/test_dummy.py")


def test_report_contents(repo):
    """REPORT.md and report.xml record exactly what was tested, and every test's result
    and time."""
    touch_test_file(repo)
    (repo / "docs/notes.md").write_text("unstaged edit\n")  # left unstaged on purpose
    parent = git(repo, "rev-parse", "--short=12", "HEAD").stdout.strip()
    out = repo / "out"
    subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "hardware_tests",
            "--gte-report",
            str(out),
            "--junitxml",
            str(out / "report.xml"),
            "-p",
            "no:cacheprovider",
        ],
        cwd=repo,
        capture_output=True,
        text=True,
        env=clean_env(),
    )
    md = (out / "REPORT.md").read_text()
    assert "| Result | 1 passed, 1 failed, 1 skipped, 1 xfailed |" in md
    assert f"| Parent commit | {parent} |" in md
    assert "| Staged changes | sha256:" in md
    assert "| Unstaged changes in working tree | yes |" in md
    assert "| Firmware version | TESTVER |" in md
    assert "| Board serial (STM32 UID) | TESTSERIAL |" in md
    assert "| IO Board | not fitted |" in md
    assert "| Instrument Board | not fitted |" in md
    for name, result in [
        ("test_pass", "passed"),
        ("test_fail", "failed"),
        ("test_skip", "skipped"),
        ("test_xfail", "xfailed"),
    ]:
        assert f"| `hardware_tests/test_dummy.py::{name}` | {result} | " in md
    cases = ET.parse(out / "report.xml").getroot().iter("testcase")
    assert sorted(c.get("name", "") for c in cases) == [
        "test_fail",
        "test_pass",
        "test_skip",
        "test_xfail",
    ]


def test_hook_never_blocks(repo):
    """A failing test never blocks the commit, and the report is part of that same
    commit."""
    touch_test_file(repo)
    r = commit(repo, "change a test", hook_env(GTE_HOOK_HARDWARE="present"))
    assert r.returncode == 0, r.stderr
    assert git(repo, "log", "-1", "--format=%s").stdout.strip() == "change a test"
    files = committed_files(repo)
    assert "hardware_tests/REPORT.md" in files
    assert "hardware_tests/report.xml" in files
    assert "1 failed" in (repo / "hardware_tests/REPORT.md").read_text()


def test_hook_skips_docs_only(repo):
    """A commit that stages only docs runs nothing and leaves the report alone."""
    touch_test_file(repo)
    commit(repo, "first", hook_env(GTE_HOOK_HARDWARE="present"))
    before = (repo / "hardware_tests/REPORT.md").read_text()
    (repo / "docs/notes.md").write_text("docs only\n")
    git(repo, "add", "docs/notes.md")
    r = commit(repo, "docs", hook_env(GTE_HOOK_HARDWARE="present"))
    assert r.returncode == 0, r.stderr
    assert committed_files(repo) == ["docs/notes.md"]
    assert (repo / "hardware_tests/REPORT.md").read_text() == before


def test_hook_no_hardware(repo):
    """With no STLINK-V3 the commit still goes through and the report says why nothing
    ran."""
    touch_test_file(repo)
    r = commit(repo, "no board", hook_env(GTE_HOOK_HARDWARE="absent"))
    assert r.returncode == 0, r.stderr
    assert (
        "| Result | not run: no hardware attached |"
        in (repo / "hardware_tests/REPORT.md").read_text()
    )
    assert "hardware_tests/REPORT.md" in committed_files(repo)


def test_hook_skip_env(repo):
    """GTE_SKIP_TESTS=1 skips the suite on purpose, and the report says so."""
    touch_test_file(repo)
    r = commit(repo, "skip", hook_env(GTE_HOOK_HARDWARE="present", GTE_SKIP_TESTS="1"))
    assert r.returncode == 0, r.stderr
    assert (
        "| Result | not run: skipped by GTE_SKIP_TESTS |"
        in (repo / "hardware_tests/REPORT.md").read_text()
    )
    assert "hardware_tests/REPORT.md" in committed_files(repo)


def test_spikes_not_collected():
    """The suite never collects the bring-up spikes."""
    assert list(
        (REPO / "bringup").rglob("test_*.py")
    ), "expected the spikes to still exist under bringup/"
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "--collect-only",
            "-q",
            "hardware_tests",
            "-p",
            "no:cacheprovider",
        ],
        cwd=REPO,
        capture_output=True,
        text=True,
        env=clean_env(),
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "bringup" not in r.stdout
    assert "test_boot.py" in r.stdout


def python_files() -> list[str]:
    tracked = subprocess.run(
        ["git", "-C", str(REPO), "ls-files", "*.py"],
        capture_output=True,
        text=True,
        env=clean_env(),
    ).stdout.split()
    # Vendored third-party code (TinyUSB) is not ours to reformat.
    ours = [f for f in tracked if "tinyusb" not in f]
    return ours + [".githooks/pre-commit"]


def test_python_style():
    """All of the repo's Python is black-formatted and passes flake8 (max line 88)."""
    files = python_files()
    venv_bin = Path(sys.executable).parent
    black = subprocess.run(
        [str(venv_bin / "black"), "--check", "--quiet", *files],
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    assert black.returncode == 0, f"not black-formatted:\n{black.stderr}"
    flake8 = subprocess.run(
        [str(venv_bin / "flake8"), *files], cwd=REPO, capture_output=True, text=True
    )
    assert flake8.returncode == 0, f"flake8:\n{flake8.stdout}"
    pyright = subprocess.run(
        [str(venv_bin / "pyright")], cwd=REPO, capture_output=True, text=True
    )
    assert pyright.returncode == 0, f"pyright (standard):\n{pyright.stdout}"


# Third-party C++ kept as delivered: STM32duino's generic variant files, the
# FreeRTOS config copy (its name is fixed by the library) and vendored TinyUSB.
THIRD_PARTY_CPP = (
    "variants/p0004_pcb_001_v1/variant_generic.",
    "variants/p0004_pcb_001_v1/PeripheralPins",
    "variants/p0004_pcb_001_v1/PinNamesVar",
    "include/STM32FreeRTOSConfig.h",
    "tinyusb",
    "bringup/",
)


def cpp_files() -> list[str]:
    tracked = subprocess.run(
        ["git", "-C", str(REPO), "ls-files", "*.cc", "*.cpp", "*.h", "*.c"],
        capture_output=True,
        text=True,
        env=clean_env(),
    ).stdout.split()
    return [f for f in tracked if not any(p in f for p in THIRD_PARTY_CPP)]


SCPI_BLOCK = re.compile(r"/\*\*\s*@scpi\b.*?\*/", re.S)
CPPLINT_LINE = re.compile(r"^(.+?):(\d+):\s")


def scpi_block_lines(path: str) -> set[int]:
    """Line numbers inside /** @scpi ... */ blocks, which are exempt from linting:
    the SCPI manual generator reads them as written (user decision, issue #43)."""
    text = (REPO / path).read_text()
    lines: set[int] = set()
    for m in SCPI_BLOCK.finditer(text):
        first = text.count("\n", 0, m.start()) + 1
        last = text.count("\n", 0, m.end()) + 1
        lines.update(range(first, last + 1))
    return lines


def test_cpp_style():
    """Our C++ follows Google style: clang-format (Google) and a clean cpplint.
    Anything inside an @scpi documentation block is exempt."""
    files = cpp_files()
    assert "lib/gte_core/src/gte/gte.cc" in files
    venv_bin = Path(sys.executable).parent
    lint = subprocess.run(
        [str(venv_bin / "cpplint"), "--quiet", *files],
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    exempt = {f: scpi_block_lines(f) for f in files}
    findings = []
    for line in lint.stderr.splitlines():
        m = CPPLINT_LINE.match(line)
        if not m or int(m.group(2)) not in exempt.get(m.group(1), set()):
            if not line.startswith(("Done processing", "Total errors")):
                findings.append(line)
    assert not findings, "cpplint:\n" + "\n".join(findings)
    fmt = subprocess.run(
        [str(venv_bin / "clang-format"), "--dry-run", "-Werror", *files],
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    assert fmt.returncode == 0, f"clang-format (Google):\n{fmt.stderr[:3000]}"
