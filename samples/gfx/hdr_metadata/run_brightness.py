#!/usr/bin/env python3
"""Measure the native frontend HDR output with real SDR core and preset paths."""
from pathlib import Path
import argparse
import json
import os
import re
import shlex
import struct
import subprocess
import zlib

here = Path(__file__).resolve().parent
root = here.parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--harness", type=Path, default=here / "brightness_test.exe")
p.add_argument("--modes", default="1,2")
p.add_argument("--brightness", default="100,200")
p.add_argument("--presets", default="none,one,two,fp16-sdr")
p.add_argument("--skip-ui", action="store_true")
a = p.parse_args()
if os.name != "nt":
    p.error("native Windows and an HDR-capable Vulkan output are required")
w = a.output.resolve()
w.mkdir(parents=True)
core = w / "brightness-core.dll"
subprocess.run(shlex.split(os.environ.get("CC", "cc")) + ["-shared", "-O1", "-std=gnu89",
               "-I" + str(root / "libretro-common/include"), str(here / "brightness_core.c"),
               "-o", str(core)], check=True)
shader = """#version 450
layout(set=0,binding=0) uniform UBO { mat4 MVP; } global;
#pragma stage vertex
layout(location=0) in vec4 Position;
layout(location=1) in vec2 TexCoord;
layout(location=0) out vec2 vTexCoord;
void main(){gl_Position=global.MVP*Position;vTexCoord=TexCoord;}
#pragma stage fragment
layout(set=0,binding=2) uniform sampler2D Source;
layout(location=0) in vec2 vTexCoord;
layout(location=0) out vec4 FragColor;
void main(){FragColor=texture(Source,vTexCoord);}
"""
(w / "identity.slang").write_text(shader)
(w / "identity-fp16.slang").write_text(shader.replace("#version 450", "#version 450\n#pragma format R16G16B16A16_SFLOAT"))
passes = {"none": 0, "one": 1, "two": 2, "fp16-sdr": 1}
for name in ("one", "two", "fp16-sdr"):
    text = 'shaders = "%d"\n' % passes[name]
    for i in range(passes[name]):
        text += 'shader%d = "%s"\nfilter_linear%d = "false"\n' % (i, "identity-fp16.slang" if name == "fp16-sdr" else "identity.slang", i)
    if name == "fp16-sdr":
        text += 'hdr_mode = "0"\n'
    (w / (name + ".slangp")).write_text(text)

def png_chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

row = bytes([0]) + bytes([0, 0, 255, 128]) * 16 + bytes([0, 0, 0, 0]) * 16
png = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">2I5B", 32, 32, 8, 6, 0, 0, 0))
png += png_chunk(b"IDAT", zlib.compress(row * 32)) + png_chunk(b"IEND", b"")
(w / "overlay.png").write_bytes(png)
(w / "overlay.cfg").write_text('overlays = "1"\noverlay0_overlay = "overlay.png"\noverlay0_full_screen = "true"\noverlay0_descs = "0"\n')
colors = [(v/255.0,) * 3 for v in (224, 240, 248, 255)] + [(1,0,0), (0,1,0), (0,0,1), (1,1,0)]
to2020 = ((.6274039,.329283,.0433131), (.0690973,.9195404,.0113623), (.0163914,.0880133,.8955953))
results = []

def pq_nits(value):
    p = (value / 65535.0) ** (32.0 / 2523.0)
    return 10000.0 * (max(0, p - 3424.0/4096.0) / (2413.0/128.0 - 2392.0/128.0*p)) ** (16384.0/2610.0)

def run(mode, brightness, preset, overlay=False):
    name = "%d-%d-%s%s" % (mode, brightness, preset, "-ui" if overlay else "")
    out = w / name
    out.mkdir()
    path = "none" if preset == "none" else str(w / (preset + ".slangp"))
    cmd = [os.environ.get("GDB", "gdb"), "--batch", "-return-child-result", "-ex", "run",
           "-ex", "thread apply all bt", "--args", str(a.harness.resolve()), str(core),
           "vulkan", str(out), str(brightness), "0", path, str(mode), str(passes[preset])]
    env = dict(os.environ)
    env["HDR_PROBE_OVERLAY"] = str(w / "overlay.cfg") if overlay else ""
    with (out / "runtime.log").open("wb") as log:
        child = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, env=env,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = child.wait(timeout=90)
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/PID", str(child.pid), "/T", "/F"], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            child.wait()
            raise RuntimeError("native probe timed out: " + name)
    if code:
        raise RuntimeError("native probe failed: " + str(out / "runtime.log"))
    text = (out / "runtime.log").read_text(errors="replace")
    frames = []
    for index in range(12):
        match = re.search(r"HDR_CAPTURE %d (\d+) (\d+) primaries=(\d+) transfer=(\d+)" % index, text)
        if not match:
            raise RuntimeError("missing capture metadata: " + name)
        width, height, primaries, transfer = map(int, match.groups())
        assert primaries == (9 if mode == 1 else 1) and transfer == (13 if mode == 0 else 16)
        raw = (out / ("frame-%02d.bin" % index)).read_bytes()
        assert len(raw) == width * height * 6
        samples = []
        for patch, rgb in enumerate(colors):
            pos = ((height//2)*width + (patch*2+1)*width//16)*6
            values = struct.unpack_from("<3H", raw, pos)
            actual = [v/257.0 if mode == 0 else pq_nits(v) for v in values]
            samples.append(actual)
            if overlay:
                continue
            linear = [v ** 2.4 for v in rgb]
            expected = [v*255 for v in rgb] if mode == 0 else [v*brightness for v in linear]
            if mode == 1:
                expected = [sum(to2020[c][j]*linear[j] for j in range(3))*brightness for c in range(3)]
            for channel in range(3):
                tolerance = 1 if mode == 0 else .5 + .01*expected[channel]
                if abs(actual[channel] - expected[channel]) > tolerance:
                    raise RuntimeError("%s frame %d patch %d/%d: %.4f expected %.4f" % (name, index, patch, channel, actual[channel], expected[channel]))
        frames.append(samples)
    result = {"case": name, "mode": mode, "brightness": brightness, "preset": preset,
              "overlay": overlay, "captures": 12, "gdb_exit": code, "patch_values": frames}
    results.append(result)
    (w / "result.json").write_text(json.dumps(results, indent=2))
    print("[pass] native brightness: " + name, flush=True)
    return frames[-1]

for preset in ("none", "one", "two"):
    run(0, 200, preset)
for mode in map(int, a.modes.split(",")):
    if mode not in (1,2):
        p.error("HDR modes must be 1 or 2")
    for brightness in map(int, a.brightness.split(",")):
        for preset in a.presets.split(","):
            if preset not in passes:
                p.error("unknown preset: " + preset)
            run(mode, brightness, preset)
if not a.skip_ui:
    stock = run(2, 200, "none", True)
    identity = run(2, 200, "one", True)
    for i in range(8):
        for c in range(3):
            assert abs(stock[i][c] - identity[i][c]) < .5, ("UI/preset disagreement", i, c)
    red, green, blue = identity[3]
    assert 20 < red < 180 and 20 < green < 180 and blue > red + 5, ("UI not visibly translucent", identity[3])
    print("[pass] translucent UI retained over opaque game RGB", flush=True)
