#!/usr/bin/env python3
"""A minimal NFSv4.2 server for testing READ_PLUS (RFC 7862).

It serves one directory as the export /export and answers only what the
built-in client uses to read a file: the session operations, a walk to
the file (PUTROOTFH, PUTFH, LOOKUP, GETFH, GETATTR), READ, and
READ_PLUS. READ_PLUS reports the runs of the file the filesystem really
holds as holes (SEEK_DATA / SEEK_HOLE), so the client under test sees a
server's view of a sparse file, not one it was told to expect.

Its encoding is checked apart from the client: the test captures a
session with tshark and Wireshark's own NFS dissector decodes the
READ_PLUS replies.

Usage: fake_nfs42.py <port> <directory>
"""

import os
import socket
import socketserver
import struct
import sys
import threading

NFS4_OK, NFS4ERR_NOENT, NFS4ERR_IO, NFS4ERR_NOTDIR = 0, 2, 5, 20
NFS4ERR_INVAL, NFS4ERR_NOTSUPP, NFS4ERR_BADXDR = 22, 10004, 10036
NFS4ERR_OP_ILLEGAL, NFS4ERR_MINOR_VERS_MISMATCH = 10044, 10021
NFS4ERR_NOFILEHANDLE = 10020

OP_GETATTR, OP_GETFH, OP_LOOKUP, OP_PUTFH, OP_PUTROOTFH = 9, 10, 15, 22, 24
OP_READ, OP_EXCHANGE_ID, OP_CREATE_SESSION, OP_SEQUENCE = 25, 42, 43, 53
OP_RECLAIM_COMPLETE, OP_READ_PLUS = 58, 68

NF4REG, NF4DIR = 1, 2
CONTENT_DATA, CONTENT_HOLE = 0, 1
ROOT = b"\x00root"            # the pseudo root, above /export


class X:
    """XDR reader."""

    def __init__(self, b):
        self.b, self.p = b, 0

    def u32(self):
        v = struct.unpack_from(">I", self.b, self.p)[0]
        self.p += 4
        return v

    def u64(self):
        v = struct.unpack_from(">Q", self.b, self.p)[0]
        self.p += 8
        return v

    def fixed(self, n):
        v = self.b[self.p:self.p + n]
        if len(v) != n:
            raise ValueError("short")
        self.p += (n + 3) & ~3
        return v

    def opaque(self):
        return self.fixed(self.u32())


def u32(v):
    return struct.pack(">I", v)


def u64(v):
    return struct.pack(">Q", v)


def opaque(b):
    return u32(len(b)) + b + b"\x00" * ((4 - len(b) % 4) % 4)


