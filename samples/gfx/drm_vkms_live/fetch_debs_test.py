#!/usr/bin/env python3
"""Exercise mirror failure without downloading or changing host apt sources."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class FetchTest(unittest.TestCase):
    def fetch(self, manifest=None, helper_rc=0):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dest = root / 'deb'
            dest.mkdir()
            log = root / 'calls'
            scripts = {
                'apt-get': '''#!/bin/sh
if [ "$1" = --print-uris ]; then
   printf '%s' "$MANIFEST"
   exit 0
fi
exit "$MIRROR_RC"
''',
                'timeout': '''#!/bin/sh
printf '%s\\n' "$*" >> "$CALL_LOG"
shift 3
if [ "$1" = /usr/lib/apt/apt-helper ]; then
   exit "$HELPER_RC"
fi
exec "$@"
''',
            }
            for name, source in scripts.items():
                path = root / name
                path.write_bytes(source.encode())
                path.chmod(0o755)
            env = os.environ.copy()
            env.update(PATH=str(root) + os.pathsep + env['PATH'],
                       MANIFEST=manifest or '', MIRROR_RC='124' if manifest is not None else '0',
                       HELPER_RC=str(helper_rc), CALL_LOG=str(log))
            helper = Path(__file__).with_name('fetch_debs.sh').resolve().as_posix()
            shell = shutil.which('sh') or 'C:/msys64/usr/bin/bash.exe'
            command = '. "$1"; download_debs "$2" image modules'
            result = subprocess.run([shell, '-c', command, 'test', helper,
                                     dest.as_posix()], env=env, capture_output=True,
                                    text=True, timeout=10)
            return result, log.read_text()

    def test_mirror_success(self):
        result, calls = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('apt-helper', calls)

    def test_stalled_mirror_uses_https_and_signed_index_hashes(self):
        manifest = ("'http://azure.archive.ubuntu.com/ubuntu/pool/main/l/image.deb' "
                    "image.deb 10 SHA512:abc\n"
                    "'mirror+file:/etc/apt/apt-mirrors.txt/pool/main/l/modules.deb' "
                    "modules.deb 20 SHA256:def\n")
        result, calls = self.fetch(manifest)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls.count('/usr/lib/apt/apt-helper'), 2)
        self.assertIn('https://archive.ubuntu.com/ubuntu/pool/main/l/image.deb', calls)
        self.assertIn('SHA512:abc', calls)
        self.assertIn('SHA256:def', calls)
        self.assertNotIn('http://azure.archive', calls)

    def test_failed_or_bad_hash_download_fails(self):
        result, _ = self.fetch("'http://mirror/pool/a.deb' a.deb 1 SHA256:abc\n", 100)
        self.assertNotEqual(result.returncode, 0)

    def test_missing_metadata_fails(self):
        for manifest in ['', "'http://mirror/pool/a.deb' a.deb 1 MD5Sum:abc\n",
                         "'http://mirror/other/a.deb' a.deb 1 SHA256:abc\n",
                         "'http://mirror/pool/a.deb' ../a.deb 1 SHA256:abc\n"]:
            with self.subTest(manifest=manifest):
                result, calls = self.fetch(manifest)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('/usr/lib/apt/apt-helper', calls)


if __name__ == '__main__':
    unittest.main()
