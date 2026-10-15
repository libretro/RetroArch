#!/usr/bin/env python3
"""Use FFmpeg's frame-header trace to locate real AV1 tile groups."""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return f'{value:08x}'


def obus(data):
    offset = 0
    while offset < len(data):
        start = offset
        header = data[offset]
        offset += 1 + bool(header & 4)
        assert header & 2
        length = 0
        for i in range(8):
            byte = data[offset]
            offset += 1
            length |= (byte & 127) << (7 * i)
            if not byte & 128:
                break
        assert length <= len(data) - offset
        yield (header >> 3) & 15, data[start:offset + length]
        offset += length


exe = str(Path(os.environ.get('AV1_TILES_TEST', './rav1_tiles_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-tiles-') as directory:
    for depth in (8, 10):
        for columns, rows in ((2, 2), (4, 1), (4, 2), (8, 8)):
            path = Path(directory) / f'{depth}-{columns}x{rows}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'testsrc2=size=512x512:rate=1', '-frames:v', '1', '-an',
                '-c:v', 'libaom-av1', '-cpu-used', '8', '-threads', '2',
                '-pix_fmt', 'yuv420p' if depth == 8 else 'yuv420p10le',
                '-tiles', f'{columns}x{rows}', str(path)], check=True)
            traced = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'trace',
                '-i', str(path), '-c', 'copy', '-bsf:v', 'trace_headers',
                '-f', 'null', '-'], check=True, capture_output=True, text=True).stderr
            # Trace bit positions include the OBU header and its size field.
            starts = re.findall(r'\] (\d+)\s+tile_start_and_end_present_flag\s+0 = 0', traced)
            sizes = re.findall(r'tile_size_bytes_minus1\s+[01]+ = (\d+)', traced)
            frames = [data for kind, data in obus(path.read_bytes()) if kind == 6]
            assert len(starts) == len(sizes) == len(frames) == 1
            assert f'tiles {rows}x{columns}.' in traced
            assert int(re.findall(r'tile_cols_log2\s+[01]+ = (\d+)', traced)[-1]) == (columns - 1).bit_length()
            assert int(re.findall(r'tile_rows_log2\s+[01]+ = (\d+)', traced)[-1]) == (rows - 1).bit_length()
            bit = int(starts[0])
            assert not bit % 8
            group = frames[0][bit // 8:]
            size_bytes = int(sizes[0]) + 1
            assert group[0] == 0
            expected = []
            offset = 1
            for tile in range(columns * rows):
                if tile + 1 == columns * rows:
                    length = len(group) - offset
                else:
                    length = int.from_bytes(group[offset:offset + size_bytes], 'little') + 1
                    offset += size_bytes
                assert 0 < length <= len(group) - offset
                expected.append(f'{tile} {tile // columns} {tile % columns} '
                    f'{offset} {length} {fnv(group[offset:offset + length])}')
                offset += length
            assert offset == len(group)
            payload = Path(directory) / 'tile-group.bin'
            payload.write_bytes(group)
            actual = subprocess.check_output([exe, str(payload), str(columns), str(rows),
                str((columns - 1).bit_length()), str((rows - 1).bit_length()),
                str(size_bytes)], text=True).splitlines()
            assert actual == expected, (depth, columns, rows, actual, expected)
            print(f'AV1 {depth}-bit {columns}x{rows}: borrowed tile offsets, sizes and hashes match')
