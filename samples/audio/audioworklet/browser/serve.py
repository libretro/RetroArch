#!/usr/bin/env python3
"""Serves a directory with the cross-origin isolation headers that
SharedArrayBuffer needs, and appends POSTed lines to log.txt there.
Usage: serve.py DIR PORT"""
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0))
        with open('log.txt', 'a') as f:
            f.write(self.rfile.read(n).decode() + '\n')
        self.send_response(200)
        self.end_headers()

    def log_message(self, *args):
        pass


os.chdir(sys.argv[1])
http.server.ThreadingHTTPServer(('127.0.0.1', int(sys.argv[2])),
                                Handler).serve_forever()
