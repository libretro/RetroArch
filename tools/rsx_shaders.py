#!/usr/bin/env python3
"""Verify committed RSX shader binaries, or explicitly regenerate with cgcomp."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'gfx/drivers/rsx_shaders'
PROGRAMS = ('modern_opaque', 'modern_alpha_blend')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate(data, vertex):
    if len(data) < 36 or data[:2] != (b'VP' if vertex else b'FP'):
        raise ValueError('Invalid RSX program header')
    magic, padding, registers, attributes, constants, instructions = struct.unpack_from('>6H', data)
    attr_offset, const_offset, code_offset = struct.unpack_from('>3I', data, 12)
    if padding or not registers or not instructions or code_offset % 16:
        raise ValueError('Invalid RSX program dimensions/alignment')
    if code_offset + instructions * 16 != len(data):
        raise ValueError('Invalid RSX microcode bounds')
    for offset, count, stride in ((attr_offset, attributes, 12), (const_offset, constants, 28)):
        if offset < 36 or offset + count * stride > code_offset:
            raise ValueError('Invalid RSX parameter table bounds')
    def names(offset, count, stride):
        result = set()
        for i in range(count):
            name_offset, = struct.unpack_from('>I', data, offset + i * stride)
            if not name_offset:
                continue
            if name_offset < 36 or name_offset >= code_offset:
                raise ValueError('Invalid RSX parameter name offset')
            end = data.find(b'\0', name_offset, code_offset)
            if end < 0:
                raise ValueError('Unterminated RSX parameter name')
            result.add(data[name_offset:end].decode('ascii'))
        return result
    attrs = names(attr_offset, attributes, 12)
    consts = names(const_offset, constants, 28)
    required = {'position', 'texcoord', 'color'} if vertex else {'texture'}
    if not required <= attrs or (vertex and 'modelViewProj' not in consts):
        raise ValueError('Missing parameter used by the RSX frontend')
    return {'instructions': instructions, 'registers': registers,
            'attributes': sorted(attrs), 'constants': sorted(consts)}


def verify(source=SOURCE):
    directory = source / 'precompiled'
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    expected = {name + ext for name in PROGRAMS for ext in ('.vpo', '.fpo')}
    if manifest['version'] != 1 or set(manifest['artifacts']) != expected:
        raise ValueError('Incomplete RSX shader manifest')
    for name in PROGRAMS:
        for ext in ('.vcg', '.fcg'):
            path = source / (name + ext)
            if digest(path.read_bytes().replace(b'\r\n', b'\n')) != manifest['sources'].get(path.name):
                raise ValueError('Stale RSX shader: regenerate ' + path.name)
    for name, record in manifest['artifacts'].items():
        data = (directory / name).read_bytes()
        if len(data) != record['bytes'] or digest(data) != record['sha256']:
            raise ValueError('Corrupt RSX artifact: ' + name)
        if validate(data, name.endswith('.vpo')) != record['program']:
            raise ValueError('Invalid RSX program metadata: ' + name)
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check', action='store_true')
    p.add_argument('--regenerate', action='store_true')
    p.add_argument('--compiler', type=Path)
    p.add_argument('--cg-library', type=Path)
    p.add_argument('--cgcomp-revision')
    p.add_argument('--compiler-note', default='')
    a = p.parse_args()
    if a.regenerate:
        if not a.compiler or not a.cg_library or not a.cgcomp_revision:
            p.error('Regeneration requires --compiler, --cg-library and --cgcomp-revision')
        manifest = {'version': 1, 'sources': {}, 'artifacts': {}, 'tools': {
            'cgcomp_revision': a.cgcomp_revision, 'cgcomp_sha256': digest(a.compiler.read_bytes()),
            'cg_library_sha256': digest(a.cg_library.read_bytes()), 'note': a.compiler_note,
            'vertex_profile': 'vp40', 'fragment_profile': 'fp40',
            'cg_options': '-O3 -fastmath -fastprecision -unroll count=8 -ifcvt all'}}
        directory = SOURCE / 'precompiled'
        directory.mkdir(exist_ok=True)
        for name in PROGRAMS:
            for vertex in (True, False):
                source = SOURCE / (name + ('.vcg' if vertex else '.fcg'))
                target = directory / (name + ('.vpo' if vertex else '.fpo'))
                subprocess.run([str(a.compiler.resolve()), '-v' if vertex else '-f',
                                str(source), str(target)], check=True)
                data = target.read_bytes()
                manifest['sources'][source.name] = digest(source.read_bytes().replace(b'\r\n', b'\n'))
                manifest['artifacts'][target.name] = {'bytes': len(data), 'sha256': digest(data),
                                                     'program': validate(data, vertex)}
        (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    elif not a.check:
        p.error('Pass --check or --regenerate')
    verify()
    print('Verified four committed RSX programs (no Cg compiler needed for verification)')


if __name__ == '__main__':
    main()
