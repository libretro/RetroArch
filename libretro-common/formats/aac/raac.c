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
 * computed at open from their defining formulas.
 *
 * What it implements: AAC-LC (audio object type 2) raw access units,
 * channel configurations 1 through 7 and configuration 0 with an
 * embedded PCE (up to eight output channels, interleaved in element
 * transmission order), explicit as well as indexed sampling
 * frequencies, M/S and intensity stereo, PNS, TNS, coupling channel
 * elements (dependently and independently switched, up to four
 * concurrent tags), in-stream PCE and DSE elements (skipped), and
 * both the s16 and f32 output paths from a shared float synthesis
 * pipeline.
 *
 * What it does not implement: other object types (Main, SSR, LTP,
 * HE-AAC's SBR/PS - configurations other than plain LC are refused
 * at open, as is the 960-sample frame variant), downmixing,
 * ADTS/LATM/LOAS framing (the caller supplies the
 * AudioSpecificConfig and raw access units, as rmp4 does), and
 * encoding. */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <formats/raac.h>
#ifdef RAAC_SBR_TRACE
#include <stdio.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RAAC_FRAME     1024
#define RAAC_MAX_CH    8
#define RAAC_MAX_SFB   51
#define RAAC_MAX_WIN   8
#define RAAC_MAX_CCE   4   /* concurrently active coupling tags      */
#define RAAC_CCE_TARG  8   /* num_coupled_elements is 3 bits, plus 1 */
#define RAAC_CCE_GAIN  16  /* gain lists: 8 CPE targets, two apiece  */

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
/* ===== 960-sample frame band tables (14496-3 tables 4.140+) =====
 * The 48 kHz long table also serves 32 kHz and the 96 kHz short table
 * also serves 64 kHz: the layouts coincide, as in the reference
 * tables, so the duplicates are folded. */
