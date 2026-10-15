#!/usr/bin/env python3
"""Compare film-grain metadata and native header-to-tile boundaries."""
from pathlib import Path
import copy
import os
import re
import subprocess
import tempfile


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


def frames(data):
    offset = 0
    while offset < len(data):
        start, header = offset, data[offset]
        offset += 1 + bool(header & 4)
        size = 0
        for i in range(8):
            byte = data[offset]
            offset += 1
            size |= (byte & 127) << (7 * i)
            if not byte & 128:
                break
        assert header & 2 and size <= len(data) - offset
        if (header >> 3) & 15 == 6:
            yield data[start:offset + size]
        offset += size


def line(label, values):
    return label + ' ' + ' '.join(map(str, values))


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


exe = str(Path(os.environ.get('AV1_REFERENCE_TEST', './rav1_reference_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-grain-') as directory:
    for depth in (8, 10):
        pixel = 'yuv420p' if depth == 8 else 'yuv420p10le'
        cases = [(str(i), pixel, i, []) for i in range(1, 9)]
        cases += [('mono', 'gray' if depth == 8 else 'gray10le', 1, []),
                  ('tiles', pixel, 1, ['-tiles', '2x2'])]
        for name, pixel_format, vector, options in cases:
            path = Path(directory) / f'{depth}-{name}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=256x256:rate=12', '-frames:v', '6', '-an',
                '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', pixel_format, '-aom-params', f'film-grain-test={vector}',
                *options, str(path)], check=True)
            trace = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers',
                '-f', 'null', '-'], check=True, capture_output=True, text=True).stderr
            raw_frames = iter(frames(path.read_bytes()))
            expected, slots, count, tile_count, inherited = [], [None] * 8, 0, 0, 0
            for block in blocks(trace):
                values, ends = {}, {}
                for text in block:
                    match = re.match(r'(\d+)\s+(\S+)\s+([01]+) = (-?\d+)', text)
                    if match:
                        position, key, bits, value = match.groups()
                        values[key], ends[key] = int(value), int(position) + len(bits)
                get = lambda key, default=0: values.get(key, default)
                if get('show_existing_frame'):
                    continue
                raw = next(raw_frames)
                apply = get('apply_grain')
                update = get('update_grain', int(bool(apply)))
                selector = get('film_grain_params_ref_idx')
                fields = [0] * 11
                points = [[0] * 29 for _ in range(3)]
                coeffs = [[0] * 25 for _ in range(3)]
                mult = [0] * 6
                if apply and not update:
                    fields, points, coeffs, mult = copy.deepcopy(slots[selector])
                    fields[1] = get('grain_seed')
                    inherited += 1
                elif apply:
                    fields = [apply, get('grain_seed'), update, selector,
                        get('chroma_scaling_from_luma'), get('grain_scaling_minus_8') + 8,
                        get('ar_coeff_lag'), get('ar_coeff_shift_minus_6') + 6,
                        get('grain_scale_shift'), get('overlap_flag'), get('clip_to_restricted_range')]
                    num_y = get('num_y_points')
                    for plane, component in enumerate(('y', 'cb', 'cr')):
                        n = get(f'num_{component}_points')
                        points[plane][0] = n
                        for i in range(n):
                            points[plane][1 + 2 * i] = get(f'point_{component}_value[{i}]')
                            points[plane][2 + 2 * i] = get(f'point_{component}_scaling[{i}]')
                        active = n or (plane and fields[4])
                        total = 2 * fields[6] * (fields[6] + 1) + int(bool(plane and num_y))
                        if active:
                            for i in range(total):
                                coeffs[plane][i] = get(f'ar_coeffs_{component}_plus_128[{i}]') - 128
                    for plane, component in enumerate(('cb', 'cr')):
                        mult[3 * plane:3 * plane + 3] = [get(f'{component}_{key}') for key in
                            ('mult', 'luma_mult', 'offset')]
                fields[2:4] = [update, selector]
                last = next((key for key in ('clip_to_restricted_range',
                    'film_grain_params_ref_idx', 'apply_grain', 'is_global[7]',
                    'reduced_tx_set') if key in ends), None)
                assert last
                grain_end = ends[last]
                expected.append(line('FG', [*fields, grain_end]))
                for plane in range(3):
                    expected.append(line('FP', [plane, *points[plane]]))
                    expected.append(line('FC', [plane, *coeffs[plane]]))
                expected.append(line('FM', mult))
                summary = re.search(r'tiles (\d+)x(\d+)', '\n'.join(block))
                assert summary
                tiles = int(summary[1]) * int(summary[2])
                offset = (grain_end + 7) // 8
                if tiles > 1:
                    assert raw[offset] == 0
                    offset += 1
                for i in range(tiles):
                    size = len(raw) - offset
                    if i + 1 < tiles:
                        width = get('tile_size_bytes_minus1', get('tile_size_bytes_minus_1')) + 1
                        size = int.from_bytes(raw[offset:offset + width], 'little') + 1
                        offset += width
                    payload = raw[offset:offset + size]
                    assert len(payload) == size and size
                    expected.append(line('FT', [i, size, fnv(payload)]))
                    offset += size
                    tile_count += 1
                assert offset == len(raw)
                refresh = get('refresh_frame_flags', 255)
                for i in range(8):
                    if refresh & (1 << i):
                        slots[i] = copy.deepcopy((fields, points, coeffs, mult))
                count += 1
            actual = subprocess.check_output([exe, '--grain', str(path)], text=True).splitlines()
            actual = [row for row in actual if row.startswith(('FG ', 'FP ', 'FC ', 'FM ', 'FT '))]
            assert actual == expected, (depth, name, actual, expected)
            assert count and tile_count
            print(f'AV1 {depth}-bit grain {name}: {count} headers, {tile_count} borrowed tiles, '
                  f'{inherited} grain inheritances; metadata/cursors/tile hashes match')
