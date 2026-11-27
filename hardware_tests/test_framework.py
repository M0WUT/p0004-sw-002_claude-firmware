"""The Service framework (Build: Service framework, issue #42), driven through the test Application.

Each test resets the test Application into a scenario (see conftest.run_scenario) and checks what
the firmware does from outside: debug UART lines, and GPIO registers read over SWD.
"""
import re
import time

import pytest

from conftest import BOOT_RE, openocd

pytestmark = pytest.mark.hardware

LOG_RE = re.compile(r'^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3})\t\[([A-Za-z_]+)\]\s*\t'
                    r'(DEBUG|INFO|WARNING|ERROR|CRITICAL)\t(\d*)\t(.*)$')
GPIOA_ODR = 0x58020014


def log_lines(uart):
    """[(host_time, timestamp, source, level, code, message)] for every log-format line."""
    out = []
    for t, line in list(uart.lines):
        m = LOG_RE.match(line)
        if m:
            out.append((t, m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)))
    return out


def ts_ms(ts: str) -> int:
    """Milliseconds since 1970-01-01T00:00:00.000 for a timestamp from the first day after boot."""
    m = re.match(r'1970-01-01T(\d\d):(\d\d):(\d\d)\.(\d{3})$', ts)
    assert m, f'unexpected timestamp {ts}'
    h, mi, s, ms = map(int, m.groups())
    return ((h * 60 + mi) * 60 + s) * 1000 + ms


def slope(xs, ys):
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)


def led() -> tuple[bool, bool, bool]:
    """(red, green, blue) as driven on PA10/PA8/PA9 (active low), read over SWD."""
    out = openocd(f'init; echo "ODR=[read_memory {GPIOA_ODR:#x} 32 1]"; shutdown')
    odr = int(re.search(r'ODR=(\S+)', out).group(1), 0)
    return not odr & (1 << 10), not odr & (1 << 8), not odr & (1 << 9)


def led_samples(count: int, interval_ms: int) -> list[tuple[float, tuple[bool, bool, bool]]]:
    out = openocd(f'init; for {{set i 0}} {{$i < {count}}} {{incr i}} {{ '
                  f'echo "ODR=[read_memory {GPIOA_ODR:#x} 32 1] T=[clock milliseconds]"; sleep {interval_ms} }}; shutdown',
                  timeout=count * interval_ms / 1000 + 30)
    res = []
    for m in re.finditer(r'ODR=(\S+) T=(\d+)', out):
        odr = int(m.group(1), 0)
        res.append((int(m.group(2)) / 1000, (not odr & (1 << 10), not odr & (1 << 8), not odr & (1 << 9))))
    return res


# ---------------------------------------------------------------------------------- lifecycle

def test_initialise_order(run_scenario):
    """Every Service initialises in registration order before the scheduler starts; no tick before."""
    u = run_scenario('normal')
    u.wait_for(r'\[APPLICATION_TEST\].*\ttick 2$', 3)
    lines = [m for _, m in [(t, line) for t, line in list(u.lines)]]
    idx = {}
    for i, line in enumerate(lines):
        for name in ('CORE', 'TEST_AUX', 'APPLICATION_TEST'):
            if re.search(rf'\[{name}\]\s*\tINFO\t\tInitialising$', line):
                idx[name] = i
        if line.endswith('starting the scheduler'):
            idx['scheduler'] = i
        if 'first_tick' not in idx and re.search(r'\t(aux )?tick 1$', line):
            idx['first_tick'] = i
    assert idx['CORE'] < idx['TEST_AUX'] < idx['APPLICATION_TEST'] < idx['scheduler'] < idx['first_tick'], idx


def test_tick_periods(run_scenario):
    """Services tick at their declared periods (100 ms and 250 ms) within ±1 %, by the host clock."""
    u = run_scenario('normal')
    u.wait_for(r'\ttick 2$', 3)
    time.sleep(10.5)
    for regex, period in ((r'\[APPLICATION_TEST\].*\ttick (\d+)$', 0.100), (r'\[TEST_AUX\].*\taux tick (\d+)$', 0.250)):
        hits = u.lines_matching(regex)
        ns = [int(m.group(1)) for _, m in hits]
        assert len(ns) > 10.0 / period * 0.9, f'too few ticks for {regex}'
        assert ns == list(range(ns[0], ns[0] + len(ns))), 'ticks not consecutive'
        s = slope(ns, [t for t, _ in hits])
        assert abs(s - period) / period < 0.01, f'{regex}: period {s * 1000:.2f} ms, expected {period * 1000:.0f} ms'


