#!/usr/bin/env python3
"""Take the shipping hold functions out of input_driver.c."""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
source = (here.parents[2] / 'input/input_driver.c').read_text(encoding='utf-8')
masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                lambda m: ' ' * len(m[0]), source, flags=re.S)


def function(name):
    match = re.search(r'^(?:static )?(?:void|bool) ' + name + r'\(',
                      masked, flags=re.M)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]


parts = [function(name) for name in (
    'input_driver_hold_core_input',
    'input_driver_hold_held_input',
    'input_driver_hold_held_input_late',
    'input_driver_hold_bits')]
(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n',
                                      encoding='utf-8')
