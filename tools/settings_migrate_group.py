#!/usr/bin/env python3
"""Migrate one menu descriptor table to a single-source settings def file.

Usage, from the repository root:
    python3 tools/settings_migrate_group.py <table_name> <def_basename> "<group title>"
e.g.
    python3 tools/settings_migrate_group.py bias_desc settings_def_video_bias.h "viewport bias group"

Extracts every scattered piece of each setting in the table - the us.h
VALUE and SUBLABEL rows, the _STR define, the label-table row, the
configuration.c row and the descriptor row - verifies per-setting
completeness up front, generates the def file, rewires all consumers by
in-place substitution, and runs the full gate battery. Nothing is kept
unless every gate passes; run from a clean worktree and reset on failure.

Handles: BOOL/UINT/INT/FLOAT kinds, rows without sublabels (_NS),
config keys that differ from the label string (kept literal behind
SETTINGS_DEF_CONFIG_PASS), and per-row preprocessor guards (carried
into the def file; all consumers gate identically).

Gates: six strict-C89 syntax lanes including D3DKMT and guard-heavy;
preprocessor token-stream identity of the table in both guard
polarities against the parent revision; lone-comma and empty-table
structural audits; msg_hash_us.json exact equality against a
scratch-tree regeneration of the parent revision; the full-language
lookup harness byte-identical; settings dump byte-identical against a
same-tree stash baseline (non-vacuous, dep-hygienic, try/finally).

The harness binaries under /tmp (stubs, lookup driver, out_pre baseline
and run configuration) are session-local; see the settings campaign
notes for how to rebuild them.
"""
import re, subprocess, os, json, sys
def run(cmd): return subprocess.run(cmd, shell=True, capture_output=True, text=True)
CSTR = r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*'
TABLE, DEF, TITLE = sys.argv[1], sys.argv[2], sys.argv[3]
assert not os.path.exists(os.path.join('settings', os.path.basename(DEF))), \
    'def file %s already exists upstream - pick a different name' % DEF
SIGS = [('S_BOOL','f, T, n, d, sd, df, c'),
        ('S_UINT','f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp'),
        ('S_INT', 'f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp'),
        ('S_FLOAT','f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp'),
        ('S_STRING','f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui'),
        ('S_DIR','f, T, n, d, el, sd, c, sta'),
        ('S_STRING_P','f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui'),
        ('S_PATH','f, T, n, d, sd, c, vals, rp, ui'),
        ('S_PATH_DS','f, T, n, df2, sd, c, vals, rp, ui'),
        ('S_ACTION','T, n'),
        ('S_BOOL_EX','f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui'),
        ('S_UINT_EX','f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui'),
        ('S_INT_EX','f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui'),
        ('S_FLOAT_EX','f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui'),
        ('S_ACTION_EX','T, n, sd, ok, rp, c')]
# Coupled platform defines: the polarity lanes derive tokens from row
# guards, but some tokens only ever appear alongside a partner in real
# builds (the SDK conditional vs the build system's own define); a lane
# carrying one without the other is a build no target produces.
DEFINE_COUPLES = {
    'TARGET_OS_IOS': ('IOS',),
    'TARGET_OS_TV': ('IOS',),
    'HAVE_COCOATOUCH': ('HAVE_COCOA_METAL',),
    'HAVE_CLOUDSYNC': ('HAVE_NETWORKING',),
}
def _couple(flag):
    toks = [f[2:] for f in flag.split() if f.startswith('-D')]
    extra = []
    for tk in toks:
        for p in DEFINE_COUPLES.get(tk, ()):
            if p not in toks and ('-D%s' % p) not in extra:
                extra.append('-D%s' % p)
    return (flag + ' ' + ' '.join(extra)).strip()
UNDEFS = '\n'.join('#undef %s%s' % (b, s) for b, _ in SIGS for s in ('', '_NS', '_H', '_NS_H'))
def defs(mk):
    L = []
    for base, sig in SIGS:
        L.append('#define %s(%s, us, sub)%s' % (base, sig, mk(base, True)))
        L.append('#define %s_NS(%s, us)%s' % (base, sig, mk(base, False)))
    return '\n'.join(L)

ms = open('menu/menu_setting.c').read()
tm = re.search(r'static const setting_desc_t %s\[\] = \{\n(.*?)\n( *)\};' % TABLE, ms, re.S)
assert tm, TABLE
def _negate_guard(g):
    body = g.strip()
    if body.startswith('#ifdef'):
        return '#if !defined(%s)' % body.split()[1]
    if body.startswith('#ifndef'):
        return '#if defined(%s)' % body.split()[1]
    return '#if !(%s)' % body[len('#if'):].strip()

def guard_at(text, pos):
    """Enclosing guard stack, else-aware: a position inside the #else
    branch carries the negation of the branch condition. Blindness to
    #else prepended '(HAVE_LAKKA || HAVE_ODROIDGO2)' to the
    restart-visibility row that actually lives in that conditional's
    else branch."""
    stack = []
    for gl in re.finditer(r'^[ \t]*#(if\w*[^\n]*|else|endif)', text[:pos], re.M):
        s = gl.group(1)
        if s.startswith('if'):
            _full, _e = s, gl.end()
            while _full.rstrip().endswith('\\'):
                _nl = text.find('\n', _e)
                if _nl < 0:
                    break
                _nl2 = text.find('\n', _nl + 1)
                _cont = text[_nl + 1:(_nl2 if _nl2 >= 0 else len(text))]
                _full = _full.rstrip().rstrip('\\').rstrip() + ' ' + _cont.strip()
                if _nl2 < 0:
                    break
                _e = _nl2
            stack.append('#' + _full.strip())
        elif s.startswith('else') and stack:
            stack[-1] = _negate_guard(stack[-1])
        elif s.startswith('endif') and stack:
            stack.pop()
    return tuple(stack)
def guard_of(text, pat):
    m = re.search(pat, text)
    return guard_at(text, m.start()) if m else None

