#!/usr/bin/env python3
"""Explicit maintainer step: precompile and commit PyroWave shader artifacts."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
from pyrowave_shaders import write_changed

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'libretro-common/formats/pyrowave/shaders'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def version(command):
    result = subprocess.run(command, capture_output=True, text=True)
    lines = (result.stdout + result.stderr).strip().splitlines()
    return lines[0] if lines else 'unreported'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=SOURCE / 'precompiled')
    p.add_argument('--dxc', required=True)
    p.add_argument('--fxc', required=True)
    p.add_argument('--glslang', default='glslangValidator')
    p.add_argument('--spirv-cross', default='spirv-cross')
    p.add_argument('--spirv-opt', default='spirv-opt')
    p.add_argument('--spirv-val', default='spirv-val')
    a = p.parse_args()
    manifest = {'version': 1, 'sources': {}, 'artifacts': {}, 'tools': {
        'glslang': version([a.glslang, '--version']),
        'spirv_opt': version([a.spirv_opt, '--version']),
        'spirv_optimization': '-Os --preserve-bindings --preserve-interface --preserve-spec-constants --strip-debug',
        'spirv_val': version([a.spirv_val, '--version']),
        'spirv_cross_sha256': digest(Path(shutil.which(a.spirv_cross) or a.spirv_cross).read_bytes()),
        'dxc': version([a.dxc, '--version']),
        'fxc': version([a.fxc, '/?']),
        'dxil_profile': 'cs_6_0', 'dxbc_profile': 'cs_5_0',
        'spirv_target': 'vulkan1.0', 'msl_target': 'metal2.1',
        'glsl_target': '430', 'gles_target': '310'}}
    with tempfile.TemporaryDirectory(prefix='pyrowave-precompile-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.executable, str(ROOT / 'tools/pyrowave_shaders.py'),
            '--output', str(directory / 'arrays.h'), '--glslang', a.glslang,
            '--spirv-cross', a.spirv_cross, '--spirv-val', a.spirv_val,
            '--spirv-opt', a.spirv_opt,
            '--export-glsl', str(directory), '--export-spirv', str(directory),
            '--export-msl', str(directory)], check=True)
        for stage in ('color', 'dwt', 'packet'):
            source = SOURCE / (stage + '.hlsl')
            manifest['sources'][source.name] = digest(source.read_bytes().replace(b'\r\n', b'\n'))
            subprocess.run([a.dxc, '-T', 'cs_6_0', '-E', 'main', '-O3',
                '-Fo', str(directory / (stage + '.dxil')), str(source)], check=True)
            subprocess.run([a.fxc, '/nologo', '/T', 'cs_5_0', '/E', 'main', '/O3',
                '/Fo', str(directory / (stage + '.dxbc')), str(source)], check=True)
            for suffix, language, symbol, text in (
                    ('.spv', 'SPIRV', 'spirv', False),
                    ('.dxil', 'D3D', 'dxil', False), ('.dxbc', 'D3D', 'dxbc', False),
                    ('.comp', 'GLSL', 'glsl', True), ('.es.comp', 'GLES', 'gles', True),
                    ('.metal', 'METAL', 'metal', True)):
                name = stage + suffix
                data = (directory / name).read_bytes()
                if text:
                    data = b'\n'.join(line.rstrip() for line in data.replace(b'\r\n', b'\n').splitlines()).rstrip() + b'\n'
                manifest['artifacts'][name] = {'sha256': digest(data), 'bytes': len(data),
                    'language': language, 'symbol': 'rpyrowave_shader_' + stage + '_' + symbol,
                    'text': text}
                write_changed(a.output / name, data)
        write_changed(a.output / 'manifest.json', (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode())
    print('Precompiled PyroWave artifacts:', a.output)

if __name__ == '__main__':
    main()
