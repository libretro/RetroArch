/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rdds.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* DirectDraw Surface (.dds) loader.
 *
 * Decodes mip level 0 of block-compressed and simple uncompressed DDS
 * images to a 32bpp buffer, matching the rest of libretro-common's
 * image backends (rpng/rjpeg/rtga/rbmp/rwebp).
 *
 * Supported block-compressed formats:
 *   FourCC 'DXT1'                     -> BC1
 *   FourCC 'DXT2' (premultiplied a)   -> BC2, un-premultiplied on decode
 *   FourCC 'DXT3'                     -> BC2
 *   FourCC 'DXT4' (premultiplied a)   -> BC3, un-premultiplied on decode
 *   FourCC 'DXT5'                     -> BC3
 *   FourCC 'ATI1'/'BC4U'              -> BC4 (R)      [DX10 too]
 *   FourCC 'ATI2'/'BC5U'              -> BC5 (RG)     [DX10 too]
 *   DX10   DXGI_FORMAT_BC6H_UF16/SF16 -> BC6H (HDR RGB, tone-mapped to 8bpp)
 *   DX10   DXGI_FORMAT_BC7_UNORM(_SRGB)-> BC7
 *   (DX10  BC1/BC2/BC3/BC4/BC5 UNORM are accepted as well)
 *
 * There is no 'DXT6'/'DXT7' FourCC: BC6H and BC7 only exist through
 * the DX10 extended header, which this loader parses.
 *
 * The block-decompression core below is adapted (C89-clean, symbols
 * prefixed, binary literals rewritten) from bcdec.h v0.97 by
 * Sergii "iOrange" Kudlai, released into the public domain
 * (MIT / The Unlicense). See https://github.com/iOrange/bcdec .
 *
 * Uncompressed images are accepted when mask-described (DDPF_RGB or
 * DDPF_LUMINANCE) at 24 or 32 bits per pixel with arbitrary channel
 * masks.
 *
 * What it does not implement: mip levels beyond 0, cubemap and volume
 * surfaces, texture arrays, 16-bit-packed and palettised uncompressed
 * formats, YUV/float non-BC formats, and encoding.
 *
 * NOTE (endianness): all on-disk block and header fields are read
 * through explicit little-endian accessors, so decoding is byte-for-byte
 * identical on little- and big-endian hosts (verified on ppc/Wii-class
 * big-endian via cross-run). No native-endian typed loads remain. */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rdds.h>

/* ================================================================== *
 *  Embedded block-decompression core (adapted from bcdec.h, PD/MIT)  *
 * ================================================================== */

/* DDS block payloads are little-endian on disk.  Read every on-disk
 * field through these explicit little-endian accessors so the decoder
 * produces identical results on little- and big-endian hosts (the
 * original bcdec reads used native-endian typed loads). */
#define RDDS_RL16(p) \
   ((unsigned short)(  (unsigned)((const unsigned char*)(p))[0] \
                    | ((unsigned)((const unsigned char*)(p))[1] << 8)))
#define RDDS_RL32(p) \
   (  (unsigned int)((const unsigned char*)(p))[0] \
   | ((unsigned int)((const unsigned char*)(p))[1] << 8) \
   | ((unsigned int)((const unsigned char*)(p))[2] << 16) \
   | ((unsigned int)((const unsigned char*)(p))[3] << 24))
#define RDDS_RL64(p) \
   (  (unsigned long long)RDDS_RL32((const unsigned char*)(p)) \
   | ((unsigned long long)RDDS_RL32((const unsigned char*)(p) + 4) << 32))

static void rdds_bcdec__color_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int onlyOpaqueMode) {
    unsigned short c0, c1;
    unsigned int refColors[4]; /* 0xAABBGGRR */
    unsigned char* dstColors;
    unsigned int colorIndices;
    int i, j, idx;
    unsigned int r0, g0, b0, r1, g1, b1, r, g, b;

    c0 = RDDS_RL16((const unsigned char*)compressedBlock);
    c1 = RDDS_RL16((const unsigned char*)compressedBlock + 2);

    /* Unpack 565 ref colors */
    r0 = (c0 >> 11) & 0x1F;
    g0 = (c0 >> 5)  & 0x3F;
    b0 =  c0        & 0x1F;

    r1 = (c1 >> 11) & 0x1F;
    g1 = (c1 >> 5)  & 0x3F;
    b1 =  c1        & 0x1F;

    /* Expand 565 ref colors to 888 */
    r = (r0 * 527 + 23) >> 6;
    g = (g0 * 259 + 33) >> 6;
    b = (b0 * 527 + 23) >> 6;
    refColors[0] = 0xFF000000 | (b << 16) | (g << 8) | r;

    r = (r1 * 527 + 23) >> 6;
    g = (g1 * 259 + 33) >> 6;
    b = (b1 * 527 + 23) >> 6;
    refColors[1] = 0xFF000000 | (b << 16) | (g << 8) | r;

    if (c0 > c1 || onlyOpaqueMode) {    /* Standard BC1 mode (also BC3 color block uses ONLY this mode) */
        /* color_2 = 2/3*color_0 + 1/3*color_1
           color_3 = 1/3*color_0 + 2/3*color_1 */
        r = ((2 * r0 + r1) *  351 +   61) >>  7;
        g = ((2 * g0 + g1) * 2763 + 1039) >> 11;
        b = ((2 * b0 + b1) *  351 +   61) >>  7;
        refColors[2] = 0xFF000000 | (b << 16) | (g << 8) | r;

        r = ((r0 + r1 * 2) *  351 +   61) >>  7;
        g = ((g0 + g1 * 2) * 2763 + 1039) >> 11;
        b = ((b0 + b1 * 2) *  351 +   61) >>  7;
        refColors[3] = 0xFF000000 | (b << 16) | (g << 8) | r;
    } else {                            /* Quite rare BC1A mode */
        /* color_2 = 1/2*color_0 + 1/2*color_1;
           color_3 = 0;                         */
        r = ((r0 + r1) * 1053 +  125) >>  8;
        g = ((g0 + g1) * 4145 + 1019) >> 11;
        b = ((b0 + b1) * 1053 +  125) >>  8;
        refColors[2] = 0xFF000000 | (b << 16) | (g << 8) | r;

        refColors[3] = 0x00000000;
    }

    colorIndices = RDDS_RL32((const unsigned char*)compressedBlock + 4);

    /* Fill out the decompressed color block */
    dstColors = (unsigned char*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            idx = colorIndices & 0x03;
            dstColors[j * 4 + 0] = (unsigned char)( refColors[idx]        & 0xFF);
            dstColors[j * 4 + 1] = (unsigned char)((refColors[idx] >>  8) & 0xFF);
            dstColors[j * 4 + 2] = (unsigned char)((refColors[idx] >> 16) & 0xFF);
            dstColors[j * 4 + 3] = (unsigned char)((refColors[idx] >> 24) & 0xFF);
            colorIndices >>= 2;
        }

        dstColors += destinationPitch;
    }
}

static void rdds_bcdec__sharp_alpha_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    const unsigned char* alpha;
    unsigned char* decompressed;
    int i, j;

    alpha = (const unsigned char*)compressedBlock;
    decompressed = (unsigned char*)decompressedBlock;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * 4] = ((RDDS_RL16(alpha + 2 * i) >> (4 * j)) & 0x0F) * 17;
        }

        decompressed += destinationPitch;
    }
}

static void rdds_bcdec__smooth_alpha_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int pixelSize) {
    unsigned char* decompressed;
    unsigned char alpha[8];
    int i, j;
    unsigned long long block, indices;

    block = RDDS_RL64(compressedBlock);
    decompressed = (unsigned char*)decompressedBlock;

    alpha[0] = block & 0xFF;
    alpha[1] = (block >> 8) & 0xFF;

    if (alpha[0] > alpha[1]) {
        /* 6 interpolated alpha values. */
        alpha[2] = (6 * alpha[0] +     alpha[1]) / 7;   /* 6/7*alpha_0 + 1/7*alpha_1 */
        alpha[3] = (5 * alpha[0] + 2 * alpha[1]) / 7;   /* 5/7*alpha_0 + 2/7*alpha_1 */
        alpha[4] = (4 * alpha[0] + 3 * alpha[1]) / 7;   /* 4/7*alpha_0 + 3/7*alpha_1 */
        alpha[5] = (3 * alpha[0] + 4 * alpha[1]) / 7;   /* 3/7*alpha_0 + 4/7*alpha_1 */
        alpha[6] = (2 * alpha[0] + 5 * alpha[1]) / 7;   /* 2/7*alpha_0 + 5/7*alpha_1 */
        alpha[7] = (    alpha[0] + 6 * alpha[1]) / 7;   /* 1/7*alpha_0 + 6/7*alpha_1 */
    }
    else {
        /* 4 interpolated alpha values. */
        alpha[2] = (4 * alpha[0] +     alpha[1]) / 5;   /* 4/5*alpha_0 + 1/5*alpha_1 */
        alpha[3] = (3 * alpha[0] + 2 * alpha[1]) / 5;   /* 3/5*alpha_0 + 2/5*alpha_1 */
        alpha[4] = (2 * alpha[0] + 3 * alpha[1]) / 5;   /* 2/5*alpha_0 + 3/5*alpha_1 */
        alpha[5] = (    alpha[0] + 4 * alpha[1]) / 5;   /* 1/5*alpha_0 + 4/5*alpha_1 */
        alpha[6] = 0x00;
        alpha[7] = 0xFF;
    }

    indices = block >> 16;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * pixelSize] = alpha[indices & 0x07];
            indices >>= 3;
        }

        decompressed += destinationPitch;
    }
}

#ifdef BCDEC_BC4BC5_PRECISE
static void rdds_bcdec__bc4_block(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int pixelSize, int isSigned) {
    signed char* sblock;
    unsigned char* ublock;
    int alpha[8];
    int i, j;
    unsigned long long block, indices;

    static int aWeights4[4] = { 13107, 26215, 39321, 52429 };
    static int aWeights6[6] = { 9363, 18724, 28086, 37450, 46812, 56173 };

    block = RDDS_RL64(compressedBlock);

    if (isSigned) {
        alpha[0] = (char)(block & 0xFF);
        alpha[1] = (char)((block >> 8) & 0xFF);
        if (alpha[0] < -127) alpha[0] = -127;     /* -128 clamps to -127 */
        if (alpha[1] < -127) alpha[1] = -127;     /* -128 clamps to -127 */
    } else {
        alpha[0] = block & 0xFF;
        alpha[1] = (block >> 8) & 0xFF;
    }

    if (alpha[0] > alpha[1]) {
        /* 6 interpolated alpha values. */
        alpha[2] = (aWeights6[5] * alpha[0] + aWeights6[0] * alpha[1] + 32768) >> 16;   /* 6/7*alpha_0 + 1/7*alpha_1 */
        alpha[3] = (aWeights6[4] * alpha[0] + aWeights6[1] * alpha[1] + 32768) >> 16;   /* 5/7*alpha_0 + 2/7*alpha_1 */
        alpha[4] = (aWeights6[3] * alpha[0] + aWeights6[2] * alpha[1] + 32768) >> 16;   /* 4/7*alpha_0 + 3/7*alpha_1 */
        alpha[5] = (aWeights6[2] * alpha[0] + aWeights6[3] * alpha[1] + 32768) >> 16;   /* 3/7*alpha_0 + 4/7*alpha_1 */
        alpha[6] = (aWeights6[1] * alpha[0] + aWeights6[4] * alpha[1] + 32768) >> 16;   /* 2/7*alpha_0 + 5/7*alpha_1 */
        alpha[7] = (aWeights6[0] * alpha[0] + aWeights6[5] * alpha[1] + 32768) >> 16;   /* 1/7*alpha_0 + 6/7*alpha_1 */
    } else {
        /* 4 interpolated alpha values. */
        alpha[2] = (aWeights4[3] * alpha[0] + aWeights4[0] * alpha[1] + 32768) >> 16;   /* 4/5*alpha_0 + 1/5*alpha_1 */
        alpha[3] = (aWeights4[2] * alpha[0] + aWeights4[1] * alpha[1] + 32768) >> 16;   /* 3/5*alpha_0 + 2/5*alpha_1 */
        alpha[4] = (aWeights4[1] * alpha[0] + aWeights4[2] * alpha[1] + 32768) >> 16;   /* 2/5*alpha_0 + 3/5*alpha_1 */
        alpha[5] = (aWeights4[0] * alpha[0] + aWeights4[3] * alpha[1] + 32768) >> 16;   /* 1/5*alpha_0 + 4/5*alpha_1 */
        alpha[6] = isSigned ? -127 :   0;
        alpha[7] = isSigned ?  127 : 255;
    }

    indices = block >> 16;
    if (isSigned) {
        sblock = (signed char*)decompressedBlock;
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j) {
                sblock[j * pixelSize] = (signed char)alpha[indices & 0x07];
                indices >>= 3;
            }
            sblock += destinationPitch;
        }
    } else {
        ublock = (unsigned char*)decompressedBlock;
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j) {
                ublock[j * pixelSize] = (unsigned char)alpha[indices & 0x07];
                indices >>= 3;
            }
            ublock += destinationPitch;
        }
    }
}

static void rdds_bcdec__bc4_block_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int pixelSize, int isSigned) {
    float* decompressed;
    float alpha[8];
    int i, j;
    unsigned long long block, indices;

    block = RDDS_RL64(compressedBlock);
    decompressed = (float*)decompressedBlock;

    if (isSigned) {
        alpha[0] = (float)((char)(block & 0xFF)) / 127.0f;
        alpha[1] = (float)((char)((block >> 8) & 0xFF)) / 127.0f;
        if (alpha[0] < -1.0f) alpha[0] = -1.0f;     /* -128 clamps to -127 */
        if (alpha[1] < -1.0f) alpha[1] = -1.0f;     /* -128 clamps to -127 */
    } else {
        alpha[0] = (float)(block & 0xFF) / 255.0f;
        alpha[1] = (float)((block >> 8) & 0xFF) / 255.0f;
    }

    if (alpha[0] > alpha[1]) {
        /* 6 interpolated alpha values. */
        alpha[2] = (6.0f * alpha[0] +        alpha[1]) / 7.0f;   /* 6/7*alpha_0 + 1/7*alpha_1 */
        alpha[3] = (5.0f * alpha[0] + 2.0f * alpha[1]) / 7.0f;   /* 5/7*alpha_0 + 2/7*alpha_1 */
        alpha[4] = (4.0f * alpha[0] + 3.0f * alpha[1]) / 7.0f;   /* 4/7*alpha_0 + 3/7*alpha_1 */
        alpha[5] = (3.0f * alpha[0] + 4.0f * alpha[1]) / 7.0f;   /* 3/7*alpha_0 + 4/7*alpha_1 */
        alpha[6] = (2.0f * alpha[0] + 5.0f * alpha[1]) / 7.0f;   /* 2/7*alpha_0 + 5/7*alpha_1 */
        alpha[7] = (       alpha[0] + 6.0f * alpha[1]) / 7.0f;   /* 1/7*alpha_0 + 6/7*alpha_1 */
    } else {
        /* 4 interpolated alpha values. */
        alpha[2] = (4.0f * alpha[0] +        alpha[1]) / 5.0f;   /* 4/5*alpha_0 + 1/5*alpha_1 */
        alpha[3] = (3.0f * alpha[0] + 2.0f * alpha[1]) / 5.0f;   /* 3/5*alpha_0 + 2/5*alpha_1 */
        alpha[4] = (2.0f * alpha[0] + 3.0f * alpha[1]) / 5.0f;   /* 2/5*alpha_0 + 3/5*alpha_1 */
        alpha[5] = (       alpha[0] + 4.0f * alpha[1]) / 5.0f;   /* 1/5*alpha_0 + 4/5*alpha_1 */
        alpha[6] = isSigned ? -1.0f : 0.0f;
        alpha[7] = 1.0f;
    }

    indices = block >> 16;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * pixelSize] = alpha[indices & 0x07];
            indices >>= 3;
        }
        decompressed += destinationPitch;
    }
}
#endif /* BCDEC_BC4BC5_PRECISE */

