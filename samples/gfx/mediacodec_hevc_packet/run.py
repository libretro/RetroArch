#!/usr/bin/env python3
"""Test-only HEVC generator and real parser/packet adapter validation."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument('--test',default='./parser_test');a=p.parse_args()
ffmpeg=shutil.which('ffmpeg');assert ffmpeg,'Test-only ffmpeg with libx265 required'
exe=str(Path(a.test).resolve())
with tempfile.TemporaryDirectory(prefix='mediacodec-hevc-parser-') as temp:
 for depth in (8,10):
  path=Path(temp)/f'main{depth}.hevc'
  subprocess.run([ffmpeg,'-v','error','-f','lavfi','-i','testsrc2=size=64x48:rate=1',
   '-frames:v','1','-pix_fmt','yuv420p' if depth==8 else 'yuv420p10le',
   '-c:v','libx265','-x265-params','pools=none:frame-threads=1:keyint=1:repeat-headers=1:log-level=error',
   '-f','hevc',str(path)],check=True)
  subprocess.run([exe,str(path)],check=True)
