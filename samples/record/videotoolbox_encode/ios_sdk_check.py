#!/usr/bin/env python3
"""Compile production VT headers against real iOS SDKs; no device acceptance claim."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def check(sdk, minimum, output):
    root = Path(__file__).resolve().parents[3]
    sdk_path = subprocess.check_output(['xcrun', '--sdk', sdk, '--show-sdk-path'], text=True).strip()
    clang = subprocess.check_output(['xcrun', '--sdk', sdk, '--find', 'clang'], text=True).strip()
    target = 'arm64-apple-ios' + minimum + ('-simulator' if sdk == 'iphonesimulator' else '')
    with tempfile.TemporaryDirectory(prefix='vt-ios-sdk-') as tmp:
        probe = Path(tmp) / 'probe.m'
        probe.write_text('#include "gfx/common/videotoolbox_encode.h"\n'
                         'VT_ENCODE_AVAILABLE void *record_vt_ios_sdk_probe(\n'
                         '      const videotoolbox_encode_config_t *config, OSStatus *status)\n'
                         '{\n   return videotoolbox_encode_new(config, status);\n}\n')
        command = [clang, '-target', target, '-isysroot', sdk_path, '-std=gnu89',
                   '-DHAVE_THREADS', '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
                   '-Wall', '-Wextra', '-Werror', '-Wunguarded-availability-new',
                   '-Wno-unused-function', '-fsyntax-only', str(probe)]
        result = subprocess.run(command, text=True, capture_output=True, timeout=120)
        name = sdk + '-' + minimum
        (output / (name + '.log')).write_text(result.stdout + result.stderr)
        return dict(sdk=sdk, minimum=minimum, target=target, command=command,
                    result='PASS' if not result.returncode else 'FAIL', exit_code=result.returncode)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('real Apple SDK compilation requires macOS/Xcode; no local pass is reported')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = [check(sdk, minimum, args.output_dir)
               for sdk in ('iphoneos', 'iphonesimulator') for minimum in ('13.0', '17.4')]
    (args.output_dir / 'results.json').write_text(json.dumps(dict(
        scope='SDK header compilation only; no native device, recording or streaming acceptance',
        cases=results), indent=2) + '\n')
    for result in results:
        print(result['result'], result['target'], flush=True)
    return int(any(result['exit_code'] for result in results))


if __name__ == '__main__':
    sys.exit(main())
