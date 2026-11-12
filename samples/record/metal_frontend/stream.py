#!/usr/bin/env python3
"""Native Metal frontend start/stop, source clocks and independent RTMP output."""
import argparse
import array
import json
import math
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time


def port(kind):
    with socket.socket(type=kind) as reserve:
        reserve.bind(('127.0.0.1', 0))
        return reserve.getsockname()[1]


def wait_until(test, process, seconds=15):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if test():
            return
        assert process.poll() is None, ('frontend exited early', process.returncode)
        time.sleep(.02)
    raise TimeoutError('frontend acceptance condition timed out')


def check(args, threaded):
    root = args.output_dir.resolve() / ('threaded' if threaded else 'direct')
    root.mkdir(parents=True, exist_ok=False)
    command_port, stream_port = port(socket.SOCK_DGRAM), port(socket.SOCK_STREAM)
    config, native = root / 'retroarch.cfg', root / 'stream.cfg'
    received, log_path = root / 'received.flv', root / 'frontend.log'
    runtime_log = root / 'runtime.log'
    url = f'rtmp://127.0.0.1:{stream_port}/live/test'
    output_rate = 44100 if args.source_rate == 44100 else 48000
    native.write_text(f'audio_enable = {str(not args.video_only).lower()}\nvcodec = "{args.codec}"\nformat = "flv"\n'
                      f'video_bit_rate = 8000000\nacodec = "aac"\nsample_rate = {output_rate}\naudio_bit_rate = 128000\n')
    config.write_text(f'''video_driver = "metal"
video_threaded = "{str(threaded).lower()}"
video_fullscreen = "false"
video_scale = "1.0"
video_vsync = "true"
video_swapchain_bit_depth = "1"
video_hdr_mode = "0"
video_record_game_only = "false"
video_record_allow_frame_drop = "true"
video_record_fifo_frames = "{args.video_buffer_frames}"
video_font_enable = "false"
record_driver = "metal"
video_stream_url = "{url}"
video_stream_config = "{native.as_posix()}"
audio_driver = "null"
audio_format_negotiation = "1"
audio_fastpath_s16 = "false"
menu_driver = "rgui"
menu_show_start_screen = "false"
history_list_enable = "true"
playlist_directory = "{root / 'playlists'}"
playlist_use_old_format = "false"
recording_output_directory = "{root / 'recordings'}"
config_save_on_exit = "false"
log_verbosity = "true"
frontend_log_level = "1"
pause_nonactive = "false"
quit_press_twice = "false"
savestate_auto_save = "false"
savestate_auto_load = "false"
network_cmd_enable = "true"
network_cmd_port = "{command_port}"
''')
    env = dict(os.environ, LC_ALL='C', NATIVE_RECORD_RATE=str(args.source_rate),
               NATIVE_RECORD_LAYOUT='4' if args.channels == 1 else '3')
    env.pop('NATIVE_RECORD_FLOAT', None)
    with (root / 'receiver.log').open('wb') as receiver_log, log_path.open('wb') as log:
        server = subprocess.Popen([args.ffmpeg, '-v', 'debug', '-listen', '1',
                                   '-timeout', '15', '-copyts', '-analyzeduration', '500000',
                                   '-i', url, '-map', '0', '-c', 'copy', '-f', 'flv',
                                   '-flush_packets', '1', str(received)],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                  stderr=receiver_log)
        command = [str(args.retroarch.resolve()), '-v', '--log-file', str(runtime_log), '--config', str(config),
                   '-L', str(args.core.resolve()), '--max-frames', '36000']
        process = subprocess.Popen(command,
                                   cwd=root, env=env, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            with socket.socket(type=socket.SOCK_DGRAM) as client:
                client.settimeout(.1)
                target = ('127.0.0.1', command_port)

                def ready():
                    client.sendto(b'GET_STATUS\n', target)
                    try:
                        return b'PLAYING' in client.recv(65536)
                    except (socket.timeout, ConnectionError):
                        return False

                wait_until(ready, process)
                def option(on):
                    request = b'SET_OPTION streaming ' + (b'on' if on else b'off') + b'\n'
                    client.sendto(request, target)
                    replies = []
                    deadline = time.monotonic() + 3
                    while time.monotonic() < deadline:
                        try:
                            reply = client.recv(65536)
                        except socket.timeout:
                            continue
                        replies.append(reply.decode(errors='replace'))
                        (root / 'commands.json').write_text(json.dumps(replies, indent=2))
                        if reply.startswith(b'streaming\t'):
                            assert reply.strip() == (b'streaming\ton' if on else b'streaming\toff'), replies
                            return
                        assert b'ERROR' not in reply, replies
                    raise TimeoutError(f'no streaming acknowledgement: {replies}')

                def logged(fragment):
                    return runtime_log.exists() and fragment in runtime_log.read_text(errors='replace')

                option(True)
                wait_until(lambda: logged('fps streaming.'), process)
                wait_until(lambda: received.exists() and received.stat().st_size > 128, process)
                time.sleep(2)
                option(False)
                wait_until(lambda: logged('Finalized '), process)
                client.sendto(b'QUIT\n', target)
            assert process.wait(timeout=15) == 0
            assert server.wait(timeout=15) == 0
        finally:
            for child in (process, server):
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=65)
    verify(args, root, threaded)


