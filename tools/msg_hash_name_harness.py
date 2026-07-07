#!/usr/bin/env python3
"""Name-keyed msg_hash lookup harness.

Usage, from the repository root:
    python3 tools/msg_hash_name_harness.py <output-dump-path>

Dumps every (enum NAME, language, resolved string) triple, sorted by
name then language, so two dumps are directly comparable even when the
enum has been renumbered. This is the comparison methodology for the
enum-region consolidation step of the single-source settings campaign:
moving MENU_LABEL rows into generated regions renumbers everything
after the insertion point, which invalidates any comparison keyed on
numeric enum values; a name-keyed dump is immune.

The tool textually parses the msg_hash_enums body - expanding the
MENU_LABEL and MENU_LBL_H helper macros and preserving preprocessor
guards - generates a name table TU plus a driver that escapes newlines
and backslashes so every dump row is exactly one line, compiles them
against msg_hash.c and the full language set, runs the dump, and
self-validates: the row count must equal names x languages and two
consecutive runs must be identical.
"""

import os
import re
import subprocess
import sys


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def parse_enum_names(msg_hash_h):
    text = open(msg_hash_h, encoding='utf-8').read()
    m = re.search(r'enum msg_hash_enums\n\{\n(.*?)\n\};', text, re.S)
    if not m:
        raise SystemExit('msg_hash_enums body not found')
    names = []
    stack = []
    for raw in m.group(1).split('\n'):
        s = raw.strip()
        if not s or s.startswith('/*') or s.startswith('*') or s.startswith('//'):
            continue
        if s.startswith('#if'):
            stack.append([s, False]); continue
        if s.startswith('#else'):
            if stack: stack[-1][1] = True
            continue
        if s.startswith('#elif'):
            raise SystemExit('#elif in enum body not supported')
        if s.startswith('#endif'):
            if stack: stack.pop()
            continue
        im = re.match(r'#include "(settings/settings_def_\w+\.h)"', s)
        if im:
            import os as _os
            guard = tuple((g[0], g[1]) for g in stack)
            dt = re.sub(r'/\*.*?\*/', '', open(im.group(1)).read(), flags=re.S)
            dstack = list(stack)
            dskip = []
            for dl in dt.split('\n'):
                ds = dl.strip()
                if 'SETTINGS_DEF_STRINGS_PASS' in ds and ds.startswith('#if'):
                    # the enum region defines the strings pass, so an
                    # alternated guard is always true there
                    dskip.append(True); continue
                if ds.startswith('#endif') and dskip and dskip[-1]:
                    dskip.pop(); continue
                if ds.startswith('#if'):
                    dskip.append(False)
                    dstack.append([ds, False]); continue
                if ds.startswith('#else'):
                    if len(dstack) > len(stack): dstack[-1][1] = True
                    continue
                if ds.startswith('#endif'):
                    if dskip: dskip.pop()
                    if len(dstack) > len(stack): dstack.pop()
                    continue
                dm = re.match(r'S_(BOOL|UINT|INT|FLOAT|STRING_P|STRING|DIR|PATH_DS|PATH)(_EX|_LV)?(_NS)?(_H)?\s*\(\s*\w+,\s*(\w+),', ds)
                if not dm:
                    am = re.match(r'S_(ACTION)(_EX|_LV)?(_NS)?(_H)?\s*\(\s*(\w+),', ds)
                    dm = am
                if dm:
                    dg = tuple((g[0], g[1]) for g in dstack)
                    for pfx in ('MENU_ENUM_LABEL_', 'MENU_ENUM_SUBLABEL_',
                                'MENU_ENUM_LABEL_VALUE_') + (
                                ('MENU_ENUM_LABEL_HELP_',) if dm.group(4) else ()):
                        names.append((pfx + dm.group(5), dg))
            continue
        if s.startswith('#'):
            continue
        guard = tuple((g[0], g[1]) for g in stack)
        mm = re.match(r'MENU_LABEL\((\w+)\),?$', s)
        if mm:
            t = mm.group(1)
            for pfx in ('MENU_ENUM_LABEL_', 'MENU_ENUM_SUBLABEL_',
                        'MENU_ENUM_LABEL_VALUE_'):
                names.append((pfx + t, guard))
            continue
        mm = re.match(r'MENU_LBL_H\((\w+)\),?$', s)
        if mm:
            t = mm.group(1)
            for pfx in ('MENU_ENUM_LABEL_', 'MENU_ENUM_SUBLABEL_',
                        'MENU_ENUM_LABEL_VALUE_', 'MENU_ENUM_LABEL_HELP_'):
                names.append((pfx + t, guard))
            continue
        mm = re.match(r'([A-Za-z_]\w*)\s*(?:=[^,]*)?,?\s*(?:/\*.*)?$', s)
        if mm:
            names.append((mm.group(1), guard))
            continue
        raise SystemExit('unparsed enum line: ' + repr(s))
    return names


