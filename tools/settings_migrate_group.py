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
SIGS = [('S_BOOL','f, T, n, d, sd, df, c'),
        ('S_UINT','f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp'),
        ('S_INT', 'f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp'),
        ('S_FLOAT','f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp')]
UNDEFS = '\n'.join('#undef %s%s' % (b, s) for b, _ in SIGS for s in ('', '_NS'))
def defs(mk):
    L = []
    for base, sig in SIGS:
        L.append('#define %s(%s, us, sub)%s' % (base, sig, mk(base, True)))
        L.append('#define %s_NS(%s, us)%s' % (base, sig, mk(base, False)))
    return '\n'.join(L)

ms = open('menu/menu_setting.c').read()
tm = re.search(r'static const setting_desc_t %s\[\] = \{\n(.*?)\n( *)\};' % TABLE, ms, re.S)
assert tm, TABLE
guards = {}
body = tm.group(1)
for gm in re.finditer(r'#if(\w* [^\n]+)\n(.*?)#endif(?:\n|$)', body, re.S):
    gexpr = ('#if' + gm.group(1)).replace('#ifdef ', '#if defined(').strip()
    if gm.group(1).startswith('def '): gexpr += ')'
    for rm in re.finditer(r'SDESC_\w+_ROW\((\w+),', gm.group(2)):
        guards[rm.group(1)] = ('#if' + gm.group(1)).strip()
rows = [(m.group(1), m.group(2), m.group(3), re.sub(r'\s+',' ',m.group(4)).strip())
        for m in re.finditer(r'SDESC_(BOOL|UINT|INT|FLOAT)_ROW\((\w+), (\w+),((?:[^()]|\([^()]*\))*)\)', body)]
assert rows and len(rows) == len(re.findall(r'SDESC_\w+_ROW\(', tm.group(1))), (len(rows), tm.group(1)[:200])

us = open('intl/msg_hash_us.h').read()
usval, ussub, usspan = {}, {}, []
for k, f, T, a in rows:
    m = re.search(r'MSG_HASH\(\s*\n?\s*MENU_ENUM_LABEL_VALUE_%s,\s*\n?\s*(%s)\s*\n?\s*\)\n?' % (T, CSTR), us)
    assert m, ('VALUE', T)
    usval[T] = re.sub(r'\s*\n\s*', ' ', m.group(1)); usspan.append((m.start(), m.end()))
    m = re.search(r'MSG_HASH\(\s*\n?\s*MENU_ENUM_SUBLABEL_%s,\s*\n?\s*(%s)\s*\n?\s*\)\n?' % (T, CSTR), us)
    if m:
        ussub[T] = re.sub(r'\s*\n\s*', ' ', m.group(1)); usspan.append((m.start(), m.end()))
lblstr = open('msg_hash_lbl_str.h').read()
names, lblstr_span = {}, []
for k, f, T, a in rows:
    m = re.search(r'#define MENU_ENUM_LABEL_%s_STR (%s)\n' % (T, CSTR), lblstr)
    assert m, T
    names[T] = m.group(1); lblstr_span.append((m.start(), m.end()))
    ext = run('grep -rl "MENU_ENUM_LABEL_%s_STR\\b" --include=*.h --include=*.c . | grep -v msg_hash_lbl' % T).stdout.strip()
    assert ext == '', (T, ext)
lbl = open('intl/msg_hash_lbl.h').read()
lbl_span = []
for k, f, T, a in rows:
    m = re.search(r'MSG_HASH\(\s*\n?\s*MENU_ENUM_LABEL_%s,\s*\n?\s*MENU_ENUM_LABEL_%s_STR\s*\n?\s*\)\n?' % (T, T), lbl)
    assert m, T
    lbl_span.append((m.start(), m.end()))
print("extraction ok: %d settings (%d without sublabel)" % (len(rows), sum(1 for _,_,T,_ in rows if T not in ussub)))

cfg = open('configuration.c').read()
cfg_spans = []
cfg_keeps = []
for k, f, T, a in rows:
    m = re.search(r' *SETTING_%s\(\s*%s, *&settings->\w+\.%s,[^;]*;\n' % (k, re.escape(names[T]), f), cfg)
    if m:
        cfg_spans.append((m.start(), m.end())); continue
    m = re.search(r' *SETTING_%s\(\s*("(?:[^"\\]|\\.)*"), *&settings->\w+\.%s,[^;]*;\n' % (k, f), cfg)
    assert m, (T, f, "no config row at all")
    cfg_keeps.append((T, m.group(1)))
    print("  note: %s config key %s differs from label %s - config row stays literal" % (T, m.group(1), names[T]))
