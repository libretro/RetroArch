#!/usr/bin/env python3
"""Enlarge video blocks to stress audio-only WebM window traversal.

This fixture exercises demux/audio residency, not video decoding. Drop
position-bearing indexes and rewrite container sizes after adding padding.
"""
from pathlib import Path
import sys


def vint(data, pos, strip=True):
    first = data[pos]
    width = next(n for n in range(1, 9) if first & (1 << (8 - n)))
    value = int.from_bytes(data[pos:pos + width], 'big')
    if strip:
        value &= (1 << (7 * width)) - 1
    return value, width


def elements(data):
    pos = 0
    while pos < len(data):
        ident, ni = vint(data, pos, False)
        size, ns = vint(data, pos + ni)
        start = pos + ni + ns
        if size == (1 << (7 * ns)) - 1:
            size = len(data) - start
        end = start + size
        if end > len(data):
            raise ValueError('truncated element')
        yield ident, data[start:end]
        pos = end


def element(ident, body):
    encoded_id = ident.to_bytes((ident.bit_length() + 7) // 8, 'big')
    width = next(n for n in range(1, 9) if len(body) < (1 << (7 * n)) - 1)
    return encoded_id + ((1 << (7 * width)) | len(body)).to_bytes(width, 'big') + body


def pad(source, destination, amount=512 * 1024):
    output = bytearray()
    for ident, body in elements(Path(source).read_bytes()):
        if ident != 0x18538067:
            output += element(ident, body)
            continue
        segment = bytearray()
        for child, payload in elements(body):
            if child in (0x114D9B74, 0x1C53BB6B):  # SeekHead, Cues
                continue
            if child == 0x1F43B675:
                cluster = bytearray()
                for block, packet in elements(payload):
                    if block == 0xA3 and vint(packet, 0)[0] == 1:
                        packet += bytes(amount)
                    cluster += element(block, packet)
                payload = cluster
            segment += element(child, payload)
        output += element(ident, segment)
    Path(destination).write_bytes(output)


def insert_void(source, destination, amount=12 * 1024 * 1024, after_picture=2):
    """Put legal padding after picture two; preserve playable codec bytes."""
    output = bytearray()
    seen = 0
    for ident, body in elements(Path(source).read_bytes()):
        if ident != 0x18538067:
            output += element(ident, body)
            continue
        segment = bytearray()
        for child, payload in elements(body):
            if child in (0x114D9B74, 0x1C53BB6B):
                continue
            if child == 0x1F43B675:
                cluster = bytearray()
                for block, packet in elements(payload):
                    cluster += element(block, packet)
                    if block == 0xA3 and vint(packet, 0)[0] == 1:
                        seen += 1
                        if seen == after_picture:
                            cluster += element(0xEC, bytes(amount))
                payload = cluster
            segment += element(child, payload)
        output += element(ident, segment)
    if seen <= after_picture:
        raise ValueError('fixture needs a picture after the padding')
    Path(destination).write_bytes(output)


if __name__ == '__main__':
    if sys.argv[1] == '--void':
        insert_void(sys.argv[2], sys.argv[3], after_picture=int(sys.argv[4]) if len(sys.argv) > 4 else 2)
    else:
        pad(sys.argv[1], sys.argv[2])
