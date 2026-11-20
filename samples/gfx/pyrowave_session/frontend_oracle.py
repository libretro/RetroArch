# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Offline actual-frontend/TCP oracle for GPU PyroWave recording."""
import argparse
import ctypes as c
import os
import re
from pathlib import Path
import socket
import struct
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--core', type=Path, required=True)
parser.add_argument('--codec', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--shader-directory', type=Path,
    default=Path(__file__).resolve().parents[3] / 'libretro-common/formats/pyrowave/shaders')
parser.add_argument('--full-output', action='store_true')
parser.add_argument('--embedded-shaders', action='store_true')
parser.add_argument('--audio-driver', default='null')
parser.add_argument('--audio-device', default='')
parser.add_argument('--threaded', action='store_true')
parser.add_argument('--backend', choices=('d3d11', 'd3d12', 'vulkan', 'glcore'), default='d3d12' if os.name == 'nt' else 'vulkan')
parser.add_argument('--require-validation', action='store_true',
                    help='Require active synchronization validation and reject validation errors')
parser.add_argument('--require-d3d12-validation', action='store_true',
                    help='Require native D3D12 GPU validation and zero collected errors')
parser.add_argument('--require-gl-debug', action='store_true',
                    help='Require a GL_DEBUG build with an active driver debug callback')
parser.add_argument('--expect-abort', action='store_true',
                    help='Require a source failure, TCP closure and resource retirement')
parser.add_argument('--disconnect-after', type=int, default=0,
                    help='Abort the peer after this many decoded frames (at least 10)')
args = parser.parse_args()
if args.require_d3d12_validation and args.backend != 'd3d12':
    parser.error('--require-d3d12-validation requires --backend d3d12')
if args.expect_abort and args.disconnect_after:
    parser.error('--expect-abort and --disconnect-after are mutually exclusive')
if args.disconnect_after and args.disconnect_after < 10:
    parser.error('--disconnect-after must be at least 10')
if os.name != 'nt' and args.backend in ('d3d11', 'd3d12'):
    parser.error('The D3D backend requires native Windows')
work = args.output.resolve()
work.mkdir(parents=True, exist_ok=True)
class Sequence(c.Structure):
    _fields_ = [('blocks', c.c_uint32)] + [(n, c.c_uint) for n in
        ['width', 'height', 'sequence', 'chroma', 'primaries', 'transfer', 'transform', 'limited', 'left']]
class Image(c.Structure):
    _fields_ = [('planes', c.POINTER(c.c_float) * 3), ('stride', c.c_size_t), ('sequence', Sequence)]
codec = c.CDLL(str(args.codec.resolve()))
codec.rpyrowave_decoder_new.argtypes = [c.c_uint, c.c_uint, c.c_uint, c.c_size_t]
codec.rpyrowave_decoder_new.restype = c.c_void_p
codec.rpyrowave_decode_frame.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.c_uint, c.POINTER(Image)]
codec.rpyrowave_decoder_free.argtypes = [c.c_void_p]
header = struct.Struct('<4sHBBIIIIQQQQQ')
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as control:
    control.bind(('127.0.0.1', 0)); control_port = control.getsockname()[1]
config = work / 'pyrowave-frontend-test.cfg'
record_config = work / 'pyrowave-frontend-record.cfg'
record_config.write_text(('' if args.embedded_shaders else 'pyrowave_shader_directory = "' + str(args.shader_directory.resolve()).replace('\\', '/') + '"\n') +
    'audio_enable = "true"\npyrowave_maximum_bytes = "262144"\n')
config.write_text('video_driver = "' + args.backend + '"\nrecord_driver = "pyrowave"\nvideo_threaded = "' + ('true' if args.threaded else 'false') + '"\n'
    'audio_driver = "' + args.audio_driver + '"\naudio_enable = "true"\naudio_volume = "-40"\nvideo_vsync = "true"\nvideo_fullscreen = "false"\npause_nonactive = "false"\n'
    'video_scale = "2"\nvideo_font_enable = "false"\nconfig_save_on_exit = "false"\n'
    'savestate_auto_save = "false"\nvideo_shader_enable = "false"\n'
    'confirm_quit = "false"\nmenu_show_start_screen = "false"\nrgui_show_start_screen = "false"\nmenu_ignore_missing_assets = "true"\nmenu_pause_libretro = "false"\n')
