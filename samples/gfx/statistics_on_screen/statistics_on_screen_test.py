#!/usr/bin/env python3
"""The statistics overlay, on the screen - threaded and not.

Runs the real retroarch binary under Xvfb with software GL and the
smoke core (samples/input/windows_wine_smoke/smoke_core.c, a flat
dark frame and no content), statistics on, the message colour
magenta and its background off, grabs the screen and looks for the
text. Magenta is no colour the core draws.

Whatever the build leaves out, the statistics have to show: the OSD
font they are drawn with reaches the video state through a binding
made at video init, and a build without video filters is still a
build with statistics. Run it against such a build
(--disable-video_filter) as well as a full one.

The screen is looked at once it has stopped changing, as the overlay
tests do: the statistics redraw every frame, so "stopped changing"
here means the amount of magenta holds steady across a poll.

Usage: statistics_on_screen_test.py /path/to/retroarch
Needs Xvfb, xwd (x11-apps), a C compiler and a software GL (Mesa
llvmpipe).
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

W, H       = 640, 480
SETTLE_S   = 3
DEADLINE_S = 40
POLL_S     = 1.5
# The statistics are a dozen lines of text; a frame without them has
# none of this colour at all.
MIN_TEXT_PX = 200

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


def build_core(d):
    out = os.path.join(d, "smoke_core.so")
    subprocess.check_call([os.environ.get("CC", "cc"), "-shared", "-fPIC",
                           "-O1", "-Wall",
                           "-I" + os.path.join(ROOT, "libretro-common", "include"),
                           "-o", out,
                           os.path.join(ROOT, "samples", "input",
                                        "windows_wine_smoke", "smoke_core.c"),
                           "-lpthread"])
    return out


def write_config(d, threaded):
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
            'pause_nonactive = "false"',
            'video_font_enable = "true"',
            'statistics_show = "true"',
            'video_message_color = "ff00ff"',
            'video_msg_bgcolor_enable = "false"',
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
    header_size, width, height = hdr[0], hdr[4], hdr[5]
    byte_order, bpp, bpl, ncolors = hdr[7], hdr[11], hdr[12], hdr[19]
    if bpp != 32:
        raise SystemExit("unexpected xwd depth: %d bpp" % bpp)
    off  = header_size + ncolors * 12
    rows = []
    for y in range(height):
        line = data[off + y * bpl: off + y * bpl + width * 4]
        if byte_order == 0:   # LSBFirst: B G R x
            rows.append([(line[i + 2], line[i + 1], line[i])
                         for i in range(0, len(line), 4)])
        else:                 # MSBFirst: x R G B
            rows.append([(line[i + 1], line[i + 2], line[i + 3])
                         for i in range(0, len(line), 4)])
    return width, height, rows


def text(px):
    """Magenta, allowing for the font's antialiased edges."""
    r, g, b = px
    return r > 150 and b > 150 and g < 90


def picture(rows):
    """(drawn anything, magenta pixels)."""
    drawn = any(px != (0, 0, 0) for row in rows for px in row)
    n     = sum(1 for row in rows for px in row if text(px))
    return drawn, n


def settle(disp, xwd_path, ra):
    time.sleep(SETTLE_S)
    deadline = time.time() + DEADLINE_S
    prev     = None
    while True:
        w, h, rows = grab(disp, xwd_path)
        cur        = picture(rows)
        if cur[0] and prev is not None and prev[0] \
              and abs(cur[1] - prev[1]) <= max(20, cur[1] // 10):
            return w, h, rows, True
        if time.time() >= deadline or ra.poll() is not None:
            return w, h, rows, False
        prev = cur
        time.sleep(POLL_S)


def interesting(log_path):
    marks = ("[GL]", "[GLX]", "[X11]", "[Video]", "[Font]", "ERR",
             "Threaded", "smoke core")
    try:
        with open(log_path, "r", errors="replace") as f:
            lines = f.read().splitlines()
    except IOError:
        return []
    hits = [ln for ln in lines if any(m in ln for m in marks)][:30]
    return hits + ["..."] + lines[-10:]


def run(retroarch, core, threaded):
    label = "threaded %s" % ("on" if threaded else "off")
    d     = tempfile.mkdtemp(prefix="statistics_screen_")
    disp  = ":%d" % (92 + int(threaded))
    xvfb  = subprocess.Popen(["Xvfb", disp, "-screen", "0",
                              "%dx%dx24" % (W, H)],
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
    ra    = None
    fails = []
    try:
        time.sleep(1.5)
        cfg = write_config(d, threaded)
        env = dict(os.environ, DISPLAY=disp, LIBGL_ALWAYS_SOFTWARE="1",
                   HOME=d, XDG_CONFIG_HOME=d)
        log = open(os.path.join(d, "log.txt"), "w")
        ra  = subprocess.Popen([retroarch, "-c", cfg, "-v", "-L", core],
                               env=env, stdout=log, stderr=subprocess.STDOUT)
        w, h, rows, ok = settle(disp, os.path.join(d, "screen.xwd"), ra)
        if ra.poll() is not None:
            return ["%s: retroarch exited early (%s), log:\n%s"
                    % (label, ra.returncode,
                       open(os.path.join(d, "log.txt")).read()[-2000:])]
        n = picture(rows)[1]
        if not ok:
            fails.append("%s: the screen was still changing after %ds"
                         % (label, SETTLE_S + DEADLINE_S))
        if n < MIN_TEXT_PX:
            fails.append("%s: no statistics on the screen (%d text pixels, "
                         "want at least %d)" % (label, n, MIN_TEXT_PX))
        print("[%s] %s (%d text pixels)"
              % ("fail" if fails else "pass", label, n))
        if fails:
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
        print("usage: statistics_on_screen_test.py /path/to/retroarch")
        return 2
    work = tempfile.mkdtemp(prefix="statistics_core_")
    try:
        core  = build_core(work)
        fails = []
        for threaded in (False, True):
            fails += run(os.path.abspath(sys.argv[1]), core, threaded)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for f in fails:
        print("[FAIL] " + f)
    print("%s statistics_on_screen_test" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
