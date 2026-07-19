/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (raac.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* MPEG-4 AAC-LC decoder (ISO/IEC 14496-3 clause 4). See raac.h for
 * the supported scope. The decode path per access unit: parse the
 * raw_data_block's channel elements (ics_info, section data, scale
 * factors, pulse, TNS, spectral Huffman data), undo channel tools
 * (M/S, intensity, PNS), inverse-quantise (x^(4/3) times the scale
 * factor gain), apply TNS synthesis filtering, and run the inverse
 * MDCT filterbank (via an FFT) with KBD/sine windowing and 50 percent
 * overlap-add. Spec-defined constant tables (Huffman codebooks,
 * scalefactor band offsets, TNS band limits) are transcribed from
 * ISO/IEC 14496-3; windows, twiddles and TNS coefficient maps are
 * computed at open from their defining formulas. */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <formats/raac.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RAAC_FRAME     1024
#define RAAC_MAX_CH    2
#define RAAC_MAX_SFB   51
#define RAAC_MAX_WIN   8

/* quantised spectrum values live in -8191..8191 (13 bit + escape) */
#define RAAC_ESC_BOOK  11

/* section codebook meanings */
#define RAAC_CB_ZERO       0
#define RAAC_CB_INTENSITY2 14
#define RAAC_CB_INTENSITY  15
#define RAAC_CB_NOISE      13

