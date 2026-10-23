"""Run the production XRandR error handler without an Xlib dependency."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--cc", default=os.environ.get("CC", "cc"))
p.add_argument("--runner", default="")
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
source = (root / "gfx/display_servers/dispserv_x11.c").read_text(encoding="utf-8")
start = source.index("static int x11_ml_error_handler(")
brace = source.index("{", start)
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
handler = source[start:end]
unit = r'''
#include <assert.h>
#include <stdio.h>
typedef struct Display { int id; } Display;
typedef struct XErrorEvent { int error_code; } XErrorEvent;
typedef struct x11_modeline
{
   Display *dpy;
   unsigned xerrors, xerrors_flag;
} x11_modeline_t;
static x11_modeline_t *x11_ml_current;
static int (*x11_ml_old_error_handler)(Display *, XErrorEvent *);
static unsigned forwarded;
#define RARCH_ERR printf
'''
unit += handler + r'''
static int previous(Display *dpy, XErrorEvent *error)
{
   (void)dpy;
   (void)error;
   forwarded++;
   return 17;
}
int main(void)
{
   Display own = {1}, other = {2};
   XErrorEvent error = {8};
   x11_modeline_t trapped;
   trapped.dpy = &own;
   trapped.xerrors = 0;
   trapped.xerrors_flag = 2;
   x11_ml_current = &trapped;
   x11_ml_old_error_handler = previous;
   assert(x11_ml_error_handler(&own, &error) == 0);
   assert(trapped.xerrors == 2 && forwarded == 0);
   assert(x11_ml_error_handler(&other, &error) == 17);
   assert(trapped.xerrors == 2 && forwarded == 1);
   x11_ml_current = NULL;
   assert(x11_ml_error_handler(&own, &error) == 17 && forwarded == 2);
   x11_ml_old_error_handler = NULL;
   assert(x11_ml_error_handler(&other, &error) == 0 && forwarded == 2);
   puts("PASS: production XRandR error handler connection scope");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="xrandr-error-scope-") as temp:
    src = Path(temp) / "scope.c"
    exe = Path(temp) / ("scope.exe" if os.name == "nt" or a.runner else "scope")
    src.write_text(unit, encoding="utf-8")
    subprocess.run(shlex.split(a.cc) + ["-std=c89", "-pedantic", "-Wall",
                   "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(a.runner) + [str(exe)], check=True)
