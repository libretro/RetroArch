#!/usr/bin/env python3
"""Writes <core>.sig, the signature RetroArch checks a core against.

  core_sign.py sign --key KEY CORE [--name NAME] [--build-time SECONDS]
                    [--out FILE]
  core_sign.py pubkey --key KEY
  core_sign.py --self-test

KEY is a PEM private key file or a PKCS#11 URI (pkcs11:...), the latter
through OpenSSL's pkcs11 provider. Signing needs OpenSSL 3.2 or newer
for RFC 6979 deterministic nonces, and checks them against RFC 6979's
own P-256 vector before every PEM signing. An older or missing OpenSSL
is refused with exit status 3; any other failure exits 1. OPENSSL names
the openssl binary.

The .sig is big-endian:

  "RACSIG01" (8) | key id (8) | build time (8) | core SHA-256 (32)
  | name length (1) | name (1-255) | r || s (64)

The key id is the first 8 bytes of the SHA-256 of the 65-byte public
key, the build time is never 0, and r || s signs the SHA-256 of every
byte before it.

pubkey prints the 65-byte public key as a core_signing_keys.h entry.
"""
import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile
import time

MAGIC = b'RACSIG01'
EXIT_REFUSED = 3
SPKI_P256 = bytes.fromhex(
    '3059301306072a8648ce3d020106082a8648ce3d030107034200')
# RFC 6979 A.2.5: P-256, SHA-256, message "sample"
RFC6979_D = bytes.fromhex(
    'c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721')
RFC6979_RS = bytes.fromhex(
    'efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716'
    'f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8')


class Refused(Exception):
    pass


class Failed(Exception):
    pass


def openssl():
    return os.environ.get('OPENSSL', 'openssl')


