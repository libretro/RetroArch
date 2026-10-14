#!/usr/bin/env python3
"""Prepare a local HEVC Main8 long-term vector for the native device oracle.

Input media is supplied by the caller; this script never downloads media.
FFmpeg/ffprobe are test tools, not player dependencies.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output_dir', type=Path)
    parser.add_argument('--frames', type=int, default=24)
    args = parser.parse_args()
    if not 1 <= args.frames <= 32:
        parser.error('--frames must be between 1 and 32')
    source = args.input.resolve(strict=True)
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    mp4 = output / 'hevc_conformance.mp4'
    raw = output / 'hevc_conformance.nv12'
    if source in (mp4, raw):
        parser.error('input must differ from output files')
    with tempfile.TemporaryDirectory() as tmp:
        full = str(Path(tmp) / 'full.mp4')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(source),
                        '-map', '0:v:0', '-c', 'copy', '-tag:v', 'hvc1', full], check=True)
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', full, '-map', '0:v:0',
                        '-frames:v', str(args.frames), '-c', 'copy',
                        '-bsf:v', 'filter_units=remove_types=32-34', '-tag:v', 'hvc1',
                        '-use_editlist', '0', str(mp4)], check=True)
    info = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
        'stream=width,height,pix_fmt', '-of', 'json', str(mp4)]))['streams'][0]
    if info['pix_fmt'] != 'yuv420p':
        raise ValueError('This oracle accepts Main8 yuv420p input only')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(mp4),
                    '-map', '0:v:0', '-fps_mode', 'passthrough',
                    '-pix_fmt', 'nv12', '-f', 'rawvideo', str(raw)], check=True)
    frame_bytes = info['width'] * info['height'] * 3 // 2
    raw_bytes = raw.stat().st_size
    if not frame_bytes or raw_bytes % frame_bytes or not 0 < raw_bytes // frame_bytes <= 32:
        raise ValueError('Invalid reference frame count')
    metadata = dict(info, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    mp4_sha256=hashlib.sha256(mp4.read_bytes()).hexdigest(),
                    nv12_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),
                    output_frames=raw_bytes // frame_bytes)
    (output / 'hevc_conformance.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('HEVC_CONFORMANCE_MP4=' + str(mp4))
    print('HEVC_CONFORMANCE_NV12=' + str(raw))
    print(json.dumps(metadata))


if __name__ == '__main__':
    main()