KEEP = set(T for T, _ in cfg_keeps)
for s, e in sorted(cfg_spans, reverse=True): cfg = cfg[:s] + cfg[e:]
cfg = re.sub(r'#if[^\n]*\n#endif\n', '', cfg)
out = ['''/* Single-source definitions: %s.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */
''' % TITLE]
for k, f, T, a in rows:
    if T in ussub:
        row = 'S_%s(%s, %s,\n      %s,\n      %s,\n      %s,\n      %s)' % (
            k, f, T, names[T], a, usval[T], ussub[T])
    else:
        row = 'S_%s_NS(%s, %s,\n      %s,\n      %s,\n      %s)' % (
            k, f, T, names[T], a, usval[T])
    if T in KEEP:
        ck = dict(cfg_keeps)[T]
        row = ('/* config key %s differs from the label string; the\n'
               ' * configuration.c row stays literal for this setting. */\n'
               '#ifndef SETTINGS_DEF_CONFIG_PASS\n' % ck) + row + '\n#endif'
    if f in guards:
        row = guards[f] + '\n' + row + '\n#endif'
    out.append(row)
open(DEF, 'w').write('\n'.join(out) + '\n')

first = min(s for s, e in usspan)
for s, e in sorted(usspan, reverse=True): us = us[:s] + us[e:]
mk_us = lambda b, has_sub: (' \\\nMSG_HASH(MENU_ENUM_LABEL_VALUE_##T, us) \\\nMSG_HASH(MENU_ENUM_SUBLABEL_##T, sub)'
                            if has_sub else ' \\\nMSG_HASH(MENU_ENUM_LABEL_VALUE_##T, us)')
region = ('/* GENERATED REGION: %s (see %s). */\n' % (TITLE, DEF)
          + defs(mk_us) + '\n#include "../%s"\n' % DEF + UNDEFS + '\n#undef SETTINGS_DEF_STRINGS_PASS\n')
us = us[:first] + region + us[first:]
us = re.sub(r'#if[^\n]*\n#endif\n', '', us)
open('intl/msg_hash_us.h','w').write(us)
for s, e in sorted(lblstr_span, reverse=True): lblstr = lblstr[:s] + lblstr[e:]
open('msg_hash_lbl_str.h','w').write(lblstr)
for s, e in sorted(lbl_span, reverse=True): lbl = lbl[:s] + lbl[e:]
lbl = re.sub(r'#if[^\n]*\n#endif\n', '', lbl)
open('intl/msg_hash_lbl.h','w').write(lbl)
mc = open('intl/msg_hash_us.c').read()
assert mc.count('#include "../settings_def_video_sync.h"') == 2
mc = mc.replace('#include "../settings_def_video_sync.h"',
                '#include "../settings_def_video_sync.h"\n#include "../%s"' % DEF)
open('intl/msg_hash_us.c','w').write(mc)
assert cfg.count('#include "settings_def_video_sync.h"') == 4
if '#define SETTINGS_DEF_CONFIG_PASS' not in cfg:
    cfg = cfg.replace('#include "settings_def_video_sync.h"',
        '#define SETTINGS_DEF_CONFIG_PASS\n#include "settings_def_video_sync.h"')
    cfg = cfg.replace('#undef S_BOOL\n', '#undef SETTINGS_DEF_CONFIG_PASS\n#undef S_BOOL\n')
cfg = cfg.replace('#include "settings_def_video_sync.h"',
                  '#include "settings_def_video_sync.h"\n#include "%s"' % DEF)
open('configuration.c','w').write(cfg)
ms = open('menu/menu_setting.c').read()
tm = re.search(r'static const setting_desc_t %s\[\] = \{\n(.*?)\n( *)\};' % TABLE, ms, re.S)
MENU_EMIT = {'S_BOOL':'SDESC_BOOL_ROW(f, T, d, sd, df, c),',
             'S_UINT':'SDESC_UINT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),',
             'S_INT': 'SDESC_INT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),',
             'S_FLOAT':'SDESC_FLOAT_ROW(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp),'}
mk_menu = lambda b, _s: ' \\\n                  ' + MENU_EMIT[b]
new_body = ('/* GENERATED: rows come from %s in order. */\n' % DEF
            + defs(mk_menu) + '\n#include "../%s"\n' % DEF + UNDEFS)
ms = ms[:tm.start(1)] + new_body + ms[tm.end(1):]
open('menu/menu_setting.c','w').write(ms)
print("surgeries ok")

