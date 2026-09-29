#!/usr/bin/env python3
"""relay.py LISTEN_PORT TARGET_PORT OUT_DIR: forward each connection on
LISTEN_PORT to 127.0.0.1:TARGET_PORT and save what the server sent, one
file per connection, as a fuzz seed."""
import socket, sys, threading, os
lp, tp, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
os.makedirs(out, exist_ok=True)
n = 0
def pump(a, b, buf):
    try:
        while True:
            d = a.recv(65536)
            if not d: break
            if buf is not None: buf.append(d)
            b.sendall(d)
    except OSError: pass
    try: b.shutdown(socket.SHUT_WR)
    except OSError: pass
def handle(c, i):
    s = socket.create_connection(("127.0.0.1", tp))
    got = []
    t = threading.Thread(target=pump, args=(c, s, None)); t.start()
    pump(s, c, got); t.join()
    c.close(); s.close()
    open(os.path.join(out, "seed_%d" % i), "wb").write(b"".join(got))
ls = socket.socket(); ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(("127.0.0.1", lp)); ls.listen(8)
while True:
    c, _ = ls.accept(); n += 1
    threading.Thread(target=handle, args=(c, n)).start()
