"""pytest plugin: writes REPORT.md, the human-readable summary of a hardware test run.

Loaded by hardware_tests/conftest.py. Enabled with --gte-report DIR (the pre-commit hook passes it).
The JUnit file (report.xml) comes from pytest's own --junitxml.

Fixtures fill META (firmware version, board serial, fitted Boards). Anything left unset is
reported as "unknown". The hook also uses write_not_run() so a skipped run produces the same
header in the same format.
"""
import datetime
import hashlib
import subprocess
import time
from pathlib import Path

REPORT_NAME = 'REPORT.md'
XML_NAME = 'report.xml'
# The report files themselves are excluded from the staged/unstaged checks.
_EXCLUDE = [f':(exclude)hardware_tests/{REPORT_NAME}', f':(exclude)hardware_tests/{XML_NAME}']

META: dict = {}
_results: dict = {}
_start = [0.0]


def _git(root: Path, *args: str) -> str:
    r = subprocess.run(['git', '-C', str(root), *args], capture_output=True)
    return r.stdout.decode('utf-8', 'replace').strip() if r.returncode == 0 else ''


def git_state(root: Path) -> dict:
    """What is about to be committed, relative to the parent commit."""
    parent = _git(root, 'rev-parse', '--short=12', 'HEAD') or 'none (first commit)'
    staged = subprocess.run(['git', '-C', str(root), 'diff', '--cached', '--binary', '--', '.', *_EXCLUDE],
                            capture_output=True).stdout
    staged_hash = 'sha256:' + hashlib.sha256(staged).hexdigest()[:16] if staged else 'none'
    unstaged = bool(_git(root, 'diff', '--name-only', '--', '.', *_EXCLUDE)) or \
        bool(_git(root, 'ls-files', '--others', '--exclude-standard', '--', '.', *_EXCLUDE))
    return {'parent': parent, 'staged': staged_hash, 'unstaged': 'yes' if unstaged else 'no'}


def _header(root: Path, result: str, duration: float | None) -> list[str]:
    g = git_state(root)
    now = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    rows = [
        ('Result', result),
        ('Parent commit', g['parent']),
        ('Staged changes', g['staged']),
        ('Unstaged changes in working tree', g['unstaged']),
        ('Firmware version', META.get('firmware_version', 'unknown')),
        ('Board serial (STM32 UID)', META.get('board_serial', 'unknown')),
        ('IO Board', META.get('io_board', 'unknown')),
        ('Instrument Board', META.get('instrument_board', 'unknown')),
        ('Run', now + (f', {duration:.1f} s' if duration is not None else '')),
    ]
    rows += [(k, v) for k, v in META.get('extra', {}).items()]
    return ['# Hardware test report', '', '| | |', '|---|---|'] + [f'| {k} | {v} |' for k, v in rows]


def write_not_run(report_dir: Path, root: Path, reason: str) -> Path:
    lines = _header(root, f'not run: {reason}', None)
    p = Path(report_dir) / REPORT_NAME
    p.write_text('\n'.join(lines) + '\n')
    return p


def pytest_addoption(parser):
    parser.addoption('--gte-report', metavar='DIR', default=None, help='write REPORT.md into DIR')


def pytest_sessionstart(session):
    _start[0] = time.time()
    _results.clear()


def pytest_runtest_logreport(report):
    r = _results.setdefault(report.nodeid, {'outcome': None, 'duration': 0.0})
    r['duration'] += report.duration
    if report.when == 'call' or report.outcome != 'passed':
        if hasattr(report, 'wasxfail'):
            outcome = 'xfailed' if report.outcome == 'skipped' else 'xpassed'
        elif report.when != 'call' and report.outcome == 'failed':
            outcome = 'error'
        else:
            outcome = report.outcome
        if r['outcome'] in (None, 'passed'):
            r['outcome'] = outcome


def pytest_sessionfinish(session, exitstatus):
    out = session.config.getoption('--gte-report')
    if not out:
        return
    counts: dict = {}
    for r in _results.values():
        counts[r['outcome']] = counts.get(r['outcome'], 0) + 1
    order = ['passed', 'failed', 'error', 'skipped', 'xfailed', 'xpassed']
    summary = ', '.join(f'{counts[k]} {k}' for k in order if counts.get(k)) or 'no tests'
    root = Path(str(session.config.rootpath))
    top = _git(root, 'rev-parse', '--show-toplevel')
    lines = _header(Path(top) if top else root, summary, time.time() - _start[0])
    lines += ['', '## Tests', '', '| Test | Result | Time |', '|---|---|---|']
    for nodeid, r in _results.items():
        lines.append(f'| `{nodeid}` | {r["outcome"]} | {r["duration"]:.2f} s |')
    Path(out).mkdir(parents=True, exist_ok=True)
    (Path(out) / REPORT_NAME).write_text('\n'.join(lines) + '\n')
