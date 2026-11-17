#!/usr/bin/env python3
"""Start/stop frontend streaming on an isolated native Windows desktop."""
import argparse
from contextlib import ExitStack
import array
import json
import math
import os
from pathlib import Path
import re
import socket
import subprocess
import time
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'libretro-common/samples/net/rtmp'))
from tls_relay import TlsRelay


def port(kind):
    with socket.socket(type=kind) as reserve:
        reserve.bind(('127.0.0.1', 0))
        return reserve.getsockname()[1]


def wait_until(test, process, seconds=15, allow_exit=False):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if test():
            return
        status = process.poll()
        if allow_exit and status is not None:
            return
        assert status is None, ('frontend exited early', status)
        time.sleep(.02)
    raise TimeoutError('frontend acceptance condition timed out')


def check(args, threaded):
    root = args.output_dir.resolve() / ('threaded' if threaded else 'direct')
    root.mkdir(parents=True, exist_ok=False)
    command_port, stream_port = port(socket.SOCK_DGRAM), port(socket.SOCK_STREAM)
    config, native = root / 'retroarch.cfg', root / 'stream.cfg'
    received, log_path = root / 'received.flv', root / 'frontend.log'
    url = f'rtmp://127.0.0.1:{stream_port}/live/test'
    native.write_text(f'audio_enable = {str(args.audio).lower()}\nvcodec = "{args.codec}"\nformat = "flv"\n'
                      + (f'video_qp = 24\nvideo_bit_depth = {args.bit_depth}\n' if args.driver == 'vulkan' else 'video_bit_rate = 8000000\n'))
    config.write_text(f'''video_driver = "{'vulkan' if args.driver == 'vulkan' else 'd3d12'}"
video_threaded = "{str(threaded).lower()}"
video_fullscreen = "false"
video_scale = "1.0"
video_vsync = "true"
video_swapchain_bit_depth = "1"
video_hdr_mode = "{args.hdr_mode}"
video_hdr_paper_white_nits = "200.0"
video_hdr_expand_gamut = "0"
video_hdr_scanlines = "0"
video_record_game_only = "false"
video_record_allow_frame_drop = "true"
video_font_enable = "false"
record_driver = "{args.driver}"
video_stream_url = "{url}"
video_stream_config = "{native.as_posix()}"
audio_driver = "null"
audio_format_negotiation = "1"
audio_fastpath_s16 = "false"
menu_driver = "rgui"
menu_show_start_screen = "false"
history_list_enable = "false"
config_save_on_exit = "false"
pause_nonactive = "false"
quit_press_twice = "false"
savestate_auto_save = "false"
savestate_auto_load = "false"
network_cmd_enable = "true"
network_cmd_port = "{command_port}"
''')
    env = dict(os.environ, NATIVE_RECORD_RATE='48000', NATIVE_RECORD_LAYOUT='3')
    pressure = None
    if args.stall_seconds:
        env['NATIVE_RECORD_NOISE'] = '1'
    with ExitStack() as stack, (root / 'receiver.log').open('wb') as receiver_log, log_path.open('wb') as log:
        server = subprocess.Popen([args.ffmpeg, '-v', 'error', '-listen', '1',
                                   '-timeout', '15', '-copyts', '-analyzeduration', '500000',
                                   '-i', url, '-map', '0', '-c', 'copy', '-f', 'flv', str(received)],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                  stderr=receiver_log, creationflags=subprocess.CREATE_NO_WINDOW)
        if args.stall_seconds:
            from slow_peer import SlowPeer
            from stream_faults import listening
            wait_until(lambda: listening(server, stream_port), server)
            slow = stack.enter_context(SlowPeer(stream_port))
            config.write_text(config.read_text().replace(url,
                              f'rtmp://127.0.0.1:{slow.port}/live/test'))
        if args.tls:
            relay = stack.enter_context(TlsRelay(root, stream_port, args.tls))
            tls_url = f'rtmps://localhost:{relay.port}/live/test'
            config.write_text(config.read_text().replace(url, tls_url))
            env['RTMPS_CA'] = str(relay.ca)
        command = [str(args.retroarch.resolve()), '-v', '--config', str(config),
                   '-L', str(args.core.resolve()), '--max-frames', '36000']
        process = subprocess.Popen([str(args.desktop_runner.resolve()), subprocess.list2cmdline(command)],
                                   cwd=root, env=env, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            with socket.socket(type=socket.SOCK_DGRAM) as client:
                client.settimeout(.1)
                target = ('127.0.0.1', command_port)

                def ready():
                    client.sendto(b'GET_STATUS\n', target)
                    try:
                        return b'PLAYING' in client.recv(65536)
                    except (socket.timeout, ConnectionResetError):
                        return False

                wait_until(ready, process)
                client.sendto(b'SET_OPTION streaming on\n', target)
                wait_until(lambda: 'Starting native ' in log_path.read_text(errors='replace'), process)
                def publishing():
                    text = log_path.read_text(errors='replace')
                    assert 'Recording failed' not in text, text[-6000:]
                    return received.exists()

                wait_until(publishing, process, seconds=30)
                time.sleep(2)
                if args.stall_seconds:
                    from slow_peer import frontend_handle, memory
                    handle, close_handle = frontend_handle(process.pid, args.retroarch.name)
                    stack.callback(close_handle)

                    def current_frame():
                        client.sendto(b'GET_OPTION frame_count\n', target)
                        try:
                            reply = client.recv(65536)
                        except (socket.timeout, ConnectionResetError):
                            return None
                        return int(reply.split(b'\t')[1]) if reply.startswith(b'frame_count\t') else None

                    first = [None]

                    def frame_ready():
                        first[0] = current_frame()
                        return first[0] is not None

                    wait_until(frame_ready, process)
                    baseline = memory(handle)
                    slow.pause.set()
                    assert slow.paused.wait(1), 'relay pause was not acknowledged'
                    time.sleep(.2)  # Let the bounded, already-read relay chunk finish.
                    forwarded = slow.forwarded
                    samples = []
                    end = time.monotonic() + args.stall_seconds
                    while time.monotonic() < end:
                        assert process.poll() is None
                        assert slow.error is None, slow.error
                        samples.append(dict(memory(handle), frame=current_frame()))
                        time.sleep(.1)
                    assert slow.forwarded == forwarded, 'paused relay consumed media'
                    latest = next(row['frame'] for row in reversed(samples) if row['frame'] is not None)
                    assert latest - first[0] >= args.stall_seconds * 30, samples
                    for field in ('private', 'working_set'):
                        assert max(row[field] for row in samples) - baseline[field] < 64 * 1024 * 1024, samples
                        tail = [row[field] for row in samples[len(samples) // 2:]]
                        assert max(tail) - min(tail) < 16 * 1024 * 1024, samples
                    (root / 'pressure-memory.json').write_text(json.dumps(samples, indent=2) + '\n')
                    pressure = dict(seconds=args.stall_seconds, frames_during_stall=latest - first[0],
                                    baseline=baseline, stalled_forwarded_bytes=forwarded,
                                    peak_growth={field: max(row[field] for row in samples) - baseline[field]
                                                 for field in ('private', 'working_set')},
                                    late_spread={field: max(row[field] for row in samples[len(samples) // 2:])
                                                 - min(row[field] for row in samples[len(samples) // 2:])
                                                 for field in ('private', 'working_set')})
                    slow.pause.clear()
                    wait_until(lambda: slow.forwarded > forwarded + 65536, process)
                    time.sleep(2)
                if args.lifecycle == 'resize':
                    from window_lifecycle import resize_window
                    resize_window(process.pid, 820, 500)
                elif args.lifecycle == 'reinit':
                    client.sendto(b'VIDEO_REINIT\n', target)
                else:
                    client.sendto(b'SET_OPTION streaming off\n', target)
                wait_until(lambda: 'Recording finalized (' in log_path.read_text(errors='replace'), process)
                baseline = [None]
                # Discard earlier option replies while observing this process's frame count.
                def frame_after(previous):
                    client.sendto(b'GET_OPTION frame_count\n', target)
                    try:
                        reply = client.recv(65536)
                    except (socket.timeout, ConnectionResetError):
                        return False
                    if not reply.startswith(b'frame_count\t'):
                        return False
                    current = int(reply.split(b'\t')[1])
                    if previous[0] is None:
                        previous[0] = current
                        return False
                    return current >= previous[0] + 8

                if args.lifecycle == 'reinit':
                    wait_until(lambda: log_path.read_text(errors='replace').count('[Vulkan] Found GPU #0:') >= 2, process)
                wait_until(lambda: frame_after(baseline), process)

                def quitting():
                    if process.poll() is not None:
                        return True
                    client.sendto(b'QUIT\n', target)
                    return False

                wait_until(quitting, process, allow_exit=True)
            assert process.wait(timeout=15) == 0
            assert server.wait(timeout=15) == 0
        finally:
            for child in (process, server):
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=65)
    text = log_path.read_text(errors='replace')
    assert '[fixture] s16 source, mask 3' in text
    assert not re.search(r'VUID-|SYNC-HAZARD|Validation Error|Recording failed', text), text[-6000:]
    if os.environ.get('VK_LOADER_DEBUG') and args.driver == 'vulkan':
        assert 'Insert instance layer \"VK_LAYER_KHRONOS_validation\"' in text, 'validation layer was not inserted'
    finalized = re.search(r'Recording finalized \((\d+) GPU frames\)', text)
    assert finalized and int(finalized[1]) > 20, text[-5000:]
    assert not list(root.glob('*.mp4')), 'streaming must not create a recording file'
    info = json.loads(subprocess.run([args.ffprobe, '-v', 'error', '-show_streams',
                                     '-show_packets', '-of', 'json', str(received)],
                                    check=True, capture_output=True, timeout=15).stdout)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    audio = next((s for s in info['streams'] if s['codec_type'] == 'audio'), None)
    assert video['codec_name'] == args.codec
    if args.driver == 'vulkan' and args.bit_depth == 10:
        assert video['pix_fmt'] == 'yuv420p10le'
    if args.hdr_mode:
        assert (video['color_primaries'], video['color_transfer'], video['color_space']) == ('bt2020', 'smpte2084', 'bt2020nc')
    assert bool(audio) == args.audio
    if audio:
        assert audio['codec_name'] == 'aac' and audio['sample_rate'] == '48000' and audio['channels'] == 2
    packets = [p for p in info['packets'] if p['stream_index'] == video['index']]
    assert len(packets) == int(finalized[1]), (len(packets), finalized[1])
    pts = [float(p['pts_time']) for p in packets]
    assert abs(pts[0]) < .0011 and all(a < b for a, b in zip(pts, pts[1:]))
    for timestamp in pts:
        frame = round(timestamp * 60000 / 1001)
        assert abs(timestamp - frame * 1001 / 60000) <= .0011, timestamp
    pixels = subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(received), '-map', '0:v:0',
                             '-fps_mode', 'passthrough', '-vf', 'scale=1:1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
                            check=True, capture_output=True, timeout=20).stdout
    assert len(pixels) == len(packets) * 3
    colors = ((255, 32, 32), (32, 255, 32), (32, 32, 255), (192, 192, 192))
    if args.hdr_mode:
        # Independent absolute BT.2020/PQ signal oracle, without display tone mapping.
        matrix = ((.6274040, .3292820, .0433136),
                  (.0690970, .9195400, .0113612),
                  (.0163916, .0880132, .8955950))

        def pq(value):
            power = max(value, 0) ** (2610 / 16384)
            return ((3424 / 4096 + 2413 / 128 * power) /
                    (1 + 2392 / 128 * power)) ** (2523 / 32)

        colors = [tuple(255 * pq(sum(weight * (value / 255) ** 2.4
                                     for weight, value in zip(row, color)) * 200 / 10000)
                        for row in matrix) for color in colors]
    if args.stall_seconds:
        assert all(105 <= value <= 150 for value in pixels), 'noise fixture mean is invalid'
        pressure['largest_recorded_gap_seconds'] = max(b - a for a, b in zip(pts, pts[1:]))
        assert pressure['largest_recorded_gap_seconds'] > .5, 'no actual encoder backpressure observed'
    else:
        seen = set()
        for i in range(0, len(pixels), 3):
            pixel = pixels[i:i + 3]
            errors = [sum(abs(a - b) for a, b in zip(pixel, color)) / 3 for color in colors]
            assert min(errors) < (8 if args.hdr_mode else 12), (list(pixel), errors)
            seen.add(errors.index(min(errors)))
        assert len(seen) >= 3, seen
    if not args.audio:
        result = dict(driver=args.driver, bit_depth=args.bit_depth, hdr_mode=args.hdr_mode, tls=args.tls, lifecycle=args.lifecycle, threaded=threaded, codec=args.codec, video_frames=len(pts), audio=False, pressure=pressure)
        (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print('PASS frontend video-only streaming and retirement: ' + json.dumps(result), flush=True)
        return
    raw = subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(received), '-map', '0:a:0',
                          '-f', 'f32le', '-'], check=True, capture_output=True, timeout=20).stdout
    samples = array.array('f', raw)
    assert len(samples) > 48000 and all(math.isfinite(s) for s in samples)
    finalized_audio = re.search(r'Audio finalized \((\d+) PCM frames\)', text)
    assert finalized_audio
    assert len(samples) // 2 == math.ceil(int(finalized_audio[1]) / 1024) * 1024
    assert abs(len(samples) / 2 / 48000 - (pts[-1] + 1001 / 60000)) < .04
    amplitudes = []
    for channel, frequency in enumerate((440, 660)):
        values = samples[4096:24000][channel::2]
        n = len(values)
        omega = 2 * math.pi * frequency / 48000
        amplitude = 2 * math.hypot(sum(v * math.cos(omega * i) for i, v in enumerate(values)),
                                   sum(v * math.sin(omega * i) for i, v in enumerate(values))) / n
        assert abs(amplitude - .2) < .04, (channel, amplitude)
        amplitudes.append(amplitude)
    result = dict(driver=args.driver, bit_depth=args.bit_depth, hdr_mode=args.hdr_mode, tls=args.tls, lifecycle=args.lifecycle, threaded=threaded, codec=args.codec, video_frames=len(pts), pcm_frames=len(samples) // 2,
                  amplitudes=amplitudes)
    (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS frontend streaming start/stop, A/V and retirement: ' + json.dumps(result), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--retroarch', required=True, type=Path)
    parser.add_argument('--core', required=True, type=Path)
    parser.add_argument('--desktop-runner', required=True, type=Path)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--bit-depth', type=int, choices=[8, 10], default=8)
    parser.add_argument('--hdr-mode', type=int, choices=[0, 1, 2], default=0)
    parser.add_argument('--tls', choices=['1.2', '1.3'], help='requires build_stream_tls.py test entry for the local CA')
    parser.add_argument('--lifecycle', choices=['stop', 'resize', 'reinit'], default='stop')
    parser.add_argument('--stall-seconds', type=float, default=0)
    parser.add_argument('--driver', choices=['nvenc', 'vulkan'], default='nvenc')
    parser.add_argument('--audio', action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument('--codec', choices=['h264', 'hevc'], default='h264')
    args = parser.parse_args()
    if args.stall_seconds and (args.driver != 'vulkan' or args.audio or args.tls or args.hdr_mode
                               or args.lifecycle != 'stop' or not 5 <= args.stall_seconds <= 12):
        parser.error('slow-peer acceptance requires SDR video-only Vulkan stop, 5-12 seconds')
    if args.hdr_mode and (args.driver != 'vulkan' or args.codec != 'hevc' or args.bit_depth != 10):
        parser.error('HDR oracle requires Vulkan HEVC Main10')
    if args.codec == 'h264' and args.bit_depth != 8:
        parser.error('H.264 requires bit depth 8')
    for threaded in (False, True):
        check(args, threaded)
