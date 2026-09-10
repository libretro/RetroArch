/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rac3_tables.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The tables of ATSC A/52:2018 that the AC-3 codec is built on,
 * transcribed from the standard's tables by number - nothing here is
 * from any implementation. The derived ones (masktab from bndtab, the
 * symmetric quantizer levels, the coupling sub-band bins) are checked
 * against their transcriptions by the tables harness, so a slip in
 * the transcription shows as a disagreement. */

#ifndef __LIBRETRO_SDK_FORMAT_RAC3_TABLES_H__
#define __LIBRETRO_SDK_FORMAT_RAC3_TABLES_H__

#include <stdint.h>

/* Tables 7.6 to 7.11: the bit allocation parameters by code. */
static const uint8_t  rac3_slowdec[4]  = { 0x0f, 0x11, 0x13, 0x15 };
static const uint8_t  rac3_fastdec[4]  = { 0x3f, 0x53, 0x67, 0x7b };
static const uint16_t rac3_slowgain[4] = { 0x540, 0x4d8, 0x478, 0x410 };
static const uint16_t rac3_dbpbtab[4]  = { 0x000, 0x700, 0x900, 0xb00 };
static const uint16_t rac3_floortab[8] = { 0x2f0, 0x2b0, 0x270, 0x230, 0x1f0, 0x170, 0x0f0, 0xf800 };
static const uint16_t rac3_fastgain[8] = { 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, 0x400 };

/* Table 7.12: the fifty bands of the bit allocation, start bin and width. */
static const uint8_t rac3_bndtab[50] = {
     0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
    20,  21,  22,  23,  24,  25,  26,  27,  28,  31,
    34,  37,  40,  43,  46,  49,  55,  61,  67,  73,
    79,  85,  97, 109, 121, 133, 157, 181, 205, 229,
};
static const uint8_t rac3_bndsz[50] = {
     1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
     1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
     1,   1,   1,   1,   1,   1,   1,   1,   3,   3,
     3,   3,   3,   3,   3,   6,   6,   6,   6,   6,
     6,  12,  12,  12,  12,  24,  24,  24,  24,  24,
};

/* Table 7.13: bin to band. */
static const uint8_t rac3_masktab[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 28, 28, 29,
   29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33, 34, 34,
   34, 35, 35, 35, 35, 35, 35, 36, 36, 36, 36, 36, 36, 37, 37, 37,
   37, 37, 37, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 40,
   40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   41, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 43, 43, 43,
   43, 43, 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44,
   44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 46,
   46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48,
   48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   48, 48, 48, 48, 48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,  0,  0,  0,
};

