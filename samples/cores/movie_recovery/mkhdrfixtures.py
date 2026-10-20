#!/usr/bin/env python3
"""Generate coded ten-bit PQ/HLG and independent SDR presentation references."""
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
parser.add_argument("--primaries",choices=("bt2020","bt709"),default="bt2020")
parser.add_argument("--codec",choices=("hevc","vp9"),default="hevc")
parser.add_argument("--unspecified-matrix",action="store_true")
parser.add_argument("--max-cll",type=int,default=0)
args=parser.parse_args()
if args.unspecified_matrix and (args.codec != "vp9" or args.primaries != "bt2020"):
    parser.error("--unspecified-matrix requires --codec vp9 --primaries bt2020")
if not 0 <= args.max_cll <= 65535 or (args.max_cll and args.codec != 'hevc'):
    parser.error("--max-cll requires HEVC MP4 and a value from 0 to 65535")


def mp4_add_clli(data, peak):
    """Append clli to the sole HEVC sample entry, preserving coded data."""
    assert data.index(b'mdat') < data.index(b'moov')
    count = 0
    def rewrite(children, path, prefix=0):
        nonlocal count
        result = bytearray(children[:prefix])
        offset = prefix
        while offset < len(children):
            size, kind = struct.unpack_from('>I4s', children, offset)
            assert size >= 8 and offset + size <= len(children)
            payload = children[offset+8:offset+size]
            if kind == path[0] or (len(path) == 1 and path[0] == b'hvc1' and kind == b'hev1'):
                if len(path) == 1:
                    payload += struct.pack('>I4sHH', 12, b'clli', peak, peak//4)
                    count += 1
                else:
                    payload = rewrite(payload, path[1:], 8 if kind == b'stsd' else 0)
            result += struct.pack('>I4s', len(payload)+8, kind) + payload
            offset += size
        return bytes(result)
    result = rewrite(data, [b'moov',b'trak',b'mdia',b'minf',b'stbl',b'stsd',b'hvc1'])
    assert count == 1, count
    return result


primaries = 1 if args.primaries == "bt709" else 9
matrix = 1 if primaries == 1 else 9
matrix_name = "bt709" if matrix == 1 else "bt2020nc"
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
        movie = d / ('movie.webm' if args.codec == 'vp9' else 'hevc-aac.mp4')
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
            'pools=1:frame-threads=1:log-level=error:range=limited:colorprim=' + args.primaries +
            ':transfer=' + tag + ':colormatrix=' + matrix_name, '-color_range', 'tv',
            '-colorspace', matrix_name, '-color_primaries', args.primaries, '-color_trc',
            tag, '-c:a', 'aac', '-b:a', '96k', str(movie)]
        if args.codec == 'vp9':
            start = command.index('-c:v')
            end = command.index('-color_range')
            command[start:end] = ['-c:v', 'libvpx-vp9', '-deadline', 'good',
                '-cpu-used', '4', '-row-mt', '1', '-threads', '2',
                '-profile:v', '2', '-g', '48', '-crf', '18', '-b:v', '0']
            command[command.index('-c:a') + 1] = 'libopus'
        subprocess.run(command,check=True)
        if args.max_cll:
            movie.write_bytes(mp4_add_clli(movie.read_bytes(), args.max_cll))
        if args.unspecified_matrix:
            # Keep every coded packet intact: only the same-size EBML
            # MatrixCoefficients value changes from BT.2020 NCL to code 2.
            data = movie.read_bytes()
            element = b'\x55\xb1\x81\x09'
            assert data.count(element) == 1
            movie.write_bytes(data.replace(element, b'\x55\xb1\x81\x02'))
        probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=profile,pix_fmt,color_range,color_space,color_transfer,color_primaries','-of','json',str(movie)]))
        stream=probe['streams'][0]
        assert stream['profile']==('Profile 2' if args.codec == 'vp9' else 'Main 10') and stream['pix_fmt']=='yuv420p10le' and stream['color_range']=='tv'
        assert stream['color_transfer']==tag and stream['color_space']==matrix_name and stream['color_primaries']==args.primaries
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
                    if key not in cache:
                        cache[key]=bytes(scope['reference'](1,False,transfer,*key,primaries,matrix,args.max_cll))
                    reference+=cache[key]
        (d/'reference.rgb').write_bytes(reference)
        manifest = dict(
            command=command, ffprobe=probe,
            reference_filter='FFmpeg decoded YUV planes plus double-precision PQ/HLG SDR oracle',
            reference_source_sha256=hashlib.sha256(reference_file.read_bytes()).hexdigest(),
            movie_sha256=hashlib.sha256(movie.read_bytes()).hexdigest(),
            reference_sha256=hashlib.sha256(reference).hexdigest(),
            output_acceptance='HDR coded input to SDR presentation; not physical HDR output')
        if args.codec == 'vp9':
            manifest['movie_file'] = movie.name
            manifest['matrix_coefficients'] = 2 if args.unspecified_matrix else matrix
        if args.max_cll:
            manifest['max_cll'] = args.max_cll
            manifest['max_fall'] = args.max_cll//4
        (d/'fixture.json').write_text(json.dumps(manifest,indent=2))
        print('Generated',d.name,'reference colors',len(cache),flush=True)
