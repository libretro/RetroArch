#!/usr/bin/env python3
"""Compare frame prefixes and explicit geometry with FFmpeg's AV1 trace."""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def trace_blocks(trace):
    blocks, current = [], None
    for line in trace.splitlines():
        match = re.match(r'\[trace_headers @ [^]]+\] (.*)', line)
        if not match:
            continue
        text = match[1]
        if text == 'Frame Header':
            current = []
            blocks.append(current)
        elif text == 'OBU header':
            current = None
        elif current is not None:
            current.append(text)
    return blocks


prefix_fields = {'show_existing_frame', 'frame_to_show_map_idx', 'frame_type',
    'show_frame', 'frame_presentation_time', 'showable_frame', 'error_resilient_mode',
    'disable_cdf_update', 'allow_screen_content_tools', 'force_integer_mv',
    'current_frame_id', 'display_frame_id', 'frame_size_override_flag', 'order_hint',
    'primary_ref_frame', 'buffer_removal_time_present_flag', 'buffer_removal_time',
    'refresh_frame_flags', 'ref_order_hint'}
size_fields = {'frame_width_minus_1', 'frame_height_minus_1', 'use_superres',
    'coded_denom', 'render_and_frame_size_different',
    'render_width_minus_1', 'render_height_minus_1'}
exe = str(Path(os.environ.get('AV1_FRAME_TEST', './rav1_frame_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-frame-') as directory:
    for depth in (8, 10):
        cases = [('reorder', 30, 192, 128, ['-lag-in-frames', '25']),
                 ('resilient', 12, 192, 128, ['-error-resilience', '1']),
                 ('still', 1, 192, 128, ['-still-picture', '1']),
                 ('intra', 6, 510, 514, ['-g', '1'])]
        for mode, count, width, height, options in cases:
            path = Path(directory) / f'{depth}-{mode}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                f'testsrc2=size={width}x{height}:rate=12', '-frames:v', str(count),
                '-an', '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
                *options, str(path)], check=True)
            trace = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers',
                '-f', 'null', '-'], check=True, capture_output=True, text=True).stderr
            reduced = int(re.findall(r'reduced_still_picture_header\s+([01]) = [01]', trace)[-1])
            expected, existing_count, hidden_count = [], 0, 0
            blocks = trace_blocks(trace)
            assert blocks
            for index, block in enumerate(blocks):
                values, ends = {}, {}
                for text in block:
                    field = re.match(r'(\d+)\s+(\S+)\s+([01]+) = (-?\d+)', text)
                    if field:
                        position, name, bits, value = field.groups()
                        values[name] = int(value)
                        ends[name] = int(position) + len(bits)
                existing = values.get('show_existing_frame', 0)
                end = max(value for name, value in ends.items()
                          if name.split('[')[0] in prefix_fields)
                if existing:
                    existing_count += 1
                    expected.append(f"P {index} 1 4 0 0 0 0 0 0 {values['frame_to_show_map_idx']} 0 {end}")
                    continue
                kind = values.get('frame_type', 0)
                show = values.get('show_frame', 1)
                showable = int(kind != 0) if show else values['showable_frame']
                hidden_count += not show
                resilient = int(kind == 3 or (kind == 0 and show and not reduced))
                resilient = values.get('error_resilient_mode', resilient)
                intra = kind in (0, 2)
                primary = 7 if intra or resilient else values['primary_ref_frame']
                refresh = 255 if kind == 3 or (kind == 0 and show) else values['refresh_frame_flags']
                expected.append(f"P {index} 0 {kind} {show} {showable} {resilient} "
                    f"{values.get('order_hint', 0)} {primary} {refresh} 0 "
                    f"{values.get('current_frame_id', 0)} {end}")
                if intra:
                    summary = re.search(r'size (\d+)x(\d+)\s+upscaled (\d+)\s+render (\d+)x(\d+)', '\n'.join(block))
                    assert summary
                    size_end = max(value for name, value in ends.items() if name in size_fields)
                    geometry = ' '.join(summary.groups())
                    denom = values['coded_denom'] + 9 if values.get('use_superres', 0) else 8
                    expected.append(f'S {geometry} {denom} {size_end}')
            actual = subprocess.check_output([exe, str(path)], text=True).splitlines()
            assert actual == expected, (depth, mode, actual, expected)
            if mode == 'reorder':
                assert existing_count and hidden_count
            if mode == 'still':
                assert reduced
            print(f'AV1 {depth}-bit {mode}: {len(blocks)} prefixes, '
                  f'{hidden_count} hidden, {existing_count} show-existing; fields/geometry/cursors match')
