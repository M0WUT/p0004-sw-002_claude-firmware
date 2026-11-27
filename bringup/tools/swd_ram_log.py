#!/usr/bin/env python3
"""Dump the spike's RAM log over SWD. Usage: spike_log.py <firmware.elf>"""
import subprocess, sys, os, re
elf = sys.argv[1]
nm = os.path.expanduser('~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm')
syms = dict((l.split()[2], int(l.split()[0], 16)) for l in subprocess.run([nm, elf], capture_output=True, text=True).stdout.splitlines() if len(l.split()) == 3)
head_addr, ring_addr = syms['spikeLogHead'], syms['spikeLogRing']
oo = os.path.expanduser('~/.platformio/packages/tool-openocd')
size = 16384
cmd = (f'init; halt; echo "HEAD=[read_memory {head_addr:#x} 32 1]"; '
       f'set d [read_memory {ring_addr:#x} 32 {size // 4}]; echo "RING=$d"; resume; shutdown')
out = subprocess.run([oo + '/bin/openocd', '-s', oo + '/openocd/scripts', '-f', 'interface/stlink.cfg',
                      '-f', 'target/stm32h7x.cfg', '-c', cmd], capture_output=True, text=True).stderr
head = int(re.search(r'HEAD=(\S+)', out).group(1), 0)
words = [int(w, 0) for w in re.search(r'RING=(.*)', out).group(1).split()]
ring = b''.join(w.to_bytes(4, 'little') for w in words)
if head <= size:
    data = ring[:head]
else:
    i = head % size
    data = ring[i:] + ring[:i]
print(f'# {head} bytes logged since boot (showing last {len(data)})')
sys.stdout.write(data.decode('ascii', 'replace'))
