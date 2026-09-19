#!/usr/bin/env python3
"""video_thread_state_check.py - the video thread reads its snapshot.

The main thread builds a video_frame_info_t and hands it across the
thread boundary; the video driver works from that. A driver function
that reaches back into config_get_ptr() instead is reading settings the
main thread may be writing at that moment - which is a data race, and
TSan found one in the null driver's present_last() rather than anyone
noticing it by reading.

This looks for the class instead of waiting for the next one. It seeds
from the entry points the video worker calls - frame(), present_last()
and their neighbours - walks the calls each one makes inside its own
translation unit, and reports live-state access anywhere it reaches.

What it cannot see: a call through a function pointer, and a callee in
another file. So a clean run means "no path this can follow", not "no
path exists". It is a floor, not a proof, and it is still worth having
- every site it names is a real one.
"""

import os
import re
import sys

# The frontend state a driver must not read from the video thread: the
# settings the main thread writes, and the video singleton whose fields
# it updates around a frame.
LIVE = ("config_get_ptr", "video_state_get_ptr", "runloop_state_get_ptr")

# Entry points the video worker runs. A driver names its own functions
# after them by convention (gl2_gfx_frame, d3d11_gfx_present_last), and
# the wrapper calls them with nothing but the snapshot.
# Only the asynchronous path. A poke the wrapper marshals - set_menu_
# texture_frame, a readback, a viewport query - runs on the video
# thread while the main thread waits inside the command, so it reads
# settings nobody is writing. frame() and present_last() are the two
# the video thread runs on its own.
SEEDS = re.compile(r"_(gfx_)?(frame|present_last)$")
# set_texture_frame and set_menu_texture_frame end in "frame" and are
# marshalled pokes, not the asynchronous path; video_driver_frame and
# video_driver_cached_frame are the frontend side, which runs on the
# main thread and is where the snapshot comes from.
NOT_SEEDS = re.compile(r"(set_[a-z_]*texture_frame|^video_driver_)")

# The drivers are the ones behind the wrapper; the wrapper itself and
# the frontend-side files run on the main thread by definition.
ROOTS  = ("gfx/drivers",)
EXTRA  = ("gfx/video_driver.c",)

# Reached from a seed by name, and safe: build_info is the snapshot's
# producer, which the main thread calls before the hand-off - reading
# the settings is its whole job.
EXEMPT = ("video_driver_build_info",)

# Known, with the reason. A driver for a platform that has no video
# thread cannot race its own frame(), so its reads are latent rather
# than live: they become findings the moment that platform runs
# threaded, and the entry is what has to be removed then. gl1's is a
# null check - it takes the pointer to test it and reads the paper
# white from its own state - which this cannot see.
ALLOWED = {
   ("gfx/drivers/ctr_gfx.c",    "3DS: no video thread"),
   ("gfx/drivers/d3d8.c",       "the D3D8 path predates threaded video"),
   ("gfx/drivers/d3d9cg.c",     "the D3D9 Cg path predates threaded video"),
   ("gfx/drivers/exynos_gfx.c", "Exynos: single-threaded KMS frontend"),
   ("gfx/drivers/omap_gfx.c",   "OMAP: single-threaded KMS frontend"),
   ("gfx/drivers/ps2_gfx.c",    "PS2: no video thread"),
   ("gfx/drivers/gl1.c",        "null check, not a read of the settings"),
}

def functions(text):
    """Every function defined in this text, as name -> body.

    A signature may run over several lines (gl2_frame does), so the
    name is taken from the line that opens the parameter list and the
    body from the brace that follows the line closing it."""
    out   = {}
    lines = text.split("\n")
    start = re.compile(r"^(?:static\s+)?(?:INLINE\s+|inline\s+)?"
                       r"[A-Za-z_][\w \*]*?\b([a-z_][\w]*)\s*\(")
    i = 0
    while i < len(lines):
        m = start.match(lines[i])
        if not m:
            i += 1
            continue
        # walk to the line that closes the parameter list
        depth = 0
        j     = i
        while j < len(lines) and j < i + 12:
            depth += lines[j].count("(") - lines[j].count(")")
            if depth <= 0:
                break
            j += 1
        if j >= len(lines) or depth > 0:
            i += 1
            continue
        tail = lines[j].split(")")[-1].strip()
        k    = j
        if tail != "{":
            if k + 1 >= len(lines) or not lines[k + 1].startswith("{"):
                i = j + 1
                continue
            k = j + 1
        body  = []
        depth = 0
        while k < len(lines):
            depth += lines[k].count("{") - lines[k].count("}")
            body.append(lines[k])
            if depth <= 0 and len(body) > 1:
                break
            k += 1
        if len(body) > 1:
            out[m.group(1)] = "\n".join(body)
            i = k
        i += 1
    return out


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    call = re.compile(r"\b([a-z_][\w]*)\s*\(")
    findings = []
    scanned = 0

    paths = list(EXTRA)
    for base in ROOTS:
        for name in sorted(os.listdir(os.path.join(root, base))):
            if name.endswith((".c", ".m")):
                paths.append(os.path.join(base, name))
    for path in paths:
        if True:
            with open(os.path.join(root, path), "r", errors="replace") as fh:
                text = fh.read()
            funcs = functions(text)
            if not funcs:
                continue
            scanned += 1

            seen = set()
            todo = [f for f in funcs
                    if SEEDS.search(f) and not NOT_SEEDS.search(f)]
            while todo:
                fn = todo.pop()
                if fn in seen:
                    continue
                seen.add(fn)
                body = funcs.get(fn)
                if not body or fn in EXEMPT:
                    continue
                for hit in LIVE:
                    if re.search(r"\b%s\s*\(" % hit, body):
                        findings.append((path, fn, hit))
                for callee in set(call.findall(body)):
                    if callee in funcs and callee not in seen:
                        todo.append(callee)

    allowed = {p for p, _ in ALLOWED}
    stale   = allowed - {p for p, _, _ in findings}
    findings = [f for f in findings if f[0] not in allowed]

    for path in sorted(stale):
        print("%s no longer reaches live state: drop it from ALLOWED in "
              "tools/video_thread_state_check.py" % path)
    for path, fn, hit in sorted(set(findings)):
        print("%s: %s() reaches %s() on the video thread; read the "
              "frame's snapshot instead" % (path, fn, hit))
    if findings or stale:
        return 1
    print("video-thread entry points in %d driver(s): no live-state "
          "access this can follow" % scanned)
    return 0

if __name__ == "__main__":
    sys.exit(main())
