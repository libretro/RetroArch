#!/usr/bin/env python3
"""The menu's mouse, with an overlay up - threaded and not.

Runs the real retroarch binary under Xvfb with software GL, fullscreen,
menu mouse on and an input overlay visible in the menu, moves the X
pointer and looks at the screen where it went.

The menu switched its mouse off whenever any overlay was alive, so an
LED overlay (led_driver = "overlay": a page of "nul" buttons whose
images are lit and unlit) took the cursor away from a desktop user who
had nothing to press on it. An overlay now claims the menu's pointer
only if its page has a desc that does something when pressed, or its
own pointer mode is on.

Two packs, each for video_threaded off and on:

  - display: two "nul" buttons - the menu cursor follows the pointer
  - input:   the same two squares bound to "a" - the menu leaves the
             pointer to the overlay and draws no cursor, as before

The cursor is looked for in a box around each place the pointer is
put: the box changes when the pointer arrives and again when it
leaves. The squares sit well away from both boxes.

Usage: overlay_menu_mouse_test.py /path/to/retroarch
Needs Xvfb, xwd (x11-apps), libX11 and a software GL (Mesa llvmpipe).
"""

import ctypes
import ctypes.util
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import overlay_on_screen_test as screen  # noqa: E402

W, H     = screen.W, screen.H
SPOTS    = [(320, 250), (440, 170)]   # clear of both squares
BOX      = 24                          # half-size of the box looked at
MIN_PX   = 20                          # a cursor changes at least this many
MOVE_S   = 1.5                         # after the pointer stops, before a grab


def write_pack(d, bind):
    screen.png_solid(os.path.join(d, "sq.png"), (255, 0, 255, 255))
    lines = ["overlays = 1",
             "overlay0_full_screen = true",
             "overlay0_normalized = true",
             "overlay0_descs = %d" % len(screen.SQUARES)]
    for i, (x, y, w, h) in enumerate(screen.SQUARES):
        lines.append('overlay0_desc%d = "%s,%f,%f,rect,%f,%f"' % (i, bind, x, y, w, h))
        lines.append('overlay0_desc%d_overlay = "sq.png"' % i)
    path = os.path.join(d, "pack.cfg")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return path


def write_config(d, pack, threaded):
    path = os.path.join(d, "retroarch.cfg")
    with open(path, "w") as f:
        f.write("\n".join([
            'video_driver = "gl"',
            'video_threaded = "%s"' % ("true" if threaded else "false"),
            'video_fullscreen = "true"',
            'video_windowed_fullscreen = "true"',
            'menu_driver = "rgui"',
            'menu_mouse_enable = "true"',
            'menu_enable_widgets = "false"',
            'input_driver = "x"',
            'audio_driver = "null"',
            'pause_nonactive = "false"',
            'input_overlay = "%s"' % pack,
            'input_overlay_enable = "true"',
            'input_overlay_hide_in_menu = "false"',
            'input_overlay_pointer_enable = "false"',
            'input_overlay_show_mouse_cursor = "false"',
            'input_overlay_opacity = "1.000000"',
            'input_overlay_auto_scale = "false"',
            'input_overlay_show_inputs = "0"',
            'config_save_on_exit = "false"',
        ]) + "\n")
    return path


class Pointer(object):
    def __init__(self, disp):
        x = ctypes.CDLL(ctypes.util.find_library("X11") or "libX11.so.6")
        x.XOpenDisplay.restype        = ctypes.c_void_p
        x.XOpenDisplay.argtypes       = [ctypes.c_char_p]
        x.XDefaultRootWindow.restype  = ctypes.c_ulong
        x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        x.XWarpPointer.argtypes       = ([ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong]
                                         + [ctypes.c_int] * 2 + [ctypes.c_uint] * 2
                                         + [ctypes.c_int] * 2)
        x.XFlush.argtypes             = [ctypes.c_void_p]
        x.XCloseDisplay.argtypes      = [ctypes.c_void_p]
        self.x   = x
        self.dpy = x.XOpenDisplay(disp.encode())
        if not self.dpy:
            raise SystemExit("cannot open display %s" % disp)
        self.root = x.XDefaultRootWindow(self.dpy)

    def glide(self, tx, ty):
        """A few steps rather than one jump, so every poll sees motion."""
        for i in range(10, -1, -1):
            self.x.XWarpPointer(self.dpy, 0, self.root, 0, 0, 0, 0, tx - i, ty - i)
            self.x.XFlush(self.dpy)
            time.sleep(0.05)
        time.sleep(MOVE_S)

    def close(self):
        self.x.XCloseDisplay(self.dpy)


def box_diff(a, b, cx, cy):
    n = 0
    for y in range(max(0, cy - BOX), min(H, cy + BOX)):
        ra, rb = a[y], b[y]
        for x in range(max(0, cx - BOX), min(W, cx + BOX)):
            if ra[x] != rb[x]:
                n += 1
    return n


def run(retroarch, bind, threaded, disp_no):
    kind  = "display" if bind == "nul" else "input"
    label = "%s pack, threaded %s" % (kind, "on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="overlay_mouse_")
    disp  = ":%d" % disp_no
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0", "%dx%dx24" % (W, H)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ra    = None
    ptr   = None
    try:
        time.sleep(1.5)
        cfg = write_config(d, write_pack(d, bind), threaded)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "--menu", "-c", cfg, "-v"],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        xwd = os.path.join(d, "screen.xwd")
        _, _, _, ok = screen.settle(disp, xwd, ra)
        if ra.poll() is not None:
            return ["%s: retroarch exited early (%s), log:\n%s"
                    % (label, ra.returncode, open(os.path.join(d, "log.txt")).read()[-2000:])]
        if not ok:
            return ["%s: the frontend never finished starting" % label]

        ptr = Pointer(disp)
        ptr.glide(*SPOTS[0])
        _, _, at0 = screen.grab(disp, xwd)
        ptr.glide(*SPOTS[1])
        _, _, at1 = screen.grab(disp, xwd)

        moved = [box_diff(at0, at1, cx, cy) for (cx, cy) in SPOTS]
        drawn = all(m >= MIN_PX for m in moved)
        print("[%s] %s: %d and %d px changed around the two pointer spots"
              % ("info", label, moved[0], moved[1]))
        if kind == "display" and not drawn:
            return ["%s: no menu cursor followed the pointer - a page of "
                    "\"nul\" buttons took the menu's mouse" % label]
        if kind == "input" and any(m >= MIN_PX for m in moved):
            return ["%s: the menu drew a cursor over an overlay that takes "
                    "input" % label]
        return []
    finally:
        if ptr:
            ptr.close()
        if ra and ra.poll() is None:
            ra.kill()
        xvfb.kill()
        shutil.rmtree(d, ignore_errors=True)


def main():
    if len(sys.argv) != 2 or not os.access(sys.argv[1], os.X_OK):
        print("usage: overlay_menu_mouse_test.py /path/to/retroarch")
        return 2
    retroarch = os.path.abspath(sys.argv[1])
    fails     = []
    disp_no   = 94
    for bind in ("nul", "a"):
        for threaded in (False, True):
            fails  += run(retroarch, bind, threaded, disp_no)
            disp_no += 1
    for f in fails:
        print("[FAIL] " + f)
    print("%s overlay_menu_mouse_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
