#!/usr/bin/env python3
"""Check that an OpenGL hardware core can draw on a thread of its own.

Runs retroarch on the gl driver under Xvfb with gl_shared_context, a
core that takes RETRO_ENVIRONMENT_GET_HW_SHARED_CONTEXT_INTERFACE in
context_reset, makes a context in its own context's share group, and
on a thread of its own clears a texture to one colour through that
context.  retro_run blits the texture to the frontend's framebuffer
after a GPU wait on the thread's fence, so the colour on screen is the
other thread's drawing, seen through the share group.

On GLX (x) and on EGL (egl_x), with threaded video on and off, and with
a compatibility and a 3.3 core profile context - the second made with
the window's context attributes.

Needs Xvfb (xvfb-run) and a software GL (Mesa llvmpipe).
Usage: gl_shared_context_test.py path/to/retroarch
"""

import os
import shutil
import subprocess
import sys
import tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
CORE  = os.path.join(HERE, "gl_shared_context_libretro.so")
COLOR = (204, 102, 51)

sys.path.insert(0, os.path.join(HERE, "..", "egl_hw_core"))
from egl_hw_core_test import read_png  # noqa: E402


def run(retroarch, name, ctx, threaded, core_profile):
    d = tempfile.mkdtemp(prefix="gl_shared_context_")
    try:
        shot = os.path.join(d, "shot.png")
        cfg  = os.path.join(d, "retroarch.cfg")
        with open(cfg, "w") as f:
            f.write("\n".join([
                'video_driver = "gl"',
                'video_context_driver = "%s"' % ctx,
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
        if core_profile:
            env["GL_SHARED_CONTEXT_CORE"] = "1"
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1024x768x24", retroarch,
               "--config=" + cfg, "-L", CORE, "--max-frames=30",
               "--max-frames-ss", "--max-frames-ss-path=" + shot, "-v"]
        try:
            proc = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=120)
        except subprocess.TimeoutExpired as e:
            sys.stdout.write((e.output or b"").decode("utf-8", "replace")[-4000:])
            print("FAIL %s: retroarch did not finish (hung)" % name)
            return False
        log = proc.stdout.decode("utf-8", "replace")

        def fail(why):
            sys.stdout.write(log[-4000:])
            print("FAIL %s: %s" % (name, why))
            return False

        if 'Found GL context: "%s"' % ctx not in log:
            return fail("the gl driver did not come up on %s" % ctx)
        if threaded != ("Starting threaded video driver" in log):
            return fail("threaded video is not %s" % ("on" if threaded else "off"))
        for line in ("context_reset: GL_VERSION",
                     "context_reset: shared context made",
                     "worker: GL_VERSION",
                     "worker: drew the texture",
                     "retro_run: showing the worker's texture"):
            if "[gl_shared_context] " + line not in log:
                return fail("missing \"%s\"" % line)
        if proc.returncode != 0:
            return fail("retroarch exited with %d" % proc.returncode)
        if not os.path.exists(shot):
            return fail("no screenshot")
        w, h, rows = read_png(shot)
        bad = sum(1 for row in rows for px in row
                  if any(abs(px[i] - COLOR[i]) > 1 for i in range(3)))
        if bad:
            return fail("%d of %d pixels are not the worker's colour (centre %s)"
                        % (bad, w * h, rows[h // 2][w // 2]))
        print("ok   %s" % name)
        return True
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    retroarch = os.path.abspath(sys.argv[1])
    ok = True
    for ctx, label in (("x", "GLX"), ("egl_x", "EGL")):
        for threaded in (False, True):
            for core_profile in (False, True):
                name = "%s, %s, %s" % (label,
                        "threaded" if threaded else "unthreaded",
                        "3.3 core" if core_profile else "compatibility")
                ok = run(retroarch, name, ctx, threaded, core_profile) and ok
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
