#!/usr/bin/python

import os, sys, zlib, socket, struct

#ip = os.getenv("WIILOAD")
ip = "tcp:10.42.0.170"
assert ip.startswith("tcp:")
wii_ip = (ip[4:], 4299)

filename = sys.argv[1]

WIILOAD_VERSION_MAJOR=0
WIILOAD_VERSION_MINOR=5

len_uncompressed = os.path.getsize(filename)
c_data = zlib.compress(open(filename).read(), 6)

chunk_size = 1024*128
chunks = [c_data[i:i+chunk_size] for i  in range(0, len(c_data), chunk_size)]

args = [os.path.basename(filename)]+sys.argv[2:]
args = "\x00".join(args) + "\x00"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(wii_ip)

s.send("HAXX")
s.send(struct.pack("B", WIILOAD_VERSION_MAJOR)) # one byte, unsigned
s.send(struct.pack("B", WIILOAD_VERSION_MINOR)) # one byte, unsigned
s.send(struct.pack(">H",len(args))) # bigendian, 2 bytes, unsigned
s.send(struct.pack(">L",len(c_data))) # bigendian, 4 bytes, unsigned
s.send(struct.pack(">L",len_uncompressed)) # bigendian, 4 bytes, unsigned

print len(chunks),"chunks to send"
for piece in chunks:
    s.send(piece)
    sys.stdout.write("."); sys.stdout.flush()
sys.stdout.write("\n")

s.send(args)

s.close()
print "done"
