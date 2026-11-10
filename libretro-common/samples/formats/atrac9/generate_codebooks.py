#!/usr/bin/env python3
"""Render the independent LibAtrac9 codebooks supplied as a local file.

Source: Thealexbarney/LibAtrac9, commit
efca2e3af35562a09a9bb6deed90e45b4b824dc4,
CSharp/LibAtrac9/HuffmanCodebooks.cs (MIT, Alex Barney 2018).
No decoder-under-test tables are read.
"""

import hashlib
from pathlib import Path
import re
import sys


def main():
    source = Path(sys.argv[1]).read_bytes()
    expected = '2bdbe2e2e1a7f1b28851187e382069866b0abfd042a853691c1591aba4dfde90'
    if hashlib.sha256(source).hexdigest() != expected:
        raise ValueError('unexpected reference source revision')
    arrays = {}
    for name, body in re.findall(
            r'(?:byte|short)\[\]\s+(\w+)\s*=\s*\{([^}]+)\}', source.decode()):
        arrays[name] = [int(n, 16 if n.startswith('0x') else 10)
                        for n in re.findall(r'0x[0-9A-Fa-f]+|\d+', body)]
    rows = []

    def emit(name, reference, power):
        if reference is None:
            rows.append('codebook ' + name + ' none')
            return
        bits, codes = arrays[reference + 'Bits'], arrays[reference + 'Codes']
        assert len(bits) == len(codes)
        values = 1 << power
        value_bits = (len(codes).bit_length() - 1) >> power
        rows.append('codebook %s symbols %d values_per_symbol %d value_bits %d '
                    'max_code_bits %d' %
                    (name, len(bits), values, value_bits, max(bits)))
        rows.extend(' symbol %d length %d code 0x%x' % (i, bit, code)
                    for i, (bit, code) in enumerate(zip(bits, codes)))

    for bits in range(1, 7):
        emit('sf_unsigned[%d]' % bits, 'ScaleFactorsA%d' % bits, 0)
    for bits in range(2, 6):
        emit('sf_signed[%d]' % bits, 'ScaleFactorsB%d' % bits, 0)
    for family, letter in enumerate('AB'):
        for precision in range(2, 8):
            for table in range(4):
                reference = 'Spectrum%s%d%d' % (letter, precision, table + 1)
                if family == 1 and table == 0:
                    reference = None
                power = (1 if table == 0 else 2) if precision == 2 else (
                    1 if precision in (3, 4) else 0)
                emit('spectrum[%d][%d][%d]' % (family, precision, table),
                     reference, power)
    Path(__file__).with_name('codebooks.txt').write_text(
        '\n'.join(rows) + '\n', encoding='ascii', newline='\n')


if __name__ == '__main__':
    main()
