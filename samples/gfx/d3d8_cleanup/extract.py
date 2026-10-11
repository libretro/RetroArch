#!/usr/bin/env python3
"""Exercise the shipping D3D8 resource cleanup."""
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
source = (here.parents[2] / 'gfx/drivers/d3d8.c').read_text()
masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                lambda m: ' ' * len(m[0]), source, flags=re.S)
start = source.index('static void d3d8_deinitialize(d3d8_video_t *d3d)')
end = masked.index('{', start) + 1
depth = 1
while depth:
    depth += (masked[end] == '{') - (masked[end] == '}')
    end += 1
(here / 'driver_code.inc').write_text(source[start:end] + '\n')
