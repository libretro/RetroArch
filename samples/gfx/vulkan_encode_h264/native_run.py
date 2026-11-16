#!/usr/bin/env python3
"""Native Vulkan cold encoder resources only; no submitted-picture acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native', action='store_true')
parser.add_argument('--artifacts', type=Path, required=True)
args = parser.parse_args()
if not args.native:
    parser.error('--native is required; a real device and validation layer are mandatory')
here = Path(__file__).resolve().parent
repo = here.parents[2]
root = args.artifacts.resolve()
root.mkdir(parents=True, exist_ok=True)
evidence = dict(scope=__doc__, result='running', commands=[], submitted_pictures=0,
                validation_layer='VK_LAYER_KHRONOS_validation required')
evidence['source'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
evidence['dirty'] = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=repo))

def save():
    (root / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')

def run(command, timeout):
    entry = dict(arguments=list(map(str, command)))
    evidence['commands'].append(entry)
    save()
    number = len(evidence['commands'])
    try:
        result = subprocess.run(entry['arguments'], cwd=repo, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        (root / f'{number}.stdout').write_bytes(error.stdout or b'')
        (root / f'{number}.stderr').write_bytes(error.stderr or b'')
        evidence['result'] = entry['result'] = 'TIMEOUT'
        save()
        raise
    (root / f'{number}.stdout').write_bytes(result.stdout)
    (root / f'{number}.stderr').write_bytes(result.stderr)
    entry['exit_code'] = result.returncode
    save()
    return result

compiler = shlex.split(os.environ.get('CC', 'gcc' if os.name == 'nt' else 'cc'))
flags = ['-O1', '-g1', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
         '-Wno-long-long', '-Wframe-larger-than=2048', '-DHAVE_DYLIB',
         '-I' + str(repo / 'libretro-common/include'), '-I' + str(repo / 'gfx/include')]
if 'clang' in subprocess.check_output(compiler + ['--version'], text=True).lower():
    flags += ['-Wno-c99-extensions']
sources = ['samples/gfx/vulkan_encode_h264/native_test.c', 'gfx/common/vulkan_encode_h264.c',
           'gfx/common/vulkan_encode_h264_session.c', 'gfx/common/vulkan_encode_h264_submit.c',
           'gfx/common/vulkan_encode_pool.c', 'libretro-common/dynamic/dylib.c',
           'libretro-common/encodings/encoding_utf.c', 'libretro-common/compat/compat_strl.c']
binary = root / ('native.exe' if os.name == 'nt' else 'native')
result = run(compiler + flags + [str(repo / name) for name in sources] + ['-o', str(binary)]
             + ([] if os.name == 'nt' else ['-ldl']), 120)
if result.returncode:
    evidence['result'] = 'FAIL compile'
    save()
    result.check_returncode()
evidence['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
result = run([binary], 30)
output = result.stdout.decode(errors='replace')
print(output, end='', flush=True)
evidence['result'] = 'UNAVAILABLE' if result.returncode == 77 else 'PASS' if result.returncode == 0 else 'FAIL'
if result.returncode == 0:
    if output.count('PASS native cold H264') != 3 or 'validation_errors=0' not in output:
        evidence['result'] = 'FAIL output contract'
        save()
        raise AssertionError(output)
save()
raise SystemExit(result.returncode)
