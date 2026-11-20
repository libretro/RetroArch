# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Offline receiver clock-selection trace; no logging on the render path."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-directory', type=Path, required=True)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--present-future', action='store_true', help='Negative fixture: bypass the presentation ceiling')
parser.add_argument('--d3d12-validation', action='store_true')
args = parser.parse_args()
build = args.build_directory.resolve()
core = build / 'record/record_pyrowave_receiver.c'
original = core.read_bytes()
patched = original.replace(b'\r\n', b'\n')
if args.output.resolve() == args.retroarch.resolve():
    parser.error('--output must differ from the configured executable')


def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Expected exactly one trace insertion point: ' + str(old))
    return text.replace(old, new)


marker = b'static bool pyrowave_receiver_due('
trace = b'''typedef struct pyrowave_av_event
{
   uint64_t frame, ceiling;
   unsigned known, kind;
} pyrowave_av_event_t;
static pyrowave_av_event_t pyrowave_av_events[4096];
static unsigned pyrowave_av_count;
static void pyrowave_av_record(pyrowave_receiver_client_t *c, const record_gpu_frame_t *frame, unsigned kind)
{
   if (pyrowave_av_count < 4096)
   {
      pyrowave_av_event_t *event = &pyrowave_av_events[pyrowave_av_count++];
      event->frame = frame->timestamp; event->ceiling = c->target_time;
      event->known = c->target_valid; event->kind = kind;
   }
}
'''
patched = once(patched, marker, trace + marker)
due = b'   return c->target_valid && frame->timestamp <= c->target_time;'
patched = once(patched, due, b'   pyrowave_av_record((pyrowave_receiver_client_t*)c, frame, 0);\n'
    + (b'   return true;' if args.present_future else due))
for name, image in [('gl', 'image'), ('d3d12', 'd3d12_image'), ('vulkan', 'vulkan_image')]:
    future = 'gl_future' if name == 'gl' else name + '_future'
    promote = ('c->' + image + ' = c->' + future + '; c->held = true; c->future = false;').encode()
    patched = once(patched, promote,
        ('pyrowave_av_record(c, &c->' + future + '.frame, 1); ').encode() + promote)
dump = b'''      {
         unsigned av;
         for (av = 0; av < pyrowave_av_count; av++)
         {
            const pyrowave_av_event_t *event = &pyrowave_av_events[av];
            RARCH_LOG("[PyroWave AV] %c %08x%08x %08x%08x %u.\\n",
                  event->kind ? 'P' : 'C', (unsigned)(event->frame >> 32), (unsigned)event->frame,
                  (unsigned)(event->ceiling >> 32), (unsigned)event->ceiling, event->known);
         }
         pyrowave_av_count = 0;
      }
'''
free = b'      free(c->mono); free(c); pyrowave_receiver_client = NULL;'
patched = once(patched, free, dump + free)
try:
    core.write_bytes(patched)
    os.utime(core, None)
    if args.d3d12_validation:
        subprocess.run(['python3', str(Path(__file__).with_name('frontend_d3d12_debug.py')),
            '--build-directory', str(build), '--retroarch', str(args.retroarch.resolve()),
            '--output', str(args.output.resolve())], cwd=build, check=True)
    else:
        subprocess.run(['make', '-j8', 'GL_DEBUG=1'], cwd=build, check=True)
        args.output.resolve().parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.retroarch.resolve(), args.output.resolve())
finally:
    core.write_bytes(original)
    os.utime(core, None)
    subprocess.run(['make', '-j8', 'GL_DEBUG=1'], cwd=build, check=True)
print('PYROWAVE_AV_CLOCK_FIXTURE_BUILT_SOURCE_RESTORED', flush=True)