table_guard = tuple(guard_at(ms, tm.start()))
guards = {}
_cont_prev = None
body = tm.group(1)
# Depth-aware: record the FULL stack of enclosing #if guards for each row.
# A flat "#if ... #endif" regex (non-greedy) stops at the first #endif and
# so drops inner guards for nested blocks - e.g. a row under
#   #ifdef HAVE_CDROM
#   #ifdef HAVE_LAKKA
# would be emitted under HAVE_CDROM only, breaking any build where the
# outer flag is set but the inner is not.  Carry every enclosing guard.
gstack = []
for ln in body.split('\n'):
    s = ln.strip()
    if _cont_prev is not None:
        _joined = _cont_prev.rstrip('\\').rstrip() + ' ' + s
        _cont_prev = _joined if s.endswith('\\') else None
        if _cont_prev is None:
            gstack.append(_joined)
        continue
    if s.startswith('#if'):
        if s.endswith('\\'):
            _cont_prev = s
            continue
        gstack.append(s)
    elif s.startswith('#endif'):
        if gstack:
            gstack.pop()
    else:
        rm = re.match(r'\s*SDESC_\w+_ROW(?:_P|_DS|_EX)?\(\s*(\w+)\s*[,)]', ln)
        if rm and gstack:
            guards[rm.group(1)] = tuple(gstack)
_ordered = [(m.start(), (m.group(1) + (m.group(2) or ''), m.group(3), m.group(4), re.sub(r'\s+',' ',m.group(5)).strip()))
        for m in re.finditer(r'SDESC_(BOOL|UINT|INT|FLOAT|STRING|DIR|PATH)_ROW(_P|_DS|_EX)?\(\s*(\w+),\s*(\w+),((?:[^()]|\((?:[^()]|\([^()]*\))*\))*)\)', body)]
_ordered += [(m.start(), ('ACTION', '', m.group(1), '')) for m in re.finditer(r'SDESC_ACTION_ROW\(\s*(\w+)\s*\)', body)]
_ordered += [(m.start(), ('ACTION_EX', '', m.group(1), re.sub(r'\s+',' ',m.group(2)).strip())) for m in re.finditer(r'SDESC_ACTION_ROW_EX\(\s*(\w+),((?:[^()]|\((?:[^()]|\([^()]*\))*\))*)\)', body)]
rows = [r for _, r in sorted(_ordered)]
all_invocations = re.findall(r'SDESC_\w+?_ROW(?:_\w+)?\(', body)
assert len(rows) == len(all_invocations), (
    'table contains %d rows but only %d are plain-grammar '
    'SDESC_{BOOL,UINT,INT,FLOAT}_ROW; variant rows (_EX/_AT/_LV/...) are '
    'outside the def grammar - migrate this table manually or extend the '
    'grammar deliberately' % (len(all_invocations), len(rows)))
assert rows and len(rows) == len(re.findall(r'SDESC_\w+_ROW(?:_P|_DS|_EX)?\(', tm.group(1))), (len(rows), tm.group(1)[:200])

us = open('intl/msg_hash_us.h').read()
ref_tokens = set()
cfg_absent = set()
usval, ussub, usspan, uscmt = {}, {}, [], {}
for k, f, T, a in rows:
    m = re.search(r'MSG_HASH\(\s*(/\*.*?\*/)?\s*\n?\s*MENU_ENUM_LABEL_VALUE_%s,\s*\n?\s*(%s)\s*\n?\s*\)\n?' % (T, CSTR), us)
    if not m:
        _own = run("grep -lE 'S_\\w*\\((\\w+, *)?%s,' settings/*.h" % T).stdout.strip()
        if _own:
            ref_tokens.add(T)
            continue
    assert m, ('VALUE', T)
    usval[T] = re.sub(r'\s*\n\s*', ' ', m.group(2)); usspan.append((m.start(), m.end()))
    if m.group(1):
        uscmt[T] = m.group(1)
    m = re.search(r'MSG_HASH\(\s*(/\*.*?\*/)?\s*\n?\s*MENU_ENUM_SUBLABEL_%s,\s*\n?\s*(%s)\s*\n?\s*\)\n?' % (T, CSTR), us)
    if m:
        ussub[T] = re.sub(r'\s*\n\s*', ' ', m.group(2)); usspan.append((m.start(), m.end()))
        if m.group(1):
            uscmt[T] = (uscmt.get(T, '') + ' ' + m.group(1)).strip()
if ref_tokens:
    _tail = [r for r in rows if r[2] in ref_tokens]
    _ref_literals = {}
    for _k3, _f3, _t3, _a3 in _tail:
        _rm3 = re.search(r'[ \t]*SDESC_\w+_ROW(?:_P|_DS|_EX)?\(\s*%s,\s*%s,(?:[^()]|\((?:[^()]|\([^()]*\))*\))*\),?' % (_f3, _t3), body)
        assert _rm3, _t3
        _rl3 = _rm3.group(0).strip()
        for _g3 in reversed(guards.get(_f3 or _t3, ())):
            _rl3 = _g3 + '\n                  ' + _rl3 + '\n#endif'
        _ref_literals[_t3] = _rl3
    print('  note: %d reference row(s) carried literal inside the def' % len(_ref_literals))
else:
    _ref_literals = {}
lblstr = open('msg_hash_lbl_str.h').read()
names, lblstr_span = {}, []
for k, f, T, a in rows:
    if T in ref_tokens:
        continue
    m = re.search(r'#define MENU_ENUM_LABEL_%s_STR (%s)\n' % (T, CSTR), lblstr)
    assert m, T
    names[T] = m.group(1); lblstr_span.append((m.start(), m.end()))
    ext = run('grep -rl "MENU_ENUM_LABEL_%s_STR\\b" --include=*.h --include=*.c . | grep -v msg_hash_lbl' % T).stdout.strip()
    assert ext == '', (T, ext)
lbl = open('intl/msg_hash_lbl.h').read()
lbl_span = []
for k, f, T, a in rows:
    if T in ref_tokens:
        continue
    m = re.search(r'MSG_HASH\(\s*\n?\s*MENU_ENUM_LABEL_%s,\s*\n?\s*MENU_ENUM_LABEL_%s_STR\s*\n?\s*\)\n?' % (T, T), lbl)
    assert m, T
    lbl_span.append((m.start(), m.end()))
