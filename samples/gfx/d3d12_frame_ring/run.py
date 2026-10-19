#!/usr/bin/env python3
"""Run the production frame ring of the D3D12 driver - its slots,
command list recording, slot constants and upload buffer turns -
against a fake queue whose GPU finishes when told to."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/drivers/d3d12.c').read_text()


def function(name):
    """The definition of @name: the first mention whose line, or the one
    before, starts 'static' and that opens a body before any ';'."""
    at = 0
    while True:
        at = source.index(name, at) + 1
        line = source.rindex('\n', 0, at) + 1
        prev = source.rindex('\n', 0, line - 1) + 1
        start = line if source.startswith('static', line) else prev
        if not source.startswith('static', start):
            continue
        opening = source.index('{', at)
        if source.find(';', at, opening) >= 0:
            continue
        depth, end = 1, opening + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end] + '\n'


def define(name):
    start = source.index('#define ' + name)
    end = start
    while True:
        end = source.index('\n', end)
        if source[end - 1] != '\\':
            return source[start:end + 1]
        end += 1


code = ''.join(define(n) for n in ('D3D12_FRAMES ', 'D3D12_CB_STRIDE(',
                                    'D3D12_SPRITES_BASE(', 'D3D12_SPRITES_END('))
for f in ('d3d12_cb_slot(', 'd3d12_cb_write_slot(', 'd3d12_cmd_begin(',
          'd3d12_cmd_execute(', 'd3d12_queue_settle(', 'd3d12_slot_begin(',
          'd3d12_queue_busy(', 'd3d12_release_texture(',
          'd3d12_texture_upload_turn('):
    code += function(f)

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'ring.inc').write_text(code)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    exe = tmp / ('test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'test')
    subprocess.run(cc + flags + ['-I' + str(tmp),
                   str(Path(__file__).with_name('test.c')), '-o', str(exe)],
                   check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)],
                   check=True)
