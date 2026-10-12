#!/usr/bin/env python3
"""The GL drivers tell the threaded presenter whether the swap just made
waited for the vertical blank (get_last_present_wait), from a flag they
keep beside the swap interval.  An interval set without the flag leaves
the presenter subtracting a wait that was not made, or counting one that
was as drawing time.

Fails if a function in a driver sets the context's swap interval and
does not set the flag after it.
"""
import re
import sys

DRIVERS = (("gfx/drivers/gl2.c", "gl2_present_vsynced"),
           ("gfx/drivers/gl3.c", "gl->present_vsynced"))
CALL = "ctx_driver->swap_interval("


def check(src, flag):
    errs = []
    for chunk in re.split(r"^\}\n", src, flags=re.M):
        call = chunk.rfind(CALL)
        if call < 0 or chunk.find(flag + " =", call) >= 0:
            continue
        names = re.findall(r"^\w[^\n;]*?\b(\w+)\(", chunk, re.M)
        errs.append("%s: sets the swap interval without %s"
                    % (names[-1] if names else "?", flag))
    return errs


def selftest():
    good = ("static void glx_set_nonblock_state(void *d, bool state)\n{\n"
            "   if (gl->ctx_driver->swap_interval)\n"
            "      gl->ctx_driver->swap_interval(gl->ctx_data, interval);\n"
            "   glx_vsynced = (interval != 0);\n}\n")
    other = "static int glx_other(void)\n{\n   return 0;\n}\n"
    unset = good.replace("   glx_vsynced = (interval != 0);\n", "")
    early = good.replace("   glx_vsynced = (interval != 0);\n", "").replace(
        "   if (gl->ctx_driver->swap_interval)\n",
        "   glx_vsynced = (interval != 0);\n"
        "   if (gl->ctx_driver->swap_interval)\n")
    assert check(good + other, "glx_vsynced") == []
    assert check(other + unset, "glx_vsynced") == [
        "glx_set_nonblock_state: sets the swap interval without glx_vsynced"]
    assert len(check(early, "glx_vsynced")) == 1
    print("gl_present_vsynced_check: selftest ok")


def main():
    if "--selftest" in sys.argv:
        selftest()
        return 0
    errs = []
    for path, flag in DRIVERS:
        with open(path, encoding="utf-8", errors="replace") as f:
            errs += ["%s: %s" % (path, e) for e in check(f.read(), flag)]
    for e in errs:
        print("FAIL " + e)
    if not errs:
        print("gl_present_vsynced_check: ok")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
