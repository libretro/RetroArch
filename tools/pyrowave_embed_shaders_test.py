#!/usr/bin/env python3
"""Regression checks for stale/corrupted committed shader artifacts."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from pyrowave_embed_shaders import SOURCE, assets

class Artifacts(unittest.TestCase):
    def test_committed_headers_reject_stale_data(self):
        repo = Path(__file__).resolve().parents[1]
        for tool, header in (
                ('pyrowave_embed_shaders.py', SOURCE / 'rpyrowave_shaders.h'),
                ('d3d_shader_embed.py', repo / 'gfx/common/d3d_builtin_data.h')):
            with self.subTest(tool=tool):
                command = [sys.executable, str(repo / 'tools' / tool), '--check', '--output']
                subprocess.run(command + [str(header)], check=True, capture_output=True)
                stale = self.root / tool.replace('.py', '.h')
                stale.write_bytes(header.read_bytes() + b'\n/* stale */\n')
                result = subprocess.run(command + [str(stale)], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('header is stale', result.stderr)

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
