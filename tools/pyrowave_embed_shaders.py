#!/usr/bin/env python3
"""Embed verified, committed PyroWave artifacts; no shader compiler required."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from pyrowave_shaders import array, text_array, write_changed
from glsl_minify import minify

SOURCE = Path(__file__).resolve().parents[1] / 'libretro-common/formats/pyrowave/shaders'

def validate_container(data, dxil):
    if len(data) < 32 or data[:4] != b'DXBC':
        raise ValueError('Invalid Direct3D shader container')
    size, count = struct.unpack_from('<II', data, 24)
    if size != len(data) or count > (size - 32) // 4:
        raise ValueError('Invalid Direct3D container bounds')
    chunks = set()
    for i in range(count):
        offset = struct.unpack_from('<I', data, 32 + 4 * i)[0]
        if offset < 32 + count * 4 or offset > size - 8:
            raise ValueError('Invalid Direct3D chunk offset')
        length = struct.unpack_from('<I', data, offset + 4)[0]
        if length > size - offset - 8:
            raise ValueError('Invalid Direct3D chunk size')
        chunks.add(data[offset:offset + 4])
    if (dxil and b'DXIL' not in chunks) or (not dxil and not ({b'SHDR', b'SHEX'} & chunks)):
        raise ValueError('Wrong Direct3D shader format')

def assets(directory):
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    if manifest['version'] != 1: raise ValueError('Unsupported manifest version')
    expected = {stage + suffix for stage in ('color', 'dwt', 'packet')
                for suffix in ('.spv', '.dxil', '.dxbc', '.comp', '.es.comp', '.metal')}
    if set(manifest['artifacts']) != expected: raise ValueError('Incomplete shader bundle')
    for name in ('color.hlsl', 'dwt.hlsl', 'packet.hlsl'):
        data = (directory.parent / name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(data).hexdigest() != manifest['sources'].get(name):
            raise ValueError('Stale precompiled shaders: regenerate after editing ' + name)
    result = []
    for name, info in sorted(manifest['artifacts'].items()):
        data = (directory / name).read_bytes()
        if info['text']: data = data.replace(b'\r\n', b'\n')
        if len(data) != info['bytes'] or hashlib.sha256(data).hexdigest() != info['sha256']:
            raise ValueError('Shader artifact hash/size mismatch: ' + name)
        if name.endswith('.spv') and (len(data) < 20 or len(data) % 4 or data[:4] != b'\x03\x02\x23\x07'):
            raise ValueError('Invalid SPIR-V: ' + name)
        if name.endswith(('.dxil', '.dxbc')): validate_container(data, name.endswith('.dxil'))
        if not info['text'] and len(data) % 4: raise ValueError('Unaligned shader binary')
        result.append((info, data))
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, default=SOURCE / 'precompiled')
    p.add_argument('--output', type=Path)
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    bundle = assets(a.input)
    if not a.output and not a.check: p.error('Pass --output or --check')
    if a.output:
        lines = ['/* Committed shader data. Regenerate offline with tools/pyrowave_embed_shaders.py. */',
                 '#ifndef RPYROWAVE_EMBEDDED_SHADER_ASSETS_H', '#define RPYROWAVE_EMBEDDED_SHADER_ASSETS_H',
                 '#include <stdint.h>']
        for language in ('GLSL', 'GLES', 'SPIRV', 'D3D', 'METAL'):
            lines.append('#ifdef RPYROWAVE_SHADER_' + language)
            for info, data in bundle:
                if info['language'] != language: continue
                dxil = info['symbol'].endswith('_dxil')
                if dxil: lines.append('#ifndef RPYROWAVE_SHADER_DXBC_ONLY')
                if info['text']:
                    if info['language'] in ('GLSL', 'GLES'):
                        data = minify(data.decode('ascii')).encode('ascii')
                    lines.append(text_array(info['symbol'], data))
                else:
                    words = struct.unpack('<%dI' % (len(data) // 4), data)
                    lines.append(array(info['symbol'], 'uint32_t', ['0x%08xU' % word for word in words], 8))
                if dxil: lines.append('#endif')
            lines.append('#endif')
        lines.append('#endif\n')
        encoded = '\n'.join(lines).encode('ascii')
        if a.check:
            if not a.output.is_file() or a.output.read_bytes().replace(b'\r\n', b'\n') != encoded:
                raise ValueError('Committed PyroWave header is stale: ' + str(a.output))
        else:
            write_changed(a.output, encoded)
    print('Verified precompiled PyroWave artifacts' + (': ' + str(a.output) if a.output else ''))

if __name__ == '__main__':
    main()
