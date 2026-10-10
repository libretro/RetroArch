#!/usr/bin/env python3
"""Every sample keeps its objects in its own directory.

Dry-runs each sample Makefile (make -n -B) and fails on any object
written outside the sample, where the next sample to link that source
would take it whatever flags it was built with.

  sample_objdir_check.py [DIR...]
  sample_objdir_check.py --selftest
"""

import os
import shlex
import subprocess
import sys

ROOTS = ("samples", "libretro-common/samples")
OBJ_EXT = (".o", ".obj")
TARGETS = (None, "all", "check")


def outputs(line):
    """Object paths a compile line writes with -o."""
    try:
        argv = shlex.split(line)
    except ValueError:
        argv = line.split()
    out = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-o" and i + 1 < len(argv):
            out.append(argv[i + 1])
            i += 2
            continue
        if a.startswith("-o") and len(a) > 2 and not a.startswith("-on"):
            out.append(a[2:])
        i += 1
    return [p for p in out if p.endswith(OBJ_EXT)]


def outside(sample_dir, path):
    base = os.path.realpath(sample_dir)
    full = os.path.realpath(os.path.join(sample_dir, path))
    return os.path.commonpath([base, full]) != base


def stray(sample_dir, text):
    """Objects in a dry run's output that land outside sample_dir."""
    bad = []
    for line in text.splitlines():
        for p in outputs(line):
            if outside(sample_dir, p) and p not in bad:
                bad.append(p)
    return bad


def dry_run(sample_dir):
    """What the default goal, all and check would build; None when
    make can dry-run none of them."""
    text = None
    for target in TARGETS:
        cmd = ["make", "-n", "-B", "-C", sample_dir]
        if target:
            cmd.append(target)
        r = subprocess.run(cmd, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL,
                           universal_newlines=True, timeout=120)
        if r.returncode == 0:
            text = (text or "") + r.stdout
    return text


def samples(top):
    """Sample directories; the Makefile at a root runs the samples
    through $(MAKE), which make -n would run for real, and builds
    nothing of its own."""
    found = []
    for root in ROOTS:
        base = os.path.join(top, root)
        for d, _, files in os.walk(base):
            if "Makefile" in files and os.path.normpath(d) != os.path.normpath(base):
                found.append(d)
    return sorted(found)


def selftest():
    d = os.path.join("samples", "x", "y")
    cases = [
        ("cc -c -o a.o a.c", []),
        ("cc -c -oobj/a.o a.c", []),
        ("cc -c -o obj/__/__/__/f/b.o ../../../f/b.c", []),
        ("cc -c -o ../../../f/b.o ../../../f/b.c", ["../../../f/b.o"]),
        ("cc -c -o../../../f/b.o ../../../f/b.c", ["../../../f/b.o"]),
        ("cc -o test a.o ../../../f/b.o", []),
        ("x86_64-w64-mingw32-gcc -c -o ../z.obj z.c", ["../z.obj"]),
    ]
    fails = 0
    for line, want in cases:
        got = stray(d, line)
        if got != want:
            print("selftest: %r gave %r, want %r" % (line, got, want))
            fails += 1
    print("selftest: %d case(s), %d failure(s)" % (len(cases), fails))
    return 1 if fails else 0


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    dirs = argv[1:] or samples(".")
    bad = 0
    for d in dirs:
        text = dry_run(d)
        if text is None:
            bad += 1
            print("%s: make cannot dry-run it, so it cannot be checked" % d)
            continue
        objs = stray(d, text)
        if objs:
            bad += 1
            print("%s: %d object(s) outside the sample, e.g. %s"
                  % (d, len(objs), objs[0]))
    print("%d of %d sample(s) build objects outside their directory"
          % (bad, len(dirs)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
