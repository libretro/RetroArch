#!/usr/bin/env python3
"""Check Vulkan AV1 parameter units and byte offsets against FFmpeg traces."""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def obus(data):
    offset = 0
    while offset < len(data):
        start, first = offset, data[offset]
        offset += 1 + bool(first & 4)
        assert first & 2
        size = 0
        for i in range(8):
            byte = data[offset]
            offset += 1
            size |= (byte & 127) << (7 * i)
            if not byte & 128:
                break
        assert size <= len(data) - offset
        yield (first >> 3) & 15, data[start:offset + size], data[offset:offset + size]
        offset += size


def blocks(trace):
    result, current = [], None
    for line in trace.splitlines():
        match = re.match(r'\[trace_headers @ [^]]+\] (.*)', line)
        if not match:
            continue
        text = match[1]
        if text == 'Frame Header':
            current = []
            result.append(current)
        elif text == 'OBU header':
            current = None
        elif current is not None:
            current.append(text)
    return result


def row(label, values):
    return label + ' ' + ' '.join(map(str, values))


exe = str(Path(os.environ.get('AV1_VULKAN_TEST', './av1_params_test')).resolve())
with tempfile.TemporaryDirectory(prefix='vulkan-av1-') as directory:
    for depth in (8, 10):
        for name, count, options in (
                ('reorder', 30, ['-lag-in-frames', '25']),
                ('grain', 18, ['-aom-params', 'film-grain-test=5']),
                ('tiles', 4, ['-tiles', '2x2', '-g', '1']),
                ('sb128', 4, ['-aom-params', 'sb-size=128']),
                ('lossless', 4, ['-crf', '0', '-b:v', '0']),
                ('still', 1, ['-still-picture', '1'])):
            path = Path(directory) / f'{depth}-{name}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=256x256:rate=12', '-frames:v', str(count), '-an',
                '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
                *options, str(path)], check=True)
            trace = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers',
                '-f', 'null', '-'], check=True, capture_output=True, text=True).stderr
            expected, coded, tiles = [], 0, 0
            packets = [raw for kind, raw, _ in obus(path.read_bytes()) if kind in (3, 6)]
            headers = blocks(trace)
            assert len(packets) == len(headers)
            sb128 = int(re.findall(r'use_128x128_superblock\s+[01] = ([01])', trace)[-1])
            for block, raw in zip(headers, packets):
                values, ends = {}, {}
                for text in block:
                    match = re.match(r'(\d+)\s+(\S+)\s+([01]+) = (-?\d+)', text)
                    if match:
                        position, key, bits, value = match.groups()
                        values[key] = int(value)
                        ends[key] = int(position) + len(bits)
                get = lambda key, default=0: values.get(key, default)
                if get('show_existing_frame'):
                    continue
                coded += 1
                fields = [get('frame_type'), get('order_hint'), get('primary_ref_frame', 7),
                    get('refresh_frame_flags', 255), get('diff_uv_delta'),
                    sum(get(f'update_ref_delta[{i}]') << i for i in range(8)),
                    sum(get(f'update_mode_delta[{i}]') << i for i in range(2)),
                    get('cdef_damping_minus_3'), get('cdef_bits'), get('coded_denom')]
                for i in range(8):
                    fields += [get(f'cdef_y_sec_strength[{i}]'), get(f'cdef_uv_sec_strength[{i}]')]
                lr = [0, 0, 0]
                if any(get(f'lr_type[{i}]') for i in range(3)):
                    lr[0] = 1 + get('lr_unit_shift') + (1 if sb128 else get('lr_unit_extra_shift'))
                    lr[1] = lr[2] = lr[0] - get('lr_uv_shift')
                fields += lr + [get('apply_grain'), get('grain_seed')]
                expected.append(row('AP', fields))
                last = next(key for key in ('clip_to_restricted_range',
                    'film_grain_params_ref_idx', 'apply_grain', 'is_global[7]',
                    'reduced_tx_set') if key in ends)
                offset = (ends[last] + 7) // 8
                summary = re.search(r'tiles (\d+)x(\d+)', '\n'.join(block))
                assert summary
                tile_count = int(summary[1]) * int(summary[2])
                if tile_count > 1:
                    assert raw[offset] == 0
                    offset += 1
                for i in range(tile_count):
                    size = len(raw) - offset
                    if i + 1 < tile_count:
                        width = get('tile_size_bytes_minus1', get('tile_size_bytes_minus_1')) + 1
                        size = int.from_bytes(raw[offset:offset + width], 'little') + 1
                        offset += width
                    assert size and offset + size <= len(raw)
                    expected.append(row('AT', [i, offset, size]))
                    offset += size
                    tiles += 1
                assert offset == len(raw)
            actual = subprocess.check_output([exe, str(path)], text=True).splitlines()
            assert actual == expected, (depth, name, actual, expected)
            print(f'AV1 Vulkan {depth}-bit {name}: {coded} coded pictures, {tiles} tile offsets/sizes match')
