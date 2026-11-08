#!/usr/bin/env python3
"""Writes core_signature_fixtures.h with tools/core_sign.py.

  gen_fixtures.py          rewrite the header
  gen_fixtures.py --check  exit 1 if the header differs from a fresh one

core_sign.py signs with RFC 6979 nonces, so the same keys and core give
the same bytes. --check first rebuilds the header around its own
signatures and verifies each with openssl, which any OpenSSL can do;
signing again needs 3.2, and older ones skip that and say so. Exit 3
when nothing could be checked (openssl can't be run) or a rewrite was
refused (older than 3.2), as core_sign.py must. The keys are test keys:
key 1 is RFC 6979 A.2.5's, key 2 is SHA-256("core_signature test key 2").
"""
import hashlib
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, '..', '..', '..', 'tools', 'core_sign.py')
OUT = os.path.join(HERE, 'core_signature_fixtures.h')
sys.path.insert(0, os.path.dirname(TOOL))
import core_sign  # noqa: E402

N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
CORE_SIZE = 200003
CORE_SEED = 0x12345678
NAME = 'test_libretro.so'
TIME = 1700000000
KEY1 = core_sign.RFC6979_D
KEY2 = hashlib.sha256(b'core_signature test key 2').digest()


def core_bytes():
    x = CORE_SEED
    out = bytearray(CORE_SIZE)
    for i in range(CORE_SIZE):
        x = (x * 1103515245 + 12345) & 0xffffffff
        out[i] = x >> 24
    return bytes(out)


def c_array(name, data):
    lines = ['static const uint8_t %s[%d] = {' % (name, len(data))]
    for i in range(0, len(data), 12):
        lines.append('   ' + ', '.join('0x%02x' % b
                                       for b in data[i:i + 12]) + ',')
    lines[-1] = lines[-1].rstrip(',')
    lines.append('};')
    return '\n'.join(lines)


FIXTURES = (
    ('fx_sig_good', 0, NAME, TIME),
    ('fx_sig_older', 0, NAME, TIME - 100000000),
    ('fx_sig_other_name', 0, 'other_libretro.so', TIME),
    ('fx_sig_key2', 1, NAME, TIME + 1))


def write_keys(tmp):
    assert 0 < int.from_bytes(KEY2, 'big') < N
    keys = []
    for i, d in enumerate((KEY1, KEY2)):
        path = os.path.join(tmp, 'key%d.pem' % (i + 1))
        with open(path, 'wb') as f:
            f.write(core_sign.sec1_pem(d))
        keys.append(path)
    return keys


def render(pubs, sigs):
    parts = ['/* Made by gen_fixtures.py with tools/core_sign.py; do not '
             'edit.\n * Test keys only. */',
             '#define FX_CORE_SIZE %d' % CORE_SIZE,
             '#define FX_CORE_SEED 0x%08xUL' % CORE_SEED,
             '#define FX_NAME "%s"' % NAME,
             '#define FX_TIME %d' % TIME,
             c_array('fx_key1', pubs[0]),
             c_array('fx_key2', pubs[1])]
    parts += [c_array(var, data) for var, data in sigs]
    return '\n\n'.join(parts) + '\n'


def make(tmp, keys):
    core = os.path.join(tmp, NAME)
    with open(core, 'wb') as f:
        f.write(core_bytes())
    sigs = []
    for var, key, name, when in FIXTURES:
        out = os.path.join(tmp, var + '.sig')
        rc = subprocess.call([sys.executable, '-I', TOOL, 'sign',
                              '--key', keys[key], '--name', name,
                              '--build-time', str(when), '--out', out,
                              core])
        if rc:
            sys.exit(rc)
        with open(out, 'rb') as f:
            sigs.append((var, f.read()))
    return render([core_sign.public_key(k) for k in keys], sigs)


def arrays_of(text):
    arrays = {}
    for m in re.finditer(r'static const uint8_t (\w+)\[(\d+)\] = \{([^}]*)\};',
                         text):
        data = bytes(int(b, 16)
                     for b in re.findall(r'0x([0-9a-f]{2})', m.group(3)))
        if len(data) == int(m.group(2)):
            arrays[m.group(1)] = data
    return arrays


def stale_offline(text, keys, tmp):
    """Why text isn't the header for its own signatures, or None. Needs
    no signing, so no OpenSSL 3.2."""
    pubs = [core_sign.public_key(k) for k in keys]
    arrays = arrays_of(text)
    core_sha256 = hashlib.sha256(core_bytes()).digest()
    sigs = []
    for i, pub in enumerate(pubs):
        if arrays.get('fx_key%d' % (i + 1)) != pub:
            return 'fx_key%d is not the test key' % (i + 1)
    for var, key, name, when in FIXTURES:
        header = core_sign.build_header(hashlib.sha256(pubs[key]).digest()[:8],
                                        when, core_sha256, name)
        sig = arrays.get(var, b'')
        if len(sig) != len(header) + 64 or not sig.startswith(header):
            return '%s: signed header differs' % var
        try:
            core_sign.verify_digest(pubs[key],
                                    hashlib.sha256(header).digest(),
                                    sig[len(header):], tmp)
        except core_sign.Failed:
            return '%s: signature does not verify' % var
        sigs.append((var, sig))
    if render(pubs, sigs) != text:
        return 'differs from one rebuilt around its signatures'
    return None


def check(tmp, keys, refused):
    with open(OUT) as f:
        text = f.read()
    try:
        why = stale_offline(text, keys, tmp)
    except OSError as e:
        print('gen_fixtures: refused: cannot run %s: %s'
              % (core_sign.openssl(), e), file=sys.stderr)
        return core_sign.EXIT_REFUSED
    except core_sign.Failed as e:
        print('gen_fixtures: %s' % e, file=sys.stderr)
        return 1
    if not why and not refused and make(tmp, keys) != text:
        why = 'signing again gives other bytes'
    if why:
        print('gen_fixtures: %s is stale: %s' % (OUT, why), file=sys.stderr)
        return 1
    if refused:
        print('gen_fixtures: fixtures match: headers rebuilt and '
              'signatures verified; not signed again: %s' % refused)
    else:
        print('gen_fixtures: fixtures match')
    return 0


def main(argv):
    try:
        core_sign.check_openssl()
        refused = None
    except core_sign.Refused as e:
        refused = e
    with tempfile.TemporaryDirectory() as tmp:
        keys = write_keys(tmp)
        if argv == ['--check']:
            return check(tmp, keys, refused)
        if refused:
            print('gen_fixtures: refused: %s' % refused, file=sys.stderr)
            return core_sign.EXIT_REFUSED
        text = make(tmp, keys)
    with open(OUT, 'w') as f:
        f.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