static const uint16_t raac_hcb1_code[81] = {
   0x07f8, 0x01f1, 0x07fd, 0x03f5, 0x0068, 0x03f0, 0x07f7, 0x01ec,
   0x07f5, 0x03f1, 0x0072, 0x03f4, 0x0074, 0x0011, 0x0076, 0x01eb,
   0x006c, 0x03f6, 0x07fc, 0x01e1, 0x07f1, 0x01f0, 0x0061, 0x01f6,
   0x07f2, 0x01ea, 0x07fb, 0x01f2, 0x0069, 0x01ed, 0x0077, 0x0017,
   0x006f, 0x01e6, 0x0064, 0x01e5, 0x0067, 0x0015, 0x0062, 0x0012,
   0x0000, 0x0014, 0x0065, 0x0016, 0x006d, 0x01e9, 0x0063, 0x01e4,
   0x006b, 0x0013, 0x0071, 0x01e3, 0x0070, 0x01f3, 0x07fe, 0x01e7,
   0x07f3, 0x01ef, 0x0060, 0x01ee, 0x07f0, 0x01e2, 0x07fa, 0x03f3,
   0x006a, 0x01e8, 0x0075, 0x0010, 0x0073, 0x01f4, 0x006e, 0x03f7,
   0x07f6, 0x01e0, 0x07f9, 0x03f2, 0x0066, 0x01f5, 0x07ff, 0x01f7,
   0x07f4
};
static const uint8_t raac_hcb1_bits[81] = {
   11, 9, 11, 10, 7, 10, 11, 9, 11, 10, 7, 10, 7, 5, 7, 9,
   7, 10, 11, 9, 11, 9, 7, 9, 11, 9, 11, 9, 7, 9, 7, 5,
   7, 9, 7, 9, 7, 5, 7, 5, 1, 5, 7, 5, 7, 9, 7, 9,
   7, 5, 7, 9, 7, 9, 11, 9, 11, 9, 7, 9, 11, 9, 11, 10,
   7, 9, 7, 5, 7, 9, 7, 10, 11, 9, 11, 10, 7, 9, 11, 9,
   11
};
static const uint16_t raac_hcb2_code[81] = {
   0x01f3, 0x006f, 0x01fd, 0x00eb, 0x0023, 0x00ea, 0x01f7, 0x00e8,
   0x01fa, 0x00f2, 0x002d, 0x0070, 0x0020, 0x0006, 0x002b, 0x006e,
   0x0028, 0x00e9, 0x01f9, 0x0066, 0x00f8, 0x00e7, 0x001b, 0x00f1,
   0x01f4, 0x006b, 0x01f5, 0x00ec, 0x002a, 0x006c, 0x002c, 0x000a,
   0x0027, 0x0067, 0x001a, 0x00f5, 0x0024, 0x0008, 0x001f, 0x0009,
   0x0000, 0x0007, 0x001d, 0x000b, 0x0030, 0x00ef, 0x001c, 0x0064,
   0x001e, 0x000c, 0x0029, 0x00f3, 0x002f, 0x00f0, 0x01fc, 0x0071,
   0x01f2, 0x00f4, 0x0021, 0x00e6, 0x00f7, 0x0068, 0x01f8, 0x00ee,
   0x0022, 0x0065, 0x0031, 0x0002, 0x0026, 0x00ed, 0x0025, 0x006a,
   0x01fb, 0x0072, 0x01fe, 0x0069, 0x002e, 0x00f6, 0x01ff, 0x006d,
   0x01f6
};
static const uint8_t raac_hcb2_bits[81] = {
   9, 7, 9, 8, 6, 8, 9, 8, 9, 8, 6, 7, 6, 5, 6, 7,
   6, 8, 9, 7, 8, 8, 6, 8, 9, 7, 9, 8, 6, 7, 6, 5,
   6, 7, 6, 8, 6, 5, 6, 5, 3, 5, 6, 5, 6, 8, 6, 7,
   6, 5, 6, 8, 6, 8, 9, 7, 9, 8, 6, 8, 8, 7, 9, 8,
   6, 7, 6, 4, 6, 8, 6, 7, 9, 7, 9, 7, 6, 8, 9, 7,
   9
};
static const uint16_t raac_hcb3_code[81] = {
   0x0000, 0x0009, 0x00ef, 0x000b, 0x0019, 0x00f0, 0x01eb, 0x01e6,
   0x03f2, 0x000a, 0x0035, 0x01ef, 0x0034, 0x0037, 0x01e9, 0x01ed,
   0x01e7, 0x03f3, 0x01ee, 0x03ed, 0x1ffa, 0x01ec, 0x01f2, 0x07f9,
   0x07f8, 0x03f8, 0x0ff8, 0x0008, 0x0038, 0x03f6, 0x0036, 0x0075,
   0x03f1, 0x03eb, 0x03ec, 0x0ff4, 0x0018, 0x0076, 0x07f4, 0x0039,
   0x0074, 0x03ef, 0x01f3, 0x01f4, 0x07f6, 0x01e8, 0x03ea, 0x1ffc,
   0x00f2, 0x01f1, 0x0ffb, 0x03f5, 0x07f3, 0x0ffc, 0x00ee, 0x03f7,
   0x7ffe, 0x01f0, 0x07f5, 0x7ffd, 0x1ffb, 0x3ffa, 0xffff, 0x00f1,
   0x03f0, 0x3ffc, 0x01ea, 0x03ee, 0x3ffb, 0x0ff6, 0x0ffa, 0x7ffc,
   0x07f2, 0x0ff5, 0xfffe, 0x03f4, 0x07f7, 0x7ffb, 0x0ff7, 0x0ff9,
   0x7ffa
};
static const uint8_t raac_hcb3_bits[81] = {
   1, 4, 8, 4, 5, 8, 9, 9, 10, 4, 6, 9, 6, 6, 9, 9,
   9, 10, 9, 10, 13, 9, 9, 11, 11, 10, 12, 4, 6, 10, 6, 7,
   10, 10, 10, 12, 5, 7, 11, 6, 7, 10, 9, 9, 11, 9, 10, 13,
   8, 9, 12, 10, 11, 12, 8, 10, 15, 9, 11, 15, 13, 14, 16, 8,
   10, 14, 9, 10, 14, 12, 12, 15, 11, 12, 16, 10, 11, 15, 12, 12,
   15
};
static const uint16_t raac_hcb4_code[81] = {
   0x0007, 0x0016, 0x00f6, 0x0018, 0x0008, 0x00ef, 0x01ef, 0x00f3,
   0x07f8, 0x0019, 0x0017, 0x00ed, 0x0015, 0x0001, 0x00e2, 0x00f0,
   0x0070, 0x03f0, 0x01ee, 0x00f1, 0x07fa, 0x00ee, 0x00e4, 0x03f2,
   0x07f6, 0x03ef, 0x07fd, 0x0005, 0x0014, 0x00f2, 0x0009, 0x0004,
   0x00e5, 0x00f4, 0x00e8, 0x03f4, 0x0006, 0x0002, 0x00e7, 0x0003,
   0x0000, 0x006b, 0x00e3, 0x0069, 0x01f3, 0x00eb, 0x00e6, 0x03f6,
   0x006e, 0x006a, 0x01f4, 0x03ec, 0x01f0, 0x03f9, 0x00f5, 0x00ec,
   0x07fb, 0x00ea, 0x006f, 0x03f7, 0x07f9, 0x03f3, 0x0fff, 0x00e9,
   0x006d, 0x03f8, 0x006c, 0x0068, 0x01f5, 0x03ee, 0x01f2, 0x07f4,
   0x07f7, 0x03f1, 0x0ffe, 0x03ed, 0x01f1, 0x07f5, 0x07fe, 0x03f5,
   0x07fc
};
static const uint8_t raac_hcb4_bits[81] = {
   4, 5, 8, 5, 4, 8, 9, 8, 11, 5, 5, 8, 5, 4, 8, 8,
   7, 10, 9, 8, 11, 8, 8, 10, 11, 10, 11, 4, 5, 8, 4, 4,
   8, 8, 8, 10, 4, 4, 8, 4, 4, 7, 8, 7, 9, 8, 8, 10,
   7, 7, 9, 10, 9, 10, 8, 8, 11, 8, 7, 10, 11, 10, 12, 8,
   7, 10, 7, 7, 9, 10, 9, 11, 11, 10, 12, 10, 9, 11, 11, 10,
   11
};
static const uint16_t raac_hcb5_code[81] = {
   0x1fff, 0x0ff7, 0x07f4, 0x07e8, 0x03f1, 0x07ee, 0x07f9, 0x0ff8,
   0x1ffd, 0x0ffd, 0x07f1, 0x03e8, 0x01e8, 0x00f0, 0x01ec, 0x03ee,
   0x07f2, 0x0ffa, 0x0ff4, 0x03ef, 0x01f2, 0x00e8, 0x0070, 0x00ec,
   0x01f0, 0x03ea, 0x07f3, 0x07eb, 0x01eb, 0x00ea, 0x001a, 0x0008,
   0x0019, 0x00ee, 0x01ef, 0x07ed, 0x03f0, 0x00f2, 0x0073, 0x000b,
   0x0000, 0x000a, 0x0071, 0x00f3, 0x07e9, 0x07ef, 0x01ee, 0x00ef,
   0x0018, 0x0009, 0x001b, 0x00eb, 0x01e9, 0x07ec, 0x07f6, 0x03eb,
   0x01f3, 0x00ed, 0x0072, 0x00e9, 0x01f1, 0x03ed, 0x07f7, 0x0ff6,
   0x07f0, 0x03e9, 0x01ed, 0x00f1, 0x01ea, 0x03ec, 0x07f8, 0x0ff9,
   0x1ffc, 0x0ffc, 0x0ff5, 0x07ea, 0x03f3, 0x03f2, 0x07f5, 0x0ffb,
   0x1ffe
};
static const uint8_t raac_hcb5_bits[81] = {
   13, 12, 11, 11, 10, 11, 11, 12, 13, 12, 11, 10, 9, 8, 9, 10,
   11, 12, 12, 10, 9, 8, 7, 8, 9, 10, 11, 11, 9, 8, 5, 4,
   5, 8, 9, 11, 10, 8, 7, 4, 1, 4, 7, 8, 11, 11, 9, 8,
   5, 4, 5, 8, 9, 11, 11, 10, 9, 8, 7, 8, 9, 10, 11, 12,
   11, 10, 9, 8, 9, 10, 11, 12, 13, 12, 12, 11, 10, 10, 11, 12,
   13
};
static const uint16_t raac_hcb6_code[81] = {
   0x07fe, 0x03fd, 0x01f1, 0x01eb, 0x01f4, 0x01ea, 0x01f0, 0x03fc,
   0x07fd, 0x03f6, 0x01e5, 0x00ea, 0x006c, 0x0071, 0x0068, 0x00f0,
   0x01e6, 0x03f7, 0x01f3, 0x00ef, 0x0032, 0x0027, 0x0028, 0x0026,
   0x0031, 0x00eb, 0x01f7, 0x01e8, 0x006f, 0x002e, 0x0008, 0x0004,
   0x0006, 0x0029, 0x006b, 0x01ee, 0x01ef, 0x0072, 0x002d, 0x0002,
   0x0000, 0x0003, 0x002f, 0x0073, 0x01fa, 0x01e7, 0x006e, 0x002b,
   0x0007, 0x0001, 0x0005, 0x002c, 0x006d, 0x01ec, 0x01f9, 0x00ee,
   0x0030, 0x0024, 0x002a, 0x0025, 0x0033, 0x00ec, 0x01f2, 0x03f8,
   0x01e4, 0x00ed, 0x006a, 0x0070, 0x0069, 0x0074, 0x00f1, 0x03fa,
   0x07ff, 0x03f9, 0x01f6, 0x01ed, 0x01f8, 0x01e9, 0x01f5, 0x03fb,
   0x07fc
};
static const uint8_t raac_hcb6_bits[81] = {
   11, 10, 9, 9, 9, 9, 9, 10, 11, 10, 9, 8, 7, 7, 7, 8,
   9, 10, 9, 8, 6, 6, 6, 6, 6, 8, 9, 9, 7, 6, 4, 4,
   4, 6, 7, 9, 9, 7, 6, 4, 4, 4, 6, 7, 9, 9, 7, 6,
   4, 4, 4, 6, 7, 9, 9, 8, 6, 6, 6, 6, 6, 8, 9, 10,
   9, 8, 7, 7, 7, 7, 8, 10, 11, 10, 9, 9, 9, 9, 9, 10,
   11
};
static const uint16_t raac_hcb7_code[64] = {
   0x0000, 0x0005, 0x0037, 0x0074, 0x00f2, 0x01eb, 0x03ed, 0x07f7,
   0x0004, 0x000c, 0x0035, 0x0071, 0x00ec, 0x00ee, 0x01ee, 0x01f5,
   0x0036, 0x0034, 0x0072, 0x00ea, 0x00f1, 0x01e9, 0x01f3, 0x03f5,
   0x0073, 0x0070, 0x00eb, 0x00f0, 0x01f1, 0x01f0, 0x03ec, 0x03fa,
   0x00f3, 0x00ed, 0x01e8, 0x01ef, 0x03ef, 0x03f1, 0x03f9, 0x07fb,
   0x01ed, 0x00ef, 0x01ea, 0x01f2, 0x03f3, 0x03f8, 0x07f9, 0x07fc,
   0x03ee, 0x01ec, 0x01f4, 0x03f4, 0x03f7, 0x07f8, 0x0ffd, 0x0ffe,
   0x07f6, 0x03f0, 0x03f2, 0x03f6, 0x07fa, 0x07fd, 0x0ffc, 0x0fff
};
static const uint8_t raac_hcb7_bits[64] = {
   1, 3, 6, 7, 8, 9, 10, 11, 3, 4, 6, 7, 8, 8, 9, 9,
   6, 6, 7, 8, 8, 9, 9, 10, 7, 7, 8, 8, 9, 9, 10, 10,
   8, 8, 9, 9, 10, 10, 10, 11, 9, 8, 9, 9, 10, 10, 11, 11,
   10, 9, 9, 10, 10, 11, 12, 12, 11, 10, 10, 10, 11, 11, 12, 12
};
static const uint16_t raac_hcb8_code[64] = {
   0x000e, 0x0005, 0x0010, 0x0030, 0x006f, 0x00f1, 0x01fa, 0x03fe,
   0x0003, 0x0000, 0x0004, 0x0012, 0x002c, 0x006a, 0x0075, 0x00f8,
   0x000f, 0x0002, 0x0006, 0x0014, 0x002e, 0x0069, 0x0072, 0x00f5,
   0x002f, 0x0011, 0x0013, 0x002a, 0x0032, 0x006c, 0x00ec, 0x00fa,
   0x0071, 0x002b, 0x002d, 0x0031, 0x006d, 0x0070, 0x00f2, 0x01f9,
   0x00ef, 0x0068, 0x0033, 0x006b, 0x006e, 0x00ee, 0x00f9, 0x03fc,
   0x01f8, 0x0074, 0x0073, 0x00ed, 0x00f0, 0x00f6, 0x01f6, 0x01fd,
   0x03fd, 0x00f3, 0x00f4, 0x00f7, 0x01f7, 0x01fb, 0x01fc, 0x03ff
};
static const uint8_t raac_hcb8_bits[64] = {
   5, 4, 5, 6, 7, 8, 9, 10, 4, 3, 4, 5, 6, 7, 7, 8,
   5, 4, 4, 5, 6, 7, 7, 8, 6, 5, 5, 6, 6, 7, 8, 8,
   7, 6, 6, 6, 7, 7, 8, 9, 8, 7, 6, 7, 7, 8, 8, 10,
   9, 7, 7, 8, 8, 8, 9, 9, 10, 8, 8, 8, 9, 9, 9, 10
};
static const uint16_t raac_hcb9_code[169] = {
   0x0000, 0x0005, 0x0037, 0x00e7, 0x01de, 0x03ce, 0x03d9, 0x07c8,
   0x07cd, 0x0fc8, 0x0fdd, 0x1fe4, 0x1fec, 0x0004, 0x000c, 0x0035,
   0x0072, 0x00ea, 0x00ed, 0x01e2, 0x03d1, 0x03d3, 0x03e0, 0x07d8,
   0x0fcf, 0x0fd5, 0x0036, 0x0034, 0x0071, 0x00e8, 0x00ec, 0x01e1,
   0x03cf, 0x03dd, 0x03db, 0x07d0, 0x0fc7, 0x0fd4, 0x0fe4, 0x00e6,
   0x0070, 0x00e9, 0x01dd, 0x01e3, 0x03d2, 0x03dc, 0x07cc, 0x07ca,
   0x07de, 0x0fd8, 0x0fea, 0x1fdb, 0x01df, 0x00eb, 0x01dc, 0x01e6,
   0x03d5, 0x03de, 0x07cb, 0x07dd, 0x07dc, 0x0fcd, 0x0fe2, 0x0fe7,
   0x1fe1, 0x03d0, 0x01e0, 0x01e4, 0x03d6, 0x07c5, 0x07d1, 0x07db,
   0x0fd2, 0x07e0, 0x0fd9, 0x0feb, 0x1fe3, 0x1fe9, 0x07c4, 0x01e5,
   0x03d7, 0x07c6, 0x07cf, 0x07da, 0x0fcb, 0x0fda, 0x0fe3, 0x0fe9,
   0x1fe6, 0x1ff3, 0x1ff7, 0x07d3, 0x03d8, 0x03e1, 0x07d4, 0x07d9,
   0x0fd3, 0x0fde, 0x1fdd, 0x1fd9, 0x1fe2, 0x1fea, 0x1ff1, 0x1ff6,
   0x07d2, 0x03d4, 0x03da, 0x07c7, 0x07d7, 0x07e2, 0x0fce, 0x0fdb,
   0x1fd8, 0x1fee, 0x3ff0, 0x1ff4, 0x3ff2, 0x07e1, 0x03df, 0x07c9,
   0x07d6, 0x0fca, 0x0fd0, 0x0fe5, 0x0fe6, 0x1feb, 0x1fef, 0x3ff3,
   0x3ff4, 0x3ff5, 0x0fe0, 0x07ce, 0x07d5, 0x0fc6, 0x0fd1, 0x0fe1,
   0x1fe0, 0x1fe8, 0x1ff0, 0x3ff1, 0x3ff8, 0x3ff6, 0x7ffc, 0x0fe8,
   0x07df, 0x0fc9, 0x0fd7, 0x0fdc, 0x1fdc, 0x1fdf, 0x1fed, 0x1ff5,
   0x3ff9, 0x3ffb, 0x7ffd, 0x7ffe, 0x1fe7, 0x0fcc, 0x0fd6, 0x0fdf,
   0x1fde, 0x1fda, 0x1fe5, 0x1ff2, 0x3ffa, 0x3ff7, 0x3ffc, 0x3ffd,
   0x7fff
};
static const uint8_t raac_hcb9_bits[169] = {
   1, 3, 6, 8, 9, 10, 10, 11, 11, 12, 12, 13, 13, 3, 4, 6,
   7, 8, 8, 9, 10, 10, 10, 11, 12, 12, 6, 6, 7, 8, 8, 9,
   10, 10, 10, 11, 12, 12, 12, 8, 7, 8, 9, 9, 10, 10, 11, 11,
   11, 12, 12, 13, 9, 8, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12,
   13, 10, 9, 9, 10, 11, 11, 11, 12, 11, 12, 12, 13, 13, 11, 9,
   10, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 11, 10, 10, 11, 11,
   12, 12, 13, 13, 13, 13, 13, 13, 11, 10, 10, 11, 11, 11, 12, 12,
   13, 13, 14, 13, 14, 11, 10, 11, 11, 12, 12, 12, 12, 13, 13, 14,
   14, 14, 12, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 12,
   11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 15, 15, 13, 12, 12, 12,
   13, 13, 13, 13, 14, 14, 14, 14, 15
};
static const uint16_t raac_hcb10_code[169] = {
   0x0022, 0x0008, 0x001d, 0x0026, 0x005f, 0x00d3, 0x01cf, 0x03d0,
   0x03d7, 0x03ed, 0x07f0, 0x07f6, 0x0ffd, 0x0007, 0x0000, 0x0001,
   0x0009, 0x0020, 0x0054, 0x0060, 0x00d5, 0x00dc, 0x01d4, 0x03cd,
   0x03de, 0x07e7, 0x001c, 0x0002, 0x0006, 0x000c, 0x001e, 0x0028,
   0x005b, 0x00cd, 0x00d9, 0x01ce, 0x01dc, 0x03d9, 0x03f1, 0x0025,
   0x000b, 0x000a, 0x000d, 0x0024, 0x0057, 0x0061, 0x00cc, 0x00dd,
   0x01cc, 0x01de, 0x03d3, 0x03e7, 0x005d, 0x0021, 0x001f, 0x0023,
   0x0027, 0x0059, 0x0064, 0x00d8, 0x00df, 0x01d2, 0x01e2, 0x03dd,
   0x03ee, 0x00d1, 0x0055, 0x0029, 0x0056, 0x0058, 0x0062, 0x00ce,
   0x00e0, 0x00e2, 0x01da, 0x03d4, 0x03e3, 0x07eb, 0x01c9, 0x005e,
   0x005a, 0x005c, 0x0063, 0x00ca, 0x00da, 0x01c7, 0x01ca, 0x01e0,
   0x03db, 0x03e8, 0x07ec, 0x01e3, 0x00d2, 0x00cb, 0x00d0, 0x00d7,
   0x00db, 0x01c6, 0x01d5, 0x01d8, 0x03ca, 0x03da, 0x07ea, 0x07f1,
   0x01e1, 0x00d4, 0x00cf, 0x00d6, 0x00de, 0x00e1, 0x01d0, 0x01d6,
   0x03d1, 0x03d5, 0x03f2, 0x07ee, 0x07fb, 0x03e9, 0x01cd, 0x01c8,
   0x01cb, 0x01d1, 0x01d7, 0x01df, 0x03cf, 0x03e0, 0x03ef, 0x07e6,
   0x07f8, 0x0ffa, 0x03eb, 0x01dd, 0x01d3, 0x01d9, 0x01db, 0x03d2,
   0x03cc, 0x03dc, 0x03ea, 0x07ed, 0x07f3, 0x07f9, 0x0ff9, 0x07f2,
   0x03ce, 0x01e4, 0x03cb, 0x03d8, 0x03d6, 0x03e2, 0x03e5, 0x07e8,
   0x07f4, 0x07f5, 0x07f7, 0x0ffb, 0x07fa, 0x03ec, 0x03df, 0x03e1,
   0x03e4, 0x03e6, 0x03f0, 0x07e9, 0x07ef, 0x0ff8, 0x0ffe, 0x0ffc,
   0x0fff
};
static const uint8_t raac_hcb10_bits[169] = {
   6, 5, 6, 6, 7, 8, 9, 10, 10, 10, 11, 11, 12, 5, 4, 4,
   5, 6, 7, 7, 8, 8, 9, 10, 10, 11, 6, 4, 5, 5, 6, 6,
   7, 8, 8, 9, 9, 10, 10, 6, 5, 5, 5, 6, 7, 7, 8, 8,
   9, 9, 10, 10, 7, 6, 6, 6, 6, 7, 7, 8, 8, 9, 9, 10,
   10, 8, 7, 6, 7, 7, 7, 8, 8, 8, 9, 10, 10, 11, 9, 7,
   7, 7, 7, 8, 8, 9, 9, 9, 10, 10, 11, 9, 8, 8, 8, 8,
   8, 9, 9, 9, 10, 10, 11, 11, 9, 8, 8, 8, 8, 8, 9, 9,
   10, 10, 10, 11, 11, 10, 9, 9, 9, 9, 9, 9, 10, 10, 10, 11,
   11, 12, 10, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 12, 11,
   10, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 11, 10, 10, 10,
   10, 10, 10, 11, 11, 12, 12, 12, 12
};
static const uint16_t raac_hcb11_code[289] = {
   0x0000, 0x0006, 0x0019, 0x003d, 0x009c, 0x00c6, 0x01a7, 0x0390,
   0x03c2, 0x03df, 0x07e6, 0x07f3, 0x0ffb, 0x07ec, 0x0ffa, 0x0ffe,
   0x038e, 0x0005, 0x0001, 0x0008, 0x0014, 0x0037, 0x0042, 0x0092,
   0x00af, 0x0191, 0x01a5, 0x01b5, 0x039e, 0x03c0, 0x03a2, 0x03cd,
   0x07d6, 0x00ae, 0x0017, 0x0007, 0x0009, 0x0018, 0x0039, 0x0040,
   0x008e, 0x00a3, 0x00b8, 0x0199, 0x01ac, 0x01c1, 0x03b1, 0x0396,
   0x03be, 0x03ca, 0x009d, 0x003c, 0x0015, 0x0016, 0x001a, 0x003b,
   0x0044, 0x0091, 0x00a5, 0x00be, 0x0196, 0x01ae, 0x01b9, 0x03a1,
   0x0391, 0x03a5, 0x03d5, 0x0094, 0x009a, 0x0036, 0x0038, 0x003a,
   0x0041, 0x008c, 0x009b, 0x00b0, 0x00c3, 0x019e, 0x01ab, 0x01bc,
   0x039f, 0x038f, 0x03a9, 0x03cf, 0x0093, 0x00bf, 0x003e, 0x003f,
   0x0043, 0x0045, 0x009e, 0x00a7, 0x00b9, 0x0194, 0x01a2, 0x01ba,
   0x01c3, 0x03a6, 0x03a7, 0x03bb, 0x03d4, 0x009f, 0x01a0, 0x008f,
   0x008d, 0x0090, 0x0098, 0x00a6, 0x00b6, 0x00c4, 0x019f, 0x01af,
   0x01bf, 0x0399, 0x03bf, 0x03b4, 0x03c9, 0x03e7, 0x00a8, 0x01b6,
   0x00ab, 0x00a4, 0x00aa, 0x00b2, 0x00c2, 0x00c5, 0x0198, 0x01a4,
   0x01b8, 0x038c, 0x03a4, 0x03c4, 0x03c6, 0x03dd, 0x03e8, 0x00ad,
   0x03af, 0x0192, 0x00bd, 0x00bc, 0x018e, 0x0197, 0x019a, 0x01a3,
   0x01b1, 0x038d, 0x0398, 0x03b7, 0x03d3, 0x03d1, 0x03db, 0x07dd,
   0x00b4, 0x03de, 0x01a9, 0x019b, 0x019c, 0x01a1, 0x01aa, 0x01ad,
   0x01b3, 0x038b, 0x03b2, 0x03b8, 0x03ce, 0x03e1, 0x03e0, 0x07d2,
   0x07e5, 0x00b7, 0x07e3, 0x01bb, 0x01a8, 0x01a6, 0x01b0, 0x01b2,
   0x01b7, 0x039b, 0x039a, 0x03ba, 0x03b5, 0x03d6, 0x07d7, 0x03e4,
   0x07d8, 0x07ea, 0x00ba, 0x07e8, 0x03a0, 0x01bd, 0x01b4, 0x038a,
   0x01c4, 0x0392, 0x03aa, 0x03b0, 0x03bc, 0x03d7, 0x07d4, 0x07dc,
   0x07db, 0x07d5, 0x07f0, 0x00c1, 0x07fb, 0x03c8, 0x03a3, 0x0395,
   0x039d, 0x03ac, 0x03ae, 0x03c5, 0x03d8, 0x03e2, 0x03e6, 0x07e4,
   0x07e7, 0x07e0, 0x07e9, 0x07f7, 0x0190, 0x07f2, 0x0393, 0x01be,
   0x01c0, 0x0394, 0x0397, 0x03ad, 0x03c3, 0x03c1, 0x03d2, 0x07da,
   0x07d9, 0x07df, 0x07eb, 0x07f4, 0x07fa, 0x0195, 0x07f8, 0x03bd,
   0x039c, 0x03ab, 0x03a8, 0x03b3, 0x03b9, 0x03d0, 0x03e3, 0x03e5,
   0x07e2, 0x07de, 0x07ed, 0x07f1, 0x07f9, 0x07fc, 0x0193, 0x0ffd,
   0x03dc, 0x03b6, 0x03c7, 0x03cc, 0x03cb, 0x03d9, 0x03da, 0x07d3,
   0x07e1, 0x07ee, 0x07ef, 0x07f5, 0x07f6, 0x0ffc, 0x0fff, 0x019d,
   0x01c2, 0x00b5, 0x00a1, 0x0096, 0x0097, 0x0095, 0x0099, 0x00a0,
   0x00a2, 0x00ac, 0x00a9, 0x00b1, 0x00b3, 0x00bb, 0x00c0, 0x018f,
   0x0004
};
static const uint8_t raac_hcb11_bits[289] = {
   4, 5, 6, 7, 8, 8, 9, 10, 10, 10, 11, 11, 12, 11, 12, 12,
   10, 5, 4, 5, 6, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10, 10,
   11, 8, 6, 5, 5, 6, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10,
   10, 10, 8, 7, 6, 6, 6, 7, 7, 8, 8, 8, 9, 9, 9, 10,
   10, 10, 10, 8, 8, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9,
   10, 10, 10, 10, 8, 8, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9,
   9, 10, 10, 10, 10, 8, 9, 8, 8, 8, 8, 8, 8, 8, 9, 9,
   9, 10, 10, 10, 10, 10, 8, 9, 8, 8, 8, 8, 8, 8, 9, 9,
   9, 10, 10, 10, 10, 10, 10, 8, 10, 9, 8, 8, 9, 9, 9, 9,
   9, 10, 10, 10, 10, 10, 10, 11, 8, 10, 9, 9, 9, 9, 9, 9,
   9, 10, 10, 10, 10, 10, 10, 11, 11, 8, 11, 9, 9, 9, 9, 9,
   9, 10, 10, 10, 10, 10, 11, 10, 11, 11, 8, 11, 10, 9, 9, 10,
   9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 8, 11, 10, 10, 10,
   10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 9, 11, 10, 9,
   9, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 9, 11, 10,
   10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 9, 12,
   10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 12, 12, 9,
   9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9,
   5
};
static const uint16_t raac_hcb_size[11] = {81,81,81,81,81,81,64,64,169,169,289};
static const uint16_t *const raac_hcb_code[11] = {
   raac_hcb1_code, raac_hcb2_code, raac_hcb3_code, raac_hcb4_code, raac_hcb5_code, raac_hcb6_code, raac_hcb7_code, raac_hcb8_code, raac_hcb9_code, raac_hcb10_code, raac_hcb11_code
};
static const uint8_t *const raac_hcb_bits[11] = {
   raac_hcb1_bits, raac_hcb2_bits, raac_hcb3_bits, raac_hcb4_bits, raac_hcb5_bits, raac_hcb6_bits, raac_hcb7_bits, raac_hcb8_bits, raac_hcb9_bits, raac_hcb10_bits, raac_hcb11_bits
};
static const uint32_t raac_sf_code[121] = {
   0x3ffe8, 0x3ffe6, 0x3ffe7, 0x3ffe5, 0x7fff5, 0x7fff1, 0x7ffed, 0x7fff6,
   0x7ffee, 0x7ffef, 0x7fff0, 0x7fffc, 0x7fffd, 0x7ffff, 0x7fffe, 0x7fff7,
   0x7fff8, 0x7fffb, 0x7fff9, 0x3ffe4, 0x7fffa, 0x3ffe3, 0x1ffef, 0x1fff0,
   0x0fff5, 0x1ffee, 0x0fff2, 0x0fff3, 0x0fff4, 0x0fff1, 0x07ff6, 0x07ff7,
   0x03ff9, 0x03ff5, 0x03ff7, 0x03ff3, 0x03ff6, 0x03ff2, 0x01ff7, 0x01ff5,
   0x00ff9, 0x00ff7, 0x00ff6, 0x007f9, 0x00ff4, 0x007f8, 0x003f9, 0x003f7,
   0x003f5, 0x001f8, 0x001f7, 0x000fa, 0x000f8, 0x000f6, 0x00079, 0x0003a,
   0x00038, 0x0001a, 0x0000b, 0x00004, 0x00000, 0x0000a, 0x0000c, 0x0001b,
   0x00039, 0x0003b, 0x00078, 0x0007a, 0x000f7, 0x000f9, 0x001f6, 0x001f9,
   0x003f4, 0x003f6, 0x003f8, 0x007f5, 0x007f4, 0x007f6, 0x007f7, 0x00ff5,
   0x00ff8, 0x01ff4, 0x01ff6, 0x01ff8, 0x03ff8, 0x03ff4, 0x0fff0, 0x07ff4,
   0x0fff6, 0x07ff5, 0x3ffe2, 0x7ffd9, 0x7ffda, 0x7ffdb, 0x7ffdc, 0x7ffdd,
   0x7ffde, 0x7ffd8, 0x7ffd2, 0x7ffd3, 0x7ffd4, 0x7ffd5, 0x7ffd6, 0x7fff2,
   0x7ffdf, 0x7ffe7, 0x7ffe8, 0x7ffe9, 0x7ffea, 0x7ffeb, 0x7ffe6, 0x7ffe0,
   0x7ffe1, 0x7ffe2, 0x7ffe3, 0x7ffe4, 0x7ffe5, 0x7ffd7, 0x7ffec, 0x7fff4,
   0x7fff3
};
static const uint8_t raac_sf_bits[121] = {
   18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   19, 19, 19, 18, 19, 18, 17, 17, 16, 17, 16, 16, 16, 16, 15, 15,
   14, 14, 14, 14, 14, 14, 13, 13, 12, 12, 12, 11, 12, 11, 10, 10,
   10, 9, 9, 8, 8, 8, 7, 6, 6, 5, 4, 3, 1, 4, 4, 5,
   6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 10, 11, 11, 11, 11, 12,
   12, 13, 13, 13, 14, 14, 16, 15, 16, 15, 18, 19, 19, 19, 19, 19,
   19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   19, 19, 19, 19, 19, 19, 19, 19, 19
};
static const uint16_t raac_swb_offset_1024_16[44] = {
   0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88,
   100, 112, 124, 136, 148, 160, 172, 184, 196, 212, 228, 244,
   260, 280, 300, 320, 344, 368, 396, 424, 456, 492, 532, 572,
   616, 664, 716, 772, 832, 896, 960, 1024
};
static const uint16_t raac_swb_offset_1024_24[48] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44,
   52, 60, 68, 76, 84, 92, 100, 108, 116, 124, 136, 148,
   160, 172, 188, 204, 220, 240, 260, 284, 308, 336, 364, 396,
   432, 468, 508, 552, 600, 652, 704, 768, 832, 896, 960, 1024
};
static const uint16_t raac_swb_offset_1024_32[52] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48,
   56, 64, 72, 80, 88, 96, 108, 120, 132, 144, 160, 176,
   196, 216, 240, 264, 292, 320, 352, 384, 416, 448, 480, 512,
   544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896,
   928, 960, 992, 1024
};
static const uint16_t raac_swb_offset_1024_48[50] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48,
   56, 64, 72, 80, 88, 96, 108, 120, 132, 144, 160, 176,
   196, 216, 240, 264, 292, 320, 352, 384, 416, 448, 480, 512,
   544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896,
   928, 1024
};
static const uint16_t raac_swb_offset_1024_64[48] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44,
   48, 52, 56, 64, 72, 80, 88, 100, 112, 124, 140, 156,
   172, 192, 216, 240, 268, 304, 344, 384, 424, 464, 504, 544,
   584, 624, 664, 704, 744, 784, 824, 864, 904, 944, 984, 1024
};
static const uint16_t raac_swb_offset_1024_8[41] = {
   0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132,
   144, 156, 172, 188, 204, 220, 236, 252, 268, 288, 308, 328,
   348, 372, 396, 420, 448, 476, 508, 544, 580, 620, 664, 712,
   764, 820, 880, 944, 1024
};
static const uint16_t raac_swb_offset_1024_96[42] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44,
   48, 52, 56, 64, 72, 80, 88, 96, 108, 120, 132, 144,
   156, 172, 188, 212, 240, 276, 320, 384, 448, 512, 576, 640,
   704, 768, 832, 896, 960, 1024
};
static const uint16_t *const raac_swb_offset_1024[13] = {
   raac_swb_offset_1024_96, raac_swb_offset_1024_96, raac_swb_offset_1024_64,
   raac_swb_offset_1024_48, raac_swb_offset_1024_48, raac_swb_offset_1024_32,
   raac_swb_offset_1024_24, raac_swb_offset_1024_24, raac_swb_offset_1024_16,
   raac_swb_offset_1024_16, raac_swb_offset_1024_16, raac_swb_offset_1024_8,
   raac_swb_offset_1024_8
};
static const uint16_t raac_swb_offset_128_16[16] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60,
   72, 88, 108, 128
};
static const uint16_t raac_swb_offset_128_24[16] = {
   0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64,
   76, 92, 108, 128
};
static const uint16_t raac_swb_offset_128_48[15] = {
   0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80,
   96, 112, 128
};
static const uint16_t raac_swb_offset_128_96[13] = {
   0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128
};
static const uint16_t raac_swb_offset_128_8[16] = {
   0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 128
};
static const uint16_t *const raac_swb_offset_128[13] = {
   raac_swb_offset_128_96, raac_swb_offset_128_96, raac_swb_offset_128_96,
   raac_swb_offset_128_48, raac_swb_offset_128_48, raac_swb_offset_128_48,
   raac_swb_offset_128_24, raac_swb_offset_128_24, raac_swb_offset_128_16,
   raac_swb_offset_128_16, raac_swb_offset_128_16, raac_swb_offset_128_8,
   raac_swb_offset_128_8
};
static const uint8_t raac_num_swb_1024[13] = {
   41, 41, 47, 49, 49, 51, 47, 47, 43, 43, 43, 40, 40
};
static const uint8_t raac_num_swb_128[13] = {
   12, 12, 12, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15
};
static const uint8_t raac_tns_max_bands_1024[13] = {
   31, 31, 34, 40, 42, 51, 46, 46, 42, 42, 42, 39, 39
};
static const uint8_t raac_tns_max_bands_128[13] = {
   9, 9, 10, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14
};

