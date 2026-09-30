#!/usr/bin/env python3
"""Check that every command in command.h describes itself.

The command tables in command.h are the one list of what RetroArch
can be told to do. HELP prints them, and interfaces that hand the
commands to a client (tool lists, key binding help) are generated from
them, so a command added without a description reaches all of those
as a blank. This fails on any row of action_map or map that has no
description, an empty one, or an unknown flag.

Rows are matched as C initializers:
  action_map: { "NAME", handler, "arg", "description", flags },
  map:        { "NAME", ID, "description", flags },
"""
import re
import sys

HEADER = 'command.h'
FLAGS  = {'0', 'CMD_INFO_READ_ONLY', 'CMD_INFO_DESTRUCTIVE'}

ACTION_ROW = re.compile(
    r'\{\s*"([A-Z0-9_]+)"\s*,\s*(\w+)\s*,\s*"([^"]*)"\s*'
    r'(?:,\s*"((?:[^"\\]|\\.)*)"\s*)?(?:,\s*([\w|\s]+?)\s*)?\}')
HOTKEY_ROW = re.compile(
    r'\{\s*"([A-Z0-9_]+)"\s*,\s*([A-Z0-9_]+)\s*'
    r'(?:,\s*"((?:[^"\\]|\\.)*)"\s*)?(?:,\s*([\w|\s]+?)\s*)?\}')


def table(text, head):
    start = text.find(head)
    if start < 0:
        return None
    return text[start:text.index('};', start)]


def check_flags(name, flags, bad):
    parts = [p.strip() for p in (flags or '').split('|')]
    if not flags or any(p not in FLAGS for p in parts):
        bad.append('%s: flags %r are not a combination of %s'
                   % (name, flags, ', '.join(sorted(FLAGS))))


def main():
    text = open(HEADER, encoding='utf-8').read()
    bad = []
    count = 0

    actions = table(text, 'struct cmd_action_map action_map[] = {')
    hotkeys = table(text, 'struct cmd_map map[] = {')
    if actions is None or hotkeys is None:
        print('command_table_check: tables not found in %s' % HEADER)
        return 1

    for m in ACTION_ROW.finditer(actions):
        name, _, _, desc, flags = m.groups()
        count += 1
        if not desc or not desc.strip():
            bad.append('%s: no description' % name)
        check_flags(name, flags, bad)
    for m in HOTKEY_ROW.finditer(hotkeys):
        name, _, desc, flags = m.groups()
        count += 1
        if not desc or not desc.strip():
            bad.append('%s: no description' % name)
        check_flags(name, flags, bad)

    if bad:
        print('command_table_check: every command in %s needs a '
              'description and flags:' % HEADER)
        for line in bad:
            print('  ' + line)
        return 1
    print('ok:   %d commands in %s all describe themselves' % (count, HEADER))
    return 0


if __name__ == '__main__':
    sys.exit(main())