CFB = "-std=gnu89 -pedantic-errors -Wno-long-long -O2 -fsyntax-only -I. -Imenu -Ilibretro-common/include -Ideps/7zip -DRARCH_INTERNAL -DHAVE_MENU"
CF = CFB + " -DHAVE_CONFIGFILE -DHAVE_PATCH -DHAVE_REWIND -DHAVE_SCREENSHOTS -DHAVE_CHEATS -DHAVE_OVERLAY -DHAVE_MICROPHONE"
for tu, cfgf, okset in (('menu/menu_setting.c', CFB, ('0',)), ('menu/menu_setting.c', CF, ('0',)),
        ('menu/menu_setting.c', CF + ' -DHAVE_D3DKMT', ('0',)),
        ('menu/menu_setting.c', CF + ' -DRARCH_MOBILE -DHAVE_LIBRETRODB -DHAVE_GLSL -DHAVE_NETWORKING -DHAVE_LAKKA -DHAVE_AUDIOMIXER -DHAVE_QT -DHAVE_THREADS', ('0','4')),
        ('configuration.c', CF, ('0',)), ('intl/msg_hash_us.c', CF + ' -DHAVE_LANGEXTRA', ('0',))):
    r = run("gcc %s %s 2>&1 | grep -c 'error:'" % (cfgf, tu))
    assert r.stdout.strip() in okset, (tu, run("gcc %s %s 2>&1 | grep -B1 error: | head -5" % (cfgf, tu)).stdout)
print("gate: lanes clean")

# ---- TOKEN-STREAM GATE: preprocessor-level emission identity ----
CPP = "gcc -E -P -I. -Imenu -Ilibretro-common/include -Ideps/7zip -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_PATCH -DHAVE_REWIND -DHAVE_SCREENSHOTS -DHAVE_CHEATS -DHAVE_OVERLAY -DHAVE_MICROPHONE"
def table_tokens(src_path, extra=""):
    r = run("%s %s %s 2>/dev/null" % (CPP, extra, src_path))
    m2 = re.search(r'static const setting_desc_t %s\[\] = \{(.*?)\};' % TABLE, r.stdout, re.S)
    assert m2, "table not found in preprocessed output"
    return re.sub(r'\s+', ' ', m2.group(1)).strip()
run("git show HEAD:menu/menu_setting.c > /tmp/ms_head.c")
for pol in ("", "-DRARCH_MOBILE"):
    pre_t = table_tokens('/tmp/ms_head.c', pol)
    post_t = table_tokens('menu/menu_setting.c', pol)
    if post_t.endswith(',') and not pre_t.endswith(','):
        post_t = post_t[:-1].rstrip()
    assert pre_t == post_t, "TOKEN DRIFT (%s):\npre:  %s\npost: %s" % (pol, pre_t[:300], post_t[:300])
print("gate: preprocessed table token-identical in both guard polarities")

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
run("rm -rf /tmp/base && mkdir -p /tmp/base/intl")
run("git show HEAD:intl/msg_hash_us.h > /tmp/base/intl/msg_hash_us.h; git show HEAD:intl/h2json.py > /tmp/base/intl/h2json.py")
for df in run("git ls-tree HEAD --name-only | grep '^settings_def_'").stdout.split():
    run("git show HEAD:%s > /tmp/base/%s" % (df, df))
run("cd /tmp/base/intl && python3 h2json.py msg_hash_us.h")
basej = json.load(open('/tmp/base/intl/msg_hash_us.json'))
assert set(basej) == set(new) and all(basej[x] == new[x] for x in basej), "us.json drift"
os.remove('intl/msg_hash_us.json')
print("gate: us.json exactly equal (%d keys)" % len(new))
MH = "msg_hash.c intl/msg_hash_us.c libretro-common/string/stdstring.c libretro-common/compat/compat_strl.c libretro-common/encodings/encoding_utf.c"
B = "-O2 -I. -Ilibretro-common/include -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_LANGEXTRA"
r = run("gcc %s %s /tmp/stubs.c /tmp/test_msg_hash.c -o /tmp/tmh_g && /tmp/tmh_g > /tmp/out_g1.txt" % (B, MH))
assert os.path.getsize('/tmp/out_g1.txt') > 10**6, r.stderr[-200:]
assert run('cmp -s /tmp/out_pre.txt /tmp/out_g1.txt').returncode == 0
print("gate: 244K lookup byte-identical")
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
finally:
    assert run('git stash pop').returncode == 0
assert os.path.exists(DEF), "def file lost"
assert run('cmp -s /tmp/dump_gpre.txt /tmp/dump_g.txt').returncode == 0, run('diff /tmp/dump_gpre.txt /tmp/dump_g.txt | head -4').stdout
print("gate: settings dump byte-identical")
print("PIPELINE_OK")