class Server:
    def __init__(self, root):
        self.root = os.path.realpath(root)
        self.lock = threading.Lock()
        self.clients = 0

    def path(self, fh):
        if fh == ROOT:
            return None
        rel = fh.decode()
        full = os.path.realpath(os.path.join(self.root, rel))
        if not (full == self.root or full.startswith(self.root + os.sep)):
            raise ValueError("outside the export")
        return full

    def getattr(self, fh, words):
        full = self.path(fh)
        if full is None:
            ftype, size, mtime = NF4DIR, 0, 0
        else:
            st = os.stat(full)
            ftype = NF4DIR if os.path.isdir(full) else NF4REG
            size, mtime = st.st_size, int(st.st_mtime)
        w0 = words[0] if words else 0
        w1 = words[1] if len(words) > 1 else 0
        out_w0, out_w1, vals = 0, 0, b""
        if w0 & (1 << 1):
            out_w0 |= 1 << 1
            vals += u32(ftype)
        if w0 & (1 << 4):
            out_w0 |= 1 << 4
            vals += u64(size)
        if w1 & (1 << (53 - 32)):
            out_w1 |= 1 << (53 - 32)
            vals += u64(mtime) + u32(0)
        return u32(2) + u32(out_w0) + u32(out_w1) + opaque(vals)

    def runs(self, full, off, count):
        """The file from @off, at most @count octets, as (kind, offset,
        length) runs from the filesystem's own map of data and holes."""
        size = os.path.getsize(full)
        end = min(off + count, size)
        out = []
        fd = os.open(full, os.O_RDONLY)
        try:
            pos = off
            while pos < end:
                try:
                    data = os.lseek(fd, pos, os.SEEK_DATA)
                except OSError:
                    data = size             # no data after pos: a hole to EOF
                if data > pos:              # a hole first
                    hole_end = min(data, end)
                    out.append((CONTENT_HOLE, pos, hole_end - pos))
                    pos = hole_end
                    continue
                hole = os.lseek(fd, pos, os.SEEK_HOLE)
                data_end = min(hole, end)
                os.lseek(fd, pos, os.SEEK_SET)
                out.append((CONTENT_DATA, pos, os.read(fd, data_end - pos)))
                pos = data_end
        finally:
            os.close(fd)
        return out, end >= size

    def compound(self, x):
        tag = x.opaque()
        minor = x.u32()
        nops = x.u32()
        if minor not in (1, 2):
            return u32(NFS4ERR_MINOR_VERS_MISMATCH) + opaque(tag) + u32(0)
        res, status, cfh = [], NFS4_OK, None
        for _ in range(nops):
            op = x.u32()
            st, body = NFS4_OK, b""
            if op == OP_SEQUENCE:
                sid = x.fixed(16)
                seq, slot, hi, _cache = x.u32(), x.u32(), x.u32(), x.u32()
                body = sid + u32(seq) + u32(slot) + u32(hi) + u32(hi) + u32(0)
            elif op == OP_EXCHANGE_ID:
                x.fixed(8)                    # verifier
                x.opaque()                    # owner id
                x.u32()                       # flags
                if x.u32() != 0:              # state protection: none only
                    st = NFS4ERR_NOTSUPP
                for _i in range(x.u32()):     # client implementation id
                    x.opaque(); x.opaque(); x.u64(); x.u32()
                with self.lock:
                    self.clients += 1
                    cid = self.clients
                body = (u64(cid) + u32(1) + u32(0x00010000) + u32(0) +
                        u64(0) + opaque(b"fake") + opaque(b"fake") + u32(0))
            elif op == OP_CREATE_SESSION:
                cid = x.u64(); seq = x.u32(); x.u32()
                chans = []
                for _c in range(2):
                    a = [x.u32() for _k in range(6)]
                    for _r in range(x.u32()):
                        x.u32()
                    chans.append(a)
                x.u32()                       # callback program
                for _s in range(x.u32()):
                    flavor = x.u32()
                    if flavor == 1:           # AUTH_SYS parameters
                        x.u32(); x.opaque(); x.u32(); x.u32()
                        for _g in range(x.u32()):
                            x.u32()
                sid = u64(cid) + u64(0x5eed)
                attrs = b""
                for a in chans:
                    attrs += b"".join(u32(v) for v in a) + u32(0)
                body = sid + u32(seq) + u32(0) + attrs
            elif op == OP_RECLAIM_COMPLETE:
                x.u32()
            elif op == OP_PUTROOTFH:
                cfh = ROOT
            elif op == OP_PUTFH:
                cfh = x.opaque()
            elif op == OP_LOOKUP:
                name = x.opaque()
                if cfh is None:
                    st = NFS4ERR_NOFILEHANDLE
                elif cfh == ROOT:
                    if name == b"export":
                        cfh = b"."
                    else:
                        st = NFS4ERR_NOENT
                else:
                    rel = os.path.normpath(os.path.join(cfh.decode(), name.decode()))
                    full = self.path(rel.encode())
                    if not os.path.exists(full):
                        st = NFS4ERR_NOENT
                    else:
                        cfh = rel.encode()
            elif op == OP_GETFH:
                if cfh is None:
                    st = NFS4ERR_NOFILEHANDLE
                else:
                    body = opaque(cfh)
            elif op == OP_GETATTR:
                words = [x.u32() for _w in range(x.u32())]
                if cfh is None:
                    st = NFS4ERR_NOFILEHANDLE
                else:
                    body = self.getattr(cfh, words)
            elif op in (OP_READ, OP_READ_PLUS):
                x.fixed(16)                   # stateid
                off, count = x.u64(), x.u32()
                full = self.path(cfh) if cfh not in (None, ROOT) else None
                if full is None or os.path.isdir(full):
                    st = NFS4ERR_INVAL
                else:
                    runs, eof = self.runs(full, off, count)
                    if op == OP_READ:
                        data = b"".join(d if k == CONTENT_DATA else b"\x00" * d
                                        for k, _o, d in runs)
                        body = u32(1 if eof else 0) + opaque(data)
                    else:
                        body = u32(1 if eof else 0) + u32(len(runs))
                        for kind, o, d in runs:
                            if kind == CONTENT_DATA:
                                body += u32(CONTENT_DATA) + u64(o) + opaque(d)
                            else:
                                body += u32(CONTENT_HOLE) + u64(o) + u64(d)
            else:
                res.append(u32(NFS4ERR_OP_ILLEGAL) + u32(NFS4ERR_OP_ILLEGAL))
                status = NFS4ERR_OP_ILLEGAL
                break
            res.append(u32(op) + u32(st) + body)
            if st != NFS4_OK:
                status = st
                break
        return u32(status) + opaque(tag) + u32(len(res)) + b"".join(res)


class Handler(socketserver.BaseRequestHandler):
    def recv_exact(self, n):
        b = b""
        while len(b) < n:
            chunk = self.request.recv(n - len(b))
            if not chunk:
                raise EOFError
            b += chunk
        return b

    def handle(self):
        srv = self.server.nfs
        try:
            while True:
                msg = b""
                while True:
                    mark = struct.unpack(">I", self.recv_exact(4))[0]
                    msg += self.recv_exact(mark & 0x7fffffff)
                    if mark & 0x80000000:
                        break
                x = X(msg)
                xid, mtype = x.u32(), x.u32()
                x.u32(); x.u32(); x.u32()     # rpcvers, program, version
                proc = x.u32()
                x.u32(); x.opaque()           # credential
                x.u32(); x.opaque()           # verifier
                results = b""
                if proc == 1:
                    try:
                        results = srv.compound(x)
                    except (ValueError, struct.error, UnicodeDecodeError):
                        results = u32(NFS4ERR_BADXDR) + opaque(b"") + u32(0)
                reply = (u32(xid) + u32(1) + u32(0) + u32(0) + u32(0) + u32(0)
                         + results)
                self.request.sendall(u32(0x80000000 | len(reply)) + reply)
        except (EOFError, ConnectionError):
            pass


class TCP(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    port, root = int(sys.argv[1]), sys.argv[2]
    with TCP(("127.0.0.1", port), Handler) as s:
        s.nfs = Server(root)
        s.serve_forever()


if __name__ == "__main__":
    main()