# Entanglement guard: a field that also appears in a variant row
# (_LV level overrides, _EX, _AT and friends) elsewhere shares runtime
# state with that row; migrating only the plain row breaks the
# variant's resolution - found empirically when the widget scale
# migration zeroed the fullscreen override's default. Such tables
# wait for the variant grammar.
_ms_all = open('menu/menu_setting.c').read()
for _k, _f, _T, _a in rows:
    _vm = re.search(r'SDESC_\w+_ROW_(?:LV|AT|AT_EX)\(%s,' % _f, _ms_all)
    assert not _vm, ("field %s is entangled with a variant row" % _f,
                     _ms_all[_vm.start():_vm.start()+60] if _vm else '')
print("extraction ok: %d settings (%d without sublabel)" % (len(rows), sum(1 for _,_,T,_ in rows if T not in ussub)))

cfg = open('configuration.c').read()
cfg_spans = []
cfg_keeps = []
cfg_guards = {}
for k, f, T, a in rows:
    if T in ref_tokens:
        continue
    _mac = {'STRING': 'ARRAY', 'DIR': 'PATH'}.get(k, k.replace('_EX', ''))
    m = re.search(r' *SETTING_%s\(\s*%s, *&?settings->\w+\.%s,[^;]*;\n' % (_mac, re.escape(names[T]), f), cfg)
    if k in ('DIR', 'PATH', 'PATH_DS', 'STRING', 'STRING_P', 'ACTION', 'ACTION_EX'):
        m = None  # dirs keep literal config rows: default-enable varies per row 
    if m:
        cfg_guards[T] = tuple(g for g in guard_at(cfg, m.start()) if g not in table_guard)
        cfg_spans.append((m.start(), m.end())); continue
    m = re.search(r' *SETTING_%s\(\s*("(?:[^"\\]|\\.)*"), *&?settings->\w+\.%s,[^;]*;\n' % (r'\w+' if k in ('DIR', 'PATH', 'PATH_DS', 'STRING_P', 'ACTION', 'ACTION_EX') else _mac, f), cfg)
    if not m and k in ('DIR', 'PATH', 'PATH_DS', 'STRING_P', 'ACTION', 'ACTION_EX'):
        # unpersisted setting: keep-literal kinds may lack a
        # config row entirely; nothing to delete or emit
        print('  note: %s has no config row - unpersisted, kept absent' % T)
        continue
    if not m and f and re.search(r'settings->\w+\.%s\s*=' % re.escape(f), cfg):
        # custom persistence: the field is assigned outside the SETTING_
        # rows (packed or computed defaults); config semantics stay with
        # that code and the def emits no config row
        print('  note: %s persists through custom code - config row kept absent' % T)
        cfg_absent.add(T)
        continue
    assert m, (T, f, "no config row at all")
    cfg_keeps.append((T, m.group(1)))
    print("  note: %s config key %s differs from label %s - config row stays literal" % (T, m.group(1), names[T]))
KEEP = set(T for T, _ in cfg_keeps)
# Common enclosing guard of the removed config rows.  These rows may sit
# inside a group-level guard in configuration.c (e.g. #ifdef HAVE_MENU
# around the whole menu-visibility block) that is NOT a per-row guard in
# the descriptor table, so it is never carried into the def file.  Capture
# it now (before the rows are deleted) and replicate it on the config-pass
# include, or a HAVE_MENU-less build expands the rows against undeclared
# DEFAULT_* macros.  Flags already carried per-row in the def file are
# dropped so the include is not double-guarded.
_row_stacks = [guard_at(cfg, s) for s, e in cfg_spans]
_common = _row_stacks[0] if _row_stacks else ()
for _st in _row_stacks[1:]:
    _i = 0
    while _i < len(_common) and _i < len(_st) and _common[_i] == _st[_i]:
        _i += 1
    _common = _common[:_i]
_defrow_flags = {g.split()[-1] for gs in guards.values() for g in gs}
CFG_GROUP_GUARD = tuple(g for g in _common if g.split()[-1] not in _defrow_flags)
if CFG_GROUP_GUARD:
    print("  config-pass include will be guarded by: %s" % ' '.join(CFG_GROUP_GUARD))
