#!/usr/bin/env python3
"""An input driver's table has no more entries than the structure has members.

Every input driver fills in an input_driver_t by position:

    input_driver_t input_uwp = {
       uwp_input_init,
       ...
       uwp_keys_down,
       uwp_bind_mouse_buttons
    };

When a member is added to struct input_driver (input/input_driver.h) and
a driver names it, the driver's table and the structure have to land
together. They did not, once: the UWP, udev and web drivers named
bind_mouse_buttons a commit before the structure had it. gcc and clang
say "excess elements in struct initializer" and carry on; MSVC says
C2078, too many initializers, and the UWP build stops - on a runner
most contributors do not have, for a file no Linux job compiles.

This is that check at source level, for every driver whatever SDK it
needs: the entries of each `input_driver_t NAME = { ... };` are counted,
down each side of any #if / #else in the table, and none may come to
more than the structure's members. Fewer is fine - the members a table
leaves out are NULL.

    python3 tools/input_driver_table_check.py [--root DIR] [--selftest]
"""

import argparse
import os
import re
import sys
import tempfile

HEADER = 'input/input_driver.h'
DIRS = ('input/drivers',)
TABLE = re.compile(r'^input_driver_t\s+(\w+)\s*=\s*\{', re.M)


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def members(root):
    text = strip_comments(open(os.path.join(root, HEADER), encoding='utf-8', errors='replace').read())
    m = re.search(r'^struct input_driver\s*\{', text, re.M)
    if not m:
        return None
    depth, i = 1, m.end()
    while depth and i < len(text):
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        i += 1
    body = text[m.end():i - 1]
    # a member is a declaration ending in ';' at the structure's own depth
    count, depth = 0, 0
    for ch in body:
        if ch in '({':
            depth += 1
        elif ch in ')}':
            depth -= 1
        elif ch == ';' and depth == 0:
            count += 1
    return count


def count_entries(lines):
    """(fewest, most) entries over every way through the #if branches."""
    def block(i):
        lo = hi = 0
        text = ''
        while i < len(lines):
            line = lines[i].strip()
            if line.startswith(('#if', '#ifdef', '#ifndef')):
                branches = []
                i, (blo, bhi) = (lambda r: (r[0], (r[1], r[2])))(block(i + 1))
                branches.append((blo, bhi))
                saw_else = False
                while lines[i].strip().startswith(('#elif', '#else')):
                    saw_else = saw_else or lines[i].strip().startswith('#else')
                    i, (blo, bhi) = (lambda r: (r[0], (r[1], r[2])))(block(i + 1))
                    branches.append((blo, bhi))
                if not saw_else:
                    branches.append((0, 0))
                lo += min(b[0] for b in branches)
                hi += max(b[1] for b in branches)
                i += 1          # the #endif
                continue
            if line.startswith(('#elif', '#else', '#endif')):
                break
            if not line.startswith('#'):
                text += ' ' + lines[i]
            i += 1
            # entries seen so far in plain text are counted when it ends
            n = len([e for e in text.split(',') if e.strip()])
            if i >= len(lines) or lines[i].strip().startswith('#'):
                lo += n
                hi += n
                text = ''
        return i, lo, hi
    _, lo, hi = block(0)
    return lo, hi


def tables(root):
    found = []
    for d in DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for f in sorted(os.listdir(base)):
            if not f.endswith(('.c', '.m', '.mm', '.cpp')):
                continue
            path = os.path.join(base, f)
            text = strip_comments(open(path, encoding='utf-8', errors='replace').read())
            for m in TABLE.finditer(text):
                end = text.find('};', m.end())
                if end < 0:
                    continue
                lo, hi = count_entries(text[m.end():end].split('\n'))
                line = text.count('\n', 0, m.start()) + 1
                found.append((os.path.relpath(path, root).replace(os.sep, '/'), line, m.group(1), lo, hi))
    return found


def run(root):
    n = members(root)
    if not n:
        print('error: struct input_driver was not found in %s; nothing was checked' % HEADER)
        return 2
    found = tables(root)
    if not found:
        print('error: no input_driver_t table was found; nothing was checked')
        return 2
    bad = 0
    for rel, line, name, lo, hi in found:
        if hi > n:
            print('%s:%d: %s has %d entries, and struct input_driver has %d members.\n'
                  '  MSVC stops at this (C2078, too many initializers). The member a new entry\n'
                  '  is for has to be in %s in the same commit.' % (rel, line, name, hi, n, HEADER))
            bad += 1
    if bad:
        print('FAIL input_driver_table_check: %d table(s)' % bad)
        return 1
    print('PASS input_driver_table_check (%d tables, none past the structure\'s %d members; '
          'the fullest has %d)' % (len(found), n, max(f[4] for f in found)))
    return 0


def selftest():
    bad = 0
    with tempfile.TemporaryDirectory() as root:
        os.makedirs(os.path.join(root, 'input', 'drivers'))
        with open(os.path.join(root, HEADER), 'w') as f:
            f.write('struct input_driver\n{\n   void *(*init)(const char *j);\n'
                    '   /* one; two */\n   void (*poll)(void *data);\n'
                    '   int16_t (*state)(void *data,\n         unsigned port);\n   const char *ident;\n};\n')
        def table(body):
            with open(os.path.join(root, 'input', 'drivers', 't_input.c'), 'w') as f:
                f.write('input_driver_t input_t = {\n' + body + '\n};\n')
        import io
        import contextlib
        cases = [('   a, b, c, "t"', 0, 'as many entries as members'),
                 ('   a, b', 0, 'fewer entries than members'),
                 ('   a, b, c, "t", e', 1, 'one entry too many'),
                 ('   a,\n#ifdef X\n   b,\n#else\n   NULL, /* b, c */\n#endif\n   c, "t"', 0, 'an #if with as many either way'),
                 ('   a, b, c,\n#ifdef X\n   "t", e\n#else\n   "t"\n#endif', 1, 'too many down one side of an #if'),
                 ('   a, b, c, "t"\n#ifdef X\n   , e\n#endif', 1, 'too many when an #if with no #else is taken')]
        for body, want, what in cases:
            table(body)
            with contextlib.redirect_stdout(io.StringIO()):
                got = run(root)
            if got != want:
                print('selftest: %s: wanted %d, got %d' % (what, want, got))
                bad += 1
        if members(root) != 4:
            print('selftest: the structure\'s members were counted as %r, not 4' % members(root))
            bad += 1
    if bad:
        print('FAIL input_driver_table_check --selftest')
        return 1
    print('PASS input_driver_table_check --selftest (%d cases)' % (len(cases) + 1))
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()
    sys.exit(selftest() if args.selftest else run(args.root))
