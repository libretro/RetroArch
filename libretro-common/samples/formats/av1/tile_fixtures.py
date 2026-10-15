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
info_exe = str(Path(os.environ.get('AV1_TILE_INFO_TEST', './rav1_tile_info_test')).resolve())
with tempfile.TemporaryDirectory(prefix='rav1-tiles-') as directory:
    for depth in (8, 10):
        cases = [(2, 2, 512, 512), (4, 1, 512, 512), (4, 2, 512, 512),
                 (8, 8, 512, 512), (3, 2, 512, 512), (2, 3, 512, 512),
                 (2, 2, 510, 514)]
        for columns, rows, width, height in cases:
            path = Path(directory) / f'{depth}-{columns}x{rows}-{width}x{height}.obu'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                f'testsrc2=size={width}x{height}:rate=1', '-frames:v', '1', '-an',
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
            info_start, uniform = re.findall(r'\] (\d+)\s+uniform_tile_spacing_flag\s+([01]) = [01]', traced)[-1]
            size_start = int(re.findall(r'\] (\d+)\s+tile_size_bytes_minus1', traced)[-1])
            context_id = int(re.findall(r'context_update_tile_id\s+[01]+ = (\d+)', traced)[-1])
            sb = int(re.findall(r'use_128x128_superblock\s+([01]) = [01]', traced)[-1])
            shift = 5 if sb else 4
            mi_cols, mi_rows = 2 * ((width + 7) // 8), 2 * ((height + 7) // 8)
            if int(uniform):
                col_log = int(re.findall(r'tile_cols_log2\s+[01]+ = (\d+)', traced)[-1])
                row_log = int(re.findall(r'tile_rows_log2\s+[01]+ = (\d+)', traced)[-1])
                sb_cols = (mi_cols + (1 << shift) - 1) >> shift
                sb_rows = (mi_rows + (1 << shift) - 1) >> shift
                tile_width = (sb_cols + (1 << col_log) - 1) >> col_log
                tile_height = (sb_rows + (1 << row_log) - 1) >> row_log
                col_starts = [x << shift for x in range(0, sb_cols, tile_width)] + [mi_cols]
                row_starts = [y << shift for y in range(0, sb_rows, tile_height)] + [mi_rows]
            else:
                col_log, row_log = (columns - 1).bit_length(), (rows - 1).bit_length()
                widths = [int(x) + 1 for x in re.findall(r'width_in_sbs_minus_1\[\d+\]\s+[01]* = (\d+)', traced)]
                heights = [int(x) + 1 for x in re.findall(r'height_in_sbs_minus_1\[\d+\]\s+[01]* = (\d+)', traced)]
                col_starts, row_starts = [0], [0]
                for value in widths:
                    col_starts.append(col_starts[-1] + (value << shift))
                for value in heights:
                    row_starts.append(row_starts[-1] + (value << shift))
                col_starts[-1], row_starts[-1] = mi_cols, mi_rows
            assert len(col_starts) == columns + 1 and len(row_starts) == rows + 1
            info_expected = [f'I {columns} {rows} {col_log} {row_log} {size_bytes} {context_id} {size_start + 2}',
                'C ' + ' '.join(map(str, col_starts)), 'R ' + ' '.join(map(str, row_starts))]
            for line in expected:
                fields = line.split()
                fields[3] = str(int(fields[3]) + bit // 8)
                info_expected.append('T ' + ' '.join(fields))
            frame_file = Path(directory) / 'frame.bin'
            frame_file.write_bytes(frames[0])
            info_actual = subprocess.check_output([info_exe, str(frame_file), info_start,
                str(bit // 8), str(width), str(height), str(sb)], text=True).splitlines()
            assert info_actual == info_expected, (depth, columns, rows, info_actual, info_expected)
            print(f'AV1 {depth}-bit {columns}x{rows} {width}x{height}: tile info, boundaries and borrowed tile hashes match')