for s, e in sorted(cfg_spans, reverse=True): cfg = cfg[:s] + cfg[e:]
cfg = re.sub(r'#if[^\n]*\n#endif\n', '', cfg)
out = ['''/* Single-source definitions: %s.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */
''' % TITLE]
_mh_text = open('msg_hash.h').read()
_h_used = set()
enum_rows = []
for k, f, T, a in rows:
    if T in ref_tokens:
        out.append('/* Row referencing %s; strings owned by another def file. */\n'
                   '#if !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)\n'
                   '%s\n#endif' % (T, _ref_literals[T]))
        continue
    _hs = '_H' if ('MENU_LBL_H(%s),' % T) in _mh_text else ''
    if _hs:
        assert ('MENU_LABEL(%s),' % T) not in _mh_text, (T, 'both enum forms present')
    if T in ussub:
        if k.startswith('ACTION'):
            row = 'S_%s%s(%s,\n      %s,%s\n      %s,\n      %s)' % (k, _hs, T, names[T], (' ' + a + ',') if a else '', usval[T], ussub[T])
        elif True:
            row = 'S_%s%s(%s, %s,\n      %s,\n      %s,\n      %s,\n      %s)' % (
            k, _hs, f, T, names[T], a, usval[T], ussub[T])
        if _hs: _h_used.add('S_%s_H' % k)
    else:
        if k.startswith('ACTION'):
            row = 'S_%s_NS%s(%s,\n      %s,%s\n      %s)' % (k, _hs, T, names[T], (' ' + a + ',') if a else '', usval[T])
        else:
            row = 'S_%s_NS%s(%s, %s,\n      %s,\n      %s,\n      %s)' % (
            k, _hs, f, T, names[T], a, usval[T])
        if _hs: _h_used.add('S_%s_NS_H' % k)
    if T in uscmt:
        row = '/* %s */\n' % uscmt[T].strip('/* ').rstrip(' */') + row
    if T in cfg_absent:
        row = ('/* Persistence lives in custom configuration code; no config\n'
               ' * row is emitted. */\n'
               '#ifndef SETTINGS_DEF_CONFIG_PASS\n') + row + '\n#endif'
    if cfg_guards.get(T):
        _cc = ' && '.join(('defined(%s)' % g[len('#ifdef '):].strip()) if g.startswith('#ifdef ')
                          else ('!defined(%s)' % g[len('#ifndef '):].strip()) if g.startswith('#ifndef ')
                          else '(' + g[len('#if '):].strip() + ')' for g in cfg_guards[T])
        row = ('/* The configuration row lives under %s; other passes are\n'
               ' * unaffected. */\n'
               '#if !defined(SETTINGS_DEF_CONFIG_PASS) || (%s)\n' % (_cc.replace('defined(', '').replace(')', '') if False else _cc, _cc)) + row + '\n#endif'
    if T in KEEP:
        ck = dict(cfg_keeps)[T]
        row = ('/* config key %s differs from the label string; the\n'
               ' * configuration.c row stays literal for this setting. */\n'
               '#ifndef SETTINGS_DEF_CONFIG_PASS\n' % ck) + row + '\n#endif'
    row_gs = list(table_guard) + (list(guards[f or T]) if (f or T) in guards else [])
    if row_gs:
        gs = row_gs
        def _cond(g):
            if g.startswith('#ifdef '):
                return 'defined(%s)' % g[len('#ifdef '):].strip()
            if g.startswith('#ifndef '):
                return '!defined(%s)' % g[len('#ifndef '):].strip()
            return g[len('#if '):].strip()
        # Parenthesize each guard level: an OR inside one level must
        # not associate with the && joining the levels - unbracketed,
        # 'A || B' && 'C' parses as A || (B && C), which eliminated
        # the restart-visibility row in every non-Lakka menu pass.
        cond   = ' && '.join(
            ('(%s)' % _cond(g)) if ('||' in _cond(g) or '&&' in _cond(g))
            else _cond(g) for g in gs)
        gopen  = '\n'.join(gs)
        gclose = '\n'.join('#endif' for _ in gs)
        us_g  = guard_of(open('intl/msg_hash_us.h').read(), r'MENU_ENUM_LABEL_VALUE_%s,' % T)
        lbl_g = guard_of(open('intl/msg_hash_lbl.h').read(), r'MENU_ENUM_LABEL_%s,' % T)
        strings_guarded = bool(us_g) and bool(lbl_g)
        strings_free    = not us_g and not lbl_g
        assert strings_guarded or strings_free, (T, us_g, lbl_g, 'mixed string-guard topology')
        if strings_free:
            row = ('/* Descriptor and configuration rows are %s; the string\n'
                   ' * tables always carry this row via the strings pass. */\n'
                   '#if %s || defined(SETTINGS_DEF_STRINGS_PASS)\n' % (
                       gopen.replace('\n', ' '), cond)) + row + '\n#endif'
            enum_rows.append((T, 'S_%s%s_H' % (k, m2 or '') in _h_used if False else ('S_%s_H' % k in _h_used or 'S_%s_NS_H' % k in _h_used), []))
        else:
            row = gopen + '\n' + row + '\n' + gclose
            enum_rows.append((T, 'S_%s_H' % k in _h_used or 'S_%s_NS_H' % k in _h_used, list(gs)))
    else:
        enum_rows.append((T, 'S_%s_H' % k in _h_used or 'S_%s_NS_H' % k in _h_used, []))
    out.append(row)
DEF = os.path.basename(DEF)  # includes are emitted relative to settings/
if _h_used:
    _pre = ['/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;',
            ' * outside the enum pass they behave exactly like the base row. */',
            '#ifndef SETTINGS_DEF_ENUM_PASS']
    for _al in sorted(_h_used):
        _pre += ['#ifndef %s' % _al, '#define %s %s' % (_al, _al[:-2]), '#endif']
    _pre.append('#endif')
    out.insert(1, '\n'.join(_pre))
open(os.path.join('settings', DEF), 'w').write('\n'.join(out) + '\n')

first = min(s for s, e in usspan)
for s, e in sorted(usspan, reverse=True): us = us[:s] + us[e:]
mk_us = lambda b, has_sub: (' \\\nMSG_HASH(MENU_ENUM_LABEL_VALUE_##T, us) \\\nMSG_HASH(MENU_ENUM_SUBLABEL_##T, sub)'
                            if has_sub else ' \\\nMSG_HASH(MENU_ENUM_LABEL_VALUE_##T, us)')
region = ('/* GENERATED REGION: %s (see %s). */\n' % (TITLE, DEF)
          + '#define SETTINGS_DEF_STRINGS_PASS\n'
          + defs(mk_us) + '\n#include "../settings/%s"\n' % DEF + UNDEFS + '\n#undef SETTINGS_DEF_STRINGS_PASS\n')
us = us[:first] + region + us[first:]
us = re.sub(r'#if[^\n]*\n#endif\n', '', us)
open('intl/msg_hash_us.h','w').write(us)
for s, e in sorted(lblstr_span, reverse=True): lblstr = lblstr[:s] + lblstr[e:]
open('msg_hash_lbl_str.h','w').write(lblstr)
for s, e in sorted(lbl_span, reverse=True): lbl = lbl[:s] + lbl[e:]
lbl = re.sub(r'#if[^\n]*\n#endif\n', '', lbl)
open('intl/msg_hash_lbl.h','w').write(lbl)
mc = open('intl/msg_hash_us.c').read()
assert mc.count('#include "../settings/settings_def_video_sync.h"') == 2
mc = mc.replace('#include "../settings/settings_def_video_sync.h"',
                '#include "../settings/settings_def_video_sync.h"\n#include "../settings/%s"' % DEF)
open('intl/msg_hash_us.c','w').write(mc)
assert cfg.count('#include "settings/settings_def_video_sync.h"') == 5
if '#define SETTINGS_DEF_CONFIG_PASS' not in cfg:
    cfg = cfg.replace('#include "settings/settings_def_video_sync.h"',
        '#define SETTINGS_DEF_CONFIG_PASS\n#include "settings/settings_def_video_sync.h"')
    cfg = cfg.replace('#undef S_BOOL\n', '#undef SETTINGS_DEF_CONFIG_PASS\n#undef S_BOOL\n')
if CFG_GROUP_GUARD:
    _cfg_inc = ('\n'.join(CFG_GROUP_GUARD) + '\n#include "settings/%s"\n' % DEF
                + '\n'.join('#endif' for _ in CFG_GROUP_GUARD))
