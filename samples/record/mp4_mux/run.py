#!/usr/bin/env python3
"""Independent codec/container/timestamp/AC-3 pixel and sample oracles."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--runner', default='')
p.add_argument('--hdr-tags', action='store_true')
p.add_argument('binary')
a = p.parse_args()
binary = str(Path(a.binary).resolve())
runner = shlex.split(a.runner)
ffmpeg = shutil.which('ffmpeg')
ffprobe = shutil.which('ffprobe')
if not ffmpeg or not ffprobe:
    raise SystemExit('MP4 oracle requires ffmpeg and ffprobe (test-only)')

def command(args):
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode:
        raise RuntimeError(p.stderr.decode(errors='replace'))
    return p.stdout

def probe(path):
    return json.loads(command([ffprobe, '-v', 'error', '-show_streams', '-show_packets',
                               '-show_format', '-of', 'json', str(path)]))

def decode(path, audio=False, start=None):
    seek = ['-ss', str(start)] if start is not None else []
    return command([ffmpeg, '-v', 'error', *seek, '-i', str(path), '-map', '0:a:0' if audio else '0:v:0',
                    '-fps_mode', 'passthrough', *(['-f', 'f32le'] if audio else
                    ['-pix_fmt', 'yuv420p10le', '-f', 'rawvideo']), '-'])

with tempfile.TemporaryDirectory(prefix='record-mp4-') as temp:
    root = Path(temp)
    passed = 0
    cases = [(codec, depth, None) for codec, depth in
             ((0, 8), (1, 8), (1, 10), (2, 8), (2, 10))]
    if a.hdr_tags:
        cases += [(codec, 10, transfer) for codec in (1, 2)
                  for transfer in ('smpte2084', 'arib-std-b67')]
    for codec, depth, transfer in cases:
        name = ('h264', 'hevc', 'obu')[codec]
        source = root / f'{codec}-{depth}-{transfer or "untagged"}.{name}'
        options = (['-c:v', 'libx264', '-preset', 'ultrafast', '-bf', '0', '-g', '8',
                    '-x264-params', 'repeat-headers=1'] if codec == 0 else
                   ['-c:v', 'libx265', '-preset', 'ultrafast', '-x265-params',
                    'bframes=0:repeat-headers=1:keyint=8:log-level=error:pools=1:frame-threads=1'] if codec == 1 else
                   ['-c:v', 'libaom-av1', '-cpu-used', '8', '-lag-in-frames', '0', '-g', '8', '-threads', '1'])
        command([ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=64x48:rate=30000/1001',
                 '-frames:v', '32', '-pix_fmt', 'yuv420p10le' if depth == 10 else 'yuv420p',
                 *options, *(['-color_primaries', 'bt2020', '-color_trc', transfer,
                              '-colorspace', 'bt2020nc', '-color_range', 'tv'] if transfer else []),
                 '-f', name, str(source)])
        source_info = probe(source)
        if transfer:
            assert source_info['streams'][0]['color_transfer'] == transfer, source_info
        packets = source_info['packets']
        assert len(packets) == 32
        payload = source.read_bytes()
        frames = [(('K' in q['flags']), payload[int(q['pos']):int(q['pos'])+int(q['size'])]) for q in packets]
        assert len(frames[0][1]) < 65536
        pixels = decode(source)
        for rate, layout, channels in ((0, 0, 0), (48000, 3, 2), (44100, 4, 1), (32000, 0x60f, 6)):
            fixture = root / 'input.bin'
            data = bytearray(struct.pack('<10I', codec, depth, 64, 48, 30000, 1001, 32, rate, layout, 192))
            data += struct.pack('<I', len(frames[0][1])) + frames[0][1]
            for key, packet in frames:
                data += struct.pack('<2I', key, len(packet)) + packet
            fixture.write_bytes(data)
            movie = root / f'{codec}-{depth}-{transfer or "untagged"}-{rate}.mp4'
            audio = root / 'reference.ac3'
            tags = ['9', '16' if transfer == 'smpte2084' else '18', '9', '0'] if transfer else []
            if codec == 2 and transfer and not rate:
                rejected = root / f'rejected-{transfer}.mp4'
                mismatch = [tags[0], '18' if tags[1] == '16' else '16', *tags[2:]]
                result = subprocess.run([*runner, binary, str(fixture), str(rejected),
                                         str(audio), *mismatch], capture_output=True)
                assert result.returncode != 0 and not rejected.exists(), result
            subprocess.run([*runner, binary, str(fixture), str(movie), str(audio), *tags], check=True)
            info = probe(movie)
            streams = info['streams']
            if codec == 2:
                assert 'av01' in info['format']['tags']['compatible_brands']
            assert len(streams) == (2 if rate else 1), streams
            assert streams[0]['codec_name'] == ('h264', 'hevc', 'av1')[codec]
            assert (streams[0]['width'], streams[0]['height']) == (64, 48)
            assert ('10' in streams[0]['pix_fmt']) == (depth == 10), streams[0]
            if transfer:
                assert streams[0]['color_primaries'] == 'bt2020', streams[0]
                assert streams[0]['color_transfer'] == transfer, streams[0]
                assert streams[0]['color_space'] == 'bt2020nc', streams[0]
                assert streams[0]['color_range'] == 'tv', streams[0]
                blob = movie.read_bytes()
                assert blob.count(b'colrnclx') == 1
            assert decode(movie) == pixels, 'video pixels changed'
            assert decode(movie, start=8 * 1001 / 30000) == pixels[8 * 64 * 48 * 3:], 'keyframe seek differs'
            video = [q for q in info['packets'] if q['stream_index'] == 0]
            assert len(video) == 32
            assert [int(q['pts']) for q in video] == [i * 1001 for i in range(32)]
            assert [int(q['dts']) for q in video] == [i * 1001 for i in range(32)]
            assert all(int(q['duration']) == 1001 for q in video if 'duration' in q)
            assert abs(float(streams[0]['duration']) - 32 * 1001 / 30000) < 0.00001
            assert [('K' in q['flags']) for q in video] == [f[0] for f in frames]
            if rate:
                assert streams[1]['codec_name'] == 'ac3' and int(streams[1]['sample_rate']) == rate
                assert streams[1]['channels'] == channels, streams[1]
                assert decode(movie, True) == decode(audio, True), 'audio samples changed'
                sounds = [q for q in info['packets'] if q['stream_index'] == 1]
                assert [int(q['pts']) for q in sounds] == [i * 1536 for i in range(len(sounds))]
                assert [int(q['dts']) for q in sounds] == [i * 1536 for i in range(len(sounds))]
                assert all(int(q['duration']) == 1536 for q in sounds)
                assert 0 <= len(sounds) * 1536 / rate - 32 * 1001 / 30000 < 1536 / rate
            # Initialization plus complete fragments form a usable prefix;
            # no seek-back/trailer is needed to recover acknowledged output.
            blob = movie.read_bytes()
            pos, fragments = 0, []
            while pos < len(blob):
                size = int.from_bytes(blob[pos:pos+4], 'big')
                assert size >= 8 and size <= len(blob) - pos
                if blob[pos+4:pos+8] == b'mdat': fragments.append(pos + size)
                pos += size
            prefix = root / 'prefix.mp4'
            prefix.write_bytes(blob[:fragments[len(fragments)//2]])
            command([ffmpeg, '-v', 'error', '-i', str(prefix), '-f', 'null', '-'])
            passed += 1
            print(f'[pass] MP4 {name}/{depth}/{transfer or "untagged"}, AC-3 {rate} Hz/{channels} ch: pixels, samples, timing, prefix', flush=True)
    print(f'PASS: {passed} independently decoded MP4 A/V cases', flush=True)
