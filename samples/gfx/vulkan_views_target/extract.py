#!/usr/bin/env python3
"""Exercise Vulkan's shipping target allocation, shared retry policy
and the XR draw: what it draws straight into the runtime's images, and
the slots it cannot draw."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
driver = (root / 'gfx/drivers/vulkan.c').read_text()
shared = (root / 'gfx/video_views_draw.h').read_text()


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
         ('vulkan_views_retire', 'vulkan_views_make')]
for name in ('VV_FN', 'VV_DRV', 'VV_TARGET', 'VV_TARGET_MAKE',
             'VV_TARGET_RETIRE'):
    match = re.search(r'^#define ' + name + r'(?:\(|\s).*$', driver, re.M)
    if not match:
        raise RuntimeError('binding missing: ' + name)
    parts.append(match[0])
parts.append(function(shared, 'VV_FN(target_ensure)'))
parts.append(function(driver, 'vulkan_xr_draw_screen'))
parts.append(function(driver, 'vulkan_xr_slot_ensure'))
parts.append(function(driver, 'vulkan_xr_draw'))
Path(sys.argv[1]).write_text('\n\n'.join(parts) + '\n')
