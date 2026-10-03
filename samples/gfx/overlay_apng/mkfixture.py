import zlib, struct
def chunk(t, d):
    c = t + d
    return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
W = H = 4
def idat(color):
    raw = b''.join(b'\x00' + bytes(color) * W for _ in range(H))
    return zlib.compress(raw)
def fctl(seq):
    # seq, w, h, x, y, delay_num, delay_den, dispose, blend
    return struct.pack('>IIIIIHHBB', seq, W, H, 0, 0, 1, 10, 0, 0)
png  = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0))
png += chunk(b'acTL', struct.pack('>II', 3, 0))
s = 0
png += chunk(b'fcTL', fctl(s)); s += 1
png += chunk(b'IDAT', idat((255, 0, 0, 255)))
for col in ((0, 255, 0, 255), (0, 0, 255, 255)):
    png += chunk(b'fcTL', fctl(s)); s += 1
    png += chunk(b'fdAT', struct.pack('>I', s) + idat(col)); s += 1
png += chunk(b'IEND', b'')
open('fixtures/anim.png', 'wb').write(png)
print('bytes', len(png))