with config.open('a') as extra:
    extra.write('pyrowave_full_output = "' + ('true' if args.full_output else 'false') + '"\n')
    if args.full_output:
        extra.write('network_cmd_enable = "true"\nnetwork_cmd_bind_address = "127.0.0.1"\nnetwork_cmd_port = "' + str(control_port) + '"\n')
    if args.audio_device: extra.write('audio_device = "' + args.audio_device + '"\n')
listener = socket.socket()
listener.bind(('127.0.0.1', 0)); listener.listen(1); listener.settimeout(45)
port = listener.getsockname()[1]
startup = None
if os.name == 'nt':
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
env = os.environ.copy()
log = (work / 'native-pyrowave-frontend-process.log').open('w')
command = [str(args.retroarch.resolve()), '--verbose', '--config', str(config),
    '-L', str(args.core.resolve()), '--max-frames', '180',
    '--record', 'rpws://127.0.0.1:' + str(port), '--recordconfig', str(record_config)]
print('Starting actual RetroArch', args.backend, 'frontend', flush=True)
process = subprocess.Popen(command, cwd=work, stdout=log, stderr=log, env=env, startupinfo=startup)
decoder = None
connection = None
def receive(size):
    data = bytearray()
    while len(data) < size:
        part = connection.recv(size - len(data))
        if not part: raise EOFError('Unexpected EOF before END')
        data.extend(part)
    return bytes(data)
