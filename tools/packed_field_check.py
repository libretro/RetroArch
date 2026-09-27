#!/usr/bin/env python3
"""Find reads of an axis field that has been packed away.

Why this exists: when a struct's width/height pair becomes one dims
word, the compiler finds every stale reader it compiles. The ones it
does not compile it cannot find, and those are the Objective-C drivers
and anything behind a platform #ifdef. video_info_t's pair went that
way twice in one evening: sdl2_gfx.c and sdl3_gfx.c kept reading
vid->video.width, and metal.m kept packing _video.width and
_video.height back together - six errors on iOS and six more on tvOS,
from a field that no longer existed.

This reads the tree as text. It collects the struct types that carry a
packed word and no longer carry the halves, works out which variables
and which members of other structs have those types, and reports any
site that still reaches for a half by name.

Usage:  python3 tools/packed_field_check.py [path ...]
Exits non-zero when a stale read is found.
"""
import os, re, sys

PACKED = ('dims', 'pos')
HALVES = ('width', 'height', 'x', 'y', 'full_width', 'full_height')


def mask(t):
    o, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if c == '/' and t[i+1:i+2] == '*':
            j = t.find('*/', i + 2); j = n if j < 0 else j + 2
            o.append(''.join('\x01' if ch != '\n' else '\n' for ch in t[i:j])); i = j
        elif c == '/' and t[i+1:i+2] == '/':
            j = t.find('\n', i); j = n if j < 0 else j
            o.append('\x01' * (j - i)); i = j
        elif c in '"\'':
            q, j = c, i + 1
            while j < n and t[j] != q:
                j += 2 if t[j] == '\\' else 1
            j = min(j + 1, n)
            o.append(''.join('\x01' if ch != '\n' else '\n' for ch in t[i:j])); i = j
        else:
            o.append(c); i += 1
    return ''.join(o)


def close_brace(t, o):
    d = 0
    for j in range(o, len(t)):
        if t[j] == '{':
            d += 1
        elif t[j] == '}':
            d -= 1
            if d == 0:
                return j
    return -1


def sources(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for dp, dns, fns in os.walk(root):
            dns[:] = [d for d in dns
                      if d not in ('.git', 'deps', 'pkg', 'media')]
            for fn in fns:
                if fn.endswith(('.c', '.h', '.m')):
                    yield os.path.join(dp, fn)


def main():
    files = sorted(set(sources(sys.argv[1:] or ['.'])))
    texts = {}
    for p in files:
        try:
            texts[p] = mask(open(p, errors='replace').read())
        except OSError:
            pass

    # 1. the types that carry a packed word and none of the halves
    packed_types = set()
    for t in texts.values():
        for mo in re.finditer(r'\b(?:typedef\s+)?struct\s+(\w+)?\s*\{', t):
            c = close_brace(t, t.index('{', mo.start()))
            if c < 0:
                continue
            body = t[t.index('{', mo.start()) + 1:c]
            members = set(re.findall(r'(?:^|;)\s*[\w \t\*]+?(\w+)\s*(?:\[[^\]]*\])?\s*;',
                                     body, re.M))
            if not (members & set(PACKED)):
                continue
            if members & set(HALVES):
                continue
            names = set()
            if mo.group(1):
                names.add(mo.group(1))
            alias = re.match(r'\s*(\w+)\s*;', t[c + 1:])
            if alias:
                names.add(alias.group(1))
            packed_types |= names
    if not packed_types:
        print('no packed types found'); return 0

    # 2. the names that hold one: locals, parameters and members
    holders = set()
    for t in texts.values():
        for ty in packed_types:
            for mo in re.finditer(r'(?<!\w)%s\s+(\**)\s*(\w+)\s*[;,=\)\[]'
                                  % re.escape(ty), t):
                holders.add(mo.group(2))

    # 3. any half still read off one of them
    bad = 0
    for p in sorted(texts):
        t = texts[p]
        raw = open(p, errors='replace').read()
        for mo in re.finditer(r'(?<![\w>])(\w+)\s*(\.|->)\s*(\w+)(?!\w)', t):
            name, half = mo.group(1), mo.group(3)
            if half not in HALVES or name not in holders:
                continue
            line = t[:mo.start()].count('\n') + 1
            print('%s:%d: %s%s%s - %s no longer carries that half'
                  % (p, line, name, mo.group(2), half, name))
            bad += 1
    print('%d stale axis read(s)' % bad)
    return 1 if bad else 0


sys.exit(main())
