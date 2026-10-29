#!/usr/bin/env python3
"""Run the recorder's submission control with portable Apple message stand-ins."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=root / 'record/drivers/record_avfoundation.m')
args = parser.parse_args()
source = args.source.read_text(encoding='utf-8')
fragments = []
for kind, buffer, extra in [('video', 'pixelBuffer', ', int presentationTime'),
                            ('audio', 'sampleBuffer', '')]:
    start = source.index('static bool avfoundation_record_push_' + kind + '(')
    end = source.index('\nstatic bool ', start + 1)
    function = source[start:end].rstrip()
    tail = function[function.index('      /* Append'):]
    declaration = re.search(r'__block bool success\s*=\s*false;', function)
    prefix = ('static bool legacy_' + kind + '_submit(record_avfoundation_t *handle, void *'
              + buffer + extra + ')\n{\n'
              + (declaration.group(0) + '\n' if declaration else '') + '   {\n')
    fragments.append(prefix + tail)
start = source.find('         if (![handle->assetWriter canApplyOutputSettings:audioSettings')
body = ''
if start >= 0:
    body = source[start:source.index('         /* Create audio input */', start)]
fragments.append('static void *legacy_audio_settings(record_avfoundation_t *handle, '
                 'NSMutableDictionary *audioSettings)\n{\n' + body
                 + '\n   return audioSettings;\n}\n')
code = '\n'.join(fragments).replace('__block ', '')
code = code.replace('dispatch_sync(handle->encodingQueue, ^{', '{').replace('});', '}')
for kind in ('video', 'audio'):
    code = code.replace('handle->' + kind + 'Input.readyForMoreMediaData',
                        'handle->' + kind + 'Input->ready')
code = code.replace('handle->assetWriter.status', 'handle->assetWriter->status')
code = re.sub(r'\[handle->pixelBufferAdaptor appendPixelBuffer:pixelBuffer\s+'
              r'withPresentationTime:presentationTime\]',
              'append_video(handle->pixelBufferAdaptor, pixelBuffer, presentationTime)', code)
code = code.replace('[handle->audioInput appendSampleBuffer:sampleBuffer]',
                    'append_audio(handle->audioInput, sampleBuffer)')
code = re.sub(r'\[handle->assetWriter canApplyOutputSettings:audioSettings\s+'
              r'forMediaType:AVMediaTypeAudio\]', 'can_apply(handle->assetWriter, audioSettings)', code)
code = code.replace('[NSMutableDictionary dictionaryWithDictionary:audioSettings]',
                    'copy_settings(audioSettings)')
code = code.replace('[fallback removeObjectForKey:AVEncoderBitRateKey]',
                    'remove_bitrate(fallback, AVEncoderBitRateKey)')
assert not any(token in code for token in ('dispatch_sync', '^{', '[handle', '[fallback', '[NSMutableDictionary'))
compiler = shlex.split(os.environ.get('CC', 'cc'))
flags = shlex.split(os.environ.get('CFLAGS', '-std=c89 -pedantic -Wall -Wextra -Werror'))
with tempfile.TemporaryDirectory(prefix='avf-append-') as temp:
    temp = Path(temp)
    (temp / 'legacy_append.inc').write_text(code, encoding='utf-8')
    suffix = '.exe' if os.name == 'nt' or 'mingw' in compiler[0] else ''
    binary = temp / ('append' + suffix)
    command = compiler + flags + ['-I' + str(temp), '-I' + str(root / 'libretro-common/include'),
                                  str(here / 'legacy_append_test.c'), '-o', str(binary)]
    command += shlex.split(os.environ.get('LDFLAGS', ''))
    subprocess.run(command, cwd=here, check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(binary)],
                   cwd=here, check=True, timeout=60)
