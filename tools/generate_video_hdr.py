#!/usr/bin/env python3
"""Regenerate the embedded HLSL copy of the shared HDR functions."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
folder = Path(__file__).resolve().parents[1] / 'gfx/drivers_shader'
expected = ''.join(json.dumps(line) + '\n' for line in
                   (folder / 'video_hdr.inc.glsl').read_text().splitlines(keepends=True))
target = folder / 'video_hdr.inc'
if args.check:
    if target.read_text() != expected:
        raise SystemExit('Regenerate video_hdr.inc with tools/generate_video_hdr.py')
else:
    target.write_text(expected)
