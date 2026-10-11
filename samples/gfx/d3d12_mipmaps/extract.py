#!/usr/bin/env python3
"""Exercise the shipping mip-count and compute dispatch code."""
import pathlib
import re
here = pathlib.Path(__file__).resolve().parent
source = (here.parents[2] / 'gfx/drivers/d3d12.c').read_text()
masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                lambda m: ' ' * len(m[0]), source, flags=re.S)
parts = []
for name in ('d3d12_mip_levels', 'd3d12_generate_mipmaps',
             'd3d12_init_compute_sampler'):
    match = re.search(r'static (?:unsigned|void) ' + name + r'\(', source)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    parts.append(source[match.start():end])
(here / 'driver_code.inc').write_text('\n\n'.join(parts) + '\n')
