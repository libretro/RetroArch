#!/usr/bin/env python3
"""Dump and compare the configuration tables emitted by configuration.c.

configuration.c builds the bool/int/uint/float/size tables that
config_set_defaults(), config_load_file() and config_save_file() all walk,
by including the single-source row files in settings/ once per kind. A row
whose guard is too narrow, whose default was transcribed wrongly, or which
lost its default_enable flag produces no diagnostic at all: the setting
simply stops getting a default and stops being saved and loaded.

This script preprocesses configuration.c for a set of platform profiles,
extracts every emitted (ident, default_enable, default) triple in order and
either writes a reference table or compares against one. The profiles are
limited to the ones whose headers preprocess on a Linux host, which is
enough to catch guard, default and default_enable drift.

  tools/settings_table_check.py --write tools/settings_table.reference
  tools/settings_table_check.py --check tools/settings_table.reference

The comparison is intentionally strict: adding a setting is a diff too, and
the reference is meant to be regenerated in the same commit that adds one.
Only the preprocessor runs, so no configured build tree is needed.
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

ROW = re.compile(
    r'tmp\[count\]\.ident\s*=\s*"([^"]*)";\s*'
    r'tmp\[count\]\.ptr\s*=\s*([^;]*);\s*'
    r'if\s*\((\d+)\)\s*\{\s*tmp\[count\]\.flags[^;]*;\s*'
    r'tmp\[count\]\.def\s*=\s*([^;]*);')

BARE = re.compile(r'tmp\[count\]\.ident\s*=\s*"([^"]*)"')


def preprocess(root, defines):
    """Return the preprocessed text of configuration.c, or exit on failure."""
    argv = ['gcc', '-E', '-P', '-std=gnu99']
    argv += ['-D' + d for d in COMMON + defines]
    for inc in INCLUDES:
        argv += ['-I', os.path.join(root, inc)]
    argv += [os.path.join(root, 'configuration.c')]
    proc = subprocess.run(argv, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        sys.exit('preprocessing configuration.c failed')
    return proc.stdout


def extract(text):
    """Return [(kind, ident, default_enable, default)] in emission order."""
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
        for kind, ident, enable, value in extract(
                preprocess(root, PROFILES[profile])):
            out.append('%s\t%s\t%s\t%s' % (kind, ident, enable, value))
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--write', metavar='FILE')
    ap.add_argument('--check', metavar='FILE')
    ap.add_argument('--root', default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..'))
    args = ap.parse_args()

    if not args.write and not args.check:
        sys.exit('pass --write FILE or --check FILE')

    current = render(os.path.normpath(args.root))

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
        '\nThe emitted configuration tables changed. If this is intended, '
        'regenerate with:\n  tools/settings_table_check.py --write %s\n'
        % args.check)
    return 1


if __name__ == '__main__':
    sys.exit(main())
