#!/usr/bin/env python3
"""Drive the production session through an independent RTMP publisher handshake."""
import argparse
import os
from pathlib import Path
import queue
import shlex
import socket
import struct
import subprocess
import tempfile
import threading
import time


def native_ffmpeg():
    ffmpeg = os.environ.get('FFMPEG', 'ffmpeg')
    help_text = subprocess.run([ffmpeg, '-hide_banner', '-h', 'protocol=rtmp'],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               check=True, timeout=10).stdout
    if b'-rtmp_listen' not in help_text:
        raise RuntimeError('FFMPEG must name a build with the native RTMP listener')
    return ffmpeg


def run(binary, runner, legacy):
    ffmpeg = native_ffmpeg()
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    url = f'rtmp://127.0.0.1:{port}/live'
    with tempfile.TemporaryFile() as server_log, tempfile.TemporaryFile() as client_log:
        server = subprocess.Popen([ffmpeg, '-v', 'debug',
                                   '-listen', '1', '-timeout', '10', '-i', url + '/test',
                                   '-f', 'null', '-'], stdin=subprocess.DEVNULL,
                                  stdout=subprocess.DEVNULL, stderr=server_log)
        client = None
        try:
            deadline = time.monotonic() + 20
            while True:
                try:
                    connection = socket.create_connection(('127.0.0.1', port), timeout=3)
                    break
                except ConnectionRefusedError:
                    if server.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError('RTMP listener did not start')
                    time.sleep(0.01)
            client = subprocess.Popen(shlex.split(runner) + [str(Path(binary).resolve()),
                                      '--peer', url, 'legacy' if legacy else 'standard'],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=client_log)
            frames = queue.Queue()

            def read_frames():
                try:
                    while True:
                        header = client.stdout.read(8)
                        if len(header) != 8:
                            raise EOFError('session bridge closed before READY')
                        state, size = struct.unpack('>II', header)
                        if size > 65536:
                            raise ValueError('unbounded session output')
                        data = client.stdout.read(size)
                        if len(data) != size:
                            raise EOFError('partial session output')
                        frames.put((state, data))
                except Exception as error:
                    frames.put(error)

            threading.Thread(target=read_frames, daemon=True).start()
            with connection:
                connection.settimeout(5)
                for _ in range(4096):
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError('publish session deadline expired')
                    frame = frames.get(timeout=remaining)
                    if isinstance(frame, Exception):
                        raise frame
                    state, data = frame
                    if data:
                        connection.sendall(data)
                    if state == 0xffffffff:
                        raise RuntimeError('production session failed')
                    if state == 2:
                        assert client.wait(timeout=5) == 0
                        break
                    incoming = connection.recv(97)
                    if not incoming:
                        raise EOFError('independent receiver closed before publish readiness')
                    client.stdin.write(struct.pack('>I', len(incoming)) + incoming)
                    client.stdin.flush()
                else:
                    raise RuntimeError('publish session exceeded fragment bound')
        except Exception:
            server_log.seek(0)
            client_log.seek(0)
            print(server_log.read().decode(errors='replace'))
            print(client_log.read().decode(errors='replace'))
            raise
        finally:
            if client:
                if client.poll() is None:
                    client.kill()
                client.wait(timeout=5)
                client.stdin.close()
                client.stdout.close()
            if server.poll() is None:
                server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)
    print('PASS complete production RTMP publish session with FFmpeg receiver: ' +
          ('legacy commands' if legacy else 'standard commands'))


def run_socket(binary, runner, legacy):
    ffmpeg = native_ffmpeg()
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    url = f'rtmp://127.0.0.1:{port}/live'
    with tempfile.TemporaryFile() as server_log, tempfile.TemporaryFile() as client_log:
        server = subprocess.Popen([ffmpeg, '-v', 'debug', '-listen', '1', '-timeout', '10',
                                   '-i', url + '/test', '-f', 'null', '-'],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                  stderr=server_log)
        try:
            deadline = time.monotonic() + 20
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError('native TCP publish deadline expired')
                result = subprocess.run(shlex.split(runner) + [str(Path(binary).resolve()),
                                        url, str(port), 'legacy' if legacy else 'standard'],
                                       stdin=subprocess.DEVNULL, stdout=client_log, stderr=client_log,
                                       timeout=min(15, remaining))
                if result.returncode == 0:
                    break
                if result.returncode != 77 or server.poll() is not None:
                    raise RuntimeError(f'native TCP publisher failed: {result.returncode}')
                time.sleep(0.01)
        except Exception:
            server_log.seek(0)
            client_log.seek(0)
            print(server_log.read().decode(errors='replace'))
            print(client_log.read().decode(errors='replace'))
            raise
        finally:
            if server.poll() is None:
                server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)
    print('PASS native TCP RTMP publish with independent receiver: ' +
          ('legacy commands' if legacy else 'standard commands'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('--socket-binary')
    parser.add_argument('binary')
    args = parser.parse_args()
    if args.socket_binary:
        run_socket(args.socket_binary, args.runner, False)
        run_socket(args.socket_binary, args.runner, True)
    else:
        run(args.binary, args.runner, False)
        run(args.binary, args.runner, True)
