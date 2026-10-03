#!/usr/bin/env python3
"""Check that an OpenGL hardware core gets a context on EGL.

Runs retroarch on the gl driver with the X11 EGL context under Xvfb
and egl_hw_core, a hardware core that reads the GL version in
context_reset and clears the frontend's framebuffer to one colour
every frame.  Each run checks that the core had a context when it was
told it had one, and that its frame is what reaches the screen.

Under threaded video the core's context lives on the main thread while
the driver's holds the window surface on the video thread.  EGL will
not bind one surface on two threads, so the core's context is made
current without it - surfaceless where the display can, on a pbuffer
where it cannot - and the shim hides parts of Mesa's EGL so that every
one of those is the path taken:

  threaded                  EGL_KHR_surfaceless_context
  threaded, pbuffer         a pbuffer on the context's own config
  threaded, pbuffer config  a pbuffer on a config searched for
  unthreaded                the driver's own context, as ever

Needs Xvfb (xvfb-run) and a software GL (Mesa llvmpipe).
Usage: egl_hw_core_test.py path/to/retroarch
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE  = os.path.dirname(os.path.abspath(__file__))
CORE  = os.path.join(HERE, "egl_hw_core_libretro.so")
SHIM  = os.path.join(HERE, "egl_shim.so")
COLOR = (51, 153, 204)


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


def run(retroarch, name, threaded, hide):
    d = tempfile.mkdtemp(prefix="egl_hw_core_")
    try:
        shot = os.path.join(d, "shot.png")
        cfg  = os.path.join(d, "retroarch.cfg")
        with open(cfg, "w") as f:
            f.write("\n".join([
                'video_driver = "gl"',
                'video_context_driver = "egl_x"',
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
        env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        if hide:
            env["LD_PRELOAD"]        = SHIM
            env["EGL_HW_CORE_SHIM"]  = hide
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1024x768x24", retroarch,
               "--config=" + cfg, "-L", CORE, "--max-frames=20",
               "--max-frames-ss", "--max-frames-ss-path=" + shot, "-v"]
        proc = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=120)
        log  = proc.stdout.decode("utf-8", "replace")

        def fail(why):
            sys.stdout.write(log[-4000:])
            print("FAIL %s: %s" % (name, why))
            return False

        # The run must be the one the case is named for.
        if 'Found GL context: "egl_x"' not in log:
            return fail("the gl driver did not come up on the EGL context")
        if threaded != ("Starting threaded video driver" in log):
            return fail("threaded video is not %s" % ("on" if threaded else "off"))
        if "[egl_hw_core] context_reset: NO CONTEXT" in log:
            return fail("the core was given no context")
        if "[egl_hw_core] context_reset: GL_VERSION" not in log:
            return fail("the core's context_reset did not run")
        if bool(hide) != ("[egl_shim] pbuffer created" in log):
            return fail("the core's context is %s a pbuffer"
                        % ("not on" if hide else "on"))
        if proc.returncode != 0:
            return fail("retroarch exited with %d" % proc.returncode)
        if not os.path.exists(shot):
            return fail("no screenshot")
        w, h, rows = read_png(shot)
        bad = sum(1 for row in rows for px in row
                  if any(abs(px[i] - COLOR[i]) > 1 for i in range(3)))
        if bad:
            return fail("%d of %d pixels are not the core's colour (centre %s)"
                        % (bad, w * h, rows[h // 2][w // 2]))
        print("ok   %s" % name)
        return True
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    retroarch = os.path.abspath(sys.argv[1])
    cases = [
        ("threaded",                 True,  ""),
        ("threaded, pbuffer",        True,  "surfaceless"),
        ("threaded, pbuffer config", True,  "surfaceless,pbuffer"),
        ("unthreaded",               False, ""),
    ]
    ok = True
    for name, threaded, hide in cases:
        ok = run(retroarch, name, threaded, hide) and ok
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