else:
    _cfg_inc = '#include "settings/%s"' % DEF
cfg = cfg.replace('#include "settings/settings_def_video_sync.h"',
                  '#include "settings/settings_def_video_sync.h"\n' + _cfg_inc)
open('configuration.c','w').write(cfg)
ms = open('menu/menu_setting.c').read()
tm = re.search(r'static const setting_desc_t %s\[\] = \{\n(.*?)\n( *)\};' % TABLE, ms, re.S)
MENU_EMIT = {'S_BOOL_EX':'SDESC_BOOL_ROW_EX(f, T, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui),',
             'S_UINT_EX':'SDESC_UINT_ROW_EX(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),',
             'S_INT_EX':'SDESC_INT_ROW_EX(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),',
             'S_FLOAT_EX':'SDESC_FLOAT_ROW_EX(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui),',
             'S_ACTION_EX':'SDESC_ACTION_ROW_EX(T, sd, ok, rp, c),',
             'S_ACTION':'SDESC_ACTION_ROW(T),',
             'S_STRING_P':'SDESC_STRING_ROW_P(f, T, d, sd, c, ok, rp, sta, sel, lf, rt, ui),',
             'S_PATH':'SDESC_PATH_ROW(f, T, d, sd, c, vals, rp, ui),',
             'S_PATH_DS':'SDESC_PATH_ROW_DS(f, T, df2, sd, c, vals, rp, ui),',
             'S_DIR':'SDESC_DIR_ROW(f, T, d, el, sd, c, sta),',
             'S_STRING':'SDESC_STRING_ROW(f, T, d, sd, c, ok, rp, sta, sel, lf, rt, ui),',
             'S_BOOL':'SDESC_BOOL_ROW(f, T, d, sd, df, c),',
             'S_UINT':'SDESC_UINT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),',
             'S_INT': 'SDESC_INT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),',
             'S_FLOAT':'SDESC_FLOAT_ROW(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp),'}
mk_menu = lambda b, _s: ' \\\n                  ' + MENU_EMIT[b]
new_body = ('/* GENERATED: rows come from %s in order. */\n' % DEF
            + defs(mk_menu) + '\n#include "../settings/%s"\n' % DEF + UNDEFS)

ms = ms[:tm.start(1)] + new_body + ms[tm.end(1):]
open('menu/menu_setting.c','w').write(ms)
print("surgeries ok")

CFB = "-std=gnu89 -pedantic-errors -Wno-long-long -Wno-overlength-strings -O2 -fsyntax-only -I. -Imenu -Ilibretro-common/include -Ideps/7zip -Ideps/rcheevos/include -I/tmp/settings_mig_stubs -DRARCH_INTERNAL -DHAVE_MENU"
CF = CFB + " -DHAVE_CONFIGFILE -DHAVE_PATCH -DHAVE_REWIND -DHAVE_SCREENSHOTS -DHAVE_CHEATS -DHAVE_OVERLAY -DHAVE_MICROPHONE"
# Guard-isolation lanes: exercise every flag that gates a row *in
# isolation*.  A dropped inner guard nested under an outer flag only
# breaks when the outer flag is set and the inner is not, so each gating
# flag must be compiled on its own - the eject-disc break (HAVE_CDROM
# set, HAVE_LAKKA unset) slipped through only because no base lane
# defined HAVE_CDROM by itself.
def _lakka(flags):
    return (flags + ' -DHAVE_LAKKA_SERVER=\\"x\\" -DHAVE_LAKKA_PROJECT=\\"x\\"'
            if 'HAVE_LAKKA ' in flags + ' ' else flags)
_gflags = sorted({tok for gs in guards.values() for g in gs
                  for tok in __import__('re').findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', g)
                  if tok != 'defined'})
_iso = []
for _fl in _gflags:
    _d = ' ' + _couple('-D' + _fl)
    # menu_setting.c carries pre-existing -pedantic noise under HAVE_LAKKA
    # (systemd_service_toggle static initializer); the base LAKKA lane
    # already tolerates it with ('0','4'), so match that here rather than
    # abort a legitimate migration that happens to gate a row on LAKKA.
    _mok = ('0', '4') if 'HAVE_LAKKA' in _fl else (('0', '2') if '_3DS' in _fl else ('0',))
    _iso += [('menu/menu_setting.c', CF + _d, _mok),
             ('intl/msg_hash_us.c', CF + ' -DHAVE_LANGEXTRA' + _d, ('0',)),
             ('configuration.c', CF + _lakka(_d), ('0',))]
_LANES = [('menu/menu_setting.c', CFB, ('0',)), ('menu/menu_setting.c', CF, ('0',)),
        ('menu/menu_setting.c', CF + ' -DHAVE_D3DKMT', ('0',)),
        ('menu/menu_setting.c', CF + ' -DRARCH_MOBILE -DHAVE_LIBRETRODB -DHAVE_GLSL -DHAVE_NETWORKING -DHAVE_LAKKA -DHAVE_AUDIOMIXER -DHAVE_QT -DHAVE_THREADS', ('0','4')),
        ('configuration.c', CF, ('0',)), ('intl/msg_hash_us.c', CF + ' -DHAVE_LANGEXTRA', ('0',))]
# Headless lanes: configuration.c and the string tables are compiled in
# HAVE_MENU-less builds too (menu_setting.c is not - it is gated on
# HAVE_MENU_COMMON in Makefile.common).  Menu-visibility rows and their
# DEFAULT_* live under #ifdef HAVE_MENU, so a migration that lands a menu
# group's config-pass include OUTSIDE that guard breaks the headless
# build.  CFB/CF always define HAVE_MENU, so without this no lane sees it
# - which is exactly how the menu_show_* groups broke the ASan headless
# CI lane.
CF_HL = CF.replace(' -DHAVE_MENU', '')
_headless = [('configuration.c', CF_HL, ('0',)),
             ('intl/msg_hash_us.c', CF_HL + ' -DHAVE_LANGEXTRA', ('0',))]
for tu, cfgf, okset in _LANES + _iso + _headless:
    r = run("gcc %s %s 2>&1 | grep -c 'error:'" % (cfgf, tu))
    assert r.stdout.strip() in okset, (tu, cfgf, run("gcc %s %s 2>&1 | grep -B1 error: | head -5" % (cfgf, tu)).stdout)
