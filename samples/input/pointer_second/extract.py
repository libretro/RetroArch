#!/usr/bin/env python3
"""The shipping code a core's pointer read goes through in input_driver.c."""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
source = (here.parents[2] / 'input/input_driver.c').read_text()
masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                lambda m: ' ' * len(m[0]), source, flags=re.S)


def block_end(start):
    end = masked.index('{', start) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return end


parts = []
for name in ('input_overlay_pointer_state',
             'input_overlay_pointing_device_state'):
    start = re.search(r'static int16_t ' + name + r'\(', source).start()
    parts.append(source[start:block_end(start)])

# input_state_device(), with whatever sits between it and the turbo code
start = source.index('\n', source.index('===== END input turbo and hold =====')) + 1
end = block_end(source.index('static int16_t input_state_device('))
parts.append(source[start:end])
(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n')
