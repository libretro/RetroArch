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

Covers every driver with an overlay interface (DRIVERS below, and
metal.m); a driver in gfx/drivers that defines the setters and is not
in the table fails, so a new one is read from its first commit.

Every driver in gfx/drivers is also read for the alpha set_alpha hands
it being packed into a byte unsaturated. The frontend's alpha is the
page opacity times a desc's alpha_mod, and packs set alpha_mod above 1
to brighten a pressed button: "mod * 0xFF" packed as a byte wraps
(1.4 comes out as 0x65, a pressed button drawn dimmer than a released
one), and cast straight to u8 it is undefined. The conversion goes
through VIDEO_ALPHA_BYTE(), which saturates.

D3D10, D3D11 and D3D12 keep a page's sprites in a copy the setters
write and the draw uploads once, when something changed: a setter that
maps the sprite buffer is a map per image per call - a page load is
three of them per image - and writes a buffer the last frame may still
be drawing from. Their setters, and the helper they go through, must
not map.

D3D8 and D3D9 (HLSL) give each overlay image a vertex buffer of its
own, filled by the image's draw: that draw locks the buffer only when
the quad differs from the one it last wrote (overlay_t::vert_sent), or
a page costs a lock per image per frame for quads that never change.

Exit status 0 when every setter passes, 1 otherwise.
"""

import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DIR  = os.path.join(ROOT, "gfx", "drivers")

DRIVERS = [
    "ctr_gfx.c", "d3d8.c", "d3d9cg.c", "d3d9hlsl.c", "d3d10.c", "d3d11.c",
    "d3d12.c", "gdi_gfx.c", "gl1.c", "gl2.c", "gl3.c", "gx_gfx.c",
    "gx2_gfx.c", "gxm_gfx.c", "hub75_gfx.c", "rsx_gfx.c", "sdl2_gfx.c",
    "switch_nx_gfx.c", "vulkan.c",
]

SETTER = r"\b(\w+_overlay_(?:set_alpha|vertex_geom|tex_geom))\s*\("
# "index >= count", "image >= gl->overlays", "index < d3d->overlays_size"
BOUND  = r"\b(?:index|image)\b\s*(?:>=|<)\s*[\w>.\-]+"
HELPER = r"\b(\w+_overlay_sprite(?:_map)?)\s*\("


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


STORE_ONLY = ("d3d10.c", "d3d11.c", "d3d12.c")
MAP        = r"\b(?:D3D12Map|Map)\s*\("


LOCK_ONCE = {"d3d8.c":     r"\bd3d8_overlay_render\s*\(",
             "d3d9hlsl.c": r"\bd3d9_hlsl_overlay_render\s*\("}
LOCK      = r"(?:_Lock|vertex_buffer_lock)\s*\("


def check_lock_once(name, code):
    problems = []
    bodies   = [b for b in (body_of(code, m.end() - 1)
                            for m in re.finditer(LOCK_ONCE[name], code))
                if b is not None]
    if len(bodies) != 1:
        return ["found %d definitions of the overlay draw - the check is "
                "not reading this driver right" % len(bodies)]
    body  = bodies[0]
    locks = [m.start() for m in re.finditer(LOCK, body)]
    guard = body.find("memcmp(overlay->vert_sent")
    if not locks:
        problems.append("the overlay draw locks no vertex buffer - the "
                        "check is not reading this driver right")
    elif guard < 0 or any(l < guard for l in locks):
        problems.append("the overlay draw locks its vertex buffer without "
                        "first comparing the quad with the one it last "
                        "wrote (vert_sent)")
    return problems


def check_c(name, code):
    problems = []
    setters  = functions(code, SETTER)
    helpers  = functions(code, HELPER)
    if len(setters) < 3:
        problems.append("found %d of the 3 setters - the check is not "
                        "reading this driver right" % len(setters))
    for fn in sorted(setters):
        body = setters[fn]
        # A driver with nothing to set (a stub) writes nothing.
        if not body.strip("{} \t\r\n"):
            continue
        if re.search(BOUND, body):
            continue
        called = [h for h in helpers if re.search(r"\b%s\s*\(" % h, body)]
        if any(re.search(BOUND, helpers[h]) for h in called):
            continue
        problems.append("%s() writes image [index] without comparing the "
                        "index to the page's count" % fn)
    if name in STORE_ONLY:
        for fn in sorted(setters):
            body   = setters[fn]
            called = [h for h in helpers if re.search(r"\b%s\s*\(" % h, body)]
            if re.search(MAP, body) or any(re.search(MAP, helpers[h])
                                           for h in called):
                problems.append("%s() maps the sprite buffer: a setter "
                                "writes the page's copy, the draw uploads"
                                % fn)
    if name in LOCK_ONCE:
        problems += check_lock_once(name, code)
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


# "mod * 0xFF", "alpha_mod * 255.0f", "0xFF * x->alpha_mod"
UNSATURATED = re.compile(
    r"\b(?:mod|alpha_mod)\s*\*\s*(?:0x[fF]{2}|255(?:\.0*f?)?)\b"
    r"|\b(?:0x[fF]{2}|255(?:\.0*f?)?)\s*\*\s*[\w>.\-\[\]]*\balpha_mod\b")


def check_alpha_bytes():
    """Every overlay alpha packed as a byte goes through
    VIDEO_ALPHA_BYTE()."""
    problems = []
    for name in sorted(os.listdir(DIR)):
        if not name.endswith((".c", ".m")):
            continue
        with open(os.path.join(DIR, name), encoding="utf-8",
                  errors="replace") as f:
            # Comments blanked line for line, so a match keeps its line.
            code = re.sub(r"/\*.*?\*/",
                          lambda c: re.sub(r"[^\n]", " ", c.group(0)),
                          f.read(), flags=re.S)
        for m in UNSATURATED.finditer(code):
            line = code.count("\n", 0, m.start()) + 1
            problems.append("%s:%d: overlay alpha packed as a byte without "
                            "VIDEO_ALPHA_BYTE(): %s" % (name, line, m.group(0)))
    return problems


def main():
    failed = 0
    total  = 0
    for name in sorted(os.listdir(DIR)):
        if not name.endswith(".c") or name in DRIVERS:
            continue
        with open(os.path.join(DIR, name), encoding="utf-8",
                  errors="replace") as f:
            if functions(strip_comments(f.read()), SETTER):
                print("[FAIL] %s: defines overlay setters but is not in "
                      "DRIVERS" % name)
                failed += 1
    unsaturated = check_alpha_bytes()
    for p in unsaturated:
        print("[FAIL] " + p)
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
    if failed or unsaturated:
        print("FAIL overlay_setter_bounds (%d of %d drivers, %d unsaturated "
              "alpha bytes)" % (failed, total, len(unsaturated)))
        return 1
    print("PASS overlay_setter_bounds (%d drivers)" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
