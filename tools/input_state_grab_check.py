#!/usr/bin/env python3
"""Input drivers ask the frontend by name; they do not take its state.

An input driver, a joypad driver or one of their helpers that needs
something from the rest of RetroArch used to take the whole state it
was in and pick the field out:

    if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
    input_state_get_ptr()->flags |= INP_FLAG_NATIVE_KB_SHOWN;
    config_get_ptr()->uints.input_mouse_index[port]

That ties every driver to the layout of the settings, the run loop,
the menu and the input frontend, and nothing says what a driver may
read or change. They are being replaced, call by call, with a named
question or statement on the side that owns the state:

    if (menu_driver_alive())
    input_driver_set_native_keyboard_shown(true);

What is counted, textually, in every file under input/ except the
frontend itself (input/input_driver.c, its header, and
input/input_driver_choice.c):

    config_get_ptr()         input_state_get_ptr()
    runloop_state_get_ptr()  menu_state_get_ptr()
    video_driver_get_ptr()   video_state_get_ptr()

One file is the frontend's for one of them: the replay code in
input/bsv keeps its state in the input state, so it takes that and
nothing else (OWN_STATE).

ALLOWED below is what is still to be converted, file by file. A file
may not have more than its number, and a file that is not listed may
have none. When a file gets below its number the check fails too, and
says so: the number is lowered in the same change, so it cannot creep
back up. The aim is an empty list.

Usage:
   tools/input_state_grab_check.py [--root DIR] [--selftest]
"""

import argparse
import os
import re
import sys
import tempfile

GRAB = re.compile(r'\b(config_get_ptr|input_state_get_ptr|runloop_state_get_ptr'
                  r'|menu_state_get_ptr|video_driver_get_ptr|video_state_get_ptr)\s*\(\s*\)')

# the frontend: it owns the input state, and reads the settings for it
# (input_driver_choice.c is where it picks the input driver by name)
FRONTEND = ('input/input_driver.c', 'input/input_driver.h',
            'input/input_driver_choice.c')

# a file whose own state is kept in one of these: it may take that one
OWN_STATE = {
    'input/bsv/bsvmovie.c': 'input_state_get_ptr',
}

ALLOWED = {
    # settings read by a driver on a platform not converted yet
    'input/common/wayland_common_webos.c':               1,
    # saving the configuration when Android takes the application away:
    # frontend work that sits in the input driver, to be moved out of it
    'input/drivers/android_input.c':                     1,
    'input/drivers/cocoa_input.m':                       3,
    'input/drivers/gx_input.c':                          2,
    # the SDL2 video driver's window, for the grab
    'input/drivers/sdl2_input.c':                        1,
    # the touch options, and a driver that rewrites the settings
    'input/drivers/udev_input.c':                        4,
    'input/drivers_joypad/gx_joypad_libogc.c':           1,
    'input/drivers_joypad/mfi_joypad.m':                 1,
    'input/drivers_joypad/psp_joypad.c':                 1,
    'input/drivers_joypad/switch_joypad.c':              1,
}


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def count_tree(root):
    found = {}
    base = os.path.join(root, 'input')
    for d, _dirs, files in os.walk(base):
        for f in files:
            if not f.endswith(('.c', '.h', '.m', '.mm', '.cpp')):
                continue
            path = os.path.join(d, f)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            if rel in FRONTEND:
                continue
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            own = OWN_STATE.get(rel)
            n = len([g for g in GRAB.findall(strip_comments(text)) if g != own])
            if n:
                found[rel] = n
    return found


def run(root, allowed):
    found = count_tree(root)
    bad = 0
    for rel in sorted(set(found) | set(allowed)):
        have = found.get(rel, 0)
        may = allowed.get(rel, 0)
        if have > may:
            print('%s: takes a state pointer %d time(s), %d allowed.\n'
                  '  Ask by name instead: a call on the side that owns the state '
                  '(input_driver_...(), menu_driver_alive(), runloop_get_flags()).' % (rel, have, may))
            bad += 1
        elif have < may:
            print('%s: down to %d, and the list in tools/input_state_grab_check.py still says %d.\n'
                  '  Lower the number%s.' % (rel, have, may, ' (remove the line)' if not have else ''))
            bad += 1
    left = sum(found.values())
    if bad:
        print('FAIL input_state_grab_check: %d file(s)' % bad)
        return 1
    print('PASS input_state_grab_check (%d state pointer(s) still taken under input/, in %d file(s); none new)'
          % (left, len(found)))
    return 0


def selftest():
    bad = 0
    with tempfile.TemporaryDirectory() as root:
        os.makedirs(os.path.join(root, 'input', 'drivers'))

        def put(name, text):
            with open(os.path.join(root, 'input', name), 'w') as f:
                f.write(text)
        put('input_driver.c', 'settings_t *s = config_get_ptr();\n')
        put('drivers/a_input.c', 'if (menu_driver_alive()) x();\n/* config_get_ptr() in a comment */\n')
        put('drivers/b_input.c', 'x = config_get_ptr ( )->uints.y;\ninput_state_get_ptr()->flags |= 1;\n')
        os.makedirs(os.path.join(root, 'input', 'bsv'))
        put('bsv/bsvmovie.c', 'input_driver_state_t *st = input_state_get_ptr();\n')
        cases = [({'input/drivers/b_input.c': 2}, 0, 'the listed count'),
                 ({'input/drivers/b_input.c': 1}, 1, 'one more than listed'),
                 ({}, 1, 'a file that is not listed'),
                 ({'input/drivers/b_input.c': 3}, 1, 'fewer than listed'),
                 ({'input/drivers/b_input.c': 2, 'input/drivers/a_input.c': 1}, 1, 'a listed file with none left')]
        import io
        import contextlib
        for allowed, want, what in cases:
            with contextlib.redirect_stdout(io.StringIO()):
                got = run(root, allowed)
            if got != want:
                print('selftest: %s: wanted %d, got %d' % (what, want, got))
                bad += 1
    if bad:
        print('FAIL input_state_grab_check --selftest')
        return 1
    print('PASS input_state_grab_check --selftest (%d cases)' % len(cases))
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()
    sys.exit(selftest() if args.selftest else run(args.root, ALLOWED))
