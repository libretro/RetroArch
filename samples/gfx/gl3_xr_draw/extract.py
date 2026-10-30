#!/usr/bin/env python3
"""Exercise GL3's headset draw: what it draws straight into the
runtime's textures, and when a chain ends a frame."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
driver = (root / 'gfx/drivers/gl3.c').read_text()


def function(source, name):
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    match = re.search(r'static (?:bool|void) ' + re.escape(name)
                      + r'\([^;]*?\)\s*\{', masked)
    if not match:
        raise RuntimeError('function missing: ' + name)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]


parts = [function(driver, name) for name in
         ('gl3_xr_same', 'gl3_xr_keep', 'gl3_xr_draw_screen',
          'gl3_xr_draw')]
Path(sys.argv[1]).write_text('\n\n'.join(parts) + '\n')
