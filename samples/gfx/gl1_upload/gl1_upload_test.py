#!/usr/bin/env python3
"""Check what the gl1 driver puts on screen for a known core frame.

Runs retroarch on the gl1 driver under Xvfb with gl1_upload_core,
which draws a fixed pattern at an odd size from a buffer whose rows
are padded with white, and compares RetroArch's own screenshot of the
viewport with the pattern pixel for pixel.  Each configuration steers
the driver down a different upload path:

  8888             frame uploaded straight from the core's buffer
  8888, no BGRA    CPU swizzle to RGBA, tightly packed
  565              GL_UNSIGNED_SHORT_5_6_5 from the core's buffer
  565, GL 1.1      CPU expansion to BGRA8888 in a staging buffer
  resize (both)    storage respecified as the frame size changes
  threaded         the threaded wrapper's copy of the frame

Needs Xvfb (xvfb-run) and a software GL (Mesa llvmpipe).
Usage: gl1_upload_test.py path/to/retroarch
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE   = os.path.dirname(os.path.abspath(__file__))
CORE   = os.path.join(HERE, "gl1_upload_core_libretro.so")
W, H   = 317, 223


def pattern(x, y):
    return ((x * 7 + y * 13 + x * y) * 2654435761) & 0xffffffff


def expected(fmt):
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            p = pattern(x, y)
            if fmt == "565":
                c = p >> 16
                r, g, b = (c >> 11) & 0x1f, (c >> 5) & 0x3f, c & 0x1f
                row.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4),
                            (b << 3) | (b >> 2)))
            else:
                row.append(((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff))
        rows.append(row)
    return rows


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s: not a PNG" % path)
    pos, idat = 8, b""
    width = height = ctype = None
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, ctype = struct.unpack(">IIBB", body[:10])
            if depth != 8 or ctype not in (2, 6):
                raise SystemExit("%s: unsupported PNG (depth %d, type %d)"
                                 % (path, depth, ctype))
        elif tag == b"IDAT":
            idat += body
        pos += 12 + length
    bpp  = 3 if ctype == 2 else 4
    raw  = zlib.decompress(idat)
    stride = width * bpp
    prev = bytearray(stride)
    rows = []
    for y in range(height):
        base = y * (stride + 1)
        ftype = raw[base]
        line = bytearray(raw[base + 1:base + 1 + stride])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xff
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xff
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xff
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xff
        rows.append([tuple(line[x * bpp:x * bpp + 3]) for x in range(width)])
        prev = line
    return width, height, rows


def run(retroarch, name, fmt, env_extra, resize, threaded=False):
    d = tempfile.mkdtemp(prefix="gl1_upload_")
    try:
        shot = os.path.join(d, "shot.png")
        cfg  = os.path.join(d, "retroarch.cfg")
        with open(cfg, "w") as f:
            f.write("\n".join([
                'video_driver = "gl1"',
                'video_threaded = "%s"' % ("true" if threaded else "false"),
                'video_smooth = "false"',
                'video_scale = "1.000000"',
                'video_scale_integer = "true"',
                'video_fullscreen = "false"',
                'video_windowed_fullscreen = "false"',
                'video_gpu_screenshot = "true"',
                'video_font_enable = "false"',
                'audio_driver = "null"',
                'input_driver = "null"',
                'menu_driver = "rgui"',
                'config_save_on_exit = "false"',
                'screenshot_directory = "%s"' % d,
                ""]))
        env = dict(os.environ)
        env.update(env_extra)
        env["GL1_UPLOAD_FMT"] = fmt
        if resize:
            env["GL1_UPLOAD_RESIZE"] = "1"
        else:
            env.pop("GL1_UPLOAD_RESIZE", None)
        # 18 frames ends inside the third seven-frame run, which is at
        # full size again after the small one, so the storage has been
        # respecified twice by then.
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1024x768x24", retroarch,
               "--config=" + cfg, "-L", CORE, "--max-frames=18",
               "--max-frames-ss", "--max-frames-ss-path=" + shot, "-v"]
        log = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, timeout=120).stdout
        if not os.path.exists(shot):
            sys.stdout.write(log.decode("utf-8", "replace")[-4000:])
            print("FAIL %s: no screenshot" % name)
            return False
        w, h, rows = read_png(shot)
        # Integer scaling with nearest filtering: every texel is an
        # exact n x n block of the viewport, whatever n the window
        # manager ended up with.
        n = w // W
        if n < 1 or (w, h) != (n * W, n * H):
            print("FAIL %s: screenshot is %dx%d, not a whole multiple of %dx%d"
                  % (name, w, h, W, H))
            return False
        want = expected(fmt)
        bad  = 0
        for y in range(H):
            for x in range(W):
                got = rows[y * n + n // 2][x * n + n // 2]
                exp = want[y][x]
                if any(abs(got[i] - exp[i]) > 1 for i in range(3)):
                    if bad < 5:
                        print("  %s: (%d,%d) got %s want %s"
                              % (name, x, y, got, exp))
                    bad += 1
        if bad:
            print("FAIL %s: %d of %d pixels differ" % (name, bad, W * H))
            return False
        print("ok   %s" % name)
        return True
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    retroarch = os.path.abspath(sys.argv[1])
    no_bgra   = {"MESA_EXTENSION_OVERRIDE": "-GL_EXT_bgra"}
    gl11      = {"MESA_GL_VERSION_OVERRIDE": "1.1"}
    cases = [
        ("8888",                "8888", {},      False),
        ("8888 no BGRA",        "8888", no_bgra, False),
        ("8888 GL 1.1",         "8888", gl11,    False),
        ("565",                 "565",  {},      False),
        ("565 GL 1.1",          "565",  gl11,    False),
        ("8888 resize",         "8888", {},      True),
        ("565 resize",          "565",  {},      True),
        ("565 GL 1.1 resize",   "565",  gl11,    True),
    ]
    ok = True
    for name, fmt, env, resize in cases:
        ok = run(retroarch, name, fmt, env, resize) and ok
    # The threaded wrapper hands gl1 its own copy of the frame at its
    # own pitch rather than the core's.
    for name, fmt, env in (("8888 threaded", "8888", {}),
                           ("565 threaded", "565", {})):
        ok = run(retroarch, name, fmt, env, False, True) and ok
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
