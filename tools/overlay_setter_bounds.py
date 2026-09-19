#!/usr/bin/env python3
"""An overlay's per-image setters refuse an image the driver does not hold.

video_overlay_interface_t has three setters that take an image index:
set_alpha, vertex_geom and tex_geom. The frontend calls them whenever
the layout, the opacity or the page changes - not only straight after
a load() that worked. With no page loaded the driver's arrays are NULL;
after a page with fewer images, or a load that failed half way, the
index is off the end. A setter that writes anyway is a NULL write or a
write into whatever sits after the page's block.

7661ab06e7 bounded the GL geometry setters and missed set_alpha in all
three GL drivers; the D3D, Vulkan and Metal setters had no check at
all. No runner can run these drivers, so this reads the source: each
setter, or the one helper it maps its buffer through, must compare the
index against the page's count before it writes.

Covers the drivers that have been audited (DRIVERS below); add a
driver to the table when its setters are brought into line.

Exit status 0 when every setter passes, 1 otherwise.
"""

import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DIR  = os.path.join(ROOT, "gfx", "drivers")

DRIVERS = [
    "d3d9cg.c", "d3d9hlsl.c", "d3d10.c", "d3d11.c", "d3d12.c",
    "gl1.c", "gl2.c", "gl3.c", "vulkan.c",
]

SETTER = r"\b(\w+_overlay_(?:set_alpha|vertex_geom|tex_geom))\s*\("
# "index >= count", "image >= gl->overlays", "index < d3d->overlays_size"
BOUND  = r"\b(?:index|image)\b\s*(?:>=|<)\s*[\w>.\-]+"
HELPER = r"\b(\w+_overlay_sprite_map)\s*\("


def strip_comments(src):
    return re.sub(r"/\*.*?\*/", " ", src, flags=re.S)


def body_of(code, start):
    """The brace-matched body of the definition whose '(' is at start,
    or None when it is a declaration or a call."""
    depth = 0
    i     = start
    while i < len(code) and code[i] != ")" or depth:
        if code[i] == "(":
            depth += 1
        elif code[i] == ")":
            depth -= 1
            if depth == 0:
                break
        i += 1
    j = i + 1
    while j < len(code) and code[j] in " \t\r\n":
        j += 1
    if j >= len(code) or code[j] != "{":
        return None
    depth = 0
    k     = j
    while k < len(code):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                return code[j:k + 1]
        k += 1
    return None


def functions(code, pattern):
    out = {}
    for m in re.finditer(pattern, code):
        body = body_of(code, m.end() - 1)
        if body is not None:
            out[m.group(1)] = body
    return out


def check_c(name, code):
    problems = []
    setters  = functions(code, SETTER)
    helpers  = functions(code, HELPER)
    if len(setters) < 3:
        problems.append("found %d of the 3 setters - the check is not "
                        "reading this driver right" % len(setters))
    for fn in sorted(setters):
        body = setters[fn]
        if re.search(BOUND, body):
            continue
        called = [h for h in helpers if re.search(r"\b%s\s*\(" % h, body)]
        if any(re.search(BOUND, helpers[h]) for h in called):
            continue
        problems.append("%s() writes image [index] without comparing the "
                        "index to the page's count" % fn)
    return problems


def check_metal(code):
    """Objective-C: every update goes through -_getForIndex:, which must
    check the index against the buffer, and each caller must take NULL
    for an answer."""
    problems = []
    m = re.search(r"-\s*\(SpriteVertex\s*\*\)\s*_getForIndex:.*?\n\}", code, re.S)
    if not m or not re.search(r"_vert\.length", m.group(0)):
        problems.append("-_getForIndex: does not check the index against "
                        "the vertex buffer's length")
    users = re.findall(r"\[self _getForIndex:index\];(.*?)\n\}", code, re.S)
    if len(users) < 3:
        problems.append("found %d of the 3 callers of -_getForIndex:"
                        % len(users))
    for u in users:
        if not re.search(r"if\s*\(\s*!pv\s*\)", u):
            problems.append("a caller of -_getForIndex: writes through its "
                            "result without checking it for NULL")
    return problems


def main():
    failed = 0
    total  = 0
    for name in DRIVERS + ["metal.m"]:
        path = os.path.join(DIR, name)
        if not os.path.isfile(path):
            print("[FAIL] %s: no such driver" % name)
            failed += 1
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            code = strip_comments(f.read())
        total += 1
        problems = check_metal(code) if name == "metal.m" \
              else check_c(name, code)
        if problems:
            failed += 1
            for p in problems:
                print("[FAIL] %s: %s" % (name, p))
        else:
            print("[pass] %s" % name)
    if failed:
        print("FAIL overlay_setter_bounds (%d of %d drivers)" % (failed, total))
        return 1
    print("PASS overlay_setter_bounds (%d drivers)" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
