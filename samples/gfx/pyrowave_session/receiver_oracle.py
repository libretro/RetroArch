# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Offline TCP peer and screenshot oracle for the actual builtin receiver."""
import argparse
import ctypes as c
import math
import os
import re
import signal
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time
from PIL import Image as PngImage

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--codec', type=Path, required=True)
parser.add_argument('--shader-directory', type=Path, required=True)
parser.add_argument('--embedded-shaders', action='store_true')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--threaded', action='store_true')
parser.add_argument('--retained-core', type=Path, help='Prove the selected fixture core resumes after EOF without a reload')
parser.add_argument('--hdr', action='store_true', help='PQ/BT.2020 stream with explicit SDR presentation')
parser.add_argument('--backend', choices=('glcore', 'd3d11', 'd3d12', 'vulkan'), default='glcore')
parser.add_argument('--mode', choices=('playback', 'end', 'abort', 'early', 'missing-shaders', 'reconnect', 'gpu-failure'), default='playback')
parser.add_argument('--history-shader', action='store_true')
parser.add_argument('--require-validation', action='store_true')
parser.add_argument('--require-retirement-marker', action='store_true')
parser.add_argument('--require-d3d12-validation', action='store_true')
parser.add_argument('--require-d3d12-device-removal', action='store_true')
parser.add_argument('--audio-channels', type=int, choices=(0, 1, 2), default=2)
parser.add_argument('--audio-driver', choices=('null', 'wasapi', 'alsa'), default='null')
parser.add_argument('--audio-device', default='', help='Native endpoint override, e.g. ALSA null for a virtual sink')
parser.add_argument('--require-av-clock', action='store_true')
parser.add_argument('--av-video-lead-us', type=int, default=0,
    help='Offline fixture: shift video timestamps ahead of PCM to exercise holding')
args = parser.parse_args()
if args.require_av_clock and (args.audio_driver == 'null' or not args.audio_channels):
    parser.error('--require-av-clock needs a real audio driver and audio channels')
if args.require_d3d12_validation and args.backend != 'd3d12':
    parser.error('--require-d3d12-validation requires --backend d3d12')
output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)

class Sequence(c.Structure):
    _fields_ = [('blocks', c.c_uint32)] + [(name, c.c_uint) for name in
        ('width', 'height', 'sequence', 'chroma', 'primaries', 'transfer', 'transform', 'limited', 'left')]
class Image(c.Structure):
    _fields_ = [('planes', c.POINTER(c.c_float) * 3), ('stride', c.c_size_t), ('sequence', Sequence)]
codec = c.CDLL(str(args.codec.resolve()))
for name in ('encoder', 'decoder'):
    new = getattr(codec, 'rpyrowave_' + name + '_new')
    new.argtypes = [c.c_uint, c.c_uint, c.c_uint, c.c_size_t]; new.restype = c.c_void_p
    getattr(codec, 'rpyrowave_' + name + '_free').argtypes = [c.c_void_p]
codec.rpyrowave_encode_frame.argtypes = [c.c_void_p, c.POINTER(Image), c.c_size_t,
    c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
codec.rpyrowave_decode_frame.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.c_uint, c.POINTER(Image)]
width, height, budget = 130, 98, 1048576
planes = [(c.c_float * (width * height))() for _ in range(3)]
for y in range(height):
    for x in range(width):
        red = .15 + .7 * x / (width - 1)
        green = .1 + .7 * y / (height - 1)
        blue = .25 + .4 * (1 - x / (width - 1)) + .15 * y / (height - 1)
        if args.hdr:
            # Code values span dim SDR, reference white and bright highlights.
            red, green, blue = [.3 + value * .55 for value in (red, green, blue)]
        kr, kb = (.2627, .0593) if args.hdr else (.2126, .0722)
        luma = kr * red + (1 - kr - kb) * green + kb * blue
        planes[0][y * width + x] = luma
        planes[1][y * width + x] = (blue - luma) / (2 * (1 - kb)) + 128 / 255
        planes[2][y * width + x] = (red - luma) / (2 * (1 - kr)) + 128 / 255
source = Image(); source.stride = width
if args.hdr:
    source.sequence.primaries = source.sequence.transfer = source.sequence.transform = 1