static const unsigned raac_sample_rates[13] = {
   96000, 88200, 64000, 48000, 44100, 32000,
   24000, 22050, 16000, 12000, 11025, 8000, 7350
};

/* ===== bit reader (MSB first) ===== */

typedef struct
{
   const uint8_t *buf;
   size_t         size;
   size_t         pos;      /* bit position    */
   int            err;      /* ran off the end */
} raac_bits;

static void raac_bits_init(raac_bits *b, const uint8_t *buf, size_t size)
{
   b->buf = buf; b->size = size; b->pos = 0; b->err = 0;
}

static uint32_t raac_getbits(raac_bits *b, int n)
{
   uint32_t v = 0;
   while (n > 0)
   {
      size_t byte = b->pos >> 3;
      int avail   = 8 - (int)(b->pos & 7);
      int take    = n < avail ? n : avail;
      if (byte >= b->size)
      {
         b->err = 1;
         return 0;
      }
      v = (v << take)
        | ((uint32_t)(b->buf[byte] >> (avail - take)) & ((1u << take) - 1));
      b->pos += (size_t)take;
      n      -= take;
   }
   return v;
}

static uint32_t raac_getbit(raac_bits *b)
{
   return raac_getbits(b, 1);
}

/* ===== Huffman decoding =====
 *
 * The codebooks are small; decode by peeking bits and linearly
 * matching against the (code, length) list bucketed by length. To
 * keep it fast enough a first-fit table maps an 8-bit peek straight
 * to a symbol for codes of eight bits or fewer; longer codes fall
 * back to the linear scan. Tables are built once at open. */

