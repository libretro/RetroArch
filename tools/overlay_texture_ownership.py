#!/usr/bin/env python3
"""A video driver must not delete overlay textures it was lent.

video_overlay_interface_t::load_textures hands a driver the overlay
pack's own texture handles for one page. They stay the pack's: it
deletes them (input_overlay_release_textures), and a handle is usually
on several pages. A driver that frees them with its page - as it
rightly does for textures it made itself in load() - leaves the pack
holding dead handles, and every page that shared them draws as black
quads until the pack is reloaded. That was gl2 from df4b5a7e0b: it
set GL2_FLAG_OVERLAY_BORROWED and never looked at it, so an Android
rotation (a page switch) blacked the overlay out for the session,
while gl3, d3d11, d3d12 and vulkan, written the same week, were fine.

Nothing in CI can run a GL driver, so this reads the source instead.
For every gfx/drivers file that implements load_textures it requires
the three things a correct driver does with its "borrowed" marker:

  set      where load_textures takes the handles;
  tested   negated, in a condition - the guard around the delete;
  cleared  so that the next load() page, whose textures ARE the
           driver's, is deleted again rather than leaked.

Exit status 0 when every driver passes, 1 otherwise.
"""

import os
import re
import sys

ROOT    = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DRIVERS = os.path.join(ROOT, "gfx", "drivers")

# Drivers whose ownership is not expressed as a marker, and why.
EXEMPT = {
    "metal.m": "textures are ARC object references; dropping the "
               "page's array releases a reference, never the texture",
}

# The marker is cleared by wiping the struct it lives in.
CLEARED_BY = {
    "vulkan.c": r"memset\s*\(\s*&vk->overlay\s*,\s*0\s*,",
}

MARKER = r"(?:\b\w*OVERLAY_BORROWED\b|(?:\.|->)borrowed\b)"


def strip_comments(src):
    return re.sub(r"/\*.*?\*/", " ", src, flags=re.S)


def check(name, src):
    problems = []
    code     = strip_comments(src)
    if not re.search(MARKER, code):
        return ["implements load_textures but has no 'borrowed' marker: "
                "its free path cannot tell the pack's textures from its own"]

    is_set = re.search(r"\|=\s*\w*OVERLAY_BORROWED\b", code) \
          or re.search(r"(?:\.|->)borrowed\s*=\s*true\b", code)
    tested = re.search(r"!\s*\(\s*[\w>.\-]+\s*&\s*\w*OVERLAY_BORROWED\s*\)", code) \
          or re.search(r"!\s*[\w>.\-]+(?:\.|->)borrowed\b", code)
    clear  = re.search(r"&=\s*~\s*\(?\s*\w*OVERLAY_BORROWED\b", code) \
          or re.search(r"(?:\.|->)borrowed\s*=\s*false\b", code) \
          or (name in CLEARED_BY and re.search(CLEARED_BY[name], code))

    if not is_set:
        problems.append("the marker is never set in load_textures")
    if not tested:
        problems.append("the marker is never tested: the free path deletes "
                        "lent textures along with its own")
    if not clear:
        problems.append("the marker is never cleared: after a load_textures "
                        "page, a load() page's own textures would leak")
    return problems


def main():
    failed  = 0
    checked = 0
    for name in sorted(os.listdir(DRIVERS)):
        path = os.path.join(DRIVERS, name)
        if not os.path.isfile(path) or not name.endswith((".c", ".m", ".cpp")):
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            src = f.read()
        if not re.search(r"\b\w+_overlay_load_textures\s*\(", src):
            continue
        if name in EXEMPT:
            print("[skip] %s: %s" % (name, EXEMPT[name]))
            continue
        checked += 1
        problems = check(name, src)
        if problems:
            failed += 1
            for p in problems:
                print("[FAIL] %s: %s" % (name, p))
        else:
            print("[pass] %s" % name)

    if not checked:
        print("[FAIL] no driver with load_textures found - the check "
              "is looking in the wrong place")
        return 1
    if failed:
        print("FAIL overlay_texture_ownership (%d of %d drivers)"
              % (failed, checked))
        return 1
    print("PASS overlay_texture_ownership (%d drivers)" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main())
