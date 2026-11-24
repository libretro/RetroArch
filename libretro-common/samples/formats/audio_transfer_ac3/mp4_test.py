import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
p = argparse.ArgumentParser()
p.add_argument('--test', required=True)
p.add_argument('--runner', default='')
p.add_argument('--input', type=Path)
p.add_argument('--preview-test', type=Path)
a = p.parse_args()
if not shutil.which('ffmpeg'):
    print('SKIP: ffmpeg unavailable for MP4 AC-3 reference')
    raise SystemExit(0)
with tempfile.TemporaryDirectory(prefix='mp4-ac3-') as name:
    root = Path(name)
    movie = a.input or root / 'audio.mp4'
    if not a.input:
        subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
            'sine=frequency=440:sample_rate=48000', '-t', '0.8', '-ac', '2',
            '-c:a', 'ac3', '-b:a', '192k', '-movflags', 'empty_moov+delay_moov+default_base_moof',
            '-frag_duration', '100000', str(movie)], check=True)
    reference = root / 'reference.f32'
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(movie), '-map', '0:a:0',
        '-f', 'f32le', str(reference)], check=True)
    duration = json.loads(subprocess.check_output(['ffprobe', '-v', 'error',
        '-show_entries', 'format=duration', '-of', 'json', str(movie)]))['format']['duration']
    runner = shlex.split(a.runner)
    paths = [str(movie.resolve()), str(reference.resolve())]
    if any('wine' in part for part in runner):
        paths = [subprocess.check_output(['winepath', '-w', path], text=True).strip() for path in paths]
    subprocess.run(runner + [str(Path(a.test).resolve())] + paths + [duration], check=True)

    if a.preview_test:
        subprocess.run(runner + [str(a.preview_test.resolve()), '--ac3', paths[0]], check=True)

        if not a.input:
            padded = root / 'padded.mp4'
            subprocess.run(['ffmpeg', '-v', 'error', '-i', str(movie), '-c', 'copy',
                '-moov_size', '4194304', str(padded)], check=True)
            path = str(padded.resolve())
            if any('wine' in part for part in runner):
                path = subprocess.check_output(['winepath', '-w', path], text=True).strip()
            subprocess.run(runner + [str(a.preview_test.resolve()), '--ac3', path], check=True)

            # A box after an oversized gap must remain outside the resident head.
            startup = root / 'startup.mp4'
            with startup.open('wb') as out:
                out.write(movie.read_bytes())
                out.write((8 * 1024 * 1024).to_bytes(4, 'big') + b'free')
                out.seek(8 * 1024 * 1024 - 8, 1)
                out.write(b'\x00\x00\x00\x08free')
            path = str(startup.resolve())
            if any('wine' in part for part in runner):
                path = subprocess.check_output(['winepath', '-w', path], text=True).strip()
            subprocess.run(runner + [str(a.preview_test.resolve()), '--startup', path], check=True)

            seek_source = root / 'seek-source.mp4'
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
                'sine=frequency=440:sample_rate=48000', '-t', '4', '-ac', '2',
                '-c:a', 'ac3', '-b:a', '192k', '-movflags',
                'empty_moov+delay_moov+default_base_moof', '-frag_duration',
                '100000', str(seek_source)], check=True)
            raw = seek_source.read_bytes()
            seek_movie = root / 'seek-window.mp4'
            with seek_movie.open('wb') as out:
                offset = fragments = 0
                while offset < len(raw):
                    length = int.from_bytes(raw[offset:offset + 4], 'big')
                    assert length >= 8 and length <= len(raw) - offset
                    if raw[offset + 4:offset + 8] == b'moof':
                        fragments += 1
                        if fragments == 4:
                            out.write((8 * 1024 * 1024).to_bytes(4, 'big') + b'free')
                            out.seek(8 * 1024 * 1024 - 8, 1)
                    out.write(raw[offset:offset + length])
                    offset += length
                assert fragments > 4
            path = str(seek_movie.resolve())
            if any('wine' in part for part in runner):
                path = subprocess.check_output(['winepath', '-w', path], text=True).strip()
            subprocess.run(runner + [str(a.preview_test.resolve()), '--ac3-seek', path], check=True)
