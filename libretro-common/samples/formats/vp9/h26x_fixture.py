#!/usr/bin/env python3
"""Portable AVC/HEVC fixture helpers."""
from pathlib import Path
import re
import subprocess


def executable(name):
    path = Path(name).resolve()
    if not path.is_file() and path.with_suffix('.exe').is_file():
        path = path.with_suffix('.exe')
    if not path.is_file():
        raise FileNotFoundError(path)
    return str(path)


def elements(blob, begin, end):
    offset = begin
    while offset < end:
        start = offset
        width = 1
        while width <= 4 and not blob[offset] & (1 << (8 - width)):
            width += 1
        if width > 4 or offset + width >= end:
            raise ValueError('Invalid fixture element ID')
        element = int.from_bytes(blob[offset:offset + width], 'big')
        offset += width
        size_at = offset
        width = 1
        while width <= 8 and not blob[offset] & (1 << (8 - width)):
            width += 1
        if width > 8 or offset + width > end:
            raise ValueError('Invalid fixture element size')
        size = int.from_bytes(blob[offset:offset + width], 'big') & ((1 << (7 * width)) - 1)
        offset += width
        if offset + size > end:
            raise ValueError('Fixture element exceeds its parent')
        yield element, start, size_at, width, offset, offset + size
        offset += size


def hide_element(data, element):
    wanted = int.from_bytes(element, 'big')
    output = bytearray(data)
    found = False

    def visit(begin, end):
        nonlocal found
        for kind, start, size_at, width, body, stop in elements(data, begin, end):
            if kind == wanted:
                output[size_at - 1] += 1
                found = True
            elif kind == 0xbf:
                output[start] = 0xec
            elif kind in (0x18538067, 0x1654ae6b, 0xae, 0xe0):
                visit(body, stop)

    visit(0, len(data))
    if not found:
        raise ValueError('Expected fixture header element is absent')
    return bytes(output)


