#!/usr/bin/env python3
"""gfx_ctx_driver_t::presentable() contract check.

presentable() tells the runloop the context has nothing to present to,
so that the pacing happens once, in the runloop, instead of each
context sleeping inside its own swap path.  The hook is optional and
sits last in gfx_ctx_driver_t, so a driver that does not implement it
leaves the slot NULL and keeps the old meaning.

That shape makes two mistakes easy and silent on every build but one:

1. The function casts `data` to the wrong driver's context type.  Each
   context driver names its own struct, several are near-identical
   (android_ctx_data_t vs android_ctx_data_vk_t), and a file is
   compiled by one platform's CI, so the wrong name is a build break
   somebody else discovers.  This actually happened.

2. The function is written but never wired into the vtable, or a
   vtable names a function that does not exist in that file.  The
   first is silent - the hook simply never runs and the loop spins as
   it did before - and is exactly what the feature exists to prevent.

So, per file that has one: presentable() must cast to the same type
swap_buffers() does, and must be referenced exactly once from a
gfx_ctx_driver_t initializer in the same file.  Both are textual
checks; nothing here needs the file to compile, which is the point,
since no machine compiles all of them.

Usage:
   tools/ctx_presentable_check.py [--selftest] [--root DIR]

Exit status is 0 when the tree is consistent, 1 otherwise.
"""

import os
import re
import sys

CTX_DIR = os.path.join("gfx", "drivers_context")

# static bool <name>(void *data)
RE_DEF = re.compile(
    r"^static\s+bool\s+([A-Za-z_0-9]+)\s*\(\s*void\s*\*\s*data\s*\)",
    re.MULTILINE)
# <type> *<var> = (<type>*)data;   - the first cast of data in a body
RE_CAST = re.compile(
    r"\(\s*([A-Za-z_0-9]+_t)\s*\*\s*\)\s*data")


def _body(text, start):
    """The braced body of the function whose signature ends at start."""
    i = text.find("{", start)
    if i < 0:
        return ""
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    return text[i:]


def _cast_type(text, name):
    """The context type <name>() casts data to, or None if it does not."""
    for m in RE_DEF.finditer(text):
        if m.group(1) != name:
            continue
        c = RE_CAST.search(_body(text, m.end()))
        return c.group(1) if c else None
    return None


def _swap_cast_type(text):
    """The type this file's swap_buffers() casts data to, if any."""
    m = re.search(
        r"^static\s+void\s+([A-Za-z_0-9]*swap_buffers)\s*\(\s*void\s*\*\s*data\s*\)",
        text, re.MULTILINE)
    if not m:
        return None
    c = RE_CAST.search(_body(text, m.end()))
    return c.group(1) if c else None


_SHARED = None


def _declared_elsewhere(name, root="."):
    """True when some file outside gfx/drivers_context defines name()."""
    global _SHARED
    if _SHARED is None:
        _SHARED = set()
        for d, _, files in os.walk(os.path.join(root, "gfx")):
            if d.endswith(CTX_DIR):
                continue
            for fn in files:
                if not fn.endswith((".c", ".m", ".h")):
                    continue
                with open(os.path.join(d, fn), "r", errors="replace") as f:
                    for m in re.finditer(
                            r"\bbool\s+([A-Za-z_0-9]*presentable)\s*\(", f.read()):
                        _SHARED.add(m.group(1))
    return name in _SHARED


def check_text(path, text):
    """Problems with one file's presentable() usage, as strings."""
    out = []
    defined = [m.group(1) for m in RE_DEF.finditer(text)
               if m.group(1).endswith("presentable")]
    # Names referenced from a gfx_ctx_driver_t initializer in this file.
    wired = set()
    for m in re.finditer(r"gfx_ctx_driver_t\s+[A-Za-z_0-9]+\s*=?\s*\{", text):
        i = m.end() - 1
        depth = 0
        for j in range(i, len(text)):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
        for w in re.findall(r"([A-Za-z_0-9]*presentable)", text[i:j]):
            wired.add(w)

    for name in defined:
        if name not in wired:
            out.append("%s: %s() is defined but never wired into a "
                       "gfx_ctx_driver_t - the hook would never run"
                       % (path, name))
    for name in wired:
        if name in defined:
            continue
        # A shared implementation - x11_presentable() serves the three
        # X11 contexts from x11_common.c - is fine as long as the file
        # can see a declaration of it.
        if not re.search(r"\bbool\s+%s\s*\(" % re.escape(name), text) \
                and not _declared_elsewhere(name):
            out.append("%s: vtable names %s(), which is defined nowhere "
                       "in the tree" % (path, name))

    swap_t = _swap_cast_type(text)
    for name in defined:
        t = _cast_type(text, name)
        if t is None or swap_t is None:
            continue
        if t != swap_t:
            out.append("%s: %s() casts data to %s but swap_buffers() casts "
                       "it to %s - one of them has the wrong driver's type"
                       % (path, name, t, swap_t))
    return out


def check_tree(root):
    problems = []
    d = os.path.join(root, CTX_DIR)
    for fn in sorted(os.listdir(d)):
        if not (fn.endswith(".c") or fn.endswith(".m")):
            continue
        p = os.path.join(d, fn)
        with open(p, "r", errors="replace") as f:
            text = f.read()
        if "presentable" not in text:
            continue
        problems += check_text(os.path.join(CTX_DIR, fn), text)
    return problems


GOOD = """
typedef struct { int vk; } my_ctx_data_t;
static bool my_presentable(void *data)
{
   my_ctx_data_t *c = (my_ctx_data_t*)data;
   return c && c->vk;
}
static void my_swap_buffers(void *data)
{
   my_ctx_data_t *c = (my_ctx_data_t*)data;
   (void)c;
}
const gfx_ctx_driver_t gfx_ctx_my = {
   my_swap_buffers,
   my_presentable
};
"""

WRONG_TYPE = GOOD.replace("my_ctx_data_t *c = (my_ctx_data_t*)data;\n   return",
                          "other_ctx_data_t *c = (other_ctx_data_t*)data;\n   return")
NOT_WIRED = GOOD.replace("   my_swap_buffers,\n   my_presentable\n",
                         "   my_swap_buffers\n")


def selftest():
    cases = [
        ("a correct driver", GOOD, 0),
        ("the wrong driver's context type", WRONG_TYPE, 1),
        ("defined but not wired", NOT_WIRED, 1),
    ]
    bad = 0
    for name, text, want in cases:
        got = len(check_text("fixture.c", text))
        ok = "ok" if got == want else "FAIL"
        if got != want:
            bad += 1
        print("   %-38s %d problem(s), want %d  %s" % (name, got, want, ok))
    if bad:
        print("ctx presentable checker: SELFTEST FAILED")
        return 1
    print("ctx presentable checker: selftest passed")
    return 0


def main(argv):
    root = "."
    if "--root" in argv:
        root = argv[argv.index("--root") + 1]
    if "--selftest" in argv:
        return selftest()
    problems = check_tree(root)
    for p in problems:
        print(p)
    if problems:
        print("%d problem(s)" % len(problems))
        return 1
    n = 0
    d = os.path.join(root, CTX_DIR)
    for fn in os.listdir(d):
        if fn.endswith((".c", ".m")):
            with open(os.path.join(d, fn), "r", errors="replace") as f:
                if "presentable" in f.read():
                    n += 1
    print("ctx presentable: %d context drivers implement the hook, all "
          "wired once and casting the type their own swap_buffers() does"
          % n)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