typedef struct
{
   int16_t  fast[256];      /* symbol for a code <= 8 bits, else -1  */
   uint8_t  fast_len[256];
   const uint16_t *code;
   const uint8_t  *bits;
   int      n;
   uint32_t max_len;
} raac_huff;

static void raac_huff_build(raac_huff *h, const uint16_t *code,
      const uint8_t *bits, int n)
{
   int i;
   h->code = code; h->bits = bits; h->n = n; h->max_len = 0;
   for (i = 0; i < 256; i++)
   {
      h->fast[i]     = -1;
      h->fast_len[i] = 0;
   }
   for (i = 0; i < n; i++)
   {
      if (bits[i] > h->max_len)
         h->max_len = bits[i];
      if (bits[i] <= 8)
      {
         uint32_t pre  = (uint32_t)code[i] << (8 - bits[i]);
         uint32_t fill = 1u << (8 - bits[i]);
         uint32_t k;
         for (k = 0; k < fill; k++)
         {
            h->fast[pre + k]     = (int16_t)i;
            h->fast_len[pre + k] = bits[i];
         }
      }
   }
}

/* the scalefactor codebook holds 19-bit codes: same scheme, 32-bit list */
typedef struct
{
   int16_t  fast[256];
   uint8_t  fast_len[256];
} raac_huff_sf;

