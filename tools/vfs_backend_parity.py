#!/usr/bin/env python3
"""Check that every VFS backend defines the same retro_vfs_*_impl set.

vfs_implementation_uwp.cpp does not extend vfs_implementation.c, it
replaces it: the UWP project compiles the .cpp and not the .c.  So a
function added to the C backend and called from file_stream.c - which
both builds share - compiles fine everywhere and fails to link on UWP
only, at the end of a long build, on a runner most contributors do not
have.  That is how retro_vfs_file_get_mapped_ptr_impl shipped broken.

This compares the two backends by symbol and fails on any function the
C backend defines and the UWP backend does not.  It is a source-level
check, so it costs nothing and runs on Linux.

A backend that genuinely cannot support a call still has to say so -
returning a documented "not available" value, the way the UWP mapping
accessor returns NULL - because a missing definition is a link error,
not a fallback.
"""
import re
import sys

C_BACKEND   = 'libretro-common/vfs/vfs_implementation.c'
UWP_BACKEND = 'libretro-common/vfs/vfs_implementation_uwp.cpp'

# Definitions only: a return type (possibly with const/*/ws) then the
# name then '(' at the start of a line.  Calls are indented, so
# anchoring to column 0 keeps them out.
DEF = re.compile(
    r'^(?:const\s+|static\s+|struct\s+)*[\w\*\s]*?\b(retro_vfs_\w+_impl)\s*\(',
    re.M)


def defined_in(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return set(DEF.findall(f.read()))


def main():
    c   = defined_in(C_BACKEND)
    uwp = defined_in(UWP_BACKEND)

    if not c or not uwp:
        print('error: parsed no symbols from a backend; the regex or the '
              'file layout has drifted', file=sys.stderr)
        return 2

    missing = sorted(c - uwp)
    if missing:
        print('error: defined in %s but missing from %s:' %
              (C_BACKEND, UWP_BACKEND), file=sys.stderr)
        for m in missing:
            print('   %s' % m, file=sys.stderr)
        print('\nAdd each to the UWP backend.  If it cannot be supported '
              'there, define it anyway and return the documented '
              '"unavailable" value - a missing definition is a link '
              'error, not a fallback.', file=sys.stderr)
        return 1

    print('vfs backend parity: %d symbols, both backends agree' % len(c))
    return 0


if __name__ == '__main__':
    sys.exit(main())
