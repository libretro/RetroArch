#!/usr/bin/env python3
"""Validate reference transactions against an independent slot model."""
from pathlib import Path
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


def headers(data):
    offset = 0
    while offset < len(data):
        first = data[offset]
        offset += 1 + bool(first & 4)
        size = 0
        for i in range(8):
            byte = data[offset]
            offset += 1
            size |= (byte & 127) << (7 * i)
            if not byte & 128:
                break
        assert first & 2 and size <= len(data) - offset
        if (first >> 3) & 15 in (3, 6):
            yield offset, offset + size
        offset += size


exe = str(Path(os.environ.get('AV1_REFERENCE_TEST', './rav1_reference_test')).resolve())
hw_exe = str(Path(os.environ.get('AV1_HW_TEST', './rav1_hw_session_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-state-') as directory:
    for depth in (8, 10):
        for mode, count, options in (
                ('reorder', 30, ['-lag-in-frames', '25']),
                ('resilient', 12, ['-error-resilience', '1']),
                ('intra', 6, ['-g', '1']),
                ('grain', 18, ['-aom-params', 'film-grain-test=5'])):
            path = Path(directory) / f'{depth}-{mode}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=192x128:rate=12', '-frames:v', str(count), '-an',
                '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
                *options, str(path)], check=True)
            trace = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers',
                '-f', 'null', '-'], check=True, capture_output=True, text=True).stderr
            data = path.read_bytes()
            spans = iter(headers(data))
            slots, expected, epoch, replays, rejected = [None] * 8, [], 1, 0, 0
            coded = shown = 0
            for index, block in enumerate(blocks(trace)):
                values = {}
                for text in block:
                    match = re.match(r'\d+\s+(\S+)\s+[01]+ = (-?\d+)', text)
                    if match:
                        values[match[1]] = int(match[2])
                get = lambda key, default=0: values.get(key, default)
                start, end = next(spans)
                existing = get('show_existing_frame')
                if existing:
                    selected = get('frame_to_show_map_idx')
                    identity, kind, showable = slots[selected]
                    assert showable
                    refresh = 255 if kind == 0 else 0
                    replays += 1
                    shown += 1
                    if not rejected:
                        invalid = next((i for i, slot in enumerate(slots)
                            if slot is None or not slot[2]), None)
                        if invalid is not None:
                            broken = bytearray(data[:end])
                            broken[start] = (broken[start] & 0x8f) | (invalid << 4)
                            bad = Path(directory) / 'bad-replay.obu'
                            bad.write_bytes(broken)
                            result = subprocess.run([exe, '--state', str(bad)],
                                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
                            assert result.returncode != 0
                            result = subprocess.run([hw_exe, str(bad)], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
                            assert result.returncode != 0
                            rejected += 1
                    if kind == 0:
                        slots = [(identity, kind, False)] * 8
                else:
                    identity = epoch + 1
                    kind = get('frame_type')
                    show = get('show_frame')
                    coded += 1
                    shown += show
                    showable = kind != 0 if show else get('showable_frame')
                    refresh = get('refresh_frame_flags', 255)
                    for i in range(8):
                        if refresh & (1 << i):
                            slots[i] = (identity, kind, showable)
                epoch += 1
                expected.append(f'RS {index} {existing} {kind} {refresh} {identity} {epoch}')
            actual = subprocess.check_output([exe, '--state', str(path)], text=True).splitlines()
            actual = [row for row in actual if row.startswith('RS ')]
            assert actual == expected, (depth, mode, actual, expected)
            hardware = subprocess.check_output([hw_exe, str(path)], text=True).strip()
            assert hardware == f'HW {len(expected)} {coded} {shown} {shown}', hardware
            if mode == 'reorder':
                assert replays and rejected
            print(f'AV1 {depth}-bit {mode}: {len(expected)} transactions, {replays} replays, '
                  f'{rejected} invalid replay rejected; identities/refresh masks/epochs and mocked HW submission/output match')
