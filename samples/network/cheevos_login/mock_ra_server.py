#!/usr/bin/env python3
"""Mock RetroAchievements-shaped HTTP/HTTPS server for cheevos_login_test.

Serves the exact request/response shape RetroArch's cheevos login uses
(POST /dorequest.php, urlencoded body, JSON reply) on one plain listener
and one TLS listener, plus misbehaving paths that reproduce real-world
transport conditions:

  POST /dorequest.php        validates the login POST byte-for-byte and
                             answers 200 JSON on a persistent connection
  POST /lying/dorequest.php  answers 200 JSON, then closes the socket
                             WITHOUT a "Connection: close" header -- the
                             idle-timeout behaviour that leaves a dead
                             connection in the client's pool
  GET  /partial              writes half a status line and closes
  GET  /stats                JSON {"connections": N} for this listener

Writes "READY plain=<port> tls=<port>" to the ready-file argument
once both listeners are bound, then serves until stdin reaches EOF (the C test holds the
pipe; server exits when the test does).
"""

import json
import socket
import ssl
import sys
import threading

LOGIN_BODY = b"r=login2&u=testuser&p=testpass1234"
LOGIN_REPLY = json.dumps({
    "Success": True,
    "User": "testuser",
    "Token": "0123456789ABCDEF",
    "Score": 100,
    "SoftcoreScore": 5,
    "Messages": 0,
    "Permissions": 1,
    "AccountType": "Registered",
}, separators=(",", ":")).encode()


class Listener:
    def __init__(self, name, tls_ctx=None):
        self.name = name
        self.tls_ctx = tls_ctx
        self.connections = 0
        self.lock = threading.Lock()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(16)
        self.port = self.sock.getsockname()[1]

    def serve(self):
        while True:
            try:
                conn, _ = self.sock.accept()
            except OSError:
                return
            with self.lock:
                self.connections += 1
            threading.Thread(target=self.handle, args=(conn,),
                             daemon=True).start()

    def handle(self, conn):
        try:
            if self.tls_ctx:
                conn = self.tls_ctx.wrap_socket(conn, server_side=True)
            conn.settimeout(30)
            while self.one_request(conn):
                pass
        except (OSError, ssl.SSLError):
            pass
        finally:
            try:
                conn.close()
            except OSError:
                pass

    def one_request(self, conn):
        """Serve one request; return True to keep the connection open."""
        raw = b""
        while b"\r\n\r\n" not in raw:
            chunk = conn.recv(4096)
            if not chunk:
                return False
            raw += chunk
        head, _, body = raw.partition(b"\r\n\r\n")
        lines = head.decode("latin-1").split("\r\n")
        method, path, _ = lines[0].split(" ", 2)
        headers = {}
        for line in lines[1:]:
            if ":" in line:
                k, v = line.split(":", 1)
                headers[k.strip().lower()] = v.strip()
        clen = int(headers.get("content-length", "0"))
        while len(body) < clen:
            chunk = conn.recv(4096)
            if not chunk:
                return False
            body += chunk

        if path == "/partial":
            conn.sendall(b"HTTP/1.1 20")
            return False

        if path == "/stats":
            with self.lock:
                n = self.connections
            payload = json.dumps({"connections": n}).encode()
            self.respond(conn, 200, payload)
            return True

        if path in ("/dorequest.php", "/lying/dorequest.php"):
            ok = (method == "POST"
                  and headers.get("content-type", "")
                      .startswith("application/x-www-form-urlencoded")
                  and clen == len(LOGIN_BODY)
                  and body == LOGIN_BODY
                  and "user-agent" in headers
                  and "host" in headers)
            if not ok:
                sys.stderr.write("[server %s] bad login request: %r %r\n"
                                 % (self.name, lines[0], body[:128]))
                self.respond(conn, 400, b'{"Success":false}')
                return True
            self.respond(conn, 200, LOGIN_REPLY)
            # The lie: no "Connection: close" was advertised, but the
            # connection is closed anyway, like an idle-timeout kill.
            return path == "/dorequest.php"

        self.respond(conn, 404, b"not found")
        return True

    @staticmethod
    def respond(conn, status, payload):
        reason = {200: "OK", 400: "Bad Request", 404: "Not Found"}[status]
        conn.sendall(("HTTP/1.1 %d %s\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %d\r\n"
                      "\r\n" % (status, reason, len(payload)))
                     .encode() + payload)


def main():
    cert, key, ready = sys.argv[1], sys.argv[2], sys.argv[3]
    tls_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls_ctx.load_cert_chain(cert, key)
    plain = Listener("plain")
    tls = Listener("tls", tls_ctx)
    threading.Thread(target=plain.serve, daemon=True).start()
    threading.Thread(target=tls.serve, daemon=True).start()
    with open(ready + ".tmp", "w") as f:
        f.write("READY plain=%d tls=%d\n" % (plain.port, tls.port))
    import os
    os.rename(ready + ".tmp", ready)
    # Exit when the C test (which holds our stdin pipe) goes away.
    sys.stdin.buffer.read()


if __name__ == "__main__":
    main()
