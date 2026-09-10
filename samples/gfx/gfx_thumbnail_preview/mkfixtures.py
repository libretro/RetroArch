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


def anim_webp(dst, secs, w, h, fps):
    """Lossless animated WebP, one ANMF per frame.  Lossless so the
    frames are large (hundreds of KB at 720p): the windowed-open test
    needs a file well past the head + lookahead + margin the feeder is
    allowed to keep resident, or a whole-file load would be
    indistinguishable from a windowed one by RSS."""
    subprocess.check_call([
        'ffmpeg', '-v', 'error', '-y',
        '-f', 'lavfi', '-i', 'testsrc2=s=%dx%d:r=%d' % (w, h, fps),
        '-t', str(secs), '-c:v', 'libwebp_anim', '-lossless', '1',
        '-compression_level', '0', '-loop', '0', dst])


def anim_webp_big_frame(src, dst, frame_bytes):
    """Rewrite an animated WebP so its SECOND frame is 'frame_bytes'
    long: the frame's VP8L payload is preceded by an unknown sub-chunk
    of padding, which the frame decoder skips.  The frame then spans
    more than the feeder's lookahead, which is the case next_span
    exists for - without it the window could never cover the frame and
    the animation would sit at the wall forever."""
    b = bytearray(open(src, 'rb').read())
    pos, n = 12, 0
    while pos + 8 <= len(b):
        tag = bytes(b[pos:pos + 4])
        sz = struct.unpack('<I', b[pos + 4:pos + 8])[0]
        if tag == b'ANMF':
            n += 1
            if n == 2:
                pad = frame_bytes - sz
                if pad < 8:
                    raise SystemExit('frame already that large')
                pad -= 8
                pad += pad & 1
                chunk = b'PADx' + struct.pack('<I', pad) + bytes(pad)
                ins = pos + 8 + 16       # after the 16-byte ANMF header
                b[ins:ins] = chunk
                b[pos + 4:pos + 8] = struct.pack('<I', sz + len(chunk))
                b[4:8] = struct.pack('<I', len(b) - 8)
                break
        pos += 8 + sz + (sz & 1)
    open(dst, 'wb').write(b)


def still_webp(dst, w, h):
    subprocess.check_call([
        'ffmpeg', '-v', 'error', '-y',
        '-f', 'lavfi', '-i', 'testsrc2=s=%dx%d:r=1' % (w, h),
        '-frames:v', '1', '-c:v', 'libwebp', '-lossless', '1',
        '-compression_level', '0', dst])


def apng(dst, secs, w, h, fps):
    """APNG: the same shape as the WebP - large lossless frames, many of
    them, well past the window the feeder may keep."""
    subprocess.check_call([
        'ffmpeg', '-v', 'error', '-y',
        '-f', 'lavfi', '-i', 'testsrc2=s=%dx%d:r=%d' % (w, h, fps),
        '-t', str(secs), '-c:v', 'apng', '-pred', 'none', '-plays', '0',
        '-f', 'apng', dst])


