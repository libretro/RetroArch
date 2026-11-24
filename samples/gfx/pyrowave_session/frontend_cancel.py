# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Offline actual-frontend cancellation before a peer completes negotiation."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--core', type=Path, required=True)
parser.add_argument('--shader-directory', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--backend', choices=('glcore', 'vulkan', 'd3d12'), required=True)
parser.add_argument('--threaded', action='store_true')
parser.add_argument('--missing-shaders', action='store_true')
args = parser.parse_args()
if args.missing_shaders and args.backend != 'glcore':
    parser.error('--missing-shaders requires the asynchronous glcore setup path')
startup = None
if os.name == 'nt':
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
for frames in (2, 4):
    output = args.output.resolve() / str(frames)
    output.mkdir(parents=True, exist_ok=True)
    config, record = output / 'retroarch.cfg', output / 'record.cfg'
    config.write_text('video_driver = "' + args.backend + '"\nrecord_driver = "pyrowave"\n'
        'video_threaded = "' + ('true' if args.threaded else 'false') + '"\n'
        'audio_driver = "null"\npause_nonactive = "false"\nvideo_vsync = "true"\n'
        'video_scale = "2"\nvideo_font_enable = "false"\nconfig_save_on_exit = "false"\n')
    directory = args.shader_directory.resolve()
    if args.missing_shaders:
        directory = output / 'missing-shaders'
        assert not directory.exists(), 'Missing-shader fixture must be absent'
    record.write_text('pyrowave_shader_directory = "' + str(directory).replace('\\', '/')
        + '"\naudio_enable = "true"\n')
    with (output / 'process.log').open('w') as log:
        result = subprocess.run([str(args.retroarch.resolve()), '--verbose', '--config', str(config),
            '-L', str(args.core.resolve()), '--max-frames', str(frames),
            '--record', 'rpws-listen://127.0.0.1:0', '--recordconfig', str(record)],
            cwd=output, stdout=log, stderr=log, startupinfo=startup, timeout=20)
    diagnostics = (output / 'process.log').read_text(errors='replace')
    assert result.returncode == 0, diagnostics[-12000:]
    assert '[PyroWave] Stream negotiating.' in diagnostics, diagnostics[-12000:]
    assert '[PyroWave] Stream resources retired.' in diagnostics, diagnostics[-12000:]
    assert ', Error)]' not in diagnostics and ', Undefined behavior)]' not in diagnostics, diagnostics[-12000:]
    assert 'Validation Error:' not in diagnostics and 'SYNC-HAZARD-' not in diagnostics, diagnostics[-12000:]
    if args.missing_shaders:
        assert '[PyroWave] Native compute capture setup failed.' in diagnostics, diagnostics[-12000:]
    print('ACTUAL_RETROARCH_STARTUP_CANCEL_PASS', args.backend, frames,
        'threaded' if args.threaded else 'direct', 'missing shaders' if args.missing_shaders else 'no peer', flush=True)
