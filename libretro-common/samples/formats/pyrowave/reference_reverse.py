# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT
"""Independent C89 image encoder -> native black-box reference decoder."""
import ctypes as c
import os
from pathlib import Path
import json
import hashlib
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--reference-dll', type=Path, required=True)
parser.add_argument('--codec-dll', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
options = parser.parse_args()
if os.name != 'nt':
    raise RuntimeError('This optional oracle harness requires native Windows DLLs')
root = options.output.resolve().parent
root.mkdir(parents=True, exist_ok=True)
binary = options.reference_dll.resolve()
directory = os.add_dll_directory(str(binary.parent))
reference = c.CDLL(str(binary))
independent = c.CDLL(str(options.codec_dll.resolve()))

class Sequence(c.Structure):
    _fields_ = [('total_blocks', c.c_uint32)] + [(n, c.c_uint) for n in
        ['width', 'height', 'sequence', 'chroma_444', 'primaries', 'transfer',
         'transform', 'limited', 'left']]
class Image(c.Structure):
    _fields_ = [('planes', c.POINTER(c.c_float) * 3), ('stride', c.c_size_t),
                ('sequence', Sequence)]
class DecoderInfo(c.Structure):
    _fields_ = [('device', c.c_void_p), ('width', c.c_int),
                ('height', c.c_int), ('chroma', c.c_int), ('fragment', c.c_bool)]
class CpuBuffer(c.Structure):
    _fields_ = [('data', c.c_void_p * 3), ('stride', c.c_size_t * 3),
                ('size', c.c_size_t * 3), ('width', c.c_int),
                ('height', c.c_int), ('format', c.c_int)]

def bind(lib, name, args, result=c.c_int):
    fn = getattr(lib, name)
    fn.argtypes, fn.restype = args, result
    return fn

new_encoder = bind(independent, 'rpyrowave_encoder_new', [c.c_uint]*3+[c.c_size_t], c.c_void_p)
free_encoder = bind(independent, 'rpyrowave_encoder_free', [c.c_void_p], None)
encode = bind(independent, 'rpyrowave_encode_frame', [c.c_void_p, c.POINTER(Image),
    c.c_size_t, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)])
new_decoder = bind(independent, 'rpyrowave_decoder_new', [c.c_uint]*3+[c.c_size_t], c.c_void_p)
free_decoder = bind(independent, 'rpyrowave_decoder_free', [c.c_void_p], None)
decode_own = bind(independent, 'rpyrowave_decode_frame', [c.c_void_p, c.c_void_p,
    c.c_size_t, c.c_uint, c.POINTER(Image)])
new_device = bind(reference, 'pyrowave_create_default_device', [c.POINTER(c.c_void_p)])
free_device = bind(reference, 'pyrowave_device_destroy', [c.c_void_p], None)
new_reference_decoder = bind(reference, 'pyrowave_decoder_create',
    [c.POINTER(DecoderInfo), c.POINTER(c.c_void_p)])
free_reference_decoder = bind(reference, 'pyrowave_decoder_destroy', [c.c_void_p], None)
push = bind(reference, 'pyrowave_decoder_push_packet', [c.c_void_p, c.c_void_p, c.c_size_t])
decode_ref = bind(reference, 'pyrowave_decoder_decode_cpu_buffer_synchronous',
    [c.c_void_p, c.POINTER(CpuBuffer)])

def checked(result):
    if result != 0: raise RuntimeError(f'Codec returned {result}')

device = c.c_void_p()
checked(new_device(c.byref(device)))
report = []
try:
    for width, height, is444 in [(1,1,1),(2,2,0),(128,128,1),(130,98,0),(321,181,1)]:
        encoder = new_encoder(width,height,is444,1048576)
        decoder = new_decoder(width,height,is444,1048576)
        ref = c.c_void_p()
        checked(new_reference_decoder(c.byref(DecoderInfo(device,width,height,is444,False)),c.byref(ref)))
        try:
            frame_number=0
            for kind in ['zero','mid','white','ramp','impulse','checker']:
                source = Image()
                source.stride = width
                source.sequence.width, source.sequence.height = width,height
                source.sequence.chroma_444 = is444
                inputs=[]
                outputs=[]
                destination = CpuBuffer()
                destination.width,destination.height,destination.format=width,height,6 if is444 else 5
                for component in range(3):
                    pw=width if not component or is444 else width//2
                    ph=height if not component or is444 else height//2
                    plane=(c.c_float*(width*ph))()
                    for y in range(ph):
                        for x in range(pw):
                            if kind=='zero': value=0
                            elif kind=='mid': value=128
                            elif kind=='white': value=255
                            elif kind=='ramp': value=(x*3+y*5+component*37)&255
                            elif kind=='impulse': value=255 if x==0 and y==0 else 128
                            else: value=255 if (x+y+component)&1 else 0
                            plane[y*width+x]=value/255
                    inputs.append(plane)
                    source.planes[component]=c.cast(plane,c.POINTER(c.c_float))
                    output=(c.c_uint16*(pw*ph))()
                    outputs.append(output)
                    destination.data[component]=c.addressof(output)
                    destination.stride[component],destination.size[component]=pw*2,pw*ph*2
                for budget in [8,512,4096,32768,262144]:
                    source.sequence.sequence=frame_number%8
                    frame_number+=1
                    stream=(c.c_uint8*budget)()
                    written=c.c_size_t()
                    checked(encode(encoder,c.byref(source),budget,stream,budget,c.byref(written)))
                    assert written.value<=budget
                    own=Image()
                    checked(decode_own(decoder,stream,written.value,0,c.byref(own)))
                    checked(push(ref,stream,written.value))
                    checked(decode_ref(ref,c.byref(destination)))
                    worst=0
                    source_error=0
                    for component in range(3):
                        pw=width if not component or is444 else width//2
                        ph=height if not component or is444 else height//2
                        for y in range(ph):
                            for x in range(pw):
                                actual=own.planes[component][y*own.stride+x]
                                other=outputs[component][y*pw+x]/65535
                                worst=max(worst,abs(actual-other))
                                source_error=max(source_error,abs(actual-inputs[component][y*width+x]))
                    entry={'width':width,'height':height,'chroma444':bool(is444),'pattern':kind,
                           'budget':budget,'bytes':written.value,'decoder_max_error':worst,
                           'source_max_error':source_error,'sha256':hashlib.sha256(bytes(stream[:written.value])).hexdigest()}
                    report.append(entry)
                    if worst>0.003:
                        (options.output.with_suffix('.failure.pyro')).write_bytes(bytes(stream[:written.value]))
                        print('Reference first samples:',[list(p[:8]) for p in outputs])
                        raise RuntimeError(entry)
            print(width,height,is444,'30 reverse-reference frames: PASS',flush=True)
        finally:
            free_reference_decoder(ref)
            free_encoder(encoder)
            free_decoder(decoder)
finally:
    free_device(device)
(options.output).write_text(json.dumps(report,indent=2))
print('Worst reverse decoder difference:',max(item['decoder_max_error'] for item in report))
