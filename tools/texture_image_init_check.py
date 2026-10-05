#!/usr/bin/env python3
"""texture_image_init_check.py - a load is asked what it should load.

image_texture_load() and its siblings read the struct texture_image
they are handed: pix10 is the caller's request for a 10-bit decode,
read before the image is filled in. A struct declared on the stack and
filled field by field leaves that request as whatever the stack held,
and a 16-bit PNG then comes back as packed 10-bit words now and then -
taken for 8-bit texels by a caller that never asked, so a shader LUT
or a texture gets the wrong colours with nothing to show why.

The rule: a local or static struct texture_image passed by address to
an image_texture_load* call has its request set first - cleared with
memset, initialised to zero at its declaration, or given a pix10 of
its own - between the declaration and the call. Pointers to images
filled elsewhere are not checked; neither is deps/.
"""

import os
import re
import sys

EXEMPT_DIRS = ("deps/", ".git/")
EXTS = (".c", ".m", ".cpp", ".cc")

DECL = re.compile(r"\bstruct\s+texture_image\s+([A-Za-z_]\w*)\s*(=\s*\{[^;]*\})?\s*;")
LOAD = re.compile(r"\bimage_texture_load\w*\s*\(\s*&\s*([A-Za-z_]\w*)\b")


def problems(path, text):
    """(line, name) for every load of an image whose request was never set."""
    out = []
    lines = text.split("\n")
    for i, line in enumerate(lines):
        m = DECL.search(line)
        if not m or m.group(2):
            continue
        name = m.group(1)
        cleared = re.compile(
            r"\bmemset\s*\(\s*&\s*" + re.escape(name) + r"\b"
            r"|\b" + re.escape(name) + r"\s*\.\s*pix10\s*=")
        # Up to the end of the enclosing function: a line opening with
        # the closing brace of a top-level block.
        for j in range(i + 1, len(lines)):
            if lines[j].startswith("}"):
                break
            if cleared.search(lines[j]):
                break
            lm = LOAD.search(lines[j])
            if lm and lm.group(1) == name:
                out.append((j + 1, name))
                break
    return out


def scan(root):
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        rel = os.path.relpath(dirpath, root).replace(os.sep, "/") + "/"
        if rel.startswith(EXEMPT_DIRS):
            dirnames[:] = []
            continue
        for fn in filenames:
            if not fn.endswith(EXTS):
                continue
            path = os.path.join(dirpath, fn)
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    text = f.read()
            except OSError:
                continue
            if "texture_image" not in text:
                continue
            for line, name in problems(path, text):
                found.append("%s:%d: '%s' reaches image_texture_load with "
                             "its 10-bit request unset (memset it, or set "
                             "%s.pix10)" % (os.path.relpath(path, root)
                                            .replace(os.sep, "/"),
                                            line, name, name))
    return found


def selftest():
    bad = ("void f(void)\n{\n   struct texture_image img;\n"
           "   img.pixels = NULL;\n   img.supports_rgba = true;\n"
           "   image_texture_load(&img, path);\n}\n")
    good_memset = ("void f(void)\n{\n   struct texture_image img;\n"
                   "   memset(&img, 0, sizeof(img));\n"
                   "   image_texture_load(&img, path);\n}\n")
    good_field = ("void f(void)\n{\n   struct texture_image img;\n"
                  "   img.pix10 = false;\n"
                  "   image_texture_load_buffer(&img, t, b, n);\n}\n")
    good_init = ("void f(void)\n{\n   struct texture_image img = {0};\n"
                 "   image_texture_load(&img, path);\n}\n")
    other_fn = ("void f(void)\n{\n   struct texture_image img;\n}\n"
                "void g(struct texture_image *p)\n{\n"
                "   image_texture_load(p, path);\n}\n")
    ok = True
    if not problems("bad.c", bad):
        print("selftest: an image loaded with its request unset was not caught")
        ok = False
    for name, src in (("memset", good_memset), ("pix10", good_field),
                      ("initialiser", good_init), ("other function", other_fn)):
        if problems(name, src):
            print("selftest: a cleared image (%s) was flagged" % name)
            ok = False
    if ok:
        print("selftest: ok")
    return ok


def main():
    if "--selftest" in sys.argv[1:]:
        return 0 if selftest() else 1
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    found = scan(root)
    for f in found:
        print(f)
    if found:
        print("%d image(s) loaded with the 10-bit request unset" % len(found))
        return 1
    print("texture_image requests: every load is asked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
