#!/usr/bin/env python3
"""Check strict C89 MinGW runtime declarations without relaxing the grammar."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
cc = shlex.split(os.environ.get("CC", "gcc"))
flags = ["-std=c89", "-ansi", "-pedantic", "-Werror=pedantic",
         "-Werror=declaration-after-statement", "-Werror=implicit-function-declaration",
         "-fno-builtin", "-include", str(root / "libretro-common/include/compat/mingw_math.h")]
bad = {
    "mixed declarations": "int main(void){int a=1; a++; int b=2; return a+b;}",
    "line comments": "int main(void){ // C99\nreturn 0;}",
    "for declaration": "int main(void){for(int i=0;i<1;i++){}return 0;}",
    "designated initializer": "int a[2]={[1]=2}; int main(void){return a[0];}",
    "compound literal": "int main(void){return ((int[2]){0,1})[0];}",
    "variadic macro": "#define X(...) 0\nint main(void){return X(1);}",
    "inline keyword": "inline int f(void){return 0;} int main(void){return f();}",
}
with tempfile.TemporaryDirectory(prefix="mingw-c89-math-") as tmp:
    tmp = Path(tmp)
    exe = tmp / "math.exe"
    subprocess.run(cc + flags + [str(here / "math_test.c"), "-o", str(exe)], check=True)
    if os.name == "nt":
        command = [os.environ.get("GDB", "gdb"), "--batch", "-return-child-result",
                   "-ex", "run", "-ex", "thread apply all bt", "--args", str(exe)]
    else:
        command = shlex.split(os.environ.get("RUNNER", "wine")) + [str(exe)]
    subprocess.run(command, check=True)
    rotation = tmp / "rotation.exe"
    subprocess.run(cc + flags + ["-O0", "-Wno-long-long", "-isystem",
                   str(root / "gfx/include/dxsdk"), str(here / "rotation_a.c"),
                   str(here / "rotation_b.c"), str(here / "rotation_main.c"),
                   "-o", str(rotation)], check=True)
    subprocess.run(command[:-1] + [str(rotation)], check=True)
    print("[pass] bundled DXSDK two-translation-unit C89 linkage and rotations")
    for name, source in bad.items():
        unit = tmp / "invalid.c"
        unit.write_text(source)
        result = subprocess.run(cc + flags + ["-c", str(unit), "-o", str(tmp / "invalid.o")],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode == 0:
            raise RuntimeError("C99 syntax accepted: " + name)
        print("[pass] strict C89 rejects " + name)
print("[pass] MinGW C89 float math runtime calls and grammar")
