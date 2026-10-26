#!/usr/bin/env python3
"""Run the shipping relay deferral state machine without a lobby service."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'network/netplay/netplay_frontend.c').read_text(encoding='utf-8')
names = ['netplay_mitm_query_ready', 'netplay_mitm_query_result',
         'netplay_mitm_query_have', 'netplay_mitm_query',
         'netplay_host_setup_handler', 'netplay_host_setup_cleanup',
         'netplay_host_setup_finder', 'netplay_host_setup_defer']
fragments = [next(line for line in source.splitlines()
                  if line.startswith('#define NETPLAY_MITM_QUERY_TIMEOUT '))]
for name in names:
    for prefix in ('static bool ', 'static void ', 'bool '):
        token = '\n' + prefix + name + '('
        if token in source:
            start = source.index(token) + 1
            break
    else:
        raise RuntimeError('Missing function: ' + name)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    fragments.append(source[start:end])

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'host_setup.inc').write_text('\n\n'.join(fragments) + '\n', encoding='utf-8')
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=gnu89 -Wall -Wextra -Werror'))
    libs = shlex.split(os.environ.get('LDFLAGS', ''))
    exe = tmp / ('host_setup_test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'host_setup_test')
    subprocess.run(cc + flags + ['-I' + str(tmp),
                   '-I' + str(root / 'libretro-common/include'),
                   str(Path(__file__).with_name('host_setup_test.c')), '-o', str(exe)] + libs,
                   check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