static int raac_huff_decode(raac_bits *b, const raac_huff *h)
{
   size_t   save = b->pos;
   uint32_t peek = 0;
   uint32_t len, i;
   int      have = 0;
   /* peek up to 8 bits without committing */
   while (have < 8 && b->pos < b->size * 8)
   {
      peek = (peek << 1) | raac_getbit(b);
      have++;
   }
   peek <<= (8 - have);
   b->pos = save;
   if (have > 0 && h->fast[peek] >= 0 && h->fast_len[peek] <= (uint8_t)have)
   {
      b->pos += h->fast_len[peek];
      return h->fast[peek];
   }
   /* long code: extend bit by bit and scan matching lengths */
   {
      uint32_t acc = 0;
      b->pos = save;
      for (len = 1; len <= h->max_len; len++)
      {
         acc = (acc << 1) | raac_getbit(b);
         if (b->err)
            return -1;
         if (len < 9)
            continue;   /* would have hit the fast table */
         for (i = 0; i < (uint32_t)h->n; i++)
            if (h->bits[i] == len && h->code[i] == acc)
               return (int)i;
      }
   }
   b->err = 1;
   return -1;
}

static int raac_huff_decode_sf(raac_bits *b, const raac_huff_sf *h)
{
   size_t   save = b->pos;
   uint32_t peek = 0, acc = 0, len, i;
   int      have = 0;
   while (have < 8 && b->pos < b->size * 8)
   {
      peek = (peek << 1) | raac_getbit(b);
      have++;
   }
   peek <<= (8 - have);
   b->pos = save;
   if (have > 0 && h->fast[peek] >= 0 && h->fast_len[peek] <= (uint8_t)have)
   {
      b->pos += h->fast_len[peek];
      return h->fast[peek];
   }
   b->pos = save;
   for (len = 1; len <= 19; len++)
   {
      acc = (acc << 1) | raac_getbit(b);
      if (b->err)
         return -1;
      if (len < 9)
         continue;
      for (i = 0; i < 121; i++)
         if (raac_sf_bits[i] == len && raac_sf_code[i] == acc)
            return (int)i;
   }
   b->err = 1;
   return -1;
}

/* ===== per-channel and decoder state ===== */

typedef struct
{
   /* ics_info */
   int      window_sequence;      /* 0 long,1 start,2 eight-short,3 stop */
   int      window_shape;         /* 0 sine, 1 KBD                       */
   int      max_sfb;
   int      num_windows;          /* 1 or 8                              */
   int      num_window_groups;
   int      group_len[RAAC_MAX_WIN];
   /* per (group, sfb) */
   uint8_t  band_cb[RAAC_MAX_WIN][RAAC_MAX_SFB];
   int      sf[RAAC_MAX_WIN][RAAC_MAX_SFB];    /* scalefactor / IS pos /
                                                * noise energy          */
   /* tns */
   int      tns_present;
   int      tns_n_filt[RAAC_MAX_WIN];
   int      tns_length[RAAC_MAX_WIN][3];
   int      tns_order[RAAC_MAX_WIN][3];
   int      tns_direction[RAAC_MAX_WIN][3];
   float    tns_coef[RAAC_MAX_WIN][3][20];
   /* spectra */
   float    coef[RAAC_FRAME];
   float    overlap[RAAC_FRAME];               /* second half of the
                                                * previous frame        */
   int      prev_window_shape;
} raac_ch;

struct raac
{
   unsigned sample_rate;
   unsigned channels;
   int      sfi;                  /* sampling frequency index            */
   raac_ch  ch[RAAC_MAX_CH];
   raac_huff     spec[11];
   raac_huff_sf  sfh;
   /* windows */
   float    kbd_long[2048];
   float    kbd_short[256];
   float    sine_long[2048];
   float    sine_short[256];
   /* FFT twiddles for the two IMDCT sizes (N/4-point complex FFT) */
   float    tw512_re[1024], tw512_im[1024];    /* pre+post twiddles 2048 */
   float    tw64_re[128],  tw64_im[128];       /* pre+post twiddles 256  */
   float    fft_re[512], fft_im[512];          /* scratch                */
   uint32_t noise_state;                        /* PNS LCG               */
};

/* ===== windows and twiddles ===== */

static double raac_bessel_i0(double x)
{
   double sum = 1.0, term = 1.0;
   int    k;
   for (k = 1; k < 64; k++)
   {
      term *= (x / (2.0 * k)) * (x / (2.0 * k));
      sum  += term;
      if (term < 1e-21 * sum)
         break;
   }
   return sum;
}

/* Kaiser-Bessel derived window (14496-3 4.6.11.3.2): alpha 4 for the
 * long window, 6 for the short. */
static void raac_kbd_window(float *w, int n, double alpha)
{
   double *kern = (double*)malloc(sizeof(double) * (size_t)(n / 2 + 1));
   double  sum = 0.0, acc = 0.0;
   int     i;
   if (!kern)
   {
      for (i = 0; i < n; i++)
         w[i] = 1.0f;   /* degraded but functional on OOM */
      return;
   }
   for (i = 0; i <= n / 2; i++)
   {
      /* the kernel is a Kaiser window over the half length: its own
       * argument spans -1..1 across n/2 points, so 4i/n - 1 */
      double t = 4.0 * i / n - 1.0;
      kern[i]  = raac_bessel_i0(M_PI * alpha * sqrt(1.0 - t * t));
      sum     += kern[i];
   }
   for (i = 0; i < n / 2; i++)
   {
      acc += kern[i];
      w[i] = (float)sqrt(acc / (sum));
   }
   free(kern);
   for (i = n / 2; i < n; i++)
      w[i] = w[n - 1 - i];
}

static void raac_sine_window(float *w, int n)
{
   int i;
   for (i = 0; i < n; i++)
      w[i] = (float)sin(M_PI / n * (i + 0.5));
}

/* ===== IMDCT via N/4-point complex FFT =====
 *
 * Standard formulation: pre-twiddle the N/2 spectral pairs, run an
 * N/4 complex FFT, post-twiddle, and scatter the quarters with the
 * MDCT symmetries. Output is the full N time samples of this frame's
 * windowed contribution before overlap-add (scale 2/N folded in). */

static void raac_fft(float *re, float *im, int n)
{
   int i, j, k, len;
   /* bit reversal */
   for (i = 1, j = 0; i < n; i++)
   {
      int bit = n >> 1;
      for (; j & bit; bit >>= 1)
         j ^= bit;
      j |= bit;
      if (i < j)
      {
         float t;
         t = re[i]; re[i] = re[j]; re[j] = t;
         t = im[i]; im[i] = im[j]; im[j] = t;
      }
   }
   for (len = 2; len <= n; len <<= 1)
   {
      double ang = -2.0 * M_PI / len;
      float  wr0 = (float)cos(ang), wi0 = (float)sin(ang);
      for (i = 0; i < n; i += len)
      {
         float wr = 1.0f, wi = 0.0f;
         for (j = 0; j < len / 2; j++)
         {
            float ur = re[i + j],           ui = im[i + j];
            float vr = re[i + j + len / 2], vi = im[i + j + len / 2];
            float tr = vr * wr - vi * wi;
            float ti = vr * wi + vi * wr;
            float nr;
            re[i + j] = ur + tr; im[i + j] = ui + ti;
            re[i + j + len / 2] = ur - tr;
            im[i + j + len / 2] = ui - ti;
            nr = wr * wr0 - wi * wi0;
            wi = wr * wi0 + wi * wr0;
            wr = nr;
         }
      }
   }
   (void)k;
}

/* in: N/2 spectral coefficients X, out: N time samples (2/N folded in).
 * Derivation: y is a signed/mirrored rearrangement of the length-N/2
 * DCT-IV of X, and the DCT-IV runs on an N/4-point complex FFT with
 * exp(-j*pi*(4k+1)/(4M)) pre- and exp(-j*pi*k/M) post-twiddles; both
 * identities were verified numerically to machine precision against
 * the defining cosine sum before this implementation. */
static void raac_imdct(raac_t *a, const float *x, float *out, int n)
{
   int    n4 = n / 4, n2 = n / 2;
   float *fre = a->fft_re, *fim = a->fft_im;
   const float *pr = (n == 2048) ? a->tw512_re : a->tw64_re;
   const float *pi_ = (n == 2048) ? a->tw512_im : a->tw64_im;
   const float *qr = (n == 2048) ? a->tw512_re + n4 : a->tw64_re + n4;
   const float *qi = (n == 2048) ? a->tw512_im + n4 : a->tw64_im + n4;
   float  v[1024];
   int    k;

   for (k = 0; k < n4; k++)
   {
      float xr = x[2 * k];
      float xi = x[n2 - 1 - 2 * k];
      fre[k] = xr * pr[k] - xi * pi_[k];
      fim[k] = xr * pi_[k] + xi * pr[k];
   }
   raac_fft(fre, fim, n4);
   for (k = 0; k < n4; k++)
   {
      float yr = fre[k] * qr[k] - fim[k] * qi[k];
      float yi = fre[k] * qi[k] + fim[k] * qr[k];
      v[2 * k]           =  yr;
      v[n2 - 1 - 2 * k]  = -yi;
   }
   for (k = 0; k < n4; k++)
   {
      float s = 2.0f / n;
      out[k]           =  v[n4 + k] * s;
      out[n4 + k]      = -v[n2 - 1 - k] * s;
      out[n2 + k]      = -v[n4 - 1 - k] * s;
      out[n2 + n4 + k] = -v[k] * s;
   }
}

/* twr/twi hold the pre-twiddles in [0..n4) and the post-twiddles in
 * [n4..n2): pre exp(-j*pi*(4k+1)/(4M)), post exp(-j*pi*k/M), M = n/2. */
