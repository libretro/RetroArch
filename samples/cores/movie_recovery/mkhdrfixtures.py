#!/usr/bin/env python3
"""Generate coded Main10 PQ/HLG and independent SDR presentation references."""
from pathlib import Path
import argparse
import importlib.util
import subprocess
import struct
import json
import hashlib
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument("output",type=Path)
args=parser.parse_args()
reference_file=Path(__file__).resolve().parents[2]/"gfx/d3d_p010_import/hdr_reference.py"
spec=importlib.util.spec_from_file_location("movie_hdr_reference",reference_file)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
scope={"reference":module.reference}
w=args.output.resolve()
w.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix="movie-hdr-fixtures-") as temporary:
    for transfer,tag in [(16,'smpte2084'),(18,'arib-std-b67')]:
        d = w / ('pq' if transfer == 16 else 'hlg')
        d.mkdir(exist_ok=True)
        raw = Path(temporary) / ('input-' + tag + '.yuv')
        movie = d / 'hevc-aac.mp4'
        with raw.open('wb') as f:
            for frame in range(192):
                planes=[]
                for plane in range(3):
                    width,height=(256,192) if plane==0 else (128,96)
                    data=[]
                    for y in range(height):
                        for x in range(width):
                            patch=(y//(48 if plane==0 else 24))*8+x//(32 if plane==0 else 16)
                            if plane == 0:
                                value = 64 + ((patch * 23 + (frame % 12) * 17) % 877)
                            elif plane == 1:
                                value = 448 + (patch % 3) * 64
                            else:
                                value = 448 + ((patch // 3) % 3) * 64
                            data.append(value)
                    planes.append(struct.pack('<%dH'%len(data),*data))
                f.write(b''.join(planes))
        command = [
            'ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'rawvideo',
            '-pix_fmt', 'yuv420p10le', '-s', '256x192', '-r', '12', '-i', str(raw),
            '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=16',
            '-t', '16', '-c:v', 'libx265', '-preset', 'fast', '-x265-params',
            'pools=1:frame-threads=1:log-level=error:range=limited:colorprim=bt2020:'
            'transfer=' + tag + ':colormatrix=bt2020nc', '-color_range', 'tv',
            '-colorspace', 'bt2020nc', '-color_primaries', 'bt2020', '-color_trc',
            tag, '-c:a', 'aac', '-b:a', '96k', str(movie)]
        subprocess.run(command,check=True)
        probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=profile,pix_fmt,color_range,color_space,color_transfer,color_primaries','-of','json',str(movie)]))
        stream=probe['streams'][0]
        assert stream['profile']=='Main 10' and stream['pix_fmt']=='yuv420p10le' and stream['color_range']=='tv'
        assert stream['color_transfer']==tag and stream['color_space']=='bt2020nc' and stream['color_primaries']=='bt2020'
        decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(movie),'-frames:v','12','-an','-pix_fmt','yuv420p10le','-f','rawvideo','-'])
        assert len(decoded)==12*256*192*3
        reference = bytearray()
        cache = {}
        for frame in range(12):
            values=struct.unpack_from('<73728H',decoded,frame*147456)
            for y in range(192):
                for x in range(256):
                    at=(y//2)*128+x//2
                    key=(values[y*256+x],values[49152+at],values[61440+at])
                    if key not in cache:cache[key]=bytes(scope['reference'](1,False,transfer,*key))
                    reference+=cache[key]
        (d/'reference.rgb').write_bytes(reference)
        manifest = dict(
            command=command, ffprobe=probe,
            reference_filter='FFmpeg decoded YUV planes plus double-precision PQ/HLG SDR oracle',
            reference_source_sha256=hashlib.sha256(reference_file.read_bytes()).hexdigest(),
            movie_sha256=hashlib.sha256(movie.read_bytes()).hexdigest(),
            reference_sha256=hashlib.sha256(reference).hexdigest(),
            output_acceptance='HDR coded input to SDR presentation; not physical HDR output')
        (d/'fixture.json').write_text(json.dumps(manifest,indent=2))
        print('Generated',d.name,'reference colors',len(cache),flush=True)
