#!/usr/bin/env python3
"""Regression checks for stale/corrupted committed shader artifacts."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from pyrowave_embed_shaders import SOURCE, assets

class Artifacts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='pyrowave-artifacts-')
        self.root = Path(self.temporary.name)
        for name in ('color.hlsl', 'dwt.hlsl', 'packet.hlsl'):
            shutil.copyfile(SOURCE / name, self.root / name)
        self.bundle = self.root / 'precompiled'
        shutil.copytree(SOURCE / 'precompiled', self.bundle)
        self.addCleanup(self.temporary.cleanup)

    def test_complete_bundle(self):
        self.assertEqual(len(assets(self.bundle)), 18)

    def test_source_edit_requires_recompilation(self):
        with (self.root / 'color.hlsl').open('ab') as source:
            source.write(b'\n/* Changed source. */\n')
        with self.assertRaisesRegex(ValueError, 'Stale precompiled'):
            assets(self.bundle)

    def test_corrupted_artifact_fails_hash(self):
        path = self.bundle / 'packet.spv'
        path.write_bytes(path.read_bytes()[:-4])
        with self.assertRaisesRegex(ValueError, 'hash/size mismatch'):
            assets(self.bundle)

    def test_wrong_container_rejected_even_with_updated_hash(self):
        path = self.bundle / 'packet.dxil'
        data = b'FAIL' + path.read_bytes()[4:]
        path.write_bytes(data)
        manifest = self.bundle / 'manifest.json'
        values = json.loads(manifest.read_text())
        values['artifacts'][path.name]['sha256'] = hashlib.sha256(data).hexdigest()
        manifest.write_text(json.dumps(values))
        with self.assertRaisesRegex(ValueError, 'Invalid Direct3D'):
            assets(self.bundle)

if __name__ == '__main__':
    unittest.main()
