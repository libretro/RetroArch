#!/usr/bin/env python3
"""Checks for RSX source drift, corrupt microcode and retained runtime symbols."""
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from rsx_shaders import SOURCE, validate, verify


class Artifacts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.source = Path(self.temporary.name) / 'rsx_shaders'
        shutil.copytree(SOURCE, self.source, ignore=shutil.ignore_patterns('*.o'))

    def tearDown(self):
        self.temporary.cleanup()

    def test_bundle(self):
        manifest = verify(self.source)
        self.assertEqual(len(manifest['artifacts']), 4)

    def test_stale_source(self):
        source = self.source / 'modern_opaque.vcg'
        source.write_bytes(source.read_bytes() + b'\n/* changed */\n')
        with self.assertRaisesRegex(ValueError, 'Stale'):
            verify(self.source)

    def test_corrupt_artifact(self):
        path = self.source / 'precompiled/modern_opaque.fpo'
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaisesRegex(ValueError, 'Corrupt'):
            verify(self.source)

    def test_bad_code_bounds(self):
        data = bytearray((self.source / 'precompiled/modern_opaque.vpo').read_bytes())
        struct.pack_into('>I', data, 20, len(data) + 16)
        with self.assertRaises(ValueError):
            validate(data, True)

    def test_parameter_names(self):
        data = (self.source / 'precompiled/modern_opaque.vpo').read_bytes()
        with self.assertRaisesRegex(ValueError, 'Missing parameter'):
            validate(data.replace(b'modelViewProj', b'brokenUniform'), True)


if __name__ == '__main__':
    unittest.main()
