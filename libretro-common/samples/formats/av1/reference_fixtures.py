#!/usr/bin/env python3
"""Compare native reference/frame setup and inherited picture metadata."""
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


def line(label, values):
    return label + ' ' + ' '.join(map(str, values))


exe = str(Path(os.environ.get('AV1_REFERENCE_TEST', './rav1_reference_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-reference-') as directory:
    for depth in (8, 10):
        for mode, count, options in (
                ('reorder', 30, ['-lag-in-frames', '25']),
                ('resilient', 12, ['-error-resilience', '1']),
                ('aq', 18, ['-aq-mode', '1']),
                ('still', 1, ['-still-picture', '1'])):
            path = Path(directory) / f'{depth}-{mode}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=192x128:rate=12', '-frames:v', str(count), '-an',
                '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
                *options, str(path)], check=True)
            trace = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers',
                '-f', 'null', '-'], check=True, capture_output=True, text=True).stderr
            expected, inherited, coded = [], 0, 0
            slots = [None] * 8
            reduced = int(re.findall(r'reduced_still_picture_header\s+([01]) = [01]', trace)[-1])
            for block in blocks(trace):
                values, positions = {}, {}
                for text in block:
                    match = re.match(r'(\d+)\s+(\S+)\s+[01]+ = (-?\d+)', text)
                    if match:
                        position, key, value = match.groups()
                        values[key], positions[key] = int(value), int(position)
                get = lambda key, default=0: values.get(key, default)
                if get('show_existing_frame'):
                    continue
                intra = get('frame_type') in (0, 2)
                short = get('frame_refs_short_signaling')
                # libaom sends explicit references in this generated corpus.
                assert not short
                indices = [get(f'ref_frame_idx[{i}]') for i in range(7)]
                filt = 0 if intra else 4 if get('is_filter_switchable') else get('interpolation_filter')
                expected.append(line('H', [short, get('allow_intrabc'),
                    get('allow_high_precision_mv'), filt, get('is_motion_mode_switchable'),
                    get('use_ref_frame_mvs'), positions['uniform_tile_spacing_flag']]))
                summary = re.search(r'size (\d+)x(\d+)\s+upscaled (\d+)\s+render (\d+)x(\d+)', '\n'.join(block))
                assert summary
                denom = get('coded_denom') + 9 if get('use_superres') else 8
                cdf = get('disable_frame_end_update_cdf', int(reduced or get('disable_cdf_update')))
                expected.append(line('S', [*map(int, summary.groups()), denom, cdf]))
                if not intra:
                    expected.append(line('I', indices))
                primary = get('primary_ref_frame', 7)
                if primary == 7:
                    deltas = [1, 0, 0, 0, -1, 0, -1, -1, 0, 0]
                    segments = [[0] * 9 for _ in range(8)]
                else:
                    deltas, segments = copy.deepcopy(slots[indices[primary]])
                    inherited += 1
                if not get('segmentation_enabled'):
                    segments = [[0] * 9 for _ in range(8)]
                elif get('segmentation_update_data', int(primary == 7)):
                    segments = []
                    for i in range(8):
                        mask, data = 0, []
                        for j, maximum in enumerate((255, 63, 63, 63, 63, 7, 0, 0)):
                            active = get(f'feature_enabled[{i}][{j}]')
                            mask |= active << j
                            value = get(f'feature_value[{i}][{j}]') if active else 0
                            data.append(max(-maximum, min(maximum, value)))
                        segments.append([mask, *data])
                for i in range(8):
                    deltas[i] = get(f'loop_filter_ref_deltas[{i}]', deltas[i])
                for i in range(2):
                    deltas[8 + i] = get(f'loop_filter_mode_deltas[{i}]', deltas[8 + i])
                end = positions['reduced_tx_set'] if intra else positions['reference_select']
                expected.append(line('P', [positions['base_q_idx'], end,
                    get('base_q_idx'), get('tx_mode'), deltas[0], deltas[4]]))
                expected.append(line('M', deltas + [v for segment in segments for v in segment]))
                # Generated streams have identity global models; nonidentity
                # models and predictors are exercised by the synthetic tests.
                assert not any(get(f'is_global[{i}]') for i in range(1, 8))
                global_end = positions['reduced_tx_set'] + 1 if intra else max(
                    positions[f'is_global[{i}]'] + 1 for i in range(1, 8))
                expected.append(line('T', [get('reference_select'), get('skip_mode_present'),
                    get('allow_warped_motion'), get('reduced_tx_set'), global_end]))
                expected += [line('G', [0, 0, 0, 65536, 0, 0, 65536])] * 7
                refresh = get('refresh_frame_flags', 255)
                for i in range(8):
                    if refresh & (1 << i):
                        slots[i] = copy.deepcopy((deltas, segments))
                coded += 1
            actual = subprocess.check_output([exe, str(path)], text=True).splitlines()
            assert actual == expected, (depth, mode, actual, expected)
            assert coded and (mode in ('still', 'resilient') or inherited)
            print(f'AV1 {depth}-bit {mode}: {coded} coded setups, {inherited} inherited '
                  'picture blocks; references/geometry/motion flags/metadata/cursors match')