print("gate: lanes clean (%d base + %d guard-isolation + %d headless)" % (
    len(_LANES), len(_iso), len(_headless)))

# ---- TOKEN-STREAM GATE: preprocessor-level emission identity ----
CPP = "gcc -E -P -I. -Imenu -Ilibretro-common/include -Ideps/7zip -Ideps/rcheevos/include -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_PATCH -DHAVE_REWIND -DHAVE_SCREENSHOTS -DHAVE_CHEATS -DHAVE_OVERLAY -DHAVE_MICROPHONE"
def table_tokens(src_path, extra=""):
    """Preprocessed initializer token stream, or None when the table's
    enclosing guards eliminate it in this polarity."""
    # Self-healing lane environment: external dependencies (mist.h for
    # Steam builds) are not in-tree, and -E only needs the header to
    # exist. Missing headers are stubbed empty, bounded, with a notice;
    # an empty preprocessor output must be a table fact, never a lane
    # environment defect.
    os.makedirs('/tmp/settings_mig_stubs', exist_ok=True)
    for _ in range(6):
        r = run("%s -I/tmp/settings_mig_stubs %s %s 2>&1" % (CPP, extra, src_path))
        fe = re.search(r"fatal error: ([\w./-]+\.h): No such file", r.stdout + r.stderr)
        if not fe:
            break
        stub = os.path.join('/tmp/settings_mig_stubs', fe.group(1))
        os.makedirs(os.path.dirname(stub) or '/tmp/settings_mig_stubs', exist_ok=True)
        assert not os.path.exists(stub), ("stub loop", fe.group(1))
        open(stub, 'w').write('/* empty stub for the token gate */\n')
        print("  note: stubbed external header %s for the token gate" % fe.group(1))
    m2 = re.search(r'static const setting_desc_t %s\[\] = \{(.*?)\};' % TABLE, r.stdout, re.S)
    if not m2:
        return None
    return re.sub(r'\s+', ' ', m2.group(1)).strip()
