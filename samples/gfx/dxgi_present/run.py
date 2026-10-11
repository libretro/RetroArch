#!/usr/bin/env python3
"""Exercise the production DXGI pacing and submission code with portable COM mocks."""
import os
import shlex
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/common/win_dxgi_present.c").read_text()
start = source.index("unsigned ra_dxgi_bridge_index(")
end = source.index("void ra_dxgi_bridge_hdr_metadata(", start)
with tempfile.TemporaryDirectory(prefix="retroarch-dxgi-") as directory:
    directory = Path(directory)
    (directory / "present.inc").write_text(source[start:end])
    executable = directory / "test.exe"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c89", "-pedantic", "-Wall",
                    "-Wextra", "-Werror", "-I" + str(directory),
                    str(Path(__file__).with_name("test.c")), "-o", str(executable)], check=True)
    subprocess.run(shlex.split(os.environ.get("TEST_RUNNER", "")) + [str(executable)], check=True)
