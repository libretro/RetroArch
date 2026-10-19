#!/usr/bin/env python3
"""Run the shipping GLES loan implementation against a scripted GL."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'gfx/common/gl_common.c').read_text()
start = source.index('#ifndef GL_MAP_PERSISTENT_BIT')
end = source.index('#endif /* HAVE_GL_TEXTURE_LEND */', start)
with tempfile.TemporaryDirectory(prefix='retroarch-gles-lend-') as directory:
    temp = Path(directory)
    (temp / 'lend.inc').write_text(source[start:end] + '\n#endif\n')
    cc = shlex.split(os.environ.get('CC', 'cc'))
    exe = temp / ('lend.exe' if os.name == 'nt' or 'mingw' in cc[0] else 'lend')
    flags = ['-std=gnu89', '-O1', '-g1', '-Wall', '-Wextra', '-Werror',
             '-Werror=declaration-after-statement']
    flags += shlex.split(os.environ.get('CFLAGS', ''))
    subprocess.run(cc + flags + ['-I' + str(temp),
        '-I' + str(root / 'libretro-common/include'),
        '-I' + str(root / 'gfx/common'),
        str(Path(__file__).with_name('lend_test.c')), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
