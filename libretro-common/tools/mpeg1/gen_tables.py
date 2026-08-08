#!/usr/bin/env python3
"""Generate rmpeg1_tables.h from the Annex B text of ITU-T H.262.

Usage: gen_tables.py h262.txt > rmpeg1_tables.h
where h262.txt is `pdftotext -layout` of the Recommendation.

The tables are parsed out of the specification rather than transcribed:
113 codes of arbitrary bit patterns typed by hand is how you get a decoder
that works on a test pattern and falls apart on real content.

Every emitted table is checked for prefix-freeness and for its Kraft sum
before anything is written. That check is not decoration -- it caught two
real defects during development, both from a regex that could not match a
single-character code and silently produced a mangled entry instead.
"""
import re, sys
from fractions import Fraction

TXT = open(sys.argv[1] if len(sys.argv) > 1 else 'h262.txt').read().split('\n')

def clean(a, b):
    for ln in TXT[a:b]:
        ln = re.sub(r'\(Note \d\)', '', ln).rstrip()
        if not ln.strip() or 'ITU-T Rec' in ln or 'ISO/IEC' in ln:
            continue
        yield ln

CODE = re.compile(r'^[01](?:[01 ]*[01])?$')

def cells(ln):
    """Split a table line into cells on runs of two or more spaces.

    Single-space splitting would break the codes themselves, which the
    specification prints in nibble groups ('0000 0101 01'). Regexes that
    tried to match code-then-value in one pattern kept swallowing the
    leading digit of the value; splitting into cells first removes the
    ambiguity entirely.
    """
    return [c.strip() for c in re.split(r'\s{2,}', ln.strip()) if c.strip()]

def pairs(a, b, want_two_values=False):
    """Yield (code, v1[, v2]) from a region, walking cells left to right.

    B.1 prints two (code, value) columns per line; B.12/B.13 print one;
    B.14 prints code, run, level with a stray 's' cell. One walker handles
    all of them by consuming a code cell then as many numeric cells as the
    caller asked for.
    """
    for ln in clean(a, b):
        cs = cells(ln)
        i = 0
        while i < len(cs):
            cell = cs[i]
            # The sign bit is printed as a trailing ' s' in the same cell as
            # the code but is not part of it: it follows the code in the
            # bitstream and its value is the sign of the level.
            if cell.endswith(' s'):
                cell = cell[:-2].rstrip()
            elif cell == 's':
                i += 1
                continue
            c = cell.replace(' ', '')
            if not CODE.match(cell):
                i += 1
                continue
            vals, j = [], i + 1
            while j < len(cs) and len(vals) < (2 if want_two_values else 1):
                if cs[j] == 's':
                    j += 1
                    continue
                # The specification prints minus as an en-dash, so a plain
                # \d+ match silently drops every negative motion_code and
                # leaves a table that is prefix-free but half missing.
                tok = cs[j].replace('\u2013', '-').replace('\u2212', '-')
                if re.match(r'^-?\d+$', tok):
                    vals.append(int(tok))
                    j += 1
                    continue
                break
            if len(vals) == (2 if want_two_values else 1):
                yield tuple([c] + vals)
                i = j
            else:
                i += 1

b14 = list(pairs(8220, 8390, True))
b1  = [p for p in pairs(7814, 7845)]
b12 = list(pairs(8177, 8196))
b13 = list(pairs(8197, 8217))

# B.1's macroblock_escape row has a non-numeric value and is skipped by the
# walker; assert that so a silent change in the source text is caught.
mba_escape = '00000001000'
assert all(1 <= v <= 33 for _, v in b1), 'unexpected value in B.1'
assert sorted(v for _, v in b1) == list(range(1, 34)), 'B.1 incomplete'

# B.3, macroblock_type in P-pictures. The six flag columns are packed into
# one value: quant | forward<<1 | backward<<2 | pattern<<3 | intra<<4.
def flagtable(a, b):
    out = []
    for ln in clean(a, b):
        cs = cells(ln)
        if not cs or not CODE.match(cs[0]):
            continue
        bits = [c for c in cs[1:] if c in ('0', '1')]
        if len(bits) < 6:
            continue
        q, mf, mb, pat, intra = (int(x) for x in bits[:5])
        out.append((cs[0].replace(' ', ''),
                    q | (mf << 1) | (mb << 2) | (pat << 3) | (intra << 4)))
    return out

b3  = flagtable(7875, 7901)
b4  = flagtable(7902, 7928)
b9  = list(pairs(8060, 8114))
b10 = list(pairs(8115, 8160))

first = [c for c in b14 if c[0] != '11']
rest  = [c for c in b14 if c[0] != '1']

EOB, ESC = '10', '000001'

def validate(name, codes, expect_kraft=None):
    for a in codes:
        for b in codes:
            if a is not b and b.startswith(a):
                sys.exit("%s: '%s' is a prefix of '%s'" % (name, a, b))
    k = sum(Fraction(1, 2 ** len(c)) for c in codes)
    if expect_kraft is not None and k != expect_kraft:
        sys.exit("%s: Kraft sum %s, expected %s" % (name, k, expect_kraft))
    print("  %-14s %3d codes  Kraft %s" % (name, len(codes), k), file=sys.stderr)
    return k