def hevc_nonoutput_fixture(ffmpeg, root, size=64, reordered=False, discard_prior=False):
    if discard_prior and not reordered:
        raise ValueError('Prior-output discard fixture needs reordering')
    coded_pictures = 29 if discard_prior else 25
    raw = root / 'all-i.hevc'
    rewritten = root / 'nonoutput.hevc'
    movie = root / 'nonoutput.mkv'
    reference = root / 'nonoutput.rgba'
    times = root / 'nonoutput.txt'
    source = root / 'reordered.mkv' if reordered else raw
    command = [ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', f'testsrc2=size={size}x{size}:rate=25',
                    '-frames:v', str(coded_pictures), '-c:v', 'libx265', '-pix_fmt', 'yuv420p',
                    '-x265-params',
                    'pools=none:frame-threads=1:keyint=12:aud=1:log-level=error'
                    + (':bframes=3' if reordered else ':bframes=0')
                    + (':open-gop=0' if discard_prior else '')]
    if not reordered:
        command += ['-force_key_frames', 'expr:gte(t,n_forced/25)']
    subprocess.run(command + ['-y', str(source)], check=True)
    if reordered:
        subprocess.run([ffmpeg, '-v', 'error', '-i', str(source), '-c:v', 'copy',
                        '-bsf:v', 'hevc_mp4toannexb', '-f', 'hevc', '-y', str(raw)], check=True)
    trace = subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'info', '-i', str(raw),
                            '-c:v', 'copy', '-bsf:v', 'trace_headers', '-f', 'null', '-'],
                           check=True, capture_output=True, text=True).stderr
    data = raw.read_bytes()
    starts = list(re.finditer(rb'\x00\x00(?:\x00)?\x01', data))
    nals = [data[m.end():starts[i + 1].start() if i + 1 < len(starts) else len(data)]
            for i, m in enumerate(starts)]
    slice_ends = []
    output_offsets = []
    for line in trace.splitlines():
        found = re.search(r'\]\s+(\d+)\s+slice_type\s+([01]+)\s+=', line)
        if found:
            output_offsets.append(int(found[1]) + len(found[2]))
        found = re.search(r'\]\s+(\d+)\s+alignment_bit_equal_to_one\s+', line)
        if found:
            slice_ends.append(int(found[1]))
        found = re.search(r'\]\s+(\d+)\s+output_flag_present_flag\s+0\s+=', line)
        if found and int(found[1]) != 19:
            raise ValueError('Unexpected fixture PPS layout')
    if len(slice_ends) != coded_pictures or len(output_offsets) != coded_pictures:
        raise ValueError('Unexpected number of fixture slice headers')

    def unescape(nal):
        result = bytearray(nal[:2])
        zeros = 0
        for value in nal[2:]:
            if zeros == 2 and value == 3:
                zeros = 0
                continue
            result.append(value)
            zeros = zeros + 1 if value == 0 else 0
        return ''.join(f'{value:08b}' for value in result)

    def escape(bits):
        result = bytearray()
        zeros = 0
        for offset in range(0, len(bits), 8):
            value = int(bits[offset:offset + 8], 2)
            if offset >= 16 and zeros == 2 and value <= 3:
                result.append(3)
                zeros = 0
            result.append(value)
            zeros = zeros + 1 if value == 0 else 0
        return bytes(result)

    output = bytearray()
    replacements = {}
    picture = 0
    for nal in nals:
        kind = (nal[0] >> 1) & 63
        bits = unescape(nal)
        if discard_prior and kind in (19, 20) and picture:
            bits = bits[:17] + '1' + bits[18:]
        if kind == 34:
            if bits[19] != '0':
                raise ValueError('Fixture already has explicit output flags')
            bits = bits[:19] + '1' + bits[20:]
        elif kind <= 31:
            insert = output_offsets[picture]
            end = slice_ends[picture]
            if bits[end] != '1':
                raise ValueError('Fixture byte alignment is absent')
            prefix = bits[:insert] + ('0' if picture == 2 else '1') + bits[insert:end] + '1'
            prefix += '0' * (-len(prefix) % 8)
            bits = prefix + bits[((end + 8) // 8) * 8:]
            picture += 1
        changed = escape(bits)
        replacements[nal] = changed
        output += b'\x00\x00\x00\x01' + changed
    if picture != coded_pictures:
        raise ValueError('Unexpected number of coded fixture pictures')
    rewritten.write_bytes(output)
    if reordered:
        data = source.read_bytes()
        has_reordering = False
        for segment in elements(data, 0, len(data)):
            if segment[0] != 0x18538067:
                continue
            for cluster in elements(data, segment[4], segment[5]):
                if cluster[0] != 0x1f43b675:
                    continue
                timestamps = [int.from_bytes(data[row[4] + 1:row[4] + 3], 'big', signed=True)
                              for row in elements(data, cluster[4], cluster[5]) if row[0] == 0xa3]
                has_reordering |= any(a > b for a, b in zip(timestamps, timestamps[1:]))
        if not has_reordering:
            raise ValueError('Fixture has no reordered block timestamps')

        def packet(payload, width):
            result = bytearray()
            offset = 0
            while offset < len(payload):
                length = int.from_bytes(payload[offset:offset + width], 'big')
                offset += width
                if not length or offset + length > len(payload):
                    raise ValueError('Invalid fixture NAL length')
                nal = payload[offset:offset + length]
                changed = replacements.get(nal, nal)
                result += len(changed).to_bytes(width, 'big') + changed
                offset += length
            return bytes(result)

        def rebuild(begin, end):
            result = bytearray()
            for element, start, size_at, width, payload, stop in elements(data, begin, end):
                identity = data[start:size_at]
                value = data[payload:stop]
                if element in (0x18538067, 0x1654ae6b, 0xae, 0x1f43b675, 0xa0):
                    value = rebuild(payload, stop)
                elif element in (0x114d9b74, 0x1c53bb6b):
                    identity = b'\xec'
                elif element == 0xbf:
                    identity = b'\xec'
                elif element == 0x63a2:
                    config = bytearray(value[:23])
                    offset = 23
                    for _ in range(value[22]):
                        config += value[offset:offset + 3]
                        count = int.from_bytes(value[offset + 1:offset + 3], 'big')
                        offset += 3
                        for _ in range(count):
                            length = int.from_bytes(value[offset:offset + 2], 'big')
                            config += packet(value[offset:offset + 2 + length], 2)
                            offset += 2 + length
                    if offset != len(value):
                        raise ValueError('Invalid fixture HEVC configuration')
                    value = bytes(config)
                elif element in (0xa3, 0xa1):
                    if value[0] != 0x81 or value[3] & 6:
                        raise ValueError('Expected an unlaced video-only block')
                    value = value[:4] + packet(value[4:], 4)
                width = max(width, (len(value).bit_length() + 7) // 7)
                result += identity + ((1 << (7 * width)) | len(value)).to_bytes(width, 'big') + value
            return bytes(result)

        movie.write_bytes(rebuild(0, len(data)))
    else:
        subprocess.run([ffmpeg, '-v', 'error', '-r', '25', '-i', str(rewritten),
                        '-c:v', 'copy', '-y', str(movie)], check=True)
    subprocess.run([ffmpeg, '-v', 'error', '-i', str(movie), '-fps_mode', 'passthrough',
                    '-f', 'rawvideo', '-sws_flags', 'neighbor+bitexact', '-pix_fmt', 'rgba',
                    '-y', str(reference)], check=True)
    if reference.stat().st_size != 24 * size * size * 4:
        raise ValueError('Independent decoder did not return 24 visible pictures')
    if reordered:
        shown = subprocess.run([ffmpeg, '-hide_banner', '-i', str(movie), '-vf', 'showinfo',
                                '-fps_mode', 'passthrough', '-f', 'null', '-'],
                               check=True, capture_output=True, text=True).stderr
        timestamps = [round(float(t) * 1000) for t in re.findall(r'\bpts_time:([\d.-]+)', shown)]
        if len(timestamps) != 24 or any(a >= b for a, b in zip(timestamps, timestamps[1:])):
            raise ValueError('Independent decoder did not return 24 ordered timestamps')
    else:
        timestamps = [i * 40 for i in range(25) if i != 2]
    times.write_text(''.join(f'{t}\n' for t in timestamps), encoding='ascii')
    return movie, reference, times
