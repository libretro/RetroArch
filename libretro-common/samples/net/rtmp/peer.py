#!/usr/bin/env python3
"""Test the production handshake against an independent loopback RTMP peer."""
import argparse
import os
from pathlib import Path
import shlex
import queue
import socket
import struct
import subprocess
import tempfile
import threading
import time
from command import decode as decode_amf
from reply import record as reply_record


def replies(read):
    streams = {}
    chunk_size = 128
    records = bytearray()
    for _ in range(1024):
        first = read(1)[0]
        fmt, ident = first >> 6, first & 63
        if ident == 0:
            ident = 64 + read(1)[0]
        elif ident == 1:
            ident = 64 + int.from_bytes(read(2), 'little')
        previous = streams.get(ident)
        if fmt == 0:
            header = read(11)
            stamp = int.from_bytes(header[:3], 'big')
            current = {'length': int.from_bytes(header[3:6], 'big'),
                       'type': header[6], 'stream': int.from_bytes(header[7:], 'little'),
                       'extended': stamp == 0xffffff, 'data': bytearray()}
            if current['extended']:
                stamp = int.from_bytes(read(4), 'big')
            current.update(time=stamp, delta=stamp, stamp=stamp)
        else:
            assert previous is not None, 'missing server chunk history'
            current = previous
            if fmt in (1, 2):
                assert not current['data'], 'server changed header in partial message'
                header = read(7 if fmt == 1 else 3)
                stamp = int.from_bytes(header[:3], 'big')
                current['extended'] = stamp == 0xffffff
                if current['extended']:
                    stamp = int.from_bytes(read(4), 'big')
                current.update(stamp=stamp, delta=stamp, time=(current['time'] + stamp) & 0xffffffff)
                if fmt == 1:
                    current.update(length=int.from_bytes(header[3:6], 'big'), type=header[6])
            else:
                if current['extended']:
                    assert int.from_bytes(read(4), 'big') == current['stamp']
                if not current['data']:
                    current['time'] = (current['time'] + current['delta']) & 0xffffffff
        streams[ident] = current
        assert current['length'] <= 8192 and len(streams) <= 16
        current['data'].extend(read(min(chunk_size, current['length'] - len(current['data']))))
        if len(current['data']) != current['length']:
            continue
        payload = bytes(current['data'])
        records.extend(struct.pack('>IIIII', ident, current['stream'], current['type'],
                                   current['time'], len(payload)) + payload)
        current['data'].clear()
        if current['type'] == 1:
            assert len(payload) == 4
            chunk_size = int.from_bytes(payload, 'big')
            assert 0 < chunk_size <= 0x7fffffff
        if current['type'] == 20 and payload.startswith(b'\x02\x00\x07_result'):
            return records, payload
    raise RuntimeError('no complete connect result within server chunk bound')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('--receive-binary', required=True)
    parser.add_argument('--command-binary', required=True)
    parser.add_argument('--reply-binary', required=True)
    parser.add_argument('binary')
    args = parser.parse_args()
    ffmpeg = os.environ.get('FFMPEG', 'ffmpeg')
    help_text = subprocess.run([ffmpeg, '-hide_banner', '-h', 'protocol=rtmp'],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               check=True, timeout=10).stdout
    if b'-rtmp_listen' not in help_text:
        raise RuntimeError('FFMPEG must name a build with the native RTMP listener')
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    with tempfile.TemporaryFile() as log, tempfile.TemporaryFile() as errors:
        server = subprocess.Popen([ffmpeg, '-v', 'error', '-listen', '1', '-timeout', '10', '-i',
                                   f'rtmp://127.0.0.1:{port}/live/test', '-f', 'null', '-'],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=log)
        client = None
        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    connection = socket.create_connection(('127.0.0.1', port), timeout=5)
                    break
                except ConnectionRefusedError:
                    if server.poll() is not None or time.monotonic() >= deadline:
                        log.seek(0)
                        raise RuntimeError('RTMP listener failed: ' + log.read().decode(errors='replace'))
                    time.sleep(0.01)
            client = subprocess.Popen(shlex.split(args.runner) +
                                      [str(Path(args.binary).resolve()), '--peer'],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors)
            with connection:
                connection.settimeout(5)

                def receive(size):
                    result = bytearray()
                    while len(result) < size:
                        part = connection.recv(size - len(result))
                        if not part:
                            raise RuntimeError('RTMP peer closed during handshake')
                        result.extend(part)
                    return result

                def read_output(size):
                    result = queue.Queue()
                    reader = threading.Thread(target=lambda: result.put(client.stdout.read(size)),
                                              daemon=True)
                    reader.start()
                    return result.get(timeout=10)

                first = read_output(1537)
                assert len(first) == 1537
                connection.sendall(first)
                client.stdin.write(receive(1537))
                client.stdin.flush()
                second = read_output(1536)
                assert len(second) == 1536
                connection.sendall(second)
                client.stdin.write(receive(1536))
                client.stdin.close()
                result = client.wait(timeout=10)
                if result:
                    errors.seek(0)
                    raise RuntimeError(errors.read().decode(errors='replace'))
                with tempfile.TemporaryDirectory(prefix='rtmp-peer-command-') as temp:
                    output = Path(temp) / 'connect.bin'
                    subprocess.run(shlex.split(args.runner) +
                                   [str(Path(args.command_binary).resolve()), '--connect', str(output),
                                    f'rtmp://127.0.0.1:{port}/live'], check=True, timeout=30)
                    command = output.read_bytes()
                header = b'\x03\x00\x00\x00' + len(command).to_bytes(3, 'big') + b'\x14\x00\x00\x00\x00'
                connection.sendall(header + command[:128])
                for offset in range(128, len(command), 128):
                    connection.sendall(b'\xc3' + command[offset:offset + 128])
                reply = bytearray()
                reply_deadline = time.monotonic() + 10

                def read_reply(size):
                    remaining = reply_deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError('server connect result deadline expired')
                    connection.settimeout(min(5, remaining))
                    data = receive(size)
                    reply.extend(data)
                    assert len(reply) <= 131072, 'unbounded connect response'
                    return data

                expected, command_reply = replies(read_reply)
                with tempfile.TemporaryDirectory(prefix='rtmp-peer-reply-') as temp:
                    source, output = Path(temp) / 'input.bin', Path(temp) / 'output.bin'
                    source.write_bytes(reply)
                    subprocess.run(shlex.split(args.runner) +
                                   [str(Path(args.receive_binary).resolve()), str(source), str(output)],
                                   check=True, timeout=30)
                    assert output.read_bytes() == expected, 'production server-reply parsing differs'
                    source.write_bytes(b'\x14' + command_reply)
                    subprocess.run(shlex.split(args.runner) +
                                   [str(Path(args.reply_binary).resolve()), str(source), str(output)],
                                   check=True, timeout=30)
                    decoded = decode_amf(command_reply)
                    assert decoded[0] == '_result' and decoded[1] == 1
                    assert decoded[3]['code'] == 'NetConnection.Connect.Success'
                    normalized = reply_record(int(decoded[1]), None, decoded[0],
                                              decoded[3].get('code', ''), decoded[3].get('level', ''),
                                              decoded[3].get('description', ''))
                    assert output.read_bytes() == normalized, 'production AMF reply interpretation differs'
        finally:
            if client and client.poll() is None:
                client.kill()
                client.wait()
            if server.poll() is None:
                server.terminate()
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
    print('PASS: production RTMP handshake, connect and interpreted AMF reply with FFmpeg listener')


if __name__ == '__main__':
    main()
