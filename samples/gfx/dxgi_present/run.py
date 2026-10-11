#!/usr/bin/env python3
"""Exercise the production DXGI pacing, submission and teardown with COM mocks."""
import os
import shlex
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/common/win_dxgi_present.c").read_text()
start = source.index("unsigned ra_dxgi_bridge_index(")
end = source.index("void ra_dxgi_bridge_hdr_metadata(", start)
destroy_start = source.index("void ra_dxgi_bridge_destroy(")
destroy_end = source.index("ra_dxgi_bridge_t *ra_dxgi_bridge_create(", destroy_start)
with tempfile.TemporaryDirectory(prefix="retroarch-dxgi-") as directory:
    directory = Path(directory)
    (directory / "present.inc").write_text(source[start:end])
    (directory / "destroy.inc").write_text(source[destroy_start:destroy_end])
    for name in ("test", "teardown"):
        executable = directory / (name + ".exe")
        subprocess.run([os.environ.get("CC", "cc"), "-std=c89", "-pedantic", "-Wall",
                        "-Wextra", "-Werror", "-I" + str(directory),
                        str(Path(__file__).with_name(name + ".c")), "-o", str(executable)], check=True)
        subprocess.run(shlex.split(os.environ.get("TEST_RUNNER", "")) + [str(executable)], check=True)
