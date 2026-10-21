#!/usr/bin/env python3
"""Independent movie timestamps for the real frontend/mixer conformance."""
from fractions import Fraction
from pathlib import Path
import json
import subprocess
import sys

out = Path(sys.argv[1] if len(sys.argv) > 1 else 'fixtures/movie-timeline')
out.mkdir(parents=True, exist_ok=True)
core_generator = Path(__file__).resolve().parents[2] / 'cores/webm_player/video_timeline_fixtures.py'
subprocess.run([sys.executable, str(core_generator), str(out)], check=True)
for codec in ('libopus', 'libvorbis'):
    path = out / ('video-timeline-' + codec + '.webm')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-itsoffset', '1',
         '-f', 'lavfi', '-i', 'testsrc2=size=64x48:rate=12:duration=2',
         '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=4',
         '-map', '0:v:0', '-map', '1:a:0', '-fps_mode', 'passthrough',
         '-c:v', 'libvpx', '-deadline', 'realtime', '-cpu-used', '8',
         '-c:a', codec, '-ac', '2', str(path)], check=True)
for path in (out / 'video-timeline-1.mp4', out / 'video-timeline-libopus.webm',
             out / 'video-timeline-libvorbis.webm'):
    data = json.loads(subprocess.check_output(['ffprobe', '-v', 'error',
         '-select_streams', 'v:0', '-show_entries',
         'stream=time_base:packet=pts:format=duration', '-of', 'json', str(path)]))
    scale = Fraction(data['streams'][0]['time_base'])
    pts = [int(int(p['pts']) * scale * 1000000000) for p in data['packets']]
    duration = int(Fraction(data['format']['duration']) * 1000000000)
    assert len(pts) == 24 and 1000000000 <= pts[0] <= 1020000000
    assert 4000000000 <= duration <= 4050000000
    # Decimal components avoid C long-width assumptions in the test.
    values = [duration] + pts
    path.with_name(path.name + '.timing').write_text(
         ''.join(f'{v // 1000000000} {v % 1000000000}\n' for v in values), encoding='utf-8')
    print(path.name, 'pictures', len(pts), 'first_ns', pts[0], 'duration_ns', duration)
