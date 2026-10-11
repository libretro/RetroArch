#!/usr/bin/env python3
"""Exercise the shipping menu cancellation and release code."""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
source = (here.parents[2] / 'input/input_driver.c').read_text()


def block(start):
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    end = masked.index('{', start) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[start:end]


start = source.index('typedef struct', source.index('static input_hotkey_set_t'))
parts = [block(start) + ' input_hotkey_block_t;']
for name in ('input_mbutton_bit', 'input_hotkey_uses_enable'):
    match = re.search(r'static (?:unsigned|bool) ' + name + r'\(', source)
    if match:
        parts.append(block(match.start()))

start = source.index('/* Set menu toggle on release */', source.index('void input_keys_pressed('))
end = source.index('BIT256_SET_PTR(p_new_state, i);', start)
end += len('BIT256_SET_PTR(p_new_state, i);')
parts.append('static void press(input_hotkey_block_t blk, unsigned i,\n'
             '      bool other_pressed, bool kb_blocked, input_bits_t *p_new_state)\n'
             '{ input_driver_state_t *input_st = blk.input_st;\n'
             '  (void)other_pressed; (void)kb_blocked; do {\n'
             + source[start:end] + '\n} while (0); }')
start = source.index('if (i == RARCH_MENU_TOGGLE)', end)
parts.append('static void release(input_driver_state_t *input_st,\n'
             '      input_bits_t *p_new_state)\n'
             '{ unsigned i = RARCH_MENU_TOGGLE; unsigned input_hotkey_block_delay = 0;\n'
             + block(start) + '\n}')
(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n')
