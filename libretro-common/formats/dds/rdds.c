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

/* http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend */
static int rdds_bcdec__extend_sign(int val, int bits) {
    return (val << (32 - bits)) >> (32 - bits);
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

    o.u |= (half & 0x8000) << 16;                           /* sign bit */
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

struct rdds
{
   uint8_t  *buff_data;
   uint32_t *output_image;
};

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

typedef struct
{
   enum rdds_fmt fmt;
   unsigned width;
   unsigned height;
   size_t   data_offset;   /* byte offset of mip 0 pixel data */
   unsigned rgb_bits;      /* uncompressed only            */
   uint32_t rmask, gmask, bmask, amask; /* uncompressed only */
} rdds_info_t;

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
   /* Guard the malloc size (width*height*4) against overflow. */
   if (out->width  > 0x7fffu || out->height > 0x7fffu)
      return false;

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
      out->fmt      = RDDS_FMT_RGBA;
      out->rgb_bits = rdds_rd32(data + 88);
      out->rmask    = rdds_rd32(data + 92);
      out->gmask    = rdds_rd32(data + 96);
      out->bmask    = rdds_rd32(data + 100);
      out->amask    = rdds_rd32(data + 104);
      if (out->rgb_bits != 32 && out->rgb_bits != 24)
         return false; /* keep the uncompressed path modest */
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
   if (bits == 0)
      return 0;
   if (bits >= 8)
      return (v >> (bits - 8)) & 0xffu;
   /* replicate high bits into the low ones */
   return (v << (8 - bits)) | (v >> (bits > (8 - bits) ? (2 * bits - 8) : 0));
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

static uint32_t *rdds_decode_compressed(const rdds_info_t *info,
      const uint8_t *data, size_t len, bool supports_rgba)
{
   uint32_t *out;
   uint8_t   rgba[64];
   unsigned  blocks_x   = (info->width  + 3u) >> 2;
   unsigned  blocks_y   = (info->height + 3u) >> 2;
   unsigned  block_size = (info->fmt == RDDS_FMT_BC1
                        || info->fmt == RDDS_FMT_BC4) ? 8u : 16u;
   size_t    needed     = (size_t)blocks_x * (size_t)blocks_y
                        * (size_t)block_size;
   unsigned  bx, by, ty, tx;
   int       premul     = (info->fmt == RDDS_FMT_BC2_PM
                        || info->fmt == RDDS_FMT_BC3_PM) ? 1 : 0;

   if (len < info->data_offset || (len - info->data_offset) < needed)
      return NULL;

   out = (uint32_t*)malloc((size_t)info->width * (size_t)info->height
         * sizeof(uint32_t));
   if (!out)
      return NULL;

   for (by = 0; by < blocks_y; by++)
   {
      for (bx = 0; bx < blocks_x; bx++)
      {
         const uint8_t *src = data + info->data_offset
                            + ((size_t)by * blocks_x + bx) * block_size;
         rdds_decode_block(info->fmt, src, rgba);

         for (ty = 0; ty < 4; ty++)
         {
            unsigned py = by * 4u + ty;
            if (py >= info->height)
               break;
            for (tx = 0; tx < 4; tx++)
            {
               unsigned px = bx * 4u + tx;
               const uint8_t *p;
               unsigned r, g, b, a;
               if (px >= info->width)
                  continue;
               p = &rgba[(ty * 4 + tx) * 4];
               r = p[0]; g = p[1]; b = p[2]; a = p[3];
               if (premul)
               {
                  r = rdds_unpremul(r, a);
                  g = rdds_unpremul(g, a);
                  b = rdds_unpremul(b, a);
               }
               out[py * info->width + px] =
                  rdds_pack(r, g, b, a, supports_rgba);
            }
         }
      }
   }

   return out;
}

static uint32_t *rdds_decode_uncompressed(const rdds_info_t *info,
      const uint8_t *data, size_t len, bool supports_rgba)
{
   uint32_t *out;
   unsigned  bpp     = info->rgb_bits >> 3; /* 3 or 4 */
   size_t    needed  = (size_t)info->width * (size_t)info->height * bpp;
   unsigned  rs      = rdds_mask_shift(info->rmask);
   unsigned  gs      = rdds_mask_shift(info->gmask);
   unsigned  bs      = rdds_mask_shift(info->bmask);
   unsigned  as      = rdds_mask_shift(info->amask);
   unsigned  rb      = rdds_mask_bits(info->rmask, rs);
   unsigned  gb      = rdds_mask_bits(info->gmask, gs);
   unsigned  bb      = rdds_mask_bits(info->bmask, bs);
   unsigned  ab      = rdds_mask_bits(info->amask, as);
   unsigned  x, y;

   if (len < info->data_offset || (len - info->data_offset) < needed)
      return NULL;
   /* default masks if the file left them zero (24bpp BGR is common) */
   if (!info->rmask && !info->gmask && !info->bmask)
   {
      bs = 0;  gs = 8;  rs = 16; as = 24;
      bb = gb = rb = 8;
      ab = (bpp == 4) ? 8 : 0;
   }

   out = (uint32_t*)malloc((size_t)info->width * (size_t)info->height
         * sizeof(uint32_t));
   if (!out)
      return NULL;

   for (y = 0; y < info->height; y++)
   {
      for (x = 0; x < info->width; x++)
      {
         const uint8_t *p = data + info->data_offset
                          + ((size_t)y * info->width + x) * bpp;
         uint32_t px = (bpp == 4)
            ? rdds_rd32(p)
            : ((uint32_t)p[0] | ((uint32_t)p[1] << 8)
                              | ((uint32_t)p[2] << 16));
         unsigned r = rdds_scale_to_8((px >> rs) & ((rb < 32) ? ((1u << rb) - 1u) : 0xffffffffu), rb);
         unsigned g = rdds_scale_to_8((px >> gs) & ((gb < 32) ? ((1u << gb) - 1u) : 0xffffffffu), gb);
         unsigned b = rdds_scale_to_8((px >> bs) & ((bb < 32) ? ((1u << bb) - 1u) : 0xffffffffu), bb);
         unsigned a = ab ? rdds_scale_to_8((px >> as) & ((ab < 32) ? ((1u << ab) - 1u) : 0xffffffffu), ab) : 255u;
         out[y * info->width + x] = rdds_pack(r, g, b, a, supports_rgba);
      }
   }

   return out;
}

/* BC6H HDR path: two passes over the compressed blocks (no large float
 * scratch).  Pass 1 finds the peak channel value; pass 2 tone-maps (only
 * if the image exceeds 1.0) and packs to the 8bpp surface. */
static uint32_t *rdds_decode_bc6h(const rdds_info_t *info,
      const uint8_t *data, size_t len, bool supports_rgba)
{
   uint32_t *out;
   float     f[48];
   unsigned  blocks_x  = (info->width  + 3u) >> 2;
   unsigned  blocks_y  = (info->height + 3u) >> 2;
   size_t    needed    = (size_t)blocks_x * (size_t)blocks_y * 16u;
   int       is_signed = (info->fmt == RDDS_FMT_BC6H_SF16) ? 1 : 0;
   unsigned  bx, by, ty, tx;
   float     maxc      = 0.0f;
   float     inv_white2;
   int       hdr;

   if (len < info->data_offset || (len - info->data_offset) < needed)
      return NULL;
   out = (uint32_t*)malloc((size_t)info->width * (size_t)info->height
         * sizeof(uint32_t));
   if (!out)
      return NULL;

   /* Pass 1: peak channel over all in-bounds texels. */
   for (by = 0; by < blocks_y; by++)
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
                  if (c > maxc)
                     maxc = c;
               }
            }
         }
      }
   }

   hdr        = (maxc > 1.0f) ? 1 : 0;
   inv_white2 = hdr ? (1.0f / (maxc * maxc)) : 0.0f;

   /* Pass 2: tone-map (if HDR) + quantize + pack. */
   for (by = 0; by < blocks_y; by++)
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

   return out;
}