def apng_dispose_previous(dst):
    """A tiny hand-built APNG exercising DISPOSE_PREVIOUS with
    alpha-blended sub-frames: the region a frame saves for restoring
    later lives outside the canvas, and a channel-order switch must
    convert it too, or the restore stamps the old order back in."""
    import zlib

    def chunk(t, body):
        return (struct.pack('>I', len(body)) + t + body
                + struct.pack('>I', zlib.crc32(t + body) & 0xffffffff))

    def raw(w, h, pix):
        rows = b''
        for y in range(h):
            rows += b'\x00' + b''.join(struct.pack('BBBB', *pix(x, y))
                                        for x in range(w))
        return zlib.compress(rows)

    W = H = 8
    seq = [0]

    def fctl(w, h, x, y, dispose, blend):
        s = struct.pack('>IIIIIHHBB', seq[0], w, h, x, y, 1, 10,
                        dispose, blend)
        seq[0] += 1
        return chunk(b'fcTL', s)

    def fdat(data):
        s = struct.pack('>I', seq[0]) + data
        seq[0] += 1
        return chunk(b'fdAT', s)

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0))
    out += chunk(b'acTL', struct.pack('>II', 4, 0))
    # frame 0: full canvas, opaque gradient, dispose NONE (it is what
    # frame 1's PREVIOUS restores, so it must stay on the canvas)
    out += fctl(W, H, 0, 0, 0, 0)
    out += chunk(b'IDAT', raw(W, H, lambda x, y: (x * 30, y * 30, 200, 255)))
    # frame 1: 4x4 patch at (2,2), half-transparent, blend OVER, dispose PREVIOUS
    out += fctl(4, 4, 2, 2, 2, 1)
    out += fdat(raw(4, 4, lambda x, y: (250, 20 + x * 40, 20 + y * 40, 128)))
    # frame 2: 4x4 patch at (0,4), opaque-ish, blend OVER, dispose NONE
    out += fctl(4, 4, 0, 4, 0, 1)
    out += fdat(raw(4, 4, lambda x, y: (10, 240, 60 + x * 20, 200)))
    # frame 3: 2x2 patch, blend SOURCE
    out += fctl(2, 2, 5, 1, 0, 0)
    out += fdat(raw(2, 2, lambda x, y: (90, 90, 250, 90)))
    out += chunk(b'IEND', b'')
    open(dst, 'wb').write(out)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out, exist_ok=True)
    j = lambda n: os.path.join(out, n)
    BIG = 7817178301          # the size that started all of this

    # The workflow rebuilds the fixtures before each sanitizer pass so
    # a clean cannot leave the run globbing nothing; the outputs are
    # deterministic, so an existing set is reused rather than encoded
    # again (the lossless animations are the expensive part).
    if all(os.path.exists(j(n)) for n in ('long_video.mp4', 'anim_lossless.webp',
            'still_lossless.webp', 'anim_lossless.png',
            'anim_dispose_prev.png', 'trailing_large.mp4',
            'trailing_huge.mp4', 'leading_huge.mp4', 'trailing_small.mp4')):
        print('fixtures present, not rebuilt')
        return

    # Frame-indexed animations for the windowed open, each just past
    # what the feeder may keep resident (head + lookahead + margin +
    # two huge pages, ~27 MB): lossless 720p frames, the WebP with a
    # second frame padded past the feeder's lookahead, plus a still
    # WebP the probe must reject from its first chunk.  Kept as small
    # as the checks allow - every byte is decoded three times under
    # the sanitizers.
    anim_webp(j('anim_lossless_base.webp'), 17, 1280, 720, 10)
    anim_webp_big_frame(j('anim_lossless_base.webp'),
                        j('anim_lossless.webp'), 12 * 1024 * 1024)
    os.unlink(j('anim_lossless_base.webp'))
    still_webp(j('still_lossless.webp'), 1920, 1080)
    apng(j('anim_lossless.png'), 30, 1280, 720, 10)
    apng_dispose_previous(j('anim_dispose_prev.png'))

    # A video longer than the feeder's fixed window span (4+8+8 MiB),
    # for the bitrate-sized feed check: 60 s at 4 Mbit/s is 30 MB.
    subprocess.check_call([
        'ffmpeg', '-v', 'error', '-y',
        '-f', 'lavfi', '-i', 'testsrc2=s=640x360:r=30', '-t', '60',
        '-c:v', 'libx264', '-preset', 'ultrafast', '-b:v', '4M',
        '-pix_fmt', 'yuv420p', '-an', '-movflags', '+faststart',
        j('long_video.mp4')])
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
        if f.endswith(('.mp4', '.webp', '.png')):
            p = j(f)
            print('%-24s %13d bytes  %s on disk'
                  % (f, os.path.getsize(p),
                     subprocess.check_output(['du', '-h', p])
                     .split()[0].decode()))


if __name__ == '__main__':
    main()
