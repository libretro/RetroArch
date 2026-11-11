#!/usr/bin/env python3
"""Independent FLV packet, timestamp and decoded video/audio checks."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def command(args):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=60)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace'))
    return result.stdout


def probe(path):
    data = json.loads(command(['ffprobe', '-v', 'error', '-show_packets',
                               '-show_data_hash', 'sha256', '-of', 'json', str(path)]))
    return [(p['codec_type'], p['dts'], p['pts'], p['size'], p['data_hash'])
            for p in data['packets']]


def decode(path, audio):
    return command(['ffmpeg', '-v', 'error', '-i', str(path), '-map',
                    '0:a:0' if audio else '0:v:0',
                    *(['-f', 'f32le'] if audio else
                      ['-fps_mode', 'passthrough', '-pix_fmt', 'yuv420p10le', '-f', 'rawvideo']), '-'])


def color_metadata(path):
    data = json.loads(command(['ffprobe', '-v', 'error', '-show_streams', '-of', 'json', str(path)]))
    video = next(s for s in data['streams'] if s['codec_type'] == 'video')
    assert (video['color_primaries'], video['color_transfer'], video['color_space']) == (
        'bt2020', 'smpte2084', 'bt2020nc')
    side = video.get('side_data_list', [])
    mastering = next(s for s in side if s['side_data_type'] == 'Mastering display metadata')
    light = next(s for s in side if s['side_data_type'] == 'Content light level metadata')
    expected = dict(red_x='0.68', red_y='0.32', green_x='0.265', green_y='0.69',
                    blue_x='0.15', blue_y='0.06', white_point_x='0.3127', white_point_y='0.329',
                    min_luminance='0.005', max_luminance='1000')
    for key, value in expected.items():
        actual, original = float(Fraction(mastering[key])), float(value)
        # Receiver-side metadata uses float precision; AMF wire checks are exact.
        assert abs(actual - original) <= max(1e-12, abs(original) * 1e-7), (
            key, mastering[key], value)
    assert light['max_content'] == 1000 and light['max_average'] == 400, light


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('--require-color-metadata', action='store_true')
    parser.add_argument('binary')
    args = parser.parse_args()
    for tool in ('ffmpeg', 'ffprobe'):
        if not shutil.which(tool):
            raise SystemExit(f'{tool} is required (test only)')
    binary = str(Path(args.binary).resolve())
    with tempfile.TemporaryDirectory(prefix='flv-mux-') as temp:
        root = Path(temp)
        for codec, depth in (('libx264', 8), ('libx265', 8), ('libx265', 10),
                             ('libaom-av1', 8), ('libaom-av1', 10)):
            for bframes in (0, 2):
                source, output = root / 'source.flv', root / 'output.flv'
                options = ['-pix_fmt', 'yuv420p10le' if depth == 10 else 'yuv420p']
                if codec == 'libx265':
                    options += ['-x265-params', 'pools=1:frame-threads=1:log-level=error']
                if codec == 'libaom-av1':
                    options += ['-cpu-used', '8', '-lag-in-frames', '8' if bframes else '0',
                                '-auto-alt-ref', '1' if bframes else '0']
                if depth == 10:
                    options += ['-color_primaries', 'bt2020', '-color_trc', 'smpte2084',
                                '-colorspace', 'bt2020nc']
                video_input = 'testsrc2=size=128x96:rate=25:duration=1'
                if depth == 10:
                    video_input += ',setparams=color_primaries=bt2020:color_trc=smpte2084:colorspace=bt2020nc:range=limited'
                command(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i',
                         video_input, '-f', 'lavfi', '-i',
                         'sine=frequency=997:sample_rate=44100:duration=1',
                         '-c:v', codec, '-threads', '1',
                         *([] if codec == 'libaom-av1' else ['-bf', str(bframes)]), *options,
                         '-g', '12', '-c:a', 'aac', '-profile:a', 'aac_low',
                         '-flvflags', 'no_duration_filesize', '-f', 'flv', str(source)])
                modes = [None, 'obu'] if codec == 'libaom-av1' else [None, 'annexb']
                if depth == 10:
                    modes.extend(['color', 'obu-color'] if codec == 'libaom-av1' else ['color', 'annexb-color'])
                for mode in modes:
                    command(shlex.split(args.runner) + [binary, str(source), str(output)]
                            + ([mode] if mode else []))
                    assert probe(source) == probe(output), 'encoded packets or timestamps differ'
                    for audio in (False, True):
                        assert decode(source, audio) == decode(output, audio), 'decoded output differs'
                    if args.require_color_metadata and mode in ('color', 'annexb-color', 'obu-color'):
                        color_metadata(output)
                        print('PASS: independent receiver parsed mastering/light units from colorInfo')
                    reorder = f'lookahead={8 if bframes else 0}' if codec == 'libaom-av1' else f'B-frames={bframes}'
                    print(f'PASS: independent FLV packet/timestamp/video/PCM oracle, '
                          f'codec={codec}/{depth}, {reorder}, '
                          f'Annex B={mode in ("annexb", "annexb-color")}, '
                          f'native OBU={mode in ("obu", "obu-color")}, '
                          f'colorInfo={mode in ("color", "annexb-color", "obu-color")}')


if __name__ == '__main__':
    main()
