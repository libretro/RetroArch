#!/usr/bin/env python3
# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT
"""Decode independently GPU-produced packets through the public reference ABI."""
import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import struct

parser = argparse.ArgumentParser()
parser.add_argument('--reference-dll', type=Path, required=True)
parser.add_argument('--codec-dll', type=Path, required=True)
parser.add_argument('--input-directory', type=Path, required=True)
parser.add_argument('--pattern', default='*.pyro')
parser.add_argument('--output', type=Path, required=True)
options = parser.parse_args()
if os.name != 'nt':
    raise SystemExit('This optional oracle harness requires native Windows DLLs')
directory = os.add_dll_directory(str(options.reference_dll.resolve().parent))
reference = c.CDLL(str(options.reference_dll.resolve()))
independent = c.CDLL(str(options.codec_dll.resolve()))

class Sequence(c.Structure):
    _fields_ = [('total_blocks', c.c_uint32)] + [(n, c.c_uint) for n in
        ['width', 'height', 'sequence', 'chroma_444', 'primaries', 'transfer',
         'transform', 'limited', 'left']]

class Image(c.Structure):
    _fields_ = [('planes', c.POINTER(c.c_float) * 3), ('stride', c.c_size_t),
                ('sequence', Sequence)]

class DecoderInfo(c.Structure):
    _fields_ = [('device', c.c_void_p), ('width', c.c_int), ('height', c.c_int),
                ('chroma', c.c_int), ('fragment', c.c_bool)]

class CpuBuffer(c.Structure):
    _fields_ = [('data', c.c_void_p * 3), ('stride', c.c_size_t * 3),
                ('size', c.c_size_t * 3), ('width', c.c_int),
                ('height', c.c_int), ('format', c.c_int)]

def bind(lib, name, args, result=c.c_int):
    function = getattr(lib, name)
    function.argtypes, function.restype = args, result
    return function

new_device = bind(reference, 'pyrowave_create_default_device', [c.POINTER(c.c_void_p)])
free_device = bind(reference, 'pyrowave_device_destroy', [c.c_void_p], None)
new_ref = bind(reference, 'pyrowave_decoder_create', [c.POINTER(DecoderInfo), c.POINTER(c.c_void_p)])
free_ref = bind(reference, 'pyrowave_decoder_destroy', [c.c_void_p], None)
push = bind(reference, 'pyrowave_decoder_push_packet', [c.c_void_p, c.c_void_p, c.c_size_t])
decode_ref = bind(reference, 'pyrowave_decoder_decode_cpu_buffer_synchronous',
                  [c.c_void_p, c.POINTER(CpuBuffer)])
new_own = bind(independent, 'rpyrowave_decoder_new', [c.c_uint] * 3 + [c.c_size_t], c.c_void_p)
free_own = bind(independent, 'rpyrowave_decoder_free', [c.c_void_p], None)
decode_own = bind(independent, 'rpyrowave_decode_frame',
                  [c.c_void_p, c.c_void_p, c.c_size_t, c.c_uint, c.POINTER(Image)])

def checked(result):
    if result != 0:
        raise RuntimeError(f'Codec returned {result}')

files = sorted(options.input_directory.glob(options.pattern))
if not files:
    raise SystemExit('No independently encoded frames found')
device = c.c_void_p()
checked(new_device(c.byref(device)))
report = []
try:
    for path in files:
        stream = path.read_bytes()
        dimensions, flags = struct.unpack_from('<II', stream)
        width, height = (dimensions & 16383) + 1, ((dimensions >> 14) & 16383) + 1
        is444 = (flags >> 26) & 1
        own = new_own(width, height, is444, 16 * 1024 * 1024)
        ref = c.c_void_p()
        if not own:
            raise RuntimeError(f'{path}: independent decoder allocation failed')
        try:
            checked(new_ref(c.byref(DecoderInfo(device, width, height, is444, False)), c.byref(ref)))
            frame = c.create_string_buffer(stream)
            image = Image()
            checked(decode_own(own, frame, len(stream), 0, c.byref(image)))
            output = CpuBuffer()
            output.width, output.height, output.format = width, height, 6 if is444 else 5
            planes = []
            for component in range(3):
                pw = width if not component or is444 else width // 2
                ph = height if not component or is444 else height // 2
                plane = (c.c_uint16 * (pw * ph))()
                planes.append(plane)
                output.data[component] = c.addressof(plane)
                output.stride[component], output.size[component] = pw * 2, pw * ph * 2
            checked(push(ref, frame, len(stream)))
            checked(decode_ref(ref, c.byref(output)))
            maximum = 0.0
            for component in range(3):
                pw = width if not component or is444 else width // 2
                ph = height if not component or is444 else height // 2
                for y in range(ph):
                    for x in range(pw):
                        value = image.planes[component][y * image.stride + x]
                        other = planes[component][y * pw + x] / 65535
                        maximum = max(maximum, abs(value - other))
            entry = {'file': path.name, 'width': width, 'height': height,
                     'chroma_444': bool(is444), 'bytes': len(stream),
                     'maximum_reference_difference': maximum,
                     'sha256': hashlib.sha256(stream).hexdigest()}
            report.append(entry)
            if maximum > 0.003:
                raise RuntimeError(entry)
        finally:
            if ref.value: free_ref(ref)
            free_own(own)
finally:
    free_device(device)
options.output.parent.mkdir(parents=True, exist_ok=True)
options.output.write_text(json.dumps(report, indent=2))
print(f'{len(report)} independently GPU-encoded frames passed native reference decode; '
      f'maximum difference {max(item["maximum_reference_difference"] for item in report):.9f}')
