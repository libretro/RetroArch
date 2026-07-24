/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (lrc_hash.c).
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

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <emmintrin.h>
#endif
#endif

#include <lrc_hash.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <streams/file_stream.h>

#define LSL32(x, n) ((uint32_t)(x) << (n))
#define LSR32(x, n) ((uint32_t)(x) >> (n))
#define ROR32(x, n) (LSR32(x, n) | LSL32(x, 32 - (n)))

/* First 32 bits of the fractional parts of the square roots of the first 8 primes 2..19 */
static const uint32_t T_H[8] = {
   0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

/* First 32 bits of the fractional parts of the cube roots of the first 64 primes 2..311 */
static const uint32_t T_K[64] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

/* SHA256 implementation from bSNES. Written by valditx. */

struct sha256_ctx
{
   union
   {
      uint8_t u8[64];
      uint32_t u32[16];
   } in;
   unsigned inlen;

   uint32_t w[64];
   uint32_t h[8];
   uint64_t len;
};

static void sha256_init(struct sha256_ctx *p)
{
   memset(p, 0, sizeof(struct sha256_ctx));
   memcpy(p->h, T_H, sizeof(T_H));
}

static void sha256_block(struct sha256_ctx *p)
{
   unsigned i;
   uint32_t s0, s1;
   uint32_t a, b, c, d, e, f, g, h;

   for (i = 0; i < 16; i++)
      p->w[i] = load32be(p->in.u32 + i);

   for (i = 16; i < 64; i++)
   {
      s0 = ROR32(p->w[i - 15],  7) ^ ROR32(p->w[i - 15], 18) ^ LSR32(p->w[i - 15],  3);
      s1 = ROR32(p->w[i -  2], 17) ^ ROR32(p->w[i -  2], 19) ^ LSR32(p->w[i -  2], 10);
      p->w[i] = p->w[i - 16] + s0 + p->w[i - 7] + s1;
   }

   a = p->h[0]; b = p->h[1]; c = p->h[2]; d = p->h[3];
   e = p->h[4]; f = p->h[5]; g = p->h[6]; h = p->h[7];

   for (i = 0; i < 64; i++)
   {
      uint32_t t1, t2, maj, ch;

      s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
      maj = (a & b) ^ (a & c) ^ (b & c);
      t2  = s0 + maj;
      s1  = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
      ch  = (e & f) ^ (~e & g);
      t1  = h + s1 + ch + T_K[i] + p->w[i];

      h   = g;
      g   = f;
      f   = e;
      e   = d + t1;
      d   = c;
      c   = b;
      b   = a;
      a   = t1 + t2;
   }

   p->h[0] += a; p->h[1] += b; p->h[2] += c; p->h[3] += d;
   p->h[4] += e; p->h[5] += f; p->h[6] += g; p->h[7] += h;

   /* Next block */
   p->inlen = 0;
}

static void sha256_chunk(struct sha256_ctx *p,
      const uint8_t *s, size_t len)
{
   p->len += len;

   while (len)
   {
      size_t l   = 64 - p->inlen;

      if (len < l)
         l       = len;

      memcpy(p->in.u8 + p->inlen, s, l);

      s         += l;
      p->inlen  += l;
      len       -= l;

      if (p->inlen == 64)
         sha256_block(p);
   }
}

static void sha256_final(struct sha256_ctx *p)
{
   uint64_t len;
   p->in.u8[p->inlen++] = 0x80;

   if (p->inlen > 56)
   {
      memset(p->in.u8 + p->inlen, 0, 64 - p->inlen);
      sha256_block(p);
   }

   memset(p->in.u8 + p->inlen, 0, 56 - p->inlen);

   len = p->len << 3;
   store32be(p->in.u32 + 14, (uint32_t)(len >> 32));
   store32be(p->in.u32 + 15, (uint32_t)len);
   sha256_block(p);
}

static void sha256_subhash(struct sha256_ctx *p, uint32_t *t)
{
   unsigned i;
   for (i = 0; i < 8; i++)
      store32be(t++, p->h[i]);
}

/**
 * sha256_hash:
 * @s                 : Output.
 * @in                : Input.
 * @size              : Size of @s.
 *
 * Hashes SHA256 and outputs a human readable string.
 **/
void sha256_hash(char *s, const uint8_t *in, size_t len)
{
   unsigned i;
   struct sha256_ctx sha;

   union
   {
      uint32_t u32[8];
      uint8_t u8[32];
   } shahash;

   sha256_init(&sha);
   sha256_chunk(&sha, in, len);
   sha256_final(&sha);
   sha256_subhash(&sha, shahash.u32);

   for (i = 0; i < 32; i++)
      snprintf(s + 2 * i, 3, "%02x", (unsigned)shahash.u8[i]);
}

/* SHA-1 implementation. */

/*
 *  sha1.c
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: sha1.c 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This file implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The Secure Hashing Standard, which uses the Secure Hashing
 *      Algorithm (SHA), produces a 160-bit message digest for a
 *      given data stream.  In theory, it is highly improbable that
 *      two messages will produce the same message digest.  Therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  If the machine word size is larger,
 *      the code should still function properly.  One caveat to that
 *      is that the input functions taking characters and character
 *      arrays assume that only 8 bits of information are stored in each
 *      character.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long. Although SHA-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is a
 *      multiple of the size of an 8-bit character.
 *
 */

/* Define the circular shift macro */
#define SHA1CircularShift(bits,word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))

struct sha1_context
{
   unsigned Message_Digest[5]; /* Message Digest (output)          */

   unsigned Length_Low;        /* Message length in bits           */
   unsigned Length_High;       /* Message length in bits           */

   unsigned char Message_Block[64]; /* 512-bit message blocks      */
   int Message_Block_Index;    /* Index into message block array   */

   int Computed;               /* Is the digest computed?          */
   int Corrupted;              /* Is the message digest corruped?  */
};


static void SHA1Reset(struct sha1_context *context)
{
   if (!context)
      return;

   context->Length_Low             = 0;
   context->Length_High            = 0;
   context->Message_Block_Index    = 0;

   context->Message_Digest[0]      = 0x67452301;
   context->Message_Digest[1]      = 0xEFCDAB89;
   context->Message_Digest[2]      = 0x98BADCFE;
   context->Message_Digest[3]      = 0x10325476;
   context->Message_Digest[4]      = 0xC3D2E1F0;

   context->Computed   = 0;
   context->Corrupted  = 0;
}

static void SHA1ProcessMessageBlock(struct sha1_context *context)
{
   const unsigned K[] =            /* Constants defined in SHA-1   */
   {
      0x5A827999,
      0x6ED9EBA1,
      0x8F1BBCDC,
      0xCA62C1D6
   };
   int         t;                  /* Loop counter                 */
   unsigned    temp;               /* Temporary word value         */
   unsigned    W[80];              /* Word sequence                */
   unsigned    A, B, C, D, E;      /* Word buffers                 */

   /* Initialize the first 16 words in the array W */
   for (t = 0; t < 16; t++)
   {
      W[t] = ((unsigned) context->Message_Block[t * 4]) << 24;
      W[t] |= ((unsigned) context->Message_Block[t * 4 + 1]) << 16;
      W[t] |= ((unsigned) context->Message_Block[t * 4 + 2]) << 8;
      W[t] |= ((unsigned) context->Message_Block[t * 4 + 3]);
   }

   for (t = 16; t < 80; t++)
      W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

   A = context->Message_Digest[0];
   B = context->Message_Digest[1];
   C = context->Message_Digest[2];
   D = context->Message_Digest[3];
   E = context->Message_Digest[4];

   for (t = 0; t < 20; t++)
   {
      temp  =  SHA1CircularShift(5,A) +
         ((B & C) | ((~B) & D)) + E + W[t] + K[0];
      temp &= 0xFFFFFFFF;
      E     = D;
      D     = C;
      C     = SHA1CircularShift(30,B);
      B     = A;
      A     = temp;
   }

   for (t = 20; t < 40; t++)
   {
      temp  = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
      temp &= 0xFFFFFFFF;
      E     = D;
      D     = C;
      C     = SHA1CircularShift(30,B);
      B     = A;
      A     = temp;
   }

   for (t = 40; t < 60; t++)
   {
      temp  = SHA1CircularShift(5,A) +
         ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
      temp &= 0xFFFFFFFF;
      E     = D;
      D     = C;
      C     = SHA1CircularShift(30,B);
      B     = A;
      A     = temp;
   }

   for (t = 60; t < 80; t++)
   {
      temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
      temp &= 0xFFFFFFFF;
      E = D;
      D = C;
      C = SHA1CircularShift(30,B);
      B = A;
      A = temp;
   }

   context->Message_Digest[0] =
      (context->Message_Digest[0] + A) & 0xFFFFFFFF;
   context->Message_Digest[1] =
      (context->Message_Digest[1] + B) & 0xFFFFFFFF;
   context->Message_Digest[2] =
      (context->Message_Digest[2] + C) & 0xFFFFFFFF;
   context->Message_Digest[3] =
      (context->Message_Digest[3] + D) & 0xFFFFFFFF;
   context->Message_Digest[4] =
      (context->Message_Digest[4] + E) & 0xFFFFFFFF;

   context->Message_Block_Index = 0;
}

static void SHA1PadMessage(struct sha1_context *context)
{
   if (!context)
      return;

   /*
    *  Check to see if the current message block is too small to hold
    *  the initial padding bits and length.  If so, we will pad the
    *  block, process it, and then continue padding into a second
    *  block.
    */
   context->Message_Block[context->Message_Block_Index++] = 0x80;

   if (context->Message_Block_Index > 55)
   {
      while (context->Message_Block_Index < 64)
         context->Message_Block[context->Message_Block_Index++] = 0;

      SHA1ProcessMessageBlock(context);
   }

   while (context->Message_Block_Index < 56)
      context->Message_Block[context->Message_Block_Index++] = 0;

   /*  Store the message length as the last 8 octets */
   context->Message_Block[56] = (context->Length_High >> 24) & 0xFF;
   context->Message_Block[57] = (context->Length_High >> 16) & 0xFF;
   context->Message_Block[58] = (context->Length_High >> 8) & 0xFF;
   context->Message_Block[59] = (context->Length_High) & 0xFF;
   context->Message_Block[60] = (context->Length_Low >> 24) & 0xFF;
   context->Message_Block[61] = (context->Length_Low >> 16) & 0xFF;
   context->Message_Block[62] = (context->Length_Low >> 8) & 0xFF;
   context->Message_Block[63] = (context->Length_Low) & 0xFF;

   SHA1ProcessMessageBlock(context);
}

static int SHA1Result(struct sha1_context *context, unsigned char digest[20])
{
   unsigned i;

   if (context->Corrupted)
      return 0;

   if (!context->Computed)
   {
      SHA1PadMessage(context);
      context->Computed = 1;
   }

   if (digest)
   {
      /* Convert Message_Digest to byte array */
      for (i = 0; i < 20; i++)
      {
         digest[i] = (unsigned char)
            ((context->Message_Digest[i>>2] >> 8 * (3 - (i & 0x03))) & 0xFF);
      }
   }

   return 1;
}

static void SHA1Input(struct sha1_context *context,
      const unsigned char *message_array,
      unsigned len)
{
   if (!len)
      return;

   if (context->Computed || context->Corrupted)
   {
      context->Corrupted = 1;
      return;
   }

   while (len-- && !context->Corrupted)
   {
      context->Message_Block[context->Message_Block_Index++] =
         (*message_array & 0xFF);

      context->Length_Low += 8;
      /* Force it to 32 bits */
      context->Length_Low &= 0xFFFFFFFF;
      if (context->Length_Low == 0)
      {
         context->Length_High++;
         /* Force it to 32 bits */
         context->Length_High &= 0xFFFFFFFF;
         if (context->Length_High == 0)
            context->Corrupted = 1; /* Message is too long */
      }

      if (context->Message_Block_Index == 64)
         SHA1ProcessMessageBlock(context);

      message_array++;
   }
}

void SHA1Digest(const uint8_t* data, size_t len, uint8_t digest[20])
{
#ifdef __APPLE__
   CC_SHA1(data, (CC_LONG)len, digest);
#else
   struct sha1_context sha;

   SHA1Reset(&sha);
   SHA1Input(&sha, data, len);

   if (!SHA1Result(&sha, digest))
      memset(digest, 0, 20);
#endif
}

int sha1_calculate(const char *path, char *result)
{
   struct sha1_context sha;
   unsigned char buff[4096];
   int rv    = 1;
   RFILE *fd = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      goto error;

   buff[0] = '\0';

   SHA1Reset(&sha);

   do
   {
      rv = (int)filestream_read(fd, buff, 4096);
      if (rv < 0)
         goto error;

      SHA1Input(&sha, buff, rv);
   } while (rv);

   if (!SHA1Result(&sha, NULL))
      goto error;

   sprintf(result, "%08X%08X%08X%08X%08X",
         sha.Message_Digest[0],
         sha.Message_Digest[1],
         sha.Message_Digest[2],
         sha.Message_Digest[3], sha.Message_Digest[4]);

   filestream_close(fd);
   return 0;

error:
   if (fd)
      filestream_close(fd);
   return -1;
}

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))