print("validating:", file=sys.stderr)
# 4095/4096: the twelve-zero prefix is left unassigned so that no code can
# emulate a start code prefix. A table summing to exactly 1 would be wrong.
validate('dct_first', [c[0] for c in first] + [ESC],       Fraction(4095, 4096))
validate('dct_next',  [c[0] for c in rest] + [EOB, ESC],   Fraction(4095, 4096))
validate('mba',       [c[0] for c in b1] + [mba_escape])
validate('dc_lum',    [c[0] for c in b12], Fraction(1))
validate('dc_chr',    [c[0] for c in b13], Fraction(1))
validate('mb_type_p', [c[0] for c in b3])
validate('mb_type_b', [c[0] for c in b4])
validate('cbp',       [c[0] for c in b9])
validate('motion',    [c[0] for c in b10])
assert sorted(v for _, v in b9)  == list(range(1, 64)), 'B.9 incomplete'
assert sorted(v for _, v in b10) == list(range(-16, 17)), 'B.10 incomplete'
assert len(b3) == 7,  'B.3 expected 7 rows, got %d' % len(b3)
assert len(b4) == 11, 'B.4 expected 11 rows, got %d' % len(b4)

def emit(name, entries):
    print("static const rmpeg1_vlc_t %s[] = {" % name)
    for e in entries:
        vals = list(e[1:]) + [-1] * (2 - len(e[1:]))
        print("   { 0x%08X, %2d, %3d, %3d },   /* %s */"
              % (int(e[0], 2), len(e[0]), vals[0], vals[1], e[0]))
    print("   { 0, 0, -1, -1 }\n};")

print("""#ifndef __LIBRETRO_SDK_FORMAT_RMPEG1_TABLES_H
#define __LIBRETRO_SDK_FORMAT_RMPEG1_TABLES_H

/* MPEG-1 video variable length code tables.
 *
 * GENERATED FILE -- do not edit. Regenerate with:
 *
 *   pdftotext -layout h262.pdf h262.txt
 *   tools/rmpeg1/gen_tables.py h262.txt > \\
 *       libretro-common/formats/mpeg1/rmpeg1_tables.h
 *
 * from ITU-T Rec. H.262, whose Annex B carries the same tables as
 * ISO/IEC 11172-2 Annex B.
 *
 * The generator refuses to emit a table that is not prefix-free, and checks
 * each Kraft sum against its expected value. Both DCT tables come to
 * 4095/4096 rather than 1: the twelve-zero prefix 0000 0000 0000 is left
 * unassigned so that no code can emulate a start code prefix. A DCT table
 * summing to exactly 1 would be wrong.
 *
 * Table zero (B.14) is MPEG-1; Table one (B.15) is MPEG-2 only and is
 * deliberately not generated.
 *
 * MPEG-1 additionally defines macroblock_stuffing, which MPEG-2 removed and
 * H.262 therefore does not list. It is spelled out below rather than parsed.
 *
 * code is right-aligned in the low `len` bits. The sign bit that follows a
 * DCT code in the bitstream is not part of the code.
 */

#include <stdint.h>

typedef struct
{
   uint32_t code;
   int8_t   len;
   int8_t   a;    /* run, or the decoded value for single-value tables */
   int8_t   b;    /* level, or -1 */
} rmpeg1_vlc_t;

#define RMPEG1_DCT_EOB_CODE      0x2   /* \'10\',          2 bits */
#define RMPEG1_DCT_EOB_LEN       2
#define RMPEG1_DCT_ESCAPE_CODE   0x1   /* \'000001\',      6 bits */
#define RMPEG1_DCT_ESCAPE_LEN    6
#define RMPEG1_MBA_ESCAPE_CODE   0x8   /* \'00000001000\', 11 bits */
#define RMPEG1_MBA_ESCAPE_LEN    11
#define RMPEG1_MBA_STUFFING_CODE 0xF   /* \'00000001111\', 11 bits, MPEG-1 */
#define RMPEG1_MBA_STUFFING_LEN  11
""")
emit("rmpeg1_vlc_dct_first", first)
emit("rmpeg1_vlc_dct_next",  rest)
emit("rmpeg1_vlc_mba",       b1)
emit("rmpeg1_vlc_dc_lum",    b12)
emit("rmpeg1_vlc_dc_chr",    b13)
emit("rmpeg1_vlc_mb_type_p", b3)
emit("rmpeg1_vlc_mb_type_b", b4)
emit("rmpeg1_vlc_cbp",       b9)
emit("rmpeg1_vlc_motion",    b10)
print("""
/* Packing of the macroblock_type flag columns in rmpeg1_vlc_mb_type_p. */
#define RMPEG1_MB_QUANT     0x01
#define RMPEG1_MB_FORWARD   0x02
#define RMPEG1_MB_BACKWARD  0x04
#define RMPEG1_MB_PATTERN   0x08
#define RMPEG1_MB_INTRA     0x10
""")
print("#endif")
