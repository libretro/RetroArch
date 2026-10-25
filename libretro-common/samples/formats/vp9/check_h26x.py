#!/usr/bin/env python3
"""Matroska codec, timing, partial-input and lease regressions."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

SPEC = importlib.util.spec_from_file_location('h26x_fixture',
        Path(__file__).with_name('h26x_fixture.py'))
FIXTURES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURES)
executable = FIXTURES.executable
elements = FIXTURES.elements
hide_element = FIXTURES.hide_element
hevc_nonoutput_fixture = FIXTURES.hevc_nonoutput_fixture


def metadata_packet(data, hevc, laced=False, block_duration=False):
    inserted = False

    def rebuild(begin, end):
        nonlocal inserted
        output = bytearray()
        rows = list(elements(data, begin, end))
        skip = set()
        for index, (element, start, size_at, width, body, stop) in enumerate(rows):
            if index in skip:
                continue
            header = data[start:size_at]
            payload = data[body:stop]
            duration = 40
            keyframe = element == 0xa3 and bool(payload[3] & 0x80)
            if element in (0x18538067, 0x1f43b675, 0x1654ae6b, 0xae):
                payload = rebuild(body, stop)
            elif laced and element == 0x9c:
                payload = b'\x01'
            elif block_duration and element == 0x23e383:
                header = b'\x23\xe3\x84'
            elif element in (0x114d9b74, 0x1c53bb6b):
                header = (element + 1).to_bytes(len(header), 'big')
            elif element == 0xbf:
                header = b'\xec'
            if not inserted and element in (0xa3, 0xa0):
                if laced:
                    frames = []
                    for next_index in range(index, index + 3):
                        row = rows[next_index]
                        block = data[row[4]:row[5]]
                        if row[0] != 0xa3 or block[0] != 0x81 or block[3] & 0x7f:
                            raise ValueError('Expected three unlaced video frames')
                        if int.from_bytes(block[1:3], 'big', signed=True) != len(frames) * 40:
                            raise ValueError('Expected contiguous 25 fps frames')
                        frames.append(block[4:])
                        if next_index != index:
                            skip.add(next_index)
                    sizes = b''.join(b'\xff' * (len(frame) // 255)
                                     + bytes([len(frame) % 255]) for frame in frames[:2])
                    payload = b'\x81\0\0\x02\x02' + sizes + b''.join(frames)
                    duration = 120
                else:
                    nal = (b'\x4e\x01' if hevc else b'\x06') + b'\x05\x10' + b'\x11' * 16 + b'\x80'
                    packet = b'\x81\0\0\0' + len(nal).to_bytes(4, 'big') + nal
                    output += b'\xa3' + bytes([0x80 | len(packet)]) + packet
                inserted = True
            if block_duration and element == 0xa3:
                duration_element = b'\x9b\x81' + bytes([duration])
                reference_element = b'' if keyframe else b'\xfb\x81\xd8'
                payload = payload[:3] + bytes([payload[3] & 0x7f]) + payload[4:]
                block = b'\xa1' + (0x4000 | len(payload)).to_bytes(2, 'big') + payload
                payload = (duration_element + block if index % 2 else block + duration_element)
                payload += reference_element
                header = b'\xa0'
            size = len(payload)
            while size >= (1 << (7 * width)) - 1:
                width += 1
                if width > 8:
                    raise ValueError('Fixture element size cannot be encoded')
            output += header + (size | (1 << (7 * width))).to_bytes(width, 'big') + payload
        return bytes(output)

    result = rebuild(0, len(data))
    if not inserted:
        raise ValueError('No video block found in fixture')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stream-test', default='./rwebm_h26x_stream_test')
    parser.add_argument('--hardware-test', default='./rwebm_h26x_hw_test')
    parser.add_argument('--ffmpeg', default=os.environ.get('FFMPEG', 'ffmpeg'))
    parser.add_argument('--runner', default=os.environ.get('RUNNER', ''))
    parser.add_argument('--codec', choices=('h264', 'hevc', 'both'), default='both')
    parser.add_argument('--pixel-format', default='yuv420p')
    parser.add_argument('--full-range', action='store_true')
    parser.add_argument('--laced', action='store_true')
    args = parser.parse_args()
    stream_test = executable(args.stream_test)
    hardware_test = executable(args.hardware_test)
    runner = shlex.split(args.runner)
    results = []
    with tempfile.TemporaryDirectory(prefix='matroska-h26x-') as directory:
        root = Path(directory)
        for codec in (('h264', 'hevc') if args.codec == 'both' else (args.codec,)):
            for variable in ((False,) if args.laced else (False, True)):
                name = codec + ('-vfr' if variable else '-cfr')
                movie = root / (name + '.mkv')
                reference = root / (name + '.rgba')
                times = root / (name + '.txt')
                rate = 25 if args.laced else 24
                source = ('nullsrc=size=64x64:rate=%d,geq=lum=16+X*219/(W-1):cb=128:cr=128'
                          if args.full_range else 'testsrc2=size=64x64:rate=%d') % rate
                command = [args.ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', source]
                filters = []
                if variable:
                    filters.append('setpts=(N+floor(N/3))/24/TB')
                if args.full_range:
                    filters.append('scale=in_range=tv:out_range=pc')
                if filters:
                    command += ['-vf', ','.join(filters)]
                command += ['-frames:v', '24', '-fps_mode', 'vfr', '-threads', '1',
                            '-pix_fmt', args.pixel_format]
                if codec == 'h264':
                    command += ['-c:v', 'libx264', '-bf', '0' if args.laced else '3',
                                '-g', '12']
                    if args.full_range:
                        command += ['-x264-params', 'fullrange=on', '-color_range', 'pc']
                else:
                    command += ['-c:v', 'libx265', '-x265-params',
                                'pools=none:frame-threads=1:log-level=error'
                                + (':bframes=0:keyint=12' if args.laced else ':bframes=3:keyint=12')
                                + (':range=full' if args.full_range else '')]
                if args.laced:
                    command += ['-cluster_time_limit', '1000', '-cluster_size_limit', '10000000']
                subprocess.run(command + ['-y', str(movie)], check=True)
                # Match the player's point chroma sampling.
                subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(movie),
                                '-fps_mode', 'passthrough', '-f', 'rawvideo',
                                '-sws_flags', 'neighbor+bitexact',
                                '-pix_fmt', 'rgba', '-y', str(reference)], check=True)
                if reference.stat().st_size != 24 * 64 * 64 * 4:
                    raise ValueError('Reference does not contain 24 RGBA frames')
                timestamps = [((i + (i // 3 if variable else 0)) * 1000 + rate // 2) // rate
                              for i in range(24)]
                times.write_text(''.join(str(t) + '\n' for t in timestamps), encoding='ascii')
                subprocess.run(runner + [stream_test, str(movie), str(reference), str(times)], check=True)
                results.append(name)
                if args.laced:
                    lace = root / (name + '-laced.mkv')
                    lace.write_bytes(metadata_packet(movie.read_bytes(), codec == 'hevc', laced=True))
                    lace_reference = root / (name + '-laced.rgba')
                    subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(lace),
                                    '-fps_mode', 'passthrough', '-f', 'rawvideo',
                                    '-sws_flags', 'neighbor+bitexact', '-pix_fmt', 'rgba',
                                    '-y', str(lace_reference)], check=True)
                    if lace_reference.read_bytes() != reference.read_bytes():
                        raise ValueError('Lacing changed the independent decoded reference')
                    subprocess.run(runner + [stream_test, str(lace), str(reference), str(times)], check=True)
                    results.append(name + '-laced')
                    duration_movie = root / (name + '-laced-duration.mkv')
                    duration_movie.write_bytes(metadata_packet(movie.read_bytes(), codec == 'hevc',
                                                                laced=True, block_duration=True))
                    subprocess.run([args.ffmpeg, '-v', 'error', '-i', str(duration_movie),
                                    '-fps_mode', 'passthrough', '-f', 'rawvideo',
                                    '-sws_flags', 'neighbor+bitexact', '-pix_fmt', 'rgba',
                                    '-y', str(lace_reference)], check=True)
                    if lace_reference.read_bytes() != reference.read_bytes():
                        raise ValueError('BlockDuration changed the decoded reference')
                    subprocess.run(runner + [stream_test, str(duration_movie), str(reference),
                                             str(times)], check=True)
                    results.append(name + '-laced-duration')
                    continue
                if not variable and args.pixel_format == 'yuv420p':
                    subprocess.run(runner + [hardware_test, str(movie)], check=True)
                    results.append(name + '-leases')
                    metadata = root / (name + '-metadata.mkv')
                    metadata.write_bytes(metadata_packet(movie.read_bytes(), codec == 'hevc'))
                    subprocess.run(runner + [stream_test, str(metadata), str(reference),
                                             str(times)], check=True)
                    subprocess.run(runner + [hardware_test, str(metadata)], check=True)
                    results.append(name + '-metadata')
                else:
                    without_default = root / (name + '-no-default.mkv')
                    without_default.write_bytes(hide_element(movie.read_bytes(), b'\x23\xe3\x83'))
                    subprocess.run(runner + [stream_test, str(without_default), str(reference),
                                             str(times)], check=True)
                    results.append(name + '-no-default')
        if args.codec != 'h264' and args.pixel_format == 'yuv420p' and not args.full_range and not args.laced:
            for reordered, discard in ((False, False), (True, False), (True, True)):
                movie, reference, times = hevc_nonoutput_fixture(args.ffmpeg, root,
                        reordered=reordered, discard_prior=discard)
                subprocess.run(runner + [stream_test, str(movie), str(reference), str(times)], check=True)
                results.append('hevc-nonoutput' + ('-reordered' if reordered else '')
                        + ('-discard-prior' if discard else ''))
                without_default = root / 'nonoutput-no-default.mkv'
                without_default.write_bytes(hide_element(movie.read_bytes(), b'\x23\xe3\x83'))
                subprocess.run(runner + [stream_test, str(without_default), str(reference), str(times)],
                               check=True)
                results.append(results[-1] + '-no-default')
                subprocess.run(runner + [hardware_test, str(without_default), str(times)],
                               check=True)
                results.append(results[-1] + '-leases')
    print(json.dumps({'passed': results, 'native_gpu_acceptance': False}))


if __name__ == '__main__':
    main()
