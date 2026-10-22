#!/usr/bin/env python3
"""Write msg_hash_lbl_str.h: one MENU_ENUM_LABEL_<TAG>_STR macro per
menu label row, naming that row's bytes in the packed label blob.

Usage, from the repository root:
    python3 tools/gen_lbl_str.py

Rewrites the generated region of msg_hash_lbl_str.h in place and prints
a summary.  Run it after adding, renaming or guarding a row in
intl/msg_hash_lbl.h or in a settings/settings_def_*.h that
intl/msg_hash_lbl_rows.h includes.

Why this exists
---------------
A menu entry's label is a language-independent identifier, but the only
way to get at one from C was msg_hash_to_str(), which is a call plus two
strtab probes -- about 7 ns, against 1.6 ns for a plain string compare
against a constant address.  Comparison chains that test a label against
a few hundred candidates therefore paid an eight-times premium for no
reason other than the string not being available at compile time.

The label strings themselves are kept once, in the packed blob that
intl/msg_hash_us.c builds from the same rows (struct msg_hash_us_lbl_blob_s,
one exactly-sized char member per row, declared by this header through
intl/msg_hash_lbl_rows.h).  Each macro here names that row's member:
a constant address, as cheap to compare as a literal, and the literal
is not duplicated in the string pool.  Where a row is compiled out, or
in a build without the menu, the macro falls back to the literal so
every use compiles as it always did.

Rows
----
intl/msg_hash_lbl.h rows are MSG_HASH(ID, "name"); a row repeated under
mutually exclusive guards is one member with a per-platform string.
Settings definition rows come in two shapes, distinguished by whether
the first argument is a tag or a config field name:

    S_ACTION*(TAG, "name", ...)          -> tag first
    S_<anything else>(field, TAG, "name", ...)  -> field second

and a level variant (_LV) carries its level tag between the row tag
and the name.

Only the row macros that intl/msg_hash_lbl_rows.h expands under the
strings pass count: a guard mentioning SETTINGS_DEF_STRINGS_PASS is true
in that pass (tools/settings_def_guard_check.c keeps those uniform), any
other guard travels onto the macro.

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

# MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X / _Y resolve to a translated
# display string under a non-English language, so msg_hash_to_str() of
# them is not the identifier their macro names; the macro is still the
# row's string, which is what the configuration pass keys on.  Do not
# substitute them at a comparison site.

BEGIN = "/* GENERATED REGION BEGIN: label strings"
END = "/* GENERATED REGION END: label strings */"
ROWS_H = os.path.join('intl', 'msg_hash_lbl_rows.h')


class Guards(object):
    """Tracks the preprocessor guard stack while walking lines."""

    def __init__(self):
        self.stack = []     # each entry: [chain, current condition, earlier branches]
        self.chains = 0

    def feed(self, line):
        s = line.strip()
        if s.startswith('#ifdef '):
            self.push('defined(%s)' % s[7:].strip())
        elif s.startswith('#ifndef '):
            self.push('!defined(%s)' % s[8:].strip())
        elif s.startswith('#if '):
            self.push(s[4:].strip())
        elif s.startswith('#elif '):
            top = self.stack[-1]
            top[2].append(top[1])
            top[1] = s[6:].strip()
        elif s.startswith('#else'):
            top = self.stack[-1]
            top[2].append(top[1])
            top[1] = None
        elif s.startswith('#endif'):
            self.stack.pop()
        else:
            return False
        return True

    def push(self, cond):
        self.chains += 1
        self.stack.append([self.chains, cond, []])

    def condition(self):
        """The guards in force, innermost last: (chain, C condition,
        is the #else branch) per nesting level.  Rows of one id that
        together cover every branch of a chain are present whatever
        that chain decides, which emit() folds away."""
        out = []
        for chain, cond, seen in self.stack:
            if cond is None:
                text = '!(' + ' || '.join('(%s)' % c for c in seen) + ')'
            elif seen:
                text = ('!(' + ' || '.join('(%s)' % c for c in seen)
                        + ') && (%s)' % cond)
            else:
                text = cond
            out.append((chain, text, cond is None))
        return out


def strings_pass_true(level):
    """A guard the strings pass always satisfies: true for every value
    of the feature macros once SETTINGS_DEF_STRINGS_PASS is defined and
    SETTINGS_DEF_CONFIG_PASS is not."""
    cond = level[1]
    syms = []

    def sym(m):
        name = m.group(1)
        if name == 'SETTINGS_DEF_STRINGS_PASS':
            return 'True'
        if name == 'SETTINGS_DEF_CONFIG_PASS':
            return 'False'
        if name not in syms:
            syms.append(name)
        return 'v[%d]' % syms.index(name)

    expr = re.sub(r'defined\s*\(\s*(\w+)\s*\)', sym, cond)
    expr = re.sub(r'defined\s+(\w+)', sym, expr)
    # a bare macro tested for its value is a symbol too
    expr = re.sub(r'(?<![\w\[])(?!True|False|v\b)([A-Za-z_]\w*)', sym, expr)
    expr = expr.replace('&&', ' and ').replace('||', ' or ').replace('!', ' not ')
    if not re.fullmatch(r'[\w\s().\[\]]*', expr) or len(syms) > 8:
        return False
    try:
        for bits in range(1 << len(syms)):
            v = [bool(bits >> i & 1) for i in range(len(syms))]
            if not eval(expr, {'__builtins__': {}}, {'v': v}):
                return False
    except Exception:
        return False
    return True


def strings_pass_value(level):
    """@level with the pass macros at their strings-pass values, so the
    condition reads the same outside that pass."""
    text = re.sub(r'defined\s*\(\s*SETTINGS_DEF_STRINGS_PASS\s*\)', '1', level[1])
    text = re.sub(r'defined\s*\(\s*SETTINGS_DEF_CONFIG_PASS\s*\)', '0', text)
    return (level[0], text, level[2])


def fold(conds):
    """The condition lists of one id's rows with every chain they cover
    completely folded away: rows that are the branches of one #if /
    #elif / #else, under the same outer guards, stand for the outer
    guards alone."""
    conds = [list(c) for c in conds]
    while True:
        groups = {}
        for c in conds:
            if c:
                groups.setdefault((tuple(c[:-1]), c[-1][0]), []).append(c)
        folded = False
        for (outer, chain), members in groups.items():
            if len(members) > 1 and any(m[-1][2] for m in members):
                conds = [c for c in conds if c not in members] + [list(outer)]
                folded = True
                break
        if not folded:
            return conds


def lbl_rows(path):
    """(id, literal, conditions) for every MSG_HASH row of
    intl/msg_hash_lbl.h."""
    text = open(path, encoding='utf-8').read()
    g = Guards()
    out = []
    cur = None
    for line in text.split('\n'):
        if g.feed(line):
            continue
        s = line.strip()
        if cur is None:
            if s.startswith('MSG_HASH('):
                cur = s[len('MSG_HASH('):]
            continue
        cur += ' ' + s
        if ')' in s:
            m = re.match(r'\s*([A-Za-z0-9_]+)\s*,\s*("(?:[^"\\]|\\.)*")\s*\)', cur)
            if not m:
                raise SystemExit('unparsed msg_hash_lbl.h row: %s' % cur)
            out.append((m.group(1), m.group(2), list(g.condition())))
            cur = None
    return out


def pass_macros(rows_h):
    """The S_ row macros intl/msg_hash_lbl_rows.h expands, and the
    settings files it includes, in order."""
    macros = set()
    files = []
    for line in open(rows_h, encoding='utf-8'):
        m = re.match(r'#define (S_[A-Z_0-9]+)\(', line)
        if m:
            macros.add(m.group(1))
        m = re.match(r'#include "\.\./settings/(settings_def_[a-z0-9_]+\.h)"', line)
        if m:
            files.append(m.group(1))
    return macros, files


def def_rows(path, macros):
    """(id, literal, conditions) for every strings-pass row of one
    settings definition file."""
    text = open(path, encoding='utf-8').read()
    # comments blanked, not removed, so the guard walk keeps its lines
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)),
                  text, flags=re.S)
    g = Guards()
    out = []
    pos = 0
    for line in text.split('\n'):
        start = pos
        pos += len(line) + 1
        if g.feed(line):
            continue
        for m in re.finditer(r'\b(S_[A-Z_0-9]+)\(', line):
            macro = m.group(1)
            # a help variant (_H) is its base row outside the enum pass
            if macro.endswith('_H'):
                macro = macro[:-2]
            if macro not in macros:
                continue
            # Walk the argument list far enough to reach the name string.
            rest = text[start + m.end():]
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
                    if len(args) >= 4:
                        break
                    continue
                cur += ch
            args = [a.strip() for a in args]
            # a level variant (_LV) carries its level tag after the row tag
            i = 0 if macro.startswith('S_ACTION') else 1
            tag = args[i]
            name = args[i + (2 if '_LV' in macro else 1)]
            if not re.fullmatch(r'[A-Z][A-Z0-9_]*', tag):
                raise SystemExit('%s: unparsed row %s' % (path, line.strip()))
            if not re.fullmatch(r'"(?:[^"\\]|\\.)*"', name):
                raise SystemExit('%s: %s has no literal name' % (path, tag))
            cond = [strings_pass_value(c) for c in g.condition()
                    if not strings_pass_true(c)]
            out.append(('MENU_ENUM_LABEL_' + tag, name, cond))
    return out


def emit(rows):
    """The generated region for rows in table order."""
    order = []
    lits = {}      # id -> [(condition list, literal)] in row order
    for ident, literal, cond in rows:
        if ident not in lits:
            order.append(ident)
            lits[ident] = []
        lits[ident].append((cond, literal))
    o = [BEGIN,
         " *",
         " * Produced by tools/gen_lbl_str.py from intl/msg_hash_lbl.h and the",
         " * settings/settings_def_*.h rows intl/msg_hash_lbl_rows.h expands.",
         " * Do not edit by hand; rerun the script after changing a row. */",
         "#ifdef HAVE_MENU",
         "#define MSG_HASH_LBL_ROW(Id, str) char m_##Id[sizeof(str)];",
         "struct msg_hash_us_lbl_blob_s",
         "{",
         '#include "intl/msg_hash_lbl_rows.h"',
         "};",
         "#undef MSG_HASH_LBL_ROW",
         "extern const struct msg_hash_us_lbl_blob_s msg_hash_us_lbl_blob;",
         "#endif",
         ""]

    def literal(ident, out):
        """The literal fallback: one line, or the rows' own guard chain
        when the string differs by platform."""
        variants = lits[ident]
        if len(set(l for _, l in variants)) == 1:
            out.append("#define %s_STR %s" % (ident, variants[0][1]))
            return
        first = True
        for cond, lit in variants:
            if not cond:
                out.append("#else" if not first else "#if 1")
            else:
                out.append("%s %s" % ("#if" if first else "#elif",
                                      ' && '.join('(%s)' % c[1] for c in cond)))
            out.append("#define %s_STR %s" % (ident, lit))
            first = False
        out.append("#endif")

    always = []
    guarded = []
    for ident in order:
        folded = fold([c for c, _ in lits[ident]])
        if any(not c for c in folded):
            always.append(ident)
        else:
            guarded.append((ident, folded))
    o.append("#ifdef HAVE_MENU")
    for ident in always:
        o.append("#define %s_STR (msg_hash_us_lbl_blob.m_%s)" % (ident, ident))
    o.append("#else")
    for ident in always:
        literal(ident, o)
    o.append("#endif")
    for ident, folded in guarded:
        alts = ' || '.join('(%s)' % ' && '.join('(%s)' % c[1] for c in cs)
                           for cs in folded)
        o.append("#if defined(HAVE_MENU) && (%s)" % alts)
        o.append("#define %s_STR (msg_hash_us_lbl_blob.m_%s)" % (ident, ident))
        o.append("#else")
        literal(ident, o)
        o.append("#endif")
    o.append(END)
    return '\n'.join(o) + '\n', len(always), len(guarded)


def main():
    hdr = 'msg_hash_lbl_str.h'
    if not os.path.exists(hdr) or not os.path.exists(ROWS_H):
        raise SystemExit('run from the repository root')
    macros, files = pass_macros(ROWS_H)
    rows = lbl_rows(os.path.join('intl', 'msg_hash_lbl.h'))
    n_lbl = len(rows)
    for fn in files:
        rows.extend(def_rows(os.path.join('settings', fn), macros))

    text = open(hdr, encoding='utf-8').read()
    if BEGIN not in text or END not in text:
        raise SystemExit('%s: generated region markers missing' % hdr)
    head = text[:text.index(BEGIN)]
    tail = text[text.index(END) + len(END):]
    body, n_always, n_guarded = emit(rows)
    open(hdr, 'w', encoding='utf-8').write(head + body + tail.lstrip('\n'))

    print("label rows           : %d (%d in msg_hash_lbl.h, %d in %d settings files)"
          % (len(rows), n_lbl, len(rows) - n_lbl, len(files)))
    print("macros emitted       : %d always, %d under a row guard"
          % (n_always, n_guarded))
    return 0


if __name__ == '__main__':
    sys.exit(main())