static void raac_make_twiddles(float *twr, float *twi, int n4, int n)
{
   int    m = n / 2;
   int    k;
   for (k = 0; k < n4; k++)
   {
      double pre  = -M_PI * (4.0 * k + 1.0) / (4.0 * m);
      double post = -M_PI * k / m;
      twr[k]      = (float)cos(pre);
      twi[k]      = (float)sin(pre);
      twr[n4 + k] = (float)cos(post);
      twi[n4 + k] = (float)sin(post);
   }
}

/* ===== syntax: ics_info, sections, scale factors, pulse, tns ===== */

static const uint16_t *raac_swb_off(const raac_t *a, int short_win)
{
   return short_win ? raac_swb_offset_128[a->sfi]
                    : raac_swb_offset_1024[a->sfi];
}

static int raac_num_swb(const raac_t *a, int short_win)
{
   return short_win ? raac_num_swb_128[a->sfi]
                    : raac_num_swb_1024[a->sfi];
}

static int raac_ics_info(raac_t *a, raac_bits *b, raac_ch *c)
{
   int short_win;
   raac_getbit(b);                        /* ics_reserved              */
   c->window_sequence = (int)raac_getbits(b, 2);
   c->window_shape    = (int)raac_getbit(b);
   short_win = (c->window_sequence == 2);
   if (short_win)
   {
      uint32_t grouping;
      int g, w;
      c->max_sfb  = (int)raac_getbits(b, 4);
      grouping    = raac_getbits(b, 7);
      c->num_windows       = 8;
      c->num_window_groups = 1;
      c->group_len[0]      = 1;
      for (w = 0; w < 7; w++)
      {
         if (grouping & (0x40u >> w))
            c->group_len[c->num_window_groups - 1]++;
         else
         {
            c->num_window_groups++;
            c->group_len[c->num_window_groups - 1] = 1;
         }
      }
      (void)g;
   }
   else
   {
      c->max_sfb = (int)raac_getbits(b, 6);
      if (raac_getbit(b))                 /* predictor_data_present    */
         return -1;                       /* Main/LTP tools: out of scope */
      c->num_windows       = 1;
      c->num_window_groups = 1;
      c->group_len[0]      = 1;
   }
   if (c->max_sfb > raac_num_swb(a, short_win))
      return -1;
   return b->err ? -1 : 0;
}

static int raac_section_data(raac_t *a, raac_bits *b, raac_ch *c)
{
   int sect_bits = (c->window_sequence == 2) ? 3 : 5;
   int esc       = (1 << sect_bits) - 1;
   int g;
   for (g = 0; g < c->num_window_groups; g++)
   {
      int k = 0;
      while (k < c->max_sfb)
      {
         int cb = (int)raac_getbits(b, 4);
         int len = 0, l;
         if (cb == 12)             /* reserved codebook               */
            return -1;
         do
         {
            l    = (int)raac_getbits(b, sect_bits);
            len += l;
         } while (l == esc);
         if (k + len > c->max_sfb || b->err)
            return -1;
         for (l = 0; l < len; l++)
            c->band_cb[g][k + l] = (uint8_t)cb;
         k += len;
      }
   }
   return 0;
}

static int raac_scale_factor_data(raac_t *a, raac_bits *b, raac_ch *c,
      int global_gain)
{
   int sf = global_gain;
   int is_pos = 0;
   int noise = global_gain - 90;
   int noise_first = 1;
   int g, k;
   for (g = 0; g < c->num_window_groups; g++)
      for (k = 0; k < c->max_sfb; k++)
      {
         int cb = c->band_cb[g][k];
         if (cb == RAAC_CB_ZERO)
         {
            c->sf[g][k] = 0;
            continue;
         }
         if (cb == RAAC_CB_INTENSITY || cb == RAAC_CB_INTENSITY2)
         {
            int d = raac_huff_decode_sf(b, &a->sfh);
            if (d < 0)
               return -1;
            is_pos     += d - 60;
            c->sf[g][k] = is_pos;
            continue;
         }
         if (cb == RAAC_CB_NOISE)
         {
            if (noise_first)
            {
               noise      += (int)raac_getbits(b, 9) - 256;
               noise_first = 0;
            }
            else
            {
               int d = raac_huff_decode_sf(b, &a->sfh);
               if (d < 0)
                  return -1;
               noise += d - 60;
            }
            if (noise < -100) noise = -100;
            if (noise >  155) noise =  155;
            c->sf[g][k] = noise;
            continue;
         }
         {
            int d = raac_huff_decode_sf(b, &a->sfh);
            if (d < 0)
               return -1;
            sf += d - 60;
            if (sf < 0 || sf > 255)
               return -1;
            c->sf[g][k] = sf;
         }
      }
   return b->err ? -1 : 0;
}

typedef struct
{
   int pulse_present;
   int number;
   int start_sfb;
   int offset[4];
   int amp[4];
} raac_pulse;

static int raac_pulse_data(raac_bits *b, raac_pulse *p)
{
   int i;
   p->number    = (int)raac_getbits(b, 2) + 1;
   p->start_sfb = (int)raac_getbits(b, 6);
   for (i = 0; i < p->number; i++)
   {
      p->offset[i] = (int)raac_getbits(b, 5);
      p->amp[i]    = (int)raac_getbits(b, 4);
   }
   return b->err ? -1 : 0;
}

static int raac_tns_data(raac_t *a, raac_bits *b, raac_ch *c)
{
   int short_win = (c->window_sequence == 2);
   int w;
   for (w = 0; w < c->num_windows; w++)
   {
      int nf = (int)raac_getbits(b, short_win ? 1 : 2);
      int coef_res = 0, f;
      c->tns_n_filt[w] = nf;
      if (nf)
         coef_res = (int)raac_getbit(b);
      for (f = 0; f < nf; f++)
      {
         int length = (int)raac_getbits(b, short_win ? 4 : 6);
         int order  = (int)raac_getbits(b, short_win ? 3 : 5);
         int i;
         c->tns_length[w][f] = length;
         c->tns_order[w][f]  = order;
         if (order > 20)
            return -1;
         if (order)
         {
            int direction = (int)raac_getbit(b);
            int compress  = (int)raac_getbit(b);
            int coef_bits = coef_res + 3 - compress;
            /* inverse quantisation per 14496-3 tns_decode_coef:
             * negative codes count down from the top of the range */
            double iqfac  = ((1 << (coef_res + 2)) - 0.5) / (M_PI / 2.0);
            double iqfac_m = ((1 << (coef_res + 2)) + 0.5) / (M_PI / 2.0);
            c->tns_direction[w][f] = direction;
            for (i = 0; i < order; i++)
            {
               int v = (int)raac_getbits(b, coef_bits);
               int s = v & (1 << (coef_bits - 1)) ? v - (1 << coef_bits) : v;
               c->tns_coef[w][f][i] = (float)sin(s /
                     (s >= 0 ? iqfac : iqfac_m));
            }
         }
      }
   }
   return b->err ? -1 : 0;
}

/* ===== spectral data ===== */

static int raac_spectral_data(raac_t *a, raac_bits *b, raac_ch *c,
      int quant[RAAC_FRAME])
{
   const uint16_t *swb = raac_swb_off(a, c->window_sequence == 2);
   int g, win_base = 0;
   memset(quant, 0, sizeof(int) * RAAC_FRAME);
   for (g = 0; g < c->num_window_groups; g++)
   {
      int glen = c->group_len[g];
      int k;
      for (k = 0; k < c->max_sfb; k++)
      {
         int cb = c->band_cb[g][k];
         int w;
         if (cb == RAAC_CB_ZERO || cb >= RAAC_CB_NOISE)
            continue;
         for (w = 0; w < glen; w++)
         {
            int lo = swb[k], hi = swb[k + 1];
            int base = win_base + w * 128;
            int i;
            if (cb <= 4)
            {
               for (i = lo; i < hi; i += 4)
               {
                  int idx = raac_huff_decode(b, &a->spec[cb - 1]);
                  int q[4], j;
                  if (idx < 0)
                     return -1;
                  if (cb <= 2)
                  {
                     q[0] = idx / 27 % 3 - 1;
                     q[1] = idx / 9  % 3 - 1;
                     q[2] = idx / 3  % 3 - 1;
                     q[3] = idx      % 3 - 1;
                  }
                  else
                  {
                     q[0] = idx / 27 % 3;
                     q[1] = idx / 9  % 3;
                     q[2] = idx / 3  % 3;
                     q[3] = idx      % 3;
                     for (j = 0; j < 4; j++)
                        if (q[j] && raac_getbit(b))
                           q[j] = -q[j];
                  }
                  for (j = 0; j < 4; j++)
                     quant[base + i + j] = q[j];
               }
            }
            else
            {
               for (i = lo; i < hi; i += 2)
               {
                  int idx = raac_huff_decode(b, &a->spec[cb - 1]);
                  int q[2], j, lav;
                  if (idx < 0)
                     return -1;
                  if (cb <= 6)          /* signed pairs, -4..4        */
                  {
                     q[0] = idx / 9 - 4;
                     q[1] = idx % 9 - 4;
                  }
                  else
                  {
                     lav  = (cb <= 8) ? 8 : (cb <= 10 ? 13 : 17);
                     q[0] = idx / lav;
                     q[1] = idx % lav;
                     for (j = 0; j < 2; j++)
                        if (q[j] && raac_getbit(b))
                           q[j] = -q[j];
                  }
                  if (cb == RAAC_ESC_BOOK)
                  {
                     for (j = 0; j < 2; j++)
                        if (q[j] == 16 || q[j] == -16)
                        {
                           int nb = 4;
                           int sign = q[j] < 0 ? -1 : 1;
                           int esc;
                           while (raac_getbit(b))
                              nb++;
                           if (nb > 21 || b->err)
                              return -1;
                           esc  = (int)((1u << nb)
                                | raac_getbits(b, nb));
                           q[j] = sign * esc;
                        }
                  }
                  quant[base + i]     = q[0];
                  quant[base + i + 1] = q[1];
               }
            }
         }
      }
      win_base += glen * 128;
   }
   return b->err ? -1 : 0;
}

/* ===== channel decode: dequant, tools, filterbank ===== */

static float raac_iquant(int q)
{
   float a = (float)(q < 0 ? -q : q);
   float v = (float)pow(a, 4.0 / 3.0);
   return q < 0 ? -v : v;
}

/* quantised -> scaled spectrum, in window-interleaved layout for
 * short sequences (as spectral_data stored it: groups x windows x 128) */