int rdds_process_image(rdds_t *rdds, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   rdds_info_t info;

   if (buf_data)
      *buf_data = NULL;

   if (!rdds || !rdds->buff_data || !buf_data)
      return IMAGE_PROCESS_ERROR;
   if (size > (size_t)INT_MAX)
      return IMAGE_PROCESS_ERROR;
   if (!rdds_parse_header(rdds->buff_data, size, &info))
      return IMAGE_PROCESS_ERROR;

   if (info.fmt == RDDS_FMT_RGBA)
      rdds->output_image = rdds_decode_uncompressed(&info,
            rdds->buff_data, size, supports_rgba);
   else if (   info.fmt == RDDS_FMT_BC6H_UF16
            || info.fmt == RDDS_FMT_BC6H_SF16)
      rdds->output_image = rdds_decode_bc6h(&info,
            rdds->buff_data, size, supports_rgba);
   else
      rdds->output_image = rdds_decode_compressed(&info,
            rdds->buff_data, size, supports_rgba);

   if (!rdds->output_image)
      return IMAGE_PROCESS_ERROR;

   *buf_data = rdds->output_image;
   *width    = info.width;
   *height   = info.height;
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
   rdds->buff_data = (uint8_t*)data;
   return true;
}

void rdds_free(rdds_t *rdds)
{
   /* Mirrors rtga_free: the decoded pixel buffer returned through
    * rdds_process_image is owned by the caller; only free the handle. */
   if (!rdds)
      return;
   free(rdds);
}

rdds_t *rdds_alloc(void)
{
   rdds_t *rdds = (rdds_t*)calloc(1, sizeof(*rdds));
   if (!rdds)
      return NULL;
   return rdds;
}
