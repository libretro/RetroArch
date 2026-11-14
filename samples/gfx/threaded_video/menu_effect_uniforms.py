#!/usr/bin/env python3
"""Check shipping ribbon uniform uploads against the shader layouts."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


failed = False
with tempfile.TemporaryDirectory(prefix='retroarch-effect-uniforms-') as directory:
    tmp = Path(directory)
    for api in ('gl3', 'd3d10', 'd3d11'):
        source = (root / f'gfx/drivers/{api}.c').read_text(encoding='utf-8')
        pipeline = function(source, f'static void gfx_display_{api}_draw_pipeline(')
        if api == 'gl3':
            start = pipeline.index('#ifdef HAVE_SLANG\n   else\n')
            block = pipeline[pipeline.index('   {', start):pipeline.index('#endif', start)]
            declarations = ''
        else:
            declarations = re.search(
                r'typedef struct ALIGN\(16\)\n\{.*?\n\} ' + api + '_uniform_t;',
                source, re.S)[0] + '\ntypedef ' + api + '_uniform_t uniform_t;\n'
            start = pipeline.index(f'   {api}->ubo_values.time =')
            block = pipeline[start:pipeline.index('\n   {', start)]
            block = block.replace(api + '->', 'driver->')
        (tmp / 'uniforms.inc').write_text(declarations, encoding='utf-8')
        (tmp / 'upload.inc').write_text(block, encoding='utf-8')
        exe = tmp / (api + '.exe')
        cc = shlex.split(os.environ.get('CC', 'cc'))
        flags = ['-std=c89', '-Wall', '-Wextra', '-Werror', '-Wframe-larger-than=4096',
                 '-DRARCH_INTERNAL', '-DTEST_GL3=' + str(int(api == 'gl3')),
                 '-I' + str(tmp), '-I' + str(root),
                 '-I' + str(root / 'libretro-common/include')]
        flags += shlex.split(os.environ.get('CFLAGS', ''))
        subprocess.run(cc + flags + [str(Path(__file__).with_suffix('.c')),
                       '-o', str(exe)], check=True)
        result = subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)])
        print(api + ': ' + ('FAIL' if result.returncode else 'PASS'), flush=True)
        failed |= result.returncode != 0
raise SystemExit(int(failed))
