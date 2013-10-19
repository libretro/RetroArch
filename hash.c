/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// SHA256 implementation from bSNES. Written by valditx.
//

#include "general.h"
#include "hash.h"
#include <string.h>
#include <stdio.h>

#define SWAP32(x) ((uint32_t)(           \
         (((uint32_t)(x) & 0x000000ff) << 24) | \
         (((uint32_t)(x) & 0x0000ff00) <<  8) | \
         (((uint32_t)(x) & 0x00ff0000) >>  8) | \
         (((uint32_t)(x) & 0xff000000) >> 24)   \
         ))

static inline void store32le(uint32_t *addr, uint32_t data)
{
   *addr = is_little_endian() ? data : SWAP32(data);
}

static inline void store32be(uint32_t *addr, uint32_t data)
{
   *addr = is_little_endian() ? SWAP32(data) : data;
}

static inline uint32_t load32le(const uint32_t *addr)
{
   return is_little_endian() ? *addr : SWAP32(*addr);
}

static inline uint32_t load32be(const uint32_t *addr)
{
   return is_little_endian() ? SWAP32(*addr) : *addr;
}

#define LSL32(x, n) ((uint32_t)(x) << (n))
#define LSR32(x, n) ((uint32_t)(x) >> (n))
#define ROR32(x, n) (LSR32(x, n) | LSL32(x, 32 - (n)))

// First 32 bits of the fractional parts of the square roots of the first 8 primes 2..19
static const uint32_t T_H[8] = {
   0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

// First 32 bits of the fractional parts of the cube roots of the first 64 primes 2..311
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
   uint32_t t1, t2, maj, ch;

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
      s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
      maj = (a & b) ^ (a & c) ^ (b & c);
      t2 = s0 + maj;
      s1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
      ch = (e & f) ^ (~e & g);
      t1 = h + s1 + ch + T_K[i] + p->w[i];

      h = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
   }

   p->h[0] += a; p->h[1] += b; p->h[2] += c; p->h[3] += d;
   p->h[4] += e; p->h[5] += f; p->h[6] += g; p->h[7] += h;

   // Next block
   p->inlen = 0;
}

static void sha256_chunk(struct sha256_ctx *p, const uint8_t *s, unsigned len) 
{
   unsigned l;
   p->len += len;

   while (len) 
   {
      l = 64 - p->inlen;
      l = (len < l) ? len : l;

      memcpy(p->in.u8 + p->inlen, s, l);
      s += l;
      p->inlen += l;
      len -= l;

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

void sha256_hash(char *out, const uint8_t *in, size_t size)
{
   unsigned i;
   struct sha256_ctx sha;

   union
   {
      uint32_t u32[8];
      uint8_t u8[32];
   } shahash;

   sha256_init(&sha);
   sha256_chunk(&sha, in, size);
   sha256_final(&sha);
   sha256_subhash(&sha, shahash.u32);

   for (i = 0; i < 32; i++)
      snprintf(out + 2 * i, 3, "%02x", (unsigned)shahash.u8[i]);
}

#ifndef HAVE_ZLIB
// Zlib crc32.
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc32_adjust(uint32_t crc32, uint8_t input)
{
   return ((crc32 >> 8) & 0x00ffffff) ^ crc32_table[(crc32 ^ input) & 0xff];
}

uint32_t crc32_calculate(const uint8_t *data, size_t length)
{
   size_t i;
   uint32_t crc32 = ~0;
   for (i = 0; i < length; i++)
      crc32 = crc32_adjust(crc32, data[i]);
   return ~crc32;
}
#endif

