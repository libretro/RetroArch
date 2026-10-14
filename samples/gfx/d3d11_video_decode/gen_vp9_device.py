#!/usr/bin/env python3
"""Generate external VP9 device fixtures and independent FFmpeg NV12/P010 oracles."""
from pathlib import Path
import subprocess
import struct
import sys

out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
for depth in (8, 10):
    for color, matrix, full in (('601-limited', 'bt470bg', False), ('709-full', 'bt709', True)):
        name = f'vp9-{depth}-{color}'
        ivf = out / (name + '.ivf')
        fmt = 'yuv420p10le' if depth == 10 else 'yuv420p'
        subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
            'testsrc2=size=320x192:rate=24', '-vf',
            f'scale=in_range=limited:out_range={"full" if full else "limited"},format={fmt}',
            '-frames:v', '12', '-c:v', 'libvpx-vp9', '-profile:v', '2' if depth == 10 else '0',
            '-colorspace', matrix, '-color_range', 'pc' if full else 'tv',
            '-g', '6', '-lag-in-frames', '0', '-threads', '2', '-y', str(ivf)], check=True)
        raw = out / (name + '.raw')
        planar = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(ivf),
            '-pix_fmt', fmt, '-f', 'rawvideo', '-'])
        # Pack decoded samples directly: swscale's NV12/P010 conversion may
        # change full-range samples to limited range rather than only repack.
        bps = 2 if depth == 10 else 1
        ys = 320 * 192 * bps
        cs = ys // 4
        frame_bytes = ys + 2 * cs
        assert len(planar) == 12 * frame_bytes
        packed = bytearray()
        for offset in range(0, len(planar), frame_bytes):
            y = planar[offset:offset + ys]
            u = planar[offset + ys:offset + ys + cs]
            v = planar[offset + ys + cs:offset + frame_bytes]
            if bps == 2:
                for (sample,) in struct.iter_unpack('<H', y):
                    packed.extend(struct.pack('<H', sample << 6))
                for (cb,), (cr,) in zip(struct.iter_unpack('<H', u), struct.iter_unpack('<H', v)):
                    packed.extend(struct.pack('<HH', cb << 6, cr << 6))
            else:
                packed.extend(y)
                for cb, cr in zip(u, v):
                    packed.extend((cb, cr))
        raw.write_bytes(packed)
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(ivf), '-c', 'copy',
            '-y', str(out / (name + '.webm'))], check=True)
        print(ivf, raw)
