#!/usr/bin/env python3
"""Independent AMF reply fixtures and semantic extraction expectations."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


def text(value):
    data = value.encode('utf-8')
    return len(data).to_bytes(2, 'big') + data


def amf(value):
    if value is None:
        return b'\x05'
    if isinstance(value, bool):
        return b'\x01' + bytes([255 if value else 0])
    if isinstance(value, (int, float)):
        return b'\x00' + struct.pack('>d', value)
    if isinstance(value, str):
        return b'\x02' + text(value)
    if isinstance(value, dict):
        return b'\x03' + b''.join(text(k) + amf(v) for k, v in value.items()) + b'\x00\x00\x09'
    return b'\x0a' + len(value).to_bytes(4, 'big') + b''.join(amf(v) for v in value)


def record(transaction, stream, command, code='', level='', description=''):
    output = b'\x01' + struct.pack('>II', transaction, stream or 0) + bytes([stream is not None])
    for value in (command, code, level, description):
        data = value.encode('utf-8')
        output += len(data).to_bytes(4, 'big') + data
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner', default='')
    parser.add_argument('binary')
    args = parser.parse_args()
    cases = []
    prefix = amf('_result') + amf(1) + amf({'capabilities': 31, 'fmsVer': 'test'})
    info = {'level': 'status', 'code': 'NetConnection.Connect.Success', 'description': 'prêt'}
    cases.append((b'\x14' + prefix + amf(info),
                  record(1, None, '_result', info['code'], info['level'], info['description'])))
    for number in (0, 1, 2, 0x1000001, 0x80000000, 0xffffffff):
        data = b''.join(amf(v) for v in ('_result', number, None, number))
        cases.append((b'\x14' + data, record(number, number, '_result')))
        cases.append((b'\x11\x00' + data, record(number, number, '_result')))
    status = {'nested': {'code': 'spoof'}, 'extra': [True, None, {'level': 'wrong'}],
              'code': 'NetStream.Publish.Start', 'level': 'status'}
    cases.append((b'\x14' + b''.join(amf(v) for v in ('onStatus', 0, None, status)),
                  record(0, None, 'onStatus', status['code'], status['level'])))
    error = {'code': 'NetStream.Publish.BadName', 'level': 'error'}
    cases.append((b'\x14' + b''.join(amf(v) for v in ('_error', 2, None, error)),
                  record(2, None, '_error', error['code'], error['level'])))
    cases.append((b'\x14' + amf('onFCPublish'), record(0, None, 'onFCPublish')))
    cases.append((b'\x11\x00' + amf('onFCPublish'), record(0, None, 'onFCPublish')))
    for name in ('_result', '_error', 'onStatus'):
        cases.append((b'\x14' + amf(name), b'\x00'))
    for number in (-1, 0.5, float('inf'), float('nan'), 0x100000000):
        cases.append((b'\x14' + amf('_result') + amf(number) + amf(None), b'\x00'))
        cases.append((b'\x14' + amf('_result') + amf(2) + amf(None) + amf(number), b'\x00'))
    duplicate = b'\x03' + text('code') + amf('first') + text('code') + amf('second') + b'\x00\x00\x09'
    cases.append((b'\x14' + prefix + duplicate, b'\x00'))
    deep = None
    for _ in range(9):
        deep = {'child': deep}
    cases.append((b'\x14' + prefix + amf(deep), b'\x00'))
    cases.append((b'\x14' + prefix + b'\x11', b'\x00'))
    status_payload = amf(info)
    status_expected = record(1, None, '_result', info['code'], info['level'], info['description'])
    cases.append((b'\x14' + prefix + b'\x08\xff\xff\xff\xff' + status_payload[1:], status_expected))
    cases.append((b'\x14' + prefix + b'\x10' + text('Status') + status_payload[1:], status_expected))
    extras = (text('reference') + b'\x07\x00\x00' + text('date') + b'\x0b' + bytes(10) +
              text('xml') + b'\x0f\x00\x00\x00\x04<a/>' + text('undefined') + b'\x06')
    cases.append((b'\x14' + prefix + b'\x03' + extras + status_payload[1:], status_expected))
    cases.append((b'\x14' + prefix + b'\x03' + text('ref') + b'\x07\xff\xff\x00\x00\x09', b'\x00'))
    cases.append((b'\x14' + prefix + b'\x0a\xff\xff\xff\xff', b'\x00'))
    cases.append((b'\x14' + prefix + amf({'code': None}), b'\x00'))
    with tempfile.TemporaryDirectory(prefix='rtmp-reply-') as temp:
        source, output = Path(temp) / 'input.bin', Path(temp) / 'output.bin'
        for index, (wire, expected) in enumerate(cases):
            source.write_bytes(wire)
            subprocess.run(shlex.split(args.runner) + [str(Path(args.binary).resolve()),
                           str(source), str(output)], check=True, timeout=30)
            assert output.read_bytes() == expected, f'AMF reply fixture {index} differs'
    print(f'PASS: {len(cases)} independent AMF reply ID/status/error/depth fixtures')


if __name__ == '__main__':
    main()