/* Table 7.14: the log-addition table. */
static const uint8_t rac3_latab[256] = {
   0x40, 0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x34, 0x33, 0x32,
   0x31, 0x30, 0x2f, 0x2f, 0x2e, 0x2d, 0x2c, 0x2c, 0x2b, 0x2a, 0x29, 0x29, 0x28, 0x27, 0x26, 0x26,
   0x25, 0x24, 0x24, 0x23, 0x23, 0x22, 0x21, 0x21, 0x20, 0x20, 0x1f, 0x1e, 0x1e, 0x1d, 0x1d, 0x1c,
   0x1c, 0x1b, 0x1b, 0x1a, 0x1a, 0x19, 0x19, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16, 0x15, 0x15, 0x15,
   0x14, 0x14, 0x13, 0x13, 0x13, 0x12, 0x12, 0x12, 0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 0x0f, 0x0f,
   0x0f, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d, 0x0d, 0x0d, 0x0c, 0x0c, 0x0c, 0x0c, 0x0b, 0x0b, 0x0b, 0x0b,
   0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x09, 0x09, 0x09, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
   0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x05, 0x05,
   0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
   0x04, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02,
   0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
   0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Table 7.15: the hearing threshold per band at each sample rate. */
static const uint16_t rac3_hth[3][50] = {
   {
   0x04d0, 0x04d0, 0x0440, 0x0400, 0x03e0, 0x03c0, 0x03b0, 0x03b0,
   0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390,
   0x0380, 0x0380, 0x0370, 0x0370, 0x0360, 0x0360, 0x0350, 0x0350,
   0x0340, 0x0340, 0x0330, 0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0,
   0x02f0, 0x02f0, 0x0300, 0x0310, 0x0340, 0x0390, 0x03e0, 0x0420,
   0x0460, 0x0490, 0x04a0, 0x0460, 0x0440, 0x0440, 0x0520, 0x0800,
   0x0840, 0x0840,
   },
   {
   0x04f0, 0x04f0, 0x0460, 0x0410, 0x03e0, 0x03d0, 0x03c0, 0x03b0,
   0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390,
   0x0390, 0x0380, 0x0380, 0x0380, 0x0370, 0x0370, 0x0360, 0x0360,
   0x0350, 0x0350, 0x0340, 0x0340, 0x0320, 0x0310, 0x0300, 0x02f0,
   0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0320, 0x0350, 0x0390, 0x03e0,
   0x0420, 0x0450, 0x04a0, 0x0490, 0x0460, 0x0440, 0x0480, 0x0630,
   0x0840, 0x0840,
   },
   {
   0x0580, 0x0580, 0x04b0, 0x0450, 0x0420, 0x03f0, 0x03e0, 0x03d0,
   0x03c0, 0x03b0, 0x03b0, 0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0,
   0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390, 0x0390,
   0x0380, 0x0380, 0x0380, 0x0370, 0x0360, 0x0350, 0x0340, 0x0330,
   0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0310,
   0x0330, 0x0350, 0x03c0, 0x0410, 0x0470, 0x04a0, 0x0460, 0x0440,
   0x0450, 0x04e0,
   }
};

/* Table 7.16: address to bit allocation pointer. */
static const uint8_t rac3_baptab[64] = {
    0,  1,  1,  1,  1,  1,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,
    6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10,
   10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14,
   14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15,
};

/* Table 7.17: mantissa bits per bap for the asymmetric quantizers,
 * bap 6 to 15; the symmetric ones (1 to 5) are grouped and listed
 * in the decoder. */
/* Annex E Table E2.10: the frame exponent strategy combinations, an
 * index to a strategy per block (0 reuse, 1 D15, 2 D25, 3 D45). */
static const uint8_t rac3_frmexpstr[32][6] = {
   {1,0,0,0,0,0}, {1,0,0,0,0,3}, {1,0,0,0,2,0}, {1,0,0,0,3,3},
   {2,0,0,2,0,0}, {2,0,0,2,0,3}, {2,0,0,3,2,0}, {2,0,0,3,3,3},
   {2,0,1,0,0,0}, {2,0,2,0,0,3}, {2,0,2,0,2,0}, {2,0,2,0,3,3},
   {2,0,3,2,0,0}, {2,0,3,2,0,3}, {2,0,3,3,2,0}, {2,0,3,3,3,3},
   {3,1,0,0,0,0}, {3,1,0,0,0,3}, {3,2,0,0,2,0}, {3,2,0,0,3,3},
   {3,2,0,2,0,0}, {3,2,0,2,0,3}, {3,2,0,3,2,0}, {3,2,0,3,3,3},
   {3,3,1,0,0,0}, {3,3,2,0,0,3}, {3,3,2,0,2,0}, {3,3,2,0,3,3},
   {3,3,3,2,0,0}, {3,3,3,2,0,3}, {3,3,3,3,2,0}, {3,3,3,3,3,3}
};

static const uint8_t rac3_bap_bits[16] = { 0, 0, 0, 0, 0, 0, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16 };

/* Table 7.33: the window, the first half; the second half mirrors it. */
static const float rac3_window[256] = {
   0.00014f, 0.00024f, 0.00037f, 0.00051f, 0.00067f, 0.00086f, 0.00107f, 0.00130f,
   0.00157f, 0.00187f, 0.00220f, 0.00256f, 0.00297f, 0.00341f, 0.00390f, 0.00443f,
   0.00501f, 0.00564f, 0.00632f, 0.00706f, 0.00785f, 0.00871f, 0.00962f, 0.01061f,
   0.01166f, 0.01279f, 0.01399f, 0.01526f, 0.01662f, 0.01806f, 0.01959f, 0.02121f,
   0.02292f, 0.02472f, 0.02662f, 0.02863f, 0.03073f, 0.03294f, 0.03527f, 0.03770f,
   0.04025f, 0.04292f, 0.04571f, 0.04862f, 0.05165f, 0.05481f, 0.05810f, 0.06153f,
   0.06508f, 0.06878f, 0.07261f, 0.07658f, 0.08069f, 0.08495f, 0.08935f, 0.09389f,
   0.09859f, 0.10343f, 0.10842f, 0.11356f, 0.11885f, 0.12429f, 0.12988f, 0.13563f,
   0.14152f, 0.14757f, 0.15376f, 0.16011f, 0.16661f, 0.17325f, 0.18005f, 0.18699f,
   0.19407f, 0.20130f, 0.20867f, 0.21618f, 0.22382f, 0.23161f, 0.23952f, 0.24757f,
   0.25574f, 0.26404f, 0.27246f, 0.28100f, 0.28965f, 0.29841f, 0.30729f, 0.31626f,
   0.32533f, 0.33450f, 0.34376f, 0.35311f, 0.36253f, 0.37204f, 0.38161f, 0.39126f,
   0.40096f, 0.41072f, 0.42054f, 0.43040f, 0.44030f, 0.45023f, 0.46020f, 0.47019f,
   0.48020f, 0.49022f, 0.50025f, 0.51028f, 0.52031f, 0.53033f, 0.54033f, 0.55031f,
   0.56026f, 0.57019f, 0.58007f, 0.58991f, 0.59970f, 0.60944f, 0.61912f, 0.62873f,
   0.63827f, 0.64774f, 0.65713f, 0.66643f, 0.67564f, 0.68476f, 0.69377f, 0.70269f,
   0.71150f, 0.72019f, 0.72877f, 0.73723f, 0.74557f, 0.75378f, 0.76186f, 0.76981f,
   0.77762f, 0.78530f, 0.79283f, 0.80022f, 0.80747f, 0.81457f, 0.82151f, 0.82831f,
   0.83496f, 0.84145f, 0.84779f, 0.85398f, 0.86001f, 0.86588f, 0.87160f, 0.87716f,
   0.88257f, 0.88782f, 0.89291f, 0.89785f, 0.90264f, 0.90728f, 0.91176f, 0.91610f,
   0.92028f, 0.92432f, 0.92822f, 0.93197f, 0.93558f, 0.93906f, 0.94240f, 0.94560f,
   0.94867f, 0.95162f, 0.95444f, 0.95713f, 0.95971f, 0.96217f, 0.96451f, 0.96674f,
   0.96887f, 0.97089f, 0.97281f, 0.97463f, 0.97635f, 0.97799f, 0.97953f, 0.98099f,
   0.98236f, 0.98366f, 0.98488f, 0.98602f, 0.98710f, 0.98811f, 0.98905f, 0.98994f,
   0.99076f, 0.99153f, 0.99225f, 0.99291f, 0.99353f, 0.99411f, 0.99464f, 0.99513f,
   0.99558f, 0.99600f, 0.99639f, 0.99674f, 0.99706f, 0.99736f, 0.99763f, 0.99788f,
   0.99811f, 0.99831f, 0.99850f, 0.99867f, 0.99882f, 0.99895f, 0.99908f, 0.99919f,
   0.99929f, 0.99938f, 0.99946f, 0.99953f, 0.99959f, 0.99965f, 0.99969f, 0.99974f,
   0.99978f, 0.99981f, 0.99984f, 0.99986f, 0.99988f, 0.99990f, 0.99992f, 0.99993f,
   0.99994f, 0.99995f, 0.99996f, 0.99997f, 0.99998f, 0.99998f, 0.99998f, 0.99999f,
   0.99999f, 0.99999f, 0.99999f, 1.00000f, 1.00000f, 1.00000f, 1.00000f, 1.00000f,
   1.00000f, 1.00000f, 1.00000f, 1.00000f, 1.00000f, 1.00000f, 1.00000f, 1.00000f,
};

#endif
