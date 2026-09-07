#!/usr/bin/env python3
"""Build the MP4 layouts the preview path has to survive.

Every fault this sample pins came from a container LAYOUT, not from a
codec: where the moov sits relative to the media, whether the media
starts past 4 GiB, whether the moov fits the resident head.  So the
fixtures vary exactly that, and nothing else.

The multi-gigabyte ones are sparse: a 64-bit `mdat` header with a hole
in the middle, so a 7.28 GB fixture occupies ~40 MB on disk and costs
nothing to generate in CI.  Sample offsets are rewritten to co64 where
the pad pushes them past 4 GiB.

Needs ffmpeg for the seed clip; everything after that is box surgery.
"""
import os
import struct
import subprocess
import sys

CONTAINERS = {b'moov', b'trak', b'mdia', b'minf', b'stbl', b'edts', b'udta'}


def parse(buf, start, end):
    out, pos = [], start
    while pos + 8 <= end:
        sz = struct.unpack('>I', buf[pos:pos + 4])[0]
        typ = buf[pos + 4:pos + 8]
        hdr = 8
        if sz == 1:
            sz = struct.unpack('>Q', buf[pos + 8:pos + 16])[0]
            hdr = 16
        elif sz == 0:
            sz = end - pos
        if sz < hdr or pos + sz > end:
            break
        out.append((pos, sz, typ, hdr))
        pos += sz
    return out


def rewrite(buf, start, end, shift):
    """Shift every chunk offset, promoting stco to co64."""
    parts = []
    for (pos, sz, typ, hdr) in parse(buf, start, end):
        body = buf[pos + hdr:pos + sz]
        if typ in (b'stco', b'co64'):
            w = 4 if typ == b'stco' else 8
            fmt = '>I' if w == 4 else '>Q'
            n = struct.unpack('>I', body[4:8])[0]
            offs = [struct.unpack(fmt, body[8 + w * i:8 + w * (i + 1)])[0]
                    + shift for i in range(n)]
            nb = body[0:4] + struct.pack('>I', n) \
                + b''.join(struct.pack('>Q', o) for o in offs)
            parts.append(struct.pack('>I', len(nb) + 8) + b'co64' + nb)
        elif typ in CONTAINERS:
            nb = rewrite(buf, pos + hdr, pos + sz, shift)
            parts.append(struct.pack('>I', len(nb) + 8) + typ + nb)
        else:
            parts.append(buf[pos:pos + sz])
    return b''.join(parts)


def boxes(path):
    buf = open(path, 'rb').read()
    return buf, {t: (p, s, h) for (p, s, t, h) in parse(buf, 0, len(buf))}


def seed(dst, secs, w, h, vbr):
    subprocess.check_call([
        'ffmpeg', '-v', 'error', '-y',
        '-f', 'lavfi', '-i', 'testsrc2=s=%dx%d:r=30' % (w, h),
        '-f', 'lavfi', '-i', 'sine=f=440:r=48000',
        '-t', str(secs), '-c:v', 'libx264', '-preset', 'ultrafast',
        '-b:v', vbr, '-pix_fmt', 'yuv420p',
        '-c:a', 'aac', '-b:a', '64k', '-ac', '2', dst])


def trailing_padded(src, dst, target):
    """Trailing moov, media before a sparse pad -> moov past 4 GiB."""
    buf, bx = boxes(src)
    mp, ms, mh = bx[b'mdat']
    vp, vs, vh = bx[b'moov']
    payload = buf[mp + mh:mp + ms]
    pad = target - (mp + 16 + len(payload) + vs)
    if pad <= 0:
        raise SystemExit('target too small')
    shift = (16 - mh) + pad
    newmoov = rewrite(buf, vp + vh, vp + vs, shift)
    newmoov = struct.pack('>I', len(newmoov) + 8) + b'moov' + newmoov
    with open(dst, 'wb') as o:
        o.write(buf[:mp])
        o.write(struct.pack('>I', 1) + b'mdat'
                + struct.pack('>Q', 16 + pad + len(payload)))
        o.seek(mp + 16 + pad)
        o.write(payload)
        o.write(newmoov)


def leading_padded(src, dst, target):
    """Leading moov, media after a sparse pad -> media past 4 GiB."""
    subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-i', src,
                           '-c', 'copy', '-movflags', '+faststart',
                           dst + '.fs.mp4'])
    buf, bx = boxes(dst + '.fs.mp4')
    fp, fs, fh = bx[b'ftyp']
    vp, vs, vh = bx[b'moov']
    mp, ms, mh = bx[b'mdat']
    payload = buf[mp + mh:mp + ms]
    # stco -> co64 grows the moov, and the grown size feeds back into
    # where the media lands: size it once with a dummy shift, then use
    # that length to place everything.
    probe = rewrite(buf, vp + vh, vp + vs, 0)
    probe = struct.pack('>I', len(probe) + 8) + b'moov' + probe
    moov_end = fs + len(probe)
    pad = target - (moov_end + 16 + len(payload))
    if pad <= 0:
        raise SystemExit('target too small')
    shift = (moov_end + 16 + pad) - (mp + mh)
    newmoov = rewrite(buf, vp + vh, vp + vs, shift)
    newmoov = struct.pack('>I', len(newmoov) + 8) + b'moov' + newmoov
    if len(newmoov) != len(probe):
        raise SystemExit('moov size unstable under rewrite')
    with open(dst, 'wb') as o:
        o.write(buf[fp:fp + fs])
        o.write(newmoov)
        o.write(struct.pack('>I', 1) + b'mdat'
                + struct.pack('>Q', 16 + pad + len(payload)))
        o.seek(moov_end + 16 + pad)
        o.write(payload)
    os.unlink(dst + '.fs.mp4')


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out, exist_ok=True)
    j = lambda n: os.path.join(out, n)
    BIG = 7817178301          # the size that started all of this

    seed(j('seed_small.mp4'), 3, 640, 360, '300k')
    seed(j('seed_4k.mp4'), 3, 3840, 2160, '400k')

    # trailing moov, small and fully resident
    os.replace(j('seed_small.mp4'), j('trailing_small.mp4'))
    # trailing moov well past any plausible head
    subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-stream_loop',
                           '60', '-i', j('seed_4k.mp4'), '-c', 'copy',
                           j('trailing_large.mp4')])
    # trailing moov at 7.28 GB, co64 offsets past 4 GiB
    trailing_padded(j('seed_4k.mp4'), j('trailing_huge.mp4'), BIG)
    # leading moov at 7.28 GB, media past 4 GiB
    leading_padded(j('seed_4k.mp4'), j('leading_huge.mp4'), BIG)

    for f in ('seed_4k.mp4',):
        if os.path.exists(j(f)):
            os.unlink(j(f))
    for f in sorted(os.listdir(out)):
        if f.endswith('.mp4'):
            p = j(f)
            print('%-24s %13d bytes  %s on disk'
                  % (f, os.path.getsize(p),
                     subprocess.check_output(['du', '-h', p])
                     .split()[0].decode()))


if __name__ == '__main__':
    main()