static void raac_dequant(raac_t *a, raac_ch *c, const int quant[RAAC_FRAME])
{
   const uint16_t *swb = raac_swb_off(a, c->window_sequence == 2);
   int g, win_base = 0;
   memset(c->coef, 0, sizeof(c->coef));
   for (g = 0; g < c->num_window_groups; g++)
   {
      int glen = c->group_len[g], k;
      for (k = 0; k < c->max_sfb; k++)
      {
         int cb = c->band_cb[g][k];
         float gain;
         int w;
         if (cb == RAAC_CB_ZERO || cb >= RAAC_CB_NOISE)
            continue;
         gain = (float)pow(2.0, 0.25 * (c->sf[g][k] - 100));
         for (w = 0; w < glen; w++)
         {
            int base = win_base + w * 128 + ((c->window_sequence == 2) ? 0 : 0);
            int i;
            for (i = swb[k]; i < swb[k + 1]; i++)
               c->coef[base + i] = raac_iquant(quant[base + i]) * gain;
         }
      }
      win_base += glen * ((c->window_sequence == 2) ? 128 : 1024);
   }
}

/* PNS: fill noise bands with unit-energy noise at the signalled level.
 * The spec leaves the generator free; a fixed LCG keeps it stable. */
static void raac_pns(raac_t *a, raac_ch *c)
{
   const uint16_t *swb = raac_swb_off(a, c->window_sequence == 2);
   int g, win_base = 0;
   for (g = 0; g < c->num_window_groups; g++)
   {
      int glen = c->group_len[g], k;
      for (k = 0; k < c->max_sfb; k++)
      {
         int w;
         if (c->band_cb[g][k] != RAAC_CB_NOISE)
            continue;
         for (w = 0; w < glen; w++)
         {
            int base = win_base + w * 128;
            int lo = swb[k], hi = swb[k + 1];
            float energy = 0.0f, scale;
            int i;
            for (i = lo; i < hi; i++)
            {
               a->noise_state = a->noise_state * 1664525u + 1013904223u;
               c->coef[base + i] =
                     (float)(int32_t)a->noise_state * (1.0f / 2147483648.0f);
               energy += c->coef[base + i] * c->coef[base + i];
            }
            scale = (float)(pow(2.0, 0.25 * c->sf[g][k])
                  / sqrt(energy > 0 ? energy : 1.0f));
            for (i = lo; i < hi; i++)
               c->coef[base + i] *= scale;
         }
      }
      win_base += glen * ((c->window_sequence == 2) ? 128 : 1024);
   }
}

/* M/S: mid/side -> left/right on bands both channels code normally */
static void raac_ms_stereo(raac_t *a, raac_ch *l, raac_ch *r,
      int ms_mask_all, uint8_t ms_used[RAAC_MAX_WIN][RAAC_MAX_SFB])
{
   const uint16_t *swb = raac_swb_off(a, l->window_sequence == 2);
   int g, win_base = 0;
   for (g = 0; g < l->num_window_groups; g++)
   {
      int glen = l->group_len[g], k;
      for (k = 0; k < l->max_sfb; k++)
      {
         int lcb = l->band_cb[g][k], rcb = r->band_cb[g][k];
         int w;
         if (!(ms_mask_all || ms_used[g][k]))
            continue;
         if (lcb >= RAAC_CB_NOISE || rcb >= RAAC_CB_NOISE)
            continue;   /* intensity/noise bands are not M/S           */
         for (w = 0; w < glen; w++)
         {
            int base = win_base + w * 128;
            int i;
            for (i = swb[k]; i < swb[k + 1]; i++)
            {
               float m = l->coef[base + i], s = r->coef[base + i];
               l->coef[base + i] = m + s;
               r->coef[base + i] = m - s;
            }
         }
      }
      win_base += glen * ((l->window_sequence == 2) ? 128 : 1024);
   }
}

/* intensity stereo: right-channel bands coded 14/15 take the left
 * spectrum scaled by 0.5^(is_pos/4); codebook 14 flips the sign, and
 * an M/S flag on the band flips it again. */
static void raac_intensity(raac_t *a, raac_ch *l, raac_ch *r,
      int ms_mask_all, uint8_t ms_used[RAAC_MAX_WIN][RAAC_MAX_SFB])
{
   const uint16_t *swb = raac_swb_off(a, r->window_sequence == 2);
   int g, win_base = 0;
   for (g = 0; g < r->num_window_groups; g++)
   {
      int glen = r->group_len[g], k;
      for (k = 0; k < r->max_sfb; k++)
      {
         int cb = r->band_cb[g][k];
         float dir, scale;
         int w;
         if (cb != RAAC_CB_INTENSITY && cb != RAAC_CB_INTENSITY2)
            continue;
         dir = (cb == RAAC_CB_INTENSITY) ? 1.0f : -1.0f;
         if (ms_mask_all || ms_used[g][k])
            dir = -dir;
         scale = dir * (float)pow(0.5, 0.25 * r->sf[g][k]);
         for (w = 0; w < glen; w++)
         {
            int base = win_base + w * 128;
            int i;
            for (i = swb[k]; i < swb[k + 1]; i++)
               r->coef[base + i] = l->coef[base + i] * scale;
         }
      }
      win_base += glen * ((r->window_sequence == 2) ? 128 : 1024);
   }
}

/* TNS synthesis: all-pole filtering of the spectrum over each
 * signalled band range (14496-3 4.6.9) */
static void raac_tns_apply(raac_t *a, raac_ch *c)
{
   int short_win = (c->window_sequence == 2);
   const uint16_t *swb = raac_swb_off(a, short_win);
   int num_swb = raac_num_swb(a, short_win);
   int tns_max = short_win ? raac_tns_max_bands_128[a->sfi]
                           : raac_tns_max_bands_1024[a->sfi];
   int mmm = tns_max < c->max_sfb ? tns_max : c->max_sfb;
   int w;
   if (!c->tns_present)
      return;
   for (w = 0; w < c->num_windows; w++)
   {
      int bottom = num_swb;
      int f;
      for (f = 0; f < c->tns_n_filt[w]; f++)
      {
         int top    = bottom;
         int order  = c->tns_order[w][f];
         int start, end, size, inc, i;
         float lpc[21];
         bottom = top - c->tns_length[w][f];
         if (bottom < 0)
            bottom = 0;
         if (!order)
            continue;
         /* reflection coefficients -> direct form (Levinson step) */
         {
            float tmp[21];
            int m, ii;
            lpc[0] = 1.0f;
            for (m = 1; m <= order; m++)
            {
               float rc = c->tns_coef[w][f][m - 1];
               for (ii = 1; ii < m; ii++)
                  tmp[ii] = lpc[ii] + rc * lpc[m - ii];
               for (ii = 1; ii < m; ii++)
                  lpc[ii] = tmp[ii];
               lpc[m] = rc;
            }
         }
         start = swb[bottom < mmm ? bottom : mmm];
         end   = swb[top    < mmm ? top    : mmm];
         size  = end - start;
         if (size <= 0)
            continue;
         start += w * 128;
         if (c->tns_direction[w][f])
         {
            start = start + size - 1;
            inc   = -1;
         }
         else
            inc = 1;
         for (i = 0; i < size; i++)
         {
            float v = c->coef[start];
            int j;
            for (j = 1; j <= order && j <= i; j++)
               v -= lpc[j] * c->coef[start - j * inc];
            c->coef[start] = v;
            start += inc;
         }
      }
   }
}

/* ===== filterbank: IMDCT + windowing + overlap-add (4.6.11) ===== */

static void raac_filterbank(raac_t *a, raac_ch *c, float out[RAAC_FRAME])
{
   const float *long_cur  = c->window_shape ? a->kbd_long  : a->sine_long;
   const float *shrt_cur  = c->window_shape ? a->kbd_short : a->sine_short;
   const float *long_prev = c->prev_window_shape ? a->kbd_long  : a->sine_long;
   const float *shrt_prev = c->prev_window_shape ? a->kbd_short : a->sine_short;
   float buf[2048];
   float win_out[2048];
   int   i;

   if (c->window_sequence != 2)
   {
      raac_imdct(a, c->coef, buf, 2048);
      /* first half: window with the previous frame's trailing shape */
      switch (c->window_sequence)
      {
         case 0:  /* only long  */
         case 1:  /* long start */
            for (i = 0; i < 1024; i++)
               win_out[i] = buf[i] * long_prev[i];
            break;
         default: /* long stop: flat head after a short run           */
            for (i = 0; i < 448; i++)
               win_out[i] = 0.0f;
            for (i = 0; i < 128; i++)
               win_out[448 + i] = buf[448 + i] * shrt_prev[i];
            for (i = 576; i < 1024; i++)
               win_out[i] = buf[i];
            break;
      }
      /* second half: this frame's trailing shape */
      switch (c->window_sequence)
      {
         case 0:  /* long tail */
         case 3:
            for (i = 0; i < 1024; i++)
               win_out[1024 + i] = buf[1024 + i] * long_cur[1024 + i];
            break;
         default: /* long start: flat, then a short tail              */
            for (i = 0; i < 448; i++)
               win_out[1024 + i] = buf[1024 + i];
            for (i = 0; i < 128; i++)
               win_out[1472 + i] = buf[1472 + i] * shrt_cur[128 + i];
            for (i = 1600; i < 2048; i++)
               win_out[i] = 0.0f;
            break;
      }
   }
   else
   {
      /* eight short windows at 128-sample stride starting at 448 */
      float acc[2048];
      int   w;
      memset(acc, 0, sizeof(acc));
      for (w = 0; w < 8; w++)
      {
         float sbuf[256];
         raac_imdct(a, c->coef + w * 128, sbuf, 256);
         for (i = 0; i < 256; i++)
         {
            const float *head = (w == 0) ? shrt_prev : shrt_cur;
            float win = (i < 128) ? head[i] : shrt_cur[i];
            acc[448 + w * 128 + i] += sbuf[i] * win;
         }
      }
      memcpy(win_out, acc, sizeof(acc));
   }

   for (i = 0; i < 1024; i++)
      out[i] = win_out[i] + c->overlap[i];
   memcpy(c->overlap, win_out + 1024, sizeof(float) * 1024);
   c->prev_window_shape = c->window_shape;
}

/* ===== element decode ===== */

