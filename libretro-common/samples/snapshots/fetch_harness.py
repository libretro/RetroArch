"""Fetch the pinned test sources; never used by a core's normal build or runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from urllib.request import urlopen


def read_lock(path):
    lock = json.loads(path.read_text(encoding='utf-8'))
    if not re.fullmatch(r'[0-9a-f]{40}', lock['revision']):
        raise ValueError('The harness revision must be a full commit SHA')
    if not re.fullmatch(r'[\w-]+/[\w.-]+', lock['repository']):
        raise ValueError('Invalid harness repository')
    if not lock['files']:
        raise ValueError('The harness file list is empty')
    for name, digest in lock['files'].items():
        relative = PurePosixPath(name)
        if (relative.is_absolute() or '..' in relative.parts or '\\' in name
                or ':' in name or relative.as_posix() != name
                or not name.startswith('libretro-common/')
                or not re.fullmatch(r'[0-9a-f]{64}', digest)):
            raise ValueError('Invalid harness file entry: ' + name)
    return lock


def source_path(lock, cache):
    return cache.resolve() / lock['revision']


def default_cache():
    base = os.environ.get('LOCALAPPDATA' if os.name == 'nt' else 'XDG_CACHE_HOME')
    return (Path(base) if base else Path.home() / '.cache') / 'libretro/snapshot-tests'


def prepare(lock, cache, offline=False):
    root = source_path(lock, cache)
    missing = []
    for name, expected in lock['files'].items():
        target = root / name
        if not target.exists():
            missing.append(name)
        elif hashlib.sha256(target.read_bytes()).hexdigest() != expected:
            raise ValueError('Cached harness file differs: ' + str(target)
                             + '; remove that file to fetch it again')
    if missing:
        if offline:
            raise ValueError('Harness is not cached; run the harness target online first'
                             ' or set TOOLS to a local harness directory')
        cache.mkdir(parents=True, exist_ok=True)
        base = ('https://raw.githubusercontent.com/' + lock['repository']
                + '/' + lock['revision'] + '/')
        # Publish only complete, checked downloads; leave existing files alone on failure.
        with tempfile.TemporaryDirectory(prefix='.fetch-', dir=cache) as staging:
            for index, name in enumerate(missing):
                print('Fetching snapshot test source: ' + name, file=sys.stderr)
                with urlopen(base + name, timeout=30) as response:
                    data = response.read()
                if hashlib.sha256(data).hexdigest() != lock['files'][name]:
                    raise ValueError('Downloaded harness checksum differs: ' + name)
                (Path(staging) / str(index)).write_bytes(data)
            for index, name in enumerate(missing):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                (Path(staging) / str(index)).replace(target)
    return root / 'libretro-common/samples/snapshots'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', type=Path,
                        default=Path(__file__).with_name('harness.lock.json'))
    parser.add_argument('--cache', type=Path, default=default_cache())
    parser.add_argument('--path', action='store_true', help='print the path without downloading')
    parser.add_argument('--offline', action='store_true', help='require a complete checked cache')
    args = parser.parse_args()
    try:
        lock = read_lock(args.lock)
        if args.path:
            path = source_path(lock, args.cache) / 'libretro-common/samples/snapshots'
        else:
            path = prepare(lock, args.cache, args.offline)
        print(path.as_posix())
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, 'Snapshot test harness: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