typedef struct rdds_bcdec__bitstream {
    unsigned long long low;
    unsigned long long high;
} rdds_bcdec__bitstream_t;

static int rdds_bcdec__bitstream_read_bits(rdds_bcdec__bitstream_t* bstream, int numBits) {
    unsigned int mask = (1 << numBits) - 1;
    /* Read the low N bits */
    unsigned int bits = (bstream->low & mask);

    bstream->low >>= numBits;
    /* Put the low N bits of "high" into the high 64-N bits of "low". */
    bstream->low |= (bstream->high & mask) << (sizeof(bstream->high) * 8 - numBits);
    bstream->high >>= numBits;
    
    return bits;
}

static int rdds_bcdec__bitstream_read_bit(rdds_bcdec__bitstream_t* bstream) {
    return rdds_bcdec__bitstream_read_bits(bstream, 1);
}

/*  reversed bits pulling, used in BC6H decoding
    why ?? just why ??? */
static int rdds_bcdec__bitstream_read_bits_r(rdds_bcdec__bitstream_t* bstream, int numBits) {
    int bits = rdds_bcdec__bitstream_read_bits(bstream, numBits);
    /* Reverse the bits. */
    int result = 0;
    while (numBits--) {
        result <<= 1;
        result |= (bits & 1);
        bits >>= 1;
    }
    return result;
}



static void rdds_bcdec_bc1(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    rdds_bcdec__color_block(compressedBlock, decompressedBlock, destinationPitch, 0);
}

static void rdds_bcdec_bc2(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    rdds_bcdec__color_block(((char*)compressedBlock) + 8, decompressedBlock, destinationPitch, 1);
    rdds_bcdec__sharp_alpha_block(compressedBlock, ((char*)decompressedBlock) + 3, destinationPitch);
}

static void rdds_bcdec_bc3(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    rdds_bcdec__color_block(((char*)compressedBlock) + 8, decompressedBlock, destinationPitch, 1);
    rdds_bcdec__smooth_alpha_block(compressedBlock, ((char*)decompressedBlock) + 3, destinationPitch, 4);
}

#ifndef BCDEC_BC4BC5_PRECISE
static void rdds_bcdec_bc4(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    rdds_bcdec__smooth_alpha_block(compressedBlock, decompressedBlock, destinationPitch, 1);
#else
static void rdds_bcdec_bc4(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    rdds_bcdec__bc4_block(compressedBlock, decompressedBlock, destinationPitch, 1, isSigned);
#endif
}

#ifndef BCDEC_BC4BC5_PRECISE
static void rdds_bcdec_bc5(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    rdds_bcdec__smooth_alpha_block(compressedBlock, decompressedBlock, destinationPitch, 2);
    rdds_bcdec__smooth_alpha_block(((char*)compressedBlock) + 8, ((char*)decompressedBlock) + 1, destinationPitch, 2);
#else
static void rdds_bcdec_bc5(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    rdds_bcdec__bc4_block(compressedBlock, decompressedBlock, destinationPitch, 2, isSigned);
    rdds_bcdec__bc4_block(((char*)compressedBlock) + 8, ((char*)decompressedBlock) + 1, destinationPitch, 2, isSigned);
#endif
}

#ifdef BCDEC_BC4BC5_PRECISE
static void rdds_bcdec_bc4_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    rdds_bcdec__bc4_block_float(compressedBlock, decompressedBlock, destinationPitch, 1, isSigned);
}

static void rdds_bcdec_bc5_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    rdds_bcdec__bc4_block_float(compressedBlock, decompressedBlock, destinationPitch, 2, isSigned);
    rdds_bcdec__bc4_block_float(((char*)compressedBlock) + 8, ((float*)decompressedBlock) + 1, destinationPitch, 2, isSigned);
}
#endif /* BCDEC_BC4BC5_PRECISE */

/* Sign-extend the low 'bits' of val.  The usual shift form,
   (val << (32 - bits)) >> (32 - bits), shifts a positive value into the
   sign bit, which is undefined behaviour and which UBSan reports; this
   does the same job in unsigned arithmetic.  'bits' comes from the BC6H
   endpoint precision tables and is 4..16, so the negated magnitude
   below stays far from INT_MIN. */
static int rdds_bcdec__extend_sign(int val, int bits) {
    unsigned mask = (1u << bits) - 1u;
    unsigned sign = 1u << (bits - 1);
    unsigned v    = (unsigned)val & mask;
    if (v & sign)
        return -(int)(mask - v + 1u);
    return (int)v;
}

static int rdds_bcdec__transform_inverse(int val, int a0, int bits, int isSigned) {
    /* If the precision of A0 is "p" bits, then the transform algorithm is:
       B0 = (B0 + A0) & ((1 << p) - 1) */
    val = (val + a0) & ((1 << bits) - 1);
    if (isSigned) {
        val = rdds_bcdec__extend_sign(val, bits);
    }
    return val;
}

/* pretty much copy-paste from documentation */
static int rdds_bcdec__unquantize(int val, int bits, int isSigned) {
    int unq, s = 0;

    if (!isSigned) {
        if (bits >= 15) {
            unq = val;
        } else if (!val) {
            unq = 0;
        } else if (val == ((1 << bits) - 1)) {
            unq = 0xFFFF;
        } else {
            unq = ((val << 16) + 0x8000) >> bits;
        }
    } else {
        if (bits >= 16) {
            unq = val;
        } else {
            if (val < 0) {
                s = 1;
                val = -val;
            }

            if (val == 0) {
                unq = 0;
            } else if (val >= ((1 << (bits - 1)) - 1)) {
                unq = 0x7FFF;
            } else {
                unq = ((val << 15) + 0x4000) >> (bits - 1);
            }

            if (s) {
                unq = -unq;
            }
        }
    }
    return unq;
}

static int rdds_bcdec__interpolate(int a, int b, int* weights, int index) {
    return (a * (64 - weights[index]) + b * weights[index] + 32) >> 6;
}

static unsigned short rdds_bcdec__finish_unquantize(int val, int isSigned) {
    int s;

    if (!isSigned) {
        return (unsigned short)((val * 31) >> 6);                   /* scale the magnitude by 31 / 64 */
    } else {
        val = (val < 0) ? -(((-val) * 31) >> 5) : (val * 31) >> 5;  /* scale the magnitude by 31 / 32 */
        s = 0;
        if (val < 0) {
            s = 0x8000;
            val = -val;
        }
        return (unsigned short)(s | val);
    }
}

/* modified half_to_float_fast4 from https://gist.github.com/rygorous/2144712 */
static float rdds_bcdec__half_to_float_quick(unsigned short half) {
    typedef union {
        unsigned int u;
        float f;
    } FP32;

    static const FP32 magic = { 113 << 23 };
    static const unsigned int shifted_exp = 0x7c00 << 13;   /* exponent mask after shift */
    FP32 o;
    unsigned int exp;

    o.u = (half & 0x7fff) << 13;                            /* exponent/mantissa bits */
    exp = shifted_exp & o.u;                                /* just the exponent */
    o.u += (127 - 15) << 23;                                /* exponent adjust */

    /* handle exponent special cases */
    if (exp == shifted_exp) {                               /* Inf/NaN? */
        o.u += (128 - 16) << 23;                            /* extra exp adjust */
    } else if (exp == 0) {                                  /* Zero/Denormal? */
        o.u += 1 << 23;                                     /* extra exp adjust */
        o.f -= magic.f;                                     /* renormalize */
    }

    /* Unsigned: 'half' promotes to int, so (half & 0x8000) << 16 shifts
       0x8000 into the sign bit of an int, which is undefined and which
       UBSan reports.  The value wanted is the unsigned 0x80000000. */
    o.u |= (unsigned int)(half & 0x8000) << 16;             /* sign bit */
    return o.f;
}

