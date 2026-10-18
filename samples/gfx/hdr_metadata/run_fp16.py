#!/usr/bin/env python3
"""Native FP16 acceptance: pass an HDR-capable RetroArch executable."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--retroarch", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--drivers", default="d3d11,d3d12,vulkan,glcore")
parser.add_argument("--stock", action="store_true", help="check stock 203-nit presentation")
parser.add_argument("--overlay", action="store_true", help="exercise isolated UI composition")
parser.add_argument("--deferred", action="store_true")
parser.add_argument("--resize", action="store_true")
parser.add_argument("--frames", type=int, default=60)
parser.add_argument("--threaded", choices=("both", "false", "true"), default="both")
parser.add_argument("--hardware", action="store_true", help="check the GLCore hardware framebuffer")
parser.add_argument("--source-hdr", choices=("builtin", "parameter"))
parser.add_argument("--passthrough", choices=("builtin", "parameter"))
parser.add_argument("--expect-passthrough", type=int, choices=(0, 1), default=1)
parser.add_argument("--tonemap-operator", choices=("builtin", "parameter"))
parser.add_argument("--min-nits", choices=("builtin", "parameter"))
parser.add_argument("--enable-hdr", choices=("builtin", "parameter"))
parser.add_argument("--peak-uniforms", choices=("builtin", "parameter"))
parser.add_argument("--peak-nits", type=float, default=600.0)
parser.add_argument("--direct-spirv", action="store_true")
parser.add_argument("--paper-white-alias", choices=("builtin", "parameter"), help="check PaperWhiteNits alongside BrightnessNits")
parser.add_argument("--hdr-uniforms", choices=("ubo", "push"), help="check existing typed HDR uniforms")
parser.add_argument("--paper-white", type=float, default=203.0)
parser.add_argument("--expand-gamut", type=int, choices=(0, 1, 2), default=0)
parser.add_argument("--preset-hdr-mode", choices=("auto", "0", "1", "2"))
parser.add_argument("--shader-sdr", action="store_true", help="checker emits gamma SDR white")
parser.add_argument("--no-format", action="store_true", help="omit the format pragma")
parser.add_argument("--scaled", action="store_true", help="keep a scaled final target")
parser.add_argument("--reference", action="store_true", help="declare ownership in a reference override")
parser.add_argument("--expect-rejection", action="store_true")
parser.add_argument("--source-pq", action="store_true", help="use native PQ Rec.2020 input")
parser.add_argument("--source-sdr", action="store_true", help="use the XRGB8888 metadata fixture")
args = parser.parse_args()
if (args.paper_white_alias or args.peak_uniforms or args.enable_hdr or args.min_nits or args.tonemap_operator or args.source_hdr or args.passthrough) and not args.hdr_uniforms:
    parser.error("HDR alias checks require --hdr-uniforms")
if args.overlay and args.preset_hdr_mode is None:
    args.stock = True
if args.source_pq and (args.source_sdr or args.hardware or args.stock):
    parser.error("--source-pq requires a software checker input")
if args.source_sdr:
    args.expand_gamut = 2
    args.paper_white = 203.0
if args.hdr_uniforms and args.stock:
    parser.error("--hdr-uniforms requires the checker shader")

executable = args.retroarch.resolve()
w = args.output.resolve()
w.mkdir(parents=True, exist_ok=True)
drivers = ["glcore"] if args.hardware else args.drivers.split(",")
r = Path(__file__).resolve().parents[3]
core = w / ("fp16-core.dll" if os.name == "nt" else "fp16-core.so")
cc = shlex.split(os.environ.get("CC", "cc"))
subprocess.run(cc + ["-std=gnu89", "-shared", "-fPIC", "-O1",
                    "-I" + str(r / "libretro-common/include"),
                    str(Path(__file__).with_name("pq_core.c" if args.source_pq else ("live_core.c" if args.source_sdr else ("fp16_hw_core.c" if args.hardware else "fp16_core.c")))),
                    "-o", str(core), "-lm"], check=True)
shader = """#version 450
#pragma format R16G16B16A16_SFLOAT
layout(set=0,binding=0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location=0) in vec4 Position;
layout(location=1) in vec2 TexCoord;
layout(location=0) out vec2 vTexCoord;
void main() { gl_Position=global.MVP*Position;vTexCoord=TexCoord; }
#pragma stage fragment
layout(set=0,binding=2) uniform sampler2D Source;
layout(location=0) in vec2 vTexCoord;
layout(location=0) out vec4 FragColor;
void main() {
 vec3 c=texture(Source,vTexCoord).rgb;
 bool ok=all(lessThan(abs(c-vec3(-0.25,0.5,2.0)),vec3(0.001)));
 FragColor=vec4(ok?vec3(203.0/80.0):vec3(2.5375,0,0),1);
 FragColor.rgb += vec3(vTexCoord,0)*0.0000001;
}
"""
if args.source_pq:
    shader = shader.replace("vec3(-0.25,0.5,2.0)", "vec3(128.0,512.0,768.0)/1023.0").replace("vec3(0.001)", "vec3(0.003)")
if args.source_sdr:
    shader = shader.replace("all(lessThan(abs(c-vec3(-0.25,0.5,2.0)),vec3(0.001)))", "true")
if args.shader_sdr:
    shader = shader.replace("ok?vec3(203.0/80.0)", "ok?vec3(1.0)")
if args.no_format:
    shader = shader.replace("#pragma format R16G16B16A16_SFLOAT\n", "")
if args.hdr_uniforms:
    members = "uint HDRMode; float BrightnessNits; uint ExpandGamut;"
    if args.passthrough:
        members += " float Passthrough;" if args.passthrough == "parameter" else " uint Passthrough;"
    if args.source_hdr:
        members += " float SourceHDR;" if args.source_hdr == "parameter" else " uint SourceHDR;"
    if args.tonemap_operator:
        members += " float TonemapOperator;" if args.tonemap_operator == "parameter" else " uint TonemapOperator;"
    if args.min_nits:
        members += " float MinNits;"
    if args.enable_hdr:
        members += " float EnableHDR;"
    if args.paper_white_alias:
        members += " float PaperWhiteNits;"
        if args.paper_white_alias == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter PaperWhiteNits "Local white" 37.0 0.0 1000.0 1.0')
    if args.peak_uniforms:
        members += " float PeakNits; float MaxNits;"
        if args.peak_uniforms == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter PeakNits "Local peak" 37.0 0.0 2000.0 1.0\n#pragma parameter MaxNits "Local max" 49.0 0.0 2000.0 1.0')
    if args.hdr_uniforms == "ubo":
        shader = shader.replace("mat4 MVP;", "mat4 MVP; " + members)
        prefix = "global."
    else:
        shader = shader.replace("#pragma stage vertex", "layout(push_constant) uniform Push { " + members + " } hdr;\n#pragma stage vertex")
        prefix = "hdr."
    shader = shader.replace("FragColor=", f"ok = ok && {prefix}HDRMode == 2u && abs({prefix}BrightnessNits - {args.paper_white}) < 0.01 && {prefix}ExpandGamut == {args.expand_gamut}u;\n FragColor=")
    if args.paper_white_alias:
        expected = 37.0 if args.paper_white_alias == "parameter" else args.paper_white
        shader = shader.replace("FragColor=", f"ok = ok && abs({prefix}PaperWhiteNits - {expected}) < 0.01;\n FragColor=")
    if args.peak_uniforms:
        peak = 37.0 if args.peak_uniforms == "parameter" else args.peak_nits
        maximum = 49.0 if args.peak_uniforms == "parameter" else args.peak_nits
        shader = shader.replace("FragColor=", f"ok = ok && abs({prefix}PeakNits - {peak}) < 0.01 && abs({prefix}MaxNits - {maximum}) < 0.01;\n FragColor=")
    if args.enable_hdr:
        enabled = 0.375 if args.enable_hdr == "parameter" else 1.0
        if args.enable_hdr == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter EnableHDR "Local HDR" 0.375 0.0 1.0 0.125')
        shader = shader.replace("FragColor=", f"ok = ok && {prefix}EnableHDR == {enabled};\n FragColor=")
    if args.passthrough:
        passthrough = "0.375" if args.passthrough == "parameter" else str(args.expect_passthrough) + "u"
        if args.passthrough == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter Passthrough "Local ownership" 0.375 0.0 1.0 0.125')
        shader = shader.replace("FragColor=", f"ok = ok && {prefix}Passthrough == {passthrough};\n FragColor=")
    if args.source_hdr:
        source_hdr = "0.375" if args.source_hdr == "parameter" else ("1u" if args.source_pq else "0u")
        if args.source_hdr == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter SourceHDR "Local source" 0.375 0.0 1.0 0.125')
        shader = shader.replace("FragColor=", f"ok = ok && {prefix}SourceHDR == {source_hdr};\n FragColor=")
    if args.tonemap_operator:
        operator = "1.0" if args.tonemap_operator == "parameter" else "0u"
        if args.tonemap_operator == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter TonemapOperator "Local operator" 1.0 0.0 2.0 1.0')
        shader = shader.replace("FragColor=", f"ok = ok && {prefix}TonemapOperator == {operator};\n FragColor=")
    if args.min_nits:
        minimum = 0.125 if args.min_nits == "parameter" else 0.0
        if args.min_nits == "parameter":
            shader = shader.replace("#version 450", '#version 450\n#pragma parameter MinNits "Local minimum" 0.125 0.0 1.0 0.125')
        shader = shader.replace("FragColor=", f"ok = ok && {prefix}MinNits == {minimum};\n FragColor=")
(w/'check.slang').write_text(shader, encoding='utf-8')
preset = 'shaders = "1"\nshader0 = "check.slang"\n'
if args.scaled:
    preset += 'scale_type0 = "source"\nscale0 = "1.5"\n'
if args.reference:
    (w/'base.slangp').write_text(preset, encoding='utf-8')
    preset = '#reference "base.slangp"\n'
if args.preset_hdr_mode is not None:
    preset += f'hdr_mode = "{args.preset_hdr_mode}"\n'
(w/'check.slangp').write_text(preset, encoding='utf-8')
if args.overlay:
    overlay = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(4):
        for x in range(4):
            overlay.putpixel((x, y), (0, 0, 0, 255))
    overlay.save(w / "overlay.png")
    (w / "overlay.cfg").write_text('overlays = "1"\noverlay0_overlay = "overlay.png"\noverlay0_full_screen = "true"\noverlay0_descs = "0"\n', encoding="utf-8")
for driver in drivers:
 for threaded in (['false','true'] if args.threaded == 'both' else [args.threaded]):
  for lend in ([False] if args.hardware or args.source_sdr else [False,True]):
   lane=('stock-' if args.stock else 'check-')+f'{driver}-{threaded}-{lend}'
   cfg=w/(lane+'.cfg');ss=w/(lane+'.png')
   cfg.write_text(f'''video_driver = "{driver}"
video_gl_direct_spirv = "{str(args.direct_spirv).lower()}"
video_threaded = "{threaded}"
video_shader_deferred_loading = "{'true' if args.deferred else 'false'}"
video_hdr_mode = "2"
video_hdr_paper_white_nits = "{args.paper_white}"
video_hdr_max_nits = "{args.peak_nits}"
video_hdr_use_display_peak = "false"
video_hdr_expand_gamut = "{args.expand_gamut}"
video_shader_enable = "{'false' if args.stock else 'true'}"
video_shader = "{(w/'check.slangp').as_posix()}"
video_gpu_screenshot = "true"
video_fullscreen = "false"
pause_nonactive = "false"
video_vsync = "false"
audio_enable = "false"
input_driver = "null"
input_joypad_driver = "null"
config_save_on_exit = "false"
menu_show_start_screen = "false"
input_overlay = "{(w/'overlay.cfg').as_posix() if args.overlay else ''}"
input_overlay_enable = "{'true' if args.overlay else 'false'}"
input_overlay_hide_when_gamepad_connected = "false"
input_overlay_opacity = "1.0"
''',encoding='utf-8')
   env=dict(os.environ)
   if args.source_sdr: env["HDR_QUERY_REQUESTED_MODE"]="2"
   if args.resize: env["FP16_RESIZE"]="1"
   else: env.pop("FP16_RESIZE",None)
   if args.stock: env["FP16_STOCK"]="1"
   else: env.pop("FP16_STOCK",None)
   if lend: env['FP16_LEND']='1'
   else: env.pop('FP16_LEND',None)
   result=subprocess.run([str(executable),'-v'] + ([] if args.stock else ['--set-shader',str(w/'check.slangp')]) + ['--config',str(cfg),'-L',str(core),'--max-frames',str(args.frames),'--max-frames-ss','--max-frames-ss-path',str(ss)],cwd=w,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
   (w/(lane+'.log')).write_bytes(result.stdout)
   assert result.returncode==0,(lane,result.returncode)
   assert (b'[pq] PASS:' if args.source_pq else (b'[hdr_metadata] PASS:' if args.source_sdr else b'[fp16] PASS:')) in result.stdout,lane
   assert b'Failed to reflect' not in result.stdout,lane
   if args.expect_rejection:
    assert b'[Shaders] Explicit HDR encoding requires a matching output.' in result.stdout,lane
    print('PASS rejected incompatible shader HDR encoding',lane,flush=True)
    continue
   im=Image.open(ss).convert('RGB');rgb=im.getpixel((im.width//2,im.height//2))
   assert 145 <= min(rgb) <= 150 and max(rgb)-min(rgb)<=1,(lane,rgb)
   if args.overlay:
    corner=im.getpixel((10,10))
    assert max(corner) < min(rgb)-10,(lane,"overlay not visible",corner,rgb)
   print('PASS native PQ shader samples' if args.source_pq else 'PASS native FP16 signed/above-white shader samples',lane,rgb,flush=True)
