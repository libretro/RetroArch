#!/usr/bin/env python3
"""Compile the production import shaders and check their GPU color output."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[3]
subprocess.run([__import__('sys').executable, str(root / 'tools/generate_video_hdr.py'), '--check'], check=True)
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    shaders = []
    for api, name in [('d3d11', 'd3d11_planar_cs_src'), ('d3d12', 'd3d12_nv12_cs_src')]:
        source = (root / 'gfx/drivers' / (api + '.c')).read_text()
        start = source.index('static const char ' + name + '[] =')
        shaders.append(source[start:source.index(';\n', start) + 1].replace(
            '#include "../drivers_shader/video_hdr.inc"',
            (root / 'gfx/drivers_shader/video_hdr.inc').read_text()))
    (tmp / 'shaders.inc').write_text('\n'.join(shaders))
    source = (root / 'gfx/drivers/d3d11.c').read_text()
    start = source.index('static bool d3d11_planar_convert(')
    (tmp / 'convert.inc').write_text(source[start:source.index('\n}', start) + 2])
    start = source.index('static int d3d11_import_tex(')
    (tmp / 'import.inc').write_text(source[start:source.index('\n}', start) + 2])
    start = source.index('static d3d11_planar_t *d3d11_import_new(')
    (tmp / 'new11.inc').write_text(source[start:source.index('\n}', start) + 2])
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
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)

    exe12 = tmp / ('test12.exe' if windows else 'test12')
    subprocess.run(cc + flags + ['-I' + str(tmp), '-I' + str(root), '-I' + str(root / 'libretro-common/include'),
        str(Path(__file__).with_name('test12.c')), str(root / 'libretro-common/formats/image/image_yuv_blit.c'),
        '-o', str(exe12)] + (['-ld3d11', '-ld3d12', '-ld3dcompiler', '-ldxgi'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe12)], check=True)

    import json, re
    sprite = (root / 'gfx/drivers/d3d_shaders/sprite_sm4.hlsl.h').read_text()
    composite = (root / 'gfx/drivers/d3d_shaders/hdr_sm5.hlsl.h').read_text()
    sprite = sprite[sprite.index('\nSRC(') + 5:sprite.rfind(')')]
    composite = composite[composite.index('\nSRC(') + 5:composite.rfind(')')]
    sprite = re.sub(r'/\*.*?\*/|//[^\n]*', '', sprite, flags=re.S)
    composite = re.sub(r'/\*.*?\*/|//[^\n]*', '', composite, flags=re.S)
    (tmp / 'raster.inc').write_text('static const char sprite_source[] = ' + json.dumps(sprite)
        + ';\nstatic const char composite_source[] = ' + json.dumps(composite) + ';\n')
    exe_raster = tmp / ('raster.exe' if windows else 'raster')
    subprocess.run(cc + flags + ['-I' + str(tmp),
        str(Path(__file__).with_name('raster_test.c')), '-o', str(exe_raster)]
        + (['-ld3d11', '-ld3dcompiler'] if windows else []) + ['-lm'], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe_raster)], check=True)

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
