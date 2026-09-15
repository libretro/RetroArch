#!/usr/bin/env python3
"""Emit MENU_ENUM_LABEL_<TAG>_STR macros from the settings definitions.

Usage, from the repository root:
    python3 tools/gen_lbl_str.py

Rewrites the generated region at the bottom of msg_hash_lbl_str.h in
place, then prints a summary.  Run it after adding or renaming a row in
settings/settings_def_*.h.

Why this exists
---------------
A menu entry's label is a language-independent identifier, but the only
way to get at one from C was msg_hash_to_str(), which is a call plus two
strtab probes -- about 7 ns, against 1.6 ns for a plain string compare
against a literal.  Comparison chains that test a label against a few
hundred candidates therefore paid an eight-times premium for no reason
other than the string not being available at compile time.

msg_hash_lbl_str.h already carried such literals for the ~900 labels
that intl/msg_hash_lbl.h names explicitly.  It could not carry the ones
whose text lives in the settings definitions, because those are X-macro
rows and the preprocessor cannot #define out of a macro expansion.
Hence a generator rather than more hand-written macros: the settings
definition stays the single source, and nothing has to be kept in sync
by hand.

Grammar
-------
Two shapes, distinguished by whether the first argument is a tag or a
config field name:

    S_ACTION*(TAG, "name", ...)          -> tag first
    S_<anything else>(field, TAG, "name", ...)  -> field second

Rows inside #ifdef guards are emitted unconditionally.  The macro is a
plain string constant, so it costs nothing when the enum it names is
compiled out, and the strings pass in intl/msg_hash_us.c already relaxes
those guards for exactly the same reason.

Verification
------------
tools/msg_hash_name_harness.py dumps every (enum name, language,
resolved string) triple.  Every macro this script emits must equal the
resolved string for its enum in every language; a label that varies by
language is not an identifier and must not be turned into a literal.
"""

import os
import re
import sys

# Rows whose enum does not actually resolve to its own name at runtime.
# Both of these have no row in intl/msg_hash_lbl.h, so msg_hash_to_str()
# falls through to the value table and returns a *translated display
# string* -- "表示領域 X 座標補正" under Japanese.  A macro claiming they
# are identifiers would be a lie, and substituting it at a comparison
# site would change behaviour for every non-English user.  Verified with
# the exhaustive sweep described in the module docstring; these are the
# only two of 717 that fail it.
SKIP = frozenset((
    'MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X',
    'MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y',
))

BEGIN = "/* GENERATED REGION BEGIN: settings-definition label strings"
END = "/* GENERATED REGION END: settings-definition label strings */"

ROW = re.compile(
    r'\b(S_[A-Z_0-9]+)\('          # macro
    r'\s*([A-Za-z0-9_]+)\s*,'      # arg 0
    r'\s*([A-Za-z0-9_]+)?\s*,?',   # arg 1 (identifier form only)
    re.M)


def rows(path):
    """Yield (tag, name) for every settings definition row in a file."""
    text = open(path, encoding='utf-8').read()
    for m in re.finditer(r'\b(S_[A-Z_0-9]+)\(', text):
        macro = m.group(1)
        # Walk the argument list far enough to reach the name string.
        rest = text[m.end():]
        args = []
        depth = 0
        cur = ''
        for ch in rest:
            if ch in '([':
                depth += 1
            elif ch in ')]':
                if depth == 0:
                    args.append(cur)
                    break
                depth -= 1
            if ch == ',' and depth == 0:
                args.append(cur)
                cur = ''
                if len(args) >= 3:
                    break
                continue
            cur += ch
        args = [a.strip() for a in args]
        if len(args) < 2:
            continue
        if macro.startswith('S_ACTION'):
            tag, name = args[0], args[1]
        else:
            if len(args) < 3:
                continue
            tag, name = args[1], args[2]
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*', tag):
            continue
        sm = re.fullmatch(r'"([^"]*)"', name)
        if not sm:
            continue
        yield tag, sm.group(1)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sdir = os.path.join(root, 'settings')
    hdr = os.path.join(root, 'msg_hash_lbl_str.h')

    found = {}
    dupes = []
    for fn in sorted(os.listdir(sdir)):
        if not (fn.startswith('settings_def_') and fn.endswith('.h')):
            continue
        for tag, name in rows(os.path.join(sdir, fn)):
            key = 'MENU_ENUM_LABEL_' + tag
            if key in found and found[key] != name:
                dupes.append((key, found[key], name))
            found.setdefault(key, name)

    text = open(hdr, encoding='utf-8').read()
    if BEGIN in text:
        head = text[:text.index(BEGIN)]
        tail = text[text.index(END) + len(END):]
    else:
        marker = '#endif'
        cut = text.rindex(marker)
        head, tail = text[:cut], text[cut:]

    existing = set(re.findall(r'^#define (MENU_ENUM_LABEL_[A-Z0-9_]+)_STR ',
                              head, re.M))
    emit = {k: v for k, v in found.items()
            if k not in existing and k not in SKIP}

    body = [BEGIN,
            " *",
            " * Produced by tools/gen_lbl_str.py from",
            " * settings/settings_def_*.h.  Do not edit by hand; rerun the",
            " * script after adding or renaming a row there. */"]
    for k in sorted(emit):
        body.append('#define %s_STR "%s"' % (k, emit[k]))
    body.append(END)

    open(hdr, 'w', encoding='utf-8').write(head + '\n'.join(body) + '\n\n' + tail.lstrip('\n'))

    print("settings rows parsed : %d" % len(found))
    print("already hand-written : %d" % (len(found) - len(emit)))
    print("emitted              : %d" % len(emit))
    print("skipped (not identifiers): %d" % len(SKIP))
    for k, a, b in dupes:
        print("  conflicting rows for %s: %r vs %r" % (k, a, b),
              file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
