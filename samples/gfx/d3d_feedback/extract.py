#!/usr/bin/env python3
"""Build the harness from the drivers' actual allocation and swap loops."""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
root = here.parents[2]


def block(source, start):
    # Mask comments and literals without changing brace positions.
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    opening = masked.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[start:end]


parts = []
chains = {'d3d11': 'd3d11_chain_t', 'd3d12': 'd3d12_filter_chain_t'}
for driver in ('d3d11', 'd3d12'):
    source = (root / 'gfx' / 'drivers' / (driver + '.c')).read_text()
    start = source.index('static void ' + driver + '_chain_init_render_targets(')
    parts.append(block(source, start))
    match = re.search(
        r'for \(i = 0; i < c->shader_preset->passes; i\+\+\)\s*\{\s*'
        + driver + r'_pass_t \*pass\s*= &c->pass\[i\];\s*'
        r'(if \(c->shader_preset->pass\[i\]\.feedback\))',
        source)
    if not match:
        raise RuntimeError('feedback swap loop missing: ' + driver)
    parts.append('static void swap_' + driver + '(' + chains[driver] + ' *c)\n'
                 '{ unsigned i; for (i = 0; i < c->shader_preset->passes; i++)\n'
                 '{ ' + driver + '_pass_t *pass = &c->pass[i];\n'
                 + block(source, match.start(1)) + '\n} }')

(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n')
