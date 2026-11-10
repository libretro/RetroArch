"""Exercise external movie settings at a filesystem root and in a child folder."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('fixture', type=Path)
parser.add_argument('--root-directory', type=Path)
args, extra = parser.parse_known_args()
fixture = args.fixture.resolve(strict=True)
root = args.root_directory
if root is None:
    if os.name != 'nt':
        parser.error('--root-directory is required outside Windows')
    root = Path(fixture.anchor)
root = root.resolve(strict=True)
if root != Path(root.anchor):
    parser.error('--root-directory must be a filesystem root')
runner = Path(__file__).resolve().parents[1] / 'rgui_media' / 'run.py'
with tempfile.TemporaryDirectory(prefix='media-settings-', dir=root) as temp:
    nested = Path(temp) / ('movie' + fixture.suffix)
    try:
        os.link(fixture, nested)
    except OSError:
        shutil.copyfile(fixture, nested)
    fd, name = tempfile.mkstemp(prefix='retroarch-media-settings-',
                                suffix=fixture.suffix, dir=root)
    os.close(fd)
    root_movie = Path(name)
    try:
        root_movie.unlink()
        try:
            os.link(nested, root_movie)
        except OSError:
            shutil.copyfile(nested, root_movie)
        for label, movie in [('root', root_movie), ('nested', nested)]:
            print('external settings path: ' + label, flush=True)
            subprocess.run([sys.executable, str(runner), '--external',
                            '--external-settings', '--driver', 'ozone',
                            *extra, str(movie)], check=True)
    finally:
        root_movie.unlink(missing_ok=True)