source.sequence.width = width; source.sequence.height = height; source.sequence.chroma = 1
for i in range(3): source.planes[i] = c.cast(planes[i], c.POINTER(c.c_float))
encoder = codec.rpyrowave_encoder_new(width, height, 1, 1048576)
decoder = codec.rpyrowave_decoder_new(width, height, 1, 1048576)
assert encoder and decoder
wire = c.create_string_buffer(budget); written = c.c_size_t()
assert codec.rpyrowave_encode_frame(encoder, c.byref(source), budget, wire, budget, c.byref(written)) == 0
packet = wire.raw[:written.value]
reference = Image()
assert codec.rpyrowave_decode_frame(decoder, wire, written.value, 0, c.byref(reference)) == 0

def reference_pixel(x, y):
    x = max(0, min(width - 1, x)); y = max(0, min(height - 1, y))
    index = y * reference.stride + x
    luma, cb, cr = [reference.planes[k][index] for k in range(3)]
    kr, kb = (.2627, .0593) if args.hdr else (.2126, .0722)
    red = luma + (cr - 128 / 255) * (2 * (1 - kr))
    blue = luma + (cb - 128 / 255) * (2 * (1 - kb))
    green = (luma - kr * red - kb * blue) / (1 - kr - kb)
    rgb = [max(0, min(1, value)) for value in (red, green, blue)]
    if args.hdr:
        light = []
        for value in rgb:
            power = value ** (32 / 2523)
            light.append(125 * (max(power - 3424 / 4096, 0)
                / (2413 / 128 - 2392 / 128 * power)) ** (16384 / 2610))
        matrix = ((1.660491002, -.587641139, -.072849863),
                  (-.124550475, 1.132899897, -.008349423),
                  (-.018150763, -.100578898, 1.118729661))
        rgb = [sum(row[k] * light[k] for k in range(3)) for row in matrix]
        peak = max(1, *rgb)
        rgb = [max(0, min(1, value / peak)) for value in rgb]
        rgb = [12.92 * value if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055 for value in rgb]
    return rgb

def verify_screenshot(path):
    image = PngImage.open(path).convert('RGB'); maximum = 0
    for fy in (.12, .31, .53, .76, .9):
        for fx in (.12, .31, .53, .76, .9):
            px, py = int(fx * image.width), int(fy * image.height)
            sx = (px + .5) * width / image.width - .5
            sy = (py + .5) * height / image.height - .5
            ix, iy = math.floor(sx), math.floor(sy); wx, wy = sx - ix, sy - iy
            neighbors = [reference_pixel(ix, iy), reference_pixel(ix + 1, iy),
                         reference_pixel(ix, iy + 1), reference_pixel(ix + 1, iy + 1)]
            expected = [(1 - wy) * ((1 - wx) * neighbors[0][k] + wx * neighbors[1][k])
                        + wy * ((1 - wx) * neighbors[2][k] + wx * neighbors[3][k]) for k in range(3)]
            if args.history_shader: expected = [expected[2], expected[0], expected[1]]
            actual = image.getpixel((px, py))
            for k in range(3):
                error = abs(actual[k] / 255 - expected[k]); maximum = max(maximum, error)
                assert error < .025, (px, py, actual, expected, error)
    print('RECEIVER_SCREENSHOT_CPU_RGB_PASS', image.size, maximum, flush=True)

server = socket.socket(); server.bind(('127.0.0.1', 0)); server.listen(1); server.settimeout(30)
config = output / 'retroarch.cfg'; connection_file = output / 'stream.rpws'; screenshot = output / 'receiver.png'
if screenshot.exists(): screenshot.unlink()
config.write_text('video_driver = "' + args.backend + '"\nvideo_threaded = "' + ('true' if args.threaded else 'false')
    + '"\naudio_driver = "' + args.audio_driver + '"\naudio_volume = "-80"\nvideo_vsync = "true"\nvideo_fullscreen = "false"\n'
    'video_scale = "2"\npause_nonactive = "false"\nvideo_font_enable = "false"\n'
    'config_save_on_exit = "false"\nvideo_shader_enable = "false"\n'
    'video_gpu_screenshot = "true"\nvideo_smooth = "true"\n')
if args.history_shader:
    preset = output / 'receiver-history.slangp'
    shader = Path(__file__).resolve().with_name('receiver_history.slang')
    preset.write_text('shaders = "1"\nshader0 = "' + shader.as_posix() + '"\nfilter_linear0 = "true"\n')
    config.write_text(config.read_text().replace('video_shader_enable = "false"', 'video_shader_enable = "true"'))
if args.audio_device:
    with config.open('a') as extra:
        extra.write('audio_device = \"' + args.audio_device.replace('\\', '/') + '\"\n')
