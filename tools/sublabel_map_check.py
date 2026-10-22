#!/usr/bin/env python3
"""Check menu_cbs_sublabel.c's sublabel_map[] is sorted by label value.

The sublabel binder finds a row by binary search, so a row out of order
is simply never found: the entry shows no sublabel and nothing fails to
build. Removing rows keeps a sorted table sorted, so every #if is taken
as true and the full table is checked once; the enum values come from
compiling msg_hash.h with the macros the guarded labels need.

Usage:
  tools/sublabel_map_check.py [--root DIR]
  tools/sublabel_map_check.py --selftest
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

SRC = os.path.join('menu', 'cbs', 'menu_cbs_sublabel.c')
ROW = re.compile(r'\{\s*(MENU_ENUM_LABEL_\w+)\s*,\s*(MENU_ENUM_SUBLABEL_\w+)\s*\}\s*,?\s*$')

# every label any guarded row names has to exist in the probe
DEFINES = [
    'RARCH_INTERNAL', 'HAVE_MENU', 'HAVE_CONFIGFILE', 'HAVE_NETWORKING',
    'HAVE_SSL', 'HAVE_NFSCLIENT', 'HAVE_SMBCLIENT', 'HAVE_MCP',
    'HAVE_KEYCHAIN', 'HAVE_CRYPTO', 'HAVE_VIDEO_FILTER', 'HAVE_WINRAWINPUT',
    '_WIN32',
]


def extract(text):
    """Return [(line, label)] for sublabel_map[], or raise ValueError."""
    lines = text.split('\n')
    start = None
    for i, l in enumerate(lines):
        if 'sublabel_map[] = {' in l:
            start = i + 1
            break
    if start is None:
        raise ValueError('sublabel_map[] not found')
    rows = []
    for i in range(start, len(lines)):
        t = lines[i].strip()
        if t == '};':
            return rows
        if not t or t.startswith('#'):
            continue
        m = ROW.match(t)
        if not m:
            raise ValueError('line %d: unreadable row: %s' % (i + 1, t))
        rows.append((i + 1, m.group(1)))
    raise ValueError('sublabel_map[] has no end')


def values(root, labels, cc):
    """Compile a probe printing each label's enum value."""
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, 'probe.c')
        exe = os.path.join(tmp, 'probe')
        with open(c, 'w') as f:
            f.write('#include <stdio.h>\n#include "msg_hash.h"\n')
            f.write('static const int v[] = {\n')
            for l in labels:
                f.write('   (int)%s,\n' % l)
            f.write('};\nint main(void)\n{\n   size_t i;\n'
                    '   for (i = 0; i < sizeof(v) / sizeof(v[0]); i++)\n'
                    '      printf("%d\\n", v[i]);\n   return 0;\n}\n')
        cmd = [cc] + ['-D' + d for d in DEFINES] + [
            '-I' + root, '-I' + os.path.join(root, 'libretro-common', 'include'),
            c, '-o', exe]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            sys.stderr.write(r.stderr)
            raise ValueError('probe did not build; a guarded label may need '
                             'its macro added to DEFINES')
        out = subprocess.run([exe], capture_output=True, text=True, check=True)
        return [int(x) for x in out.stdout.split()]


def check(rows, vals, name):
    bad = 0
    for k in range(1, len(rows)):
        if vals[k] <= vals[k - 1]:
            sys.stderr.write('%s:%d: %s (%d) is not after %s (%d)\n' % (
                name, rows[k][0], rows[k][1], vals[k],
                rows[k - 1][1], vals[k - 1]))
            bad += 1
    return bad


def selftest():
    fx = ('   static const sublabel_enum_map_t sublabel_map[] = {\n'
          '      { MENU_ENUM_LABEL_A, MENU_ENUM_SUBLABEL_A },\n'
          '#ifdef HAVE_X\n'
          '      { MENU_ENUM_LABEL_B, MENU_ENUM_SUBLABEL_B },\n'
          '#endif\n'
          '      { MENU_ENUM_LABEL_C, MENU_ENUM_SUBLABEL_C },\n'
          '   };\n')
    rows = extract(fx)
    fails = 0
    if [r[1] for r in rows] != ['MENU_ENUM_LABEL_A', 'MENU_ENUM_LABEL_B',
                                'MENU_ENUM_LABEL_C']:
        fails += 1
    if check(rows, [1, 2, 3], 'sorted') != 0:
        fails += 1
    sys.stderr.write('expected failures follow:\n')
    if check(rows, [1, 3, 2], 'swapped') != 1:
        fails += 1
    if check(rows, [1, 2, 2], 'duplicate') != 1:
        fails += 1
    try:
        extract(fx.replace('MENU_ENUM_SUBLABEL_C }', 'func_c }'))
        fails += 1
    except ValueError:
        pass
    print('selftest: %s' % ('FAIL' if fails else 'ok'))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    path = os.path.join(a.root, SRC)
    with open(path) as f:
        rows = extract(f.read())
    vals = values(a.root, [r[1] for r in rows], os.environ.get('CC', 'cc'))
    bad = check(rows, vals, SRC)
    if bad:
        sys.stderr.write('%d row(s) out of order\n' % bad)
        return 1
    print('%s: %d rows sorted' % (SRC, len(rows)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
