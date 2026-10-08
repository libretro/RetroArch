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

ALLOWED below was what was still to be converted, file by file with a
count, and it is empty now: a file that is not listed may have none,
so nothing goes back in.

A second thing is counted, the other way round: code outside input/
calling an input driver's state function itself,

    current_input->input_state(input_st->current_data, joypad, ...)

which the menu did, fourteen times, with the driver, its data and the
joypads taken out of the input state. It asks the frontend now
(input_driver_device_state(), input_driver_bind_capture_state()).
The same goes for a joypad driver's functions,

    joypad->poll();   joypad->button(port, i);   joypad->name(pad)

which the capture of a bind, a driver restart and the saving of a
controller profile called (input_driver_capture_pad(),
input_driver_poll_devices(), input_driver_autoconfigure_pad()).
There is to be none of either.

A third: the input state taken by the menu and the UI layer
(OUTSIDE_ALLOWED), which are being moved onto the same named calls.
The menu drivers take none any more; what is left is listed with a
count that only goes down.

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

# a file whose own state is kept in one of these could take that one;
# there is none: the replay code in input/bsv, whose state is in the
# input state, is handed it by its callers
OWN_STATE = {
}

ALLOWED = {
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


# Frontend commands an input driver issued itself - quit, shut down,
# toggle the menu, restart video - which it asks for now with
# input_driver_platform_request(): there is to be none under input/
# but in the frontend.
COMMAND = re.compile(r'\b(command_event|retroarch_ctl|retroarch_main_quit)\s*\(')


# The menu reads the pointer from the view the frontend compiles once a
# poll (input_driver_pointer_view()) and calls no driver for it.
MENU_DEVICE_READ = re.compile(r'\binput_driver_device_state\s*\(')


def menu_device_reads(root):
    found = {}
    base = os.path.join(root, 'menu')
    for d, _dirs, files in os.walk(base):
        for f in files:
            if not f.endswith(('.c', '.m', '.cpp')):
                continue
            path = os.path.join(d, f)
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            n = len(MENU_DEVICE_READ.findall(strip_comments(text)))
            if n:
                found[os.path.relpath(path, root).replace(os.sep, '/')] = n
    return found


def command_calls(root):
    found = {}
    base = os.path.join(root, 'input')
    for d, _dirs, files in os.walk(base):
        for f in files:
            if not f.endswith(('.c', '.m', '.mm', '.cpp')):
                continue
            path = os.path.join(d, f)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            if rel in FRONTEND:
                continue
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            n = len(COMMAND.findall(strip_comments(text)))
            if n:
                found[rel] = n
    return found


DRIVER_CALL = re.compile(r'->\s*input_state\s*\('
                         r'|\b(?:sec_|primary_|secondary_)?joypad\s*->\s*[a-z_]+\s*\(')
# where an input driver's own functions are not looked for: the input
# layer itself, the tests, and code that is not RetroArch's
NOT_CALLERS = ('input/', 'samples/', 'deps/', 'libretro-common/', 'pkg/', '.git/')


def driver_calls(root):
    found = {}
    for d, dirs, files in os.walk(root):
        rel_d = os.path.relpath(d, root).replace(os.sep, '/') + '/'
        if rel_d.startswith(NOT_CALLERS):
            dirs[:] = []
            continue
        for f in files:
            if not f.endswith(('.c', '.m', '.mm', '.cpp')):
                continue
            path = os.path.join(d, f)
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            n = len(DRIVER_CALL.findall(strip_comments(text)))
            if n:
                found[os.path.relpath(path, root).replace(os.sep, '/')] = n
    return found


# The bind arrays are the frontend input file's own: input_driver.c and
# the header that declares them. Everything else - the input drivers
# too - gets at a bind through input_config_bind() and
# input_autoconf_bind(), and at a whole set through
# input_config_binds_copy_out() and _copy_in(), so how the binds are
# kept can change without a driver, the menu, the configuration or the
# UI changing with it.
BIND_ARRAY = re.compile(r'\binput_(?:config|autoconf)_binds\b(?!_)')
BIND_OWNERS = ('input/input_driver.c', 'input/input_driver.h')
BIND_NOT_CALLERS = tuple(d for d in NOT_CALLERS if d != 'input/')


def bind_array_uses(root):
    found = {}
    for d, dirs, files in os.walk(root):
        rel_d = os.path.relpath(d, root).replace(os.sep, '/') + '/'
        if rel_d.startswith(BIND_NOT_CALLERS):
            dirs[:] = []
            continue
        for f in files:
            if not f.endswith(('.c', '.h', '.m', '.mm', '.cpp')):
                continue
            path = os.path.join(d, f)
            if os.path.relpath(path, root).replace(os.sep, '/') in BIND_OWNERS:
                continue
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            n = len(BIND_ARRAY.findall(strip_comments(text)))
            if n:
                found[os.path.relpath(path, root).replace(os.sep, '/')] = n
    return found


# Which two binds are behind a stick's axis is the frontend's to know:
# every input driver used to work a stick out from the keys bound to
# it, each with the macro that maps the axis to its binds. That lives
# in input/input_driver.c now (INPUT_ANALOG_AXIS_BIND: the binds are
# numbered in a row, so it is arithmetic) and nothing else uses it, by
# its old name or its new one.
ANALOG_BIND_MACRO = re.compile(r'\b(?:input_conv_analog_id_to_bind_id|INPUT_ANALOG_AXIS_BIND)\b')


# An input driver is handed the binds with every question it is asked,
# and none reads them: the pad's buttons, the keys and the mouse
# buttons a control is bound to are the frontend's to put together
# (keys_down and bind_mouse_buttons in the driver table give it the
# keys and the mouse). So how the binds are kept - how many a user has,
# what a record holds - is no driver's business. A driver may pass the
# set on; it may not look inside.
HANDED_BIND_READ = re.compile(r'\b(?:binds|retro_keybinds)\s*\[')


def handed_bind_reads(root):
    found = {}
    d = os.path.join(root, 'input', 'drivers')
    if not os.path.isdir(d):
        return found
    for f in sorted(os.listdir(d)):
        if not f.endswith(('.c', '.h', '.m', '.mm', '.cpp')):
            continue
        path = os.path.join(d, f)
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        n = len(HANDED_BIND_READ.findall(strip_comments(text)))
        if n:
            found['input/drivers/' + f] = n
    return found


# A joypad driver, and an HID backend behind one, is handed no binds at
# all. Its state call gets two lists - the pad's button and the pad's
# axis behind each of the RetroPad's sixteen - that the frontend has
# resolved: the port's own bind, or its controller's profile's. So no
# file of theirs has a bind's record, a set of them or a profile's to
# name.
JOYPAD_BIND_NAME = re.compile(r'\b(?:retro_keybind|retro_keybind_set|binds|binds_data|auto_binds)\b')


def joypad_bind_names(root):
    found = {}
    for sub in ('drivers_joypad', 'drivers_hid'):
        top = os.path.join(root, 'input', sub)
        for d, _, files in sorted(os.walk(top)):
            for f in sorted(files):
                if not f.endswith(('.c', '.h', '.m', '.mm', '.cpp')):
                    continue
                path = os.path.join(d, f)
                try:
                    text = open(path, encoding='utf-8', errors='replace').read()
                except OSError:
                    continue
                n = len(JOYPAD_BIND_NAME.findall(strip_comments(text)))
                if n:
                    found[os.path.relpath(path, root).replace(os.sep, '/')] = n
    return found


# A user's remap of a control is set through
# input_config_set_remap_id(), which counts it as a change: what the
# frontend keeps of the remaps - who has anything remapped, which
# buttons are left as themselves - is made again after a counted change
# and not otherwise. A remap written into the settings directly would
# take effect whenever something else happened to be counted. The
# input's own files and the configuration's loader, which end with a
# counted call, are where the table is written.
REMAP_DIRECT_WRITE = re.compile(
    r'\binput_remap_ids\s*\[[^\]]*\]\s*\[[^\]]*\]\s*(?:[-+|&]?=)(?!=)')
REMAP_WRITERS = ('input/', 'configuration.c')


def remap_direct_writes(root):
    found = {}
    for d, dirs, files in os.walk(root):
        rel_d = os.path.relpath(d, root).replace(os.sep, '/')
        if rel_d == '.':
            dirs[:] = [x for x in dirs if x not in ('deps', 'samples', '.git')]
            rel_d = ''
        for f in sorted(files):
            if not f.endswith(('.c', '.h', '.m', '.mm', '.cpp')):
                continue
            rel = (rel_d + '/' + f) if rel_d else f
            if rel.startswith(REMAP_WRITERS[0]) or rel == REMAP_WRITERS[1]:
                continue
            try:
                text = open(os.path.join(d, f), encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            n = len(REMAP_DIRECT_WRITE.findall(strip_comments(text)))
            if n:
                found[rel] = n
    return found


def analog_bind_macro_uses(root):
    found = {}
    for d, dirs, files in os.walk(root):
        rel_d = os.path.relpath(d, root).replace(os.sep, '/') + '/'
        if rel_d.startswith(('samples/', 'deps/', 'libretro-common/', 'pkg/', '.git/')):
            dirs[:] = []
            continue
        for f in files:
            if not f.endswith(('.c', '.h', '.m', '.mm', '.cpp')):
                continue
            path = os.path.join(d, f)
            rel = os.path.relpath(path, root).replace(os.sep, '/')
            if rel == 'input/input_driver.c':
                continue
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            n = len(ANALOG_BIND_MACRO.findall(strip_comments(text)))
            if n:
                found[rel] = n
    return found


# The input state taken by the menu and the UI layer, which are being
# moved onto named calls as the drivers were: what is left, file by
# file. A file may not have more than its number, one that is not
# listed may have none, and a file below its number has it lowered.
#
# Nothing under menu/ or ui/ takes it any more: the Apple UI hands its
# mouse, pointer and touch events to the Cocoa input driver's own calls
# (input/drivers/cocoa_input.h), and does not write the driver's data.
#
# tasks/, network/ and the frontend files below too. What is left there
# is the replay code being handed the input state by the commands, the
# save-state and movie tasks and rewind, which is the frontend passing
# its own state on.
OUTSIDE_DIRS = ('menu', 'ui', 'tasks', 'network')
OUTSIDE_FILES = ('command.c', 'configuration.c', 'state_manager.c')
OUTSIDE_ALLOWED = {
    'command.c':           4,
    'state_manager.c':     3,
    'tasks/task_movie.c':  3,
    'tasks/task_save.c':   7,
}
INPUT_STATE = re.compile(r'\binput_state_get_ptr\s*\(\s*\)')


def count_outside(root):
    found = {}
    for rel in OUTSIDE_FILES:
        path = os.path.join(root, rel)
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        n = len(INPUT_STATE.findall(strip_comments(text)))
        if n:
            found[rel] = n
    for top in OUTSIDE_DIRS:
        for d, dirs, files in os.walk(os.path.join(root, top)):
            if 'test' in dirs:
                dirs.remove('test')
            for f in files:
                if not f.endswith(('.c', '.m', '.mm', '.cpp')):
                    continue
                path = os.path.join(d, f)
                try:
                    text = open(path, encoding='utf-8', errors='replace').read()
                except OSError:
                    continue
                n = len(INPUT_STATE.findall(strip_comments(text)))
                if n:
                    found[os.path.relpath(path, root).replace(os.sep, '/')] = n
    return found


def run(root, allowed, outside_allowed=None):
    found = count_tree(root)
    bad = 0
    outside = count_outside(root)
    if outside_allowed is None:
        outside_allowed = OUTSIDE_ALLOWED
    for rel in sorted(set(outside) | set(outside_allowed)):
        have = outside.get(rel, 0)
        may = outside_allowed.get(rel, 0)
        if have > may:
            print('%s: takes the input state %d time(s), %d allowed.\n'
                  '  Ask the frontend by name (input_driver_...()).' % (rel, have, may))
            bad += 1
        elif have < may:
            print('%s: down to %d, and OUTSIDE_ALLOWED in tools/input_state_grab_check.py still says %d.\n'
                  '  Lower the number%s.' % (rel, have, may, ' (remove the line)' if not have else ''))
            bad += 1
    for rel, n in sorted(menu_device_reads(root).items()):
        print('%s: reads the input driver for the pointer itself, %d time(s).\n'
              '  Read the view the frontend compiles once a poll: '
              'input_driver_pointer_view().' % (rel, n))
        bad += 1
    for rel, n in sorted(command_calls(root).items()):
        print('%s: issues a frontend command itself, %d time(s).\n'
              '  Ask for it: input_driver_platform_request().' % (rel, n))
        bad += 1
    for rel, n in sorted(driver_calls(root).items()):
        print('%s: calls an input or joypad driver itself, %d time(s).\n'
              '  Ask the frontend: input_driver_device_state(), '
              'input_driver_capture_pad(), input_driver_poll_devices().' % (rel, n))
        bad += 1
    for rel, n in sorted(analog_bind_macro_uses(root).items()):
        print('%s: maps a stick\'s axis to its binds itself, %d time(s).\n'
              '  The frontend answers a stick from the keys bound to it: give it\n'
              '  the keys (keys_down in the driver table).' % (rel, n))
        bad += 1
    for rel, n in sorted(remap_direct_writes(root).items()):
        print('%s: writes a remap into the settings directly, %d time(s).\n'
              '  Set it with input_config_set_remap_id(user, id, remap), which\n'
              '  counts the change; what the frontend keeps of the remaps is made\n'
              '  again only after a counted change.' % (rel, n))
        bad += 1
    for rel, n in sorted(joypad_bind_names(root).items()):
        print('%s: names binds, %d time(s).\n'
              '  A joypad driver is handed what is behind each of the RetroPad\'s\n'
              '  sixteen on its pad (joykeys, joyaxes in its state call), resolved\n'
              '  by the frontend; it has no binds to read or to pass on.' % (rel, n))
        bad += 1
    for rel, n in sorted(handed_bind_reads(root).items()):
        print('%s: reads the binds it is handed, %d time(s).\n'
              '  The frontend answers what is bound: give it the keys and the\n'
              '  mouse (keys_down, bind_mouse_buttons in the driver table).' % (rel, n))
        bad += 1
    for rel, n in sorted(bind_array_uses(root).items()):
        print('%s: names a bind array itself, %d time(s).\n'
              '  Get at a bind through input_config_bind() or '
              'input_autoconf_bind().' % (rel, n))
        bad += 1
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
    print('PASS input_state_grab_check (under input/: %d state pointer(s) taken; '
          'outside it: the input state taken %d time(s) in %d file(s), none new)'
          % (left, sum(outside.values()), len(outside)))
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
        cases = [({'input/drivers/b_input.c': 2}, 0, 'the listed count'),
                 ({'input/drivers/b_input.c': 1}, 1, 'one more than listed'),
                 ({}, 1, 'a file that is not listed'),
                 ('menu', 1, 'a menu file calling the driver'),
                 ({'input/drivers/b_input.c': 3}, 1, 'fewer than listed'),
                 ({'input/drivers/b_input.c': 2, 'input/drivers/a_input.c': 1}, 1, 'a listed file with none left')]
        import io
        import contextlib
        for allowed, want, what in cases:
            if allowed == 'menu':
                os.makedirs(os.path.join(root, 'menu'))
                with open(os.path.join(root, 'menu', 'm.c'), 'w') as f:
                    f.write('x = current_input->input_state(data, 0);\n')
                allowed = {'input/drivers/b_input.c': 2}
            with contextlib.redirect_stdout(io.StringIO()):
                got = run(root, allowed, {})
            if what.startswith('a menu file'):
                os.remove(os.path.join(root, 'menu', 'm.c'))
            if got != want:
                print('selftest: %s: wanted %d, got %d' % (what, want, got))
                bad += 1
        # the menu and UI lists: a file over its number, and one under it
        os.makedirs(os.path.join(root, 'ui'))
        with open(os.path.join(root, 'ui', 'u.c'), 'w') as f:
            f.write('a = input_state_get_ptr()->flags;\nb = input_state_get_ptr();\n')
        for outside, want, what in (({'ui/u.c': 2}, 0, 'a UI file at its number'),
                                    ({'ui/u.c': 1}, 1, 'a UI file over its number'),
                                    ({'ui/u.c': 3}, 1, 'a UI file under its number'),
                                    ({}, 1, 'a UI file that is not listed')):
            with contextlib.redirect_stdout(io.StringIO()):
                got = run(root, {'input/drivers/b_input.c': 2}, outside)
            if got != want:
                print('selftest: %s: wanted %d, got %d' % (what, want, got))
                bad += 1
        # the menu reading the driver for the pointer itself
        os.makedirs(os.path.join(root, 'menu'), exist_ok=True)
        with open(os.path.join(root, 'menu', 'm.c'), 'w') as f:
            f.write('int m(void) { return input_driver_device_state(0, 2, 0, 0); }\n')
        if menu_device_reads(root).get('menu/m.c') != 1:
            print('selftest: the menu reading the driver for the pointer was not found')
            bad += 1
        os.remove(os.path.join(root, 'menu', 'm.c'))
        # a driver issuing a frontend command itself, and one asking
        with open(os.path.join(root, 'input', 'drivers', 'cmd.c'), 'w') as f:
            f.write('void q(void) { command_event(CMD_EVENT_QUIT, 0); }\n')
        if command_calls(root).get('input/drivers/cmd.c') != 1:
            print('selftest: a driver issuing a frontend command was not found')
            bad += 1
        with open(os.path.join(root, 'input', 'drivers', 'cmd.c'), 'w') as f:
            f.write('void q(void) { input_driver_platform_request(INPUT_PLATFORM_QUIT); }\n')
        if command_calls(root):
            print('selftest: a driver asking for a quit was taken for one issuing it')
            bad += 1
        # code outside input/ naming a bind array, and going through the call
        os.makedirs(os.path.join(root, 'menu'), exist_ok=True)
        with open(os.path.join(root, 'menu', 'b.c'), 'w') as f:
            f.write('x = input_config_binds[0][1].joykey;\n'
                    'y = &input_autoconf_binds[p][i];\n'
                    'input_config_binds_copy_out(sets); /* input_config_binds */\n')
        if bind_array_uses(root).get('menu/b.c') != 2:
            print('selftest: a menu file naming the bind arrays was not counted as two')
            bad += 1
        with open(os.path.join(root, 'menu', 'b.c'), 'w') as f:
            f.write('x = input_config_bind(0, 1)->joykey;\n'
                    'y = input_autoconf_bind(p, i);\n')
        if bind_array_uses(root):
            print('selftest: a menu file going through the calls was taken for one naming the arrays')
            bad += 1
        os.remove(os.path.join(root, 'menu', 'b.c'))
        # an input driver naming them is counted; the frontend's file is not
        with open(os.path.join(root, 'input', 'drivers', 'bn.c'), 'w') as f:
            f.write('x = input_autoconf_binds[0][1].joykey;\n')
        with open(os.path.join(root, 'input', 'input_driver.c'), 'a') as f:
            f.write('y = input_config_binds[0][1].joykey;\n')
        if bind_array_uses(root) != {'input/drivers/bn.c': 1}:
            print('selftest: an input driver naming the bind arrays was not the one file counted')
            bad += 1
        os.remove(os.path.join(root, 'input', 'drivers', 'bn.c'))
        # a driver working a stick out from its binds itself
        with open(os.path.join(root, 'input', 'drivers', 'an.c'), 'w') as f:
            f.write('input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);\n')
        with open(os.path.join(root, 'input', 'input_driver.c'), 'a') as f:
            f.write('input_conv_analog_id_to_bind_id(idx, id, a, b);\n')
        # a driver looking inside the binds it is handed; passing them on is not
        with open(os.path.join(root, 'input', 'drivers', 'hb.c'), 'w') as f:
            f.write('if (binds[port][id].joykey != NO_BTN) return 1;\n'
                    'return other_state(udev, binds, port); /* binds[port] */\n')
        if handed_bind_reads(root) != {'input/drivers/hb.c': 1}:
            print('selftest: a driver reading the binds it is handed was not the one read counted')
            bad += 1
        os.remove(os.path.join(root, 'input', 'drivers', 'hb.c'))
        # a joypad driver choosing between a bind and the profile's, and an
        # HID backend taking binds; one that goes by its two lists is neither
        os.makedirs(os.path.join(root, 'input', 'drivers_joypad', 'sub'))
        os.makedirs(os.path.join(root, 'input', 'drivers_hid'))
        with open(os.path.join(root, 'input', 'drivers_joypad', 'sub', 'jb.c'), 'w') as f:
            f.write('k = (binds[i].joykey != NO_BTN) ? binds[i].joykey\n'
                    '   : joypad_info->auto_binds[i].joykey;\n')
        with open(os.path.join(root, 'input', 'drivers_hid', 'hb.c'), 'w') as f:
            f.write('static int16_t st(void *d, const void *binds_data) { return 0; }\n')
        with open(os.path.join(root, 'input', 'drivers_joypad', 'ok.c'), 'w') as f:
            f.write('/* no binds here */ k = joykeys[i]; a = joyaxes[i];\n')
        if joypad_bind_names(root) != {'input/drivers_joypad/sub/jb.c': 3,
                                       'input/drivers_hid/hb.c': 1}:
            print('selftest: joypad and HID files naming binds were not the two counted')
            bad += 1
        # a remap written straight into the settings by the menu; the same
        # write in the input's own file, a read and a comparison are not it
        os.makedirs(os.path.join(root, 'menu', 'cbs'))
        with open(os.path.join(root, 'menu', 'cbs', 'rw.c'), 'w') as f:
            f.write('settings->uints.input_remap_ids[u][b] = idx;\n'
                    'if (settings->uints.input_remap_ids[u][b] == idx) x = 1;\n'
                    'y = settings->uints.input_remap_ids[u][b];\n')
        with open(os.path.join(root, 'input', 'ours.c'), 'w') as f:
            f.write('settings->uints.input_remap_ids[u][b] = idx;\n')
        if remap_direct_writes(root) != {'menu/cbs/rw.c': 1}:
            print('selftest: a remap written directly was not the one counted')
            bad += 1
        if analog_bind_macro_uses(root) != {'input/drivers/an.c': 1}:
            print('selftest: a driver mapping a stick to its binds was not found, '
                  'or the frontend was taken for one')
            bad += 1
        os.remove(os.path.join(root, 'input', 'drivers', 'an.c'))
    if bad:
        print('FAIL input_state_grab_check --selftest')
        return 1
    print('PASS input_state_grab_check --selftest (%d cases)' % (len(cases) + 13))
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()
    sys.exit(selftest() if args.selftest else run(args.root, ALLOWED))
