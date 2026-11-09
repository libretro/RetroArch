#!/usr/bin/env python3
"""Exercise the frontend and menu's shipping bind capture functions."""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
root = here.parents[2]
parts = []
for path, names in (
    ('input/input_driver.c', (
        'input_driver_has_device_state',
        'input_driver_bind_capture_state',
        'input_driver_bind_capture_keys',
        'input_snapshot_driver_checked',
        'input_driver_capture_pad',
        'input_driver_set_wait_input_release',
        'input_driver_waiting_input_release',
        'input_driver_set_keyboard_press_cb',
        'input_hotkey_user',
        'input_config_hotkey_user',
        'input_port_sharers_live',
        'input_config_hotkey_listen_slots',
        'input_config_hotkey_bind_own')),
    ('menu/menu_driver.c', (
        'menu_bind_is_trigger',
        'menu_bind_first_bit',
        'menu_input_key_bind_poll_find_hold_pad',
        'menu_input_key_bind_poll_find_hold',
        'menu_input_key_bind_poll_find_trigger_pad',
        'menu_input_key_bind_poll_find_trigger',
        'menu_input_key_bind_poll_bind_get_rested_axes',
        'menu_input_key_bind_poll_bind_state',
        'menu_input_key_bind_set_mode',
        'menu_input_key_bind_iterate')),
):
    source = (root / path).read_text(encoding='utf-8')
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    if path == 'menu/menu_driver.c':
        start = source.index('typedef struct menu_input_ctx_bind')
        end = source.index('menu_input_ctx_bind_t;', start)
        parts.append(source[start:end] + 'menu_input_ctx_bind_t;')
    for name in names:
        match = re.search(r'^(?:MENU_NOINLINE )?(?:static )?'
                          r'(?:bool|int16_t|unsigned|uint32_t|void) '
                          + name + r'\(', source, flags=re.M)
        start = match.start()
        end = masked.index('{', start) + 1
        depth = 1
        while depth:
            depth += (masked[end] == '{') - (masked[end] == '}')
            end += 1
        parts.append(source[start:end])

(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n',
                                      encoding='utf-8')