def emit_guard_transition(out, prev, cur):
    if prev == cur:
        return
    for _ in prev:
        out.append('#endif')
    for cond, in_else in cur:
        out.append(cond)
        if in_else:
            out.append('#else')


DRIVER = """
static void put_escaped(const char *s)
{
   for (; *s; s++)
   {
      if (*s == '\\n')
         fputs("\\\\n", stdout);
      else if (*s == '\\\\')
         fputs("\\\\\\\\", stdout);
      else
         fputc(*s, stdout);
   }
}

int main(void)
{
   unsigned lang, i;
   unsigned long rows = 0;
   for (lang = 0; lang < RETRO_LANGUAGE_LAST; lang++)
   {
      msg_hash_set_uint(MSG_HASH_USER_LANGUAGE, lang);
      for (i = 0; i < sizeof(name_tab)/sizeof(name_tab[0]); i++)
      {
         const char *s = msg_hash_to_str((enum msg_hash_enums)name_tab[i].v);
         printf("%s|%u|", name_tab[i].n, lang);
         put_escaped(s ? s : "(null)");
         fputc('\\n', stdout);
         rows++;
      }
   }
   fprintf(stderr, "names=%u langs=%u rows=%lu\\n",
           (unsigned)(sizeof(name_tab)/sizeof(name_tab[0])),
           (unsigned)RETRO_LANGUAGE_LAST, rows);
   return 0;
}
"""


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    out_path = sys.argv[1]
    names = parse_enum_names('msg_hash.h')
    if len(names) < 4000:
        raise SystemExit('implausibly few enum names: %d' % len(names))
    lines = ['#include <stdio.h>',
             '#include <libretro.h>',
             '#include "msg_hash.h"',
             '',
             'static const struct { const char *n; unsigned v; } name_tab[] = {']
    prev = ()
    for n, g in names:
        emit_guard_transition(lines, prev, g)
        prev = g
        lines.append('   { "%s", (unsigned)%s },' % (n, n))
    emit_guard_transition(lines, prev, ())
    lines.append('};')
    lines.append(DRIVER)
    open('/tmp/mh_name_tu.c', 'w').write('\n'.join(lines))
    open('/tmp/mh_name_stubs.c', 'w').write(
        'const char *config_get_ptr(void) { return 0; }\n')
    mh = ('msg_hash.c intl/msg_hash_us.c '
          'libretro-common/string/stdstring.c '
          'libretro-common/compat/compat_strl.c '
          'libretro-common/encodings/encoding_utf.c')
    flags = ('-O0 -g0 -I. -Ilibretro-common/include '
             '-DRARCH_INTERNAL -DHAVE_MENU -DHAVE_LANGEXTRA')
    r = run('gcc %s %s /tmp/mh_name_stubs.c /tmp/mh_name_tu.c -o /tmp/mh_name'
            % (flags, mh))
    if r.returncode != 0:
        raise SystemExit('harness TU failed to build:\n' + r.stderr[-800:])
    r = run('/tmp/mh_name > /tmp/mh_name_raw.txt')
    if r.returncode != 0:
        raise SystemExit('harness run failed: ' + r.stderr[-200:])
    m = re.search(r'names=(\d+) langs=(\d+) rows=(\d+)', r.stderr)
    rows = [x for x in open('/tmp/mh_name_raw.txt', encoding='utf-8',
                            errors='surrogateescape').read().split('\n') if x]
    if len(rows) != int(m.group(3)) or len(rows) != int(m.group(1)) * int(m.group(2)):
        raise SystemExit('row count mismatch: file=%d driver=%s names*langs=%d'
                         % (len(rows), m.group(3),
                            int(m.group(1)) * int(m.group(2))))
    rows.sort()
    with open(out_path, 'w', encoding='utf-8', errors='surrogateescape') as f:
        f.write('\n'.join(rows) + '\n')
    sys.stderr.write('name-keyed dump: %d rows -> %s\n' % (len(rows), out_path))


if __name__ == '__main__':
    main()
