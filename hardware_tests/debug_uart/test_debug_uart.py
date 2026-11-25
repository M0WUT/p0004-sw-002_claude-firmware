"""Debug UART (USART2, PD5 TX / PD6 RX) through the STLINK-V3 VCP on CN4.

The firmware streams 'SEQ <n> T=<ms> ...' lines at ~40/s and echoes received lines back as 'ECHO:<text>'.
"""
import re
import time
from pathlib import Path

LINE = re.compile(r'^SEQ (\d{8}) T=(\d{10}) ABCDEFGHIJKLMNOPQRSTU$')
PROJECT = Path(__file__).parent
SECONDS = 10


def test_tx_stream_has_no_gaps(flash, open_uart):
    flash(PROJECT)
    uart = open_uart()
    time.sleep(0.5)
    uart.clear()
    time.sleep(SECONDS)
    lines = uart.text.split('\r\n')[1:-1]  # drop partial first/last lines
    seqs = [int(m.group(1)) for m in map(LINE.match, lines) if m]
    corrupt = [l for l in lines if not LINE.match(l) and not l.startswith('ECHO:')]
    gaps = [(a, b) for a, b in zip(seqs, seqs[1:]) if b != a + 1]
    assert not corrupt, f'corrupt lines: {corrupt[:5]}'
    assert not gaps, f'sequence gaps: {gaps[:10]}'
    assert len(seqs) >= SECONDS * 40 * 0.9, f'only {len(seqs)} lines in {SECONDS} s'


def test_rx_echo(flash, open_uart):
    flash(PROJECT)
    uart = open_uart()
    token = f'GTE{int(time.time()) % 100000:05d}'
    uart.write((token + '\n').encode())
    uart.wait_for(f'^ECHO:{token}', timeout=3)
