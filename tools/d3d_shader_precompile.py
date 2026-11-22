#!/usr/bin/env python3
"""Maintainer-only compilation of the native built-in shader model matrix."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DRIVERS = ROOT / 'gfx/drivers'
MODERN = ['opaque', 'mesh', 'sprite', 'views', 'hdr', 'mipmapgen',
          'planar11', 'planar11_array', 'planar12', 'nv12', 'ribbon', 'ribbon_simple',
          'simple_snow', 'snow', 'bokeh', 'snowflake', 'bloom_vs', 'bloom_ps']
EFFECTS = {'ribbon', 'ribbon_simple', 'simple_snow', 'snow', 'bokeh', 'snowflake', 'bloom', 'bloom_vs', 'bloom_ps'}
LEGACY = ['opaque', 'mesh', 'sprite', 'views', 'hdr', 'mipmapgen', 'planar',
          'ribbon', 'ribbon_simple', 'simple_snow', 'snow', 'bokeh', 'snowflake', 'bloom']

def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return '%016x' % value

def stage(entry):
    return 'vs' if entry.startswith('VS') else 'gs' if entry.startswith('GS') else 'cs' if entry in ('CSMain', 'main') else 'ps'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sources', type=Path, required=True, help='Native executable built from d3d_shader_sources.c')
    p.add_argument('--fxc', required=True)
    p.add_argument('--dxc', required=True)
    a = p.parse_args()
    inputs = {}
    for path in [ROOT/'tools/d3d_shader_sources.c', ROOT/'gfx/common/xmb_plastic.h', ROOT/'gfx/drivers/xmb_plastic.h', ROOT/'gfx/common/menu_ribbon_shader.h', ROOT/'gfx/drivers/menu_ribbon_shader.h', ROOT/'gfx/drivers_shader/video_hdr.inc'] + [p for directory in DRIVERS.glob('d3d_sm*_shaders') for p in directory.rglob('*') if p.suffix in ('.h','.hlsl')]:
        inputs[path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    compilers = {name: hashlib.sha256(Path(path).read_bytes()).hexdigest()
                 for name, path in [('fxc', a.fxc), ('dxc', a.dxc)]}
    sources = {name: subprocess.check_output([str(a.sources.resolve()), name]).replace(b'\r\n', b'\n') for name in MODERN}
    for model in (2,3,4,5,6):
        directory = DRIVERS / ('d3d_sm%d_shaders' % model)
        records = []
        programs = LEGACY if model < 4 else MODERN + (['planar', 'mipmapgen_pixel'] if model==4 else [])
        with tempfile.TemporaryDirectory(prefix='d3d-native-shaders-') as temporary:
            temp = Path(temporary)
            for name in programs:
                raw = model < 4 or name in ('planar', 'mipmapgen_pixel')
                if model==4 and name in ('mipmapgen','planar11','planar11_array','planar12','nv12'): continue
                if raw:
                    key = 'mipmapgen' if name=='mipmapgen_pixel' else name
                    source = directory / ('effects' if name in EFFECTS else '') / ('%s_sm%d.hlsl' % (key,model))
                    data = source.read_bytes().replace(b'\r\n',b'\n')
                    entries = ['VSMain','PSMain']
                    if name=='sprite':entries+=['PSMainA8']
                else:
                    data = sources[name]
                    source = temp / (name+'.hlsl')
                    # DXC requires the explicit native sampler type.
                    source.write_bytes((b'#define sampler SamplerState\n' if model==6 else b'') + data)
                    entries = sorted(set(re.findall(rb'\b((?:VS|PS|GS|CS)Main\w*)\s*\(',data)))
                    entries = [entry.decode() for entry in entries]
                    if name in ('planar11','planar11_array','planar12','nv12'):entries=['main']
                if name=='views':entries=['VSMain','PSRect','PSLayer','PSAnaglyph','PSInterlaced']
                for entry in entries:
                    profile = stage(entry)+'_%d_0'%model
                    suffix = '.dxil' if model==6 else '.dxbc' if model>=4 else '.cso'
                    target = directory / ('effects' if name in EFFECTS else '') / 'precompiled' / (name+'.'+entry+suffix)
                    target.parent.mkdir(parents=True,exist_ok=True)
                    command = ([a.dxc,'-T',profile,'-E',entry,'-HV','2016','-O3','-Qstrip_reflect','-Qstrip_debug','-Fo',str(target),str(source)] if model==6 else
                               [a.fxc,'/nologo','/T',profile,'/E',entry,'/O3'] + (['/Qstrip_reflect','/Qstrip_debug'] if model >= 4 else []) + ['/Fo',str(target),str(source)])
                    result = subprocess.run(command,capture_output=True,text=True)
                    if result.returncode:
                        raise SystemExit('%s %s %s\n%s%s'%(model,name,entry,result.stdout,result.stderr))
                    blob=target.read_bytes()
                    records.append({'program':name,'entry':entry,'profile':profile,
                        'file':target.relative_to(directory).as_posix(),'bytes':len(blob),
                        'sha256':hashlib.sha256(blob).hexdigest(), 'source_fingerprint':fingerprint(data),
                        'source_bytes':len(data),'source_sha256':hashlib.sha256(data).hexdigest(), 'raster_fallback':raw})
            (directory/'manifest.json').write_text(json.dumps({'version':1,'model':model,'compilers':compilers,'optimization':'O3; strip reflection/debug for SM4+; DXC HV2016','inputs':inputs,'shaders':records},indent=2)+'\n',encoding='utf-8')
        print('SM%d: %d native shader stages compiled'%(model,len(records)),flush=True)

if __name__=='__main__': main()
