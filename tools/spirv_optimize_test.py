#!/usr/bin/env python3
"""Keep optimized built-ins compatible with GL reflection and buffer uploads."""
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest
from spirv_optimize import optimize

ROOT = Path(__file__).resolve().parents[1]
RESOURCE_KEYS = ('inputs', 'outputs', 'textures', 'separate_images',
                 'separate_samplers', 'ubos', 'ssbos', 'push_constants')


def interface(reflection, names=True):
    def resolve(value):
        if isinstance(value, dict):
            return {key: resolve(reflection['types'][item])
                    if key == 'type' and item in reflection.get('types', {}) else resolve(item)
                    for key, item in value.items()
                    if key != 'id' and (names or key != 'name')}
        if isinstance(value, list):
            return [resolve(item) for item in value]
        return value
    return {key: resolve(reflection[key]) for key in RESOURCE_KEYS if key in reflection}


def reflect(data, directory):
    path = directory / 'module.spv'
    path.write_bytes(data)
    return json.loads(subprocess.check_output(['spirv-cross', str(path), '--reflect']))


class Optimize(unittest.TestCase):
    def test_shared_interfaces(self):
        with tempfile.TemporaryDirectory() as tmp:
            for path in (ROOT / 'gfx/drivers/vulkan_shaders').rglob('*.inc'):
                if path.name.endswith('.src.inc'):
                    continue
                with self.subTest(shader=path.name):
                    words = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', path.read_text())]
                    self.assertEqual(words[0], 0x07230203)
                    data = struct.pack('<%dI' % len(words), *words)
                    before = reflect(data, Path(tmp))
                    after = reflect(optimize(data), Path(tmp))
                    self.assertEqual(interface(before), interface(after))

    def test_shared_ribbon_upload_layout(self):
        # The frontend uploads this whole block, including members unused in VS.
        path = ROOT / 'gfx/drivers/vulkan_shaders/effects/pipeline_ribbon.vert.inc'
        words = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', path.read_text())]
        with tempfile.TemporaryDirectory() as tmp:
            reflection = reflect(struct.pack('<%dI' % len(words), *words), Path(tmp))
        block = reflection['ubos'][0]
        members = reflection['types'][block['type']]['members']
        self.assertEqual([(item['name'], item['offset']) for item in members],
                         [('time', 0), ('yflip', 4), ('alpha', 8), ('padding', 12), ('Ribbon', 16)])
        self.assertEqual(block['block_size'], 816)

    def test_vulkan_codec_bindings(self):
        with tempfile.TemporaryDirectory() as tmp:
            for path in (ROOT / 'libretro-common/formats/pyrowave/shaders/precompiled').glob('*.spv'):
                with self.subTest(shader=path.name):
                    before = reflect(path.read_bytes(), Path(tmp))
                    after = reflect(optimize(path.read_bytes(), strip_debug=True), Path(tmp))
                    self.assertEqual(interface(before, False), interface(after, False))
                    self.assertEqual(before['entryPoints'], after['entryPoints'])


if __name__ == '__main__':
    unittest.main()
