#!/usr/bin/env python3
"""A setting's label exists wherever its translations name it.

The rule this holds
-------------------
A translation file under intl/ names a label for every string it has,
and is compiled in every build.  Where a setting belongs to a feature,
intl/json2h.py copies the guard of its row in settings/settings_def_*.h
into the translation file, so the name is only there when the label is.

It makes one exception: a guard line that mentions a SETTINGS_DEF_*
pass is dropped whole, because such a line is meant to be the same for
every build once the pass is known - "HAVE_X or the strings pass",
"not the configuration pass".

A line that mentions a pass and still depends on a feature breaks that:

    #if defined(HAVE_X) && !defined(SETTINGS_DEF_CONFIG_PASS)

json2h.py drops it, the translations name the label in every build,
and the label only exists with HAVE_X.  Nothing fails until the first
translation of the string arrives from Crowdin; then every build
without the feature stops compiling.  That happened in October 2026
with the MCP server's token and the SMB client's Kerberos rows.

What is checked
---------------
For every guard line that mentions a pass, in a settings/settings_def_*.h
that msg_hash.h takes its labels from: with the passes set as msg_hash.h sets them (the label list) and
as intl/msg_hash_us.h sets them (the strings), the line has to come
out the same whatever the feature macros are - within the builds that
compile the row's region of msg_hash.h at all.

A line that fails names the feature it still depends on.  The usual
repair is "defined(HAVE_X) || defined(SETTINGS_DEF_STRINGS_PASS)", as
the rows around it have.

Usage:  settings_def_guard_check.py [--root DIR] [--selftest]
"""
import argparse
import glob
import itertools
import os
import re
import sys

PASSES = ('SETTINGS_DEF_STRINGS_PASS', 'SETTINGS_DEF_ENUM_PASS',
          'SETTINGS_DEF_CONFIG_PASS')
# the two consumers that decide whether a label and its string exist
CONSUMERS = (('msg_hash.h', {'SETTINGS_DEF_STRINGS_PASS': True,
                             'SETTINGS_DEF_ENUM_PASS': True,
                             'SETTINGS_DEF_CONFIG_PASS': False}),
             ('intl/msg_hash_us.h', {'SETTINGS_DEF_STRINGS_PASS': True,
                                     'SETTINGS_DEF_ENUM_PASS': False,
                                     'SETTINGS_DEF_CONFIG_PASS': False}))


class Unparsed(Exception):
    pass


def condition(line):
    """The condition of a #if/#ifdef/#ifndef line, as text, or None."""
    s = line.strip()
    m = re.match(r'#\s*ifdef\s+(\w+)', s)
    if m:
        return 'defined(%s)' % m.group(1)
    m = re.match(r'#\s*ifndef\s+(\w+)', s)
    if m:
        return '!defined(%s)' % m.group(1)
    m = re.match(r'#\s*(?:el)?if\s+(.*)', s)
    if m:
        return re.sub(r'/\*.*?\*/', '', m.group(1)).strip()
    return None


def compile_condition(text):
    """Returns (function of a dict of names, set of names)."""
    names = set()

    def name(m):
        names.add(m.group(1))
        return ' V[%r] ' % m.group(1)
    expr = re.sub(r'defined\s*\(\s*(\w+)\s*\)', name, text)
    expr = re.sub(r'defined\s+(\w+)', name, expr)
    expr = expr.replace('&&', ' and ').replace('||', ' or ')
    expr = re.sub(r'!(?!=)', ' not ', expr)
    # a bare macro, as in ((TARGET_OS_IOS))
    expr = re.sub(r'(?<![\w\'\[])([A-Za-z_]\w*)(?![\w\'\]])',
                  lambda m: m.group(0) if m.group(1) in ('and', 'or', 'not', 'V')
                  else name(m), expr)
    if re.search(r'[^\s\w\[\]\'()]', expr):
        raise Unparsed(text)
    try:
        code = compile(expr.strip(), '<guard>', 'eval')
    except SyntaxError:
        raise Unparsed(text)
    return (lambda v: bool(eval(code, {'V': v}))), names


def region_guards(msg_hash_text, def_name):
    """The guard lines around each include of a def file in msg_hash.h."""
    found = []
    stack = []
    for line in msg_hash_text.split('\n'):
        s = line.strip()
        if not s.startswith('#'):
            continue
        if re.match(r'#\s*if', s):
            stack.append(condition(s))
        elif re.match(r'#\s*else', s):
            if stack:
                stack[-1] = '!(%s)' % stack[-1] if stack[-1] else None
        elif re.match(r'#\s*elif', s):
            if stack:
                stack[-1] = condition(s)
        elif re.match(r'#\s*endif', s):
            if stack:
                stack.pop()
        elif re.match(r'#\s*include\s+"settings/%s"' % re.escape(def_name), s):
            found.append([c for c in stack if c])
    return found


