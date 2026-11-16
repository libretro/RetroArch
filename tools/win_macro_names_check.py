#!/usr/bin/env python3
"""Identifiers the Windows SDK headers turn into types.

rpcndr.h, which windows.h, the D3D, DXGI and EGL headers and every
UWP build pull in, has

    #define small char
    #define hyper __int64

so a variable or parameter by either name stops compiling under MSVC
- and only in a translation unit that happens to include it, which in
a griffin build is all of them. GCC and Clang on the other platforms
never see it. This finds them before MSVC does: every C, C++ and
Objective-C file outside deps/ and the samples, with comments,
strings and preprocessor lines left out, and member names (a.small,
p->small) allowed, as the macros cannot reach those.

    python3 tools/win_macro_names_check.py            # the tree
    python3 tools/win_macro_names_check.py --selftest
"""
import re
import subprocess
import sys

NAMES = ("small", "hyper")
EXTS = ("*.c", "*.h", "*.cpp", "*.cc", "*.m", "*.mm")
# deps/ is not ours; the samples are built by GCC and Clang only
SKIP = ("deps/", "samples/", "libretro-common/samples/")

LITERAL = re.compile(
    r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
    re.S)
NAME = re.compile(r'(?<![\w.])(?<!->)(?<!->\s)\b(%s)\b' % "|".join(NAMES))


def blank(match):
    return re.sub(r'[^\n]', ' ', match.group(0))


def scan(text):
    """(line, name) for each use of a name in @text's code"""
    found = []
    code = LITERAL.sub(blank, text)
    continued = False
    for number, line in enumerate(code.split("\n"), 1):
        directive = continued or line.lstrip().startswith("#")
        continued = directive and line.rstrip().endswith("\\")
        if directive:
            continue
        for match in NAME.finditer(line):
            before = line[:match.start()].rstrip()
            if before.endswith(".") or before.endswith("->"):
                continue
            found.append((number, match.group(1)))
    return found


def selftest():
    bad = (
        "static bool f(unsigned small, int x)\n"
        "{\n"
        "   long long hyper = x;\n"
        "   unsigned w = SCALE_W(small);\n"
        "}\n")
    good = (
        "/* a small image, a hyper link */\n"
        "#define LABEL small\n"
        "static const char *s = \"small hyper\";\n"
        "x = b->small + a.small + c-> hyper;\n"
        "unsigned small_dims, smaller, hypertext;\n")
    got = [name for _, name in scan(bad)]
    if got != ["small", "hyper", "small"]:
        print("selftest: expected small, hyper, small; got %s" % got)
        return 1
    if scan(good):
        print("selftest: false positives %s" % scan(good))
        return 1
    print("win_macro_names_check selftest: passed")
    return 0


def main():
    if "--selftest" in sys.argv[1:]:
        return selftest()
    files = subprocess.check_output(["git", "ls-files", "--"] + list(EXTS),
            text=True).split()
    failures = 0
    for path in files:
        if path.startswith(SKIP):
            continue
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        for number, name in scan(text):
            print("%s:%d: '%s' is a type under the Windows SDK (rpcndr.h);"
                  " give it another name" % (path, number, name))
            failures += 1
    if failures:
        print("%d identifier(s) the Windows headers redefine" % failures)
        return 1
    print("win_macro_names_check: %d files, none use %s"
          % (len(files), " or ".join(NAMES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
