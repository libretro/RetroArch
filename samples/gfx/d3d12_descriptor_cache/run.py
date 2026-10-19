#!/usr/bin/env python3
"""Exercise the production SRV cache with recyclable resource addresses."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/drivers/d3d12.c').read_text()
bind = source.index('static void d3d12_chain_bind_pass(')
start = source.index('            d3d12_texture_t* tex =', bind)
end = source.index('\n         }\n', start)
cache = source[start:end]
# The frame's slot's table and cache (D3D12_FRAMES)
start = source.index("      /* The frame's slot's table, and what it holds */", bind)
end = source.index('      while (texture_sem->stage_mask)', start)
select = source[start:end]
teardown = source.index('static void d3d12_chain_free(')
end = source.index('      free(c->pass[i].semantics.textures);', teardown)
start = source.find('      for (k = 0; k < D3D12_FRAMES; k++)', teardown, end)
release = source[start:end] if start >= 0 else ''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp / 'cache.inc').write_text(cache)
    (tmp / 'select.inc').write_text(select)
    (tmp / 'release.inc').write_text(release)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    exe = tmp / ('test.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'test')
    subprocess.run(cc + flags + ['-I' + str(tmp),
                   str(Path(__file__).with_name('test.c')), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
