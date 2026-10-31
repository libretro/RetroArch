"""Check pinned dependency setup without requiring a network connection."""
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from urllib.error import URLError

import fetch_harness as fetch


class FetchHarnessTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='snapshot test ')
        self.addCleanup(self.directory.cleanup)
        self.base = Path(self.directory.name)
        self.cache = self.base / 'cache with spaces'
        self.sources = {
            'libretro-common/samples/snapshots/snapshot_smoke.c': b'host source\n',
            'libretro-common/include/libretro.h': b'interface header\n',
        }
        self.lock = dict(repository='example/RetroArch', revision='1' * 40,
                         files={name: hashlib.sha256(data).hexdigest()
                                for name, data in self.sources.items()})
        self.lock_path = self.base / 'harness.lock.json'
        self.lock_path.write_text(json.dumps(self.lock), encoding='utf-8')
        self.requests = []

    def download(self, url, timeout):
        prefix = 'https://raw.githubusercontent.com/example/RetroArch/' + self.lock['revision'] + '/'
        self.assertTrue(url.startswith(prefix))
        self.assertEqual(timeout, 30)
        self.requests.append(url)
        return io.BytesIO(self.sources[url[len(prefix):]])

    def prepare(self, offline=False):
        return fetch.prepare(self.lock, self.cache, offline)

    def assert_sources(self):
        root = fetch.source_path(self.lock, self.cache)
        for name, data in self.sources.items():
            self.assertEqual((root / name).read_bytes(), data)
        self.assertFalse(list(self.cache.glob('.fetch-*')))

    def test_download_and_offline_reuse(self):
        with patch.object(fetch, 'urlopen', side_effect=self.download):
            path = self.prepare()
        self.assertEqual(len(self.requests), 2)
        self.assertEqual(path, fetch.source_path(self.lock, self.cache) / 'libretro-common/samples/snapshots')
        self.assert_sources()
        with patch.object(fetch, 'urlopen', side_effect=AssertionError('unexpected network')):
            self.assertEqual(self.prepare(), path)
            self.assertEqual(self.prepare(offline=True), path)

    def test_empty_offline_cache_is_not_created(self):
        with patch.object(fetch, 'urlopen', side_effect=AssertionError('unexpected network')):
            with self.assertRaisesRegex(ValueError, 'not cached'):
                self.prepare(offline=True)
        self.assertFalse(self.cache.exists())

    def test_path_query_neither_fetches_nor_creates_cache(self):
        result = subprocess.run([sys.executable, str(Path(fetch.__file__)), '--path',
                                 '--lock', str(self.lock_path), '--cache', str(self.cache)],
                                capture_output=True, text=True, check=True)
        self.assertEqual(result.stdout.strip(),
                         (fetch.source_path(self.lock, self.cache) / 'libretro-common/samples/snapshots').as_posix())
        self.assertFalse(self.cache.exists())

    def test_corrupt_cache_is_rejected_without_fetching(self):
        with patch.object(fetch, 'urlopen', side_effect=self.download):
            self.prepare()
        target = fetch.source_path(self.lock, self.cache) / next(iter(self.sources))
        target.write_bytes(b'changed after a successful fetch')
        with patch.object(fetch, 'urlopen', side_effect=AssertionError('unexpected network')):
            with self.assertRaisesRegex(ValueError, 'Cached harness file differs'):
                self.prepare()
        self.assertEqual(target.read_bytes(), b'changed after a successful fetch')

    def test_bad_download_never_publishes_sources(self):
        with patch.object(fetch, 'urlopen', side_effect=lambda *a, **kw: io.BytesIO(b'wrong source')):
            with self.assertRaisesRegex(ValueError, 'Downloaded harness checksum differs'):
                self.prepare()
        self.assertFalse(fetch.source_path(self.lock, self.cache).exists())
        self.assertFalse(list(self.cache.glob('.fetch-*')))

    def test_interrupted_download_then_retry(self):
        def interrupted(url, timeout):
            if self.requests:
                raise URLError('interrupted')
            return self.download(url, timeout)
        with patch.object(fetch, 'urlopen', side_effect=interrupted):
            with self.assertRaises(URLError):
                self.prepare()
        self.assertFalse(fetch.source_path(self.lock, self.cache).exists())
        self.assertFalse(list(self.cache.glob('.fetch-*')))
        self.requests.clear()
        with patch.object(fetch, 'urlopen', side_effect=self.download):
            self.prepare()
        self.assert_sources()

    def test_missing_file_failure_keeps_previously_cached_files(self):
        with patch.object(fetch, 'urlopen', side_effect=self.download):
            self.prepare()
        root = fetch.source_path(self.lock, self.cache)
        missing = root / 'libretro-common/include/libretro.h'
        missing.unlink()
        with patch.object(fetch, 'urlopen', side_effect=URLError('offline')):
            with self.assertRaises(URLError):
                self.prepare()
        host_name = next(iter(self.sources))
        self.assertEqual((root / host_name).read_bytes(), self.sources[host_name])
        self.assertFalse(missing.exists())
        self.requests.clear()
        with patch.object(fetch, 'urlopen', side_effect=self.download):
            self.prepare()
        self.assertEqual(len(self.requests), 1)
        self.assert_sources()

    def test_revision_changes_have_separate_caches(self):
        with patch.object(fetch, 'urlopen', side_effect=self.download):
            old_path = self.prepare()
            self.lock['revision'] = '2' * 40
            new_path = self.prepare()
        self.assertNotEqual(old_path, new_path)
        self.assertTrue(old_path.is_dir())
        self.assert_sources()

    def test_lock_rejects_unpinned_revisions_and_invalid_paths(self):
        for field, value in [('revision', 'main'), ('revision', '1' * 39),
                             ('repository', '../RetroArch'), ('files', {}),
                             ('files', {'../outside': '1' * 64}),
                             ('files', {'libretro-common/../outside': '1' * 64}),
                             ('files', {'libretro-common/file': 'not a checksum'})]:
            with self.subTest(field=field, value=value):
                invalid = copy.deepcopy(self.lock)
                invalid[field] = value
                self.lock_path.write_text(json.dumps(invalid), encoding='utf-8')
                with self.assertRaises(ValueError):
                    fetch.read_lock(self.lock_path)


if __name__ == '__main__':
    unittest.main()