def run(args):
    proc = subprocess.run(args, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
    if proc.returncode != 0:
        raise Failed('%s failed: %s' % (' '.join(args[:2]),
                     proc.stderr.decode(errors='replace').strip()))
    return proc.stdout


def openssl_version(text):
    m = re.match(r'OpenSSL (\d+)\.(\d+)\.', text)
    return (int(m.group(1)), int(m.group(2))) if m else None


def check_openssl():
    try:
        text = run([openssl(), 'version']).decode(errors='replace')
    except OSError as e:
        raise Refused('cannot run %s: %s' % (openssl(), e))
    version = openssl_version(text)
    if not version or version < (3, 2):
        raise Refused('OpenSSL 3.2 or newer is needed for RFC 6979 '
                      'nonces, found: %s' % text.strip())


def sec1_pem(d):
    """An EC PRIVATE KEY PEM for the P-256 scalar d (32 bytes)."""
    import base64
    oid = bytes.fromhex('06082a8648ce3d030107')
    body = (bytes.fromhex('020101') + b'\x04\x20' + d
            + bytes([0xa0, len(oid)]) + oid)
    der = bytes([0x30, len(body)]) + body
    b64 = base64.encodebytes(der).decode().replace('\n', '')
    lines = [b64[i:i + 64] for i in range(0, len(b64), 64)]
    return ('-----BEGIN EC PRIVATE KEY-----\n' + '\n'.join(lines)
            + '\n-----END EC PRIVATE KEY-----\n').encode()


def der_to_raw(der):
    """Strict DER ECDSA-Sig-Value to the 64-byte r || s."""
    def integer(i):
        if i + 2 > len(der) or der[i] != 0x02:
            raise ValueError('expected INTEGER')
        n = der[i + 1]
        v = der[i + 2:i + 2 + n]
        if n < 1 or n > 33 or len(v) != n or v[0] & 0x80:
            raise ValueError('bad INTEGER')
        if n > 1 and v[0] == 0 and not v[1] & 0x80:
            raise ValueError('non-minimal INTEGER')
        v = v.lstrip(b'\0')
        if not v or len(v) > 32:
            raise ValueError('INTEGER out of range')
        return v.rjust(32, b'\0'), i + 2 + n
    if len(der) < 2 or der[0] != 0x30 or der[1] != len(der) - 2:
        raise ValueError('expected one SEQUENCE')
    r, i = integer(2)
    s, i = integer(i)
    if i != len(der):
        raise ValueError('trailing bytes')
    return r + s


def build_header(key_id, build_time, core_sha256, name):
    raw = name.encode('utf-8')
    if (not 1 <= len(raw) <= 255 or b'\0' in raw or b'/' in raw
            or b'\\' in raw):
        raise ValueError('bad core name: %r' % name)
    if len(key_id) != 8 or len(core_sha256) != 32:
        raise ValueError('bad key id or hash')
    # RetroArch reads 0 as unsigned, never too old
    if not 0 < build_time < 1 << 64:
        raise ValueError('build time out of range: %d' % build_time)
    return (MAGIC + key_id + struct.pack('>Q', build_time) + core_sha256
            + bytes([len(raw)]) + raw)


def key_args(key, flag):
    if key.startswith('pkcs11:'):
        return ['-provider', 'pkcs11', '-provider', 'default', flag, key]
    return [flag, key]


def public_key(key):
    der = run([openssl(), 'pkey'] + key_args(key, '-in')
              + ['-pubout', '-outform', 'DER'])
    if (len(der) != len(SPKI_P256) + 65 or not der.startswith(SPKI_P256)
            or der[len(SPKI_P256)] != 4):
        raise Failed('not a P-256 key: %s' % key)
    return der[len(SPKI_P256):]


def sign_digest(key, digest, tmp):
    dpath = os.path.join(tmp, 'digest')
    spath = os.path.join(tmp, 'sig.der')
    with open(dpath, 'wb') as f:
        f.write(digest)
    args = ([openssl(), 'pkeyutl', '-sign'] + key_args(key, '-inkey')
            + ['-in', dpath, '-out', spath, '-pkeyopt', 'digest:sha256'])
    # A token makes its own nonces
    if not key.startswith('pkcs11:'):
        args += ['-pkeyopt', 'nonce-type:1']
    run(args)
    with open(spath, 'rb') as f:
        return der_to_raw(f.read())


def verify_digest(pub, digest, rs, tmp):
    """Checks rs with openssl before it is written out."""
    def der_int(v):
        v = v.lstrip(b'\0') or b'\0'
        if v[0] & 0x80:
            v = b'\0' + v
        return bytes([0x02, len(v)]) + v
    body = der_int(rs[:32]) + der_int(rs[32:])
    paths = [os.path.join(tmp, n) for n in ('pub.der', 'digest', 'v.der')]
    for path, data in zip(paths, (SPKI_P256 + pub, digest,
                                  bytes([0x30, len(body)]) + body)):
        with open(path, 'wb') as f:
            f.write(data)
    run([openssl(), 'pkeyutl', '-verify', '-pubin', '-keyform', 'DER',
         '-inkey', paths[0], '-in', paths[1], '-sigfile', paths[2],
         '-pkeyopt', 'digest:sha256'])


def check_rfc6979(tmp):
    """Fails on an OpenSSL whose nonces are not RFC 6979's."""
    kpath = os.path.join(tmp, 'rfc6979.pem')
    with open(kpath, 'wb') as f:
        f.write(sec1_pem(RFC6979_D))
    rs = sign_digest(kpath, hashlib.sha256(b'sample').digest(), tmp)
    if rs != RFC6979_RS:
        raise Failed('OpenSSL did not sign with RFC 6979 nonces')


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.digest()


def sign(args):
    check_openssl()
    name = args.name or os.path.basename(args.core)
    build_time = (int(time.time()) if args.build_time is None
                  else args.build_time)
    out = args.out or args.core + '.sig'
    with tempfile.TemporaryDirectory() as tmp:
        if not args.key.startswith('pkcs11:'):
            check_rfc6979(tmp)
        pub = public_key(args.key)
        header = build_header(hashlib.sha256(pub).digest()[:8], build_time,
                              file_sha256(args.core), name)
        digest = hashlib.sha256(header).digest()
        rs = sign_digest(args.key, digest, tmp)
        verify_digest(pub, digest, rs, tmp)
    with open(out + '.tmp', 'wb') as f:
        f.write(header + rs)
    os.replace(out + '.tmp', out)


def pubkey(args):
    pub = public_key(args.key)
    print('/* key id %s */' % hashlib.sha256(pub).digest()[:8].hex())
    print('{ ' + ', '.join('0x%02x' % b for b in pub) + ' }')


def self_test():
    """The parts that need no OpenSSL, then RFC 6979 through it."""
    checks = 0

    def check(cond, what):
        nonlocal checks
        checks += 1
        if not cond:
            raise AssertionError(what)

    def rejects(der):
        try:
            der_to_raw(der)
        except ValueError:
            return True
        return False

    r32 = bytes([0x7f] + [0x11] * 31)
    s33 = b'\0' + bytes([0x80] + [0x22] * 31)
    r31 = bytes([0x33] * 31)
    seq = lambda b: bytes([0x30, len(b)]) + b
    i = lambda v: bytes([0x02, len(v)]) + v
    raw = der_to_raw(seq(i(r32) + i(s33)))
    check(raw == r32 + s33[1:], 'r 32 bytes, s with its 0x00')
    raw = der_to_raw(seq(i(r31) + i(s33)))
    check(raw[:32] == b'\0' + r31, 'short r left-padded')
    check(rejects(seq(i(r32) + i(s33)) + b'\0'), 'trailing byte')
    check(rejects(seq(i(b'\0' + r32) + i(s33))), 'non-minimal r')
    check(rejects(seq(i(bytes([0x80]) + r31) + i(s33))), 'negative r')
    check(rejects(seq(i(b'\0') + i(s33))), 'zero r')
    check(rejects(bytes([0x30, 0x81, 0x46]) + i(r32) + i(s33)),
          'long-form length')
    check(rejects(seq(i(r32))), 'one INTEGER')
    header = build_header(bytes(range(8)), 0x0102030405060708,
                          bytes([0xaa]) * 32, 'a_libretro.so')
    check(len(header) == 57 + 13, 'header length')
    check(header[:8] == MAGIC and header[8:16] == bytes(range(8)),
          'magic and key id')
    check(header[16:24] == bytes([1, 2, 3, 4, 5, 6, 7, 8]),
          'big-endian build time')
    check(header[56] == 13 and header[57:] == b'a_libretro.so',
          'name length and name')
    for bad in ('', 'a/b.so', 'a\\b.so', 'x' * 256, 'a\0.so'):
        try:
            build_header(bytes(8), 1, bytes(32), bad)
            check(False, 'name %r accepted' % bad)
        except ValueError:
            check(True, '')
    for bad in (0, -1, 1 << 64):
        try:
            build_header(bytes(8), bad, bytes(32), 'a_libretro.so')
            check(False, 'build time %d accepted' % bad)
        except ValueError:
            check(True, '')
    check(openssl_version('OpenSSL 3.1.4 24 Oct 2023') == (3, 1),
          'version parse')
    check(openssl_version('LibreSSL 3.8.2') is None, 'LibreSSL refused')
    check(openssl_version('OpenSSL 3.2.0 23 Nov 2023') >= (3, 2),
          '3.2 accepted')
    print('core_sign self-test: %d checks ok' % checks)
    check_openssl()
    with tempfile.TemporaryDirectory() as tmp:
        check_rfc6979(tmp)
    print('core_sign self-test: RFC 6979 vector ok')


def fail(msg, status):
    sys.stdout.flush()
    print('core_sign: %s' % msg, file=sys.stderr)
    return status


def main(argv):
    if argv == ['--self-test']:
        try:
            self_test()
        except Refused as e:
            return fail('refused: %s' % e, EXIT_REFUSED)
        except (Failed, OSError, ValueError) as e:
            return fail(e, 1)
        return 0
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = parser.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('sign')
    p.add_argument('--key', required=True)
    p.add_argument('--name')
    p.add_argument('--build-time', type=int)
    p.add_argument('--out')
    p.add_argument('core')
    p = sub.add_parser('pubkey')
    p.add_argument('--key', required=True)
    args = parser.parse_args(argv)
    try:
        if args.cmd == 'sign':
            sign(args)
        else:
            pubkey(args)
    except Refused as e:
        return fail('refused: %s' % e, EXIT_REFUSED)
    except (Failed, OSError, ValueError) as e:
        return fail(e, 1)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
