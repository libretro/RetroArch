#!/usr/bin/env python3
"""A movable stick's image follows the finger, and only its moves are sent.

The real retroarch under Xvfb, as overlay_on_screen_test.py runs it,
with one movable analog stick: a magenta square that the overlay draws
where the finger holds the stick. Its geometry goes to the video driver
only when the stick moves - held still, or let go and at rest, nothing
is sent - so the picture is the check that nothing a move needed was
left out:

  - at rest, the square is in the middle of its area
  - pressed and dragged right, it is right of the middle
  - dragged on up, it is above where it was
  - held there, it stays there
  - let go, it is back in the middle

Threaded and not.

Usage: overlay_stick_on_screen_test.py /path/to/retroarch
Needs Xvfb, xwd (x11-apps), libX11, libXtst and a software GL.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import overlay_on_screen_test as base  # noqa: E402
import overlay_menu_mouse_test as mouse  # noqa: E402
import overlay_apng_press_on_screen_test as press  # noqa: E402

# the stick's area: centre, half width, half height - normalised
AREA   = (0.50, 0.50, 0.15, 0.15)
WAIT_S = 1.0


def write_overlay(d):
    base.png_solid(os.path.join(d, "stick.png"), (255, 0, 255, 255))
    x, y, w, h = AREA
    path = os.path.join(d, "pack.cfg")
    with open(path, "w") as f:
        f.write("\n".join([
            "overlays = 1",
            "overlay0_full_screen = true",
            "overlay0_normalized = true",
            "overlay0_descs = 1",
            'overlay0_desc0 = "analog_left,%f,%f,radial,%f,%f"' % (x, y, w, h),
            'overlay0_desc0_overlay = "stick.png"',
            "overlay0_desc0_movable = true",
        ]) + "\n")
    return path


def centre(rows, w, h):
    """The middle of the magenta on the screen, or None."""
    _, n, (x0, x1, y0, y1) = base.picture(w, h, rows)
    if not n:
        return None
    return ((x0 + x1) / 2.0, (y0 + y1) / 2.0)


def run(retroarch, threaded, disp_no):
    label = "threaded %s" % ("on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="overlay_stick_")
    disp  = ":%d" % disp_no
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0", "%dx%dx24" % (base.W, base.H)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ra    = None
    ptr   = None
    fails = []
    try:
        time.sleep(1.5)
        cfg = mouse.write_config(d, write_overlay(d), threaded)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "--menu", "-c", cfg, "-v"],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        xwd = os.path.join(d, "screen.xwd")
        _, _, _, ok = base.settle(disp, xwd, ra)
        if ra.poll() is not None or not ok:
            return ["%s: retroarch did not start (%s)" % (label, ra.poll())]

        def look():
            time.sleep(WAIT_S)
            w, h, rows = base.grab(disp, xwd)
            return centre(rows, w, h)

        ptr = mouse.Pointer(disp)
        btn = press.Buttons(ptr)
        x, y, aw, ah = AREA
        cx, cy = int(x * base.W), int(y * base.H)
        dx, dy = int(0.6 * aw * base.W), int(0.6 * ah * base.H)
        ptr.glide(cx, cy)
        rest  = look()
        btn.left(True)
        ptr.glide(cx + dx, cy)
        right = look()
        ptr.glide(cx + dx, cy - dy)
        up    = look()
        still = look()
        btn.left(False)
        back  = look()

        print("[info] %s: at rest %s, right %s, up %s, held %s, let go %s"
              % (label, rest, right, up, still, back))
        if rest is None or right is None or up is None or still is None or back is None:
            fails.append("%s: the stick's image left the screen" % label)
        else:
            if abs(rest[0] - cx) > 4 or abs(rest[1] - cy) > 4:
                fails.append("%s: at rest the stick is at %s, not the middle (%d, %d)"
                             % (label, rest, cx, cy))
            if right[0] < rest[0] + dx / 2:
                fails.append("%s: dragged right, the stick did not follow (%s)" % (label, right))
            if up[1] > right[1] - dy / 2:
                fails.append("%s: dragged up, the stick did not follow (%s)" % (label, up))
            if abs(still[0] - up[0]) > 2 or abs(still[1] - up[1]) > 2:
                fails.append("%s: held still, the stick moved (%s, then %s)" % (label, up, still))
            if abs(back[0] - rest[0]) > 2 or abs(back[1] - rest[1]) > 2:
                fails.append("%s: let go, the stick is at %s, not back at %s"
                             % (label, back, rest))
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
        print("usage: overlay_stick_on_screen_test.py /path/to/retroarch")
        return 2
    retroarch = os.path.abspath(sys.argv[1])
    fails     = []
    for i, threaded in enumerate((False, True)):
        fails += run(retroarch, threaded, 90 + i)
    for f in fails:
        print("[FAIL] " + f)
    print("%s overlay_stick_on_screen_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
