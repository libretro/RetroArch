#!/usr/bin/env python3
"""The KMS context's present_immediate, cut from gfx/drivers_context/
drm_ctx.c: it must say a present tears exactly when the flip it queues
is an async one."""
from pathlib import Path
import os, re, shlex, subprocess, tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
source = (root / 'gfx/drivers_context/drm_ctx.c').read_text()


def block(text, name):
    start = text.index(name)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


# The condition the flip itself is queued async under, as an expression
flip = block(source, 'static bool gfx_ctx_drm_queue_flip(')
m = re.search(r'if \(\s*([^{;]*?)\)\s*\{\s*if \(drmModePageFlip\([^;]*?'
              r'DRM_MODE_PAGE_FLIP_ASYNC', flip, re.S)
assert m, 'the async flip condition was not found'
cond = m.group(1)
assert 'drm->' in cond, 'the async flip condition was not found'

with tempfile.TemporaryDirectory(prefix='drm-immediate-') as tmp:
    tmp = Path(tmp)
    (tmp / 'shipping.inc').write_text(
        block(source, 'static bool gfx_ctx_drm_present_immediate(') + '\n'
        + '#define FLIP_IS_ASYNC(drm) (' + ' '.join(cond.split()) + ')\n')
    exe = tmp / 'test'
    cmd = shlex.split(os.environ.get('CC', 'cc')) \
        + shlex.split(os.environ.get('CFLAGS', '')) \
        + ['-std=c89', '-Wall', '-Wextra', '-Werror',
           '-Werror=declaration-after-statement',
           '-I' + str(root / 'libretro-common/include'), '-I' + str(tmp),
           str(here / 'test.c'), '-o', str(exe)]
    subprocess.run(cmd, stdin=subprocess.DEVNULL, check=True)
    subprocess.run([str(exe)], stdin=subprocess.DEVNULL, check=True)
