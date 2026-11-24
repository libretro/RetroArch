#!/usr/bin/env python3
"""Exercise SDL's configure handler while its X11 parent disappears."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

source = Path(sys.argv[1]).read_text()
start = source.index("// Real configure notify events")
end = source.index("/* Some window managers", start)
configure = source[start:end]
handler = ""
if "static unsigned char x11_last_error;" in source:
    start = source.index("static unsigned char x11_last_error;")
    handler = source[start:source.index("static void X11_HandleClipboardEvent", start)]

fixture = r'''
#include <X11/Xlib.h>
#include <assert.h>
#include <stdio.h>
#define SDL_LOG_CATEGORY_VIDEO 0
#define SDL_LogError(category, ...) fprintf(stderr, __VA_ARGS__)
#define X11_XGetErrorText XGetErrorText
#define X11_XSetErrorHandler XSetErrorHandler
#define X11_XSync XSync
#define X11_XTranslateCoordinates XTranslateCoordinates
static int reparent(Display *d, Window w, Window *root, Window *parent,
      Window **children, unsigned *count)
{
   int result = XQueryTree(d, w, root, parent, children, count);
   assert(result && *parent != *root);
   if (*children) XFree(*children);
   *children = NULL;
   XReparentWindow(d, w, *root, 0, 0);
   XDestroyWindow(d, *parent);
   XSync(d, False);
   return result;
}
#define X11_XQueryTree reparent
HANDLER
static void configure_event(Display *display, XEvent *xevent)
{
   struct video { Display *display; } video = {display};
   struct window { struct video *videodata; } window = {&video};
   struct window *data = &window;
   switch (xevent->type) {
   case ConfigureNotify:
      CONFIGURE
      break;
   }
}
int main(void)
{
   XEvent event = {0};
   XWindowAttributes attrs;
   Display *d = XOpenDisplay(NULL);
   Window parent, client;
   assert(d);
   parent = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 80, 80, 0, 0, 0);
   client = XCreateSimpleWindow(d, parent, 0, 0, 40, 40, 0, 0, 0);
   event.type = ConfigureNotify;
   event.xconfigure.display = d;
   event.xconfigure.window = client;
   configure_event(d, &event);
   assert(XGetWindowAttributes(d, client, &attrs));
   XDestroyWindow(d, client);
   XCloseDisplay(d);
   puts("PASS SDL X11 configure: vanished parent, live client, no fatal error");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="sdl-x11-reparent-") as directory:
    unit = Path(directory) / "reparent.c"
    executable = Path(directory) / "reparent"
    unit.write_text(fixture.replace("HANDLER", handler).replace("CONFIGURE", configure))
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c99", "-Wall", "-Wextra", "-Werror", str(unit), "-lX11",
        "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=10)