#if _MSC_VER
#define DJB2_ALIGN(x) __declspec(align(x))
#else
#define DJB2_ALIGN(x) __attribute__((aligned(x)))
#endif

static const DJB2_ALIGN(16) uint32_t DJB2_W8[8] = {
   0xEC41D4E1, /* 33^7 */
   0x4CFA3CC1, /* 33^6 */
   0x025528A1, /* 33^5 */
   0x00121881, /* 33^4 */
   0x00008C61, /* 33^3 */
   0x00000441, /* 33^2 */
   0x00000021, /* 33^1 */
   0x00000001, /* 33^0 */
};
#endif

uint32_t djb2_calculate(const char *str)
{
   uint32_t h = 5381;
   const unsigned char *p = (const unsigned char*)str;
#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
   __m128i w_lo = _mm_load_si128((const __m128i *)&DJB2_W8[0]);
   __m128i w_hi = _mm_load_si128((const __m128i *)&DJB2_W8[4]);
   size_t len   = strlen((const char*)p);
   const unsigned char *end8 = p + (len & ~(size_t)7);

   while (p < end8)
   {
      uint32_t sum = 0;
      __m128i raw  = _mm_loadl_epi64((const __m128i *)p);
      __m128i zero = _mm_setzero_si128();
      __m128i b16  = _mm_unpacklo_epi8(raw, zero);
      __m128i b_lo = _mm_unpacklo_epi16(b16, zero);
      __m128i b_hi = _mm_unpackhi_epi16(b16, zero);

     /* _mm_mul_epu32 multiplies lanes 0,2 → 64-bit results.
      * Shuffle to access lanes 1,3. */
      __m128i p02_lo = _mm_mul_epu32(b_lo, w_lo);
      __m128i p13_lo = _mm_mul_epu32(_mm_shuffle_epi32(b_lo, 0xF5),
                                     _mm_shuffle_epi32(w_lo, 0xF5));
      __m128i p02_hi = _mm_mul_epu32(b_hi, w_hi);
      __m128i p13_hi = _mm_mul_epu32(_mm_shuffle_epi32(b_hi, 0xF5),
                                     _mm_shuffle_epi32(w_hi, 0xF5));
      sum += (uint32_t)_mm_cvtsi128_si32(p02_lo);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p02_lo, 8));
      sum += (uint32_t)_mm_cvtsi128_si32(p13_lo);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p13_lo, 8));
      sum += (uint32_t)_mm_cvtsi128_si32(p02_hi);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p02_hi, 8));
      sum += (uint32_t)_mm_cvtsi128_si32(p13_hi);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p13_hi, 8));

      h    = h * UINT32_C(0x747C7101) + sum;
      p   += 8;
    }
#endif
    while (*p)
        h = (h << 5) + h + *p++;
    return h;
}
