#!/usr/bin/env python3
"""Autoreleased objects stored past the pool, in the MRC-built Apple sources.

Every Objective-C file the Apple build compiles is built twice, two
different ways:

  - the qb/top-level Makefile builds each .m on its own, MRC, except
    the four given -fobjc-arc at Makefile:275;
  - Xcode builds griffin_objc.m, which #includes most of them, as one
    translation unit with CLANG_ENABLE_OBJC_ARC=YES.

So the same line is manual-retain-release in one build and ARC in the
other, and a convenience constructor - +dictionaryWithCapacity:,
+stringWithFormat:, anything not alloc/new/copy/retain - returns an
autoreleased object that ARC will retain into a strong slot and MRC
will not. Store one in a static, an ivar or a C struct and the Xcode
build is correct while the Makefile build reads freed memory once the
pool drains.

That is not a theoretical shape: ui_cocoatouch.m's app-icon texture
cache was +dictionaryWithCapacity: in a static, correct under Xcode and
a use-after-free under the Makefile, for as long as anyone had opened
that menu.

This finds assignments of a class-side message that is not one of the
owning ones into a slot that outlives the current autorelease pool.
It is a lint, not a prover: a permanent singleton assigned to a struct
field is reported and is fine. Read each hit.

Usage: tools/apple_mrc_ownership_check.py [--list]
Exit status is 0 always; this reports, it does not gate.
"""
import re, subprocess

arc = {"gfx/drivers/metal.m", "input/drivers_joypad/mfi_joypad.m",
       "input/drivers/cocoa_input.m", "location/drivers/corelocation.m"}
files = [f for f in subprocess.check_output(
    "grep -oE '\"\\.\\./[^\"]+\\.m\"' griffin/griffin_objc.m | tr -d '\"' | sed 's|\\.\\./||' | sort -u",
    shell=True, text=True).split() if f not in arc]

# +1 on the right-hand side: safe to store anywhere.
OWNING = re.compile(r"\balloc\]|\bnew\]|\bretain\]|\bcopy\]|\bmutableCopy\]"
                    r"|RARCH_RETAIN|\[\s*[A-Za-z_]\w*\s+new\s*\]")
# [SomeClass someMessage...] - a class-side message, i.e. a convenience
# constructor unless it is one of the owning ones above.
CONV = re.compile(r"=\s*\[\s*([A-Z][A-Za-z0-9_]*)\s+([a-zA-Z][A-Za-z0-9_]*)")
# static object pointers, declared anywhere (file or function scope).
STATIC = re.compile(r"^\s*static\s+(?:__\w+\s+)?([A-Z][A-Za-z0-9_]*"
                    r"(?:\s*<[^>]*>)?|id)\s*\*?\s*([A-Za-z_]\w*)\s*(?:=|;)")
# ivars: object-typed members inside a { } block after @interface/@implementation
IVAR = re.compile(r"^\s*(?:__\w+\s+)?([A-Z][A-Za-z0-9_]*(?:\s*<[^>]*>)?|id)\s*\*\s*"
                  r"([A-Za-z_]\w*)\s*;")
# object-typed fields of C structs
FIELD = re.compile(r"^\s{2,}([A-Z][A-Za-z0-9_]*(?:\s*<[^>]*>)?|id)\s*\*\s*([A-Za-z_]\w*)\s*;")

hits, checked = [], 0
for f in files:
    src = open(f, errors="replace").read().split("\n")
    longlived, in_iface = set(), False
    for l in src:
        t = l.strip()
        if t.startswith("@interface") or t.startswith("@implementation"):
            in_iface = True
        elif t.startswith("@end"):
            in_iface = False
        m = STATIC.match(l)
        if m:
            longlived.add(m.group(2))
        if in_iface:
            m = IVAR.match(l) or FIELD.match(l)
            if m:
                longlived.add(m.group(2))
        m = FIELD.match(l)
        if m:
            longlived.add(m.group(2))
    checked += len(longlived)
    for i, l in enumerate(src, 1):
        if "=" not in l or "==" in l or l.strip().startswith(("*", "/*", "//")):
            continue
        m = CONV.search(l)
        if not m or OWNING.search(l):
            continue
        # what is being assigned to
        lhs = l.split("=")[0]
        name = re.findall(r"([A-Za-z_]\w*)\s*$", lhs.strip())
        if not name:
            continue
        n = name[0]
        if n in longlived or lhs.strip().startswith("static ") or "->" in lhs or "_" == n[:1]:
            hits.append((f, i, n, "[%s %s...]" % m.groups(), l.strip()[:100]))

for f, i, dest, ctor, l in hits:
    print("%s:%d  %s = %s" % (f, i, dest, ctor))
    print("      %s" % l)
print("\n%d candidate(s); %d long-lived object slots examined across %d MRC files"
      % (len(hits), checked, len(files)))
print("Each is a slot outliving the pool assigned from a class-side message.")
print("Owning it (alloc/init, or retain) is the fix; a permanent singleton")
print("or a local that dies with the pool is a false positive - read them.")
