#!/usr/bin/env python3
"""GPU oracle for shipping D3D12 menu and packed 10-bit frame mappings."""
import argparse
import ctypes
import hashlib
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]

def regenerate(source, header):
    if os.name != 'nt':
        raise SystemExit('Regenerate with Windows D3DCompiler_47, then run the GPU oracle.')
    compiler = ctypes.WinDLL(str(Path(os.environ['SystemRoot']) / 'System32/d3dcompiler_47.dll'))
    compile_shader = compiler.D3DCompile
    compile_shader.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p,
                              ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p,
                              ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint,
                              ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]
    compile_shader.restype = ctypes.c_long
    code, error = ctypes.c_void_p(), ctypes.c_void_p()

    def method(blob, index, result):
        table = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        return ctypes.WINFUNCTYPE(result, ctypes.c_void_p)(table[index])

    def data(blob):
        pointer = method(blob, 3, ctypes.c_void_p)(blob)
        size = method(blob, 4, ctypes.c_size_t)(blob)
        return ctypes.string_at(pointer, size)

    try:
        status = compile_shader(source, len(source), None, None, None,
                               b'main', b'cs_5_0', 0, 0, ctypes.byref(code), ctypes.byref(error))
        if status < 0:
            raise SystemExit(data(error).decode('utf-8', errors='replace') if error else 'D3DCompile failed')
        bytecode = data(code)
    finally:
        for blob in (code, error):
            if blob:
                method(blob, 2, ctypes.c_ulong)(blob)
    words = struct.unpack('<' + 'I' * (len(bytecode) // 4), bytecode)
    lines = ['/* Generated from sample_cs.hlsl by run.py --regenerate. */',
             '#define MENU_MAPPING_SOURCE_SHA256 "' + hashlib.sha256(source).hexdigest() + '"',
             'static const uint32_t menu_mapping_cs[] = {']
    lines += ['   ' + ', '.join('0x%08xu' % word for word in words[i:i+8]) + ','
              for i in range(0, len(words), 8)]
    header.write_text('\n'.join(lines) + '\n};\n', encoding='utf-8', newline='\n')

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--regenerate', action='store_true', help='rebuild shader.h using Windows D3DCompiler_47')
args = parser.parse_args()
folder = Path(__file__).parent
shader_source = (folder / 'sample_cs.hlsl').read_text(encoding='utf-8').encode('utf-8')
header = folder / 'shader.h'
if args.regenerate:
    regenerate(shader_source, header)
    raise SystemExit(0)
match = re.search(r'MENU_MAPPING_SOURCE_SHA256 "([0-9a-f]+)"', header.read_text(encoding='utf-8'))
if not match or match[1] != hashlib.sha256(shader_source).hexdigest():
    raise SystemExit('Shader source changed: regenerate shader.h on Windows with run.py --regenerate')

with tempfile.TemporaryDirectory(prefix='retroarch-menu-mapping-') as directory:
    temp = Path(directory)
    code = function((root / 'gfx/drivers/d3d12.c').read_text(encoding='utf-8'),
                    'static void d3d12_menu_mapping(')
    code += function((root / 'gfx/common/dxgi_common.c').read_text(encoding='utf-8'),
                     'void dxgi_copy(')
    driver = (root / 'gfx/drivers/d3d12.c').read_text(encoding='utf-8')
    code += next(line for line in driver.splitlines() if line.startswith('#define D3D12_FRAME_MAPPING_2101010 ')) + '\n'
    for signature in ('static void d3d12_frame_mapping(',
                      'static void d3d12_update_texture_into('):
        code += function(driver, signature)
    (temp / 'driver.inc').write_text(code, encoding='utf-8')
    cc = shlex.split(os.environ.get('CC', 'cc'))
    windows = os.name == 'nt' or 'mingw' in cc[0]
    exe = temp / ('test.exe' if windows else 'test')
    flags = ['-std=gnu89', '-Wall', '-Wextra', '-Werror=declaration-after-statement',
             '-O1', '-g1', '-I' + str(temp), '-I' + str(root / 'libretro-common/include')]
    flags += shlex.split(os.environ.get('CFLAGS', ''))
    libraries = ['-ld3d12', '-ldxguid', '-luuid', '-lm'] if windows else []
    extra = [str(root / 'libretro-common/gfx/scaler/pixconv.c')] if windows else []
    subprocess.run(cc + flags + extra + [str(Path(__file__).with_name('test.c')),
                   '-o', str(exe)] + libraries, check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
