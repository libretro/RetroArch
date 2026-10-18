#!/usr/bin/env python3
"""Run shipping D3D11 upload conversion, failure and ownership paths."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/d3d11.c").read_text(encoding="utf-8")
copy = (root / "gfx/common/dxgi_common.c").read_text(encoding="utf-8")
def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n",start)+3]
copy = function(copy,"void dxgi_copy(")
formats = sorted(set(re.findall(r"DXGI_FORMAT[_A-Z0-9]+",copy)) | {"DXGI_FORMAT_UNKNOWN"})
with tempfile.TemporaryDirectory(prefix="retroarch-d3d11-upload-") as directory:
    temp = Path(directory)
    (temp / "formats.inc").write_text("typedef enum { "+", ".join(formats)+" } DXGI_FORMAT;\n")
    (temp / "copy.inc").write_text(copy)
    (temp / "driver.inc").write_text("\n".join(function(driver,name) for name in (
        "static INLINE void d3d11_release_texture(",
        "static unsigned d3d11_format_bytes(",
        "static void d3d11_update_texture(",
        "static uintptr_t d3d11_gfx_load_texture_internal(",
        "static enum video_texture_update d3d11_gfx_update_texture_internal(")))
    cc = shlex.split(os.environ.get("CC","cc"))
    exe = temp / ("test.exe" if os.name == "nt" or "mingw" in cc[0] else "test")
    flags = ["-std=gnu89","-O1","-g1","-Wall","-Wextra","-Werror=declaration-after-statement"]
    flags += shlex.split(os.environ.get("CFLAGS",""))
    subprocess.run(cc+flags+["-I"+str(temp),"-I"+str(root / "libretro-common/include"),
        str(Path(__file__).with_name("upload_test.c")),
        str(root / "libretro-common/gfx/scaler/pixconv.c"),"-o",str(exe)],check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER",""))+[str(exe)],check=True)
