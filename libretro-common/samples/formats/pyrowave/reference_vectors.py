# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT
"""Black-box use of the public PyroWave C ABI; no implementation imports."""
import ctypes as c
import hashlib
import json
import os
import argparse
from pathlib import Path

root = Path(__file__).resolve().parent
args = argparse.ArgumentParser()
args.add_argument('--dll', type=Path, required=True)
args.add_argument('--output', type=Path, required=True)
args.add_argument('--width', type=int, default=128)
args.add_argument('--height', type=int, default=128)
args.add_argument('--chroma', type=int, choices=[420, 444], default=444)
args.add_argument('--patterns', nargs='+', choices=['zero', 'mid', 'white', 'ramp', 'impulse', 'checker'],
                  default=['zero', 'mid', 'white', 'ramp', 'impulse', 'checker'])
options = args.parse_args()
width, height = options.width, options.height
is444 = options.chroma == 444
dll_path = options.dll.resolve()
if os.name != 'nt':
    raise RuntimeError('This optional oracle harness requires a native Windows DLL')
dll_dir = os.add_dll_directory(str(dll_path.parent))
dll = c.CDLL(str(dll_path))

class EncoderInfo(c.Structure):
    _fields_ = [('device', c.c_void_p), ('width', c.c_int),
                ('height', c.c_int), ('chroma', c.c_int)]

class CpuBuffer(c.Structure):
    _fields_ = [('data', c.c_void_p * 3), ('stride', c.c_size_t * 3),
                ('size', c.c_size_t * 3), ('width', c.c_int),
                ('height', c.c_int), ('format', c.c_int)]

class DecoderInfo(c.Structure):
    _fields_ = [('device', c.c_void_p), ('width', c.c_int),
                ('height', c.c_int), ('chroma', c.c_int), ('fragment', c.c_bool)]

class Rate(c.Structure):
    _fields_ = [('maximum_size', c.c_size_t)]

class Packet(c.Structure):
    _fields_ = [('offset', c.c_size_t), ('size', c.c_size_t)]

def bind(name, args, result=c.c_int):
    fn = getattr(dll, 'pyrowave_' + name)
    fn.argtypes, fn.restype = args, result
    return fn

get_version = bind('get_api_version', [c.POINTER(c.c_uint32)] * 3, None)
create_device = bind('create_default_device', [c.POINTER(c.c_void_p)])
destroy_device = bind('device_destroy', [c.c_void_p], None)
create_encoder = bind('encoder_create', [c.POINTER(EncoderInfo), c.POINTER(c.c_void_p)])
destroy_encoder = bind('encoder_destroy', [c.c_void_p], None)
encode = bind('encoder_encode_cpu', [c.c_void_p, c.POINTER(CpuBuffer), c.POINTER(Rate)])
count_packets = bind('encoder_compute_num_packets', [c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)])
packetize = bind('encoder_packetize', [c.c_void_p, c.POINTER(Packet), c.c_size_t,
                                    c.POINTER(c.c_size_t), c.c_void_p, c.c_size_t])
create_decoder = bind('decoder_create', [c.POINTER(DecoderInfo), c.POINTER(c.c_void_p)])
destroy_decoder = bind('decoder_destroy', [c.c_void_p], None)
push = bind('decoder_push_packet', [c.c_void_p, c.c_void_p, c.c_size_t])
decode = bind('decoder_decode_cpu_buffer_synchronous', [c.c_void_p, c.POINTER(CpuBuffer)])

def checked(code):
    if code != 0:
        raise RuntimeError(f'reference returned {code}')

version = [c.c_uint32() for _ in range(3)]
get_version(*(c.byref(v) for v in version))
print('Reference ABI:', [v.value for v in version], flush=True)
if version[0].value != 1:
    raise RuntimeError('Unsupported reference ABI')
device = c.c_void_p()
checked(create_device(c.byref(device)))
print('Reference Vulkan device created', flush=True)
encoder = c.c_void_p()
decoder = c.c_void_p()
outdir = options.output.resolve()
outdir.mkdir(parents=True, exist_ok=True)
manifest = {'dll_sha256': hashlib.sha256(dll_path.read_bytes()).hexdigest(),
            'abi': [v.value for v in version], 'width': width, 'height': height,
            'chroma_444': is444, 'frames': []}
try:
    checked(create_encoder(c.byref(EncoderInfo(device, width, height, is444)), c.byref(encoder)))
    checked(create_decoder(c.byref(DecoderInfo(device, width, height, is444, False)), c.byref(decoder)))
    for kind in options.patterns:
        inputs = []
        for component in range(3):
            plane_width = width if not component or is444 else width // 2
            plane_height = height if not component or is444 else height // 2
            values = []
            for y in range(plane_height):
                for x in range(plane_width):
                    if kind == 'zero': value = 0
                    elif kind == 'mid': value = 128
                    elif kind == 'white': value = 255
                    elif kind == 'ramp': value = (x * 3 + y * 5 + component * 37) & 255
                    elif kind == 'impulse': value = 255 if x == 0 and y == 0 else 128
                    else: value = 255 if ((x + y + component) & 1) else 0
                    values.append(value)
            inputs.append((c.c_uint8 * len(values))(*values))
        buffers = CpuBuffer()
        buffers.width, buffers.height, buffers.format = width, height, 2 if is444 else 1
        for component in range(3):
            buffers.data[component] = c.addressof(inputs[component])
            buffers.stride[component] = width if not component or is444 else width // 2
            buffers.size[component] = len(inputs[component])
        checked(encode(encoder, c.byref(buffers), c.byref(Rate(262144))))
        count = c.c_size_t()
        checked(count_packets(encoder, 8192, c.byref(count)))
        packets = (Packet * count.value)()
        output = (c.c_uint8 * (count.value * 8192))()
        actual = c.c_size_t()
        checked(packetize(encoder, packets, 8192, c.byref(actual), output, len(output)))
        frame = b''.join(bytes(output[p.offset:p.offset+p.size]) for p in packets[:actual.value])
        original = b''.join(bytes(p) for p in inputs)
        reconstructed = [(c.c_uint16 * len(p))() for p in inputs]
        decoded_buffer = CpuBuffer()
        decoded_buffer.width, decoded_buffer.height, decoded_buffer.format = width, height, 6 if is444 else 5
        for component in range(3):
            decoded_buffer.data[component] = c.addressof(reconstructed[component])
            decoded_buffer.stride[component] = buffers.stride[component] * 2
            decoded_buffer.size[component] = len(inputs[component]) * 2
        frame_buffer = c.create_string_buffer(frame)
        checked(push(decoder, frame_buffer, len(frame)))
        checked(decode(decoder, c.byref(decoded_buffer)))
        reference_output = b''.join(bytes(p) for p in reconstructed)
        (outdir / f'{kind}.pyro').write_bytes(frame)
        (outdir / f'{kind}.yuv').write_bytes(original)
        (outdir / f'{kind}.reference.u16').write_bytes(reference_output)
        manifest['frames'].append({'name': kind, 'bytes': len(frame),
            'sha256': hashlib.sha256(frame).hexdigest(),
            'input_sha256': hashlib.sha256(original).hexdigest(),
            'reference_output_sha256': hashlib.sha256(reference_output).hexdigest()})
        print(kind, len(frame), actual.value, flush=True)
finally:
    if decoder.value: destroy_decoder(decoder)
    if encoder.value: destroy_encoder(encoder)
    if device.value: destroy_device(device)
(outdir / 'manifest.json').write_text(json.dumps(manifest, indent=2))