try:
    connection, _ = listener.accept(); connection.settimeout(30)
    h = header.unpack(receive(64))
    assert h[0:4] == (b'RPWS', 1, 1, 0), h
    assert h[4:8] == (64, 64, 0, 0), h
    generation = h[8]
    values = struct.unpack('<16I', receive(64))
    width, height, depth, vui, budget, fps_n, fps_d, rate, channels = values[:9]
    assert rate == 48000 and channels == 2 and fps_n == 60 and fps_d == 1, values
    print('CONFIG', width, height, 'audio', rate, channels, flush=True)
    connection.sendall(header.pack(b'RPWS', 1, 2, 0, 0, 0, 0, 0, generation, 0, 0, 0, 0))
    decoder = codec.rpyrowave_decoder_new(width, height, vui & 1, 16777216)
    assert decoder
    mixed_nonzero = False
    frames = audio_records = 0
    audio_end = 0
    payload = bytearray()
    last_video = -1
    deadline = time.monotonic() + 45
    aborted = False
    quit_sent = False
    while time.monotonic() < deadline:
        try:
            h = header.unpack(receive(64))
            data = receive(h[4]) if h[4] <= 16384 else None
        except EOFError:
            if not args.expect_abort: raise
            aborted = True; payload.clear()
            break
        magic, version, kind, flags, size, total, offset, reserved, gen, serial, pts, duration, tail = h
        assert magic == b'RPWS' and version == 1 and gen == generation and not reserved and not tail
        assert size <= 16384 and total <= 16777216
        if kind == 5: break
        if kind == 4:
            count = size // (2 * channels)
            assert serial >= audio_end and pts == serial * 1000000 // rate
            assert duration == (serial + count) * 1000000 // rate - pts
            assert flags == (4 if serial > audio_end else 0)
            samples = struct.unpack('<%dh' % (size // 2), data)
            if args.full_output:
                # The frontend sinc resampler can overshoot square-wave edges.
                assert max(abs(value) for value in samples) < 8192, 'Mixed output exceeded twice the fixture amplitude'
                mixed_nonzero |= any(value != 0 for value in samples)
            else: assert set(samples) == {-4096, 4096}
            audio_end = serial + count; audio_records += 1
        elif kind == 3:
            assert offset == len(payload)
            if offset == 0:
                assert pts >= last_video and total <= budget
                frame_index = (pts * fps_n + 500000 * fps_d) // (1000000 * fps_d)
                assert pts == frame_index * 1000000 * fps_d // fps_n
                assert duration == (frame_index + 1) * 1000000 * fps_d // fps_n - pts
                last_video = pts
            payload.extend(data)
            if flags & 2:
                assert len(payload) == total
                image = Image()
                raw = c.create_string_buffer(bytes(payload))
                assert codec.rpyrowave_decode_frame(decoder, raw, len(payload), 0, c.byref(image)) == 0
                luma = image.planes[0][height // 2 * image.stride + width // 2]
                expected = (0.2126 * 64 + 0.7152 * 128 + 0.0722 * 192) / 255
                assert abs(luma - expected) < 0.03, (luma, expected)
                chroma_index = ((height // 2 if vui & 1 else height // 4) * image.stride
                                + (width // 2 if vui & 1 else width // 4))
                cb, cr = image.planes[1][chroma_index], image.planes[2][chroma_index]
                expected_cb = (192 / 255 - expected) / (2 * (1 - 0.0722)) + 0.5
                expected_cr = (64 / 255 - expected) / (2 * (1 - 0.2126)) + 0.5
                assert abs(cb - expected_cb) < 0.03 and abs(cr - expected_cr) < 0.03, (cb, cr)
                frames += 1; payload.clear()
                if args.disconnect_after and frames >= args.disconnect_after:
                    connection.close(); connection = None
                    break
        else: raise AssertionError(('Unexpected record', h))
        if args.full_output and not quit_sent and frames >= 30 and audio_records >= 30:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as control:
                control.sendto(b'QUIT\n', ('127.0.0.1', control_port))
            quit_sent = True
    else: raise AssertionError('END watchdog expired')
    assert aborted == args.expect_abort, 'Expected source failure did not close TCP'
    assert frames >= 10 and audio_records >= (1 if args.disconnect_after else 10), (frames, audio_records)
    if args.full_output: assert mixed_nonzero, 'Post-mixer capture produced only silence'
    assert not payload
    status = process.wait(timeout=20)
    assert status == 0 or (args.expect_abort and status == 1), status
    log.flush()
    diagnostics = (work / 'native-pyrowave-frontend-process.log').read_text(errors='replace')
    assert '[PyroWave] Stream resources retired.' in diagnostics, 'Frontend exited without retiring source resources'
    assert 'The GPU did not signal its fence' not in diagnostics, diagnostics[-12000:]
    assert 'Validation Error:' not in diagnostics and 'SYNC-HAZARD-' not in diagnostics, diagnostics[-12000:]
    assert ', Error)]' not in diagnostics and ', Undefined behavior)]' not in diagnostics, diagnostics[-12000:]
    if args.require_d3d12_validation:
        assert '[PyroWave oracle] D3D12 GPU validation enabled.' in diagnostics
        counts = re.findall(r'\[PyroWave oracle\] D3D12 validation errors: (\d+)\.', diagnostics)
        assert counts and all(int(count) == 0 for count in counts), diagnostics[-12000:]
    if args.require_gl_debug:
        assert args.backend == 'glcore'
        assert '[GLCore] GL debugging enabled.' in diagnostics, 'GL debugging was not confirmed active'
    if args.require_validation:
        assert args.backend == 'vulkan'
        active = ('CURRENT-VALIDATION-ENABLED' in diagnostics and '  - Synchronization' in diagnostics)
        active |= ('VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION' in diagnostics)
        assert active, 'Synchronization validation was not confirmed active in the process log'
    print('ACTUAL_RETROARCH_' + args.backend.upper() + '_TCP_CAPTURE_PASS', frames, 'frames', audio_records,
          'PCM records', 'threaded' if args.threaded else 'direct',
          'source failure' if args.expect_abort else 'peer abort' if args.disconnect_after else 'END drain', flush=True)
finally:
    if decoder: codec.rpyrowave_decoder_free(decoder)
    if connection: connection.close()
    listener.close()
    if process.poll() is None:
        process.kill(); process.wait()
    log.close()