static const uint16_t raac_swb_offset_960_96[41] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72,
   80, 88, 96, 108, 120, 132, 144, 156, 172, 188, 212, 240, 276, 320,
   384, 448, 512, 576, 640, 704, 768, 832, 896, 960
};
static const uint16_t raac_swb_offset_960_64[47] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72,
   80, 88, 100, 112, 124, 140, 156, 172, 192, 216, 240, 268, 304,
   344, 384, 424, 464, 504, 544, 584, 624, 664, 704, 744, 784, 824,
   864, 904, 944, 960
};
static const uint16_t raac_swb_offset_960_48[50] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 88,
   96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320,
   352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736,
   768, 800, 832, 864, 896, 928, 960
};
static const uint16_t raac_swb_offset_960_24[47] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68, 76, 84,
   92, 100, 108, 116, 124, 136, 148, 160, 172, 188, 204, 220, 240,
   260, 284, 308, 336, 364, 396, 432, 468, 508, 552, 600, 652, 704,
   768, 832, 896, 960
};
static const uint16_t raac_swb_offset_960_16[43] = {
   0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 100, 112, 124, 136,
   148, 160, 172, 184, 196, 212, 228, 244, 260, 280, 300, 320, 344,
   368, 396, 424, 456, 492, 532, 572, 616, 664, 716, 772, 832, 896,
   960
};
static const uint16_t raac_swb_offset_960_8[41] = {
   0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 172,
   188, 204, 220, 236, 252, 268, 288, 308, 328, 348, 372, 396, 420,
   448, 476, 508, 544, 580, 620, 664, 712, 764, 820, 880, 944, 960
};
static const uint16_t *const raac_swb_offset_960[13] = {
   raac_swb_offset_960_96, raac_swb_offset_960_96, raac_swb_offset_960_64,
   raac_swb_offset_960_48, raac_swb_offset_960_48, raac_swb_offset_960_48,
   raac_swb_offset_960_24, raac_swb_offset_960_24, raac_swb_offset_960_16,
   raac_swb_offset_960_16, raac_swb_offset_960_16, raac_swb_offset_960_8,
   raac_swb_offset_960_8
};
static const uint16_t raac_swb_offset_120_96[13] = {
   0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 120
};
static const uint16_t raac_swb_offset_120_48[15] = {
   0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80, 96, 112, 120
};
static const uint16_t raac_swb_offset_120_24[16] = {
   0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64, 76, 92, 108, 120
};
static const uint16_t raac_swb_offset_120_16[16] = {
   0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60, 72, 88, 108, 120
};
static const uint16_t raac_swb_offset_120_8[16] = {
   0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 120
};
static const uint16_t *const raac_swb_offset_120[13] = {
   raac_swb_offset_120_96, raac_swb_offset_120_96, raac_swb_offset_120_96,
   raac_swb_offset_120_48, raac_swb_offset_120_48, raac_swb_offset_120_48,
   raac_swb_offset_120_24, raac_swb_offset_120_24, raac_swb_offset_120_16,
   raac_swb_offset_120_16, raac_swb_offset_120_16, raac_swb_offset_120_8,
   raac_swb_offset_120_8
};
static const uint8_t raac_num_swb_960[13] = {
   40, 40, 46, 49, 49, 49, 46, 46, 42, 42, 42, 40, 40
};
static const uint8_t raac_num_swb_120[13] = {
   12, 12, 12, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15
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
/* TNS band limits: the 1024/128 tables also serve 960-sample frames,
 * matching the dominant deployed decoder rather than the separate
 * (near-identical) 960 columns of the reference tables. */
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
   b->buf  = buf;
   b->size = size;
   b->pos  = 0;
   b->err  = 0;
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
      peek = (peek << 1) | raac_getbits(b, 1);
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
         acc = (acc << 1) | raac_getbits(b, 1);
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
      peek = (peek << 1) | raac_getbits(b, 1);
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
      acc = (acc << 1) | raac_getbits(b, 1);
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

/* one coupling channel, bound to an element instance tag. The
 * spectral state persists across frames so time-domain (independently
 * switched) coupling keeps filterbank overlap continuity. */
typedef struct
{
   int      in_use;                    /* slot bound to a tag         */
   int      tag;
   int      present;                   /* parsed in the current frame */
   int      point;                     /* 0 before target TNS,
                                        * 1 between TNS and IMDCT,
                                        * 3 after IMDCT, time domain  */
   int      ntarg;
   uint8_t  targ_is_cpe[RAAC_CCE_TARG];
   uint8_t  targ_tag[RAAC_CCE_TARG];
   uint8_t  targ_sel[RAAC_CCE_TARG];   /* SCE fixed 2; CPE 0..3       */
   float    gain[RAAC_CCE_GAIN][RAAC_MAX_WIN * RAAC_MAX_SFB];
   raac_ch  ch;                        /* the coupling channel        */
   float    time[RAAC_FRAME];          /* CC synthesis for time-
                                        * domain coupling             */
} raac_cce;

/* one decoded channel element of the current access unit, for
 * resolving coupling targets (type, tag) to output channels */
typedef struct
{
   uint8_t  kind;                      /* 0 SCE, 1 CPE, 3 LFE         */
   uint8_t  tag;
   uint8_t  ch;                        /* first claimed channel       */
} raac_elem;

#define RAAC_MAX_SBR 4  /* concurrently tracked SBR-bearing elements */

/* per-channel SBR frame data: envelope grid, delta flags, inverse
 * filtering modes and the quantized envelope/noise scalefactors,
 * with row 0 carrying the previous frame's trailing values */
typedef struct
{
   unsigned bs_num_env;
   unsigned bs_num_noise;
   uint8_t  bs_freq_res[7];
   uint8_t  bs_frame_class;
   uint8_t  bs_amp_res;
   uint8_t  bs_df_env[5];
   uint8_t  bs_df_noise[2];
   uint8_t  bs_invf_mode[2][5];
   uint8_t  bs_add_harmonic_flag;
   uint8_t  bs_add_harmonic[48];
   int      t_env[8];
   int      t_env_num_env_old;
   int      t_q[3];
   int      e_a[2];
   int      env_facs_q[6][48];
   int      noise_facs_q[3][5];
} raac_sbr_ch;

/* one SBR decoder bound to a channel element (type, tag). Stage one
 * of the SBR effort: the complete bitstream layer and frequency band
 * tables; synthesis lands with the QMF/HF stages. */
typedef struct
{
   int      in_use;
   int      is_cpe;
   int      tag;
   int      start;             /* a header has arrived               */
   int      reset;
   unsigned sample_rate;       /* the SBR rate: twice the core       */
   /* sbr_header */
   uint8_t  bs_amp_res_hdr;
   uint8_t  bs_start_freq, bs_stop_freq, bs_xover_band;
   uint8_t  bs_freq_scale, bs_alter_scale, bs_noise_bands;
   uint8_t  bs_limiter_bands, bs_limiter_gains;
   uint8_t  bs_interpol_freq, bs_smoothing_mode;
   uint8_t  bs_coupling;
   /* frequency band tables (14496-3 4.6.18.3) */
   int      k0, k1, k2;
   int      kx[2], m[2];       /* crossover and band count, [0] holds
                                * the previous frame's values        */
   int      n_master;
   int      n[2];              /* N_low, N_high                      */
   int      n_q, n_lim;
   uint16_t f_master[49];
   uint16_t f_tablehigh[49];
   uint16_t f_tablelow[25];
   uint16_t f_tablenoise[6];
   uint16_t f_tablelim[30];
   int      num_patches;
   uint8_t  patch_start_subband[6];
   uint8_t  patch_num_subbands[6];
   raac_sbr_ch d[2];
} raac_sbr;

struct raac
{
   unsigned sample_rate;
   unsigned channels;
   int      sfi;                  /* sampling frequency index            */
   raac_ch  ch[RAAC_MAX_CH];
   raac_cce cce[RAAC_MAX_CCE];
   raac_sbr sbr[RAAC_MAX_SBR];
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
   unsigned frame_len;                          /* 1024, or 960 when the
                                                * short frame length is
                                                * signalled at open     */
   float    mr_re[1280], mr_im[1280];          /* mixed-radix FFT output
                                                * and recursion scratch  */
   float    w480_re[480], w480_im[480];        /* roots of unity for the
                                                * 960-mode transforms    */
   float    w60_re[60], w60_im[60];
   float    pcm[RAAC_MAX_CH][RAAC_FRAME];      /* per-frame synthesis
                                                * scratch: too large for
                                                * the stack at 8 ch     */
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

/* Mixed-radix complex FFT for the 960-frame transforms, whose cores
 * are 480 = 2^5*3*5 and 60 = 2^2*3*5 points. Recursive decimation in
 * time over radices 5/3/2, out-of-place per level into caller scratch,
 * with a shared full-size root-of-unity table (every sub-length
 * divides the root length, so twiddle indices stay integral). The
 * radix-2 transform above keeps the 1024-frame path unchanged. */
static void raac_fft_mr(const float *in_re, const float *in_im,
      float *out_re, float *out_im, int n, int stride,
      const float *w_re, const float *w_im, int root,
      float *sc_re, float *sc_im)
{
   int r, m, q, j, k;
   if (n == 1)
   {
      out_re[0] = in_re[0];
      out_im[0] = in_im[0];
      return;
   }
   r = (n % 5 == 0) ? 5 : ((n % 3 == 0) ? 3 : 2);
   m = n / r;
   for (q = 0; q < r; q++)
      raac_fft_mr(in_re + q * stride, in_im + q * stride,
            sc_re + q * m, sc_im + q * m, m, stride * r,
            w_re, w_im, root, sc_re + n, sc_im + n);
   for (j = 0; j < r; j++)
      for (k = 0; k < m; k++)
      {
         int   idx  = j * m + k;
         int   step = root / n;
         float ar = 0.0f, ai = 0.0f;
         for (q = 0; q < r; q++)
         {
            int   tw = (idx * q * step) % root;
            float wr = w_re[tw], wi = w_im[tw];
            float xr = sc_re[q * m + k], xi = sc_im[q * m + k];
            ar += xr * wr - xi * wi;
            ai += xr * wi + xi * wr;
         }
         out_re[idx] = ar;
         out_im[idx] = ai;
      }
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
   /* the twiddle arrays hold this instance's frame-length variant:
    * 512/64-point tables in 1024 mode, 480/60-point in 960 mode */
   int    lng = (n >= 1920);
   const float *pr  = lng ? a->tw512_re : a->tw64_re;
   const float *pi_ = lng ? a->tw512_im : a->tw64_im;
   const float *qr  = lng ? a->tw512_re + n4 : a->tw64_re + n4;
   const float *qi  = lng ? a->tw512_im + n4 : a->tw64_im + n4;
   float  v[1024];
   int    k;

   for (k = 0; k < n4; k++)
   {
      float xr = x[2 * k];
      float xi = x[n2 - 1 - 2 * k];
      fre[k] = xr * pr[k] - xi * pi_[k];
      fim[k] = xr * pi_[k] + xi * pr[k];
   }
   if ((n4 & (n4 - 1)) == 0)
      raac_fft(fre, fim, n4);
   else
   {
      const float *wr = (n4 == 480) ? a->w480_re : a->w60_re;
      const float *wi = (n4 == 480) ? a->w480_im : a->w60_im;
      raac_fft_mr(fre, fim, a->mr_re, a->mr_im, n4, 1, wr, wi, n4,
            a->mr_re + n4, a->mr_im + n4);
      memcpy(fre, a->mr_re, sizeof(float) * (size_t)n4);
      memcpy(fim, a->mr_im, sizeof(float) * (size_t)n4);
   }
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
   if (a->frame_len == 960)
      return short_win ? raac_swb_offset_120[a->sfi]
                       : raac_swb_offset_960[a->sfi];
   return short_win ? raac_swb_offset_128[a->sfi]
                    : raac_swb_offset_1024[a->sfi];
}

static int raac_num_swb(const raac_t *a, int short_win)
{
   if (a->frame_len == 960)
      return short_win ? raac_num_swb_120[a->sfi]
                       : raac_num_swb_960[a->sfi];
   return short_win ? raac_num_swb_128[a->sfi]
                    : raac_num_swb_1024[a->sfi];
}

static int raac_ics_info(raac_t *a, raac_bits *b, raac_ch *c)
{
   int short_win;
   raac_getbits(b, 1); /* ics_reserved              */
   c->window_sequence = (int)raac_getbits(b, 2);
   c->window_shape    = (int)raac_getbits(b, 1);
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
      if (raac_getbits(b, 1)) /* predictor_data_present    */
         return -1; /* Main/LTP tools: out of scope */
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
         coef_res = (int)raac_getbits(b, 1);
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
            int direction = (int)raac_getbits(b, 1);
            int compress  = (int)raac_getbits(b, 1);
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
                        if (q[j] && raac_getbits(b, 1))
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
                        if (q[j] && raac_getbits(b, 1))
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
                           while (raac_getbits(b, 1))
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
   /* frame geometry: everything below derives from the frame length
    * (1024: 2048/256-point transforms, flat run 448, short hop 128;
    * 960: 1920/240, flat run 420, hop 120) */
   int   L    = (int)a->frame_len;
   int   Ls   = L >> 3;
   int   nl   = L * 2;
   int   ns   = Ls * 2;
   int   flat = (L - Ls) / 2;
   float buf[2048];
   float win_out[2048];
   int   i;

   if (c->window_sequence != 2)
   {
      raac_imdct(a, c->coef, buf, nl);
      /* first half: window with the previous frame's trailing shape */
      switch (c->window_sequence)
      {
         case 0:  /* only long  */
         case 1:  /* long start */
            for (i = 0; i < L; i++)
               win_out[i] = buf[i] * long_prev[i];
            break;
         default: /* long stop: flat head after a short run           */
            for (i = 0; i < flat; i++)
               win_out[i] = 0.0f;
            for (i = 0; i < Ls; i++)
               win_out[flat + i] = buf[flat + i] * shrt_prev[i];
            for (i = flat + Ls; i < L; i++)
               win_out[i] = buf[i];
            break;
      }
      /* second half: this frame's trailing shape */
      switch (c->window_sequence)
      {
         case 0:  /* long tail */
         case 3:
            for (i = 0; i < L; i++)
               win_out[L + i] = buf[L + i] * long_cur[L + i];
            break;
         default: /* long start: flat, then a short tail              */
            for (i = 0; i < flat; i++)
               win_out[L + i] = buf[L + i];
            for (i = 0; i < Ls; i++)
               win_out[L + flat + i] = buf[L + flat + i] * shrt_cur[Ls + i];
            for (i = L + flat + Ls; i < nl; i++)
               win_out[i] = 0.0f;
            break;
      }
   }
   else
   {
      /* eight short windows, hop Ls, starting at the flat offset. The
       * spectral hop stays 128 for both frame lengths (960-frame short
       * windows carry 120 coefficients in 128-wide slots). */
      float acc[2048];
      int   w;
      memset(acc, 0, sizeof(acc));
      for (w = 0; w < 8; w++)
      {
         float sbuf[256];
         raac_imdct(a, c->coef + w * 128, sbuf, ns);
         for (i = 0; i < ns; i++)
         {
            const float *head = (w == 0) ? shrt_prev : shrt_cur;
            float win = (i < Ls) ? head[i] : shrt_cur[i];
            acc[flat + w * Ls + i] += sbuf[i] * win;
         }
      }
      memcpy(win_out, acc, sizeof(acc));
   }

   for (i = 0; i < L; i++)
      out[i] = win_out[i] + c->overlap[i];
   memcpy(c->overlap, win_out + L, sizeof(float) * (size_t)L);
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
   if (raac_getbits(b, 1)) /* pulse_data_present */
   {
      if (c->window_sequence == 2)
         return -1;                  /* pulse is long-window only     */
      pulse.pulse_present = 1;
      if (raac_pulse_data(b, &pulse) < 0)
         return -1;
   }
   c->tns_present = (int)raac_getbits(b, 1);
   if (c->tns_present)
      if (raac_tns_data(a, b, c) < 0)
         return -1;
   if (raac_getbits(b, 1)) /* gain_control_data_present     */
      return -1; /* SSR tool: out of scope */
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

/* Decode one individual_channel_stream and reconstruct its spectrum
 * (the caller consumes the element_instance_tag). TNS synthesis is
 * deferred to the frame tail so spectral coupling can be applied on
 * either side of it. Also decodes the coupling channel inside a CCE,
 * which shares this exact syntax. */
static int raac_decode_sce(raac_t *a, raac_bits *b, raac_ch *c)
{
   if (raac_decode_ics(a, b, c, 0) < 0)
      return -1;
   raac_pns(a, c);
   return 0;
}

static int raac_decode_cpe(raac_t *a, raac_bits *b, raac_ch *l, raac_ch *r)
{
   uint8_t  ms_used[RAAC_MAX_WIN][RAAC_MAX_SFB];
   int      ms_mask = 0;
   int      common;
   common = (int)raac_getbits(b, 1);
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
               ms_used[g][k] = (uint8_t)raac_getbits(b, 1);
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
   return 0;
}

/* gain step per coded gain word: 2^(1/8), 2^(1/4), 2^(1/2), 2 */
static const float raac_cce_scale[4] =
{
   1.09050773266525765921f, 1.18920711500272106672f,
   1.41421356237309504880f, 2.0f
};

/* coupling_channel_element (14496-3 4.4.2.2, table 4.8). Parses the
 * target list, the coupling channel's own individual_channel_stream,
 * and the gain element lists. Gain semantics track the reference
 * behaviour of deployed decoders: the first list is unity and sends
 * no bits; per-band lists DPCM-accumulate in the coded domain, a
 * delta of zero keeps the previous band's cached gain, and with
 * gain_element_sign set the accumulated word carries the sign in its
 * LSB with the magnitude above it. */
static int raac_decode_cce(raac_t *a, raac_bits *b, raac_cce *cc)
{
   int   num_gain = 0;
   int   c, g, sfb;
   int   sign, ind_sw;
   float scale;

   ind_sw    = (int)raac_getbits(b, 1);
   cc->point = 2 * ind_sw;
   cc->ntarg = (int)raac_getbits(b, 3) + 1;
   for (c = 0; c < cc->ntarg; c++)
   {
      num_gain++;
      cc->targ_is_cpe[c] = (uint8_t)raac_getbits(b, 1);
      cc->targ_tag[c]    = (uint8_t)raac_getbits(b, 4);
      if (cc->targ_is_cpe[c])
      {
         cc->targ_sel[c] = (uint8_t)raac_getbits(b, 2);
         if (cc->targ_sel[c] == 3)
            num_gain++;
      }
      else
         cc->targ_sel[c] = 2;
   }
   /* cc_domain; independently switched coupling is always applied in
    * the time domain, folding both flags into one coupling point */
   cc->point += (raac_getbits(b, 1) || ind_sw) ? 1 : 0;

   sign  = (int)raac_getbits(b, 1);
   scale = raac_cce_scale[raac_getbits(b, 2)];

   if (raac_decode_sce(a, b, &cc->ch) < 0)
      return -1;

   for (c = 0; c < num_gain; c++)
   {
      int   idx  = 0;
      int   cge  = 1;
      int   gain = 0;
      float gain_cache = 1.0f;
      if (c)
      {
         cge = (cc->point == 3) ? 1 : (int)raac_getbits(b, 1);
         if (cge)
         {
            gain = raac_huff_decode_sf(b, &a->sfh);
            if (gain < 0)
               return -1;
            gain -= 60;
         }
         gain_cache = (float)pow((double)scale, (double)-gain);
      }
      if (cc->point == 3)
         cc->gain[c][0] = gain_cache;
      else
      {
         for (g = 0; g < cc->ch.num_window_groups; g++)
            for (sfb = 0; sfb < cc->ch.max_sfb; sfb++, idx++)
            {
               if (cc->ch.band_cb[g][sfb] == RAAC_CB_ZERO)
                  continue;
               if (!cge)
               {
                  int t = raac_huff_decode_sf(b, &a->sfh);
                  if (t < 0)
                     return -1;
                  t -= 60;
                  if (t)
                  {
                     int s = 1;
                     t = gain += t;
                     if (sign)
                     {
                        s  -= 2 * (t & 1);
                        t >>= 1;
                     }
                     gain_cache = (float)pow((double)scale,
                           (double)-t) * (float)s;
                  }
               }
               cc->gain[c][idx] = gain_cache;
            }
      }
   }
   return b->err ? -1 : 0;
}

/* Spectral-domain coupling: add one gain element list's scaling of
 * the coupling channel into a target channel, walking the coupling
 * channel's own band layout. The layouts are expected to agree; if a
 * hostile stream disagrees the adds still stay inside the frame. */
static void raac_cce_add_spec(const raac_t *a, raac_cce *cc, raac_ch *t,
      int list)
{
   const raac_ch  *c   = &cc->ch;
   const uint16_t *swb = raac_swb_off(a, c->window_sequence == 2);
   int g, idx = 0, win_base = 0;
   for (g = 0; g < c->num_window_groups; g++)
   {
      int glen = c->group_len[g], sfb;
      for (sfb = 0; sfb < c->max_sfb; sfb++, idx++)
      {
         if (c->band_cb[g][sfb] != RAAC_CB_ZERO)
         {
            float gain = cc->gain[list][idx];
            int w;
            for (w = 0; w < glen; w++)
            {
               int base = win_base + w * 128;
               int i;
               for (i = swb[sfb]; i < swb[sfb + 1]; i++)
                  t->coef[base + i] += gain * c->coef[base + i];
            }
         }
      }
      win_base += glen * ((c->window_sequence == 2) ? 128 : 1024);
   }
}

/* Walk one CCE's target list for a decoded element, keeping the gain
 * list index in step with unmatched targets, and apply the coupling
 * at the requested point to the element's channel(s). ch_select: 2
 * couples the first (or only) channel, 1 the second, 0 both from a
 * shared list, 3 both from separate lists. */
static void raac_cce_couple(raac_t *a, raac_cce *cc, int point,
      int is_cpe, int tag, unsigned ch0, float *pcm0, float *pcm1)
{
   int c, index = 0;
   if (!cc->present || cc->point != point)
      return;
   for (c = 0; c < cc->ntarg; c++)
   {
      if ((int)cc->targ_is_cpe[c] == is_cpe && (int)cc->targ_tag[c] == tag)
      {
         if (cc->targ_sel[c] != 1)
         {
            if (point == 3)
            {
               float gain = cc->gain[index][0];
               int i;
               for (i = 0; i < (int)a->frame_len; i++)
                  pcm0[i] += gain * cc->time[i];
            }
            else
               raac_cce_add_spec(a, cc, &a->ch[ch0], index);
            if (cc->targ_sel[c] != 0)
               index++;
         }
         if (cc->targ_sel[c] != 2)
         {
            if (point == 3)
            {
               float gain = cc->gain[index][0];
               int i;
               for (i = 0; i < (int)a->frame_len; i++)
                  pcm1[i] += gain * cc->time[i];
            }
            else
               raac_cce_add_spec(a, cc, &a->ch[ch0 + 1], index);
            index++;
         }
      }
      else
         index += 1 + (cc->targ_sel[c] == 3);
   }
}

/* ===== SBR huffman codebooks (14496-3 tables 4.A.75+): symbol i
 * decodes to i - lav, with lav per book below ===== */

static const uint8_t raac_t_sbr_env_1_5dB_bits[121] = {
   18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 17, 18,
   16, 17, 18, 17, 16, 16, 16, 16, 15, 14, 14, 13, 13, 12, 11, 10, 9, 8, 7,
   6, 5, 4, 3, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 14, 15, 16, 17, 16,
   19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   19, 19, 19, 19, 19, 19, 19, 19
};

static const uint32_t raac_t_sbr_env_1_5dB_code[121] = {
   0x3ffd6, 0x3ffd7, 0x3ffd8, 0x3ffd9, 0x3ffda, 0x3ffdb, 0x7ffb8, 0x7ffb9,
   0x7ffba, 0x7ffbb, 0x7ffbc, 0x7ffbd, 0x7ffbe, 0x7ffbf, 0x7ffc0, 0x7ffc1,
   0x7ffc2, 0x7ffc3, 0x7ffc4, 0x7ffc5, 0x7ffc6, 0x7ffc7, 0x7ffc8, 0x7ffc9,
   0x7ffca, 0x7ffcb, 0x7ffcc, 0x7ffcd, 0x7ffce, 0x7ffcf, 0x7ffd0, 0x7ffd1,
   0x7ffd2, 0x7ffd3, 0x1ffe6, 0x3ffd4, 0x0fff0, 0x1ffe9, 0x3ffd5, 0x1ffe7,
   0x0fff1, 0x0ffec, 0x0ffed, 0x0ffee, 0x07ff4, 0x03ff9, 0x03ff7, 0x01ffa,
   0x01ff9, 0x00ffb, 0x007fc, 0x003fc, 0x001fd, 0x000fd, 0x0007d, 0x0003d,
   0x0001d, 0x0000d, 0x00005, 0x00001, 0x00000, 0x00004, 0x0000c, 0x0001c,
   0x0003c, 0x0007c, 0x000fc, 0x001fc, 0x003fd, 0x00ffa, 0x01ff8, 0x03ff6,
   0x03ff8, 0x07ff5, 0x0ffef, 0x1ffe8, 0x0fff2, 0x7ffd4, 0x7ffd5, 0x7ffd6,
   0x7ffd7, 0x7ffd8, 0x7ffd9, 0x7ffda, 0x7ffdb, 0x7ffdc, 0x7ffdd, 0x7ffde,
   0x7ffdf, 0x7ffe0, 0x7ffe1, 0x7ffe2, 0x7ffe3, 0x7ffe4, 0x7ffe5, 0x7ffe6,
   0x7ffe7, 0x7ffe8, 0x7ffe9, 0x7ffea, 0x7ffeb, 0x7ffec, 0x7ffed, 0x7ffee,
   0x7ffef, 0x7fff0, 0x7fff1, 0x7fff2, 0x7fff3, 0x7fff4, 0x7fff5, 0x7fff6,
   0x7fff7, 0x7fff8, 0x7fff9, 0x7fffa, 0x7fffb, 0x7fffc, 0x7fffd, 0x7fffe,
   0x7ffff
};

static const uint8_t raac_f_sbr_env_1_5dB_bits[121] = {
   19, 19, 20, 20, 20, 20, 20, 20, 20, 19, 20, 20, 20, 20, 19, 20, 19, 19,
   20, 18, 20, 20, 20, 19, 20, 20, 20, 19, 20, 19, 18, 19, 18, 18, 17, 18,
   17, 17, 17, 16, 16, 16, 15, 15, 14, 13, 13, 12, 12, 11, 10, 9, 9, 8, 7, 6,
   5, 4, 3, 2, 2, 3, 4, 5, 6, 8, 8, 9, 10, 11, 11, 11, 12, 12, 13, 13, 14,
   14, 16, 16, 17, 17, 18, 18, 18, 18, 18, 18, 18, 20, 19, 20, 20, 20, 20,
   20, 20, 19, 20, 20, 20, 20, 19, 20, 18, 20, 20, 19, 19, 20, 20, 20, 20,
   20, 20, 20, 20, 20, 20, 20, 20
};

static const uint32_t raac_f_sbr_env_1_5dB_code[121] = {
   0x7ffe7, 0x7ffe8, 0xfffd2, 0xfffd3, 0xfffd4, 0xfffd5, 0xfffd6, 0xfffd7,
   0xfffd8, 0x7ffda, 0xfffd9, 0xfffda, 0xfffdb, 0xfffdc, 0x7ffdb, 0xfffdd,
   0x7ffdc, 0x7ffdd, 0xfffde, 0x3ffe4, 0xfffdf, 0xfffe0, 0xfffe1, 0x7ffde,
   0xfffe2, 0xfffe3, 0xfffe4, 0x7ffdf, 0xfffe5, 0x7ffe0, 0x3ffe8, 0x7ffe1,
   0x3ffe0, 0x3ffe9, 0x1ffef, 0x3ffe5, 0x1ffec, 0x1ffed, 0x1ffee, 0x0fff4,
   0x0fff3, 0x0fff0, 0x07ff7, 0x07ff6, 0x03ffa, 0x01ffa, 0x01ff9, 0x00ffa,
   0x00ff8, 0x007f9, 0x003fb, 0x001fc, 0x001fa, 0x000fb, 0x0007c, 0x0003c,
   0x0001c, 0x0000c, 0x00005, 0x00001, 0x00000, 0x00004, 0x0000d, 0x0001d,
   0x0003d, 0x000fa, 0x000fc, 0x001fb, 0x003fa, 0x007f8, 0x007fa, 0x007fb,
   0x00ff9, 0x00ffb, 0x01ff8, 0x01ffb, 0x03ff8, 0x03ff9, 0x0fff1, 0x0fff2,
   0x1ffea, 0x1ffeb, 0x3ffe1, 0x3ffe2, 0x3ffea, 0x3ffe3, 0x3ffe6, 0x3ffe7,
   0x3ffeb, 0xfffe6, 0x7ffe2, 0xfffe7, 0xfffe8, 0xfffe9, 0xfffea, 0xfffeb,
   0xfffec, 0x7ffe3, 0xfffed, 0xfffee, 0xfffef, 0xffff0, 0x7ffe4, 0xffff1,
   0x3ffec, 0xffff2, 0xffff3, 0x7ffe5, 0x7ffe6, 0xffff4, 0xffff5, 0xffff6,
   0xffff7, 0xffff8, 0xffff9, 0xffffa, 0xffffb, 0xffffc, 0xffffd, 0xffffe,
   0xfffff
};

static const uint8_t raac_t_sbr_env_bal_1_5dB_bits[49] = {
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
   12, 11, 9, 7, 5, 3, 1, 2, 4, 6, 8, 11, 12, 15, 16, 16, 16, 16, 16, 16, 16,
   17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

static const uint32_t raac_t_sbr_env_bal_1_5dB_code[49] = {
   0x0ffe4, 0x0ffe5, 0x0ffe6, 0x0ffe7, 0x0ffe8, 0x0ffe9, 0x0ffea, 0x0ffeb,
   0x0ffec, 0x0ffed, 0x0ffee, 0x0ffef, 0x0fff0, 0x0fff1, 0x0fff2, 0x0fff3,
   0x0fff4, 0x0ffe2, 0x00ffc, 0x007fc, 0x001fe, 0x0007e, 0x0001e, 0x00006,
   0x00000, 0x00002, 0x0000e, 0x0003e, 0x000fe, 0x007fd, 0x00ffd, 0x07ff0,
   0x0ffe3, 0x0fff5, 0x0fff6, 0x0fff7, 0x0fff8, 0x0fff9, 0x0fffa, 0x1fff6,
   0x1fff7, 0x1fff8, 0x1fff9, 0x1fffa, 0x1fffb, 0x1fffc, 0x1fffd, 0x1fffe,
   0x1ffff
};

static const uint8_t raac_f_sbr_env_bal_1_5dB_bits[49] = {
   18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 16, 17, 14,
   11, 11, 8, 7, 4, 2, 1, 3, 5, 6, 9, 11, 12, 15, 16, 18, 18, 18, 18, 18, 18,
   18, 18, 18, 18, 18, 18, 18, 18, 19, 19
};

static const uint32_t raac_f_sbr_env_bal_1_5dB_code[49] = {
   0x3ffe2, 0x3ffe3, 0x3ffe4, 0x3ffe5, 0x3ffe6, 0x3ffe7, 0x3ffe8, 0x3ffe9,
   0x3ffea, 0x3ffeb, 0x3ffec, 0x3ffed, 0x3ffee, 0x3ffef, 0x3fff0, 0x0fff7,
   0x1fff0, 0x03ffc, 0x007fe, 0x007fc, 0x000fe, 0x0007e, 0x0000e, 0x00002,
   0x00000, 0x00006, 0x0001e, 0x0003e, 0x001fe, 0x007fd, 0x00ffe, 0x07ffa,
   0x0fff6, 0x3fff1, 0x3fff2, 0x3fff3, 0x3fff4, 0x3fff5, 0x3fff6, 0x3fff7,
   0x3fff8, 0x3fff9, 0x3fffa, 0x3fffb, 0x3fffc, 0x3fffd, 0x3fffe, 0x7fffe,
   0x7ffff
};

static const uint8_t raac_t_sbr_env_3_0dB_bits[63] = {
   18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 17,
   16, 16, 16, 14, 14, 14, 13, 12, 11, 8, 6, 4, 2, 1, 3, 5, 7, 9, 11, 13, 14,
   14, 15, 16, 17, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   19, 19, 19, 19, 19, 19
};

static const uint32_t raac_t_sbr_env_3_0dB_code[63] = {
   0x3ffed, 0x3ffee, 0x7ffde, 0x7ffdf, 0x7ffe0, 0x7ffe1, 0x7ffe2, 0x7ffe3,
   0x7ffe4, 0x7ffe5, 0x7ffe6, 0x7ffe7, 0x7ffe8, 0x7ffe9, 0x7ffea, 0x7ffeb,
   0x7ffec, 0x1fff4, 0x0fff7, 0x0fff9, 0x0fff8, 0x03ffb, 0x03ffa, 0x03ff8,
   0x01ffa, 0x00ffc, 0x007fc, 0x000fe, 0x0003e, 0x0000e, 0x00002, 0x00000,
   0x00006, 0x0001e, 0x0007e, 0x001fe, 0x007fd, 0x01ffb, 0x03ff9, 0x03ffc,
   0x07ffa, 0x0fff6, 0x1fff5, 0x3ffec, 0x7ffed, 0x7ffee, 0x7ffef, 0x7fff0,
   0x7fff1, 0x7fff2, 0x7fff3, 0x7fff4, 0x7fff5, 0x7fff6, 0x7fff7, 0x7fff8,
   0x7fff9, 0x7fffa, 0x7fffb, 0x7fffc, 0x7fffd, 0x7fffe, 0x7ffff
};

static const uint8_t raac_f_sbr_env_3_0dB_bits[63] = {
   20, 20, 20, 20, 20, 20, 20, 18, 19, 19, 19, 19, 18, 18, 20, 19, 17, 18,
   17, 16, 16, 15, 14, 12, 11, 10, 9, 8, 6, 4, 2, 1, 3, 5, 8, 9, 10, 11, 12,
   13, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 20, 19, 19, 19, 20, 19, 19,
   20, 20, 20, 20, 20, 20
};

static const uint32_t raac_f_sbr_env_3_0dB_code[63] = {
   0xffff0, 0xffff1, 0xffff2, 0xffff3, 0xffff4, 0xffff5, 0xffff6, 0x3fff3,
   0x7fff5, 0x7ffee, 0x7ffef, 0x7fff6, 0x3fff4, 0x3fff2, 0xffff7, 0x7fff0,
   0x1fff5, 0x3fff0, 0x1fff4, 0x0fff7, 0x0fff6, 0x07ff8, 0x03ffb, 0x00ffd,
   0x007fd, 0x003fd, 0x001fd, 0x000fd, 0x0003e, 0x0000e, 0x00002, 0x00000,
   0x00006, 0x0001e, 0x000fc, 0x001fc, 0x003fc, 0x007fc, 0x00ffc, 0x01ffc,
   0x03ffa, 0x07ff9, 0x07ffa, 0x0fff8, 0x0fff9, 0x1fff6, 0x1fff7, 0x3fff5,
   0x3fff6, 0x3fff1, 0xffff8, 0x7fff1, 0x7fff2, 0x7fff3, 0xffff9, 0x7fff7,
   0x7fff4, 0xffffa, 0xffffb, 0xffffc, 0xffffd, 0xffffe, 0xfffff
};

static const uint8_t raac_t_sbr_env_bal_3_0dB_bits[25] = {
   13, 13, 13, 13, 13, 13, 13, 12, 8, 7, 4, 3, 1, 2, 5, 6, 9, 13, 13, 13, 13,
   13, 13, 14, 14
};

static const uint32_t raac_t_sbr_env_bal_3_0dB_code[25] = {
   0x1ff2, 0x1ff3, 0x1ff4, 0x1ff5, 0x1ff6, 0x1ff7, 0x1ff8, 0x0ff8, 0x00fe,
   0x007e, 0x000e, 0x0006, 0x0000, 0x0002, 0x001e, 0x003e, 0x01fe, 0x1ff9,
   0x1ffa, 0x1ffb, 0x1ffc, 0x1ffd, 0x1ffe, 0x3ffe, 0x3fff
};

static const uint8_t raac_f_sbr_env_bal_3_0dB_bits[25] = {
   13, 13, 13, 13, 13, 14, 14, 11, 8, 7, 4, 2, 1, 3, 5, 6, 9, 12, 13, 14, 14,
   14, 14, 14, 14
};

static const uint32_t raac_f_sbr_env_bal_3_0dB_code[25] = {
   0x1ff7, 0x1ff8, 0x1ff9, 0x1ffa, 0x1ffb, 0x3ff8, 0x3ff9, 0x07fc, 0x00fe,
   0x007e, 0x000e, 0x0002, 0x0000, 0x0006, 0x001e, 0x003e, 0x01fe, 0x0ffa,
   0x1ff6, 0x3ffa, 0x3ffb, 0x3ffc, 0x3ffd, 0x3ffe, 0x3fff
};

static const uint8_t raac_t_sbr_noise_3_0dB_bits[63] = {
   13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
   13, 13, 13, 13, 13, 13, 13, 13, 11, 8, 6, 4, 3, 1, 2, 5, 8, 10, 13, 13,
   13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
   13, 13, 13, 13, 13, 14, 14
};

static const uint32_t raac_t_sbr_noise_3_0dB_code[63] = {
   0x1fce, 0x1fcf, 0x1fd0, 0x1fd1, 0x1fd2, 0x1fd3, 0x1fd4, 0x1fd5, 0x1fd6,
   0x1fd7, 0x1fd8, 0x1fd9, 0x1fda, 0x1fdb, 0x1fdc, 0x1fdd, 0x1fde, 0x1fdf,
   0x1fe0, 0x1fe1, 0x1fe2, 0x1fe3, 0x1fe4, 0x1fe5, 0x1fe6, 0x1fe7, 0x07f2,
   0x00fd, 0x003e, 0x000e, 0x0006, 0x0000, 0x0002, 0x001e, 0x00fc, 0x03f8,
   0x1fcc, 0x1fe8, 0x1fe9, 0x1fea, 0x1feb, 0x1fec, 0x1fcd, 0x1fed, 0x1fee,
   0x1fef, 0x1ff0, 0x1ff1, 0x1ff2, 0x1ff3, 0x1ff4, 0x1ff5, 0x1ff6, 0x1ff7,
   0x1ff8, 0x1ff9, 0x1ffa, 0x1ffb, 0x1ffc, 0x1ffd, 0x1ffe, 0x3ffe, 0x3fff
};

static const uint8_t raac_t_sbr_noise_bal_3_0dB_bits[25] = {
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 2, 1, 3, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};

static const uint32_t raac_t_sbr_noise_bal_3_0dB_code[25] = {
   0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0x1c, 0x02,
   0x00, 0x06, 0x3a, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
   0xff
};

/* largest absolute value per book, in table order: decoded symbol i
 * means value i - lav */
static const int raac_sbr_lav[10] =
{
   60, 60, 24, 24, 31, 31, 12, 12, 31, 12
};

/* the ten books in lav order; codes reach 19 bits so these decode
 * by a length-walking scan rather than the 16-bit fast tables - SBR
 * side data is a couple hundred symbols per frame at most */
typedef struct
{
   const uint32_t *code;
   const uint8_t  *bits;
   int             n;
} raac_sbr_book;

static const raac_sbr_book raac_sbr_books[10] =
{
   { raac_t_sbr_env_1_5dB_code,       raac_t_sbr_env_1_5dB_bits,       121 },
   { raac_f_sbr_env_1_5dB_code,       raac_f_sbr_env_1_5dB_bits,       121 },
   { raac_t_sbr_env_bal_1_5dB_code,   raac_t_sbr_env_bal_1_5dB_bits,    49 },
   { raac_f_sbr_env_bal_1_5dB_code,   raac_f_sbr_env_bal_1_5dB_bits,    49 },
   { raac_t_sbr_env_3_0dB_code,       raac_t_sbr_env_3_0dB_bits,        63 },
   { raac_f_sbr_env_3_0dB_code,       raac_f_sbr_env_3_0dB_bits,        63 },
   { raac_t_sbr_env_bal_3_0dB_code,   raac_t_sbr_env_bal_3_0dB_bits,    25 },
   { raac_f_sbr_env_bal_3_0dB_code,   raac_f_sbr_env_bal_3_0dB_bits,    25 },
   { raac_t_sbr_noise_3_0dB_code,     raac_t_sbr_noise_3_0dB_bits,      63 },
   { raac_t_sbr_noise_bal_3_0dB_code, raac_t_sbr_noise_bal_3_0dB_bits,  25 }
};

static int raac_sbr_huff(raac_bits *b, const raac_sbr_book *h)
{
   uint32_t acc = 0;
   unsigned len = 0;
   while (len < 24 && !b->err)
   {
      int i;
      acc = (acc << 1) | raac_getbits(b, 1);
      len++;
      for (i = 0; i < h->n; i++)
         if (h->bits[i] == len && h->code[i] == acc)
            return i;
   }
   return -1;
}

/* QMF subband offsets of the start-frequency index, by SBR rate
 * class (16000 / 22050 / 24000 / 32000 / 44100-64000 / above) */
static const int8_t raac_sbr_offset[6][16] =
{
   { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2,  3,  4,  5,  6,  7 },
   { -5, -4, -3, -2, -1,  0,  1,  2, 3, 4, 5,  6,  7,  9, 11, 13 },
   { -5, -3, -2, -1,  0,  1,  2,  3, 4, 5, 6,  7,  9, 11, 13, 16 },
   { -6, -4, -2, -1,  0,  1,  2,  3, 4, 5, 6,  7,  9, 11, 13, 16 },
   { -4, -2, -1,  0,  1,  2,  3,  4, 5, 6, 7,  9, 11, 13, 16, 20 },
   { -2, -1,  0,  1,  2,  3,  4,  5, 6, 7, 9, 11, 13, 16, 20, 24 }
};

static void raac_sbr_turnoff(raac_sbr *s)
{
   s->start = 0;
}

static int raac_sbr_cmp_u16(const void *a, const void *b)
{
   return (int)*(const uint16_t *)a - (int)*(const uint16_t *)b;
}

static int raac_sbr_cmp_s16(const void *a, const void *b)
{
   return (int)*(const int16_t *)a - (int)*(const int16_t *)b;
}

/* geometric band split with cumulative rounding, per the reference
 * float implementation of the spec's band helper */
static void raac_sbr_make_bands(int16_t *bands, int start, int stop,
      int num_bands)
{
   int    k, previous, present;
   double base, prod;
   base     = pow((double)stop / start, 1.0 / num_bands);
   prod     = start;
   previous = start;
   for (k = 0; k < num_bands - 1; k++)
   {
      prod    *= base;
      present  = (int)floor(prod + 0.5);
      bands[k] = (int16_t)(present - previous);
      previous = present;
   }
   bands[num_bands - 1] = (int16_t)(stop - previous);
}

static int raac_sbr_in_table(const int16_t *table, int n, int needle)
{
   int i;
   for (i = 0; i < n; i++)
      if (table[i] == needle)
         return 1;
   return 0;
}

/* limiter band table: low-resolution borders merged with patch
 * borders, thinned to the signalled limiter density */
static void raac_sbr_make_f_tablelim(raac_sbr *s)
{
   int k;
   if (s->bs_limiter_bands > 0)
   {
      static const double warped[3] =
      {
         1.32715174233856803909, /* 2^(0.49/1.2) */
         1.18509277094158210129, /* 2^(0.49/2)   */
         1.11987160404675912501  /* 2^(0.49/3)   */
      };
      double    dens = warped[s->bs_limiter_bands - 1];
      int16_t   patch_borders[7];
      uint16_t *in  = s->f_tablelim + 1;
      uint16_t *out = s->f_tablelim;
      patch_borders[0] = (int16_t)s->kx[1];
      for (k = 1; k <= s->num_patches; k++)
         patch_borders[k] = (int16_t)(patch_borders[k - 1] +
               s->patch_num_subbands[k - 1]);
      memcpy(s->f_tablelim, s->f_tablelow,
            (size_t)(s->n[0] + 1) * sizeof(uint16_t));
      if (s->num_patches > 1)
      {
         for (k = 0; k < s->num_patches - 1; k++)
            s->f_tablelim[s->n[0] + 1 + k] = (uint16_t)patch_borders[k + 1];
      }
      qsort(s->f_tablelim, (size_t)(s->num_patches + s->n[0]),
            sizeof(uint16_t), raac_sbr_cmp_u16);
      s->n_lim = s->n[0] + s->num_patches - 1;
      while (out < s->f_tablelim + s->n_lim)
      {
         if ((double)*in >= *out * dens)
            *++out = *in++;
         else if (*in == *out ||
               !raac_sbr_in_table(patch_borders, s->num_patches, *in))
         {
            in++;
            s->n_lim--;
         }
         else if (!raac_sbr_in_table(patch_borders, s->num_patches, *out))
         {
            *out = *in++;
            s->n_lim--;
         }
         else
            *++out = *in++;
      }
   }
   else
   {
      s->f_tablelim[0] = s->f_tablelow[0];
      s->f_tablelim[1] = s->f_tablelow[s->n[0]];
      s->n_lim         = 1;
   }
}

/* master frequency band table (14496-3 4.6.18.3.2) */
static int raac_sbr_make_f_master(raac_sbr *s)
{
   unsigned      start_min, stop_min;
   unsigned      max_qmf_subbands;
   int           k;
   const int8_t *offs;
   unsigned      temp;
   int16_t       stop_dk[13];

   switch (s->sample_rate)
   {
      case 16000: offs = raac_sbr_offset[0]; break;
      case 22050: offs = raac_sbr_offset[1]; break;
      case 24000: offs = raac_sbr_offset[2]; break;
      case 32000: offs = raac_sbr_offset[3]; break;
      case 44100: case 48000: case 64000:
         offs = raac_sbr_offset[4]; break;
      case 88200: case 96000: case 128000: case 176400: case 192000:
         offs = raac_sbr_offset[5]; break;
      default:
         return -1;
   }
   if (s->sample_rate < 32000)
      temp = 3000;
   else if (s->sample_rate < 64000)
      temp = 4000;
   else
      temp = 5000;
   start_min = ((temp << 7) + (s->sample_rate >> 1)) / s->sample_rate;
   stop_min  = ((temp << 8) + (s->sample_rate >> 1)) / s->sample_rate;

   s->k0 = (int)start_min + offs[s->bs_start_freq];

   if (s->bs_stop_freq < 14)
   {
      s->k2 = (int)stop_min;
      raac_sbr_make_bands(stop_dk, (int)stop_min, 64, 13);
      qsort(stop_dk, 13, sizeof(int16_t), raac_sbr_cmp_s16);
      for (k = 0; k < s->bs_stop_freq; k++)
         s->k2 += stop_dk[k];
   }
   else if (s->bs_stop_freq == 14)
      s->k2 = 2 * s->k0;
   else
      s->k2 = 3 * s->k0;
   if (s->k2 > 64)
      s->k2 = 64;

   if (s->sample_rate <= 32000)
      max_qmf_subbands = 48;
   else if (s->sample_rate == 44100)
      max_qmf_subbands = 35;
   else
      max_qmf_subbands = 32;
   if (s->k0 < 0 || (unsigned)(s->k2 - s->k0) > max_qmf_subbands)
      return -1;

   if (!s->bs_freq_scale)
   {
      int dk = s->bs_alter_scale + 1;
      int k2diff;
      s->n_master = ((s->k2 - s->k0 + (dk & 2)) >> dk) << 1;
      if (s->n_master <= 0 || s->n_master > 48 ||
            s->bs_xover_band >= s->n_master)
         return -1;
      for (k = 1; k <= s->n_master; k++)
         s->f_master[k] = (uint16_t)dk;
      k2diff = s->k2 - s->k0 - s->n_master * dk;
      if (k2diff < 0)
      {
         s->f_master[1]--;
         s->f_master[2] = (uint16_t)(s->f_master[2] - (k2diff < -1));
      }
      else if (k2diff)
         s->f_master[s->n_master]++;
      s->f_master[0] = (uint16_t)s->k0;
      for (k = 1; k <= s->n_master; k++)
         s->f_master[k] = (uint16_t)(s->f_master[k] + s->f_master[k - 1]);
   }
   else
   {
      int     half_bands = 7 - s->bs_freq_scale;
      int     two_regions, num_bands_0;
      int     vdk0_max, vdk1_min;
      int16_t vk0[49];
      if (49 * s->k2 > 110 * s->k0)
      {
         two_regions = 1;
         s->k1       = 2 * s->k0;
      }
      else
      {
         two_regions = 0;
         s->k1       = s->k2;
      }
      num_bands_0 = (int)floor(half_bands *
            (log((double)s->k1 / s->k0) / log(2.0)) + 0.5) * 2;
      if (num_bands_0 <= 0 || num_bands_0 > 48)
         return -1;
      vk0[0] = 0;
      raac_sbr_make_bands(vk0 + 1, s->k0, s->k1, num_bands_0);
      qsort(vk0 + 1, (size_t)num_bands_0, sizeof(int16_t),
            raac_sbr_cmp_s16);
      vdk0_max = vk0[num_bands_0];
      vk0[0]   = (int16_t)s->k0;
      for (k = 1; k <= num_bands_0; k++)
      {
         if (vk0[k] <= 0)
            return -1;
         vk0[k] = (int16_t)(vk0[k] + vk0[k - 1]);
      }
      if (two_regions)
      {
         int16_t vk1[49];
         double  invwarp = s->bs_alter_scale ? (1.0 / 1.3) : 1.0;
         int     num_bands_1 = (int)floor(half_bands * invwarp *
               (log((double)s->k2 / s->k1) / log(2.0)) + 0.5) * 2;
         if (num_bands_1 < 0 || num_bands_0 + num_bands_1 > 48)
            return -1;
         raac_sbr_make_bands(vk1 + 1, s->k1, s->k2, num_bands_1);
         vdk1_min = vk1[1];
         for (k = 2; k <= num_bands_1; k++)
            if (vk1[k] < vdk1_min)
               vdk1_min = vk1[k];
         if (vdk1_min < vdk0_max)
         {
            int change;
            qsort(vk1 + 1, (size_t)num_bands_1, sizeof(int16_t),
                  raac_sbr_cmp_s16);
            change = vdk0_max - vk1[1];
            if (change > (vk1[num_bands_1] - vk1[1]) >> 1)
               change = (vk1[num_bands_1] - vk1[1]) >> 1;
            vk1[1]           = (int16_t)(vk1[1] + change);
            vk1[num_bands_1] = (int16_t)(vk1[num_bands_1] - change);
         }
         qsort(vk1 + 1, (size_t)num_bands_1, sizeof(int16_t),
               raac_sbr_cmp_s16);
         vk1[0] = (int16_t)s->k1;
         for (k = 1; k <= num_bands_1; k++)
         {
            if (vk1[k] <= 0)
               return -1;
            vk1[k] = (int16_t)(vk1[k] + vk1[k - 1]);
         }
         s->n_master = num_bands_0 + num_bands_1;
         if (s->bs_xover_band >= s->n_master)
            return -1;
         for (k = 0; k <= num_bands_0; k++)
            s->f_master[k] = (uint16_t)vk0[k];
         for (k = 1; k <= num_bands_1; k++)
            s->f_master[num_bands_0 + k] = (uint16_t)vk1[k];
      }
      else
      {
         s->n_master = num_bands_0;
         if (s->bs_xover_band >= s->n_master)
            return -1;
         for (k = 0; k <= num_bands_0; k++)
            s->f_master[k] = (uint16_t)vk0[k];
      }
   }
   return 0;
}

/* patch construction (14496-3 4.6.18.6.3) */
static int raac_sbr_calc_patches(raac_sbr *s)
{
   int i, k, sb = 0;
   int msb     = s->k0;
   int usb     = s->kx[1];
   int goal_sb = (int)(((1000u << 11) + (s->sample_rate >> 1)) /
         s->sample_rate);

   s->num_patches = 0;
   if (goal_sb < s->kx[1] + s->m[1])
   {
      for (k = 0; s->f_master[k] < goal_sb; k++) ;
   }
   else
      k = s->n_master;

   do
   {
      int odd = 0;
      for (i = k; i == k || sb > (s->k0 - 1 + msb - odd); i--)
      {
         sb  = s->f_master[i];
         odd = (sb + s->k0) & 1;
      }
      if (s->num_patches > 5)
         return -1;
      s->patch_num_subbands[s->num_patches] =
            (uint8_t)((sb - usb) > 0 ? (sb - usb) : 0);
      s->patch_start_subband[s->num_patches] =
            (uint8_t)(s->k0 - odd - s->patch_num_subbands[s->num_patches]);
      if (s->patch_num_subbands[s->num_patches] > 0)
      {
         usb = sb;
         msb = sb;
         s->num_patches++;
      }
      else
         msb = s->kx[1];
      if (s->f_master[k] - sb < 3)
         k = s->n_master;
   } while (sb != s->kx[1] + s->m[1]);

   if (s->num_patches > 1 &&
         s->patch_num_subbands[s->num_patches - 1] < 3)
      s->num_patches--;
   return 0;
}

/* derived tables: high/low resolution, noise floor and limiter bands
 * (14496-3 4.6.18.3.2.2) */
static int raac_sbr_make_f_derived(raac_sbr *s)
{
   int k, temp;
   s->n[1] = s->n_master - s->bs_xover_band;
   s->n[0] = (s->n[1] + 1) >> 1;
   memcpy(s->f_tablehigh, &s->f_master[s->bs_xover_band],
         (size_t)(s->n[1] + 1) * sizeof(uint16_t));
   s->m[1]  = s->f_tablehigh[s->n[1]] - s->f_tablehigh[0];
   s->kx[1] = s->f_tablehigh[0];
   if (s->kx[1] + s->m[1] > 64 || s->kx[1] > 32)
      return -1;
   s->f_tablelow[0] = s->f_tablehigh[0];
   temp = s->n[1] & 1;
   for (k = 1; k <= s->n[0]; k++)
      s->f_tablelow[k] = s->f_tablehigh[2 * k - temp];
   temp = (int)floor(s->bs_noise_bands *
         (log((double)s->k2 / s->kx[1]) / log(2.0)) + 0.5);
   s->n_q = temp < 1 ? 1 : temp;
   if (s->n_q > 5)
      return -1;
   s->f_tablenoise[0] = s->f_tablelow[0];
   temp = 0;
   for (k = 1; k <= s->n_q; k++)
   {
      temp += (s->n[0] - temp) / (s->n_q + 1 - k);
      s->f_tablenoise[k] = s->f_tablelow[temp];
   }
   if (raac_sbr_calc_patches(s) < 0)
      return -1;
   raac_sbr_make_f_tablelim(s);
   return 0;
}

/* ceil(log2(x + 1)) for the small relative-border counts */
static const uint8_t raac_sbr_ceil_log2[6] = { 0, 1, 2, 2, 3, 3 };

/* sbr_header (14496-3 table 4.56). Sets reset when the spectrum
 * parameters changed and rebuilds the limiter table when only the
 * limiter density did. */
static void raac_sbr_read_header(raac_sbr *s, raac_bits *b)
{
   uint8_t extra_1, extra_2;
   uint8_t o_start = s->bs_start_freq, o_stop = s->bs_stop_freq;
   uint8_t o_xover = s->bs_xover_band, o_scale = s->bs_freq_scale;
   uint8_t o_alter = s->bs_alter_scale, o_noise = s->bs_noise_bands;
   uint8_t o_lim   = s->bs_limiter_bands;

   s->start = 1;
   s->bs_amp_res_hdr = (uint8_t)raac_getbits(b, 1);
   s->bs_start_freq  = (uint8_t)raac_getbits(b, 4);
   s->bs_stop_freq   = (uint8_t)raac_getbits(b, 4);
   s->bs_xover_band  = (uint8_t)raac_getbits(b, 3);
   raac_getbits(b, 2);          /* bs_reserved                       */
   extra_1 = (uint8_t)raac_getbits(b, 1);
   extra_2 = (uint8_t)raac_getbits(b, 1);
   if (extra_1)
   {
      s->bs_freq_scale  = (uint8_t)raac_getbits(b, 2);
      s->bs_alter_scale = (uint8_t)raac_getbits(b, 1);
      s->bs_noise_bands = (uint8_t)raac_getbits(b, 2);
   }
   else
   {
      s->bs_freq_scale  = 2;
      s->bs_alter_scale = 1;
      s->bs_noise_bands = 2;
   }
   if (o_start != s->bs_start_freq || o_stop != s->bs_stop_freq ||
         o_xover != s->bs_xover_band || o_scale != s->bs_freq_scale ||
         o_alter != s->bs_alter_scale || o_noise != s->bs_noise_bands)
      s->reset = 1;
   if (extra_2)
   {
      s->bs_limiter_bands  = (uint8_t)raac_getbits(b, 2);
      s->bs_limiter_gains  = (uint8_t)raac_getbits(b, 2);
      s->bs_interpol_freq  = (uint8_t)raac_getbits(b, 1);
      s->bs_smoothing_mode = (uint8_t)raac_getbits(b, 1);
   }
   else
   {
      s->bs_limiter_bands  = 2;
      s->bs_limiter_gains  = 2;
      s->bs_interpol_freq  = 1;
      s->bs_smoothing_mode = 1;
   }
   if (s->bs_limiter_bands != o_lim && !s->reset)
      raac_sbr_make_f_tablelim(s);
}

/* sbr_grid (14496-3 table 4.62): envelope and noise floor time
 * borders in the four frame classes, and the pointer-derived
 * transient envelope index */
static int raac_sbr_read_grid(raac_sbr *s, raac_bits *b, raac_sbr_ch *c)
{
   int      i;
   int      bs_pointer     = 0;
   int      abs_bord_trail = 16;      /* numTimeSlots at 1024 frames */
   int      num_rel_lead, num_rel_trail;
   unsigned bs_num_env_old = c->bs_num_env;
   int      frame_class, num_env;

   c->bs_freq_res[0]    = c->bs_freq_res[c->bs_num_env];
   c->bs_amp_res        = s->bs_amp_res_hdr;
   c->t_env_num_env_old = c->t_env[c->bs_num_env];

   frame_class = (int)raac_getbits(b, 2);
   switch (frame_class)
   {
      case 0:  /* FIXFIX */
         num_env = 1 << raac_getbits(b, 2);
         if (num_env > 4)
            return -1;
         c->bs_num_env = (unsigned)num_env;
         num_rel_lead  = num_env - 1;
         if (num_env == 1)
            c->bs_amp_res = 0;
         c->t_env[0]       = 0;
         c->t_env[num_env] = abs_bord_trail;
         abs_bord_trail    = (abs_bord_trail + (num_env >> 1)) / num_env;
         for (i = 0; i < num_rel_lead; i++)
            c->t_env[i + 1] = c->t_env[i] + abs_bord_trail;
         c->bs_freq_res[1] = (uint8_t)raac_getbits(b, 1);
         for (i = 1; i < num_env; i++)
            c->bs_freq_res[i + 1] = c->bs_freq_res[1];
         break;
      case 1:  /* FIXVAR */
         abs_bord_trail   += (int)raac_getbits(b, 2);
         num_rel_trail     = (int)raac_getbits(b, 2);
         c->bs_num_env     = (unsigned)(num_rel_trail + 1);
         c->t_env[0]       = 0;
         c->t_env[c->bs_num_env] = abs_bord_trail;
         for (i = 0; i < num_rel_trail; i++)
            c->t_env[c->bs_num_env - 1 - i] =
                  c->t_env[c->bs_num_env - i] -
                  2 * (int)raac_getbits(b, 2) - 2;
         bs_pointer = (int)raac_getbits(b,
               raac_sbr_ceil_log2[c->bs_num_env]);
         for (i = 0; i < (int)c->bs_num_env; i++)
            c->bs_freq_res[c->bs_num_env - i] =
                  (uint8_t)raac_getbits(b, 1);
         break;
      case 2:  /* VARFIX */
         c->t_env[0]   = (int)raac_getbits(b, 2);
         num_rel_lead  = (int)raac_getbits(b, 2);
         c->bs_num_env = (unsigned)(num_rel_lead + 1);
         c->t_env[c->bs_num_env] = abs_bord_trail;
         for (i = 0; i < num_rel_lead; i++)
            c->t_env[i + 1] = c->t_env[i] +
                  2 * (int)raac_getbits(b, 2) + 2;
         bs_pointer = (int)raac_getbits(b,
               raac_sbr_ceil_log2[c->bs_num_env]);
         for (i = 0; i < (int)c->bs_num_env; i++)
            c->bs_freq_res[i + 1] = (uint8_t)raac_getbits(b, 1);
         break;
      default: /* VARVAR */
         c->t_env[0]     = (int)raac_getbits(b, 2);
         abs_bord_trail += (int)raac_getbits(b, 2);
         num_rel_lead    = (int)raac_getbits(b, 2);
         num_rel_trail   = (int)raac_getbits(b, 2);
         num_env         = num_rel_lead + num_rel_trail + 1;
         if (num_env > 5)
            return -1;
         c->bs_num_env = (unsigned)num_env;
         c->t_env[num_env] = abs_bord_trail;
         for (i = 0; i < num_rel_lead; i++)
            c->t_env[i + 1] = c->t_env[i] +
                  2 * (int)raac_getbits(b, 2) + 2;
         for (i = 0; i < num_rel_trail; i++)
            c->t_env[num_env - 1 - i] = c->t_env[num_env - i] -
                  2 * (int)raac_getbits(b, 2) - 2;
         bs_pointer = (int)raac_getbits(b,
               raac_sbr_ceil_log2[num_env]);
         for (i = 0; i < num_env; i++)
            c->bs_freq_res[i + 1] = (uint8_t)raac_getbits(b, 1);
         break;
   }
   c->bs_frame_class = (uint8_t)frame_class;

   if (bs_pointer > (int)c->bs_num_env + 1)
      return -1;
   for (i = 1; i <= (int)c->bs_num_env; i++)
      if (c->t_env[i - 1] >= c->t_env[i])
         return -1;

   c->bs_num_noise = (c->bs_num_env > 1) + 1;
   c->t_q[0] = c->t_env[0];
   c->t_q[c->bs_num_noise] = c->t_env[c->bs_num_env];
   if (c->bs_num_noise > 1)
   {
      int idx;
      if (frame_class == 0)
         idx = (int)(c->bs_num_env >> 1);
      else if (frame_class & 1)          /* FIXVAR or VARVAR         */
      {
         idx = bs_pointer - 1;
         if (idx < 1)
            idx = 1;
         idx = (int)c->bs_num_env - idx;
      }
      else                               /* VARFIX                   */
      {
         if (!bs_pointer)
            idx = 1;
         else if (bs_pointer == 1)
            idx = (int)c->bs_num_env - 1;
         else
            idx = bs_pointer - 1;
      }
      c->t_q[1] = c->t_env[idx];
   }

   c->e_a[0] = -(c->e_a[1] != (int)bs_num_env_old);
   c->e_a[1] = -1;
   if ((frame_class & 1) && bs_pointer)
      c->e_a[1] = (int)c->bs_num_env + 1 - bs_pointer;
   else if (frame_class == 2 && bs_pointer > 1)
      c->e_a[1] = bs_pointer - 1;
   return 0;
}

static void raac_sbr_copy_grid(raac_sbr_ch *dst, const raac_sbr_ch *src)
{
   uint8_t freq_res_prev = dst->bs_freq_res[dst->bs_num_env];
   int     t_env_old     = dst->t_env[dst->bs_num_env];
   int     e_a1          = dst->e_a[1];
   unsigned num_env_old  = dst->bs_num_env;
   memcpy(dst->bs_freq_res, src->bs_freq_res, sizeof(dst->bs_freq_res));
   memcpy(dst->t_env, src->t_env, sizeof(dst->t_env));
   memcpy(dst->t_q, src->t_q, sizeof(dst->t_q));
   dst->bs_num_env      = src->bs_num_env;
   dst->bs_num_noise    = src->bs_num_noise;
   dst->bs_frame_class  = src->bs_frame_class;
   dst->bs_amp_res      = src->bs_amp_res;
   dst->bs_freq_res[0]  = freq_res_prev;
   dst->t_env_num_env_old = t_env_old;
   dst->e_a[0] = -(e_a1 != (int)num_env_old);
   dst->e_a[1] = src->e_a[1];
}

static void raac_sbr_read_dtdf(raac_sbr *s, raac_bits *b, raac_sbr_ch *c)
{
   unsigned i;
   (void)s;
   for (i = 0; i < c->bs_num_env; i++)
      c->bs_df_env[i] = (uint8_t)raac_getbits(b, 1);
   for (i = 0; i < c->bs_num_noise; i++)
      c->bs_df_noise[i] = (uint8_t)raac_getbits(b, 1);
}

static void raac_sbr_read_invf(raac_sbr *s, raac_bits *b, raac_sbr_ch *c)
{
   int i;
   memcpy(c->bs_invf_mode[1], c->bs_invf_mode[0], 5);
   for (i = 0; i < s->n_q; i++)
      c->bs_invf_mode[0][i] = (uint8_t)raac_getbits(b, 2);
}

/* sbr_envelope (14496-3 4.6.18.3.4): time- or frequency-delta coded
 * envelope scalefactors, huffman book chosen by amplitude resolution
 * and channel coupling, with resolution-crossing index mapping when
 * consecutive envelopes differ in frequency resolution */
static int raac_sbr_read_envelope(raac_t *a, raac_sbr *s, raac_bits *b,
      raac_sbr_ch *c, int ch)
{
   int bits, t_lav, f_lav;
   const raac_sbr_book *t_huff, *f_huff;
   int i, j, k;
   int delta = ((ch == 1 && s->bs_coupling == 1) ? 1 : 0) + 1;
   int odd   = s->n[1] & 1;
   int bi;

   (void)a;
   if (s->bs_coupling && ch)
      bi = c->bs_amp_res ? 6 : 2;
   else
      bi = c->bs_amp_res ? 4 : 0;
   bits   = (s->bs_coupling && ch) ? (c->bs_amp_res ? 5 : 6)
                                   : (c->bs_amp_res ? 6 : 7);
   t_huff = &raac_sbr_books[bi];
   f_huff = &raac_sbr_books[bi + 1];
   t_lav  = raac_sbr_lav[bi];
   f_lav  = raac_sbr_lav[bi + 1];

   for (i = 0; i < (int)c->bs_num_env; i++)
   {
      int nb = s->n[c->bs_freq_res[i + 1]];
      if (c->bs_df_env[i])
      {
         for (j = 0; j < nb; j++)
         {
            int t = raac_sbr_huff(b, t_huff);
            int ref;
            if (t < 0)
               return -1;
            if (c->bs_freq_res[i + 1] == c->bs_freq_res[i])
               ref = c->env_facs_q[i][j];
            else if (c->bs_freq_res[i + 1])
            {
               k   = (j + odd) >> 1;
               ref = c->env_facs_q[i][k];
            }
            else
            {
               k   = j ? 2 * j - odd : 0;
               ref = c->env_facs_q[i][k];
            }
            c->env_facs_q[i + 1][j] = ref + delta * (t - t_lav);
            if ((unsigned)c->env_facs_q[i + 1][j] > 127u)
               return -1;
         }
      }
      else
      {
         c->env_facs_q[i + 1][0] = delta * (int)raac_getbits(b, bits);
         for (j = 1; j < nb; j++)
         {
            int t = raac_sbr_huff(b, f_huff);
            if (t < 0)
               return -1;
            c->env_facs_q[i + 1][j] = c->env_facs_q[i + 1][j - 1] +
                  delta * (t - f_lav);
            if ((unsigned)c->env_facs_q[i + 1][j] > 127u)
               return -1;
         }
      }
   }
   memcpy(c->env_facs_q[0], c->env_facs_q[c->bs_num_env],
         sizeof(c->env_facs_q[0]));
   return 0;
}

static int raac_sbr_read_noise(raac_t *a, raac_sbr *s, raac_bits *b,
      raac_sbr_ch *c, int ch)
{
   const raac_sbr_book *t_huff, *f_huff;
   int t_lav, f_lav;
   int i, j;
   int delta = ((ch == 1 && s->bs_coupling == 1) ? 1 : 0) + 1;

   (void)a;
   if (s->bs_coupling && ch)
   {
      t_huff = &raac_sbr_books[9]; t_lav = raac_sbr_lav[9];
      f_huff = &raac_sbr_books[7]; f_lav = raac_sbr_lav[7];
   }
   else
   {
      t_huff = &raac_sbr_books[8]; t_lav = raac_sbr_lav[8];
      f_huff = &raac_sbr_books[5]; f_lav = raac_sbr_lav[5];
   }
   for (i = 0; i < (int)c->bs_num_noise; i++)
   {
      if (c->bs_df_noise[i])
      {
         for (j = 0; j < s->n_q; j++)
         {
            int t = raac_sbr_huff(b, t_huff);
            if (t < 0)
               return -1;
            c->noise_facs_q[i + 1][j] = c->noise_facs_q[i][j] +
                  delta * (t - t_lav);
            if ((unsigned)c->noise_facs_q[i + 1][j] > 30u)
               return -1;
         }
      }
      else
      {
         c->noise_facs_q[i + 1][0] = delta * (int)raac_getbits(b, 5);
         for (j = 1; j < s->n_q; j++)
         {
            int t = raac_sbr_huff(b, f_huff);
            if (t < 0)
               return -1;
            c->noise_facs_q[i + 1][j] = c->noise_facs_q[i + 1][j - 1] +
                  delta * (t - f_lav);
            if ((unsigned)c->noise_facs_q[i + 1][j] > 30u)
               return -1;
         }
      }
   }
   memcpy(c->noise_facs_q[0], c->noise_facs_q[c->bs_num_noise],
         sizeof(c->noise_facs_q[0]));
   return 0;
}

static int raac_sbr_read_sce(raac_t *a, raac_sbr *s, raac_bits *b)
{
   if (raac_getbits(b, 1))     /* bs_data_extra                     */
      raac_getbits(b, 4);
   if (raac_sbr_read_grid(s, b, &s->d[0]) < 0)
      return -1;
   raac_sbr_read_dtdf(s, b, &s->d[0]);
   raac_sbr_read_invf(s, b, &s->d[0]);
   if (raac_sbr_read_envelope(a, s, b, &s->d[0], 0) < 0)
      return -1;
   if (raac_sbr_read_noise(a, s, b, &s->d[0], 0) < 0)
      return -1;
   if ((s->d[0].bs_add_harmonic_flag = (uint8_t)raac_getbits(b, 1)))
   {
      int i;
      for (i = 0; i < s->n[1]; i++)
         s->d[0].bs_add_harmonic[i] = (uint8_t)raac_getbits(b, 1);
   }
   return 0;
}

static int raac_sbr_read_cpe(raac_t *a, raac_sbr *s, raac_bits *b)
{
   int i;
   if (raac_getbits(b, 1))     /* bs_data_extra                     */
      raac_getbits(b, 8);
   if ((s->bs_coupling = (uint8_t)raac_getbits(b, 1)))
   {
      if (raac_sbr_read_grid(s, b, &s->d[0]) < 0)
         return -1;
      raac_sbr_copy_grid(&s->d[1], &s->d[0]);
      raac_sbr_read_dtdf(s, b, &s->d[0]);
      raac_sbr_read_dtdf(s, b, &s->d[1]);
      raac_sbr_read_invf(s, b, &s->d[0]);
      memcpy(s->d[1].bs_invf_mode[1], s->d[1].bs_invf_mode[0], 5);
      memcpy(s->d[1].bs_invf_mode[0], s->d[0].bs_invf_mode[0], 5);
      if (raac_sbr_read_envelope(a, s, b, &s->d[0], 0) < 0 ||
            raac_sbr_read_noise(a, s, b, &s->d[0], 0) < 0 ||
            raac_sbr_read_envelope(a, s, b, &s->d[1], 1) < 0 ||
            raac_sbr_read_noise(a, s, b, &s->d[1], 1) < 0)
         return -1;
   }
   else
   {
      if (raac_sbr_read_grid(s, b, &s->d[0]) < 0 ||
            raac_sbr_read_grid(s, b, &s->d[1]) < 0)
         return -1;
      raac_sbr_read_dtdf(s, b, &s->d[0]);
      raac_sbr_read_dtdf(s, b, &s->d[1]);
      raac_sbr_read_invf(s, b, &s->d[0]);
      raac_sbr_read_invf(s, b, &s->d[1]);
      if (raac_sbr_read_envelope(a, s, b, &s->d[0], 0) < 0 ||
            raac_sbr_read_envelope(a, s, b, &s->d[1], 1) < 0 ||
            raac_sbr_read_noise(a, s, b, &s->d[0], 0) < 0 ||
            raac_sbr_read_noise(a, s, b, &s->d[1], 1) < 0)
         return -1;
   }
   for (i = 0; i < 2; i++)
      if ((s->d[i].bs_add_harmonic_flag = (uint8_t)raac_getbits(b, 1)))
      {
         int j;
         for (j = 0; j < s->n[1]; j++)
            s->d[i].bs_add_harmonic[j] = (uint8_t)raac_getbits(b, 1);
      }
   return 0;
}

/* sbr_extension_data carried in a FIL element (14496-3 table 4.55).
 * Parses into the element's persistent SBR state; a malformed payload
 * turns SBR off for the element rather than failing the frame, as
 * reference decoders do. Synthesis is not yet wired up: this stage
 * carries the complete bitstream layer and band tables. */
static void raac_sbr_extension(raac_t *a, raac_sbr *s, raac_bits *b,
      int crc, size_t cnt)
{
   size_t limit = b->pos + cnt * 8 - 4;    /* ext type already read  */

   s->reset = 0;
   if (!s->sample_rate)
      s->sample_rate = 2 * a->sample_rate;
   if (crc)
      raac_getbits(b, 10);

   s->kx[0] = s->kx[1];
   s->m[0]  = s->m[1];

   if (raac_getbits(b, 1))     /* bs_header_flag                    */
      raac_sbr_read_header(s, b);
   if (s->reset)
   {
      if (raac_sbr_make_f_master(s) < 0 ||
            raac_sbr_make_f_derived(s) < 0)
      {
         raac_sbr_turnoff(s);
         return;
      }
   }
   if (s->start)
   {
      int ok;
      if (s->is_cpe)
         ok = raac_sbr_read_cpe(a, s, b);
      else
         ok = raac_sbr_read_sce(a, s, b);
      if (ok < 0 || b->err || b->pos > limit)
      {
#ifdef RAAC_SBR_TRACE
         fprintf(stderr, "sbr desync: ok=%d err=%d pos=%u limit=%u\n",
               ok, b->err, (unsigned)b->pos, (unsigned)limit);
#endif
         raac_sbr_turnoff(s);
         return;
      }
#ifdef RAAC_SBR_TRACE
      fprintf(stderr,
            "sbr ok: cpe=%d k0=%d k2=%d kx=%d M=%d nm=%d nq=%d nl=%d "
            "pat=%d env=%u/%u amp=%d cpl=%d slack=%d\n",
            s->is_cpe, s->k0, s->k2, s->kx[1], s->m[1], s->n_master,
            s->n_q, s->n_lim, s->num_patches, s->d[0].bs_num_env,
            s->is_cpe ? s->d[1].bs_num_env : 0, s->bs_amp_res_hdr,
            s->bs_coupling, (int)(limit - b->pos));
#endif
      if (raac_getbits(b, 1))  /* bs_extended_data                  */
      {
         unsigned left = raac_getbits(b, 4);
         if (left == 15)
            left += raac_getbits(b, 8);
         b->pos += (size_t)left * 8;   /* PS and fill: skipped       */
      }
      if (b->err || b->pos > limit)
         raac_sbr_turnoff(s);
   }
}

/* Apply every active CCE at one coupling point to one element, in
 * element instance tag order. */
static void raac_couple_all(raac_t *a, int point, int is_cpe, int tag,
      unsigned ch0, float *pcm0, float *pcm1)
{
   int t, s;
   for (t = 0; t < 16; t++)
      for (s = 0; s < RAAC_MAX_CCE; s++)
         if (a->cce[s].in_use && a->cce[s].tag == t)
            raac_cce_couple(a, &a->cce[s], point, is_cpe, tag,
                  ch0, pcm0, pcm1);
}

/* program_config_element (14496-3 4.4.1.1). Parses one PCE, leaving
 * the reader just past it. Writes the number of channel elements the
 * layout lists (SCE, CPE and LFE each count one) and the channel
 * total (CPEs count two; coupling channels add none) through the
 * optional out pointers. Returns 0, or -1 when the element is
 * truncated. byte_alignment() inside a PCE is
 * relative to the start of the enclosing structure - the ASC or the
 * raw_data_block - and the bit reader's buffer begins there in both
 * callers, so aligning the raw position is correct. */
static int raac_pce(raac_bits *b, int *elems, int *channels)
{
   int nfront, nside, nback, nlfe, nassoc, ncc;
   int i, n_elems, n_ch;
   raac_getbits(b, 4);           /* element_instance_tag       */
   raac_getbits(b, 2);           /* object_type                */
   raac_getbits(b, 4);           /* sampling_frequency_index   */
   nfront = (int)raac_getbits(b, 4);
   nside  = (int)raac_getbits(b, 4);
   nback  = (int)raac_getbits(b, 4);
   nlfe   = (int)raac_getbits(b, 2);
   nassoc = (int)raac_getbits(b, 3);
   ncc    = (int)raac_getbits(b, 4);
   if (raac_getbits(b, 1))       /* mono_mixdown_present       */
      raac_getbits(b, 4);
   if (raac_getbits(b, 1))       /* stereo_mixdown_present     */
      raac_getbits(b, 4);
   if (raac_getbits(b, 1))       /* matrix_mixdown_idx_present */
      raac_getbits(b, 3);        /* idx + pseudo_surround      */
   n_elems = nfront + nside + nback + nlfe;
   n_ch    = nlfe;
   for (i = 0; i < nfront + nside + nback; i++)
   {
      /* element_is_cpe + element_tag_select */
      n_ch += raac_getbits(b, 1) ? 2 : 1;
      raac_getbits(b, 4);
   }
   for (i = 0; i < nlfe; i++)
      raac_getbits(b, 4);        /* lfe_element_tag_select     */
   for (i = 0; i < nassoc; i++)
      raac_getbits(b, 4);        /* assoc_data_element_tag     */
   for (i = 0; i < ncc; i++)
      raac_getbits(b, 5);        /* cc_element_is_ind_sw + tag */
   b->pos = (b->pos + 7) & ~(size_t)7;   /* byte_alignment()   */
   {
      /* comment_field_bytes; read it into a local first: += would
       * leave the order of the old-pos read against the call's pos
       * advance indeterminately sequenced */
      size_t cf = (size_t)raac_getbits(b, 8);
      b->pos += cf * 8;
   }
   if (b->pos > b->size * 8 || b->err)
      return -1;
   if (elems)
      *elems    = n_elems;
   if (channels)
      *channels = n_ch;
   return 0;
}

/* channels carried by channelConfiguration 1..7 (14496-3 Table 1.19):
 * 1 mono, 2 stereo, 3 SCE+CPE, 4 SCE+CPE+SCE, 5 SCE+2CPE,
 * 6 adds an LFE (5.1), 7 is SCE+3CPE+LFE (7.1, eight channels) */
static const uint8_t raac_chcfg_channels[8] = { 0, 1, 2, 3, 4, 5, 6, 8 };

/* Map an explicitly coded sampling frequency onto the sampling
 * frequency index whose scalefactor band tables apply, per the
 * ranges of ISO/IEC 14496-3 Table 4.55. */
static int raac_freq_to_sfi(unsigned freq)
{
   static const unsigned lo[11] = {
      92017, 75132, 55426, 46009, 37566, 27713,
      23004, 18783, 13856, 11502, 9391
   };
   int i;
   for (i = 0; i < 11; i++)
      if (freq >= lo[i])
         return i;
   return 11;
}

/* ===== public API ===== */

raac_t *raac_open(const uint8_t *asc, size_t asc_size)
{
   raac_t   *a;
   raac_bits b;
   unsigned  aot, sfi, chcfg, freq, channels, frame_960;
   int       i;

   if (!asc || asc_size < 2)
      return NULL;
   raac_bits_init(&b, asc, asc_size);
   aot = raac_getbits(&b, 5);
   if (aot == 31)
      aot = 32 + raac_getbits(&b, 6);
   sfi  = raac_getbits(&b, 4);
   freq = 0;
   if (sfi == 15)
      freq = raac_getbits(&b, 24); /* explicit samplingFrequency */
   chcfg = raac_getbits(&b, 4);
   if (aot != 2) /* AAC-LC only */
      return NULL;
   if (sfi == 15)
   {
      if (!freq)
         return NULL;
      sfi = (unsigned)raac_freq_to_sfi(freq);
   }
   else if (sfi > 12)
      return NULL;
   else
      freq = raac_sample_rates[sfi];
   if (chcfg > 7)
      return NULL;
   frame_960 = raac_getbits(&b, 1); /* frameLengthFlag: 960 frames */
   if (raac_getbits(&b, 1)) /* dependsOnCoreCoder */
      return NULL;
   if (raac_getbits(&b, 1)) /* extensionFlag */
      return NULL;
   if (chcfg == 0)
   {
      /* the channel layout is deferred to a PCE embedded in the
       * GASpecificConfig; the frame parser assigns elements to output
       * channels sequentially, so any coupling-free layout that fits
       * is decodable */
      int elems = 0, nch = 0;
      if (raac_pce(&b, &elems, &nch) < 0)
         return NULL;
      if (nch < 1 || nch > RAAC_MAX_CH)
         return NULL;
      channels = (unsigned)nch;
   }
   else
      channels = raac_chcfg_channels[chcfg];
   if (b.err)
      return NULL;

   if (!(a = (raac_t*)calloc(1, sizeof(*a))))
      return NULL;
   a->sfi         = (int)sfi;
   a->sample_rate = freq;
   a->channels    = channels;
   a->frame_len   = frame_960 ? 960 : 1024;
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

   {
      int nl = (int)a->frame_len * 2;   /* 2048 or 1920 */
      int ns = nl / 8;                  /*  256 or  240 */
      raac_kbd_window(a->kbd_long, nl, 4.0);
      raac_kbd_window(a->kbd_short, ns, 6.0);
      raac_sine_window(a->sine_long, nl);
      raac_sine_window(a->sine_short, ns);
      raac_make_twiddles(a->tw512_re, a->tw512_im, nl / 4, nl);
      raac_make_twiddles(a->tw64_re, a->tw64_im, ns / 4, ns);
      if (a->frame_len == 960)
      {
         int k;
         for (k = 0; k < 480; k++)
         {
            double ang    = -2.0 * M_PI * k / 480.0;
            a->w480_re[k] = (float)cos(ang);
            a->w480_im[k] = (float)sin(ang);
         }
         for (k = 0; k < 60; k++)
         {
            double ang   = -2.0 * M_PI * k / 60.0;
            a->w60_re[k] = (float)cos(ang);
            a->w60_im[k] = (float)sin(ang);
         }
      }
   }
   return a;
}

unsigned raac_channels(const raac_t *a)    { return a ? a->channels : 0; }
unsigned raac_frame_len(const raac_t *a)   { return a ? a->frame_len : 0; }
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
   raac_elem etab[RAAC_MAX_CH];
   int       n_elems = 0;
   int       done = 0;
   unsigned  ch;
   unsigned  next = 0;
   int       s, t, e;

   if (!a || !pkt || !size)
      return -1;
   raac_bits_init(&b, pkt, size);
   memset(got, 0, sizeof(got));
   for (s = 0; s < RAAC_MAX_CCE; s++)
      a->cce[s].present = 0;

   while (!done)
   {
      unsigned id = raac_getbits(&b, 3);
      if (b.err)
         return -1;
      switch (id)
      {
         case 0:  /* SCE */
         case 3:  /* LFE (same individual_channel_stream)             */
         {
            /* channel elements claim output channels in transmission
             * order, which for the standard configurations is the ISO
             * order (e.g. 5.1: SCE C, CPE L/R, CPE Ls/Rs, LFE) */
            unsigned tag = raac_getbits(&b, 4);
            if (next + 1 > a->channels)
               return -1;
            if (raac_decode_sce(a, &b, &a->ch[next]) < 0)
               return -1;
            etab[n_elems].kind = (uint8_t)id;
            etab[n_elems].tag  = (uint8_t)tag;
            etab[n_elems].ch   = (uint8_t)next;
            n_elems++;
            got[next] = 1;
            next++;
            break;
         }
         case 1:  /* CPE */
         {
            unsigned tag = raac_getbits(&b, 4);
            if (next + 2 > a->channels)
               return -1;
            if (raac_decode_cpe(a, &b, &a->ch[next], &a->ch[next + 1]) < 0)
               return -1;
            etab[n_elems].kind = 1;
            etab[n_elems].tag  = (uint8_t)tag;
            etab[n_elems].ch   = (uint8_t)next;
            n_elems++;
            got[next] = got[next + 1] = 1;
            next += 2;
            break;
         }
         case 2:  /* CCE */
         {
            unsigned  tag = raac_getbits(&b, 4);
            raac_cce *cc  = NULL;
            for (s = 0; s < RAAC_MAX_CCE; s++)
               if (a->cce[s].in_use && a->cce[s].tag == (int)tag)
               {
                  cc = &a->cce[s];
                  break;
               }
            if (!cc)
               for (s = 0; s < RAAC_MAX_CCE; s++)
                  if (!a->cce[s].in_use)
                  {
                     cc = &a->cce[s];
                     memset(cc, 0, sizeof(*cc));
                     cc->in_use = 1;
                     cc->tag    = (int)tag;
                     break;
                  }
            if (!cc || cc->present) /* out of slots, or a duplicate  */
               return -1;
            if (raac_decode_cce(a, &b, cc) < 0)
               return -1;
            cc->present = 1;
            break;
         }
         case 4:  /* DSE */
         {
            unsigned cnt;
            unsigned align;
            raac_getbits(&b, 4);          /* element_instance_tag  */
            align = raac_getbits(&b, 1);  /* data_byte_align_flag  */
            cnt   = raac_getbits(&b, 8);
            if (cnt == 255)
               cnt += raac_getbits(&b, 8);
            /* byte_alignment() comes after the count fields and only
             * when the flag asks for it (14496-3 data_stream_element);
             * it is relative to the start of the raw_data_block, which
             * is where this bit reader's buffer begins */
            if (align)
               b.pos = (b.pos + 7) & ~(size_t)7;
            b.pos += (size_t)cnt * 8;
            break;
         }
         case 5:  /* PCE: parse to keep bit position, contents unused */
            if (raac_pce(&b, NULL, NULL) < 0)
               return -1;
            break;
         case 6:  /* FIL: SBR extension payloads route to the SBR
                   * state of the element they follow; anything else
                   * (fill bytes, DRC) is skipped                     */
         {
            unsigned cnt = raac_getbits(&b, 4);
            size_t   payload;
            if (cnt == 15)
               cnt += raac_getbits(&b, 8) - 1;
            payload = b.pos;
            if (cnt > 0 && a->frame_len == 1024 && n_elems > 0 &&
                  etab[n_elems - 1].kind <= 1)
            {
               unsigned ext = raac_getbits(&b, 4);
               if (ext == 13 || ext == 14)   /* EXT_SBR_DATA[_CRC]   */
               {
                  raac_sbr *sb = NULL;
                  int       is_cpe = (etab[n_elems - 1].kind == 1);
                  int       tag    = (int)etab[n_elems - 1].tag;
                  for (s = 0; s < RAAC_MAX_SBR; s++)
                     if (a->sbr[s].in_use &&
                           a->sbr[s].is_cpe == is_cpe &&
                           a->sbr[s].tag == tag)
                     {
                        sb = &a->sbr[s];
                        break;
                     }
                  if (!sb)
                     for (s = 0; s < RAAC_MAX_SBR; s++)
                        if (!a->sbr[s].in_use)
                        {
                           sb = &a->sbr[s];
                           memset(sb, 0, sizeof(*sb));
                           sb->in_use = 1;
                           sb->is_cpe = is_cpe;
                           sb->tag    = tag;
                           break;
                        }
                  if (sb)
                     raac_sbr_extension(a, sb, &b, ext == 14, cnt);
               }
            }
            b.pos = payload + (size_t)cnt * 8;
            break;
         }
         case 7:  /* END */
            done = 1;
            break;
         default:
            return -1;
      }
      if (b.pos > b.size * 8)
         return -1;
   }

   /* the coupling channels first: their own TNS, and the synthesis
    * feeding time-domain coupling, so targets can consume them */
   for (t = 0; t < 16; t++)
      for (s = 0; s < RAAC_MAX_CCE; s++)
      {
         raac_cce *cc = &a->cce[s];
         if (cc->in_use && cc->tag == t && cc->present)
         {
            raac_tns_apply(a, &cc->ch);
            if (cc->point == 3)
               raac_filterbank(a, &cc->ch, cc->time);
         }
      }

   /* per element: spectral coupling on either side of the target's
    * TNS, filterbank, then time-domain coupling on the output. LFE
    * elements are not coupling targets. */
   for (e = 0; e < n_elems; e++)
   {
      unsigned c0     = etab[e].ch;
      int      is_cpe = (etab[e].kind == 1);
      int      targetable = (etab[e].kind <= 1);
      if (targetable)
         raac_couple_all(a, 0, is_cpe, etab[e].tag, c0, NULL, NULL);
      raac_tns_apply(a, &a->ch[c0]);
      if (is_cpe)
         raac_tns_apply(a, &a->ch[c0 + 1]);
      if (targetable)
         raac_couple_all(a, 1, is_cpe, etab[e].tag, c0, NULL, NULL);
      raac_filterbank(a, &a->ch[c0], pcm[c0]);
      if (is_cpe)
         raac_filterbank(a, &a->ch[c0 + 1], pcm[c0 + 1]);
      if (targetable)
         raac_couple_all(a, 3, is_cpe, etab[e].tag, c0,
               pcm[c0], is_cpe ? pcm[c0 + 1] : NULL);
   }

   /* channels no element claimed keep decaying through the overlap */
   for (ch = 0; ch < a->channels; ch++)
      if (!got[ch])
      {
         memset(a->ch[ch].coef, 0, sizeof(a->ch[ch].coef));
         raac_filterbank(a, &a->ch[ch], pcm[ch]);
      }
   return (int)a->frame_len;
}

int raac_decode_s16(raac_t *a, const uint8_t *pkt, size_t size,
      int16_t *out)
{
   int      ret;
   int      i;
   unsigned ch;
   if (!a)
      return -1;
   if ((ret = raac_decode_frame(a, pkt, size, a->pcm)) < 0)
      return ret;
   for (i = 0; i < (int)a->frame_len; i++)
      for (ch = 0; ch < a->channels; ch++)
      {
         /* one rounding, clamped in the float domain: casting an
          * out-of-range or non-finite float to int is undefined, and
          * hostile TNS filters can push the synthesis arbitrarily
          * high, so saturate before the cast (NaN pins to zero). */
         float v = a->pcm[ch][i];
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
   int      ret;
   int      i;
   unsigned ch;
   if (!a)
      return -1;
   if ((ret = raac_decode_frame(a, pkt, size, a->pcm)) < 0)
      return ret;
   for (i = 0; i < (int)a->frame_len; i++)
      for (ch = 0; ch < a->channels; ch++)
         out[i * a->channels + ch] = a->pcm[ch][i] * (1.0f / 32768.0f);
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
   memset(a->cce, 0, sizeof(a->cce));
   memset(a->sbr, 0, sizeof(a->sbr));
   /* reseed the PNS generator so a rewound stream decodes exactly as a
    * fresh one */
   a->noise_state = 0x1f2e3d4cu;
}

void raac_close(raac_t *a)
{
   free(a);
}