def test_application_base(run_scenario):
    """The Application derives from ApplicationService and its validate_instrument_board() is called."""
    u = run_scenario('normal')
    u.wait_for(r'\[ServiceHelper\].*\tApplication: APPLICATION_TEST$', 3)
    u.wait_for(r'\[APPLICATION_TEST\].*\tvalidate_instrument_board called$', 3)
    u.wait_for(r'Instrument Board accepted by the Application$', 3)


# ---------------------------------------------------------- Config Parameters and Status Values

def test_config_printed_at_boot(run_scenario):
    """Each Service prints its Config Parameters at boot, with units; secrets are masked."""
    u = run_scenario('normal')
    u.wait_for(r'starting the scheduler$', 3)
    app = [m for _, _, src, _, _, m in log_lines(u) if src == 'APPLICATION_TEST']
    for expected in ['  Log level (logLevel): INFO', '  Count (count): 10', '  Gain (gain): 1.5 dB',
                     '  Enabled (enabled): ON', '  Name (name): GTE', '  Mode (mode): SLOW',
                     '  Password (password): ****', '  Channels (channels): 4 [reset]']:
        assert expected in app, f'missing "{expected}" in {app}'
    assert any(m == '  Log level (logLevel): INFO' for _, _, src, _, _, m in log_lines(u) if src == 'CORE')
    assert 'hunter2' not in u.text, 'a secret was printed'


def test_config_update_path(run_scenario):
    """Generic update path: range, step and custom rules; Live applies at once; Reset needs a restart."""
    u = run_scenario('normal')
    u.wait_for(r'restart required=\d$', 5)
    msgs = [m for _, _, src, _, _, m in log_lines(u) if src == 'APPLICATION_TEST']
    assert 'Config count = 105: rejected: Out of range 0 to 100' in msgs
    assert 'Config count = 12: rejected: Must be a multiple of 5 from 0' in msgs
    assert 'Config gain = 9.0: rejected: Gain above 8 dB needs Enabled OFF' in msgs
    assert 'Side effect for gain' in msgs, 'Live change did not run the side-effect hook'
    assert 'Config gain = 2.5: accepted' in msgs
    assert 'Config channels = 6: accepted (restart required)' in msgs
    assert 'Side effect for channels' not in msgs, 'Reset change applied immediately'
    assert 'Config password = ****: accepted' in msgs
    assert 'newsecret' not in u.text, 'a secret was printed'
    assert 'restart required=1' in msgs


def test_status_values(run_scenario):
    """Typed Status Values carry id, label, unit and display hint, with the exact value kept."""
    u = run_scenario('normal')
    u.wait_for(r'Status label \(Label\)', 5)
    msgs = [m for _, _, src, _, _, m in log_lines(u) if src == 'APPLICATION_TEST']
    assert 'Status dataUsed (Data used): 1.2 GiB [exact 1288490188]' in msgs
    assert 'Status temperature (Temperature): 23.5 °C [exact 23.5]' in msgs
    assert 'Status ready (Ready): Yes [exact 1]' in msgs
    assert 'Status label (Label): TEST [exact TEST]' in msgs


# ------------------------------------------------------------------------------ logging and time

def test_log_line_format(run_scenario):
    """Every log line is: timestamp, [Service], severity, Error Code (or blank), message."""
    u = run_scenario('normal')
    u.wait_for(r'\ttick 5$', 3)
    raw = [line for _, line in list(u.lines) if line.strip()]
    boot = next(i for i, line in enumerate(raw) if re.match(BOOT_RE, line))
    body = [line for line in raw[boot:] if not line.startswith('GTE ')]
    assert len(body) > 20
    bad = [line for line in body if not LOG_RE.match(line)]
    assert not bad, f'lines not in log format: {bad[:5]}'


