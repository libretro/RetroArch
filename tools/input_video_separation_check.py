#!/usr/bin/env python3
"""Video and input code stay out of each other's state.

The video drivers used to make the input driver, and some input
drivers held the video driver's window or reached into its state. The
two are being taken apart (the input layer plan, Phase 2): the
frontend starts the input driver, told only what kind of window there
is, and neither side reads the other's state.

Three things are counted, textually, so that no machine has to compile
every platform's files:

1. gfx/ calls input_state_get_ptr().            Must be none.
2. input/ calls video_state_get_ptr().          Must be none.
3. gfx/ names an input driver or a joypad driver (input_x, input_udev,
   dinput_joypad, ...), as declared in input/input_driver.h.
                                                Must be none.

All three are at none. The two lists below are what was left to move
while that was being done, file by file with a count, and they are
empty now: a file that is not listed fails, so nothing goes back in.

Usage:
   tools/input_video_separation_check.py [--root DIR] [--selftest]

Exit status is 0 when the tree matches, 1 otherwise.
"""

import os
import re
import sys

SOURCE_EXT = (".c", ".h", ".m", ".mm", ".cpp")

# input/ files that still take the video state (2): none. The last
# was the Android input driver, which handles the app's lifecycle
# commands; it asks the video driver by name for what it needs
# (video_context_surface_create() and its neighbours).
VIDEO_STATE_IN_INPUT = {
}

# gfx/ files that still name an input or a joypad driver (3): none.
# Every video driver and context leaves the input driver to the
# frontend (input_driver_left_to_frontend()), and nothing goes back in
# here.
INPUT_DRIVERS_IN_GFX = {
}


