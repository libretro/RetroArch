#!/usr/bin/env python3
"""A two-frame overlay image shows its pressed frame while pressed.

The real retroarch under Xvfb, as overlay_on_screen_test.py runs it,
with one square bound to "a" whose image is an APNG of two solid frames
- red unpressed, green pressed. Both frames are one texture, stacked, and
a press moves the texture's rectangle; so the square is sampled near
its top, middle and bottom: all red at rest, all green while the mouse
button is held on it, all red again once let go. A square showing both
colours would be the whole stack drawn. Threaded and not.

Usage: overlay_apng_press_on_screen_test.py /path/to/retroarch
VIDEO_DRIVER in the environment picks another driver than gl.
Needs Xvfb, xwd (x11-apps), libX11, libXtst and a software GL.
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
import overlay_on_screen_test as base  # noqa: E402
import overlay_apng_on_screen_test as anim  # noqa: E402
import overlay_menu_mouse_test as mouse  # noqa: E402

SQUARE = (0.30, 0.30, 0.20, 0.20)
RED    = (255, 0, 0)
GREEN  = (0, 255, 0)
WAIT_S = 1.0


def write_overlay(d):
    anim.COLOURS = [RED, GREEN]
    anim.apng(os.path.join(d, "pair.png"))
    x, y, w, h = SQUARE
    path = os.path.join(d, "pack.cfg")
    with open(path, "w") as f:
        f.write("\n".join([
            "overlays = 1",
            "overlay0_full_screen = true",
            "overlay0_normalized = true",
            "overlay0_descs = 1",
            'overlay0_desc0 = "a,%f,%f,rect,%f,%f"' % (x, y, w, h),
            'overlay0_desc0_overlay = "pair.png"',
        ]) + "\n")
    return path


class Buttons(object):
    def __init__(self, ptr):
        xtst = ctypes.CDLL(ctypes.util.find_library("Xtst") or "libXtst.so.6")
        xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                              ctypes.c_int, ctypes.c_ulong]
        self.xtst = xtst
        self.ptr  = ptr

    def left(self, down):
        self.xtst.XTestFakeButtonEvent(self.ptr.dpy, 1, 1 if down else 0, 0)
        self.ptr.x.XFlush(self.ptr.dpy)


def colours(rows, w, h):
    """The square near its top, middle and bottom."""
    x, y, sw, sh = SQUARE
    cx   = int((x + sw / 2) * w)
    out  = []
    for f in (0.15, 0.5, 0.85):
        px = rows[int((y + sh * f) * h)][cx]
        if   abs(px[0] - 255) < 60 and px[1] < 60:
            out.append("red")
        elif abs(px[1] - 255) < 60 and px[0] < 60:
            out.append("green")
        else:
            out.append("other")
    return out


def run(retroarch, threaded, disp_no):
    label = "threaded %s" % ("on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="overlay_apng_press_")
    disp  = ":%d" % disp_no
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0", "%dx%dx24" % (base.W, base.H)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ra    = None
    ptr   = None
    fails = []
    try:
        time.sleep(1.5)
        cfg = mouse.write_config(d, write_overlay(d), threaded)
        # another driver than gl, to look at it by hand
        if os.environ.get("VIDEO_DRIVER"):
            with open(cfg) as f:
                text = f.read().replace('video_driver = "gl"',
                        'video_driver = "%s"' % os.environ["VIDEO_DRIVER"])
            with open(cfg, "w") as f:
                f.write(text)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "--menu", "-c", cfg, "-v"],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        xwd = os.path.join(d, "screen.xwd")
        _, _, _, ok = base.settle(disp, xwd, ra)
        if ra.poll() is not None or not ok:
            return ["%s: retroarch did not start (%s)" % (label, ra.poll())]

        ptr = mouse.Pointer(disp)
        btn = Buttons(ptr)
        x, y, sw, sh = SQUARE
        ptr.glide(int((x + sw / 2) * base.W), int((y + sh / 2) * base.H))
        w, h, rows = base.grab(disp, xwd)
        rest = colours(rows, w, h)
        btn.left(True)
        time.sleep(WAIT_S)
        w, h, rows = base.grab(disp, xwd)
        held = colours(rows, w, h)
        btn.left(False)
        time.sleep(WAIT_S)
        w, h, rows = base.grab(disp, xwd)
        after = colours(rows, w, h)

        print("[info] %s: at rest %s, held %s, let go %s" % (label, rest, held, after))
        if rest != ["red"] * 3:
            fails.append("%s: at rest the square is %s, not all red" % (label, rest))
        if held != ["green"] * 3:
            fails.append("%s: pressed the square is %s, not all green" % (label, held))
        if after != ["red"] * 3:
            fails.append("%s: let go the square is %s, not all red" % (label, after))
        if fails:
            for line in base.interesting(os.path.join(d, "log.txt")):
                print("    " + line)
    finally:
        if ptr:
            ptr.close()
        if ra and ra.poll() is None:
            ra.kill()
        xvfb.kill()
        shutil.rmtree(d, ignore_errors=True)
    return fails


def main():
    if len(sys.argv) != 2 or not os.access(sys.argv[1], os.X_OK):
        print("usage: overlay_apng_press_on_screen_test.py /path/to/retroarch")
        return 2
    retroarch = os.path.abspath(sys.argv[1])
    fails     = []
    for i, threaded in enumerate((False, True)):
        fails += run(retroarch, threaded, 90 + i)
    for f in fails:
        print("[FAIL] " + f)
    print("%s overlay_apng_press_on_screen_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
