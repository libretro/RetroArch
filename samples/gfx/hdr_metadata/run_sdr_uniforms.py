#!/usr/bin/env python3
"""Check Slang HDR uniforms on an actual SDR output, including fallback."""
import argparse
from itertools import product
import os
from pathlib import Path
import shlex
import subprocess
from PIL import Image

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--retroarch", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--drivers", default="glcore")
p.add_argument("--requested-hdr", choices=(0, 1, 2), type=int, default=0)
a = p.parse_args()
exe = a.retroarch.resolve()
w = a.output.resolve()
w.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[3]
core = w / ("hdr-core.dll" if os.name == "nt" else "hdr-core.so")
cc = shlex.split(os.environ.get("CC", "cc"))
subprocess.run(cc + ["-std=gnu89", "-shared", "-fPIC", "-O1",
                    "-I" + str(root / "libretro-common/include"),
                    str(Path(__file__).with_name("live_core.c")),
                    "-o", str(core), "-lm"], check=True)
for driver, storage in product(a.drivers.split(","), ("ubo", "push")):
    members = "uint HDRMode; float BrightnessNits; uint ExpandGamut;"
    ubo = "mat4 MVP;" + (members if storage == "ubo" else "")
    hdr = "" if storage == "ubo" else "layout(push_constant) uniform HDR { " + members + " } hdr;"
    prefix = "global" if storage == "ubo" else "hdr"
    shader = w / "check.slang"
    shader.write_text("""#version 450
layout(set=0,binding=0) uniform UBO { UBO_MEMBERS } global;
HDR_DECL
#pragma stage vertex
layout(location=0) in vec4 Position;
layout(location=1) in vec2 TexCoord;
layout(location=0) out vec2 vTexCoord;
void main() { gl_Position=global.MVP*Position; vTexCoord=TexCoord; }
#pragma stage fragment
layout(location=0) in vec2 vTexCoord;
layout(location=0) out vec4 FragColor;
void main() {
 bool ok=hdr.HDRMode==0u && hdr.BrightnessNits==0.0 && hdr.ExpandGamut==0u;
 FragColor=vec4(ok ? vec3(1) : vec3(1,0,0),1);
 FragColor.rgb += vec3(vTexCoord,0)*0.000001;
}
""".replace("UBO_MEMBERS", ubo).replace("HDR_DECL", hdr).replace("hdr.", prefix + "."), encoding="utf-8")
    preset = w / "check.slangp"
    preset.write_text('shaders = "1"\nshader0 = "check.slang"\n', encoding="utf-8")
    for threaded in ("false", "true"):
        for deferred in ("false", "true"):
            lane = f"{driver}-{storage}-{threaded}-{deferred}"
            config = w / (lane + ".cfg")
            screenshot = w / (lane + ".png")
            config.write_text(f'''video_driver = "{driver}"
video_threaded = "{threaded}"
video_shader_deferred_loading = "{deferred}"
video_shader_enable = "true"
video_hdr_mode = "{a.requested_hdr}"
video_hdr_paper_white_nits = "203"
video_hdr_max_nits = "600"
video_hdr_use_display_peak = "false"
video_hdr_expand_gamut = "2"
video_gpu_screenshot = "true"
video_fullscreen = "false"
video_vsync = "false"
pause_nonactive = "false"
audio_enable = "false"
input_driver = "null"
input_joypad_driver = "null"
config_save_on_exit = "false"
menu_show_start_screen = "false"
''', encoding="utf-8")
            log = w / (lane + ".log")
            with log.open("wb") as output:
                result = subprocess.run([str(exe), "-v", "--config", str(config),
                                     "--set-shader", str(preset), "-L", str(core),
                                     "--max-frames", "180", "--max-frames-ss",
                                     "--max-frames-ss-path", str(screenshot)],
                                    cwd=w, env=dict(os.environ, HDR_QUERY_REQUESTED_MODE="0"),
                                    stdout=output, stderr=subprocess.STDOUT, timeout=60)
            evidence = log.read_bytes()
            assert result.returncode == 0 and b"[hdr_metadata] PASS:" in evidence, log
            assert b"[ERROR] [Slang]" not in evidence and b"Failed to reflect" not in evidence, log
            im = Image.open(screenshot).convert("RGB")
            rgb = im.getpixel((im.width // 2, im.height // 2))
            assert rgb == (255, 255, 255), (lane, rgb)
            print("PASS SDR HDR uniforms", lane, flush=True)