static void rdds_bcdec_bc6h_half(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    static char actual_bits_count[4][14] = {
        { 10, 7, 11, 11, 11, 9, 8, 8, 8, 6, 10, 11, 12, 16 },   /*  W */
        {  5, 6,  5,  4,  4, 5, 6, 5, 5, 6, 10,  9,  8,  4 },   /* dR */
        {  5, 6,  4,  5,  4, 5, 5, 6, 5, 6, 10,  9,  8,  4 },   /* dG */
        {  5, 6,  4,  4,  5, 5, 5, 5, 6, 6, 10,  9,  8,  4 }    /* dB */
    };

    /* There are 32 possible partition sets for a two-region tile.
       Each 4x4 block represents a single shape.
       Here also every fix-up index has MSB bit set. */
    static unsigned char partition_sets[32][4][4] = {
        { {128, 0,   1, 1}, {0, 0, 1, 1}, {  0, 0, 1, 1}, {0, 0, 1, 129} },   /*  0 */
        { {128, 0,   0, 1}, {0, 0, 0, 1}, {  0, 0, 0, 1}, {0, 0, 0, 129} },   /*  1 */
        { {128, 1,   1, 1}, {0, 1, 1, 1}, {  0, 1, 1, 1}, {0, 1, 1, 129} },   /*  2 */
        { {128, 0,   0, 1}, {0, 0, 1, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} },   /*  3 */
        { {128, 0,   0, 0}, {0, 0, 0, 1}, {  0, 0, 0, 1}, {0, 0, 1, 129} },   /*  4 */
        { {128, 0,   1, 1}, {0, 1, 1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} },   /*  5 */
        { {128, 0,   0, 1}, {0, 0, 1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} },   /*  6 */
        { {128, 0,   0, 0}, {0, 0, 0, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} },   /*  7 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  0, 0, 0, 1}, {0, 0, 1, 129} },   /*  8 */
        { {128, 0,   1, 1}, {0, 1, 1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /*  9 */
        { {128, 0,   0, 0}, {0, 0, 0, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} },   /* 10 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  0, 0, 0, 1}, {0, 1, 1, 129} },   /* 11 */
        { {128, 0,   0, 1}, {0, 1, 1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /* 12 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /* 13 */
        { {128, 0,   0, 0}, {1, 1, 1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} },   /* 14 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {  0, 0, 0, 0}, {1, 1, 1, 129} },   /* 15 */
        { {128, 0,   0, 0}, {1, 0, 0, 0}, {  1, 1, 1, 0}, {1, 1, 1, 129} },   /* 16 */
        { {128, 1, 129, 1}, {0, 0, 0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} },   /* 17 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {129, 0, 0, 0}, {1, 1, 1,   0} },   /* 18 */
        { {128, 1, 129, 1}, {0, 0, 1, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} },   /* 19 */
        { {128, 0, 129, 1}, {0, 0, 0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} },   /* 20 */
        { {128, 0,   0, 0}, {1, 0, 0, 0}, {129, 1, 0, 0}, {1, 1, 1,   0} },   /* 21 */
        { {128, 0,   0, 0}, {0, 0, 0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} },   /* 22 */
        { {128, 1,   1, 1}, {0, 0, 1, 1}, {  0, 0, 1, 1}, {0, 0, 0, 129} },   /* 23 */
        { {128, 0, 129, 1}, {0, 0, 0, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} },   /* 24 */
        { {128, 0,   0, 0}, {1, 0, 0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} },   /* 25 */
        { {128, 1, 129, 0}, {0, 1, 1, 0}, {  0, 1, 1, 0}, {0, 1, 1,   0} },   /* 26 */
        { {128, 0, 129, 1}, {0, 1, 1, 0}, {  0, 1, 1, 0}, {1, 1, 0,   0} },   /* 27 */
        { {128, 0,   0, 1}, {0, 1, 1, 1}, {129, 1, 1, 0}, {1, 0, 0,   0} },   /* 28 */
        { {128, 0,   0, 0}, {1, 1, 1, 1}, {129, 1, 1, 1}, {0, 0, 0,   0} },   /* 29 */
        { {128, 1, 129, 1}, {0, 0, 0, 1}, {  1, 0, 0, 0}, {1, 1, 1,   0} },   /* 30 */
        { {128, 0, 129, 1}, {1, 0, 0, 1}, {  1, 0, 0, 1}, {1, 1, 0,   0} }    /* 31 */
    };

    static int aWeight3[8] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    static int aWeight4[16] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    rdds_bcdec__bitstream_t bstream;
    int mode, partition, numPartitions, i, j, partitionSet, indexBits, index, ep_i, actualBits0Mode;
    int r[4], g[4], b[4];       /* wxyz */
    unsigned short* decompressed;
    int* weights;

    decompressed = (unsigned short*)decompressedBlock;

    bstream.low  = RDDS_RL64((const unsigned char*)compressedBlock);
    bstream.high = RDDS_RL64((const unsigned char*)compressedBlock + 8);

    r[0] = r[1] = r[2] = r[3] = 0;
    g[0] = g[1] = g[2] = g[3] = 0;
    b[0] = b[1] = b[2] = b[3] = 0;

    mode = rdds_bcdec__bitstream_read_bits(&bstream, 2);
    if (mode > 1) {
        mode |= (rdds_bcdec__bitstream_read_bits(&bstream, 3) << 2);
    }

    /* modes >= 11 (10 in my code) are using 0 one, others will read it from the bitstream */
    partition = 0;

    switch (mode) {
        /* mode 1 */
        case 0x00: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 75 bits (10.555, 10.555, 10.555) */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 0;
        } break;

        /* mode 2 */
        case 0x01: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 75 bits (7666, 7666, 7666) */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* gy[5]   */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* gz[5]   */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 7);        /* rw[6:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 7);        /* gw[6:0] */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* by[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 7);        /* bw[6:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* bz[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rx[5:0] */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* gx[5:0] */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* bx[5:0] */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* ry[5:0] */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rz[5:0] */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 1;
        } break;

        /* mode 3 */
        case 0x02: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (11.555, 11.444, 11.444) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            r[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            g[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* bx[3:0] */
            b[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 2;
        } break;

        /* mode 4 */
        case 0x06: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (11.444, 11.555, 11.444) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* rx[3:0] */
            r[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            g[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* bx[3:0] */
            b[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* ry[3:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* rz[3:0] */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 3;
        } break;

        /* mode 5 */
        case 0x0A: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (11.444, 11.444, 11.555) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* rx[3:0] */
            r[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            g[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* ry[3:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* rz[3:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */ 
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 4;
        } break;

        /* mode 6 */
        case 0x0E: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (9555, 9555, 9555) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 9);        /* rw[8:0] */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 9);        /* gw[8:0] */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 9);        /* bw[8:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 5;
        } break;

        /* mode 7 */
        case 0x12: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (8666, 8555, 8555) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* rw[7:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* gw[7:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* bw[7:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rx[5:0] */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* ry[5:0] */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rz[5:0] */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 6;
        } break;

        /* mode 8 */
        case 0x16: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (8555, 8666, 8555) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* rw[7:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* gw[7:0] */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* gy[5]   */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* bw[7:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* gz[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rx[4:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* gx[5:0] */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* zx[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* bx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 7;
        } break;

        /* mode 9 */
        case 0x1A: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (8555, 8555, 8666) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* rw[7:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* gw[7:0] */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* by[5]   */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* bw[7:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* bz[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* bw[4:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* gx[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* bx[5:0] */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* ry[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 5);        /* rz[4:0] */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 8;
        } break;

        /* mode 10 */
        case 0x1E: {
            /* Partitition indices: 46 bits
               Partition: 5 bits
               Color Endpoints: 72 bits (6666, 6666, 6666) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rw[5:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gz[4]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream);            /* bz[0]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 1;       /* bz[1]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* by[4]   */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* gw[5:0] */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* gy[5]   */
            b[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* by[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 2;       /* bz[2]   */
            g[2] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* gy[4]   */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* bw[5:0] */
            g[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* gz[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 3;       /* bz[3]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 5;       /* bz[5]   */
            b[3] |= rdds_bcdec__bitstream_read_bit(&bstream) << 4;       /* bz[4]   */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rx[5:0] */
            g[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gy[3:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* gx[5:0] */
            g[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gz[3:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* bx[5:0] */
            b[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* by[3:0] */
            r[2] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* ry[5:0] */
            r[3] |= rdds_bcdec__bitstream_read_bits(&bstream, 6);        /* rz[5:0] */
            partition = rdds_bcdec__bitstream_read_bits(&bstream, 5);    /* d[4:0]  */
            mode = 9;
        } break;

        /* mode 11 */
        case 0x03: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (10.10, 10.10, 10.10) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rx[9:0] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gx[9:0] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bx[9:0] */
            mode = 10;
        } break;

        /* mode 12 */
        case 0x07: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (11.9, 11.9, 11.9) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 9);        /* rx[8:0] */
            r[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* rw[10]  */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 9);        /* gx[8:0] */
            g[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* gw[10]  */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 9);        /* bx[8:0] */
            b[0] |= rdds_bcdec__bitstream_read_bit(&bstream) << 10;      /* bw[10]  */
            mode = 11;
        } break;

        /* mode 13 */
        case 0x0B: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (12.8, 12.8, 12.8) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* rx[7:0] */
            r[0] |= rdds_bcdec__bitstream_read_bits_r(&bstream, 2) << 10;/* rx[10:11] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* gx[7:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits_r(&bstream, 2) << 10;/* gx[10:11] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 8);        /* bx[7:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits_r(&bstream, 2) << 10;/* bx[10:11] */
            mode = 12;
        } break;

        /* mode 14 */
        case 0x0F: {
            /* Partitition indices: 63 bits
               Partition: 0 bits
               Color Endpoints: 60 bits (16.4, 16.4, 16.4) */
            r[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* rw[9:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* gw[9:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits(&bstream, 10);       /* bw[9:0] */
            r[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* rx[3:0] */
            r[0] |= rdds_bcdec__bitstream_read_bits_r(&bstream, 6) << 10;/* rw[10:15] */
            g[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* gx[3:0] */
            g[0] |= rdds_bcdec__bitstream_read_bits_r(&bstream, 6) << 10;/* gw[10:15] */
            b[1] |= rdds_bcdec__bitstream_read_bits(&bstream, 4);        /* bx[3:0] */
            b[0] |= rdds_bcdec__bitstream_read_bits_r(&bstream, 6) << 10;/* bw[10:15] */
            mode = 13;
        } break;

        default: {
            /* Modes 10011, 10111, 11011, and 11111 (not shown) are reserved.
               Do not use these in your encoder. If the hardware is passed blocks
               with one of these modes specified, the resulting decompressed block
               must contain all zeroes in all channels except for the alpha channel. */
            for (i = 0; i < 4; ++i) {
                for (j = 0; j < 4; ++j) {
                    decompressed[j * 3 + 0] = 0;
                    decompressed[j * 3 + 1] = 0;
                    decompressed[j * 3 + 2] = 0;
                }
                decompressed += destinationPitch;
            }

            return;
        }
    }

    numPartitions = (mode >= 10) ? 0 : 1;

    actualBits0Mode = actual_bits_count[0][mode];
    if (isSigned) {
        r[0] = rdds_bcdec__extend_sign(r[0], actualBits0Mode);
        g[0] = rdds_bcdec__extend_sign(g[0], actualBits0Mode);
        b[0] = rdds_bcdec__extend_sign(b[0], actualBits0Mode);
    }

    /* Mode 11 (like Mode 10) does not use delta compression,
       and instead stores both color endpoints explicitly.  */
    if ((mode != 9 && mode != 10) || isSigned) {
        for (i = 1; i < (numPartitions + 1) * 2; ++i) {
            r[i] = rdds_bcdec__extend_sign(r[i], actual_bits_count[1][mode]);
            g[i] = rdds_bcdec__extend_sign(g[i], actual_bits_count[2][mode]);
            b[i] = rdds_bcdec__extend_sign(b[i], actual_bits_count[3][mode]);
        }
    }

    if (mode != 9 && mode != 10) {
        for (i = 1; i < (numPartitions + 1) * 2; ++i) {
            r[i] = rdds_bcdec__transform_inverse(r[i], r[0], actualBits0Mode, isSigned);
            g[i] = rdds_bcdec__transform_inverse(g[i], g[0], actualBits0Mode, isSigned);
            b[i] = rdds_bcdec__transform_inverse(b[i], b[0], actualBits0Mode, isSigned);
        }
    }

    for (i = 0; i < (numPartitions + 1) * 2; ++i) {
        r[i] = rdds_bcdec__unquantize(r[i], actualBits0Mode, isSigned);
        g[i] = rdds_bcdec__unquantize(g[i], actualBits0Mode, isSigned);
        b[i] = rdds_bcdec__unquantize(b[i], actualBits0Mode, isSigned);
    }

    weights = (mode >= 10) ? aWeight4 : aWeight3;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            partitionSet = (mode >= 10) ? ((i|j) ? 0 : 128) : partition_sets[partition][i][j];

            indexBits = (mode >= 10) ? 4 : 3;
            /* fix-up index is specified with one less bit */
            /* The fix-up index for subset 0 is always index 0 */
            if (partitionSet & 0x80) {
                indexBits--;
            }
            partitionSet &= 0x01;

            index = rdds_bcdec__bitstream_read_bits(&bstream, indexBits);

            ep_i = partitionSet * 2;
            decompressed[j * 3 + 0] = rdds_bcdec__finish_unquantize(
                                            rdds_bcdec__interpolate(r[ep_i], r[ep_i+1], weights, index), isSigned);
            decompressed[j * 3 + 1] = rdds_bcdec__finish_unquantize(
                                            rdds_bcdec__interpolate(g[ep_i], g[ep_i+1], weights, index), isSigned);
            decompressed[j * 3 + 2] = rdds_bcdec__finish_unquantize(
                                            rdds_bcdec__interpolate(b[ep_i], b[ep_i+1], weights, index), isSigned);
        }

        decompressed += destinationPitch;
    }
}

static void rdds_bcdec_bc6h_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    unsigned short block[16*3];
    float* decompressed;
    const unsigned short* b;
    int i, j;

    rdds_bcdec_bc6h_half(compressedBlock, block, 4*3, isSigned);
    b = block;
    decompressed = (float*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[j * 3 + 0] = rdds_bcdec__half_to_float_quick(*b++);
            decompressed[j * 3 + 1] = rdds_bcdec__half_to_float_quick(*b++);
            decompressed[j * 3 + 2] = rdds_bcdec__half_to_float_quick(*b++);
        }
        decompressed += destinationPitch;
    }
}

static void rdds_bcdec__swap_values(int* a, int* b) {
    a[0] ^= b[0], b[0] ^= a[0], a[0] ^= b[0];
}

static void rdds_bcdec_bc7(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    static char actual_bits_count[2][8] = {
        { 4, 6, 5, 7, 5, 7, 7, 5 },     /* RGBA  */
        { 0, 0, 0, 0, 6, 8, 7, 5 },     /* Alpha */
    };

    /* There are 64 possible partition sets for a two-region tile.
       Each 4x4 block represents a single shape.
       Here also every fix-up index has MSB bit set. */
    static unsigned char partition_sets[2][64][4][4] = {
        {   /* Partition table for 2-subset BPTC */
            { {128, 0,   1, 1}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {0, 0, 1, 129} }, /*  0 */
            { {128, 0,   0, 1}, {0, 0,   0, 1}, {  0, 0, 0, 1}, {0, 0, 0, 129} }, /*  1 */
            { {128, 1,   1, 1}, {0, 1,   1, 1}, {  0, 1, 1, 1}, {0, 1, 1, 129} }, /*  2 */
            { {128, 0,   0, 1}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} }, /*  3 */
            { {128, 0,   0, 0}, {0, 0,   0, 1}, {  0, 0, 0, 1}, {0, 0, 1, 129} }, /*  4 */
            { {128, 0,   1, 1}, {0, 1,   1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} }, /*  5 */
            { {128, 0,   0, 1}, {0, 0,   1, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} }, /*  6 */
            { {128, 0,   0, 0}, {0, 0,   0, 1}, {  0, 0, 1, 1}, {0, 1, 1, 129} }, /*  7 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  0, 0, 0, 1}, {0, 0, 1, 129} }, /*  8 */
            { {128, 0,   1, 1}, {0, 1,   1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /*  9 */
            { {128, 0,   0, 0}, {0, 0,   0, 1}, {  0, 1, 1, 1}, {1, 1, 1, 129} }, /* 10 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  0, 0, 0, 1}, {0, 1, 1, 129} }, /* 11 */
            { {128, 0,   0, 1}, {0, 1,   1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /* 12 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /* 13 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {  1, 1, 1, 1}, {1, 1, 1, 129} }, /* 14 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {  0, 0, 0, 0}, {1, 1, 1, 129} }, /* 15 */
            { {128, 0,   0, 0}, {1, 0,   0, 0}, {  1, 1, 1, 0}, {1, 1, 1, 129} }, /* 16 */
            { {128, 1, 129, 1}, {0, 0,   0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} }, /* 17 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {129, 0, 0, 0}, {1, 1, 1,   0} }, /* 18 */
            { {128, 1, 129, 1}, {0, 0,   1, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} }, /* 19 */
            { {128, 0, 129, 1}, {0, 0,   0, 1}, {  0, 0, 0, 0}, {0, 0, 0,   0} }, /* 20 */
            { {128, 0,   0, 0}, {1, 0,   0, 0}, {129, 1, 0, 0}, {1, 1, 1,   0} }, /* 21 */
            { {128, 0,   0, 0}, {0, 0,   0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} }, /* 22 */
            { {128, 1,   1, 1}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {0, 0, 0, 129} }, /* 23 */
            { {128, 0, 129, 1}, {0, 0,   0, 1}, {  0, 0, 0, 1}, {0, 0, 0,   0} }, /* 24 */
            { {128, 0,   0, 0}, {1, 0,   0, 0}, {129, 0, 0, 0}, {1, 1, 0,   0} }, /* 25 */
            { {128, 1, 129, 0}, {0, 1,   1, 0}, {  0, 1, 1, 0}, {0, 1, 1,   0} }, /* 26 */
            { {128, 0, 129, 1}, {0, 1,   1, 0}, {  0, 1, 1, 0}, {1, 1, 0,   0} }, /* 27 */
            { {128, 0,   0, 1}, {0, 1,   1, 1}, {129, 1, 1, 0}, {1, 0, 0,   0} }, /* 28 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {129, 1, 1, 1}, {0, 0, 0,   0} }, /* 29 */
            { {128, 1, 129, 1}, {0, 0,   0, 1}, {  1, 0, 0, 0}, {1, 1, 1,   0} }, /* 30 */
            { {128, 0, 129, 1}, {1, 0,   0, 1}, {  1, 0, 0, 1}, {1, 1, 0,   0} }, /* 31 */
            { {128, 1,   0, 1}, {0, 1,   0, 1}, {  0, 1, 0, 1}, {0, 1, 0, 129} }, /* 32 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {  0, 0, 0, 0}, {1, 1, 1, 129} }, /* 33 */
            { {128, 1,   0, 1}, {1, 0, 129, 0}, {  0, 1, 0, 1}, {1, 0, 1,   0} }, /* 34 */
            { {128, 0,   1, 1}, {0, 0,   1, 1}, {129, 1, 0, 0}, {1, 1, 0,   0} }, /* 35 */
            { {128, 0, 129, 1}, {1, 1,   0, 0}, {  0, 0, 1, 1}, {1, 1, 0,   0} }, /* 36 */
            { {128, 1,   0, 1}, {0, 1,   0, 1}, {129, 0, 1, 0}, {1, 0, 1,   0} }, /* 37 */
            { {128, 1,   1, 0}, {1, 0,   0, 1}, {  0, 1, 1, 0}, {1, 0, 0, 129} }, /* 38 */
            { {128, 1,   0, 1}, {1, 0,   1, 0}, {  1, 0, 1, 0}, {0, 1, 0, 129} }, /* 39 */
            { {128, 1, 129, 1}, {0, 0,   1, 1}, {  1, 1, 0, 0}, {1, 1, 1,   0} }, /* 40 */
            { {128, 0,   0, 1}, {0, 0,   1, 1}, {129, 1, 0, 0}, {1, 0, 0,   0} }, /* 41 */
            { {128, 0, 129, 1}, {0, 0,   1, 0}, {  0, 1, 0, 0}, {1, 1, 0,   0} }, /* 42 */
            { {128, 0, 129, 1}, {1, 0,   1, 1}, {  1, 1, 0, 1}, {1, 1, 0,   0} }, /* 43 */
            { {128, 1, 129, 0}, {1, 0,   0, 1}, {  1, 0, 0, 1}, {0, 1, 1,   0} }, /* 44 */
            { {128, 0,   1, 1}, {1, 1,   0, 0}, {  1, 1, 0, 0}, {0, 0, 1, 129} }, /* 45 */
            { {128, 1,   1, 0}, {0, 1,   1, 0}, {  1, 0, 0, 1}, {1, 0, 0, 129} }, /* 46 */
            { {128, 0,   0, 0}, {0, 1, 129, 0}, {  0, 1, 1, 0}, {0, 0, 0,   0} }, /* 47 */
            { {128, 1,   0, 0}, {1, 1, 129, 0}, {  0, 1, 0, 0}, {0, 0, 0,   0} }, /* 48 */
            { {128, 0, 129, 0}, {0, 1,   1, 1}, {  0, 0, 1, 0}, {0, 0, 0,   0} }, /* 49 */
            { {128, 0,   0, 0}, {0, 0, 129, 0}, {  0, 1, 1, 1}, {0, 0, 1,   0} }, /* 50 */
            { {128, 0,   0, 0}, {0, 1,   0, 0}, {129, 1, 1, 0}, {0, 1, 0,   0} }, /* 51 */
            { {128, 1,   1, 0}, {1, 1,   0, 0}, {  1, 0, 0, 1}, {0, 0, 1, 129} }, /* 52 */
            { {128, 0,   1, 1}, {0, 1,   1, 0}, {  1, 1, 0, 0}, {1, 0, 0, 129} }, /* 53 */
            { {128, 1, 129, 0}, {0, 0,   1, 1}, {  1, 0, 0, 1}, {1, 1, 0,   0} }, /* 54 */
            { {128, 0, 129, 1}, {1, 0,   0, 1}, {  1, 1, 0, 0}, {0, 1, 1,   0} }, /* 55 */
            { {128, 1,   1, 0}, {1, 1,   0, 0}, {  1, 1, 0, 0}, {1, 0, 0, 129} }, /* 56 */
            { {128, 1,   1, 0}, {0, 0,   1, 1}, {  0, 0, 1, 1}, {1, 0, 0, 129} }, /* 57 */
            { {128, 1,   1, 1}, {1, 1,   1, 0}, {  1, 0, 0, 0}, {0, 0, 0, 129} }, /* 58 */
            { {128, 0,   0, 1}, {1, 0,   0, 0}, {  1, 1, 1, 0}, {0, 1, 1, 129} }, /* 59 */
            { {128, 0,   0, 0}, {1, 1,   1, 1}, {  0, 0, 1, 1}, {0, 0, 1, 129} }, /* 60 */
            { {128, 0, 129, 1}, {0, 0,   1, 1}, {  1, 1, 1, 1}, {0, 0, 0,   0} }, /* 61 */
            { {128, 0, 129, 0}, {0, 0,   1, 0}, {  1, 1, 1, 0}, {1, 1, 1,   0} }, /* 62 */
            { {128, 1,   0, 0}, {0, 1,   0, 0}, {  0, 1, 1, 1}, {0, 1, 1, 129} }  /* 63 */
        },
        {   /* Partition table for 3-subset BPTC */
            { {128, 0, 1, 129}, {0,   0,   1, 1}, {  0,   2,   2, 1}, {  2,   2, 2, 130} }, /*  0 */
            { {128, 0, 0, 129}, {0,   0,   1, 1}, {130,   2,   1, 1}, {  2,   2, 2,   1} }, /*  1 */
            { {128, 0, 0,   0}, {2,   0,   0, 1}, {130,   2,   1, 1}, {  2,   2, 1, 129} }, /*  2 */
            { {128, 2, 2, 130}, {0,   0,   2, 2}, {  0,   0,   1, 1}, {  0,   1, 1, 129} }, /*  3 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {129,   1,   2, 2}, {  1,   1, 2, 130} }, /*  4 */
            { {128, 0, 1, 129}, {0,   0,   1, 1}, {  0,   0,   2, 2}, {  0,   0, 2, 130} }, /*  5 */
            { {128, 0, 2, 130}, {0,   0,   2, 2}, {  1,   1,   1, 1}, {  1,   1, 1, 129} }, /*  6 */
            { {128, 0, 1,   1}, {0,   0,   1, 1}, {130,   2,   1, 1}, {  2,   2, 1, 129} }, /*  7 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {129,   1,   1, 1}, {  2,   2, 2, 130} }, /*  8 */
            { {128, 0, 0,   0}, {1,   1,   1, 1}, {129,   1,   1, 1}, {  2,   2, 2, 130} }, /*  9 */
            { {128, 0, 0,   0}, {1,   1, 129, 1}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 10 */
            { {128, 0, 1,   2}, {0,   0, 129, 2}, {  0,   0,   1, 2}, {  0,   0, 1, 130} }, /* 11 */
            { {128, 1, 1,   2}, {0,   1, 129, 2}, {  0,   1,   1, 2}, {  0,   1, 1, 130} }, /* 12 */
            { {128, 1, 2,   2}, {0, 129,   2, 2}, {  0,   1,   2, 2}, {  0,   1, 2, 130} }, /* 13 */
            { {128, 0, 1, 129}, {0,   1,   1, 2}, {  1,   1,   2, 2}, {  1,   2, 2, 130} }, /* 14 */
            { {128, 0, 1, 129}, {2,   0,   0, 1}, {130,   2,   0, 0}, {  2,   2, 2,   0} }, /* 15 */
            { {128, 0, 0, 129}, {0,   0,   1, 1}, {  0,   1,   1, 2}, {  1,   1, 2, 130} }, /* 16 */
            { {128, 1, 1, 129}, {0,   0,   1, 1}, {130,   0,   0, 1}, {  2,   2, 0,   0} }, /* 17 */
            { {128, 0, 0,   0}, {1,   1,   2, 2}, {129,   1,   2, 2}, {  1,   1, 2, 130} }, /* 18 */
            { {128, 0, 2, 130}, {0,   0,   2, 2}, {  0,   0,   2, 2}, {  1,   1, 1, 129} }, /* 19 */
            { {128, 1, 1, 129}, {0,   1,   1, 1}, {  0,   2,   2, 2}, {  0,   2, 2, 130} }, /* 20 */
            { {128, 0, 0, 129}, {0,   0,   0, 1}, {130,   2,   2, 1}, {  2,   2, 2,   1} }, /* 21 */
            { {128, 0, 0,   0}, {0,   0, 129, 1}, {  0,   1,   2, 2}, {  0,   1, 2, 130} }, /* 22 */
            { {128, 0, 0,   0}, {1,   1,   0, 0}, {130,   2, 129, 0}, {  2,   2, 1,   0} }, /* 23 */
            { {128, 1, 2, 130}, {0, 129,   2, 2}, {  0,   0,   1, 1}, {  0,   0, 0,   0} }, /* 24 */
            { {128, 0, 1,   2}, {0,   0,   1, 2}, {129,   1,   2, 2}, {  2,   2, 2, 130} }, /* 25 */
            { {128, 1, 1,   0}, {1,   2, 130, 1}, {129,   2,   2, 1}, {  0,   1, 1,   0} }, /* 26 */
            { {128, 0, 0,   0}, {0,   1, 129, 0}, {  1,   2, 130, 1}, {  1,   2, 2,   1} }, /* 27 */
            { {128, 0, 2,   2}, {1,   1,   0, 2}, {129,   1,   0, 2}, {  0,   0, 2, 130} }, /* 28 */
            { {128, 1, 1,   0}, {0, 129,   1, 0}, {  2,   0,   0, 2}, {  2,   2, 2, 130} }, /* 29 */
            { {128, 0, 1,   1}, {0,   1,   2, 2}, {  0,   1, 130, 2}, {  0,   0, 1, 129} }, /* 30 */
            { {128, 0, 0,   0}, {2,   0,   0, 0}, {130,   2,   1, 1}, {  2,   2, 2, 129} }, /* 31 */
            { {128, 0, 0,   0}, {0,   0,   0, 2}, {129,   1,   2, 2}, {  1,   2, 2, 130} }, /* 32 */
            { {128, 2, 2, 130}, {0,   0,   2, 2}, {  0,   0,   1, 2}, {  0,   0, 1, 129} }, /* 33 */
            { {128, 0, 1, 129}, {0,   0,   1, 2}, {  0,   0,   2, 2}, {  0,   2, 2, 130} }, /* 34 */
            { {128, 1, 2,   0}, {0, 129,   2, 0}, {  0,   1, 130, 0}, {  0,   1, 2,   0} }, /* 35 */
            { {128, 0, 0,   0}, {1,   1, 129, 1}, {  2,   2, 130, 2}, {  0,   0, 0,   0} }, /* 36 */
            { {128, 1, 2,   0}, {1,   2,   0, 1}, {130,   0, 129, 2}, {  0,   1, 2,   0} }, /* 37 */
            { {128, 1, 2,   0}, {2,   0,   1, 2}, {129, 130,   0, 1}, {  0,   1, 2,   0} }, /* 38 */
            { {128, 0, 1,   1}, {2,   2,   0, 0}, {  1,   1, 130, 2}, {  0,   0, 1, 129} }, /* 39 */
            { {128, 0, 1,   1}, {1,   1, 130, 2}, {  2,   2,   0, 0}, {  0,   0, 1, 129} }, /* 40 */
            { {128, 1, 0, 129}, {0,   1,   0, 1}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 41 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {130,   1,   2, 1}, {  2,   1, 2, 129} }, /* 42 */
            { {128, 0, 2,   2}, {1, 129,   2, 2}, {  0,   0,   2, 2}, {  1,   1, 2, 130} }, /* 43 */
            { {128, 0, 2, 130}, {0,   0,   1, 1}, {  0,   0,   2, 2}, {  0,   0, 1, 129} }, /* 44 */
            { {128, 2, 2,   0}, {1,   2, 130, 1}, {  0,   2,   2, 0}, {  1,   2, 2, 129} }, /* 45 */
            { {128, 1, 0,   1}, {2,   2, 130, 2}, {  2,   2,   2, 2}, {  0,   1, 0, 129} }, /* 46 */
            { {128, 0, 0,   0}, {2,   1,   2, 1}, {130,   1,   2, 1}, {  2,   1, 2, 129} }, /* 47 */
            { {128, 1, 0, 129}, {0,   1,   0, 1}, {  0,   1,   0, 1}, {  2,   2, 2, 130} }, /* 48 */
            { {128, 2, 2, 130}, {0,   1,   1, 1}, {  0,   2,   2, 2}, {  0,   1, 1, 129} }, /* 49 */
            { {128, 0, 0,   2}, {1, 129,   1, 2}, {  0,   0,   0, 2}, {  1,   1, 1, 130} }, /* 50 */
            { {128, 0, 0,   0}, {2, 129,   1, 2}, {  2,   1,   1, 2}, {  2,   1, 1, 130} }, /* 51 */
            { {128, 2, 2,   2}, {0, 129,   1, 1}, {  0,   1,   1, 1}, {  0,   2, 2, 130} }, /* 52 */
            { {128, 0, 0,   2}, {1,   1,   1, 2}, {129,   1,   1, 2}, {  0,   0, 0, 130} }, /* 53 */
            { {128, 1, 1,   0}, {0, 129,   1, 0}, {  0,   1,   1, 0}, {  2,   2, 2, 130} }, /* 54 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {  2,   1, 129, 2}, {  2,   1, 1, 130} }, /* 55 */
            { {128, 1, 1,   0}, {0, 129,   1, 0}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 56 */
            { {128, 0, 2,   2}, {0,   0,   1, 1}, {  0,   0, 129, 1}, {  0,   0, 2, 130} }, /* 57 */
            { {128, 0, 2,   2}, {1,   1,   2, 2}, {129,   1,   2, 2}, {  0,   0, 2, 130} }, /* 58 */
            { {128, 0, 0,   0}, {0,   0,   0, 0}, {  0,   0,   0, 0}, {  2, 129, 1, 130} }, /* 59 */
            { {128, 0, 0, 130}, {0,   0,   0, 1}, {  0,   0,   0, 2}, {  0,   0, 0, 129} }, /* 60 */
            { {128, 2, 2,   2}, {1,   2,   2, 2}, {  0,   2,   2, 2}, {129,   2, 2, 130} }, /* 61 */
            { {128, 1, 0, 129}, {2,   2,   2, 2}, {  2,   2,   2, 2}, {  2,   2, 2, 130} }, /* 62 */
            { {128, 1, 1, 129}, {2,   0,   1, 1}, {130,   2,   0, 1}, {  2,   2, 2,   0} }  /* 63 */
        }
    };

    static int aWeight2[] = { 0, 21, 43, 64 };
    static int aWeight3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    static int aWeight4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    static unsigned char sModeHasPBits = 0xCB;

    rdds_bcdec__bitstream_t bstream;
    int mode, partition, numPartitions, numEndpoints, i, j, k, rotation, partitionSet;
    int indexSelectionBit, indexBits, indexBits2, index, index2;
    int endpoints[6][4];
    char indices[4][4];
    int r, g, b, a;
    int* weights, * weights2;
    unsigned char* decompressed;

    decompressed = (unsigned char*)decompressedBlock;

    bstream.low  = RDDS_RL64((const unsigned char*)compressedBlock);
    bstream.high = RDDS_RL64((const unsigned char*)compressedBlock + 8);

    for (mode = 0; mode < 8 && (0 == rdds_bcdec__bitstream_read_bit(&bstream)); ++mode);

    /* unexpected mode, clear the block (transparent black) */
    if (mode >= 8) {
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j) {
                decompressed[j * 4 + 0] = 0;
                decompressed[j * 4 + 1] = 0;
                decompressed[j * 4 + 2] = 0;
                decompressed[j * 4 + 3] = 0;
            }
            decompressed += destinationPitch;
        }

        return;
    }

    partition = 0;
    numPartitions = 1;
    rotation = 0;
    indexSelectionBit = 0;

    if (mode == 0 || mode == 1 || mode == 2 || mode == 3 || mode == 7) {
        numPartitions = (mode == 0 || mode == 2) ? 3 : 2;
        partition = rdds_bcdec__bitstream_read_bits(&bstream, (mode == 0) ? 4 : 6);
    }

    numEndpoints = numPartitions * 2;

    if (mode == 4 || mode == 5) {
        rotation = rdds_bcdec__bitstream_read_bits(&bstream, 2);

        if (mode == 4) {
            indexSelectionBit = rdds_bcdec__bitstream_read_bit(&bstream);
        }
    }

    /* Extract endpoints */
    /* RGB */
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < numEndpoints; ++j) {
            endpoints[j][i] = rdds_bcdec__bitstream_read_bits(&bstream, actual_bits_count[0][mode]);
        }
    }
    /* Alpha (if any) */
    if (actual_bits_count[1][mode] > 0) {
        for (j = 0; j < numEndpoints; ++j) {
            endpoints[j][3] = rdds_bcdec__bitstream_read_bits(&bstream, actual_bits_count[1][mode]);
        }
    }

    /* Fully decode endpoints */
    /* First handle modes that have P-bits */
    if (mode == 0 || mode == 1 || mode == 3 || mode == 6 || mode == 7) {
        for (i = 0; i < numEndpoints; ++i) {
            /* component-wise left-shift */
            for (j = 0; j < 4; ++j) {
                endpoints[i][j] <<= 1;
            }
        }

        /* if P-bit is shared */
        if (mode == 1) {
            i = rdds_bcdec__bitstream_read_bit(&bstream);
            j = rdds_bcdec__bitstream_read_bit(&bstream);

            /* rgb component-wise insert pbits */
            for (k = 0; k < 3; ++k) {
                endpoints[0][k] |= i;
                endpoints[1][k] |= i;
                endpoints[2][k] |= j;
                endpoints[3][k] |= j;
            }
        } else if (sModeHasPBits & (1 << mode)) {
            /* unique P-bit per endpoint */
            for (i = 0; i < numEndpoints; ++i) {
                j = rdds_bcdec__bitstream_read_bit(&bstream);
                for (k = 0; k < 4; ++k) {
                    endpoints[i][k] |= j;
                }
            }
        }
    }

    for (i = 0; i < numEndpoints; ++i) {
        /* get color components precision including pbit */
        j = actual_bits_count[0][mode] + ((sModeHasPBits >> mode) & 1);

        for (k = 0; k < 3; ++k) {
            /* left shift endpoint components so that their MSB lies in bit 7 */
            endpoints[i][k] = endpoints[i][k] << (8 - j);
            /* Replicate each component's MSB into the LSBs revealed by the left-shift operation above */
            endpoints[i][k] = endpoints[i][k] | (endpoints[i][k] >> j);
        }

        /* get alpha component precision including pbit */
        j = actual_bits_count[1][mode] + ((sModeHasPBits >> mode) & 1);

        /* left shift endpoint components so that their MSB lies in bit 7 */
        endpoints[i][3] = endpoints[i][3] << (8 - j);
        /* Replicate each component's MSB into the LSBs revealed by the left-shift operation above */
        endpoints[i][3] = endpoints[i][3] | (endpoints[i][3] >> j);
    }

    /* If this mode does not explicitly define the alpha component */
    /* set alpha equal to 1.0 */
    if (!actual_bits_count[1][mode]) {
        for (j = 0; j < numEndpoints; ++j) {
            endpoints[j][3] = 0xFF;
        }
    }

    /* Determine weights tables */
    indexBits = (mode == 0 || mode == 1) ? 3 : ((mode == 6) ? 4 : 2);
    indexBits2 = (mode == 4) ? 3 : ((mode == 5) ? 2 : 0);
    weights = (indexBits == 2) ? aWeight2 : ((indexBits == 3) ? aWeight3 : aWeight4);
    weights2 = (indexBits2 == 2) ? aWeight2 : aWeight3;

    /* Quite inconvenient that indices aren't interleaved so we have to make 2 passes here */
    /* Pass #1: collecting color indices */
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            partitionSet = (numPartitions == 1) ? ((i | j) ? 0 : 128) : partition_sets[numPartitions - 2][partition][i][j];

            indexBits = (mode == 0 || mode == 1) ? 3 : ((mode == 6) ? 4 : 2);
            /* fix-up index is specified with one less bit */
            /* The fix-up index for subset 0 is always index 0 */
            if (partitionSet & 0x80) {
                indexBits--;
            }

            indices[i][j] = rdds_bcdec__bitstream_read_bits(&bstream, indexBits);
        }
    }

    /* Pass #2: reading alpha indices (if any) and interpolating & rotating */
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            partitionSet = (numPartitions == 1) ? ((i|j) ? 0 : 128) : partition_sets[numPartitions - 2][partition][i][j];
            partitionSet &= 0x03;

            index = indices[i][j];

            if (!indexBits2) {
                r = rdds_bcdec__interpolate(endpoints[partitionSet * 2][0], endpoints[partitionSet * 2 + 1][0], weights, index);
                g = rdds_bcdec__interpolate(endpoints[partitionSet * 2][1], endpoints[partitionSet * 2 + 1][1], weights, index);
                b = rdds_bcdec__interpolate(endpoints[partitionSet * 2][2], endpoints[partitionSet * 2 + 1][2], weights, index);
                a = rdds_bcdec__interpolate(endpoints[partitionSet * 2][3], endpoints[partitionSet * 2 + 1][3], weights, index);
            } else {
                index2 = rdds_bcdec__bitstream_read_bits(&bstream, (i|j) ? indexBits2 : (indexBits2 - 1));
                /* The index value for interpolating color comes from the secondary index bits for the texel
                   if the mode has an index selection bit and its value is one, and from the primary index bits otherwise.
                   The alpha index comes from the secondary index bits if the block has a secondary index and
                   the block either doesn’t have an index selection bit or that bit is zero, and from the primary index bits otherwise. */
                if (!indexSelectionBit) {
                    r = rdds_bcdec__interpolate(endpoints[partitionSet * 2][0], endpoints[partitionSet * 2 + 1][0],  weights,  index);
                    g = rdds_bcdec__interpolate(endpoints[partitionSet * 2][1], endpoints[partitionSet * 2 + 1][1],  weights,  index);
                    b = rdds_bcdec__interpolate(endpoints[partitionSet * 2][2], endpoints[partitionSet * 2 + 1][2],  weights,  index);
                    a = rdds_bcdec__interpolate(endpoints[partitionSet * 2][3], endpoints[partitionSet * 2 + 1][3], weights2, index2);
                } else {
                    r = rdds_bcdec__interpolate(endpoints[partitionSet * 2][0], endpoints[partitionSet * 2 + 1][0], weights2, index2);
                    g = rdds_bcdec__interpolate(endpoints[partitionSet * 2][1], endpoints[partitionSet * 2 + 1][1], weights2, index2);
                    b = rdds_bcdec__interpolate(endpoints[partitionSet * 2][2], endpoints[partitionSet * 2 + 1][2], weights2, index2);
                    a = rdds_bcdec__interpolate(endpoints[partitionSet * 2][3], endpoints[partitionSet * 2 + 1][3],  weights,  index);
                }
            }

            switch (rotation) {
                case 1: {   /* 01 – Block format is Scalar(R) Vector(AGB) - swap A and R */
                    rdds_bcdec__swap_values(&a, &r);
                } break;
                case 2: {   /* 10 – Block format is Scalar(G) Vector(RAB) - swap A and G */
                    rdds_bcdec__swap_values(&a, &g);
                } break;
                case 3: {   /* 11 - Block format is Scalar(B) Vector(RGA) - swap A and B */
                    rdds_bcdec__swap_values(&a, &b);
                } break;
            }

            decompressed[j * 4 + 0] = r;
            decompressed[j * 4 + 1] = g;
            decompressed[j * 4 + 2] = b;
            decompressed[j * 4 + 3] = a;
        }

        decompressed += destinationPitch;
    }
}


/* ================================================================== *
 *  DDS container parsing + format dispatch + public rdds_t API       *
 * ================================================================== */


/* DDS magic and DDS_PIXELFORMAT flags */
#define RDDS_MAGIC          0x20534444u /* "DDS " little-endian */
#define RDDS_DDPF_ALPHA     0x00000001u
#define RDDS_DDPF_FOURCC    0x00000004u
#define RDDS_DDPF_RGB       0x00000040u
#define RDDS_DDPF_LUMINANCE 0x00020000u

#define RDDS_FOURCC(a,b,c,d) \
   (  (uint32_t)(uint8_t)(a)        | ((uint32_t)(uint8_t)(b) << 8) \
   | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

enum rdds_fmt
{
   RDDS_FMT_UNKNOWN = 0,
   RDDS_FMT_BC1,        /* DXT1                          */
   RDDS_FMT_BC2,        /* DXT3                          */
   RDDS_FMT_BC2_PM,     /* DXT2 (premultiplied alpha)    */
   RDDS_FMT_BC3,        /* DXT5                          */
   RDDS_FMT_BC3_PM,     /* DXT4 (premultiplied alpha)    */
   RDDS_FMT_BC4,        /* ATI1 / BC4U (single channel)  */
   RDDS_FMT_BC5,        /* ATI2 / BC5U (two channel)     */
   RDDS_FMT_BC6H_UF16,  /* DX10 unsigned HDR             */
   RDDS_FMT_BC6H_SF16,  /* DX10 signed HDR               */
   RDDS_FMT_BC7,        /* DX10 LDR RGBA                 */
   RDDS_FMT_RGBA        /* uncompressed, mask-described  */
};

/* Channel decomposition for the uncompressed path, derived from the
 * header masks once at begin.  Recomputing it per sliced call measured
 * ~20% slower overall on a 2048x2048 32bpp surface - the setup is
 * cheap, but hoisting it keeps the row loop free of the default-mask
 * fixup branches. */
typedef struct
{
   unsigned bpp;             /* 1..4 bytes per texel                    */
   unsigned rs, gs, bs, as;  /* channel shift                           */
   unsigned rb, gb, bb, ab;  /* channel bit width                       */
   unsigned rmax, gmax, bmax, amax; /* (1<<bits)-1, precomputed         */
   int      lum;             /* replicate R -> G,B (DDPF_LUMINANCE)     */
   /* Fast byte-shuffle path for 8-bit byte-aligned channels: sh_[rgba]
    * name the source byte index inside a little-endian texel that feeds
    * each output channel (0xFF = "fill with 255", used for absent alpha).
    * shuffle is 1 only when every present channel qualifies.  */
   int      shuffle;
   unsigned char sh_r, sh_g, sh_b, sh_a;
} rdds_rgba_masks_t;

typedef struct
{
   enum rdds_fmt fmt;
   unsigned width;
   unsigned height;
   size_t   data_offset;   /* byte offset of mip 0 pixel data */
   unsigned rgb_bits;      /* uncompressed only            */
   int      luminance;     /* DDPF_LUMINANCE set           */
   uint32_t rmask, gmask, bmask, amask; /* uncompressed only */
} rdds_info_t;

#define RDDS_PHASE_IDLE 0
#define RDDS_PHASE_SCAN 1   /* BC6H pass 1: peak channel search */
#define RDDS_PHASE_ROWS 2   /* pack rows into the output surface */

/* Output texels produced per sliced call.  Sized from the slowest
 * format so one call stays well inside a frame: BC6H measures about
 * 63 Mtexel/s per pass on a modern desktop core, so 16K texels is
 * roughly 0.26 ms there, and proportionally less for the cheaper
 * formats.  Slicing granularity is one block row (four texel rows) for
 * the block formats and one pixel row for uncompressed, so a surface
 * wide enough that a single row exceeds the budget rounds up to one
 * row per call rather than subdividing further. */
#define RDDS_TEXELS_PER_CALL 16384u

struct rdds
{
   uint8_t    *buff_data;
   uint32_t   *output_image;
   rdds_info_t info;       /* latched at begin */
   size_t      len;        /* buffer length latched at begin */
   unsigned    cursor;     /* next block row, or pixel row when RGBA */
   unsigned    rows_total; /* blocks_y, or height when RGBA */
   unsigned    rows_step;  /* rows per call, from the budget above */
   rdds_rgba_masks_t m;    /* uncompressed only, derived at begin */
   float       maxc;       /* BC6H pass 1 accumulator */
   float       inv_white2;
   int         hdr;
   int         phase;
   int         swap_rb;    /* supports_rgba latched at begin */
   uint32_t   *lut;        /* 8/16bpp: packed-value -> packed RGBA, or NULL */
};


static INLINE uint32_t rdds_rd32(const uint8_t *p)
{
   return   (uint32_t)p[0]        | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* DXGI_FORMAT subset we care about (DX10 extended header) */
static enum rdds_fmt rdds_from_dxgi(uint32_t dxgi)
{
   switch (dxgi)
   {
      case 70: /* BC1_TYPELESS   */
      case 71: /* BC1_UNORM      */
      case 72: /* BC1_UNORM_SRGB */
         return RDDS_FMT_BC1;
      case 73: /* BC2_TYPELESS   */
      case 74: /* BC2_UNORM      */
      case 75: /* BC2_UNORM_SRGB */
         return RDDS_FMT_BC2;
      case 76: /* BC3_TYPELESS   */
      case 77: /* BC3_UNORM      */
      case 78: /* BC3_UNORM_SRGB */
         return RDDS_FMT_BC3;
      case 79: /* BC4_TYPELESS   */
      case 80: /* BC4_UNORM      */
      case 81: /* BC4_SNORM      */
         return RDDS_FMT_BC4;
      case 82: /* BC5_TYPELESS   */
      case 83: /* BC5_UNORM      */
      case 84: /* BC5_SNORM      */
         return RDDS_FMT_BC5;
      case 94: /* BC6H_TYPELESS  */
      case 95: /* BC6H_UF16      */
         return RDDS_FMT_BC6H_UF16;
      case 96: /* BC6H_SF16      */
         return RDDS_FMT_BC6H_SF16;
      case 97: /* BC7_TYPELESS   */
      case 98: /* BC7_UNORM      */
      case 99: /* BC7_UNORM_SRGB */
         return RDDS_FMT_BC7;
      default:
         break;
   }
   return RDDS_FMT_UNKNOWN;
}

/* Parse the 128-byte (or 148-byte with DX10) header.  Returns true
 * and fills *out on success. */
static bool rdds_parse_header(const uint8_t *data, size_t len,
      rdds_info_t *out)
{
   uint32_t pf_flags, fourcc;
   size_t   off = 128; /* 4 magic + 124 header */

   if (len < 128)
      return false;
   if (rdds_rd32(data) != RDDS_MAGIC)
      return false;
   if (rdds_rd32(data + 4) != 124) /* dwSize of DDS_HEADER */
      return false;

   out->fmt         = RDDS_FMT_UNKNOWN;
   out->height      = rdds_rd32(data + 12);
   out->width       = rdds_rd32(data + 16);
   out->rgb_bits    = 0;
   out->rmask = out->gmask = out->bmask = out->amask = 0;

   pf_flags         = rdds_rd32(data + 80);
   fourcc           = rdds_rd32(data + 84);

   if (out->width == 0 || out->height == 0)
      return false;
   /* Guard the malloc size (width*height*4) against overflow.
    *
    * DDS dimensions are 32-bit header fields, so their product times
    * four can overflow even a 64-bit size_t and has to be checked
    * rather than merely widened.  Check the product by dividing,
    * though, rather than capping each side: a per-side ceiling of
    * 0x7fff refuses a 32768x8192 surface that decodes to only 1 GiB,
    * while admitting 32767x32767 at 4 GiB - it bounds the wrong
    * quantity.  Rejecting only what cannot be addressed on this host
    * lets the allocation decide, and a request malloc cannot satisfy
    * fails there, which the caller already handles.  On a 32-bit host
    * this is the same wrap guard the old constant approximated, now
    * exact. */
   {
      size_t max_px = (size_t)-1 / sizeof(uint32_t);
      if ((size_t)out->width > max_px / (size_t)out->height)
         return false;
      /* Row addressing inside the decoders is 'py * width + px' in
       * unsigned int, so the texel count has to fit there too or the
       * index wraps and the surface decodes scrambled (in bounds, but
       * wrong).  Widening that multiply to size_t at every texel
       * measured ~9% slower on BC3, and nothing legitimate is affected:
       * 2^32 texels is a 16 GiB decoded mip.  Refuse it here instead
       * and keep the inner loops narrow.  On a 32-bit host the guard
       * above is the binding one and this never fires. */
      if ((size_t)out->width * (size_t)out->height > (size_t)UINT_MAX)
         return false;
   }

   if (pf_flags & RDDS_DDPF_FOURCC)
   {
      if      (fourcc == RDDS_FOURCC('D','X','T','1'))
         out->fmt = RDDS_FMT_BC1;
      else if (fourcc == RDDS_FOURCC('D','X','T','2'))
         out->fmt = RDDS_FMT_BC2_PM;
      else if (fourcc == RDDS_FOURCC('D','X','T','3'))
         out->fmt = RDDS_FMT_BC2;
      else if (fourcc == RDDS_FOURCC('D','X','T','4'))
         out->fmt = RDDS_FMT_BC3_PM;
      else if (fourcc == RDDS_FOURCC('D','X','T','5'))
         out->fmt = RDDS_FMT_BC3;
      else if (   fourcc == RDDS_FOURCC('A','T','I','1')
               || fourcc == RDDS_FOURCC('B','C','4','U')
               || fourcc == RDDS_FOURCC('B','C','4','S'))
         out->fmt = RDDS_FMT_BC4;
      else if (   fourcc == RDDS_FOURCC('A','T','I','2')
               || fourcc == RDDS_FOURCC('B','C','5','U')
               || fourcc == RDDS_FOURCC('B','C','5','S'))
         out->fmt = RDDS_FMT_BC5;
      else if (fourcc == RDDS_FOURCC('D','X','1','0'))
      {
         uint32_t dxgi;
         if (len < 148)
            return false;
         dxgi     = rdds_rd32(data + 128); /* dxgiFormat */
         out->fmt = rdds_from_dxgi(dxgi);
         off      = 148;                   /* skip DX10 header */
      }
   }
   else if (pf_flags & (RDDS_DDPF_RGB | RDDS_DDPF_LUMINANCE))
   {
      out->fmt       = RDDS_FMT_RGBA;
      out->luminance = (pf_flags & RDDS_DDPF_LUMINANCE) ? 1 : 0;
      out->rgb_bits  = rdds_rd32(data + 88);
      out->rmask     = rdds_rd32(data + 92);
      out->gmask     = rdds_rd32(data + 96);
      out->bmask     = rdds_rd32(data + 100);
      out->amask     = rdds_rd32(data + 104);
      /* 8/16/24/32 bpp, mask-described.  Packed 16-bit (565, 1555,
       * 4444, ...), 8-bit (L8/A8/R3G3B2) and luminance (L8/A8L8/L16)
       * all decode through the generic mask path; the wider two keep
       * their byte-shuffle fast path.  Odd depths are refused. */
      if (   out->rgb_bits != 8  && out->rgb_bits != 16
          && out->rgb_bits != 24 && out->rgb_bits != 32)
         return false;
   }

   if (out->fmt == RDDS_FMT_UNKNOWN)
      return false;

   out->data_offset = off;
   return true;
}

static INLINE uint32_t rdds_pack(unsigned r, unsigned g, unsigned b,
      unsigned a, bool supports_rgba)
{
   if (supports_rgba)
      return   ((uint32_t)a << 24) | ((uint32_t)b << 16)
             | ((uint32_t)g << 8)  |  (uint32_t)r;
   return      ((uint32_t)a << 24) | ((uint32_t)r << 16)
             | ((uint32_t)g << 8)  |  (uint32_t)b;
}

static INLINE unsigned rdds_unpremul(unsigned c, unsigned a)
{
   unsigned v;
   if (a == 0)
      return 0;
   v = (c * 255u + (a >> 1)) / a;
   return (v > 255u) ? 255u : v;
}

static INLINE uint8_t rdds_float_to_u8(float f)
{
   int v;
   if (f <= 0.0f)
      return 0;
   if (f >= 1.0f)
      return 255;
   v = (int)(f * 255.0f + 0.5f);
   if (v < 0)   v = 0;
   if (v > 255) v = 255;
   return (uint8_t)v;
}

/* Sanitize a decoded BC6H channel: drop NaN, clamp to [0, max-half]. */
static INLINE float rdds_sanitize(float c)
{
   if (c != c)
      c = 0.0f;
   if (c < 0.0f)
      c = 0.0f;
   if (c > 65504.0f)
      c = 65504.0f;
   return c;
}

/* Extended Reinhard tone-map for BC6H HDR, white-pointed at the image's
 * own peak channel value (passed as inv_white2 = 1/white^2).  With
 * white >= 1 the operator never brightens a value, maps the peak to 1.0,
 * and rolls highlights above 1.0 into range instead of clipping them to
 * white.  Only invoked when the image actually exceeds 1.0, so SDR-range
 * BC6H is quantized unchanged.  c is assumed clamped to [0, 65504]. */
static INLINE float rdds_tonemap(float c, float inv_white2)
{
   return c * (1.0f + c * inv_white2) / (1.0f + c);
}

/* mask -> shift/scale helper for the uncompressed path */
static INLINE unsigned rdds_mask_shift(uint32_t mask)
{
   unsigned s = 0;
   if (!mask)
      return 0;
   while (!(mask & 1u))
   {
      mask >>= 1;
      s++;
   }
   return s;
}

static INLINE unsigned rdds_mask_bits(uint32_t mask, unsigned shift)
{
   unsigned n = 0;
   mask >>= shift;
   while (mask & 1u)
   {
      mask >>= 1;
      n++;
   }
   return n;
}

static INLINE unsigned rdds_scale_to_8(unsigned v, unsigned bits)
{
   unsigned max;
   if (bits == 0)
      return 0;
   if (bits >= 8)
      return (v >> (bits - 8)) & 0xffu;
   /* Exact linear expansion to 0..255: maps 0->0 and max->255 with
    * correct rounding.  For 4/5/6-bit channels this equals the usual
    * bit-replication (so R5G6B5/A4R4G4B4 stay byte-identical to other
    * decoders), and for 1/2/3-bit channels it is properly saturated
    * where plain replication leaves a gap. */
   max = (1u << bits) - 1u;
   return (v * 255u + (max >> 1)) / max;
}

/* Decode one 4x4 block of format 'fmt' at 'src' into a tight RGBA8
 * scratch 'rgba' (16 texels * 4 bytes, row-major). */
static void rdds_decode_block(enum rdds_fmt fmt, const uint8_t *src,
      uint8_t *rgba)
{
   int t;
   switch (fmt)
   {
      case RDDS_FMT_BC1:
         rdds_bcdec_bc1(src, rgba, 16);
         break;
      case RDDS_FMT_BC2:
      case RDDS_FMT_BC2_PM:
         rdds_bcdec_bc2(src, rgba, 16);
         break;
      case RDDS_FMT_BC3:
      case RDDS_FMT_BC3_PM:
         rdds_bcdec_bc3(src, rgba, 16);
         break;
      case RDDS_FMT_BC4:
      {
         uint8_t r[16];
         rdds_bcdec_bc4(src, r, 4);
         for (t = 0; t < 16; t++)
         {
            rgba[t * 4 + 0] = r[t];
            rgba[t * 4 + 1] = 0;
            rgba[t * 4 + 2] = 0;
            rgba[t * 4 + 3] = 255;
         }
         break;
      }
      case RDDS_FMT_BC5:
      {
         uint8_t rg[32];
         rdds_bcdec_bc5(src, rg, 8);
         for (t = 0; t < 16; t++)
         {
            rgba[t * 4 + 0] = rg[t * 2 + 0];
            rgba[t * 4 + 1] = rg[t * 2 + 1];
            rgba[t * 4 + 2] = 0;
            rgba[t * 4 + 3] = 255;
         }
         break;
      }
      case RDDS_FMT_BC7:
         rdds_bcdec_bc7(src, rgba, 16);
         break;
      default:
         memset(rgba, 0, 64);
         break;
   }
}

/* Direct RGBA decoders for the small block formats.
 *
 * These write a fully-interior 4x4 block straight into the RGBA8 surface
 * with one 32-bit store per texel (versus bcdec's four byte stores and,
 * for BC2/BC3, a second pass over the block for alpha).  The 5/6-bit
 * endpoint expansion and the 2:1 endpoint mixes are pulled from small
 * constant tables that hold exactly bcdec's rounded results, so the
 * output is bit-identical to the stock two-pass path - the RGBA fast
 * path and the BGRA scratch path must agree for every texel, since the
 * same texture may be decoded under either video driver.  Verified by
 * the direct/scratch consistency check in the test harness. */

/* 5-bit -> 8-bit, exact bcdec expansion: (k*527+23)>>6 */
static const unsigned char rdds_e5[32] = {
    0,  8, 16, 25, 33, 41, 49, 58, 66, 74, 82, 90, 99,107,115,123,
  132,140,148,156,165,173,181,189,197,206,214,222,230,239,247,255
};
/* 6-bit -> 8-bit, exact bcdec expansion: (k*259+33)>>6 */
static const unsigned char rdds_e6[64] = {
    0,  4,  8, 12, 16, 20, 24, 28, 32, 36, 40, 45, 49, 53, 57, 61,
   65, 69, 73, 77, 81, 85, 89, 93, 97,101,105,109,113,117,121,125,
  130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190,
  194,198,202,206,210,215,219,223,227,231,235,239,243,247,251,255
};
/* derived 5-bit endpoint mix, index = 2*a+b (0..93): (k*351+61)>>7 */
static const unsigned char rdds_d5[94] = {
    0,  3,  5,  8, 11, 14, 16, 19, 22, 25, 27, 30, 33, 36, 38, 41,
   44, 47, 49, 52, 55, 58, 60, 63, 66, 69, 71, 74, 77, 80, 82, 85,
   88, 90, 93, 96, 99,101,104,107,110,112,115,118,121,123,126,129,
  132,134,137,140,143,145,148,151,154,156,159,162,165,167,170,173,
  175,178,181,184,186,189,192,195,197,200,203,206,208,211,214,217,
  219,222,225,228,230,233,236,239,241,244,247,250,252,255
};
/* derived 6-bit endpoint mix, index = 2*a+b (0..189): (k*2763+1039)>>11 */
static const unsigned char rdds_d6[190] = {
    0,  1,  3,  4,  5,  7,  8,  9, 11, 12, 13, 15, 16, 18, 19, 20,
   22, 23, 24, 26, 27, 28, 30, 31, 32, 34, 35, 36, 38, 39, 40, 42,
   43, 45, 46, 47, 49, 50, 51, 53, 54, 55, 57, 58, 59, 61, 62, 63,
   65, 66, 67, 69, 70, 72, 73, 74, 76, 77, 78, 80, 81, 82, 84, 85,
   86, 88, 89, 90, 92, 93, 94, 96, 97, 98,100,101,103,104,105,107,
  108,109,111,112,113,115,116,117,119,120,121,123,124,125,127,128,
  130,131,132,134,135,136,138,139,140,142,143,144,146,147,148,150,
  151,152,154,155,157,158,159,161,162,163,165,166,167,169,170,171,
  173,174,175,177,178,179,181,182,183,185,186,188,189,190,192,193,
  194,196,197,198,200,201,202,204,205,206,208,209,210,212,213,215,
  216,217,219,220,221,223,224,225,227,228,229,231,232,233,235,236,
  237,239,240,242,243,244,246,247,248,250,251,252,254,255
};

/* Build the four reference colours (byte order R,G,B,A in memory) for a
 * BC2/BC3 colour block, which always uses the 4-colour opaque mode.  All
 * four carry 0xFF alpha; the caller overwrites byte 3 with the format's
 * real alpha.  (BC1 keeps the stock decoder, whose contiguous byte
 * stores the compiler already coalesces, so it needs no fused variant
 * and no punch-through handling here.) */
static INLINE void rdds_bc_refcolors(const unsigned char *cp,
      unsigned int ref[4])
{
   unsigned c0 = RDDS_RL16(cp), c1 = RDDS_RL16(cp + 2);
   unsigned r0 = (c0 >> 11) & 0x1F, g0 = (c0 >> 5) & 0x3F, b0 = c0 & 0x1F;
   unsigned r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;

   ref[0] = 0xFF000000u | ((unsigned)rdds_e5[b0] << 16)
          | ((unsigned)rdds_e6[g0] << 8) | rdds_e5[r0];
   ref[1] = 0xFF000000u | ((unsigned)rdds_e5[b1] << 16)
          | ((unsigned)rdds_e6[g1] << 8) | rdds_e5[r1];
   ref[2] = 0xFF000000u | ((unsigned)rdds_d5[2 * b0 + b1] << 16)
          | ((unsigned)rdds_d6[2 * g0 + g1] << 8) | rdds_d5[2 * r0 + r1];
   ref[3] = 0xFF000000u | ((unsigned)rdds_d5[b0 + 2 * b1] << 16)
          | ((unsigned)rdds_d6[g0 + 2 * g1] << 8) | rdds_d5[r0 + 2 * r1];
}

/* BC2: opaque colour + explicit 4-bit alpha, fused into one store. */
static void rdds_bcdec_bc2_direct(const void *compressedBlock,
      void *decompressedBlock, int destinationPitch)
{
   const unsigned char *p = (const unsigned char*)compressedBlock;
   unsigned char       *dst = (unsigned char*)decompressedBlock;
   unsigned int         ref[4];
   unsigned int         ci = RDDS_RL32(p + 12);
   int                  i, j;

   rdds_bc_refcolors(p + 8, ref);
   for (i = 0; i < 4; ++i)
   {
      unsigned int arow = RDDS_RL16(p + 2 * i); /* four 4-bit alphas */
      uint32_t    *o    = (uint32_t*)dst;
      for (j = 0; j < 4; ++j)
      {
         /* colour already carries 0xFF alpha; overwrite it with the
          * expanded 4-bit nibble (n*17 == n*0x11) shifted into place. */
         o[j] = (ref[ci & 0x03] & 0x00FFFFFFu)
              | ((((arow >> (4 * j)) & 0x0Fu) * 17u) << 24);
         ci >>= 2;
      }
      dst += destinationPitch;
   }
}

/* BC3: opaque colour + smooth (interpolated) 8-value alpha, one store. */
static void rdds_bcdec_bc3_direct(const void *compressedBlock,
      void *decompressedBlock, int destinationPitch)
{
   const unsigned char   *p = (const unsigned char*)compressedBlock;
   unsigned char         *dst = (unsigned char*)decompressedBlock;
   unsigned int           ref[4];
   unsigned int           ci;
   unsigned char          al[8];
   unsigned long long     blk = RDDS_RL64(p);   /* alpha block */
   unsigned long long     idx;
   int                    i, j;

   al[0] = (unsigned char)(blk & 0xFF);
   al[1] = (unsigned char)((blk >> 8) & 0xFF);
   if (al[0] > al[1])
   {
      al[2] = (unsigned char)((6 * al[0] +     al[1]) / 7);
      al[3] = (unsigned char)((5 * al[0] + 2 * al[1]) / 7);
      al[4] = (unsigned char)((4 * al[0] + 3 * al[1]) / 7);
      al[5] = (unsigned char)((3 * al[0] + 4 * al[1]) / 7);
      al[6] = (unsigned char)((2 * al[0] + 5 * al[1]) / 7);
      al[7] = (unsigned char)((    al[0] + 6 * al[1]) / 7);
   }
   else
   {
      al[2] = (unsigned char)((4 * al[0] +     al[1]) / 5);
      al[3] = (unsigned char)((3 * al[0] + 2 * al[1]) / 5);
      al[4] = (unsigned char)((2 * al[0] + 3 * al[1]) / 5);
      al[5] = (unsigned char)((    al[0] + 4 * al[1]) / 5);
      al[6] = 0x00;
      al[7] = 0xFF;
   }

   rdds_bc_refcolors(p + 8, ref);
   ci  = RDDS_RL32(p + 12);
   idx = blk >> 16;                              /* 16 * 3-bit indices */
   for (i = 0; i < 4; ++i)
   {
      uint32_t *o = (uint32_t*)dst;
      for (j = 0; j < 4; ++j)
      {
         o[j] = (ref[ci & 0x03] & 0x00FFFFFFu)
              | ((unsigned)al[idx & 0x07] << 24);
         ci  >>= 2;
         idx >>= 3;
      }
      dst += destinationPitch;
   }
}

/* Emit an edge (partially out-of-range) block from a decoded RGBA8
 * scratch into the surface with per-texel bounds tests.  Shared by every
 * path so the hot interior loops stay branch-free. */
static INLINE void rdds_emit_edge(const uint8_t *rgba, uint32_t *out,
      unsigned w, unsigned h, unsigned px0, unsigned py0, int premul,
      bool supports_rgba)
{
   unsigned ty, tx;
   for (ty = 0; ty < 4; ty++)
   {
      unsigned py = py0 + ty;
      if (py >= h)
         break;
      for (tx = 0; tx < 4; tx++)
      {
         unsigned px = px0 + tx;
         const uint8_t *p;
         unsigned r, g, b, a;
         if (px >= w)
            continue;
         p = &rgba[(ty * 4 + tx) * 4];
         r = p[0]; g = p[1]; b = p[2]; a = p[3];
         if (premul)
         {
            r = rdds_unpremul(r, a);
            g = rdds_unpremul(g, a);
            b = rdds_unpremul(b, a);
         }
         out[(size_t)py * w + px] = rdds_pack(r, g, b, a, supports_rgba);
      }
   }
}

/* Generate a specialized RGBA-output decoder per small BC format.  The
 * DECODE call is direct (not through a function pointer), so the block
 * decoder inlines into the loop and the interior case writes straight
 * into the surface with no scratch and no bounds test.  This is the hot
 * path for BC1/BC2/BC3 under an RGBA video driver. */
#define RDDS_GEN_ROWS_DIRECT(NAME, DECODE, BLOCKSZ)                        \
static void NAME(const rdds_info_t *info, const uint8_t *data,            \
      uint32_t *out, unsigned by0, unsigned by1)                          \
{                                                                         \
   unsigned  w        = info->width;                                      \
   unsigned  h        = info->height;                                     \
   size_t    base     = info->data_offset;                                \
   unsigned  blocks_x = (w + 3u) >> 2;                                    \
   int       pitch    = (int)(w * 4u);                                    \
   unsigned  bx, by;                                                      \
   uint32_t  scratch[16];  /* 4x4 RGBA, 32-bit aligned for edge stores */ \
   for (by = by0; by < by1; by++)                                         \
   {                                                                      \
      unsigned py0     = by * 4u;                                         \
      int      row_int = (py0 + 4u <= h);                                 \
      const uint8_t *rowsrc = data + base                                 \
                            + (size_t)by * blocks_x * (BLOCKSZ);          \
      for (bx = 0; bx < blocks_x; bx++)                                   \
      {                                                                   \
         unsigned       px0 = bx * 4u;                                    \
         const uint8_t *src = rowsrc + (size_t)bx * (BLOCKSZ);            \
         if (row_int && px0 + 4u <= w)                                    \
            DECODE(src, (uint8_t*)(out + (size_t)py0 * w + px0), pitch);  \
         else                                                             \
         {                                                                \
            DECODE(src, scratch, 16);                                     \
            rdds_emit_edge((const uint8_t*)scratch, out, w, h,            \
                  px0, py0, 0, true);                                     \
         }                                                                \
      }                                                                   \
   }                                                                      \
}

RDDS_GEN_ROWS_DIRECT(rdds_rows_bc1_rgba, rdds_bcdec_bc1,        8u)
RDDS_GEN_ROWS_DIRECT(rdds_rows_bc2_rgba, rdds_bcdec_bc2_direct, 16u)
RDDS_GEN_ROWS_DIRECT(rdds_rows_bc3_rgba, rdds_bcdec_bc3_direct, 16u)

/* Decode block rows [by0, by1) of a BCn surface into 'out'.  The caller
 * has already validated that the payload holds every block. */
static void rdds_rows_compressed(const rdds_info_t *info,
      const uint8_t *data, uint32_t *out, unsigned by0, unsigned by1,
      bool supports_rgba)
{
   uint8_t   rgba[64];
   unsigned  w          = info->width;
   unsigned  h          = info->height;
   size_t    base       = info->data_offset;
   unsigned  blocks_x   = (w + 3u) >> 2;
   unsigned  block_size = (info->fmt == RDDS_FMT_BC1
                        || info->fmt == RDDS_FMT_BC4) ? 8u : 16u;
   unsigned  bx, by, ty, tx;
   int       premul     = (info->fmt == RDDS_FMT_BC2_PM
                        || info->fmt == RDDS_FMT_BC3_PM) ? 1 : 0;

   /* Fast, specialized RGBA paths for the small block formats: the block
    * decoder inlines and interior blocks skip the scratch entirely. */
   if (supports_rgba && !premul)
   {
      switch (info->fmt)
      {
         case RDDS_FMT_BC1:
            rdds_rows_bc1_rgba(info, data, out, by0, by1); return;
         case RDDS_FMT_BC2:
            rdds_rows_bc2_rgba(info, data, out, by0, by1); return;
         case RDDS_FMT_BC3:
            rdds_rows_bc3_rgba(info, data, out, by0, by1); return;
         default:
            break; /* BC4/BC5/BC7 fall through to the scratch loop */
      }
   }

   /* Generic scratch path: BGRA output order, premultiplied alpha,
    * BC4/BC5 channel expansion, and BC7 (whose decode dominates, so the
    * scratch copy is negligible).  Interior blocks still skip per-texel
    * bounds tests. */
   for (by = by0; by < by1; by++)
   {
      unsigned py0     = by * 4u;
      int      row_int = (py0 + 4u <= h);
      for (bx = 0; bx < blocks_x; bx++)
      {
         unsigned       px0 = bx * 4u;
         const uint8_t *src = data + base
                            + ((size_t)by * blocks_x + bx) * block_size;

         rdds_decode_block(info->fmt, src, rgba);

         if (row_int && px0 + 4u <= w)
         {
            uint32_t *obase = out + (size_t)py0 * w + px0;
            for (ty = 0; ty < 4; ty++)
            {
               const uint8_t *p = &rgba[ty * 16];
               uint32_t      *o = obase + (size_t)ty * w;
               if (premul)
               {
                  for (tx = 0; tx < 4; tx++, p += 4)
                  {
                     unsigned a = p[3];
                     o[tx] = rdds_pack(rdds_unpremul(p[0], a),
                           rdds_unpremul(p[1], a), rdds_unpremul(p[2], a),
                           a, supports_rgba);
                  }
               }
               else
               {
                  for (tx = 0; tx < 4; tx++, p += 4)
                     o[tx] = rdds_pack(p[0], p[1], p[2], p[3],
                           supports_rgba);
               }
            }
         }
         else
            rdds_emit_edge(rgba, out, w, h, px0, py0, premul, supports_rgba);
      }
   }
}

/* Decompose the header masks once, applying the default layout when the
 * file left them zero (24bpp BGR is common). */
static void rdds_masks_init(const rdds_info_t *info,
      rdds_rgba_masks_t *m)
{
   m->bpp = info->rgb_bits >> 3; /* 1..4 */
   m->rs  = rdds_mask_shift(info->rmask);
   m->gs  = rdds_mask_shift(info->gmask);
   m->bs  = rdds_mask_shift(info->bmask);
   m->as  = rdds_mask_shift(info->amask);
   m->rb  = rdds_mask_bits(info->rmask, m->rs);
   m->gb  = rdds_mask_bits(info->gmask, m->gs);
   m->bb  = rdds_mask_bits(info->bmask, m->bs);
   m->ab  = rdds_mask_bits(info->amask, m->as);
   m->lum = 0;

   /* Only the 24/32bpp RGB path has a sensible zero-mask default (BGR
    * 8-8-8); 8/16bpp files that omit masks are malformed and left as
    * decoded (channels stay zero).  Luminance never defaults here - it
    * carries an explicit R (luminance) mask. */
   if (!info->luminance && m->bpp >= 3
         && !info->rmask && !info->gmask && !info->bmask)
   {
      m->bs = 0;  m->gs = 8;  m->rs = 16; m->as = 24;
      m->bb = m->gb = m->rb = 8;
      m->ab = (m->bpp == 4) ? 8 : 0;
   }

   /* Luminance: R holds the luminance channel, G/B are usually zero.
    * Replicate R across G and B so L8 / L16 read grey rather than red,
    * and A8L8 keeps its separate alpha mask.  If the file did supply
    * real G/B masks (unusual for a luminance surface) honour them. */
   if (info->luminance && !info->gmask && !info->bmask)
   {
      m->gs = m->bs = m->rs;
      m->gb = m->bb = m->rb;
      m->lum = 1;
   }

   m->rmax = (m->rb >= 32) ? 0xffffffffu : ((1u << m->rb) - 1u);
   m->gmax = (m->gb >= 32) ? 0xffffffffu : ((1u << m->gb) - 1u);
   m->bmax = (m->bb >= 32) ? 0xffffffffu : ((1u << m->bb) - 1u);
   m->amax = (m->ab >= 32) ? 0xffffffffu : ((1u << m->ab) - 1u);

   /* Byte-shuffle fast path: every present channel is exactly 8 bits
    * and byte-aligned, so decoding is a per-channel byte pick with no
    * scale.  Covers the dominant A8R8G8B8 / X8R8G8B8 / R8G8B8 files. */
   m->shuffle = 0;
   if (      (m->bpp == 4 || m->bpp == 3)
          &&  m->rb == 8 && m->gb == 8 && m->bb == 8
          && (m->rs & 7) == 0 && (m->gs & 7) == 0 && (m->bs & 7) == 0
          && (m->ab == 0 || (m->ab == 8 && (m->as & 7) == 0))
          &&  m->rs < (m->bpp * 8) && m->gs < (m->bpp * 8)
          &&  m->bs < (m->bpp * 8)
          && !m->lum)
   {
      m->shuffle = 1;
      m->sh_r    = (unsigned char)(m->rs >> 3);
      m->sh_g    = (unsigned char)(m->gs >> 3);
      m->sh_b    = (unsigned char)(m->bs >> 3);
      m->sh_a    = (m->ab == 8) ? (unsigned char)(m->as >> 3)
                                : (unsigned char)0xFF;
   }
}

/* Decode a single packed texel value through the mask description.  Used
 * to build the 8/16bpp lookup table and by the generic scalar path. */
static INLINE uint32_t rdds_decode_one(uint32_t px,
      const rdds_rgba_masks_t *m, bool supports_rgba)
{
   unsigned r = rdds_scale_to_8((px >> m->rs) & m->rmax, m->rb);
   unsigned g, b, a;
   if (m->lum)
      g = b = r;
   else
   {
      g = rdds_scale_to_8((px >> m->gs) & m->gmax, m->gb);
      b = rdds_scale_to_8((px >> m->bs) & m->bmax, m->bb);
   }
   a = m->ab ? rdds_scale_to_8((px >> m->as) & m->amax, m->ab) : 255u;
   return rdds_pack(r, g, b, a, supports_rgba);
}

/* Build the packed-value -> packed-RGBA table for an 8- or 16-bit
 * surface (256 or 65536 entries).  Returns NULL on OOM; the caller then
 * falls back to the scalar path. */
static uint32_t *rdds_build_lut(const rdds_rgba_masks_t *m,
      bool supports_rgba)
{
   unsigned  n = 1u << (m->bpp * 8);   /* 256 or 65536 */
   uint32_t *lut = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
   unsigned  i;
   if (!lut)
      return NULL;
   for (i = 0; i < n; i++)
      lut[i] = rdds_decode_one(i, m, supports_rgba);
   return lut;
}

/* Decode pixel rows [y0, y1) of an uncompressed surface into 'out'. */
static void rdds_rows_uncompressed(const rdds_info_t *info,
      const rdds_rgba_masks_t *m, const uint8_t *data, uint32_t *out,
      unsigned y0, unsigned y1, bool supports_rgba,
      const uint32_t *lut)
{
   unsigned  bpp = m->bpp;
   unsigned  rs  = m->rs, gs = m->gs, bs = m->bs, as = m->as;
   unsigned  rb  = m->rb, gb = m->gb, bb = m->bb, ab = m->ab;
   unsigned  rmax = m->rmax, gmax = m->gmax, bmax = m->bmax, amax = m->amax;
   /* Hoisted deliberately: 'out' is a parameter here rather than a
    * fresh allocation, so the compiler can no longer prove it does not
    * alias *info, and reloads width / data_offset on every pixel.  That
    * cost about 20% of the uncompressed path when measured. */
   unsigned  w   = info->width;
   size_t    off = info->data_offset;
   unsigned  x, y;

   /* Fastest path: 8/16bpp with a prebuilt packed-value table.  The row
    * loop is a single indexed load per texel - no shifts, scales or
    * masks - which makes the narrow formats memory-bound rather than
    * ALU-bound (and folds luminance replication and alpha into the
    * table for free). */
   if (lut)
   {
      if (bpp == 2)
      {
         for (y = y0; y < y1; y++)
         {
            const uint8_t *p = data + off + (size_t)y * w * 2u;
            uint32_t      *o = out + (size_t)y * w;
            for (x = 0; x < w; x++, p += 2)
               o[x] = lut[(unsigned)p[0] | ((unsigned)p[1] << 8)];
         }
      }
      else /* bpp == 1 */
      {
         for (y = y0; y < y1; y++)
         {
            const uint8_t *p = data + off + (size_t)y * w;
            uint32_t      *o = out + (size_t)y * w;
            for (x = 0; x < w; x++)
               o[x] = lut[p[x]];
         }
      }
      return;
   }

   /* Fast path: 8-bit byte-aligned channels (A8R8G8B8 / X8R8G8B8 /
    * R8G8B8).  The whole texel is a byte permutation - no shifts, no
    * scale, no per-channel masking.  This is the overwhelmingly common
    * uncompressed layout, so it carries the throughput of the path. */
   if (m->shuffle)
   {
      unsigned sr = m->sh_r, sg = m->sh_g, sb = m->sh_b;
      int      have_a = (m->sh_a != 0xFF);
      unsigned sa = have_a ? m->sh_a : 0;

      for (y = y0; y < y1; y++)
      {
         const uint8_t *p  = data + off + (size_t)y * w * bpp;
         uint32_t      *o  = out + (size_t)y * w;
         for (x = 0; x < w; x++, p += bpp)
         {
            unsigned r = p[sr], g = p[sg], b = p[sb];
            unsigned a = have_a ? p[sa] : 255u;
            o[x] = rdds_pack(r, g, b, a, supports_rgba);
         }
      }
      return;
   }

   for (y = y0; y < y1; y++)
   {
      const uint8_t *rowp = data + off + (size_t)y * w * bpp;
      uint32_t      *o    = out + (size_t)y * w;
      for (x = 0; x < w; x++)
      {
         const uint8_t *p = rowp + (size_t)x * bpp;
         uint32_t px;
         unsigned r, g, b, a;
         switch (bpp)
         {
            case 4:  px = rdds_rd32(p); break;
            case 3:  px = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
                                         | ((uint32_t)p[2] << 16); break;
            case 2:  px = (uint32_t)p[0] | ((uint32_t)p[1] << 8); break;
            default: px = (uint32_t)p[0]; break;
         }
         r = rdds_scale_to_8((px >> rs) & rmax, rb);
         if (m->lum)
            g = b = r;
         else
         {
            g = rdds_scale_to_8((px >> gs) & gmax, gb);
            b = rdds_scale_to_8((px >> bs) & bmax, bb);
         }
         a = ab ? rdds_scale_to_8((px >> as) & amax, ab) : 255u;
         o[x] = rdds_pack(r, g, b, a, supports_rgba);
      }
   }
}

/* BC6H HDR path: two passes over the compressed blocks (no large float
 * scratch).  Pass 1 finds the peak channel value; pass 2 tone-maps (only
 * if the image exceeds 1.0) and packs to the 8bpp surface.  Both are
 * sliced by block row, so the peak has to be accumulated across calls
 * in the handle rather than held in a local - pass 2 cannot start until
 * pass 1 has seen every block. */
static void rdds_rows_bc6h_scan(const rdds_info_t *info,
      const uint8_t *data, unsigned by0, unsigned by1, float *maxc)
{
   float     f[48];
   unsigned  blocks_x  = (info->width + 3u) >> 2;
   int       is_signed = (info->fmt == RDDS_FMT_BC6H_SF16) ? 1 : 0;
   unsigned  bx, by, ty, tx;
   /* Accumulate in a local and store once: 'data' is a uint8_t pointer
    * and may alias *maxc as far as the compiler knows, so updating
    * through the pointer reloads it across the whole inner loop.  That
    * measured ~7% of the BC6H path. */
   float     peak      = *maxc;

   for (by = by0; by < by1; by++)
   {
      for (bx = 0; bx < blocks_x; bx++)
      {
         const uint8_t *src = data + info->data_offset
                            + ((size_t)by * blocks_x + bx) * 16u;
         rdds_bcdec_bc6h_float(src, f, 12, is_signed);
         for (ty = 0; ty < 4; ty++)
         {
            unsigned py = by * 4u + ty;
            if (py >= info->height)
               break;
            for (tx = 0; tx < 4; tx++)
            {
               unsigned px = bx * 4u + tx;
               int      k;
               if (px >= info->width)
                  continue;
               for (k = 0; k < 3; k++)
               {
                  float c = rdds_sanitize(f[(ty * 4 + tx) * 3 + k]);
                  if (c > peak)
                     peak = c;
               }
            }
         }
      }
   }

   *maxc = peak;
}

/* Pass 2: tone-map (if HDR) + quantize + pack, block rows [by0, by1). */
static void rdds_rows_bc6h_pack(const rdds_info_t *info,
      const uint8_t *data, uint32_t *out, unsigned by0, unsigned by1,
      int hdr, float inv_white2, bool supports_rgba)
{
   float     f[48];
   unsigned  blocks_x  = (info->width + 3u) >> 2;
   int       is_signed = (info->fmt == RDDS_FMT_BC6H_SF16) ? 1 : 0;
   unsigned  bx, by, ty, tx;

   for (by = by0; by < by1; by++)
   {
      for (bx = 0; bx < blocks_x; bx++)
      {
         const uint8_t *src = data + info->data_offset
                            + ((size_t)by * blocks_x + bx) * 16u;
         rdds_bcdec_bc6h_float(src, f, 12, is_signed);
         for (ty = 0; ty < 4; ty++)
         {
            unsigned py = by * 4u + ty;
            if (py >= info->height)
               break;
            for (tx = 0; tx < 4; tx++)
            {
               unsigned px = bx * 4u + tx;
               unsigned i3;
               float    cr, cg, cb;
               if (px >= info->width)
                  continue;
               i3 = (ty * 4 + tx) * 3;
               cr = rdds_sanitize(f[i3 + 0]);
               cg = rdds_sanitize(f[i3 + 1]);
               cb = rdds_sanitize(f[i3 + 2]);
               if (hdr)
               {
                  cr = rdds_tonemap(cr, inv_white2);
                  cg = rdds_tonemap(cg, inv_white2);
                  cb = rdds_tonemap(cb, inv_white2);
               }
               out[py * info->width + px] = rdds_pack(
                     rdds_float_to_u8(cr), rdds_float_to_u8(cg),
                     rdds_float_to_u8(cb), 255u, supports_rgba);
            }
         }
      }
   }
}

/* Bytes of mip 0 payload the surface needs, 0 if the format is not
 * recognised (rdds_parse_header has already rejected that case). */
static size_t rdds_payload_needed(const rdds_info_t *info)
{
   unsigned blocks_x, blocks_y, block_size;

   if (info->fmt == RDDS_FMT_RGBA)
      return (size_t)info->width * (size_t)info->height
           * (size_t)(info->rgb_bits >> 3);

   blocks_x   = (info->width  + 3u) >> 2;
   blocks_y   = (info->height + 3u) >> 2;
   block_size = (info->fmt == RDDS_FMT_BC1
              || info->fmt == RDDS_FMT_BC4) ? 8u : 16u;
   return (size_t)blocks_x * (size_t)blocks_y * (size_t)block_size;
}

/* Abandon an in-flight sliced decode, freeing the partial surface.
 * The END path clears output_image first, transferring ownership to
 * the caller, so this only ever frees a surface nobody received. */
static void rdds_proc_reset(rdds_t *rdds)
{
   if (rdds->phase != RDDS_PHASE_IDLE)
   {
      free(rdds->output_image);
      rdds->output_image = NULL;
   }
   free(rdds->lut);
   rdds->lut    = NULL;
   rdds->phase  = RDDS_PHASE_IDLE;
   rdds->cursor = 0;
}

/* Parse, validate and allocate; leaves the handle ready to produce
 * rows.  Returns false on any error, having freed nothing the caller
 * owns. */
static bool rdds_begin(rdds_t *rdds, size_t size, bool supports_rgba)
{
   rdds_info_t *info = &rdds->info;
   size_t       needed, texels_per_row, step;
   int          is_bc6h;

   if (!rdds_parse_header(rdds->buff_data, size, info))
      return false;

   needed = rdds_payload_needed(info);
   if (size < info->data_offset || (size - info->data_offset) < needed)
      return false;

   rdds->output_image = (uint32_t*)malloc((size_t)info->width
         * (size_t)info->height * sizeof(uint32_t));
   if (!rdds->output_image)
      return false;

   is_bc6h = (info->fmt == RDDS_FMT_BC6H_UF16
           || info->fmt == RDDS_FMT_BC6H_SF16) ? 1 : 0;

   /* Row granularity: a block row covers four texel rows, a pixel row
    * one.  size_t throughout - width alone can approach the addressable
    * limit on a surface only one texel tall, so width * 4 would wrap in
    * unsigned. */
   if (info->fmt == RDDS_FMT_RGBA)
   {
      rdds_masks_init(info, &rdds->m);
      rdds->rows_total = info->height;
      texels_per_row   = (size_t)info->width;
      /* 8/16bpp: a build-once packed-value table turns the row loop into
       * a single load per texel.  Skip it for the 24/32bpp shuffle path
       * (a 32-bit table would be 16 GiB) and tolerate OOM by falling
       * back to the scalar decode. */
      if (rdds->m.bpp <= 2)
         rdds->lut = rdds_build_lut(&rdds->m, supports_rgba);
   }
   else
   {
      rdds->rows_total = (info->height + 3u) >> 2;
      texels_per_row   = (size_t)info->width * 4u;
   }

   step = (size_t)RDDS_TEXELS_PER_CALL / texels_per_row;
   if (step == 0)
      step = 1;
   if (step > rdds->rows_total)
      step = rdds->rows_total;
   rdds->rows_step  = (unsigned)step;

   rdds->len        = size;
   rdds->swap_rb    = supports_rgba ? 1 : 0;
   rdds->cursor     = 0;
   rdds->maxc       = 0.0f;
   rdds->inv_white2 = 0.0f;
   rdds->hdr        = 0;
   rdds->phase      = is_bc6h ? RDDS_PHASE_SCAN : RDDS_PHASE_ROWS;
   return true;
}

int rdds_process_image(rdds_t *rdds, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   unsigned end;

   if (buf_data)
      *buf_data = NULL;

   if (!rdds || !rdds->buff_data || !buf_data)
      return IMAGE_PROCESS_ERROR;

   if (rdds->phase == RDDS_PHASE_IDLE)
   {
      if (size > (size_t)INT_MAX)
         return IMAGE_PROCESS_ERROR;
      if (!rdds_begin(rdds, size, supports_rgba))
      {
         rdds_proc_reset(rdds);
         return IMAGE_PROCESS_ERROR;
      }
      *width  = rdds->info.width;
      *height = rdds->info.height;
      return IMAGE_PROCESS_NEXT;
   }

   *width  = rdds->info.width;
   *height = rdds->info.height;

   end = rdds->cursor + rdds->rows_step;
   if (end > rdds->rows_total)
      end = rdds->rows_total;

   if (rdds->phase == RDDS_PHASE_SCAN)
   {
      rdds_rows_bc6h_scan(&rdds->info, rdds->buff_data,
            rdds->cursor, end, &rdds->maxc);
      rdds->cursor = end;
      if (rdds->cursor >= rdds->rows_total)
      {
         rdds->hdr        = (rdds->maxc > 1.0f) ? 1 : 0;
         rdds->inv_white2 = rdds->hdr
                          ? (1.0f / (rdds->maxc * rdds->maxc)) : 0.0f;
         rdds->cursor     = 0;
         rdds->phase      = RDDS_PHASE_ROWS;
      }
      return IMAGE_PROCESS_NEXT;
   }

   switch (rdds->info.fmt)
   {
      case RDDS_FMT_RGBA:
         rdds_rows_uncompressed(&rdds->info, &rdds->m, rdds->buff_data,
               rdds->output_image, rdds->cursor, end,
               rdds->swap_rb ? true : false, rdds->lut);
         break;
      case RDDS_FMT_BC6H_UF16:
      case RDDS_FMT_BC6H_SF16:
         rdds_rows_bc6h_pack(&rdds->info, rdds->buff_data,
               rdds->output_image, rdds->cursor, end,
               rdds->hdr, rdds->inv_white2,
               rdds->swap_rb ? true : false);
         break;
      default:
         rdds_rows_compressed(&rdds->info, rdds->buff_data,
               rdds->output_image, rdds->cursor, end,
               rdds->swap_rb ? true : false);
         break;
   }

   rdds->cursor = end;
   if (rdds->cursor < rdds->rows_total)
      return IMAGE_PROCESS_NEXT;

   *buf_data          = rdds->output_image;
   rdds->output_image = NULL;   /* ownership -> caller */
   free(rdds->lut);             /* table was decode-scoped */
   rdds->lut          = NULL;
   rdds->phase        = RDDS_PHASE_IDLE;
   rdds->cursor       = 0;
   return IMAGE_PROCESS_END;
}

/* Report the BCn mip layout for direct GPU upload without decoding.
 * Only BC1/BC2/BC3/BC7 (which the GPU samples the same way the CPU path
 * produces them) are offered; DXT2/DXT4 (premultiplied), BC4/BC5 (channel
 * swizzle), BC6H (HDR tone-map) and uncompressed DDS return false and go
 * through the normal decode-to-RGBA8 path. */
bool rdds_get_gpu_layout(rdds_t *rdds, size_t len, struct image_gpu_layout *out)
{
   rdds_info_t info;
   enum texture_gpu_format gfmt;
   unsigned block_size, count, i, w, h;
   size_t   off;

   if (!rdds || !rdds->buff_data || !out)
      return false;
   if (len < 128 || len > (size_t)INT_MAX)
      return false;
   if (!rdds_parse_header(rdds->buff_data, len, &info))
      return false;

   switch (info.fmt)
   {
      case RDDS_FMT_BC1: gfmt = TEXTURE_GPU_FORMAT_BC1; block_size = 8;  break;
      case RDDS_FMT_BC2: gfmt = TEXTURE_GPU_FORMAT_BC2; block_size = 16; break;
      case RDDS_FMT_BC3: gfmt = TEXTURE_GPU_FORMAT_BC3; block_size = 16; break;
      case RDDS_FMT_BC7: gfmt = TEXTURE_GPU_FORMAT_BC7; block_size = 16; break;
      default:
         return false;
   }

   /* dwMipMapCount lives at header offset 28; 0 means a single level. */
   count = rdds_rd32(rdds->buff_data + 28);
   if (count == 0)
      count = 1;
   if (count > IMAGE_MAX_MIPS)
      count = IMAGE_MAX_MIPS;

   w             = info.width;
   h             = info.height;
   off           = info.data_offset;
   out->format   = gfmt;
   out->num_mips = 0;

   for (i = 0; i < count; i++)
   {
      unsigned bx   = (w + 3u) >> 2;
      unsigned by   = (h + 3u) >> 2;
      size_t   size = (size_t)bx * (size_t)by * (size_t)block_size;

      if (off + size > len) /* truncated or partial chain: stop here */
         break;

      out->width[i]  = w;
      out->height[i] = h;
      out->offset[i] = off;
      out->size[i]   = size;
      out->num_mips  = i + 1;

      off += size;
      if (w == 1 && h == 1)
         break;
      w = (w > 1) ? (w >> 1) : 1u;
      h = (h > 1) ? (h >> 1) : 1u;
   }

   return out->num_mips > 0;
}

bool rdds_set_buf_ptr(rdds_t *rdds, void *data)
{
   if (!rdds)
      return false;
   /* Repointing the handle invalidates any decode still in flight. */
   rdds_proc_reset(rdds);
   rdds->buff_data = (uint8_t*)data;
   return true;
}

void rdds_free(rdds_t *rdds)
{
   /* A completed decode has handed its buffer to the caller, who owns
    * it; an abandoned one still holds a partial surface that would
    * otherwise leak, which rdds_proc_reset releases. */
   if (!rdds)
      return;
   rdds_proc_reset(rdds);
   free(rdds);
}

rdds_t *rdds_alloc(void)
{
   rdds_t *rdds = (rdds_t*)calloc(1, sizeof(*rdds));
   if (!rdds)
      return NULL;
   return rdds;
}
