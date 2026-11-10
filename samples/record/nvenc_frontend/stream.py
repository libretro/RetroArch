#!/usr/bin/env python3
"""Start/stop frontend streaming on an isolated native Windows desktop."""
import argparse
import array
import json
import math
import os
from pathlib import Path
import re
import socket
import subprocess
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
    url = f'rtmp://127.0.0.1:{stream_port}/live/test'
    native.write_text('audio_enable = true\nvcodec = "h264"\nformat = "flv"\n'
                      'video_bit_rate = 8000000\n')
    config.write_text(f'''video_driver = "d3d12"
video_threaded = "{str(threaded).lower()}"
video_fullscreen = "false"
video_scale = "1.0"
video_vsync = "true"
video_swapchain_bit_depth = "1"
video_hdr_mode = "0"
video_record_game_only = "false"
video_record_allow_frame_drop = "true"
video_font_enable = "false"
record_driver = "nvenc"
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
    with (root / 'receiver.log').open('wb') as receiver_log, log_path.open('wb') as log:
        server = subprocess.Popen([args.ffmpeg, '-v', 'error', '-listen', '1',
                                   '-timeout', '15', '-copyts', '-analyzeduration', '500000',
                                   '-i', url, '-map', '0', '-c', 'copy', '-f', 'flv', str(received)],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                  stderr=receiver_log, creationflags=subprocess.CREATE_NO_WINDOW)
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
                wait_until(lambda: 'Starting native AAC/video' in log_path.read_text(errors='replace'), process)
                time.sleep(2)
                client.sendto(b'SET_OPTION streaming off\n', target)
                wait_until(lambda: 'Recording finalized (' in log_path.read_text(errors='replace'), process)
                client.sendto(b'QUIT\n', target)
            assert process.wait(timeout=15) == 0
            assert server.wait(timeout=15) == 0
        finally:
            for child in (process, server):
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=65)
    text = log_path.read_text(errors='replace')
    assert '[fixture] s16 source, mask 3' in text
    finalized = re.search(r'Recording finalized \((\d+) GPU frames\)', text)
    assert finalized and int(finalized[1]) > 20, text[-5000:]
    assert not list(root.glob('*.mp4')), 'streaming must not create a recording file'
    info = json.loads(subprocess.run([args.ffprobe, '-v', 'error', '-show_streams',
                                     '-show_packets', '-of', 'json', str(received)],
                                    check=True, capture_output=True, timeout=15).stdout)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
    assert video['codec_name'] == 'h264' and audio['codec_name'] == 'aac'
    assert audio['sample_rate'] == '48000' and audio['channels'] == 2
    packets = [p for p in info['packets'] if p['stream_index'] == video['index']]
    assert len(packets) == int(finalized[1]), (len(packets), finalized[1])
    pts = [float(p['pts_time']) for p in packets]
    assert abs(pts[0]) < .0011 and all(a < b for a, b in zip(pts, pts[1:]))
    for timestamp in pts:
        frame = round(timestamp * 60000 / 1001)
        assert abs(timestamp - frame * 1001 / 60000) <= .0011, timestamp
    pixels = subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(received), '-map', '0:v:0',
                             '-vf', 'scale=1:1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
                            check=True, capture_output=True, timeout=20).stdout
    assert len(pixels) == len(packets) * 3
    colors = ((255, 32, 32), (32, 255, 32), (32, 32, 255), (192, 192, 192))
    for i in range(0, len(pixels), 3):
        pixel = pixels[i:i + 3]
        assert min(max(abs(a - b) for a, b in zip(pixel, color)) for color in colors) < 35, list(pixel)
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
    result = dict(threaded=threaded, video_frames=len(pts), pcm_frames=len(samples) // 2,
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
    args = parser.parse_args()
    for threaded in (False, True):
        check(args, threaded)
