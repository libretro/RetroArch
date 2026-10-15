#!/usr/bin/env python3
"""An animated overlay image plays on the screen, threaded and not.

The real retroarch under Xvfb, as overlay_on_screen_test.py runs it,
with one square whose image is an APNG of three solid frames - red,
green, blue, 300 ms each - and no loop count. The square's centre is
sampled for several passes: every frame must show, in order, and the
animation must come round again after the last.

Usage: overlay_apng_on_screen_test.py /path/to/retroarch
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import overlay_on_screen_test as base  # noqa: E402

SQUARE  = (0.25, 0.25, 0.15, 0.15)
COLOURS = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
WATCH_S = 4.0
FRAME_S = 0.3


def apng(path, size=16):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)

    def pixels(rgb):
        row = b"\x00" + bytes(rgb + (255,)) * size
        return zlib.compress(row * size)

    def fctl(seq):
        return struct.pack(">IIIIIHHBB", seq, size, size, 0, 0, 3, 10, 0, 0)

    png  = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    png += chunk(b"acTL", struct.pack(">II", len(COLOURS), 0))
    seq  = 0
    png += chunk(b"fcTL", fctl(seq))
    seq += 1
    png += chunk(b"IDAT", pixels(COLOURS[0]))
    for rgb in COLOURS[1:]:
        png += chunk(b"fcTL", fctl(seq))
        seq += 1
        png += chunk(b"fdAT", struct.pack(">I", seq) + pixels(rgb))
        seq += 1
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def write_overlay(d):
    apng(os.path.join(d, "anim.png"))
    x, y, w, h = SQUARE
    path = os.path.join(d, "test.cfg")
    with open(path, "w") as f:
        f.write("\n".join([
            "overlays = 1",
            "overlay0_full_screen = true",
            "overlay0_normalized = true",
            "overlay0_descs = 1",
            'overlay0_desc0 = "nul,%f,%f,rect,%f,%f"' % (x, y, w, h),
            'overlay0_desc0_overlay = "anim.png"',
        ]) + "\n")
    return path


def which(px):
    for i, (r, g, b) in enumerate(COLOURS):
        if abs(px[0] - r) < 60 and abs(px[1] - g) < 60 and abs(px[2] - b) < 60:
            return i
    return None


def run(retroarch, threaded):
    label = "threaded %s" % ("on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="overlay_apng_")
    disp  = ":%d" % (94 + int(threaded))
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0", "%dx%dx24" % (base.W, base.H)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ra    = None
    fails = []
    try:
        time.sleep(1.5)
        cfg = base.write_config(d, write_overlay(d), threaded)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "--menu", "-c", cfg, "-v"],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        base.settle(disp, os.path.join(d, "screen.xwd"), ra)
        if ra.poll() is not None:
            return ["%s: retroarch exited early (%s)" % (label, ra.returncode)]

        # A change seen across a grab gap longer than a frame may have
        # skipped one, so its order is not judged.
        seen  = []
        close = []
        prev  = None
        end   = time.time() + WATCH_S
        while time.time() < end:
            w, h, rows = base.grab(disp, os.path.join(d, "screen.xwd"))
            now = time.time()
            k = which(rows[int(SQUARE[1] * h)][int(SQUARE[0] * w)])
            if k is not None and (not seen or seen[-1] != k):
                seen.append(k)
                close.append(prev is not None and now - prev < FRAME_S)
            prev = now
            time.sleep(0.05)

        if len(set(seen)) < len(COLOURS):
            fails.append("%s: the square showed frames %s, not all of %d"
                         % (label, seen, len(COLOURS)))
        for a, b, near in zip(seen, seen[1:], close[1:]):
            if near and b != (a + 1) % len(COLOURS):
                fails.append("%s: frame %d followed frame %d" % (label, b, a))
                break
        if seen.count(0) < 2:
            fails.append("%s: the animation never came round again (%s)"
                         % (label, seen))
        print("[%s] %s: frames %s" % ("fail" if fails else "pass", label, seen))
        if fails:
            for line in base.interesting(os.path.join(d, "log.txt")):
                print("    " + line)
    finally:
        if ra and ra.poll() is None:
            ra.kill()
        xvfb.kill()
        shutil.rmtree(d, ignore_errors=True)
    return fails


def main():
    if len(sys.argv) != 2 or not os.access(sys.argv[1], os.X_OK):
        print("usage: overlay_apng_on_screen_test.py /path/to/retroarch")
        return 2
    fails = []
    for threaded in (False, True):
        fails += run(os.path.abspath(sys.argv[1]), threaded)
    for f in fails:
        print("[FAIL] " + f)
    print("%s overlay_apng_on_screen_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
