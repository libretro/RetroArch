#!/usr/bin/env python3
"""Explicit developer regeneration of the sampled planar-atlas compute shader."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--glslang', default='glslangValidator')
p.add_argument('--output', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
source = root / 'gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp'
output = a.output or source.with_name('yuv_atlas_to_rgba8888.comp.inc')
with tempfile.TemporaryDirectory(prefix='vulkan-atlas-shader-') as directory:
    binary = Path(directory) / 'atlas.spv'
    subprocess.run([a.glslang, '-V', '-DPLANAR_ATLAS', str(source), '-o', str(binary)], check=True)
    data = binary.read_bytes()
    words = struct.unpack('<%dI' % (len(data) // 4), data)
    lines = [','.join('0x%08x' % w for w in words[i:i + 4]) + ','
             for i in range(0, len(words), 4)]
    output.write_text('{' + '\n'.join(lines) + '}\n', encoding='utf-8')
