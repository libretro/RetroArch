"""Check Unicode Windows command-line arguments through the shipping entry helper."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "frontend/drivers/platform_win32.c").read_text(encoding="utf-8")
start = source.index("int frontend_win32_main(")
end = source.index("\n}\n", start) + 3
compiler = (os.environ.get("CC") or shutil.which("cc")
            or shutil.which("gcc") or shutil.which("clang"))
if not compiler:
    raise SystemExit("A C compiler is required")
flags = []
if "clang" in subprocess.check_output([compiler, "--version"], text=True).lower():
    flags = ["-Wno-c99-extensions", "-Wno-c11-extensions"]
runner = shlex.split(os.environ.get("RUNNER", ""))
with tempfile.TemporaryDirectory(prefix="win32-argv-") as directory:
    temporary = Path(directory)
    (temporary / "argv_impl.inc").write_text(source[start:end], encoding="utf-8")
    exe = temporary / "argv_test.exe"
    for profile in ([], ["-DHAVE_WIN9X_RUNTIME"], ["-DLEGACY_WIN32"]):
        command = [compiler, "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-function", "-Wno-long-long", "-I" + str(temporary),
                   "-I" + str(root / "libretro-common/include"),
                   str(Path(__file__).with_name("argv_test.c")),
                   str(root / "libretro-common/encodings/encoding_utf.c"),
                   str(root / "libretro-common/compat/compat_strl.c"),
                   "-o", str(exe), "-lshell32"]
        subprocess.run(command + flags + profile, check=True)
        arguments = ["plain.7z", "", "two words", 'quote"inside', "trailing\\"]
        if "-DLEGACY_WIN32" not in profile:
            arguments += ["4 人街霸.7z", "日本語/é/🎮.nes", "-L", "核心/core.dll"]
        result = subprocess.run(runner + [str(exe)] + arguments, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=20)
        assert result.returncode == 23, (result.returncode, result.stderr)
        expected = b"".join(arg.encode("utf-8").hex().encode() + b"\n" for arg in arguments)
        assert result.stdout.replace(b"\r\n", b"\n") == expected, result.stdout
        print("PASS Windows argv:", profile or "Unicode", flush=True)
