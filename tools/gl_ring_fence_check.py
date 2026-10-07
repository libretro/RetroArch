#!/usr/bin/env python3
"""The GL drivers hand the threaded wrapper's hardware ring a fence taken
ahead of the buffer swap.  A fence taken after it completes only when a
driver that queues swaps releases that swap, which holds a hardware
rendered core to the display's rate in fast-forward.

Fails if a driver's frame function swaps before taking the fence, or if
its hand-over (hw_ring_fence_signal) stops using the one taken.

The thread that waits on the fence does not delete it either: the video
thread made it and can still be in the swap that follows it, and a
driver has aborted on the delete.  Fails if hw_ring_fence_wait deletes.
"""
import re
import sys

DRIVERS = (("gfx/drivers/gl2.c", "gl2"), ("gfx/drivers/gl3.c", "gl3"))


def body(src, name):
    m = re.search(r"^static \w+ %s\(" % re.escape(name), src, re.M)
    if not m:
        return None
    end = src.find("\n}\n", m.start())
    return src[m.start():end] if end >= 0 else None


def check(src, prefix):
    errs = []
    frame = body(src, prefix + "_frame")
    signal = body(src, prefix + "_hw_ring_fence_signal")
    if frame is None or signal is None:
        return ["%s_frame or %s_hw_ring_fence_signal not found" % (prefix, prefix)]
    swap = frame.find("swap_buffers(")
    drawn = frame.find("%s_hw_ring_drawn(gl);" % prefix)
    if swap < 0:
        errs.append("%s_frame: no swap_buffers call" % prefix)
    elif drawn < 0 or drawn > swap:
        errs.append("%s_frame: swaps before %s_hw_ring_drawn(gl)" % (prefix, prefix))
    if "hw_ring_done_sync" not in signal:
        errs.append("%s_hw_ring_fence_signal: does not hand over hw_ring_done_sync" % prefix)
    wait = body(src, prefix + "_hw_ring_fence_wait")
    if wait is not None and "glDeleteSync" in wait:
        errs.append("%s_hw_ring_fence_wait: deletes the video thread's sync" % prefix)
    return errs


def selftest():
    good = ("static bool glx_frame(void *d)\n{\n   glx_hw_ring_drawn(gl);\n"
            "   gl->ctx_driver->swap_buffers(gl->ctx_data);\n   return true;\n}\n"
            "static void glx_hw_ring_fence_signal(void *d, void *f)\n{\n"
            "   f->sync = gl->hw_ring_done_sync;\n}\n")
    late = good.replace("   glx_hw_ring_drawn(gl);\n", "").replace(
        "   return true;", "   glx_hw_ring_drawn(gl);\n   return true;")
    unused = good.replace("gl->hw_ring_done_sync", "glFenceSync(0, 0)")
    assert check(good, "glx") == []
    assert len(check(late, "glx")) == 1
    assert len(check(unused, "glx")) == 1
    deletes = good + ("static bool glx_hw_ring_fence_wait(void *d, void *f, unsigned t)\n{\n"
                      "   glDeleteSync(f->sync);\n   return true;\n}\n")
    assert len(check(deletes, "glx")) == 1
    print("gl_ring_fence_check: selftest ok")


def main():
    if "--selftest" in sys.argv:
        selftest()
        return 0
    errs = []
    for path, prefix in DRIVERS:
        with open(path, encoding="utf-8", errors="replace") as f:
            errs += ["%s: %s" % (path, e) for e in check(f.read(), prefix)]
    for e in errs:
        print("FAIL " + e)
    if not errs:
        print("gl_ring_fence_check: ok")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
