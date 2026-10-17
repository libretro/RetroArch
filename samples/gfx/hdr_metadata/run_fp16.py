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
args = parser.parse_args()
if args.overlay:
    args.stock = True

executable = args.retroarch.resolve()
w = args.output.resolve()
w.mkdir(parents=True, exist_ok=True)
drivers = ["glcore"] if args.hardware else args.drivers.split(",")
r = Path(__file__).resolve().parents[3]
core = w / ("fp16-core.dll" if os.name == "nt" else "fp16-core.so")
cc = shlex.split(os.environ.get("CC", "cc"))
subprocess.run(cc + ["-std=gnu89", "-shared", "-fPIC", "-O1",
                    "-I" + str(r / "libretro-common/include"),
                    str(Path(__file__).with_name("fp16_hw_core.c" if args.hardware else "fp16_core.c")),
                    "-o", str(core), "-lm"], check=True)
(w/'check.slang').write_text("""#version 450
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
}
""",encoding='utf-8')
(w/'check.slangp').write_text('shaders = "1"\nshader0 = "check.slang"\n',encoding='utf-8')
if args.overlay:
    overlay = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(4):
        for x in range(4):
            overlay.putpixel((x, y), (0, 0, 0, 255))
    overlay.save(w / "overlay.png")
    (w / "overlay.cfg").write_text('overlays = "1"\noverlay0_overlay = "overlay.png"\noverlay0_full_screen = "true"\noverlay0_descs = "0"\n', encoding="utf-8")
for driver in drivers:
 for threaded in (['false','true'] if args.threaded == 'both' else [args.threaded]):
  for lend in ([False] if args.hardware else [False,True]):
   lane=('stock-' if args.stock else 'check-')+f'{driver}-{threaded}-{lend}'
   cfg=w/(lane+'.cfg');ss=w/(lane+'.png')
   cfg.write_text(f'''video_driver = "{driver}"
video_threaded = "{threaded}"
video_shader_deferred_loading = "{'true' if args.deferred else 'false'}"
video_hdr_mode = "2"
video_hdr_paper_white_nits = "203"
video_hdr_max_nits = "600"
video_hdr_use_display_peak = "false"
video_hdr_expand_gamut = "0"
video_shader_enable = "{'false' if args.stock else 'true'}"
video_shader = "{(w/'check.slangp').as_posix()}"
video_gpu_screenshot = "true"
video_fullscreen = "false"
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
   if args.resize: env["FP16_RESIZE"]="1"
   else: env.pop("FP16_RESIZE",None)
   if args.stock: env["FP16_STOCK"]="1"
   else: env.pop("FP16_STOCK",None)
   if lend: env['FP16_LEND']='1'
   else: env.pop('FP16_LEND',None)
   result=subprocess.run([str(executable),'-v'] + ([] if args.stock else ['--set-shader',str(w/'check.slangp')]) + ['--config',str(cfg),'-L',str(core),'--max-frames',str(args.frames),'--max-frames-ss','--max-frames-ss-path',str(ss)],cwd=w,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
   (w/(lane+'.log')).write_bytes(result.stdout)
   assert result.returncode==0,(lane,result.returncode)
   assert b'[fp16] PASS:' in result.stdout,lane
   im=Image.open(ss).convert('RGB');rgb=im.getpixel((im.width//2,im.height//2))
   assert 145 <= min(rgb) <= 150 and max(rgb)-min(rgb)<=1,(lane,rgb)
   if args.overlay:
    corner=im.getpixel((10,10))
    assert max(corner) < min(rgb)-10,(lane,"overlay not visible",corner,rgb)
   print('PASS native FP16 signed/above-white shader samples',lane,rgb,flush=True)
