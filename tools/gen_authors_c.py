#!/usr/bin/env python3
"""AUTHORS.h is a C++11 raw string (Qt reads it). The native companions
are C / Objective-C, so this writes AUTHORS_c.h: the same list as a
plain C string literal. Run after editing AUTHORS.h; CI checks the two
agree."""
import os, sys
root = os.path.join(os.path.dirname(__file__), '..')
src = open(os.path.join(root, 'AUTHORS.h'), encoding='utf-8').read()
start = src.index('R"(') + 3
end = src.rindex(')"')
body = src[start:end]
lines = body.split('\n')
out = ['/* Generated from AUTHORS.h by tools/gen_authors_c.py - do not edit. */',
       '#ifndef __RETROARCH_AUTHORS_C_H', '#define __RETROARCH_AUTHORS_C_H',
       'static const char *retroarch_contributors_list =']
for l in lines:
    esc = l.replace('\\', '\\\\').replace('"', '\\"')
    out.append('   "%s\\n"' % esc)
out.append('   ;')
out.append('#endif')
text = '\n'.join(out) + '\n'
dst = os.path.join(root, 'AUTHORS_c.h')
if len(sys.argv) > 1 and sys.argv[1] == '--check':
    cur = open(dst, encoding='utf-8').read() if os.path.exists(dst) else ''
    if cur != text:
        print('AUTHORS_c.h is out of date: run tools/gen_authors_c.py'); sys.exit(1)
    print('AUTHORS_c.h up to date'); sys.exit(0)
open(dst, 'w', encoding='utf-8').write(text)
print('wrote AUTHORS_c.h (%d lines)' % len(lines))
