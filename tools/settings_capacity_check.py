#!/usr/bin/env python3
"""Checks that every settings list in configuration.c has room for what
it is given.

configuration.c builds its lists of settings - booleans, integers,
strings, paths and so on - in populate_settings_*() functions, each
into an array of a fixed size, SETTINGS_*_COUNT_MAX. Adding a setting
is a line in one of those functions, or a row in a table one of them
includes; nothing made anyone raise the size, and one setting too many
wrote past the end of the array. That happened: two per-port string
settings (the keyboard and the mouse a port is pinned to, sixteen of
each) overflowed the string list, and RetroArch died of heap
corruption at startup.

The lists now refuse what they have no room for, and say so. This
finds it before anything is run. configuration.c is preprocessed, as
tools/settings_check.py does it and for the same build profiles, so
that every table and macro is expanded; then, for each list, the
settings its function adds are counted - one for each place a
setting's name is stored, times the loop's count where that is inside
a loop over the users - and compared with the size the list is
allocated with.

    python3 tools/settings_capacity_check.py

Exits 1 when a list is given more than it holds.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import settings_check  # noqa: E402  (the preprocessing, and the profiles)

KINDS = ('array', 'path', 'bool', 'float', 'uint', 'size', 'int')
ADD = re.compile(r'tmp\s*\[\s*count\s*\]\s*\.\s*ident\s*=')
# a loop over the users, once MAX_USERS has been expanded
LOOP = re.compile(r'\bfor\s*\([^;]*;[^;<]*<\s*\(?\s*(\d+)\s*\)?\s*;[^)]*\)')
ALLOC = r'calloc\s*\(([^,]+),\s*sizeof\s*\(\s*struct\s+config_%s_setting\s*\)\s*\)'


def count_adds(body):
    total = 0
    pos = 0
    while True:
        loop = LOOP.search(body, pos)
        end = loop.start() if loop else len(body)
        total += len(ADD.findall(body, pos, end))
        if not loop:
            return total
        # the loop's body: a block, or one statement
        i = loop.end()
        while body[i].isspace():
            i += 1
        if body[i] == '{':
            depth, j = 0, i
            while True:
                if body[j] == '{':
                    depth += 1
                elif body[j] == '}':
                    depth -= 1
                    if not depth:
                        break
                j += 1
            inner, pos = body[i + 1:j], j + 1
        else:
            j = body.index(';', i)
            inner, pos = body[i:j + 1], j + 1
        total += int(loop.group(1)) * count_adds(inner)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bad = 0

    for profile in sorted(settings_check.PROFILES):
        text = settings_check.preprocess(root, settings_check.PROFILES[profile])
        for kind in KINDS:
            start = text.find('*populate_settings_%s(' % kind)
            end = text.find('*size = count;', start)
            alloc = re.search(ALLOC % kind, text[start:end]) if start >= 0 else None
            if start < 0 or end < 0 or not alloc:
                print('FAIL [%s] %s list: populate_settings_%s() or its'
                      ' allocation not found' % (profile, kind, kind))
                bad += 1
                continue
            expr = alloc.group(1).strip()
            if not re.fullmatch(r'[0-9+*() ]+', expr):
                print('FAIL [%s] %s list: cannot read its size: %s'
                      % (profile, kind, expr))
                bad += 1
                continue
            cap = int(eval(expr))
            need = count_adds(text[start:end])
            if need > cap:
                print('FAIL [%s] %-5s list: given %d settings, holds %d:'
                      ' raise SETTINGS_%s_COUNT_MAX in configuration.c'
                      % (profile, kind, need, cap, kind.upper()))
                bad += 1
            else:
                print('ok   [%s] %-5s list: given %3d settings, holds %3d'
                      % (profile, kind, need, cap))
    if bad:
        print('FAIL settings_capacity_check: %d' % bad)
        return 1
    print('PASS settings_capacity_check')
    return 0


if __name__ == '__main__':
    sys.exit(main())
