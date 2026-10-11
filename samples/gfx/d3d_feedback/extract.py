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
for driver in ('d3d11', 'd3d12'):
    source = (root / 'gfx' / 'drivers' / (driver + '.c')).read_text()
    start = source.index('static void ' + driver + '_init_render_targets(')
    parts.append(block(source, start))
    match = re.search(
        r'for \(i = 0; i < ' + driver + r'->shader_preset->passes; i\+\+\)'
        r'\s*\{\s*if \(' + driver + r'->shader_preset->pass\[i\]\.feedback\)',
        source)
    if not match:
        raise RuntimeError('feedback swap loop missing: ' + driver)
    parts.append('static void swap_' + driver + '(' + driver + '_video_t *'
                 + driver + ') { unsigned i;\n' + block(source, match.start()) + '\n}')
    if driver == 'd3d12':
        color = re.search(r'static const float clear_color\[4\]\s*=\s*\{[^}]*\};', source)
        start = source.index('for (k = 0; k < d3d12->shader_preset->passes; k++)',
                             source.index('Transition them now'))
        parts.append('static void prepare_d3d12(d3d12_video_t *d3d12)\n'
                     '{ unsigned k; struct context *cmd = d3d12->context;\n'
                     + (color[0] + '\n' if color else '')
                     + block(source, start) + '\n}')

(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n')
