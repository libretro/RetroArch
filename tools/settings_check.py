#!/usr/bin/env python3
"""Regression checks for the single-source settings definitions.

Two checks, both preprocessor/front-end only, no configured build tree:

table  - preprocess configuration.c for several platform profiles, extract
         every (kind, ident, default_enable, default) the five
         populate_settings_* tables emit, and diff against a committed
         reference. A def row whose guard is too narrow, whose default was
         transcribed wrongly, or which lost its no-default registration
         produces no compile diagnostic; this is the only thing that
         notices.

unity  - compile the settings translation units concatenated in
         griffin/griffin.c order with -Werror, the way the MSVC griffin
         build compiles them. Macro leakage between msg_hash.h, the def
         files and configuration.c only manifests in this concatenation;
         per-file builds cannot see it.

Usage:
  tools/settings_check.py table --check tools/settings_table.reference
  tools/settings_check.py table --write tools/settings_table.reference
  tools/settings_check.py unity
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

KINDS = ('bool', 'int', 'uint', 'float', 'size')

COMMON = [
    'RARCH_INTERNAL', 'HAVE_MENU', 'HAVE_XMB', 'HAVE_OZONE', 'HAVE_RGUI',
    'HAVE_MATERIALUI', 'HAVE_NETWORKING', 'HAVE_NETPLAY', 'HAVE_NETWORK_CMD',
    'HAVE_NETWORKGAMEPAD', 'HAVE_COMMAND', 'HAVE_CHEEVOS', 'HAVE_OVERLAY',
    'HAVE_TRANSLATE', 'HAVE_LANGEXTRA', 'HAVE_MICROPHONE', 'HAVE_CLOUDSYNC',
    'HAVE_SMBCLIENT', 'HAVE_IMAGEVIEWER', 'HAVE_CRTSWITCHRES',
    'HAVE_ACCESSIBILITY', 'HAVE_GFX_WIDGETS', 'HAVE_THREADS',
    'HAVE_AUDIOMIXER', 'HAVE_REWIND', 'HAVE_RUNAHEAD', 'HAVE_SCREENSHOTS',
    'HAVE_BLUETOOTH', 'HAVE_WIFI', 'HAVE_BSV_MOVIE', 'HAVE_CONFIGFILE',
    'HAVE_PATCH', 'HAVE_CHEATS', 'HAVE_ZLIB', 'HAVE_COMPRESSION',
    'HAVE_UPDATE_CORES', 'HAVE_LIBRETRODB', 'HAVE_DYNAMIC',
    'HAVE_SHADERPIPELINE', 'HAVE_GLSL', 'HAVE_SLANG', 'HAVE_OPENGL',
    'HAVE_VIDEO_LAYOUT', 'HAVE_STB_FONT', 'HAVE_CC_RESAMPLER',
]

PROFILES = {
    'desktop': ['HAVE_VULKAN', 'HAVE_QT', 'HAVE_DISCORD'],
    'android': ['ANDROID', 'RARCH_MOBILE', 'HAVE_VULKAN'],
    'console': ['RARCH_CONSOLE'],
}

INCLUDES = [
    '.', 'libretro-common/include', 'deps', 'deps/stb',
    'deps/rcheevos/include',
]

# The settings slice of griffin/griffin.c, in griffin's inclusion order.
# These four are the consumers of the settings/ def files; the collision
# class this guards against (macro leakage across them) needs exactly this
# concatenation to show up.
UNITY_TUS = [
    'configuration.c',
    'msg_hash.c',
    'intl/msg_hash_us.c',
    'menu/menu_setting.c',
]

UNITY_EXTRA = [
    'HAVE_STDIN_CMD', 'HAVE_RWAV', 'HAVE_ONLINE_UPDATER',
    'HAVE_UPDATE_ASSETS', 'HAVE_CORE_INFO_CACHE', 'HAVE_DSP_FILTER',
    'HAVE_VIDEO_FILTER', 'HAVE_7ZIP', 'HAVE_RPNG', 'HAVE_RJPEG',
    'HAVE_RBMP', 'HAVE_RTGA',
]

# The unity check compiles at -Werror, so it gets its own profile set:
# ANDROID is replaced by bare RARCH_MOBILE because configuration.c's
# '#ifdef ANDROID' accessibility block calls is_screen_reader_enabled()
# without a visible prototype (declared in frontend/drivers/
# platform_unix.h, which configuration.c does not include) - a
# pre-existing condition unrelated to the settings definitions this
# check guards. The macro-collision class it detects is
# platform-independent, so mobile coverage of the def-file guards is
# what matters, not the ANDROID spelling.
UNITY_PROFILES = {
    'desktop': ['HAVE_VULKAN', 'HAVE_QT', 'HAVE_DISCORD'],
    'mobile':  ['RARCH_MOBILE', 'HAVE_VULKAN'],
    'console': ['RARCH_CONSOLE'],
}

ROW = re.compile(
    r'tmp\[count\]\.ident\s*=\s*"([^"]*)";\s*'
    r'tmp\[count\]\.ptr\s*=\s*([^;]*);\s*'
    r'if\s*\((\d+)\)\s*\{\s*tmp\[count\]\.flags[^;]*;\s*'
    r'tmp\[count\]\.def\s*=\s*([^;]*);')

BARE = re.compile(r'tmp\[count\]\.ident\s*=\s*"([^"]*)"')


def gcc_args(root, defines):
    argv = ['gcc', '-std=gnu99']
    argv += ['-D' + d for d in defines]
    for inc in INCLUDES:
        argv += ['-I', os.path.join(root, inc)]
    return argv


def preprocess(root, defines):
    argv = gcc_args(root, COMMON + defines) + [
        '-E', '-P', os.path.join(root, 'configuration.c')]
    proc = subprocess.run(argv, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        sys.exit('preprocessing configuration.c failed')
    return proc.stdout


def extract(text):
    rows = []
    for kind in KINDS:
        start = text.find('*populate_settings_%s(' % kind)
        if start < 0:
            continue
        end = text.find('*size = count;', start)
        body = text[start:end]
        defs = {}
        for m in ROW.finditer(body):
            defs[m.group(1)] = (m.group(3), ' '.join(m.group(4).split()))
        for ident in BARE.findall(body):
            enable, value = defs.get(ident, ('0', '-'))
            rows.append((kind, ident, enable, value))
    return rows


def render(root):
    out = []
    for profile in sorted(PROFILES):
        out.append('[%s]' % profile)
        for row in extract(preprocess(root, PROFILES[profile])):
            out.append('\t'.join(row))
    return '\n'.join(out) + '\n'


def cmd_table(args, root):
    current = render(root)
    if args.write:
        with open(args.write, 'w') as handle:
            handle.write(current)
        print('wrote %s' % args.write)
        return 0
    with open(args.check) as handle:
        reference = handle.read()
    if reference == current:
        print('settings tables match %s' % args.check)
        return 0
    import difflib
    sys.stdout.writelines(difflib.unified_diff(
        reference.splitlines(True), current.splitlines(True),
        fromfile='reference', tofile='current'))
    sys.stderr.write(
        '\nThe emitted configuration tables changed. If intended, '
        'regenerate:\n  tools/settings_check.py table --write %s\n'
        % args.check)
    return 1


def cmd_unity(args, root):
    src = ['/* Settings slice of griffin/griffin.c; same inclusion order.',
           ' * Generated by tools/settings_check.py, not committed. */']
    for tu in UNITY_TUS:
        src.append('#include "%s"' % os.path.join(root, tu))
    with tempfile.NamedTemporaryFile(
            'w', suffix='.c', prefix='settings_unity_',
            delete=False) as handle:
        handle.write('\n'.join(src) + '\n')
        path = handle.name
    try:
        failures = 0
        for profile in sorted(UNITY_PROFILES):
            defines = COMMON + UNITY_EXTRA + UNITY_PROFILES[profile]
            # -Werror promotes macro-redefinition (and every other)
            # warning in these TUs to a failure; the normal build is
            # warning-clean here, so anything new is a regression.
            argv = gcc_args(root, defines) + [
                '-fsyntax-only', '-Werror', path]
            proc = subprocess.run(argv, capture_output=True, text=True)
            if proc.returncode != 0:
                sys.stderr.write('[%s]\n%s' % (profile, proc.stderr))
                failures += 1
            else:
                print('unity [%s]: clean' % profile)
        return 1 if failures else 0
    finally:
        os.unlink(path)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    t = sub.add_parser('table')
    t.add_argument('--write', metavar='FILE')
    t.add_argument('--check', metavar='FILE')
    sub.add_parser('unity')
    ap.add_argument('--root', default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..'))
    args = ap.parse_args()
    root = os.path.normpath(args.root)
    if args.cmd == 'table':
        if not args.write and not args.check:
            sys.exit('table: pass --write FILE or --check FILE')
        return cmd_table(args, root)
    return cmd_unity(args, root)


if __name__ == '__main__':
    sys.exit(main())
