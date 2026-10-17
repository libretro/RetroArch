#!/usr/bin/env python3
"""Real frontend GPU recording; FFmpeg is a test-only decoder."""
import argparse
import array
import math
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--core', required=True)
parser.add_argument('--retroarch')
parser.add_argument('--output-dir')
parser.add_argument('--codecs', nargs='+', default=['h264', 'hevc', 'av1'])
parser.add_argument('--threads', nargs='+', default=['false', 'true'])
parser.add_argument('--audio', nargs='+', choices=['false', 'true'], default=['false', 'true'])
parser.add_argument('--rates', nargs='+', type=int, default=[32000, 44100, 48000])
parser.add_argument('--layouts', nargs='+', type=lambda x: int(x, 0), default=[3])
parser.add_argument('--float', action='store_true', dest='floating')
args = parser.parse_args()
if not args.retroarch:
    print('SKIP: provide a native Windows RetroArch build with D3D12/NVENC')
    raise SystemExit(0)
if sys.platform != 'win32':
    raise SystemExit('Native frontend GPU acceptance requires Windows')
ffmpeg = shutil.which('ffmpeg')
assert ffmpeg, 'test-only ffmpeg decoder required'
exe, core = str(Path(args.retroarch).resolve()), str(Path(args.core).resolve())


