#!/usr/bin/env python3
"""surface_requirements_check.py - producers ask the surface layer.

What a producer should emit - channel order, whether a 10-bit source
is worth decoding - is the surface layer's answer
(gfx_surface_query_requirements), so that a decoder has one place to
ask instead of three unrelated flags to know about.

The files listed below asked those flags directly before the query
existed, and are migrated one at a time; a file that is not on the
list may not start. That is the whole rule: this fails when a source
file outside the allowlist reads VIDEO_FLAG_USE_RGBA or
GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE, and it fails just as loudly when a
file on the list has stopped reading them and nobody removed it - a
migration is not finished until the allowlist says so.

The video drivers themselves are not producers: they are where the
flags are answered from, so they are not listed and not checked.
"""

import os
import re
import sys

FLAGS = ("VIDEO_FLAG_USE_RGBA", "GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE")

# Where the flags are answered rather than consumed.
EXEMPT_DIRS = ("gfx/drivers/", "gfx/drivers_shader/", "gfx/drivers_context/",
               "samples/", "deps/")
EXEMPT_FILES = ("gfx/gfx_surface.c", "gfx/video_driver.c")

# Producers that read the flags directly, each still to be migrated to
# gfx_surface_query_requirements(). Shrinks; never grows.
# retroarch.c clears the flag as part of tearing a driver down and
# runloop.c answers a core's environment query with it; neither is a
# producer deciding what to decode, so neither is migrated.
ALLOWLIST = {
    "retroarch.c",
    "runloop.c",
}

def sources(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames
                       if d not in (".git", "deps", "media", "pkg")]
        for name in filenames:
            if not name.endswith((".c", ".m", ".cpp")):
                continue
            path = os.path.relpath(os.path.join(dirpath, name), root)
            path = path.replace(os.sep, "/")
            if path in EXEMPT_FILES:
                continue
            if any(path.startswith(d) for d in EXEMPT_DIRS):
                continue
            yield path

def quiet(fn, *args):
    import io, contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        return fn(*args)


def selftest(root):
    """A file outside the allowlist that asks must be caught, and an
    entry whose file has stopped asking must be caught: the second is
    what keeps the list shrinking as producers migrate."""
    probe = os.path.join(root, "menu", "surface_requirements_probe.c")
    ok    = True
    with open(probe, "w") as handle:
        handle.write("int probe = VIDEO_FLAG_USE_RGBA;\n")
    try:
        if quiet(main_check, root) == 0:
            print("selftest: an unlisted producer was not caught")
            ok = False
    finally:
        os.remove(probe)

    ALLOWLIST.add("no/such/file.c")
    try:
        if quiet(main_check, root) == 0:
            print("selftest: a stale allowlist entry was not caught")
            ok = False
    finally:
        ALLOWLIST.discard("no/such/file.c")

    if quiet(main_check, root) != 0:
        print("selftest: the tree does not pass its own check")
        ok = False
    if ok:
        print("selftest: unlisted producer and stale entry both caught")
    return 0 if ok else 1


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if "--selftest" in sys.argv[1:]:
        return selftest(root)
    return main_check(root)


def main_check(root):
    pattern = re.compile("|".join(FLAGS))
    found = set()
    for path in sources(root):
        with open(os.path.join(root, path), "r", errors="replace") as handle:
            for line in handle:
                if line.lstrip().startswith(("/*", "*", "//")):
                    continue
                if pattern.search(line):
                    found.add(path)
                    break

    new = sorted(found - ALLOWLIST)
    stale = sorted(ALLOWLIST - found)
    for path in new:
        print("%s asks the video flags directly; ask "
              "gfx_surface_query_requirements() instead" % path)
    for path in stale:
        print("%s no longer asks the video flags: drop it from the "
              "allowlist in tools/surface_requirements_check.py" % path)
    if new or stale:
        return 1
    print("producers asking the video flags directly: %d, all known"
          % len(found))
    return 0

if __name__ == "__main__":
    sys.exit(main())