def verify(args, root, threaded):
    """Independently recheck saved native output without rerunning the frontend."""
    received = root / 'received.flv'
    log_path, runtime_log = root / 'frontend.log', root / 'runtime.log'
    output_rate = 44100 if args.source_rate == 44100 else 48000
    config = (root / 'retroarch.cfg').read_text()
    buffer = re.search(r'^video_record_fifo_frames = "(\d+)"$', config, re.MULTILINE)
    requested_buffer = int(buffer[1]) if buffer else 32
    if args.video_buffer_frames is not None:
        assert requested_buffer == args.video_buffer_frames, (requested_buffer, args.video_buffer_frames)
    url = re.search(r'^video_stream_url = "([^"]+)"$', config, re.MULTILINE)[1]
    text = log_path.read_text(errors='replace') + runtime_log.read_text(errors='replace')
    assert f'[fixture] s16 source, mask {4 if args.channels == 1 else 3}' in text
    finalized = re.search(r'Finalized (\d+) GPU frames, (\d+) PCM frames', text)
    assert finalized and int(finalized[1]) > 20, text[-5000:]
    assert not list(root.rglob('*.mp4')) and not list(root.rglob('*.mkv')), 'streaming must not create a recording file'
    assert all(url not in p.read_text(errors='replace') for p in root.rglob('*.lpl')), 'stream URL entered movie history'
    histories = list(root.rglob('content_video_history.lpl'))
    entries = [entry for history in histories for entry in json.loads(history.read_text())['items']]
    assert not entries, 'streaming inserted a movie-history entry'
    info = json.loads(subprocess.run([args.ffprobe, '-v', 'error', '-show_streams',
                                     '-show_packets', '-of', 'json', str(received)],
                                    check=True, capture_output=True, timeout=15).stdout)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    audios = [s for s in info['streams'] if s['codec_type'] == 'audio']
    assert video['codec_name'] == args.codec and bool(audios) == (not args.video_only)
    assert video['color_primaries'] == video['color_transfer'] == video['color_space'] == 'bt709'
    assert video['color_range'] == 'tv'
    packets = [p for p in info['packets'] if p['stream_index'] == video['index']]
    assert len(packets) == int(finalized[1]), (len(packets), finalized[1])
    pts = [float(p['pts_time']) for p in packets]
    assert all(a < b for a, b in zip(pts, pts[1:]))
    assert (0 < pts[0] < .12) if audios else abs(pts[0]) < .0011
    for timestamp in pts:
        frame = round((timestamp - pts[0]) * 60000 / 1001)
        assert abs(timestamp - pts[0] - frame * 1001 / 60000) <= .0021, timestamp
    pixels = subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(received), '-map', '0:v:0',
                             '-vf', 'scale=1:1', '-fps_mode', 'passthrough',
                             '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
                            check=True, capture_output=True, timeout=20).stdout
    assert len(pixels) == len(packets) * 3
    colors = ((255, 32, 32), (32, 255, 32), (32, 32, 255), (192, 192, 192))
    for i in range(0, len(pixels), 3):
        pixel = pixels[i:i + 3]
        assert min(max(abs(a - b) for a, b in zip(pixel, color)) for color in colors) < 35, list(pixel)
    result = dict(threaded=threaded, codec=args.codec, source_rate=args.source_rate,
                  channels=args.channels if audios else 0, video_frames=len(pts), video_epoch=pts[0],
                  accepted_source_frames=int(finalized[2]), requested_buffer_frames=requested_buffer, result='PASS')
    if not audios:
        assert int(finalized[2]) == 0
        (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print('PASS native video-only Metal frontend stream: ' + json.dumps(result), flush=True)
        return
    audio = audios[0]
    assert audio['codec_name'] == 'aac' and int(audio['sample_rate']) == output_rate
    assert audio['channels'] == args.channels
    raw = subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(received), '-map', '0:a:0',
                          '-f', 'f32le', '-'], check=True, capture_output=True, timeout=20).stdout
    samples = array.array('f', raw)
    assert len(samples) > output_rate and all(math.isfinite(s) for s in samples)
    decoded_frames = len(samples) // args.channels
    output_frames = int(finalized[2]) * output_rate / args.source_rate
    assert decoded_frames % 1024 == 0 and output_frames <= decoded_frames < output_frames + 4096
    assert abs(decoded_frames / output_rate - (pts[-1] + 1001 / 60000)) < .06
    amplitudes = []
    for channel, frequency in enumerate((440, 660)[:args.channels]):
        values = samples[4096:24000][channel::args.channels]
        n = len(values)
        omega = 2 * math.pi * frequency / output_rate
        amplitude = 2 * math.hypot(sum(v * math.cos(omega * i) for i, v in enumerate(values)),
                                   sum(v * math.sin(omega * i) for i, v in enumerate(values))) / n
        assert abs(amplitude - .2) < .04, (channel, amplitude)
        amplitudes.append(amplitude)
    result.update(pcm_frames=decoded_frames, amplitudes=amplitudes, audio_rate=output_rate,
                  scope='Aligned quality; exact native AAC priming/SRC padding covered by the worker fixtures')
    (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS Metal frontend streaming start/stop, A/V and retirement: ' + json.dumps(result), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--retroarch', type=Path)
    parser.add_argument('--core', type=Path)
    parser.add_argument('--source-rate', type=float, default=48000)
    parser.add_argument('--channels', type=int, choices=[1, 2], default=2)
    parser.add_argument('--video-only', action='store_true')
    parser.add_argument('--video-buffer-frames', type=int, choices=[2, 4, 8, 16, 32])
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--verify-dir', type=Path, help='Recheck one saved direct/threaded native case')
    parser.add_argument('--codec', choices=['h264', 'hevc'], default='h264')
    args = parser.parse_args()
    assert math.isfinite(args.source_rate) and 8000 <= args.source_rate <= 192000
    if args.verify_dir:
        verify(args, args.verify_dir, args.verify_dir.name == 'threaded')
    else:
        assert sys.platform == 'darwin', 'Native frontend acceptance requires macOS'
        if not args.retroarch or not args.core or not args.output_dir:
            parser.error('--retroarch, --core and --output-dir are required for native execution')
        if args.video_buffer_frames is None:
            args.video_buffer_frames = 32
        for threaded in (False, True):
            check(args, threaded)
