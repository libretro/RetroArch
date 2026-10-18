#!/usr/bin/env python3
"""Compile the D3D shader loaders' actual uniform bindings against their enum."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
header = (root / "gfx/drivers_shader/slang_process.h").read_text(encoding="utf-8")
enum = re.search(r"enum slang_semantic\s*\{.*?\};", header, re.S).group()
fields = {
    "MVP": None,
    "OUTPUT": "rt.size_data",
    "FINAL_VIEWPORT": None,
    "FRAME_COUNT": "frame_count",
    "FRAME_DIRECTION": "frame_direction",
    "FRAME_TIME_DELTA": "frame_time_delta",
    "ORIGINAL_FPS": "original_fps",
    "ROTATION": "rotation",
    "CORE_ASPECT": "core_aspect",
    "CORE_ASPECT_ROT": "core_aspect_rot",
    "TOTAL_SUBFRAMES": "total_subframes",
    "CURRENT_SUBFRAME": "current_subframe",
    "SWAP_COUNT": "swap_count",
}
hdr_fields = {
    "HDR": "hdr_mode",
    "PAPER_WHITE_NITS": "paper_white_nits",
    "PAPER_WHITE_ALIAS": "paper_white_nits",
    "PEAK_NITS": "peak_nits",
    "MAX_NITS_ALIAS": "peak_nits",
    "ENABLE_HDR": "enable_hdr",
    "SCANLINES": "scanlines",
    "SUBPIXEL_LAYOUT": "subpixel_layout",
    "EXPAND_GAMUT": "expand_gamut",
    "INVERSE_TONEMAP": "inverse_tonemap",
    "HDR10": "hdr10",
}
fixture = """
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
ENUM
struct pass
{
   struct { unsigned size_data; } rt;
   unsigned frame_count, frame_direction, frame_time_delta, original_fps;
   unsigned rotation, core_aspect, core_aspect_rot;
   unsigned total_subframes, current_subframe, swap_count, source_hdr, passthrough;
   unsigned hdr_mode, paper_white_nits, scanlines, subpixel_layout;
   unsigned expand_gamut, inverse_tonemap, hdr10, peak_nits, enable_hdr;
};
struct preset { unsigned passes; };
struct chain
{
   struct pass pass[1];
   struct preset *shader_preset;
   unsigned output_size, mvp_last_pass;
};
struct video { struct chain chain, filter; unsigned mvp, identity; };
struct deferred { struct preset *shader_preset; };
struct map { void *uniforms[SLANG_NUM_BASE_SEMANTICS]; };
int main(void)
{
   struct preset preset = { 1 };
   struct video video = { 0 };
   struct video *DRIVER = &video;
   struct deferred deferred = { &preset };
   struct deferred *ds = &deferred;
   unsigned i = 0;
   assert(SLANG_SEMANTIC_PAPER_WHITE_ALIAS == 26);
   (void)ds;
   video.CHAIN.shader_preset = &preset;
   {
   struct map semantics_map = { {
BINDINGS
   } };
   ASSIGNMENTS
   CHECKS
   }
   puts("PASS: LABEL");
   return 0;
}
"""

cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-slang-bindings-") as temporary:
    temporary = Path(temporary)
    for driver, chain in (("d3d10", "chain"), ("d3d11", "chain"), ("d3d12", "filter")):
        source = (root / f"gfx/drivers/{driver}.c").read_text(encoding="utf-8")
        pattern = (r"\{\n(?P<bindings>\s*(?:i ==|&d3d10->mvp)[^}]*?current_subframe[^}]*?)"
                   r"\n\s*\}\n\s*\};(?P<after>.*?)if \(!slang_process\(")
        loaders = list(re.finditer(pattern, source, re.S))
        assert len(loaders) == 2, (driver, len(loaders))
        for lane, loader in enumerate(loaders):
            assignments = "\n".join(re.findall(
                r"#ifdef HAVE_DXGI_HDR|#endif|semantics_map\.uniforms\[[^]]+\]\s*=\s*[^;]+;",
                loader["after"]))
            for hdr in (False, True):
                checks = []
                for semantic, field in fields.items():
                    if field:
                        expected = f"&{driver}->{chain}.pass[i].{field}"
                    elif semantic == "MVP":
                        expected = (f"&{driver}->{chain}.mvp_last_pass" if driver == "d3d11"
                                    else f"&{driver}->mvp")
                    else:
                        expected = f"&{driver}->{chain}.output_size"
                    checks.append(f"assert(semantics_map.uniforms[SLANG_SEMANTIC_{semantic}] == {expected});")
                for semantic, field in hdr_fields.items():
                    expected = f"&{driver}->{chain}.pass[i].{field}" if hdr and driver != "d3d10" else "NULL"
                    checks.append(f"assert(semantics_map.uniforms[SLANG_SEMANTIC_{semantic}] == {expected});")
                expected_source = f"&{driver}->{chain}.pass[i].source_hdr" if driver != "d3d10" else "NULL"
                checks.append(f"assert(semantics_map.uniforms[SLANG_SEMANTIC_SOURCE_HDR] == {expected_source});")
                expected_passthrough = f"&{driver}->{chain}.pass[i].passthrough" if driver != "d3d10" else "NULL"
                checks.append(f"assert(semantics_map.uniforms[SLANG_SEMANTIC_PASSTHROUGH] == {expected_passthrough});")
                for semantic in ("GYROSCOPE", "ACCELEROMETER", "ACCELEROMETER_REST"):
                    checks.append(f"assert(semantics_map.uniforms[SLANG_SEMANTIC_{semantic}] == NULL);")
                label = f"{driver} loader {lane}, DXGI HDR {int(hdr)}"
                code = fixture
                for key, value in (("ENUM", enum), ("DRIVER", driver), ("CHAIN", chain),
                                   ("BINDINGS", loader["bindings"]), ("ASSIGNMENTS", assignments),
                                   ("CHECKS", "\n   ".join(checks)), ("LABEL", label)):
                    code = code.replace(key, value)
                path = temporary / "bindings.c"
                path.write_bytes(code.encode("utf-8"))
                exe = temporary / ("bindings.exe" if os.name == "nt" else "bindings")
                defines = ["-DHAVE_DXGI_HDR"] if hdr else []
                subprocess.run(cc + ["-std=gnu89", "-Wall", "-Wextra", "-Werror",
                                     "-Werror=declaration-after-statement"] + flags + defines +
                               [str(path), "-o", str(exe)], check=True)
                subprocess.run([str(exe)], check=True)
