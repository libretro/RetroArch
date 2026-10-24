"""Build the modal pump regression on Windows or with MinGW and Wine."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
native = os.name == "nt"
compiler = os.environ.get("CC") or shutil.which("gcc" if native else "x86_64-w64-mingw32-gcc")
if not compiler:
    raise SystemExit("A native Windows compiler or MinGW cross-compiler is required")
flags = []
if "clang" in subprocess.check_output([compiler, "--version"], text=True).lower():
    flags.append("-Wno-c99-extensions")
with tempfile.TemporaryDirectory(prefix="win32-modal-") as directory:
    temporary = Path(directory)
    executable = temporary / "modal_frame_test.exe"
    subprocess.run([compiler, "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-Wframe-larger-than=4096",
                    "-I" + str(root / "libretro-common/include")] + flags + [
                    str(Path(__file__).with_name("modal_frame_test.c")),
                    "-o", str(executable), "-lgdi32", "-luser32"], check=True)
    environment = os.environ.copy()
    command = [str(executable)]
    if not native:
        wine = shutil.which("wine")
        if not wine:
            raise SystemExit("Wine is required to execute the cross-built regression")
        environment.update(WINEPREFIX=str(temporary / "wine"), WINEARCH="win64", WINEDEBUG="-all")
        command.insert(0, wine)
        if not environment.get("DISPLAY"):
            command = ["xvfb-run", "-a"] + command
    try:
        result = subprocess.run(command, env=environment, timeout=120)
        if result.returncode == 77 and not native:
            print("UNAVAILABLE: Wine modal continuity; checking safe fallback", flush=True)
            subprocess.run([wine, str(executable), "--fallback"], env=environment,
                           check=True, timeout=30)
        else:
            result.check_returncode()
    finally:
        if not native and shutil.which("wineserver"):
            subprocess.run(["wineserver", "-k"], env=environment, check=False)
