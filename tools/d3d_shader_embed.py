#!/usr/bin/env python3
"""Verify and embed committed native D3D shaders without shader compilers."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from pyrowave_shaders import array, write_changed
ROOT=Path(__file__).resolve().parents[1]

def verify_profile(data, profile):
    stage, major, minor = profile.split('_')
    major, minor = int(major), int(minor)
    if major < 4:
        token, = struct.unpack_from('<I', data)
        expected = ((0xfffe if stage == 'vs' else 0xffff) << 16) | (major << 8) | minor
        if token != expected:
            raise ValueError('Invalid legacy profile: ' + profile)
        return
    if data[:4] != b'DXBC' or struct.unpack_from('<I', data, 24)[0] != len(data):
        raise ValueError('Invalid D3D container')
    count, = struct.unpack_from('<I', data, 28)
    wanted = b'DXIL' if major == 6 else None
    for offset in struct.unpack_from('<%dI' % count, data, 32):
        kind = data[offset:offset+4]
        if (wanted and kind == wanted) or (not wanted and kind in (b'SHDR', b'SHEX')):
            token, = struct.unpack_from('<I', data, offset+8)
            if token >> 16 != {'ps': 0, 'vs': 1, 'gs': 2, 'cs': 5}[stage] or token & 0xff != (major << 4) | minor:
                raise ValueError('Wrong native profile: ' + profile)
            return
    raise ValueError('Missing native program: ' + profile)


def records():
    result=[]
    for model in range(2,7):
        directory=ROOT/('gfx/drivers/d3d_sm%d_shaders'%model)
        manifest=json.loads((directory/'manifest.json').read_text())
        for name,digest in manifest['inputs'].items():
            data=(ROOT/name).read_bytes().replace(b'\r\n',b'\n')
            if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('Stale D3D shaders: '+name)
        for item in manifest['shaders']:
            data=(directory/item['file']).read_bytes()
            if len(data)!=item['bytes'] or hashlib.sha256(data).hexdigest()!=item['sha256']:
                raise ValueError('Corrupted shader: '+item['file'])
            if len(data)%4:raise ValueError('Unaligned shader')
            verify_profile(data, item['profile'])
            result.append((model,item,data))
    return result

def production_guard(model, item):
    if item['raster_fallback']:
        return 'defined(D3D_BUILTIN_TEST)'
    if item['program'] in ('planar11', 'planar11_array'):
        return 'defined(D3D_BUILTIN_TEST)' if model == 6 else 'defined(D3D_BUILTIN_TEST) || defined(HAVE_D3D11)'
    if item['program'] in ('planar12', 'nv12', 'mipmapgen'):
        return 'defined(D3D_BUILTIN_TEST) || defined(HAVE_D3D12)'
    return '1'


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path);p.add_argument('--check',action='store_true');p.add_argument('--test-output',type=Path);a=p.parse_args()
    shaders=records()
    if not a.output and not a.check:p.error('Pass --output or --check')
    if a.output:
        lines=['/* Committed shader data. Regenerate offline with tools/d3d_shader_embed.py. */']
        for model in range(2,7):
            lines.append('#ifdef D3D_BUILTIN_SM%d'%model)
            for m,item,data in shaders:
                if m!=model:continue
                name='d3d_builtin_sm%d_%s_%s'%(m,item['program'],item['entry'])
                words=struct.unpack('<%dI'%(len(data)//4),data)
                lines.append('#if ' + production_guard(m, item))
                lines.append(array(name,'uint32_t',['0x%08xU'%w for w in words],8))
                lines.append('#endif')
            lines.append('#endif')
        lines.append('static const d3d_builtin_record_t d3d_builtin_records[] = {')
        for model in range(2,7):
            lines.append('#ifdef D3D_BUILTIN_SM%d'%model)
            for m,item,data in shaders:
                if m!=model:continue
                name='d3d_builtin_sm%d_%s_%s'%(m,item['program'],item['entry'])
                lines.append('#if ' + production_guard(m, item))
                lines.append('   { D3D_BUILTIN_SOURCE(UINT64_C(0x%s), %du) "%s", "%s", "%s", %s, sizeof(%s) },'%(item['source_fingerprint'],item['source_bytes'],item['program'],item['entry'],item['profile'],name,name))
                lines.append('#endif')
            lines.append('#endif')
        lines+=['   { D3D_BUILTIN_SOURCE(0, 0) NULL, NULL, NULL, NULL, 0 }','};','']
        encoded = '\n'.join(lines).encode('ascii')
        if a.check:
            if not a.output.is_file() or a.output.read_bytes().replace(b'\r\n', b'\n') != encoded:
                raise ValueError('Committed D3D header is stale: ' + str(a.output))
        else:
            write_changed(a.output, encoded)
    if a.test_output:
        rows = []
        for model, item, data in shaders:
            program = item['program']
            source = 'NULL' if item['raster_fallback'] else ('ribbon_source()' if program == 'ribbon' else
                     program + '_source' if program.startswith('bloom_') else 'shader_' + program)
            rows.append('   {"%s", "%s", "%s", %s},' %
                        (program, item['entry'], item['profile'], source))
        write_changed(a.test_output, ('\n'.join(rows) + '\n').encode('ascii'))
    print('Verified %d native D3D shader stages'%len(shaders))
if __name__=='__main__':main()