def test_log_levels(run_scenario):
    """Per-Service Log levels filter messages; Helpers follow CORE; Live changes apply at once."""
    u = run_scenario('log_levels')
    u.wait_for(r'\[APPLICATION_TEST\].*\tTEST_AUX logLevel -> WARNING: accepted$', 5)
    time.sleep(2)
    lines = log_lines(u)
    core_change = next(i for i, l in enumerate(lines) if l[2] == 'CORE' and l[5] == 'Config logLevel = DEBUG: accepted')
    aux_change = next(i for i, l in enumerate(lines) if l[5] == 'TEST_AUX logLevel -> WARNING: accepted')
    helper_debug = [i for i, l in enumerate(lines) if l[2] == 'Watchdog' and l[3] == 'DEBUG']
    assert helper_debug, 'Helper DEBUG lines never appeared after CORE went to DEBUG'
    assert min(helper_debug) > core_change, 'Helper DEBUG lines appeared while CORE was at INFO'
    assert not [l for l in lines if l[2] == 'TEST_AUX' and l[3] == 'DEBUG'], 'TEST_AUX DEBUG lines at level INFO'
    aux_ticks = [i for i, l in enumerate(lines) if l[2] == 'TEST_AUX' and l[5].startswith('aux tick')]
    assert aux_ticks and max(aux_ticks) < aux_change, 'TEST_AUX INFO lines after its level went to WARNING'
    assert any(i < aux_change for i in aux_ticks)


def test_time_from_boot(run_scenario):
    """Timestamps start at 1970-01-01T00:00:00 at boot and track real time within 0.5 %."""
    u = run_scenario('normal')
    u.wait_for(r'\ttick 2$', 3)
    time.sleep(10)
    lines = [(t, ts) for t, ts, src, _, _, m in log_lines(u) if src == 'APPLICATION_TEST' and m.startswith('tick ')]
    assert ts_ms(lines[0][1]) < 3000, f'first tick at {lines[0][1]}, expected within 3 s of boot'
    s = slope([ts_ms(ts) / 1000 for _, ts in lines], [t for t, _ in lines])
    assert abs(s - 1) < 0.005, f'device clock runs at {s:.4f} x real time'


# ----------------------------------------------------------------------- Warnings and Error Codes

def wait_active(u, n, count=1):
    u.wait_for(rf'(?s)(active warnings={n}\n.*){{{count}}}', 6)


def test_error_code_numbering(run_scenario):
    """Codes are 100 x (ServiceId + 1) + n with the header's message and severity."""
    u = run_scenario('warnings')
    u.wait_for(r'active warnings=1$', 5)
    w = [l for l in log_lines(u) if l[3] == 'WARNING']
    assert w, 'no Warning line'
    _, _, src, level, code, msg = w[0]
    assert (src, level, code, msg) == ('APPLICATION_TEST', 'WARNING', str(100 * (13 + 1) + 1), 'Test warning 1: first')


def test_warning_raise(run_scenario):
    """A raise logs code, message and supplementary text, and turns the LED amber."""
    u = run_scenario('warnings')
    u.wait_for(r'\ttick 2$', 3)
    assert led() == (False, False, False), 'LED not off before any Warning'
    u.wait_for(r'active warnings=1$', 5)
    assert led() == (True, True, False), 'LED not amber with a Warning active'


def test_warning_update_in_place(run_scenario):
    """Raising an active code again updates it, with no new entry."""
    u = run_scenario('warnings')
    u.wait_for(r'\ttick 11$', 5)
    msgs = [(l[4], l[5]) for l in log_lines(u) if l[2] == 'APPLICATION_TEST']
    assert ('1401', 'Test warning 1: second') in msgs
    assert ('1401', 'Warning updated (already active)') in msgs
    counts = [m for c, m in msgs if m.startswith('active warnings=')]
    assert counts[:2] == ['active warnings=1', 'active warnings=1'], counts


def test_warning_clear(run_scenario):
    """Clearing by code removes it; with none left the LED goes off."""
    u = run_scenario('warnings')
    u.wait_for(r'\ttick 26$', 6)
    msgs = [(l[4], l[5]) for l in log_lines(u) if l[2] == 'APPLICATION_TEST']
    assert ('1401', 'Warning cleared') in msgs and ('1402', 'Warning cleared') in msgs
    counts = [m for c, m in msgs if m.startswith('active warnings=')]
    assert counts == ['active warnings=1', 'active warnings=1', 'active warnings=2', 'active warnings=1',
                      'active warnings=0'], counts
    assert led() == (False, False, False), 'LED not off once all Warnings cleared'


