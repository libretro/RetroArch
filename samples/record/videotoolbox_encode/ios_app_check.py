#!/usr/bin/env python3
"""Build and link the production iOS application; never report device acceptance."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('full iOS application linking requires macOS/Xcode')
    root = Path(__file__).resolve().parents[3]
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    common = ['xcodebuild', '-project', 'pkg/apple/RetroArch_iOS13.xcodeproj',
              '-scheme', 'RetroArch iOS Release', '-configuration', 'Release',
              '-destination', 'generic/platform=iOS', '-xcconfig', 'pkg/apple/GitHubCI.xcconfig',
              '-derivedDataPath', str(output / 'derived'),
              'CODE_SIGNING_ALLOWED=NO', 'CODE_SIGNING_REQUIRED=NO',
              'CODE_SIGN_IDENTITY=', 'PROVISIONING_PROFILE_SPECIFIER=']
    settings = subprocess.run(common + ['-showBuildSettings', '-json'], cwd=root,
                              check=True, capture_output=True, text=True, timeout=180)
    (output / 'build-settings.json').write_text(settings.stdout)
    rows = json.loads(settings.stdout)
    main_target = next(row['buildSettings'] for row in rows if row['target'] == 'RetroArchiOS')
    flags = shlex.split(main_target['OTHER_CFLAGS'])
    defines = {flag[2:].split('=', 1)[0] for flag in flags if flag.startswith('-D')}
    required = {'HAVE_METAL', 'HAVE_AVF', 'HAVE_THREADS', 'HAVE_CONFIGFILE',
                'HAVE_NETWORKING', 'HAVE_CRYPTO', 'HAVE_RTMP', 'HAVE_RFLV'}
    assert required <= defines, sorted(required - defines)
    assert 'HAVE_FFMPEG' not in defines
    command = common + ['-resultBundlePath', str(output / 'build.xcresult'), 'build']
    report = dict(command=command, required_defines=sorted(required),
                  scope='Full unsigned device application build/link only; no physical playback or recording acceptance',
                  result='FAIL')
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    with (output / 'xcodebuild.log').open('w') as log:
        result = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=3000)
    report['exit_code'] = result.returncode
    if not result.returncode:
        executable = output / 'derived/Build/Products/Release-iphoneos/RetroArch.app/RetroArch'
        assert executable.is_file() and executable.stat().st_size > 0
        linked = subprocess.run(['otool', '-L', str(executable)], check=True,
                                capture_output=True, text=True, timeout=60).stdout
        (output / 'linked-libraries.txt').write_text(linked)
        assert not any(name in linked.lower() for name in ('libavcodec', 'libavformat', 'libavutil', 'libswscale'))
        report.update(result='PASS', executable_bytes=executable.stat().st_size)
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['result'], report['scope'], flush=True)
    return int(bool(result.returncode))


if __name__ == '__main__':
    sys.exit(main())
