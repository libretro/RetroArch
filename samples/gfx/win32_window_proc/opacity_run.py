"""Check the shipping Win32 opacity policy with native APIs and host stubs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/common/win32_common.c").read_text(encoding="utf-8")
start = source.index("bool win32_window_set_opacity(")
end = source.index("\nstatic bool win32_window_create(", start)
compiler = (os.environ.get("CC") or shutil.which("cc")
            or shutil.which("gcc") or shutil.which("clang"))
if not compiler:
    raise SystemExit("A C compiler is required")
compiler_flags = []
if "clang" in subprocess.check_output([compiler, "--version"], text=True).lower():
    compiler_flags = ["-Wno-c99-extensions", "-Wno-c11-extensions"]
with tempfile.TemporaryDirectory(prefix="window-opacity-") as directory:
    temporary = Path(directory)
    (temporary / "opacity_impl.inc").write_text(source[start:end], encoding="utf-8")
    executable = temporary / ("opacity_test.exe" if os.name == "nt" else "opacity_test")
    for enabled in (False, True):
        command = [compiler, "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-function", "-Wframe-larger-than=4096",
                   "-I" + str(temporary), "-I" + str(root / "libretro-common/include"),
                   str(Path(__file__).with_name("opacity_test.c")), "-o", str(executable)]
        if enabled:
            command += ["-DHAVE_WINDOW_TRANSP"]
        if os.name == "nt":
            command += ["-luser32"]
        subprocess.run(command + compiler_flags, check=True)
        subprocess.run([str(executable)], check=True, timeout=15)