command_port = None
if args.mode == 'reconnect':
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0)); command_port = reservation.getsockname()[1]
    with config.open('a') as extra:
        extra.write('network_cmd_enable = "true"\nnetwork_cmd_bind_address = "127.0.0.1"\n'
            'network_cmd_port = "' + str(command_port) + '"\n')

directory = output / 'missing-shaders' if args.mode == 'missing-shaders' else args.shader_directory.resolve()
connection_file.write_text('pyrowave_url = "rpws://127.0.0.1:' + str(server.getsockname()[1])
    + '"\n' + ('' if args.embedded_shaders and args.mode != 'missing-shaders' else
    'pyrowave_shader_directory = "' + str(directory).replace('\\', '/') + '"\n'))
header = struct.Struct('<4sHBBIIIIQQQQQ'); generation = 7
startup = None
if os.name == 'nt':
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
command = [str(args.retroarch.resolve()), '--verbose', '--config', str(config), '--pyrowave-receive',
           str(connection_file)]
if args.retained_core: command += ['-L', str(args.retained_core.resolve())]
if args.history_shader: command += ['--set-shader', str(preset)]
if args.mode == 'early': command += ['--max-frames', '2']
if args.mode in ('playback', 'reconnect'):
    command += ['--max-frames', '180', '--max-frames-ss', '--max-frames-ss-path', str(screenshot)]
log = (output / 'process.log').open('w')
process = subprocess.Popen(command, cwd=output, stdout=log, stderr=log, startupinfo=startup)
connection = None

def close_own_process():
    if os.name != 'nt':
        process.send_signal(signal.SIGINT)
        return True
    user32 = c.windll.user32
    callback_type = c.WINFUNCTYPE(c.c_int, c.c_void_p, c.c_ssize_t)
    user32.EnumWindows.argtypes = [callback_type, c.c_ssize_t]
    user32.GetWindowThreadProcessId.argtypes = [c.c_void_p, c.POINTER(c.c_ulong)]
    user32.GetClassNameW.argtypes = [c.c_void_p, c.c_wchar_p, c.c_int]
    user32.PostMessageW.argtypes = [c.c_void_p, c.c_uint, c.c_size_t, c.c_ssize_t]
    sent = []
    @callback_type
    def visit(window, unused):
        owner = c.c_ulong(); name = c.create_unicode_buffer(128)
        user32.GetWindowThreadProcessId(window, c.byref(owner))
        user32.GetClassNameW(window, name, len(name))
        if owner.value == process.pid:
            if user32.PostMessageW(window, 0x10, 0, 0): sent.append(True)
        return 1
    user32.EnumWindows(visit, 0)
    return bool(sent)

def receive(size):
    result = bytearray()
    while len(result) < size:
        part = connection.recv(size - len(result))
        if not part: raise EOFError('Receiver closed before READY')
        result.extend(part)
    return bytes(result)

def send_header(kind, flags=0, size=0, total=0, offset=0, serial=0, pts=0, duration=0):
    connection.sendall(header.pack(b'RPWS', 1, kind, flags, size, total, offset, 0,
        generation, serial, pts, duration, 0))

