#!/usr/bin/env python3
"""Self-test for tools/settings_migrate_group.py.

Builds a miniature but real-shaped source tree in a scratch directory
and runs the migrator over it with --stop-after-emit, then asserts the
emitted def file and the surgeries encode the three defect classes that
produced the July settings regressions:

  1. default_enable == false (state_slot class): the hand-written
     configuration row must be kept and the generated row excluded from
     the configuration pass, never re-emitted with a default applied.
  2. DEFAULT_* macro spelling (menu_linear_filter class): a descriptor
     row that transcribed the macro as a literal must come out spelled
     as the macro when the configuration row carries it; a genuine
     value divergence (cloud sync class) must abort, not pick a side.
  3. Guard breadth (audio_block_frames class): a configuration row
     registered more broadly than its descriptor row must stay visible
     to the configuration pass outside the descriptor's guard, and a
     configuration row with its own extra guard must keep it.

Run from the repository root:
    python3 tools/settings_migrate_selftest.py
Exit status 0 on success. Pure text fixtures plus one gcc -E probe;
runs in seconds with no build tree.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
TOOL = os.path.join(REPO, 'tools', 'settings_migrate_group.py')


def W(root, rel, text):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as handle:
        handle.write(text)


def fixture(root):
    """A tree with exactly the shape the migrator's extraction and
    surgery stages read: one descriptor table of four healthy-plus-
    defective rows, the matching string/label/config rows, and the
    anchor lines the surgeries key on."""
    W(root, 'config.def.h', '#define DEFAULT_SELFTEST_SMOOTH true\n')

    W(root, 'menu/menu_setting.c', '''\
static const setting_desc_t selftest_desc[] = {
   SDESC_BOOL_ROW(selftest_plain, SELFTEST_PLAIN, true, SD_FLAG_NONE, 0, 0),
   SDESC_INT_ROW(selftest_noslot, SELFTEST_NOSLOT, 0, SD_FLAG_NONE, 0, 0, -1, 999, 1, 0, NULL, NULL),
   SDESC_BOOL_ROW(selftest_macro, SELFTEST_MACRO, true, SD_FLAG_NONE, 0, 0),
#ifdef RARCH_MOBILE
   SDESC_UINT_ROW(selftest_block, SELFTEST_BLOCK, 0, SD_FLAG_ADVANCED, 0, 0, 0, 0, 0, 0, NULL, NULL),
#endif
#ifdef HAVE_ODROIDGO2
   SDESC_BOOL_ROW(selftest_scaling, SELFTEST_SCALING, false, SD_FLAG_NONE, 0, 0),
#endif
   SDESC_BOOL_ROW(selftest_gamepad, SELFTEST_GAMEPAD, false, SD_FLAG_NONE, 0, 0),
};
''')

    us_rows = []
    for tok, val, sub in (
            ('SELFTEST_PLAIN', 'Plain', 'A healthy row.'),
            ('SELFTEST_NOSLOT', 'No Slot', 'Carries no default.'),
            ('SELFTEST_MACRO', 'Macro', 'Default is a macro.'),
            ('SELFTEST_BLOCK', 'Block', 'Guarded descriptor.'),
            ('SELFTEST_GAMEPAD', 'Gamepad', 'Config-only guard.')):
        us_rows.append('MSG_HASH(\n   MENU_ENUM_LABEL_VALUE_%s,\n   "%s"\n   )\n'
                       % (tok, val))
        us_rows.append('MSG_HASH(\n   MENU_ENUM_SUBLABEL_%s,\n   "%s"\n   )\n'
                       % (tok, sub))
    us_rows.append('#ifdef HAVE_ODROIDGO2\n'
                   'MSG_HASH(\n   MENU_ENUM_LABEL_VALUE_SELFTEST_SCALING,\n'
                   '   "Scaling"\n   )\n'
                   'MSG_HASH(\n   MENU_ENUM_SUBLABEL_SELFTEST_SCALING,\n'
                   '   "Guarded strings."\n   )\n'
                   '#endif\n')
    W(root, 'intl/msg_hash_us.h', ''.join(us_rows) + '\n')

    W(root, 'msg_hash_lbl_str.h', ''.join(
        '#define MENU_ENUM_LABEL_%s_STR "%s"\n' % (tok, key)
        for tok, key in (('SELFTEST_PLAIN', 'selftest_plain'),
                         ('SELFTEST_NOSLOT', 'selftest_noslot'),
                         ('SELFTEST_MACRO', 'selftest_macro'),
                         ('SELFTEST_BLOCK', 'selftest_block'),
                         ('SELFTEST_GAMEPAD', 'selftest_gamepad'),
                         ('SELFTEST_SCALING', 'selftest_scaling'))))

    W(root, 'intl/msg_hash_lbl.h', ''.join(
        'MSG_HASH(\n   MENU_ENUM_LABEL_%s,\n   MENU_ENUM_LABEL_%s_STR\n   )\n'
        % (tok, tok)
        for tok in ('SELFTEST_PLAIN', 'SELFTEST_NOSLOT', 'SELFTEST_MACRO',
                    'SELFTEST_BLOCK', 'SELFTEST_GAMEPAD'))
      + '#ifdef HAVE_ODROIDGO2\n'
        'MSG_HASH(\n   MENU_ENUM_LABEL_SELFTEST_SCALING,\n'
        '   MENU_ENUM_LABEL_SELFTEST_SCALING_STR\n   )\n'
        '#endif\n')

    anchors = '\n'.join('#include "settings/settings_def_video_sync.h"'
                        for _ in range(5))
    W(root, 'configuration.c', '''\
%s
   SETTING_BOOL("selftest_plain",            &settings->bools.selftest_plain, true, true, false);
   SETTING_INT("selftest_noslot",            &settings->ints.selftest_noslot, false, 0, false);
   SETTING_BOOL("selftest_macro",            &settings->bools.selftest_macro, true, DEFAULT_SELFTEST_SMOOTH, false);
   SETTING_UINT("selftest_block",            &settings->uints.selftest_block, true, 0, false);
#ifdef HAVE_NETWORKGAMEPAD
   SETTING_BOOL("selftest_gamepad",          &settings->bools.selftest_gamepad, true, false, false);
#endif
   SETTING_BOOL("selftest_scaling",          &settings->bools.selftest_scaling, true, false, false);
#undef S_BOOL
''' % anchors)

    W(root, 'msg_hash.h', '''\
enum msg_hash_enums
{
   MENU_LABEL(SELFTEST_PLAIN),
   MENU_LABEL(SELFTEST_NOSLOT),
   MENU_LABEL(SELFTEST_MACRO),
   MENU_LABEL(SELFTEST_BLOCK),
   MENU_LABEL(SELFTEST_GAMEPAD),
   MENU_LABEL(SELFTEST_SCALING),
};
''')

    W(root, 'intl/msg_hash_us.c',
      '#include "../settings/settings_def_video_sync.h"\n'
      '#include "../settings/settings_def_video_sync.h"\n')

    os.makedirs(os.path.join(root, 'settings'), exist_ok=True)
    W(root, 'settings/settings_def_video_sync.h', '/* anchor */\n')


def run_tool(root, expect_ok, defname='settings_def_selftest.h'):
    proc = subprocess.run(
        [sys.executable, TOOL, 'selftest_desc', defname,
         'selftest group', '--stop-after-emit'],
        cwd=root, capture_output=True, text=True)
    ok = proc.returncode == 0 and 'EMIT_OK' in proc.stdout
    if ok != expect_ok:
        sys.stderr.write(proc.stdout[-2000:] + '\n' + proc.stderr[-2000:])
        raise AssertionError('tool %s unexpectedly'
                             % ('succeeded' if ok else 'failed'))
    # Abort messages surface through the AssertionError traceback on
    # stderr; return both streams so callers can match either.
    return proc.stdout + proc.stderr


def main():
    failures = 0

    # ---------------- positive run: the three fixes ----------------
    root = tempfile.mkdtemp(prefix='settings_migrate_selftest_')
    try:
        fixture(root)
        out = run_tool(root, expect_ok=True)
        d = open(os.path.join(root, 'settings',
                              'settings_def_selftest.h')).read()
        cfg = open(os.path.join(root, 'configuration.c')).read()

        # Class 1: no-default row keeps its literal config row and the
        # generated row is excluded from the configuration pass.
        assert 'SETTING_INT("selftest_noslot"' in cfg, \
            'no-default config row was consumed'
        m = re.search(
            r'#ifndef SETTINGS_DEF_CONFIG_PASS\n'
            r'S_INT\(selftest_noslot,', d)
        assert m, 'no-default def row is not excluded from the config pass'
        assert 'carries no default' in out

        # Class 2: the macro spelling from the config row replaces the
        # descriptor's literal transcription.
        assert re.search(r'S_BOOL\(selftest_macro, SELFTEST_MACRO,\n'
                         r'      "selftest_macro",\n'
                         r'      DEFAULT_SELFTEST_SMOOTH,', d), \
            'macro default spelling was not restored'
        assert 'macro spelling kept' in out

        # Healthy row: consumed config row, plain emission, literal
        # default preserved verbatim.
        assert 'SETTING_BOOL("selftest_plain"' not in cfg
        assert re.search(r'S_BOOL\(selftest_plain, SELFTEST_PLAIN,\n'
                         r'      "selftest_plain",\n'
                         r'      true,', d)

        # Class 3a: descriptor guarded RARCH_MOBILE, config row
        # unguarded - the config pass must escape the descriptor guard.
        m = re.search(r'#if defined\(RARCH_MOBILE\)[^\n]*\n'
                      r'S_UINT\(selftest_block,', d)
        assert m, 'guarded-descriptor row missing'
        head = d[:d.index('S_UINT(selftest_block,')]
        gline = head[head.rfind('#if '):].split('\n')[0]
        assert 'SETTINGS_DEF_CONFIG_PASS' in gline, \
            'config pass does not escape the narrowed descriptor guard: %s' % gline
        assert 'SETTING_UINT("selftest_block"' not in cfg

        # Class 3b: config row carried its own HAVE_NETWORKGAMEPAD
        # guard - it must survive as a config-pass-only condition.
        assert re.search(
            r'#if !defined\(SETTINGS_DEF_CONFIG_PASS\) \|\| '
            r'\(defined\(HAVE_NETWORKGAMEPAD\)\)\n'
            r'S_BOOL\(selftest_gamepad,', d), \
            'config-only guard was dropped from the config pass'
        assert 'SETTING_BOOL("selftest_gamepad"' not in cfg

        # Class 3a': same narrowing but with GUARDED strings - the
        # branch that previously wrapped everything in the raw guard
        # stack. The config pass must escape here too.
        m = re.search(r'#if [^\n]*HAVE_ODROIDGO2[^\n]*\n'
                      r'S_BOOL\(selftest_scaling,', d)
        assert m, 'guarded-strings row missing'
        gline = m.group(0).split('\n')[0]
        assert 'SETTINGS_DEF_CONFIG_PASS' in gline, \
            'strings-guarded branch still narrows the config pass: %s' % gline
        assert 'SETTINGS_DEF_STRINGS_PASS' not in gline, \
            'strings-guarded row must not use the strings-always form'
        assert 'SETTING_BOOL("selftest_scaling"' not in cfg

        print('selftest: emission fixes hold (no-default, macro '
              'spelling, both guard directions)')
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # ------------- negative run: genuine value divergence -------------
    root = tempfile.mkdtemp(prefix='settings_migrate_selftest_')
    try:
        fixture(root)
        p = os.path.join(root, 'configuration.c')
        src = open(p).read()
        src = src.replace(
            'SETTING_BOOL("selftest_plain",            '
            '&settings->bools.selftest_plain, true, true, false);',
            'SETTING_BOOL("selftest_plain",            '
            '&settings->bools.selftest_plain, true, false, false);')
        open(p, 'w').write(src)
        out = run_tool(root, expect_ok=False)
        assert 'DEFAULT DIVERGENCE' in out, \
            'divergent defaults did not abort with the divergence message'
        print('selftest: genuine default divergence aborts loudly')
    finally:
        shutil.rmtree(root, ignore_errors=True)

    print('SELFTEST_OK')
    return failures


if __name__ == '__main__':
    sys.exit(main())
