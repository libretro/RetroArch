#!/usr/bin/env python3
"""An image the LED driver hides stays hidden while its button is held.

Runs the real retroarch binary under Xvfb with software GL, the menu
up, the overlay LED driver on and "Show Inputs on Overlay" set to the
physical controller, holds a key through XTEST and looks at the screen.

led_driver = "overlay" hides the images its ledN_map names until a core
lights them. The highlight of a pressed desc set that desc's alpha
without asking whether its image was hidden, so a hidden image showed
for as long as its button was held - with a gamepad pack loaded, the
d-pad arms at the mapped slots were gone except while pressed.

The pack is two solid magenta squares at full opacity: square 0 is a
"nul" button at image slot 0, square 1 is bound to "up" at slot 1, and
led1_map = 1 hides slot 1. With Up held, for video_threaded off and on:

  - square 0 is on the screen      (the overlay is up)
  - square 1 is not                (hidden stays hidden)

Usage: overlay_led_hidden_test.py /path/to/retroarch
Needs Xvfb, xwd (x11-apps), libX11, libXtst and a software GL (Mesa
llvmpipe).
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

W, H    = screen.W, screen.H
# centre x, centre y, half width, half height, bind
SQUARES = [(0.15, 0.20, 0.10, 0.10, "nul"), (0.85, 0.80, 0.10, 0.10, "up")]
HOLD_S  = 1.5      # Up held this long before the screen is looked at
XK_UP   = 0xff52


def write_overlay(d):
    screen.png_solid(os.path.join(d, "sq.png"), (255, 0, 255, 255))
    lines = ["overlays = 1",
             "overlay0_full_screen = true",
             "overlay0_normalized = true",
             "overlay0_descs = %d" % len(SQUARES)]
    for i, (x, y, w, h, bind) in enumerate(SQUARES):
        lines.append('overlay0_desc%d = "%s,%f,%f,rect,%f,%f"'
                     % (i, bind, x, y, w, h))
        lines.append('overlay0_desc%d_overlay = "sq.png"' % i)
    path = os.path.join(d, "test.cfg")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return path


def write_config(d, overlay, threaded):
    path = os.path.join(d, "retroarch.cfg")
    with open(path, "w") as f:
        f.write("\n".join([
            'video_driver = "gl"',
            'video_threaded = "%s"' % ("true" if threaded else "false"),
            'video_fullscreen = "true"',
            'video_windowed_fullscreen = "true"',
            'menu_driver = "rgui"',
            'menu_enable_widgets = "false"',
            'audio_driver = "null"',
            'input_driver = "x"',
            'pause_nonactive = "false"',
            'led_driver = "overlay"',
            'led1_map = "1"',
            'input_overlay = "%s"' % overlay,
            'input_overlay_enable = "true"',
            'input_overlay_hide_in_menu = "false"',
            'input_overlay_opacity = "1.000000"',
            'input_overlay_auto_scale = "false"',
            'input_overlay_auto_rotate = "false"',
            # 2: physical controller, port 1 - the keyboard's RetroPad
            # binds count as that controller.
            'input_overlay_show_inputs = "2"',
            'input_overlay_show_inputs_port = "0"',
            'config_save_on_exit = "false"',
        ]) + "\n")
    return path


class Keyboard(object):
    def __init__(self, disp):
        x = ctypes.CDLL(ctypes.util.find_library("X11") or "libX11.so.6")
        t = ctypes.CDLL(ctypes.util.find_library("Xtst") or "libXtst.so.6")
        x.XOpenDisplay.restype         = ctypes.c_void_p
        x.XOpenDisplay.argtypes        = [ctypes.c_char_p]
        x.XKeysymToKeycode.restype     = ctypes.c_ubyte
        x.XKeysymToKeycode.argtypes    = [ctypes.c_void_p, ctypes.c_ulong]
        x.XFlush.argtypes              = [ctypes.c_void_p]
        x.XCloseDisplay.argtypes       = [ctypes.c_void_p]
        t.XTestFakeKeyEvent.argtypes   = [ctypes.c_void_p, ctypes.c_uint,
                                          ctypes.c_int, ctypes.c_ulong]
        self.x, self.t = x, t
        self.dpy       = x.XOpenDisplay(disp.encode())
        if not self.dpy:
            raise SystemExit("cannot open display %s" % disp)

    def key(self, keysym, down):
        code = self.x.XKeysymToKeycode(self.dpy, keysym)
        self.t.XTestFakeKeyEvent(self.dpy, code, 1 if down else 0, 0)
        self.x.XFlush(self.dpy)

    def close(self):
        self.x.XCloseDisplay(self.dpy)


def run(retroarch, threaded):
    label = "threaded %s" % ("on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="overlay_led_")
    disp  = ":%d" % (94 + int(threaded))
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0", "%dx%dx24" % (W, H)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ra    = None
    kbd   = None
    fails = []
    try:
        time.sleep(1.5)
        cfg = write_config(d, write_overlay(d), threaded)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "--menu", "-c", cfg, "-v"],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        w, h, rows, ok = screen.settle(disp, os.path.join(d, "screen.xwd"), ra)
        if ra.poll() is not None:
            return ["%s: retroarch exited early (%s)" % (label, ra.returncode)]
        if not ok:
            fails.append("%s: the screen never held still" % label)

        # Released: slot 1 is hidden, slot 0 is not.
        at = [rows[int(y * h)][int(x * w)] for (x, y, _, _, _) in SQUARES]
        if not screen.magenta(at[0]):
            fails.append("%s: square 0 is not on the screen (%s)" % (label, at[0]))
        if screen.magenta(at[1]):
            fails.append("%s: square 1 shows before anything is pressed - "
                         "the LED driver did not hide it" % label)

        kbd = Keyboard(disp)
        kbd.key(XK_UP, True)
        time.sleep(HOLD_S)
        w, h, rows = screen.grab(disp, os.path.join(d, "held.xwd"))
        kbd.key(XK_UP, False)

        at = [rows[int(y * h)][int(x * w)] for (x, y, _, _, _) in SQUARES]
        if not screen.magenta(at[0]):
            fails.append("%s: square 0 went while Up was held (%s)"
                         % (label, at[0]))
        if screen.magenta(at[1]):
            fails.append("%s: square 1, hidden by the LED driver, shows "
                         "while its button is held" % label)
        print("[%s] %s" % ("fail" if fails else "pass", label))
    finally:
        if kbd:
            kbd.close()
        if ra and ra.poll() is None:
            ra.kill()
        xvfb.kill()
        shutil.rmtree(d, ignore_errors=True)
    return fails


def main():
    if len(sys.argv) != 2 or not os.access(sys.argv[1], os.X_OK):
        print("usage: overlay_led_hidden_test.py /path/to/retroarch")
        return 2
    fails = []
    for threaded in (False, True):
        fails += run(os.path.abspath(sys.argv[1]), threaded)
    for f in fails:
        print("[FAIL] " + f)
    print("%s overlay_led_hidden_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