def check_line(text, regions):
    """Names of the features a pass-mentioning guard still depends on."""
    fn, names = compile_condition(text)
    features = sorted(n for n in names if n not in PASSES)
    if not features:
        return []
    depends = set()
    for region in regions:
        reg = [compile_condition(c) for c in region
               if not any(p in c for p in PASSES)]
        reg_names = set()
        for _, ns in reg:
            reg_names |= ns
        every = sorted(set(features) | reg_names)
        for _consumer, passes in CONSUMERS:
            for bits in itertools.product((False, True), repeat=len(every)):
                v = dict(zip(every, bits))
                v.update(passes)
                if not all(f(v) for f, _ in reg):
                    continue
                # a feature matters if switching it alone, within the
                # builds that compile the region, changes the outcome
                for n in features:
                    w = dict(v)
                    w[n] = not v[n]
                    if all(f(w) for f, _ in reg) and fn(w) != fn(v):
                        depends.add(n)
    return sorted(depends)


def run(root):
    try:
        msg_hash = open(os.path.join(root, 'msg_hash.h'), encoding='utf-8',
                        errors='replace').read()
    except OSError:
        print('settings_def_guard_check: no msg_hash.h under %s' % root)
        return 2
    bad = 0
    lines = 0
    for path in sorted(glob.glob(os.path.join(root, 'settings', 'settings_def_*.h'))):
        name = os.path.basename(path)
        regions = None
        for n, line in enumerate(open(path, encoding='utf-8', errors='replace'), 1):
            if not re.match(r'\s*#\s*(?:el)?if', line):
                continue
            if not any(p in line for p in PASSES):
                continue
            text = condition(line)
            if text is None:
                continue
            lines += 1
            if regions is None:
                regions = region_guards(msg_hash, name)
            if not regions:
                # msg_hash.h does not take its labels from this file:
                # they are written out there, and this guard does not
                # decide whether they exist
                continue
            try:
                depends = check_line(text, regions)
            except Unparsed:
                print('%s:%d: cannot read this guard: %s' % (
                    os.path.relpath(path, root), n, line.strip()))
                bad += 1
                continue
            if depends:
                print('%s:%d: this guard mentions a pass and still depends on %s:\n'
                      '    %s\n'
                      '  intl/json2h.py drops such a line, so the translations name the\n'
                      '  label in builds that do not have it. Make the label exist in the\n'
                      '  strings pass too: defined(%s) || defined(SETTINGS_DEF_STRINGS_PASS).' % (
                          os.path.relpath(path, root), n, ', '.join(depends),
                          line.strip(), depends[0]))
                bad += 1
    if bad:
        print('FAIL settings_def_guard_check: %d guard(s)' % bad)
        return 1
    print('PASS settings_def_guard_check (%d pass-mentioning guards in '
          'settings/settings_def_*.h come out the same in every build)' % lines)
    return 0


def selftest():
    cases = [
        # (guard, region guards, features it must be found to depend on)
        ('defined(HAVE_MCP) && !defined(SETTINGS_DEF_CONFIG_PASS)', [[]], ['HAVE_MCP']),
        ('(defined(HAVE_MCP) || defined(SETTINGS_DEF_STRINGS_PASS)) && !defined(SETTINGS_DEF_CONFIG_PASS)', [[]], []),
        ('defined(HAVE_MCP) || defined(SETTINGS_DEF_STRINGS_PASS)', [[]], []),
        ('!defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))', [[]], []),
        ('!defined(SETTINGS_DEF_CONFIG_PASS) || ((TARGET_OS_IOS))', [[]], []),
        ('defined(HAVE_EGL) && !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)', [[]], []),
        # the SMB rows: wrong on their own, right with either client
        ('defined(HAVE_SMBCLIENT) && !defined(SETTINGS_DEF_CONFIG_PASS)',
         [['defined(HAVE_SMBCLIENT) || defined(HAVE_NFSCLIENT)']], ['HAVE_SMBCLIENT']),
        ('(defined(HAVE_SMBCLIENT) || (defined(SETTINGS_DEF_STRINGS_PASS) && defined(HAVE_NFSCLIENT))) && !defined(SETTINGS_DEF_CONFIG_PASS)',
         [['defined(HAVE_SMBCLIENT) || defined(HAVE_NFSCLIENT)']], []),
        # and that last form is wrong outside such a region
        ('(defined(HAVE_SMBCLIENT) || (defined(SETTINGS_DEF_STRINGS_PASS) && defined(HAVE_NFSCLIENT))) && !defined(SETTINGS_DEF_CONFIG_PASS)',
         [[]], ['HAVE_NFSCLIENT', 'HAVE_SMBCLIENT']),
    ]
    bad = 0
    for text, regions, want in cases:
        got = check_line(text, regions)
        if got != want:
            print('selftest: %s\n   wanted %s, got %s' % (text, want, got))
            bad += 1
    try:
        compile_condition('_MSC_VER < 1900 && defined(SETTINGS_DEF_CONFIG_PASS)')
        print('selftest: a guard that cannot be read was read')
        bad += 1
    except Unparsed:
        pass
    if bad:
        print('FAIL settings_def_guard_check --selftest')
        return 1
    print('PASS settings_def_guard_check --selftest (%d guards, right and wrong)' % len(cases))
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()
    sys.exit(selftest() if args.selftest else run(args.root))