def test_nine_warnings_fatal(run_scenario):
    """A 9th active Warning is a Fatal Error."""
    u = run_scenario('nine_warnings')
    u.wait_for(r'\[CORE\]\s*\tERROR\t101\tMore than 8 Warnings active: new Warning 1409$', 5)


# ---------------------------------------------------------------------------------- Fatal Errors

def test_fatal_error(run_scenario):
    """Printed with code and Service, repeated every 2 s, other tasks stop, red LED blinks at 0.5 Hz,
    and the board never reboots (10 s > the 4 s IWDG timeout, so the fatal loop feeds it)."""
    u = run_scenario('fatal')
    u.wait_for(r'\[APPLICATION_TEST\]\s*\tERROR\t1450\tTest fatal error: boom$', 5)
    t0 = time.time()
    samples = led_samples(16, 250)
    time.sleep(max(0, 10.5 - (time.time() - t0)))
    lines = [line for _, line in list(u.lines)]
    fatal_at = next(i for i, l in enumerate(lines) if '\tTest fatal error: boom' in l)
    after = lines[fatal_at + 1:]
    assert not [l for l in after if re.match(BOOT_RE, l)], 'the board rebooted'
    assert not [l for l in after if re.search(r'\t(aux )?tick \d+$', l)], 'other tasks kept running'
    repeats = u.lines_matching(r'^FATAL .*\tTest fatal error: boom$')
    assert 4 <= len(repeats) <= 6, f'{len(repeats)} repeats in 10 s'
    gaps = [b[0] - a[0] for a, b in zip(repeats, repeats[1:])]
    assert all(1.8 < g < 2.2 for g in gaps), f'repeat gaps {gaps}'
    assert all(not g and not b for _, (r, g, b) in samples), 'green or blue lit in the fatal loop'
    reds = [r for _, (r, _, _) in samples]
    assert any(reds) and not all(reds), f'red not blinking: {reds}'
    changes = [samples[i][0] for i in range(1, len(samples)) if samples[i][1][0] != samples[i - 1][1][0]]
    gaps = [b - a for a, b in zip(changes, changes[1:])]
    assert gaps and all(0.75 < g < 1.25 for g in gaps), f'red toggles every {gaps} s, expected 1 s (0.5 Hz)'


def test_stack_overflow_fatal(run_scenario):
    """An overflowing task is a Fatal Error naming its Service."""
    u = run_scenario('stack_overflow')
    u.wait_for(r'\[CORE\]\s*\tERROR\t102\tTask stack overflow: TEST_AUX$', 8)


# -------------------------------------------------------------------------------------- watchdog

def test_watchdog_quiet_when_healthy(run_scenario):
    """20 s of normal running, no reset."""
    u = run_scenario('normal')
    time.sleep(20)
    assert len(u.lines_matching(BOOT_RE)) == 1, 'the board reset'
    ticks = u.lines_matching(r'\[APPLICATION_TEST\].*\ttick (\d+)$')
    assert int(ticks[-1][1].group(1)) > 190


def test_watchdog_resets_hung_task(run_scenario):
    """A task that stops checking in resets the board; the next boot reports a watchdog reset."""
    u = run_scenario('hang')
    u.wait_for(r'\[TEST_AUX\].*\tHanging on purpose$', 5)
    t0 = time.time()
    u.wait_for(r'\[Watchdog\]\s*\tERROR\t\tTEST_AUX missed its check-in; resetting$', 4)
    u.wait_for(r'(?s)Hanging on purpose.*GTE boot env=', 8)
    assert time.time() - t0 < 8
    u.wait_for(r'(?s)Hanging on purpose.*GTE reset reason=watchdog\n', 2)
    u.wait_for(r'(?s)Hanging on purpose.*Test scenario 0\b', 5)


def test_watchdog_frozen_under_debugger(run_scenario):
    """Halting the core over SWD for 6 s (longer than the 4 s timeout) causes no reset."""
    u = run_scenario('normal')
    u.wait_for(r'\ttick 5$', 3)
    openocd('init; halt; sleep 6000; resume; shutdown', timeout=30)
    n_before = len(u.lines_matching(r'\ttick \d+$'))
    time.sleep(5)
    assert len(u.lines_matching(BOOT_RE)) == 1, 'the board reset while halted'
    assert len(u.lines_matching(r'\ttick \d+$')) > n_before + 20, 'ticks did not resume'