try:
    connection, _ = server.accept(); connection.settimeout(30)
    if args.mode != 'early':
        channels = args.audio_channels; rate = 48000 if channels else 0
        cfg = struct.pack('<16I', width, height, 8, 15 if args.hdr else 1, budget, 60, 1, rate,
            channels, 3 if channels == 2 else 4 if channels else 0, 1 if channels else 0,
            1024 if channels else 0, 0, 0, 0, 0)
        send_header(1, size=64, total=64); connection.sendall(cfg)
        if args.mode == 'missing-shaders':
            try: receive(64)
            except EOFError: pass
            else: raise AssertionError('Missing shaders unexpectedly accepted')
        else:
            ready = header.unpack(receive(64)); assert ready[0:4] == (b'RPWS', 1, 2, 0), ready
            assert ready[8] == generation, ready
            pcm = struct.pack('<' + 'h' * (800 * channels),
                *([4096, -4096] * 800 if channels == 2 else [4096] * 800 if channels else []))
            # GPU-based validation can compile instrumentation on first dispatch.
            count = 2048 if args.mode == 'gpu-failure' else (
                256 if args.mode in ('playback', 'reconnect') else 48)
            for i in range(count):
                if process.poll() is not None: break
                if args.mode == 'reconnect' and i == 12:
                    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as commands:
                        commands.settimeout(10)
                        commands.sendto(b'RESET off\n', ('127.0.0.1', command_port))
                        reply = commands.recv(65536)
                        assert b'ERROR' not in reply, reply
                        print('RECEIVER_RESET_REPLY', reply, flush=True)
                    # The old peer must close before the replacement is admitted.
                    while connection.recv(4096): pass
                    connection.close()
                    connection, _ = server.accept(); connection.settimeout(30)
                    generation += 1
                    send_header(1, size=64, total=64); connection.sendall(cfg)
                    ready = header.unpack(receive(64)); assert ready[0:4] == (b'RPWS', 1, 2, 0), ready
                    assert ready[8] == generation, ready
                    # A new image proves the second session supplies the display.
                    for y in range(height):
                        for x in range(width // 2):
                            for plane in planes:
                                a, b = y * width + x, y * width + width - 1 - x
                                plane[a], plane[b] = plane[b], plane[a]
                    assert codec.rpyrowave_encode_frame(encoder, c.byref(source), budget, wire, budget, c.byref(written)) == 0
                    packet = wire.raw[:written.value]
                    assert codec.rpyrowave_decode_frame(decoder, wire, written.value, 0, c.byref(reference)) == 0
                serial = i - 12 if args.mode == 'reconnect' and i >= 12 else i
                pts = serial * 1000000 // 60; duration = (serial + 1) * 1000000 // 60 - pts
                try:
                    if pcm:
                        send_header(4, size=len(pcm), total=len(pcm), serial=serial * 800, pts=pts, duration=duration)
                        connection.sendall(pcm)
                    for offset in range(0, len(packet), 4096):
                        data = packet[offset:offset + 4096]
                        flags = (1 if not offset else 0) | (2 if offset + len(data) == len(packet) else 0)
                        send_header(3, flags, len(data), len(packet), offset, serial + 1, pts + args.av_video_lead_us, duration)
                        connection.sendall(data)
                except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                    if args.mode not in ('playback', 'reconnect', 'gpu-failure'): raise
                    break
                # Offline sender pacing; production receiver uses device/display pacing.
                time.sleep(.005)
            if args.mode == 'end': send_header(5)
            elif args.mode == 'abort':
                send_header(3, 1, 4096, len(packet), 0, 49, 800000, 16666)
                connection.sendall(packet[:37]); connection.close(); connection = None
    if args.mode in ('end', 'abort', 'missing-shaders', 'gpu-failure'):
        # EOF returns to the existing frontend session. Close only this test's
        # application after the receiver has retired its loans.
        deadline = time.monotonic() + 20; quit_sent = False
        while process.poll() is None and time.monotonic() < deadline:
            log.flush(); text = (output / 'process.log').read_text(errors='replace')
            if (not quit_sent and '[PyroWave] Frontend session restored.' in text
                    and (not args.retained_core or 'FRONTEND_FIXTURE_RUN' in text)):
                closed = close_own_process()
                # Windows may replace its window while returning to the menu.
                quit_sent = closed and os.name != 'nt'
            time.sleep(.02)
    status = process.wait(timeout=20); log.flush()
    diagnostics = (output / 'process.log').read_text(errors='replace')
    assert status == 0, diagnostics[-12000:]
    assert '[PyroWave] Receiver resources retired.' in diagnostics, diagnostics[-12000:]
    if args.audio_driver != 'null':
        assert '[PyroWave] Audio presentation failed' not in diagnostics, diagnostics[-12000:]
        if args.audio_channels and args.mode == 'end':
            assert '[PyroWave] Device-clock audio drained.' in diagnostics, diagnostics[-12000:]
    if args.require_av_clock:
        events = re.findall(r'\[PyroWave AV\] ([CP]) ([0-9a-f]{16}) ([0-9a-f]{16}) ([01])\.', diagnostics)
        presented = [(int(frame, 16), int(ceiling, 16), int(known))
            for kind, frame, ceiling, known in events if kind == 'P']
        waiting = [(int(frame, 16), int(ceiling, 16), int(known))
            for kind, frame, ceiling, known in events if kind == 'C']
        assert presented, 'No native decoded-frame presentation trace'
        assert all(known and frame <= ceiling for frame, ceiling, known in presented), presented
        assert any(not known or frame > ceiling for frame, ceiling, known in waiting), 'No future-frame hold was exercised'
    assert ', Error)]' not in diagnostics and ', Undefined behavior)]' not in diagnostics, diagnostics[-12000:]
    if args.backend == 'glcore':
        assert '[GLCore] GL debugging enabled.' in diagnostics, 'GL debug callback was not confirmed active'
    if args.require_validation:
        assert args.backend == 'vulkan', '--require-validation needs Vulkan'
        assert 'VK_LAYER_KHRONOS_validation' in diagnostics, diagnostics[-12000:]
        active = 'CURRENT-VALIDATION-ENABLED' in diagnostics and '  - Synchronization' in diagnostics
        active |= 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION' in diagnostics
        assert active, diagnostics[-12000:]
        assert 'Validation Error' not in diagnostics and 'SYNC-HAZARD' not in diagnostics, diagnostics[-24000:]
    if args.require_d3d12_validation:
        assert '[PyroWave oracle] D3D12 GPU validation enabled.' in diagnostics, diagnostics[-12000:]
        counts = re.findall(r'\[PyroWave oracle\] D3D12 validation errors: (\d+)\.', diagnostics)
        assert counts and all(int(count) == 0 for count in counts), diagnostics[-24000:]
    if args.mode not in ('early', 'missing-shaders'):
        assert '[PyroWave] Receiver ready.' in diagnostics, diagnostics[-12000:]
    if args.mode in ('end', 'abort', 'missing-shaders'):
        assert '[Environ] SHUTDOWN.' not in diagnostics, diagnostics[-12000:]
        assert '[PyroWave] Frontend session restored.' in diagnostics, diagnostics[-12000:]
    if args.retained_core:
        assert diagnostics.count('FRONTEND_FIXTURE_INIT') == 1, diagnostics[-12000:]
        assert diagnostics.count('FRONTEND_FIXTURE_DEINIT') == 1, diagnostics[-12000:]
        assert diagnostics.index('FRONTEND_FIXTURE_RUN') > diagnostics.index('[PyroWave] Frontend session restored.'), diagnostics[-12000:]
    if args.mode == 'gpu-failure':
        fault = '[PyroWave oracle] Recorded D3D12 decode fault.' if args.backend == 'd3d12' else '[PyroWave oracle] Recorded decode finalization fault.'
        assert fault in diagnostics, diagnostics[-12000:]
    if args.require_d3d12_device_removal:
        assert args.backend == 'd3d12' and args.mode == 'gpu-failure'
        assert '[PyroWave oracle] D3D12 logical device removed.' in diagnostics
        assert '[D3D12] The device was removed:' in diagnostics
        assert '[PyroWave oracle] D3D12 removal acknowledged.' in diagnostics
        assert diagnostics.index('[PyroWave oracle] D3D12 logical device removed.') < diagnostics.index('[PyroWave] Receiver resources retired.')
    if args.require_retirement_marker:
        assert args.mode == 'gpu-failure'
        if args.backend == 'd3d12':
            markers = ('[PyroWave oracle] Recorded D3D12 decode fault.',
                       '[PyroWave oracle] D3D12 retirement marker held.',
                       '[PyroWave oracle] D3D12 retirement marker acknowledged.',
                       '[PyroWave] Receiver resources retired.')
        else:
            markers = ('[PyroWave oracle] Retained unsubmitted decode.',
                       '[PyroWave oracle] Retirement marker held.',
                       '[PyroWave oracle] Retirement marker acknowledged.',
                       '[PyroWave] Receiver resources retired.')
        positions = [diagnostics.index(marker) for marker in markers]
        assert positions == sorted(positions), positions
    if args.mode == 'reconnect':
        assert diagnostics.count('[PyroWave] Receiver ready.') == 2, diagnostics[-12000:]
        assert diagnostics.count('[PyroWave] Receiver resources retired.') == 2, diagnostics[-12000:]
    if args.mode in ('playback', 'reconnect'): verify_screenshot(screenshot)
    print('ACTUAL_RETROARCH_RECEIVER_PASS', args.backend, args.mode,
        'threaded' if args.threaded else 'direct', args.audio_channels, 'channels',
        'PQ/BT.2020 to SDR' if args.hdr else 'SDR', flush=True)
except Exception:
    log.flush()
    print((output / 'process.log').read_text(errors='replace')[-16000:], file=sys.stderr, flush=True)
    raise
finally:
    if connection: connection.close()
    server.close()
    if process.poll() is None: process.kill(); process.wait()
    log.close(); codec.rpyrowave_encoder_free(encoder); codec.rpyrowave_decoder_free(decoder)