def run_case(root, codec, threaded, kind='normal', audio=False, rate=48000, layout=3):
    path = root / f'{codec}-{threaded}-{kind}-av{rate if audio else 0}-mask{layout:x}-{'float' if args.floating else 's16'}'
    path.mkdir()
    config, record, movie = path / 'retroarch.cfg', path / 'record.cfg', path / 'output.mp4'
    config.write_text(f'''
video_driver = "d3d12"
video_threaded = "{threaded}"
video_fullscreen = "false"
video_scale = "1.0"
video_vsync = "true"
video_swapchain_bit_depth = "1"
video_hdr_mode = "0"
video_record_game_only = "false"
video_record_allow_frame_drop = "true"
video_font_enable = "false"
record_driver = "nvenc"
audio_driver = "null"
audio_format_negotiation = "1"
audio_fastpath_s16 = "false"
menu_driver = "rgui"
menu_show_start_screen = "false"
history_list_enable = "false"
config_save_on_exit = "false"
pause_nonactive = "false"
savestate_auto_save = "false"
savestate_auto_load = "false"
''', encoding='utf-8')
    record.write_text(f'audio_enable = "{"true" if audio else "false"}"\n'
                      f'vcodec = "{codec}"\nformat = "mp4"\nvideo_bit_rate = "8000000"\n', encoding='utf-8')
    if kind == 'exists':
        movie.write_bytes(b'preserve existing output')
    frames = '1' if kind == 'early' else '120' if kind == 'normal' else '10'
    environment = os.environ.copy()
    environment['NATIVE_RECORD_RATE'] = str(22050 if kind == 'rate' else rate)
    environment['NATIVE_RECORD_LAYOUT'] = str(layout)
    if args.floating: environment['NATIVE_RECORD_FLOAT'] = '1'
    else: environment.pop('NATIVE_RECORD_FLOAT', None)
    result = subprocess.run([exe, '-v', '--config', str(config), '-L', core,
                             '--record', str(movie), '--recordconfig', str(record),
                             '--max-frames', frames], cwd=path,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45, env=environment)
    log = result.stdout.decode(errors='replace')
    (path / 'run.log').write_text(log, encoding='utf-8')
    assert result.returncode == 0, (path, result.returncode, log[-5000:])
    assert f'[fixture] {"float" if args.floating else "s16"} source, mask {layout:x}' in log, log
    if kind in ('rate', 'layout'):
        assert not movie.exists() and 'Audio requires an AC-3 speaker layout' in log, log
        print(f'[pass] unsupported native audio {kind} refused (mask {layout:x})', flush=True)
        return
    if kind == 'exists':
        assert movie.read_bytes() == b'preserve existing output' and 'Recording failed' in log, log
        print('[pass] startup failure preserves existing file and retires', flush=True)
        return
    if kind == 'early':
        assert 'Starting native' in log or not movie.exists(), log
        print('[pass] quit during startup drains without hanging', flush=True)
        return
    finalized = re.search(r'Recording finalized \((\d+) GPU frames\)', log)
    assert finalized and int(finalized[1]) >= 20, log
    decoded = subprocess.run([ffmpeg, '-hide_banner', '-i', str(movie), '-map', '0:v:0',
                              '-vf', 'showinfo', '-pix_fmt', 'rgb24', '-fps_mode', 'passthrough',
                              '-f', 'rawvideo', '-'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             check=True, timeout=30)
    text = decoded.stderr.decode(errors='replace')
    source = text.split('Output #', 1)[0]
    assert ('Audio:' in source) == audio and codec in source, source
    assert 'time_base: 1/60000' in text and 'frame_rate: 60000/1001' in text, text[:5000]
    pts = [int(x) for x in re.findall(r'\bn:\s*\d+\s+pts:\s*(-?\d+)', text)]
    size = re.search(r'\bs:(\d+)x(\d+)', text)
    assert size and len(pts) == int(finalized[1]) and (pts[0] >= 0 if audio else pts[0] == 0), (pts, log)
    assert all(b > a and (b - a) % 1001 == 0 for a, b in zip(pts, pts[1:])), pts
    width, height = map(int, size.groups())
    frame_size = width * height * 3
    assert len(decoded.stdout) == len(pts) * frame_size
    references = [(255, 32, 32), (32, 255, 32), (32, 32, 255), (192, 192, 192)]
    colors = set()
    for n in range(len(pts)):
        offset = n * frame_size + ((height // 2) * width + width // 2) * 3
        pixel = decoded.stdout[offset:offset + 3]
        errors = [sum(abs(a - b) for a, b in zip(pixel, color)) / 3 for color in references]
        assert min(errors) < 8, (codec, threaded, n, list(pixel), errors)
        colors.add(errors.index(min(errors)))
    assert len(colors) >= 3, (codec, threaded, colors)
    if audio:
        pcm_count = re.search(r'Audio finalized \((\d+) PCM frames\)', log)
        assert pcm_count, log
        pcm_frames = int(pcm_count[1])
        channels = layout.bit_count()
        label = {3: 'stereo', 4: 'mono', 0x603: 'quad(side)', 0x60f: '5.1(side)'}[layout]
        assert f'{rate} Hz, {label}' in source and 'ac3' in source, source
        decoded_audio = subprocess.run([ffmpeg, '-v', 'error', '-i', str(movie),
                                       '-map', '0:a:0', '-f', 'f32le', '-'],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                      check=True, timeout=30).stdout
        samples = array.array('f', decoded_audio)
        assert len(samples) == ((pcm_frames + 1535) // 1536) * 1536 * channels, (len(samples), pcm_frames)
        expected_end = (pts[-1] + 1001) * rate / 60000
        assert abs(pcm_frames - expected_end) <= rate * 1001 / 60000 + 2, (pcm_frames, expected_end, pts)
        frequencies = [440, 660, 880, 80, 1000, 1200]
        for channel, frequency in enumerate(frequencies[:channels]):
            amplitude = .2
            values = samples[2048 * channels: min(pcm_frames, 10240) * channels][channel::channels]
            n = len(values)
            assert n > 2000
            omega = 2 * math.pi * frequency / rate
            cosine = sum(v * math.cos(omega * i) for i, v in enumerate(values))
            sine = sum(v * math.sin(omega * i) for i, v in enumerate(values))
            measured = 2 * math.hypot(cosine, sine) / n
            assert abs(measured - amplitude) < .04, (channel, frequency, measured)
        print(f'[pass] frontend {codec}/{threaded}: {len(pts)} GPU frames, {pcm_frames} PCM frames, mask {layout:x}, at {rate} Hz, clocks, tones, async quit', flush=True)
    else:
        print(f'[pass] frontend {codec}/{threaded}: {len(pts)} GPU frames, NTSC clock, colors, no audio, async quit', flush=True)


temporary = None
if args.output_dir:
    root = Path(args.output_dir).resolve()
    root.mkdir(parents=True, exist_ok=True)
else:
    temporary = tempfile.TemporaryDirectory(prefix='nvenc-frontend-')
    root = Path(temporary.name)
try:
    for codec in args.codecs:
        for threaded in args.threads:
            for audio in args.audio:
                for rate in args.rates if audio == 'true' else [48000]:
                    for layout in args.layouts if audio == 'true' else [3]:
                        run_case(root, codec, threaded, audio=audio == 'true', rate=rate, layout=layout)
    for layout in (0x33, 0x3f, 0x63f):
        run_case(root, 'h264', 'true', 'layout', audio=True, layout=layout)
    for kind in ('rate', 'exists', 'early'):
        run_case(root, 'h264', 'true', kind, audio=True)
finally:
    if temporary:
        temporary.cleanup()