static int raac_decode_ics(raac_t *a, raac_bits *b, raac_ch *c,
      int common_window)
{
   int quant[RAAC_FRAME];
   raac_pulse pulse;
   int global_gain = (int)raac_getbits(b, 8);
   pulse.pulse_present = 0;
   if (!common_window)
      if (raac_ics_info(a, b, c) < 0)
         return -1;
   if (raac_section_data(a, b, c) < 0)
      return -1;
   if (raac_scale_factor_data(a, b, c, global_gain) < 0)
      return -1;
   if (raac_getbit(b))               /* pulse_data_present            */
   {
      if (c->window_sequence == 2)
         return -1;                  /* pulse is long-window only     */
      pulse.pulse_present = 1;
      if (raac_pulse_data(b, &pulse) < 0)
         return -1;
   }
   c->tns_present = (int)raac_getbit(b);
   if (c->tns_present)
      if (raac_tns_data(a, b, c) < 0)
         return -1;
   if (raac_getbit(b))               /* gain_control_data_present     */
      return -1;                     /* SSR tool: out of scope        */
   if (raac_spectral_data(a, b, c, quant) < 0)
      return -1;
   if (pulse.pulse_present)
   {
      const uint16_t *swb = raac_swb_off(a, 0);
      int k, i;
      if (pulse.start_sfb > raac_num_swb(a, 0))
         return -1;
      k = swb[pulse.start_sfb];
      for (i = 0; i < pulse.number; i++)
      {
         k += pulse.offset[i];
         if (k >= RAAC_FRAME)
            return -1;
         if (quant[k] > 0)
            quant[k] += pulse.amp[i];
         else
            quant[k] -= pulse.amp[i];   /* zero takes the negative sign,
                                         * matching deployed decoders   */
      }
   }
   raac_dequant(a, c, quant);
   return 0;
}

static int raac_decode_sce(raac_t *a, raac_bits *b, raac_ch *c)
{
   raac_getbits(b, 4);               /* element_instance_tag          */
   if (raac_decode_ics(a, b, c, 0) < 0)
      return -1;
   raac_pns(a, c);
   raac_tns_apply(a, c);
   return 0;
}

static int raac_decode_cpe(raac_t *a, raac_bits *b)
{
   raac_ch *l = &a->ch[0], *r = &a->ch[1];
   uint8_t  ms_used[RAAC_MAX_WIN][RAAC_MAX_SFB];
   int      ms_mask = 0;
   int      common;
   raac_getbits(b, 4);
   common = (int)raac_getbit(b);
   memset(ms_used, 0, sizeof(ms_used));
   if (common)
   {
      if (raac_ics_info(a, b, l) < 0)
         return -1;
      memcpy(&r->window_sequence, &l->window_sequence,
            (char*)&l->band_cb - (char*)&l->window_sequence);
      ms_mask = (int)raac_getbits(b, 2);
      if (ms_mask == 3)
         return -1;
      if (ms_mask == 1)
      {
         int g, k;
         for (g = 0; g < l->num_window_groups; g++)
            for (k = 0; k < l->max_sfb; k++)
               ms_used[g][k] = (uint8_t)raac_getbit(b);
      }
   }
   if (raac_decode_ics(a, b, l, common) < 0)
      return -1;
   if (raac_decode_ics(a, b, r, common) < 0)
      return -1;
   raac_pns(a, l);
   raac_pns(a, r);
   if (common)
      raac_ms_stereo(a, l, r, ms_mask == 2, ms_used);
   raac_intensity(a, l, r, ms_mask == 2, ms_used);
   raac_tns_apply(a, l);
   raac_tns_apply(a, r);
   return 0;
}

/* ===== public API ===== */

raac_t *raac_open(const uint8_t *asc, size_t asc_size)
{
   raac_t   *a;
   raac_bits b;
   unsigned  aot, sfi, chcfg;
   int       i;

   if (!asc || asc_size < 2)
      return NULL;
   raac_bits_init(&b, asc, asc_size);
   aot = raac_getbits(&b, 5);
   if (aot == 31)
      aot = 32 + raac_getbits(&b, 6);
   sfi = raac_getbits(&b, 4);
   if (sfi == 15)
   {
      raac_getbits(&b, 24);
      return NULL;                    /* explicit-frequency oddity     */
   }
   chcfg = raac_getbits(&b, 4);
   if (aot != 2)                      /* AAC-LC only                   */
      return NULL;
   if (sfi > 12 || chcfg < 1 || chcfg > 2)
      return NULL;
   if (raac_getbit(&b))               /* frameLengthFlag: 960          */
      return NULL;
   if (raac_getbit(&b))               /* dependsOnCoreCoder            */
      return NULL;
   if (raac_getbit(&b))               /* extensionFlag                 */
      return NULL;
   if (b.err)
      return NULL;

   if (!(a = (raac_t*)calloc(1, sizeof(*a))))
      return NULL;
   a->sfi         = (int)sfi;
   a->sample_rate = raac_sample_rates[sfi];
   a->channels    = chcfg;
   a->noise_state = 0x1f2e3d4cu;

   for (i = 0; i < 11; i++)
      raac_huff_build(&a->spec[i], raac_hcb_code[i], raac_hcb_bits[i],
            raac_hcb_size[i]);
   /* scalefactor fast table (19-bit codes; 8-bit prefix slice) */
   for (i = 0; i < 256; i++)
   {
      a->sfh.fast[i]     = -1;
      a->sfh.fast_len[i] = 0;
   }
   for (i = 0; i < 121; i++)
      if (raac_sf_bits[i] <= 8)
      {
         uint32_t pre  = raac_sf_code[i] << (8 - raac_sf_bits[i]);
         uint32_t fill = 1u << (8 - raac_sf_bits[i]);
         uint32_t k;
         for (k = 0; k < fill; k++)
         {
            a->sfh.fast[pre + k]     = (int16_t)i;
            a->sfh.fast_len[pre + k] = raac_sf_bits[i];
         }
      }

   raac_kbd_window(a->kbd_long, 2048, 4.0);
   raac_kbd_window(a->kbd_short, 256, 6.0);
   raac_sine_window(a->sine_long, 2048);
   raac_sine_window(a->sine_short, 256);
   raac_make_twiddles(a->tw512_re, a->tw512_im, 512, 2048);
   raac_make_twiddles(a->tw64_re,  a->tw64_im,  64,  256);
   return a;
}

unsigned raac_channels(const raac_t *a)    { return a ? a->channels : 0; }
unsigned raac_sample_rate(const raac_t *a) { return a ? a->sample_rate : 0; }

/* Shared worker: parse and synthesise one access unit into per-channel
 * float PCM in full-scale (+-32768) units. Both public entry points
 * convert from here at the edge, so the pipeline stays float
 * throughout with no intermediate quantisation. */
static int raac_decode_frame(raac_t *a, const uint8_t *pkt, size_t size,
      float pcm[RAAC_MAX_CH][RAAC_FRAME])
{
   raac_bits b;
   int       got[RAAC_MAX_CH];
   int       done = 0;
   unsigned  ch;

   if (!a || !pkt || !size)
      return -1;
   raac_bits_init(&b, pkt, size);
   got[0] = got[1] = 0;

   while (!done)
   {
      unsigned id = raac_getbits(&b, 3);
      if (b.err)
         return -1;
      switch (id)
      {
         case 0:  /* SCE */
         case 3:  /* LFE (same individual_channel_stream)             */
            if (a->channels != 1 && id == 0 && got[0])
               return -1;
            if (raac_decode_sce(a, &b, &a->ch[0]) < 0)
               return -1;
            got[0] = 1;
            break;
         case 1:  /* CPE */
            if (a->channels != 2)
               return -1;
            if (raac_decode_cpe(a, &b) < 0)
               return -1;
            got[0] = got[1] = 1;
            break;
         case 4:  /* DSE */
         {
            unsigned cnt;
            raac_getbits(&b, 4);
            if (raac_getbit(&b))
               raac_getbits(&b, 3);   /* byte alignment */
            cnt = raac_getbits(&b, 8);
            if (cnt == 255)
               cnt += raac_getbits(&b, 8);
            b.pos = (b.pos + 7) & ~(size_t)7;
            b.pos += (size_t)cnt * 8;
            break;
         }
         case 6:  /* FIL: skip (carries implicit SBR and fill bytes)  */
         {
            unsigned cnt = raac_getbits(&b, 4);
            if (cnt == 15)
               cnt += raac_getbits(&b, 8) - 1;
            b.pos += (size_t)cnt * 8;
            break;
         }
         case 7:  /* END */
            done = 1;
            break;
         default: /* CCE, PCE: out of scope for LC mono/stereo files  */
            return -1;
      }
      if (b.pos > b.size * 8)
         return -1;
   }
   for (ch = 0; ch < a->channels; ch++)
   {
      if (!got[ch])
         memset(a->ch[ch].coef, 0, sizeof(a->ch[ch].coef));
      raac_filterbank(a, &a->ch[ch], pcm[ch]);
   }
   return RAAC_FRAME;
}

int raac_decode_s16(raac_t *a, const uint8_t *pkt, size_t size,
      int16_t *out)
{
   float    pcm[RAAC_MAX_CH][RAAC_FRAME];
   int      ret = raac_decode_frame(a, pkt, size, pcm);
   int      i;
   unsigned ch;
   if (ret < 0)
      return ret;
   for (i = 0; i < RAAC_FRAME; i++)
      for (ch = 0; ch < a->channels; ch++)
      {
         /* one rounding, clamped in the float domain: casting an
          * out-of-range or non-finite float to int is undefined, and
          * hostile TNS filters can push the synthesis arbitrarily
          * high, so saturate before the cast (NaN pins to zero). */
         float v = pcm[ch][i];
         if (!(v > -1e9f && v < 1e9f))
            v = 0.0f;
         v += (v >= 0.0f) ? 0.5f : -0.5f;
         if (v >  32767.0f) v =  32767.0f;
         if (v < -32768.0f) v = -32768.0f;
         out[i * a->channels + ch] = (int16_t)(int)v;
      }
   return ret;
}

int raac_decode_f32(raac_t *a, const uint8_t *pkt, size_t size,
      float *out)
{
   float    pcm[RAAC_MAX_CH][RAAC_FRAME];
   int      ret = raac_decode_frame(a, pkt, size, pcm);
   int      i;
   unsigned ch;
   if (ret < 0)
      return ret;
   for (i = 0; i < RAAC_FRAME; i++)
      for (ch = 0; ch < a->channels; ch++)
         out[i * a->channels + ch] = pcm[ch][i] * (1.0f / 32768.0f);
   return ret;
}

void raac_reset(raac_t *a)
{
   unsigned ch;
   if (!a)
      return;
   for (ch = 0; ch < RAAC_MAX_CH; ch++)
   {
      memset(a->ch[ch].overlap, 0, sizeof(a->ch[ch].overlap));
      memset(a->ch[ch].coef,    0, sizeof(a->ch[ch].coef));
      a->ch[ch].prev_window_shape = 0;
   }
   /* reseed the PNS generator so a rewound stream decodes exactly as a
    * fresh one */
   a->noise_state = 0x1f2e3d4cu;
}

void raac_close(raac_t *a)
{
   free(a);
}