run("git show HEAD:menu/menu_setting.c > /tmp/ms_head.c")
# Polarities are derived from the guards in and around the table: the
# enclosing stack (a whole table can live under a platform guard) plus
# every per-row guard. Each flag gets a lane; the table may legitimately
# not exist in some polarity, but pre and post must agree about that,
# and at least one polarity must see it.
_head = open('/tmp/ms_head.c').read()
_tm = re.search(r'static const setting_desc_t %s\[\] = \{' % TABLE, _head)
_gtoks = set()
for _g in guard_at(_head, _tm.start()):
    _gtoks |= set(re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', _g)) - {'defined'}
for _gs in guards.values():
    for _g in (_gs if isinstance(_gs, (list, tuple)) else [_gs]):
        _gtoks |= set(re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', str(_g))) - {'defined'}
pols = [""] + sorted(_couple("-D%s" % g) for g in _gtoks)
if len(_gtoks) > 1:
    pols.append(_couple(' '.join(sorted("-D%s" % g for g in _gtoks))))
seen_present = False
for pol in pols:
    pre_t = table_tokens('/tmp/ms_head.c', pol)
    post_t = table_tokens('menu/menu_setting.c', pol)
    assert (pre_t is None) == (post_t is None), \
        "TABLE EXISTENCE DRIFT (%s): pre=%s post=%s" % (pol, pre_t is not None, post_t is not None)
    if pre_t is None:
        continue
    seen_present = True
    if post_t.endswith(',') and not pre_t.endswith(','):
        post_t = post_t[:-1].rstrip()
    assert pre_t == post_t, "TOKEN DRIFT (%s):\npre:  %s\npost: %s" % (pol, pre_t[:300], post_t[:300])
assert seen_present, "table not visible in any derived polarity"
print("gate: preprocessed table token-identical across %d derived polarities" % len(pols))

msn = open('menu/menu_setting.c').read()
for m in re.finditer(r'static const setting_desc_t \w+\[\] = \{\n(.*?)\n *\};', msn, re.S):
    depth = 0; has_top = False
    for ln in m.group(1).split('\n'):
        s = ln.strip()
        assert s != ',', "lone comma"
        if s.startswith('#if') and not s.startswith('#include'): depth += 1
        elif s.startswith('#endif'): depth -= 1
        elif s.startswith('SDESC_') and depth == 0: has_top = True
    assert has_top or '#include' in m.group(1), "empty-risk table"
print("gate: structural audits clean")
r = run("cd intl && python3 h2json.py msg_hash_us.h")
assert r.returncode == 0, r.stderr[-200:]
new = json.load(open('intl/msg_hash_us.json'))
run("rm -rf /tmp/base && mkdir -p /tmp/base/intl /tmp/base/settings")
run("git show HEAD:intl/msg_hash_us.h > /tmp/base/intl/msg_hash_us.h; git show HEAD:intl/h2json.py > /tmp/base/intl/h2json.py")
for df in run("git ls-tree -r HEAD --name-only | grep '^settings/settings_def_'").stdout.split():
    run("git show HEAD:%s > /tmp/base/%s" % (df, df))
run("cd /tmp/base/intl && python3 h2json.py msg_hash_us.h")
basej = json.load(open('/tmp/base/intl/msg_hash_us.json'))
_miss = sorted(set(basej) - set(new)); _extra = sorted(set(new) - set(basej))
_diff = sorted(x for x in set(basej) & set(new) if basej[x] != new[x])
assert not _miss and not _extra and not _diff, (
    'us.json drift: missing=%s extra=%s changed=%s' % (_miss[:4], _extra[:4],
    [(k, basej[k][:50], new[k][:50]) for k in _diff[:2]]))
os.remove('intl/msg_hash_us.json')
print("gate: us.json exactly equal (%d keys)" % len(new))
MH = "msg_hash.c intl/msg_hash_us.c libretro-common/string/stdstring.c libretro-common/compat/compat_strl.c libretro-common/encodings/encoding_utf.c"
B = "-O2 -I. -Ilibretro-common/include -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_LANGEXTRA"
r = run("gcc %s %s /tmp/stubs.c /tmp/test_msg_hash.c -o /tmp/tmh_g && /tmp/tmh_g > /tmp/out_g1.txt" % (B, MH))
assert os.path.getsize('/tmp/out_g1.txt') > 10**6, r.stderr[-200:]
def clean_objs():
    for f in ('intl/msg_hash_us','menu/menu_setting','configuration'):
        for ext in ('.o','.d'):
            p = 'obj-unix/release/%s%s' % (f, ext)
            if os.path.exists(p): os.remove(p)
def build_dump(out):
    clean_objs(); run("rm -f ./retroarch"); run("nice make -j%d" % os.cpu_count())
    assert os.path.exists('./retroarch'), "BUILD FAILED: " + run("make 2>&1 | tail -2").stdout
    run("HOME=/tmp/ra_home RETROARCH_SETTINGS_DUMP=%s timeout 8 ./retroarch --config /tmp/ra.cfg" % out)
    assert os.path.getsize(out) > 10000
build_dump('/tmp/dump_g.txt')
assert run('git diff --quiet').returncode != 0, "VACUOUS"
r = run('git stash -u'); assert 'Saved' in r.stdout
try:
    build_dump('/tmp/dump_gpre.txt')
    # Per-run 244K baseline from the parent revision, inside the stash
    # window: the integrated enum stage renumbers on every migration,
    # so a session baseline goes stale after each commit.
    r = run("gcc %s %s /tmp/stubs.c /tmp/test_msg_hash.c -o /tmp/tmh_p && /tmp/tmh_p > /tmp/out_gpre.txt" % (B, MH))
    assert os.path.getsize('/tmp/out_gpre.txt') > 10**6, r.stderr[-200:]
finally:
    assert run('git stash pop').returncode == 0
assert run('cmp -s /tmp/out_gpre.txt /tmp/out_g1.txt').returncode == 0, \
    run('diff /tmp/out_gpre.txt /tmp/out_g1.txt | head -4').stdout
print("gate: 244K lookup byte-identical (per-run parent baseline)")
assert os.path.exists(os.path.join('settings', DEF)), "def file lost"
assert run('cmp -s /tmp/dump_gpre.txt /tmp/dump_g.txt').returncode == 0, run('diff /tmp/dump_gpre.txt /tmp/dump_g.txt | head -4').stdout
print("gate: settings dump byte-identical")

# ---- ENUM STAGE: consolidate this group's enum rows in msg_hash.h ----
# Marker-based insertion: the first removed row is replaced by a unique
# marker BEFORE the husk cleanup, so no offset can go stale - the class
# of defect that spliced the ANDROID overlay member.
r = run("python3 tools/msg_hash_name_harness.py /tmp/enum_pre.txt")
assert r.returncode == 0, r.stderr[-300:]
mh = open('msg_hash.h').read()

def _plains(text):
    b = re.search(r'enum msg_hash_enums\n\{\n(.*?)\n\};', text, re.S).group(1)
    return set(mm.group(1) for ln in b.split('\n')
               for mm in [re.match(r'([A-Za-z_]\w*)\s*(?:=[^,]*)?,', ln.strip())] if mm)
_census_pre = _plains(mh)

_enum_ok = True
for _T, _h, _eg in enum_rows:
    _form = 'MENU_LBL_H' if _h else 'MENU_LABEL'
    _line = '   %s(%s),\n' % (_form, _T)
    if mh.count(_line) != 1:
        print("  enum stage skipped: %s row count %d" % (_T, mh.count(_line)))
        _enum_ok = False; break
    _stack = []
    for _gl in re.finditer(r'^[ \t]*#(if\w*[^\n]*|else|endif)', mh[:mh.index(_line)], re.M):
        _s = _gl.group(1)
        if _s.startswith('if'): _stack.append('#' + _s.strip())
        elif _s.startswith('endif') and _stack: _stack.pop()
    _et = set(x for g in _stack if '__MSG_HASH_H' not in g
              for x in re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', g) if x != 'defined')
    _dt = set(x for g in _eg for x in re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', g) if x != 'defined')
    if _et != _dt:
        print("  enum stage skipped: %s guard mismatch enum=%s def=%s" % (_T, sorted(_et), sorted(_dt)))
        _enum_ok = False; break

if _enum_ok:
    MARK = '/*__SETTINGS_DEF_ENUM_MARK__*/'
    _first = True
    for _T, _h, _eg in enum_rows:
        _line = '   %s(%s),\n' % ('MENU_LBL_H' if _h else 'MENU_LABEL', _T)
        mh = mh.replace(_line, MARK + '\n' if _first else '', 1)
        _first = False
    mh = re.sub(r'#if[^\n]*\n(?:%s\n)?#endif\n' % re.escape(MARK),
                lambda m: MARK + '\n' if MARK in m.group(0) else '', mh)
    _ar = dict(re.findall(r'#define (S_\w+)\(([^)]*)\)', open('intl/msg_hash_us.c').read()))
    _d = ['   /* GENERATED REGION: %s enum rows (see settings/%s). */' % (TITLE, DEF),
          '#define SETTINGS_DEF_ENUM_PASS',
          '#define SETTINGS_DEF_STRINGS_PASS']
    for _n in ('S_BOOL','S_BOOL_NS','S_UINT','S_UINT_NS','S_INT','S_INT_NS','S_FLOAT','S_FLOAT_NS','S_STRING','S_STRING_NS','S_DIR','S_DIR_NS','S_STRING_P','S_STRING_P_NS','S_PATH','S_PATH_NS','S_PATH_DS','S_PATH_DS_NS','S_ACTION','S_ACTION_NS','S_BOOL_EX','S_BOOL_EX_NS','S_UINT_EX','S_UINT_EX_NS','S_INT_EX','S_INT_EX_NS','S_FLOAT_EX','S_FLOAT_EX_NS','S_ACTION_EX','S_ACTION_EX_NS'):
        _d.append('#define %s(%s) MENU_LABEL(T),' % (_n, _ar[_n]))
    for _hn, _b in (('S_BOOL_H','S_BOOL'), ('S_UINT_H','S_UINT'), ('S_BOOL_NS_H','S_BOOL_NS'),
                    ('S_INT_H','S_INT'), ('S_FLOAT_H','S_FLOAT'),
                    ('S_UINT_NS_H','S_UINT_NS'), ('S_INT_NS_H','S_INT_NS'), ('S_FLOAT_NS_H','S_FLOAT_NS'),
                    ('S_STRING_H','S_STRING'), ('S_STRING_NS_H','S_STRING_NS'),
                    ('S_DIR_H','S_DIR'), ('S_DIR_NS_H','S_DIR_NS'),
                    ('S_STRING_P_H','S_STRING_P'), ('S_STRING_P_NS_H','S_STRING_P_NS'),
                    ('S_PATH_H','S_PATH'), ('S_PATH_NS_H','S_PATH_NS'),
                    ('S_PATH_DS_H','S_PATH_DS'), ('S_PATH_DS_NS_H','S_PATH_DS_NS'),
                    ('S_ACTION_H','S_ACTION'), ('S_ACTION_NS_H','S_ACTION_NS'),
                    ('S_BOOL_EX_H','S_BOOL_EX'), ('S_BOOL_EX_NS_H','S_BOOL_EX_NS'),
                    ('S_UINT_EX_H','S_UINT_EX'), ('S_UINT_EX_NS_H','S_UINT_EX_NS'),
                    ('S_INT_EX_H','S_INT_EX'), ('S_INT_EX_NS_H','S_INT_EX_NS'),
                    ('S_FLOAT_EX_H','S_FLOAT_EX'), ('S_FLOAT_EX_NS_H','S_FLOAT_EX_NS'),
                    ('S_ACTION_EX_H','S_ACTION_EX'), ('S_ACTION_EX_NS_H','S_ACTION_EX_NS')):
        _d.append('#define %s(%s) MENU_LBL_H(T),' % (_hn, _ar[_b]))
    _d.append('#include "settings/%s"' % DEF)
    for _n in ('S_BOOL','S_BOOL_NS','S_UINT','S_UINT_NS','S_INT','S_INT_NS','S_FLOAT','S_FLOAT_NS',
               'S_STRING','S_STRING_NS','S_STRING_H','S_STRING_NS_H',
               'S_DIR','S_DIR_NS','S_DIR_H','S_DIR_NS_H',
               'S_STRING_P','S_STRING_P_NS','S_STRING_P_H','S_STRING_P_NS_H',
               'S_PATH','S_PATH_NS','S_PATH_H','S_PATH_NS_H',
               'S_PATH_DS','S_PATH_DS_NS','S_PATH_DS_H','S_PATH_DS_NS_H',
               'S_ACTION','S_ACTION_NS','S_ACTION_H','S_ACTION_NS_H',
               'S_BOOL_EX','S_BOOL_EX_NS','S_BOOL_EX_H','S_BOOL_EX_NS_H',
               'S_UINT_EX','S_UINT_EX_NS','S_UINT_EX_H','S_UINT_EX_NS_H',
               'S_INT_EX','S_INT_EX_NS','S_INT_EX_H','S_INT_EX_NS_H',
               'S_FLOAT_EX','S_FLOAT_EX_NS','S_FLOAT_EX_H','S_FLOAT_EX_NS_H',
               'S_ACTION_EX','S_ACTION_EX_NS','S_ACTION_EX_H','S_ACTION_EX_NS_H',
               'S_BOOL_H','S_UINT_H','S_BOOL_NS_H','S_INT_H','S_FLOAT_H','S_UINT_NS_H','S_INT_NS_H','S_FLOAT_NS_H',
               'SETTINGS_DEF_STRINGS_PASS','SETTINGS_DEF_ENUM_PASS'):
        _d.append('#undef %s' % _n)
    assert mh.count(MARK) == 1
    mh = mh.replace(MARK + '\n', '\n'.join(_d) + '\n')
    open('msg_hash.h','w').write(mh)
    assert _plains(open('msg_hash.h').read()) == _census_pre, "enum census drift"
    r = run("python3 tools/msg_hash_name_harness.py /tmp/enum_post.txt")
    assert r.returncode == 0, r.stderr[-300:]
    if run('cmp -s /tmp/enum_pre.txt /tmp/enum_post.txt').returncode != 0:
        # A token whose strings were first introduced by this migration's
        # census rows resolves to null in the mid-run pre dump (the def is
        # not yet included anywhere in msg_hash.h) and to its own name
        # after the enum stage. That exact transition, for this
        # migration's tokens only, is the definition of correct.
        _mine = set(names[_t2] for _k2, _f2, _t2, _a2 in rows)
        _pre = open('/tmp/enum_pre.txt').read().split('\n')
        _post = open('/tmp/enum_post.txt').read().split('\n')
        assert len(_pre) == len(_post), 'name dump row count changed'
        for _lp, _lq in zip(_pre, _post):
            if _lp == _lq:
                continue
            _pp, _qq = _lp.split('|'), _lq.split('|')
            assert (_pp[:2] == _qq[:2] and _pp[2] == 'null'
                    and _qq[2] in _mine), (_lp, _lq)
    _body = re.search(r'enum msg_hash_enums\n\{\n(.*?)\n\};', mh, re.S).group(1)
    _toks = set(x for g in re.findall(r'^#if\w*([^\n]*)', _body, re.M)
                for x in re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', g) if x != 'defined')
    import glob as _glob
    for _df in _glob.glob('settings/settings_def_*.h'):
        for g in re.findall(r'^#if\w*([^\n]*)', open(_df).read(), re.M):
            _toks |= set(x for x in re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', g)
                         if x != 'defined' and 'SETTINGS_DEF' not in x)
    open('/tmp/mh_tu.c','w').write('#include <stdint.h>\n#include "msg_hash.h"\nint main(void){return (int)MSG_LAST;}\n')
    _b = "gcc -std=gnu89 -fsyntax-only -I. -Ilibretro-common/include -DRARCH_INTERNAL"
    _lanes = [''] + [_couple('-D%s' % x) for x in sorted(_toks)] + [_couple(' '.join('-D%s' % x for x in sorted(_toks)))]
    for _lane in _lanes:
        r = run("%s %s /tmp/mh_tu.c 2>&1 | grep -c error:" % (_b, _lane))
        assert r.stdout.strip() == '0', (_lane[:60],)
    print("gate: enum stage - name dump byte-identical, census exact, %d polarity lanes clean" % len(_lanes))

print("PIPELINE_OK")
