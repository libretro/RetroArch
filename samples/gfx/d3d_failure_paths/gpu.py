#!/usr/bin/env python3
"""Sample shipping D3D11 mapped and busy packed 10-bit uploads on the GPU."""
from pathlib import Path
import os
import hashlib
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
shader = root / "samples/gfx/d3d12_menu_mapping"
expected = re.search(r'MENU_MAPPING_SOURCE_SHA256 "([0-9a-f]+)"',
                     (shader / "shader.h").read_text(encoding="utf-8"))[1]
assert hashlib.sha256((shader / "sample_cs.hlsl").read_text(encoding="utf-8").encode()).hexdigest() == expected
with tempfile.TemporaryDirectory(prefix="retroarch-d3d11-upload-") as directory:
    temp = Path(directory)
    (temp / "formats.inc").write_text("typedef enum { "+", ".join(formats)+" } DXGI_FORMAT;\n")
    (temp / "copy.inc").write_text(copy)
    (temp / "driver.inc").write_text("\n".join(function(driver,name) for name in (
        "static INLINE void d3d11_release_texture(",
        "static unsigned d3d11_format_bytes(",
        "static void d3d11_update_texture(")))
    cc = shlex.split(os.environ.get("CC","cc"))
    exe = temp / ("test.exe" if os.name == "nt" or "mingw" in cc[0] else "test")
    flags = ["-std=gnu89","-O1","-g1","-Wall","-Wextra","-Werror=declaration-after-statement"]
    flags += shlex.split(os.environ.get("CFLAGS",""))
    windows = os.name == "nt" or "mingw" in cc[0]
    libraries = ["-ld3d11","-ldxguid","-luuid","-lm"] if windows else []
    subprocess.run(cc+flags+["-I"+str(temp),"-I"+str(root / "libretro-common/include"),
        str(Path(__file__).with_name("gpu_test.c")),
        str(root / "libretro-common/gfx/scaler/pixconv.c"),"-o",str(exe)]+libraries,check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER",""))+[str(exe)],check=True)