def source_files(root, sub):
    for base, _dirs, files in os.walk(os.path.join(root, sub)):
        for name in files:
            if name.endswith(SOURCE_EXT):
                path = os.path.join(base, name)
                yield os.path.relpath(path, root).replace(os.sep, "/"), path


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def strip_comments(text):
    """Comments name drivers freely; only code counts."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def driver_names(root):
    """The input and joypad driver objects input/input_driver.h declares."""
    text = read(os.path.join(root, "input", "input_driver.h"))
    names = re.findall(
        r"^extern\s+input_(?:device_)?driver_t\s+([A-Za-z0-9_]+)\s*;",
        text, flags=re.M)
    return sorted(set(names))


def count_calls(root, sub, symbol):
    found = {}
    pat = re.compile(r"\b%s\s*\(" % re.escape(symbol))
    for rel, path in source_files(root, sub):
        n = len(pat.findall(strip_comments(read(path))))
        if n:
            found[rel] = n
    return found


def count_names(root, sub, names):
    found = {}
    if not names:
        return found
    pat = re.compile(r"\b(?:%s)\b" % "|".join(re.escape(n) for n in names))
    for rel, path in source_files(root, sub):
        n = len(pat.findall(strip_comments(read(path))))
        if n:
            found[rel] = n
    return found


def compare(what, found, allowed, errors):
    for rel in sorted(set(found) | set(allowed)):
        have = found.get(rel, 0)
        may = allowed.get(rel, 0)
        if have > may:
            errors.append("%s: %s has %d, %s" % (
                what, rel, have,
                ("%d listed" % may) if may else "and is not listed"))
        elif have < may:
            errors.append("%s: %s has %d but %d are listed: bring the"
                          " listing in tools/input_video_separation_check.py"
                          " down to match" % (what, rel, have, may))


def check(root):
    errors = []
    names = driver_names(root)
    if len(names) < 10:
        errors.append("input/input_driver.h: only %d driver objects found;"
                      " the declarations this check reads have changed"
                      % len(names))
    compare("gfx/ calls input_state_get_ptr()",
            count_calls(root, "gfx", "input_state_get_ptr"), {}, errors)
    compare("input/ calls video_state_get_ptr()",
            count_calls(root, "input", "video_state_get_ptr"),
            VIDEO_STATE_IN_INPUT, errors)
    compare("gfx/ names an input or joypad driver",
            count_names(root, "gfx", names), INPUT_DRIVERS_IN_GFX, errors)
    return errors


def selftest():
    import shutil
    import tempfile
    tmp = tempfile.mkdtemp(prefix="ivsep_")
    failures = []
    try:
        os.makedirs(os.path.join(tmp, "gfx", "drivers"))
        os.makedirs(os.path.join(tmp, "input", "drivers"))
        with open(os.path.join(tmp, "input", "input_driver.h"), "w") as f:
            for i in range(10):
                f.write("extern input_driver_t input_n%d;\n" % i)
            f.write("extern input_driver_t input_x;\n")
            f.write("extern input_device_driver_t dinput_joypad;\n")

        def put(rel, text):
            with open(os.path.join(tmp, rel), "w") as f:
                f.write(text)

        global VIDEO_STATE_IN_INPUT, INPUT_DRIVERS_IN_GFX
        keep = (VIDEO_STATE_IN_INPUT, INPUT_DRIVERS_IN_GFX)
        VIDEO_STATE_IN_INPUT = {"input/drivers/a_input.c": 1}
        INPUT_DRIVERS_IN_GFX = {"gfx/drivers/listed_gfx.c": 2}

        put("gfx/drivers/clean_gfx.c",
            "/* input_x is only named here, in a comment */\nint a;\n")
        put("gfx/drivers/listed_gfx.c",
            "void f(void) { g(&input_x); h(&dinput_joypad); }\n")
        put("input/drivers/a_input.c",
            "void f(void) { video_state_get_ptr(); }\n")
        if check(tmp):
            failures.append("a tree that matches its listing fails: %r"
                            % check(tmp))

        put("gfx/drivers/new_gfx.c", "void f(void) { g(&input_x); }\n")
        if not any("new_gfx.c" in e for e in check(tmp)):
            failures.append("a new file naming an input driver passes")
        os.remove(os.path.join(tmp, "gfx/drivers/new_gfx.c"))

        put("gfx/drivers/listed_gfx.c", "void f(void) { g(&input_x); }\n")
        if not any("bring the listing" in e for e in check(tmp)):
            failures.append("a listing higher than the code passes")
        put("gfx/drivers/listed_gfx.c",
            "void f(void) { g(&input_x); h(&dinput_joypad); }\n")

        put("gfx/drivers/clean_gfx.c",
            "void f(void) { input_state_get_ptr(); }\n")
        if not any("input_state_get_ptr" in e for e in check(tmp)):
            failures.append("gfx/ calling input_state_get_ptr() passes")
        put("gfx/drivers/clean_gfx.c", "int a;\n")

        put("input/drivers/b_input.c",
            "void f(void) { video_state_get_ptr(); }\n")
        if not any("b_input.c" in e for e in check(tmp)):
            failures.append("a new input file taking the video state passes")

        VIDEO_STATE_IN_INPUT, INPUT_DRIVERS_IN_GFX = keep
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for f in failures:
        print("SELFTEST FAIL: %s" % f)
    if not failures:
        print("SELFTEST_OK")
    return 1 if failures else 0


def main(argv):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if "--selftest" in argv:
        return selftest()
    if "--root" in argv:
        root = argv[argv.index("--root") + 1]
    errors = check(root)
    for e in errors:
        print("FAIL %s" % e)
    if errors:
        print("FAIL input_video_separation_check: %d" % len(errors))
        return 1
    left = (sum(VIDEO_STATE_IN_INPUT.values()),
            len(INPUT_DRIVERS_IN_GFX))
    if left == (0, 0):
        print("PASS input_video_separation_check (gfx/ and input/ stay out"
              " of each other's state, and gfx/ names no input driver)")
    else:
        print("PASS input_video_separation_check (still listed: %d call(s)"
              " into the video state from input/, %d gfx/ file(s) naming an"
              " input driver)" % left)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
