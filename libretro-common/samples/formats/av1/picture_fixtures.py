#!/usr/bin/env python3
"""Validate picture tools reached through the native intra-header parsers."""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def obus(data):
    offset = 0
    while offset < len(data):
        start = offset
        header = data[offset]
        offset += 1 + bool(header & 4)
        assert header & 2
        size = 0
        for i in range(8):
            byte = data[offset]
            offset += 1
            size |= (byte & 127) << (7 * i)
            if not byte & 128:
                break
        assert size <= len(data) - offset
        yield (header >> 3) & 15, data[start:offset + size], data[offset:offset + size]
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


def line(label, values):
    return label + ' ' + ' '.join(map(str, values))


exe = str(Path(os.environ.get('AV1_PICTURE_TEST', './rav1_picture_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-picture-') as directory:
    for depth in (8, 10):
        yuv420 = 'yuv420p' if depth == 8 else 'yuv420p10le'
        cases = [('lossy', yuv420, 8, 35, 2, []),
                 ('aq', yuv420, 8, 35, 2, ['-aq-mode', '1']),
                 ('lossless', yuv420, 8, 0, 2, []),
                 ('mono', 'gray' if depth == 8 else 'gray10le', 8, 35, 2, []),
                 ('444', 'yuv444p' if depth == 8 else 'yuv444p10le', 8, 35, 2, []),
                 ('restore', yuv420, 4, 45, 2, []),
                 ('still', yuv420, 8, 35, 1, ['-still-picture', '1'])]
        for name, pixel_format, speed, crf, count, options in cases:
            path = Path(directory) / f'{depth}-{name}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=192x128:rate=12', '-frames:v', str(count), '-an',
                '-c:v', 'libaom-av1', '-cpu-used', str(speed), '-threads', '2',
                '-g', '1', '-crf', str(crf), '-b:v', '0', '-pix_fmt', pixel_format,
                '-enable-cdef', '1', '-enable-restoration', '1', *options, str(path)], check=True)
            trace = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers', '-f', 'null', '-'],
                check=True, capture_output=True, text=True).stderr
            frames, sequence = [], None
            for kind, raw, payload in obus(path.read_bytes()):
                if kind == 1:
                    sequence = payload
                elif kind == 6:
                    assert sequence
                    frames.append((sequence, raw))
            headers = blocks(trace)
            assert len(headers) == len(frames) == count
            for index, (block, (sequence, raw)) in enumerate(zip(headers, frames)):
                values, positions = {}, {}
                for text in block:
                    field = re.match(r'(\d+)\s+(\S+)\s+[01]+ = (-?\d+)', text)
                    if field:
                        position, key, value = field.groups()
                        values[key], positions[key] = int(value), int(position)
                get = lambda key, default=0: values.get(key, default)
                q = [get('base_q_idx')] + [get('delta_q_' + key + '.delta_q')
                    for key in ('y_dc', 'u_dc', 'u_ac', 'v_dc', 'v_ac')]
                if not get('diff_uv_delta'):
                    q[4:6] = q[2:4]
                qm = [get('using_qmatrix'), get('qm_y'), get('qm_u'), get('qm_v', get('qm_u'))]
                expected = [f"B {positions['base_q_idx']} {positions['reduced_tx_set']}", line('Q', q + qm)]
                enabled, features = [], []
                maximum = [255, 63, 63, 63, 63, 7, 0, 0]
                for segment in range(8):
                    mask, data = 0, []
                    for feature in range(8):
                        active = get(f'feature_enabled[{segment}][{feature}]')
                        mask |= active << feature
                        value = get(f'feature_value[{segment}][{feature}]') if active else 0
                        data.append(max(-maximum[feature], min(maximum[feature], value)))
                    enabled.append(mask)
                    features.append(data)
                last = max([i for i, mask in enumerate(enabled) if mask] or [0])
                seg = get('segmentation_enabled')
                expected.append(line('S', [seg, seg, 0, seg, last, int(any(mask & 0xe0 for mask in enabled))]))
                expected += [line('G', [i, enabled[i], *features[i]]) for i in range(8)]
                lossless = 0
                for i in range(8):
                    qindex = max(0, min(255, q[0] + (features[i][0] if enabled[i] & 1 else 0)))
                    if not qindex and not any(q[1:]):
                        lossless |= 1 << i
                coded_lossless = int(lossless == 255)
                summary = re.search(r'size (\d+)x\d+\s+upscaled (\d+)', '\n'.join(block))
                assert summary
                all_lossless = int(coded_lossless and summary[1] == summary[2])
                expected.append(line('D', [get(key) for key in ('delta_q_present', 'delta_q_res',
                    'delta_lf_present', 'delta_lf_res', 'delta_lf_multi')] + [lossless, coded_lossless, all_lossless]))
                ref_deltas = [1, 0, 0, 0, -1, 0, -1, -1]
                ref_deltas = [get(f'loop_filter_ref_deltas[{i}]', value) for i, value in enumerate(ref_deltas)]
                expected.append(line('L', [get(f'loop_filter_level[{i}]') for i in range(4)] +
                    [get('loop_filter_sharpness'), get('loop_filter_delta_enabled'), get('loop_filter_delta_update')] +
                    ref_deltas + [get(f'loop_filter_mode_deltas[{i}]') for i in range(2)]))
                cdef = [get('cdef_damping_minus_3') + 3, get('cdef_bits')]
                for i in range(1 << cdef[1]):
                    for plane in ('y', 'uv'):
                        secondary = get(f'cdef_{plane}_sec_strength[{i}]')
                        cdef += [get(f'cdef_{plane}_pri_strength[{i}]'), 4 if secondary == 3 else secondary]
                expected.append(line('C', cdef))
                types = [(0, 3, 1, 2)[get(f'lr_type[{i}]')] for i in range(3)]
                sizes = [0, 0, 0]
                if any(types):
                    sb128 = int(re.findall(r'use_128x128_superblock\s+([01]) = [01]', trace)[-1])
                    shift = get('lr_unit_shift') + (1 if sb128 else get('lr_unit_extra_shift'))
                    sizes[0] = 64 << shift
                    sizes[1] = sizes[2] = sizes[0] >> get('lr_uv_shift')
                expected.append(line('R', [value for pair in zip(types, sizes) for value in pair]))
                expected.append(line('T', [get('tx_mode')]))
                seq_path, frame_path = Path(directory) / 'sequence.bin', Path(directory) / 'frame.bin'
                seq_path.write_bytes(sequence)
                frame_path.write_bytes(raw)
                actual = subprocess.check_output([exe, str(seq_path), str(frame_path)], text=True).splitlines()
                assert actual == expected, (depth, name, index, actual, expected)
            print(f'AV1 {depth}-bit {name}: {count} picture tool blocks and native start/end cursors match')
