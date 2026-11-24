#!/usr/bin/env python3
"""Compile the production import shaders and check their GPU color output."""
from pathlib import Path
import argparse, os, shlex, subprocess, sys, tempfile
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pool-only', action='store_true', help='Run the native D3D11 pooled conversion oracle only')
parser.add_argument('--d3d11-only', action='store_true', help='Run D3D11 source, array and pooled pixel checks')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
subprocess.run([__import__('sys').executable, str(root / 'tools/generate_video_hdr.py'), '--check'], check=True)
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    shaders = []
    for api, name in [('d3d11', 'd3d11_planar_cs_src'), ('d3d12', 'd3d12_nv12_cs_src'), ('d3d11_array', 'd3d11_array_cs_src')]:
        header = 'planar_d3d11.hlsl.h' if api != 'd3d12' else 'nv12_d3d12.hlsl.h'
        source = (root / 'gfx/drivers/d3d_sm5_shaders' / header).read_text()
        source = source.replace('#include "../../drivers_shader/video_hdr.inc"',
            (root / 'gfx/drivers_shader/video_hdr.inc').read_text())
        shaders.append(('#define D3D11_PLANAR_ARRAY\n' if api == 'd3d11_array' else '') + 'static const char ' + name + '[] =\n' + source + ';\n#undef D3D11_PLANAR_ARRAY')
    (tmp / 'shaders.inc').write_text('\n'.join(shaders))
    source = (root / 'gfx/drivers/d3d11.c').read_text()
    start = source.index('static bool d3d11_planar_convert(')
    (tmp / 'convert.inc').write_text(source[start:source.index('\n}', start) + 2])
    start = source.index('static int d3d11_import_tex(')
    (tmp / 'import.inc').write_text(source[start:source.index('\n}', start) + 2])
    start = source.index('static d3d11_planar_t *d3d11_import_new(')
    (tmp / 'new11.inc').write_text(source[start:source.index('\n}', start) + 2])
    pool = []
    for signature in ('static bool d3d11_media_pool_begin(',
                      'static D3D11ShaderResourceView d3d11_media_writable(',
                      'static void d3d11_media_publish(',
                      'static D3D11UnorderedAccessView d3d11_planar_writable('):
        start = source.index(signature)
        pool.append(source[start:source.index('\n}', start) + 2])
    (tmp / 'pool11.inc').write_text('\n'.join(pool))
    source12 = (root / 'gfx/drivers/d3d12.c').read_text()
    bodies = []
    for signature in ('static void d3d12_import_forget(', 'static int d3d12_import_tex(',
                      'static ID3D12Fence *d3d12_import_fence(', 'static bool d3d12_import_list(',
                      'static void d3d12_import_transition(',
                      'static enum video_texture_update d3d12_import_texture('):
        start = source12.index(signature)
        bodies.append(source12[start:source12.index('\n}', start) + 2])
    (tmp / 'import12.inc').write_text('\n'.join(bodies))
    start = source12.index('typedef struct d3d12_import\n')
    (tmp / 'types12.inc').write_text(source12[start:source12.index('} d3d12_import_t;', start) + len('} d3d12_import_t;')])
    video = (root / 'gfx/video_driver.h').read_text()
    start = video.index('typedef struct video_frame_import\n')
    (tmp / 'frame.inc').write_text(video[start:video.index('} video_frame_import_t;', start) + len('} video_frame_import_t;')])
    cc = shlex.split(os.environ.get('CC', 'cc'))
    windows = 'mingw' in cc[0] or os.name == 'nt'
    exe = tmp / ('test.exe' if windows else 'test')
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -std=c89 -Wall -Wextra'))
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test.c')), str(root / 'libretro-common/formats/image/image_yuv_blit.c'),
        '-o', str(exe)] + (['-ld3d11', '-ld3dcompiler'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True,
                   env={**os.environ, **({'POOL_ONLY': '1'} if args.pool_only else {})})
    if args.pool_only or args.d3d11_only:
        sys.exit(0)

    exe12 = tmp / ('test12.exe' if windows else 'test12')
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test12.c')), str(root / 'libretro-common/formats/image/image_yuv_blit.c'),
        '-o', str(exe12)] + (['-ld3d11', '-ld3d12', '-ld3dcompiler', '-ldxgi'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe12)], check=True)

    # Include the C string headers directly, including shared HDR fragments.
    includes = []
    for name, header in [('sprite', 'd3d_sm4_shaders/sprite_sm4.hlsl.h'),
                         ('composite', 'd3d_sm5_shaders/hdr_sm5.hlsl.h')]:
        path = (root / 'gfx/drivers' / header).as_posix()
        includes.append('static const char ' + name + '_source[] =\n#include "'
                        + path + '"\n;')
    (tmp / 'raster.inc').write_text('\n'.join(includes))
    exe_raster = tmp / ('raster.exe' if windows else 'raster')
    subprocess.run(cc + flags + ['-I' + str(tmp),
        str(Path(__file__).with_name('raster_test.c')), '-o', str(exe_raster)]
        + (['-ld3d11', '-ld3dcompiler'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe_raster)], check=True)

    import json
    dumper = tmp / ('sources.exe' if windows else 'sources')
    subprocess.run(cc + flags + ['-I' + str(root / 'libretro-common/include'),
        str(root / 'tools/d3d_shader_sources.c'),
        '-o', str(dumper)], check=True)
    runner = shlex.split(os.environ.get('RUNNER', ''))
    sprite = subprocess.check_output(runner + [str(dumper), 'sprite']).decode()
    composite = subprocess.check_output(runner + [str(dumper), 'hdr']).decode()
    sprite = sprite.replace('t0.Sample(s0, input.texcoord)', 't0.SampleLevel(s0, input.texcoord, 0)')
    composite = composite.replace('t0.Sample(s0, texcoord)', 't0.SampleLevel(s0, texcoord, 0)')
    sprite += """
RWTexture2D<float4> Output : register(u0);
[numthreads(1,1,1)] void Roundtrip(uint3 id : SV_DispatchThreadID)
{
   PSInput p; p.position = 0; p.texcoord = float2((id.x + 0.5) / 8.0, 0.5); p.color = 1;
   Output[id.xy] = PSMainLinearHDR(p);
}
"""
    composite += """
RWTexture2D<float4> Output : register(u0);
[numthreads(1,1,1)] void Roundtrip(uint3 id : SV_DispatchThreadID)
{
   float4 layer = Sample(float4(1,1,1,1), float2((id.x + 0.5) / 8.0, 0.5));
   float3 result = To2020(layer.rgb);
   if (global.hdr_mode == 2u)
      result = mul(k2020to709, result) * (global.brightness_nits / 80.0);
   else
      result = HDR10ToscRGB(HDR10(To2020(layer)).rgb);
   Output[id.xy] = float4(result, layer.a);
}
"""
    (tmp / 'roundtrip.inc').write_text('static const char encode_source[] = ' + json.dumps(sprite)
        + ';\nstatic const char decode_source[] = ' + json.dumps(composite) + ';\n')
    exe_roundtrip = tmp / ('roundtrip.exe' if windows else 'roundtrip')
    subprocess.run(cc + flags + ['-I' + str(tmp),
        str(Path(__file__).with_name('roundtrip_test.c')), '-o', str(exe_roundtrip)]
        + (['-ld3d11', '-ld3dcompiler'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe_roundtrip)], check=True)
