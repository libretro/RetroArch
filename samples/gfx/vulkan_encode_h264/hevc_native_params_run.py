#!/usr/bin/env python3
"""Check native HEVC session headers with the independent software decoder."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native-test', required=True, type=Path)
    parser.add_argument('--headers-test', required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='vulkan-hevc-headers-') as directory:
        for depth in (8, 10):
            headers = Path(directory) / ('main%d.hevc' % depth)
            result = subprocess.run([str(args.native_test.resolve()),
                                     str(depth), str(headers)], check=False)
            if result.returncode == 77:
                raise SystemExit(77)
            result.check_returncode()
            subprocess.run([str(args.headers_test.resolve()), str(depth),
                            str(headers)], check=True)


if __name__ == '__main__':
    main()
