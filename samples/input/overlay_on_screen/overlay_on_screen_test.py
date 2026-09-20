#!/usr/bin/env python3
"""The overlay, on the screen, where its config says - threaded and not.

Runs the real retroarch binary under Xvfb with software GL, an input
overlay enabled and the menu up, grabs the screen and looks at it. No
stub, no copy of any logic: this is the check a person makes by eye.

Every other overlay test in the tree passed while an enabled overlay
covered the whole screen with threaded video off (2026-09-19): the
pack's pixels were released once the driver had its textures, the
struct that release cleared was the desc's own image, and "does this
desc have an image" was asked of its pixels - so no desc's geometry
was ever set and every image was drawn full-screen. It took a user
with threaded video off to see it, because no test ever looked.

The overlay is made here: two solid magenta squares, top left and
bottom right, on a full-screen page at full opacity. Magenta is no
colour the menu draws. For video_threaded off and on:

  - the middle of each square is magenta          (the overlay is up)
  - the middle of the screen is not               (it is not stretched)
  - magenta covers about the area of two squares  (nor anything between)

Usage: overlay_on_screen_test.py /path/to/retroarch
Needs Xvfb, xwd (x11-apps) and a software GL (Mesa llvmpipe).
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

W, H     = 640, 480
# centre x, centre y, half width, half height - normalised, as in the cfg
SQUARES  = [(0.15, 0.20, 0.10, 0.10), (0.85, 0.80, 0.10, 0.10)]
SETTLE_S = 8


def png_solid(path, rgba, size=16):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    row = b"\x00" + bytes(rgba) * size
    raw = row * size
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw)))
        f.write(chunk(b"IEND", b""))


def write_overlay(d):
    png_solid(os.path.join(d, "sq.png"), (255, 0, 255, 255))
    lines = ["overlays = 1",
             "overlay0_full_screen = true",
             "overlay0_normalized = true",
             "overlay0_descs = %d" % len(SQUARES)]
    for i, (x, y, w, h) in enumerate(SQUARES):
        lines.append('overlay0_desc%d = "nul,%f,%f,rect,%f,%f"' % (i, x, y, w, h))
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
            'audio_driver = "null"',
            'pause_nonactive = "false"',
            'input_overlay = "%s"' % overlay,
            'input_overlay_enable = "true"',
            'input_overlay_hide_in_menu = "false"',
            'input_overlay_opacity = "1.000000"',
            'input_overlay_auto_scale = "false"',
            'input_overlay_auto_rotate = "false"',
            'input_overlay_show_inputs = "0"',
            'config_save_on_exit = "false"',
        ]) + "\n")
    return path


def grab(display, out):
    """The root window as (width, height, rows of (r, g, b))."""
    subprocess.check_call(["xwd", "-root", "-silent", "-display", display,
                           "-out", out])
    with open(out, "rb") as f:
        data = f.read()
    hdr = struct.unpack(">25I", data[:100])
    header_size, depth, width, height = hdr[0], hdr[3], hdr[4], hdr[5]
    byte_order, bpp, bpl, ncolors     = hdr[7], hdr[11], hdr[12], hdr[19]
    if bpp != 32:
        raise SystemExit("unexpected xwd depth: %d bpp" % bpp)
    off  = header_size + ncolors * 12
    rows = []
    for y in range(height):
        line = data[off + y * bpl: off + y * bpl + width * 4]
        if byte_order == 0:   # LSBFirst: B G R x
            rows.append([(line[i + 2], line[i + 1], line[i]) for i in range(0, len(line), 4)])
        else:                 # MSBFirst: x R G B
            rows.append([(line[i + 1], line[i + 2], line[i + 3]) for i in range(0, len(line), 4)])
    return width, height, rows


def magenta(px):
    r, g, b = px
    return r > 200 and b > 200 and g < 60


def shapes(w, h, rows):
    """The overlay on the screen, as rectangles.

    A run of rows with the same magenta spans is one band of the
    picture, so two squares side by side print as two bands of two
    spans. That is the whole diagnosis of a geometry failure: where
    the driver actually put the images, against where the config
    puts them, without anyone having to reproduce the run.
    """
    out  = []
    last = None
    for y in range(h):
        spans = []
        x0    = None
        for x in range(w):
            if magenta(rows[y][x]):
                if x0 is None:
                    x0 = x
            elif x0 is not None:
                spans.append((x0, x - 1))
                x0 = None
        if x0 is not None:
            spans.append((x0, w - 1))
        if not spans:
            last = None
            continue
        if last is not None and out[-1][2] == spans:
            out[-1][1] = y
        else:
            out.append([y, y, spans])
        last = spans
    return ["rows %d-%d: %s" % (a, b, ", ".join("x %d-%d" % s for s in sp))
            for a, b, sp in out]


def interesting(log_path):
    """The lines of the log that say what drew, and the last few."""
    marks = ("[Overlay]", "[GL]", "[GLX]", "[EGL]", "[X11]", "[Video]",
             "ERR", "resolution", "Threaded")
    try:
        with open(log_path, "r", errors="replace") as f:
            lines = f.read().splitlines()
    except IOError:
        return []
    hits = [ln for ln in lines if any(m in ln for m in marks)][:30]
    return hits + ["..."] + lines[-10:]


def run(retroarch, threaded):
    label = "threaded %s" % ("on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="overlay_screen_")
    disp  = ":%d" % (90 + int(threaded))
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0", "%dx%dx24" % (W, H)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ra    = None
    fails = []
    try:
        time.sleep(1.5)
        cfg = write_config(d, write_overlay(d), threaded)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "--menu", "-c", cfg, "-v"],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        time.sleep(SETTLE_S)
        if ra.poll() is not None:
            return ["%s: retroarch exited early (%s), log:\n%s"
                    % (label, ra.returncode, open(os.path.join(d, "log.txt")).read()[-2000:])]
        w, h, rows = grab(disp, os.path.join(d, "screen.xwd"))

        for i, (x, y, hw, hh) in enumerate(SQUARES):
            if not magenta(rows[int(y * h)][int(x * w)]):
                fails.append("%s: square %d is not on the screen where its "
                             "config puts it (%s at its centre)"
                             % (label, i, rows[int(y * h)][int(x * w)]))
        if magenta(rows[h // 2][w // 2]):
            fails.append("%s: the middle of the screen is overlay - an image "
                         "is stretched over the whole screen" % label)
        covered  = sum(1 for row in rows for px in row if magenta(px)) / float(w * h)
        expected = sum(4 * hw * hh for (_, _, hw, hh) in SQUARES)
        if not (0.5 * expected <= covered <= 1.5 * expected):
            fails.append("%s: overlay covers %.1f%% of the screen, its two "
                         "squares are %.1f%%" % (label, 100 * covered, 100 * expected))
        print("[%s] %s (overlay covers %.1f%% of the screen, expected %.1f%%)"
              % ("fail" if fails else "pass", label, 100 * covered, 100 * expected))
        if fails:
            print("  screen is %dx%d, the config puts the squares at:" % (w, h))
            for i, (x, y, hw, hh) in enumerate(SQUARES):
                print("    square %d: rows %d-%d, x %d-%d"
                      % (i, int((y - hh) * h), int((y + hh) * h) - 1,
                         int((x - hw) * w), int((x + hw) * w) - 1))
            print("  what is on it:")
            for line in shapes(w, h, rows) or ["nothing magenta"]:
                print("    " + line)
            print("  log:")
            for line in interesting(os.path.join(d, "log.txt")):
                print("    " + line)
    finally:
        if ra and ra.poll() is None:
            ra.kill()
        xvfb.kill()
        shutil.rmtree(d, ignore_errors=True)
    return fails


def main():
    if len(sys.argv) != 2 or not os.access(sys.argv[1], os.X_OK):
        print("usage: overlay_on_screen_test.py /path/to/retroarch")
        return 2
    fails = []
    for threaded in (False, True):
        fails += run(os.path.abspath(sys.argv[1]), threaded)
    for f in fails:
        print("[FAIL] " + f)
    print("%s overlay_on_screen_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
