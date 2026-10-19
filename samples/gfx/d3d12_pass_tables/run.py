#!/usr/bin/env python3
"""Run the production descriptor-table setup of every shader pass: the
tables d3d12_init_descriptors() makes, then both ways a preset is
installed, each against a fake heap that records which pass every slot
was made for."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/drivers/d3d12.c').read_text()


def between(text, start, end, after=0):
    a = text.index(start, after)
    b = text.index(end, a + len(start))
    return text[a:b + len(end)]


alloc = between(source,
                'static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_heap_slot_alloc(',
                '\n   return handle;\n}\n')
alloc += between(source,
                 'static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_heap_range_alloc(',
                 '\n   return handle;\n}\n')
init_at = source.index('static void d3d12_init_descriptors(')
init = between(source, '   for (i = 0; i < GFX_MAX_SHADERS; i++)\n', '\n   }\n', init_at)
# The second slot's tables (D3D12_FRAMES), made after
init += between(source, "   /* The passes' second texture tables", '         : 0;\n', init_at)
# The deferred install: the per-pass loop's descriptor pointers
deferred_at = source.index('/* Install per-pass state */')
deferred = between(source, '         /* Set up descriptor heap pointers */',
                   '\n      }\n', deferred_at)
# d3d12_gfx_set_shader: from the pass's render target view to the end
# of the block that sets its views up
set_shader_at = source.index('static bool d3d12_gfx_set_shader(')
set_shader_at = source.index('if (!d3d12->filter.pass[i].pipe)', set_shader_at)
set_shader = between(source, '#ifdef HAVE_DXGI_HDR\n', '\n      }\n', set_shader_at)

with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'alloc.inc').write_text(alloc)
    (tmp / 'init.inc').write_text(init)
    # Each block without the closing brace of the loop it ends
    (tmp / 'deferred.inc').write_text(deferred[:deferred.rindex('}')])
    (tmp / 'set_shader.inc').write_text(set_shader[:set_shader.rindex('}')])
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    exe = tmp / ('test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'test')
    for hdr in ([], ['-DHAVE_DXGI_HDR']):
        subprocess.run(cc + flags + hdr + ['-I' + str(tmp),
                       str(Path(__file__).with_name('test.c')), '-o', str(exe)],
                       check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)],
                       check=True)
