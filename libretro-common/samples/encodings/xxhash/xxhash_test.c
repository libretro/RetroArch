/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xxhash_test.c).
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

/* XXH64 and XXH3 (64-bit) against xxHash's own values: the ones its
 * sanity check holds, over the buffer it fills, and more made with
 * xxHash 0.8.1 over the same buffer, longer, for the lengths around
 * each of XXH3's turns and for many blocks. XXH64 is taken in one call
 * and in pieces of every size; XXH3's long inputs go through each path
 * this CPU can run, which must all agree with each other at every
 * length up to three blocks and at every alignment. XXH3 taken in
 * pieces must be what it is in one call, through each path, for the
 * vectors and for every length to three blocks, split anywhere. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/xxh64.h>

/* the paths themselves, which xxh3_64() chooses between */
#include "../../../encodings/encoding_xxh3.c"

#define PRIME32 2654435761U
#define PRIME64 11400714785074694797ULL
#define BUF_LEN 100000

typedef struct
{
   uint32_t len;
   uint64_t seed;
   uint64_t hash;
} vec_t;

/* XSUM_XXH64_testdata */
static const vec_t xxh64_spec[] = {
   {   0,       0, 0xEF46DB3751D8E999ULL },
   {   0, PRIME32, 0xAC75FDA2929B17EFULL },
   {   1,       0, 0xE934A84ADB052768ULL },
   {   1, PRIME32, 0x5014607643A9B4C3ULL },
   {   4,       0, 0x9136A0DCA57457EEULL },
   {  14,       0, 0x8282DCC4994E35C8ULL },
   {  14, PRIME32, 0xC3BD6BF63DEB6DF0ULL },
   { 222,       0, 0xB641AE8CB691C174ULL },
   { 222, PRIME32, 0x20CB8AB7AE10C14AULL },
   {     31, 0x0000000000000000ULL, 0x299B39A290E6D783ULL },
   {     31, 0x9E3779B185EBCA8DULL, 0x51AAF1A336575F00ULL },
   {     31, 0x0123456789ABCDEFULL, 0x15DF0C5F454CB442ULL },
   {     32, 0x0000000000000000ULL, 0x18B216492BB44B70ULL },
   {     32, 0x9E3779B185EBCA8DULL, 0x21D817283F4B6283ULL },
   {     32, 0x0123456789ABCDEFULL, 0x82335D2BA9AFBF4EULL },
   {     33, 0x0000000000000000ULL, 0x55C8DC3E578F5B59ULL },
   {     33, 0x9E3779B185EBCA8DULL, 0xB09782549294DF85ULL },
   {     33, 0x0123456789ABCDEFULL, 0x28F7AA5CD4F51D46ULL },
   {     63, 0x0000000000000000ULL, 0xA9EFBE0FA0F3F4E7ULL },
   {     63, 0x9E3779B185EBCA8DULL, 0x34C3933DC0040446ULL },
   {     63, 0x0123456789ABCDEFULL, 0x9F2E7DD211D5FD08ULL },
   {     64, 0x0000000000000000ULL, 0xEF558F8ACAC2B5CDULL },
   {     64, 0x9E3779B185EBCA8DULL, 0xF90D26FED8023D61ULL },
   {     64, 0x0123456789ABCDEFULL, 0x9406ECC32D7852ECULL },
   {     65, 0x0000000000000000ULL, 0xDE0F20DC2631AF7AULL },
   {     65, 0x9E3779B185EBCA8DULL, 0x7814174CD6405BEEULL },
   {     65, 0x0123456789ABCDEFULL, 0x94E74DEC4157C995ULL },
   {    100, 0x0000000000000000ULL, 0x4BFE019CD91D9EA4ULL },
   {    100, 0x9E3779B185EBCA8DULL, 0x38F1D4AABFD12D0FULL },
   {    100, 0x0123456789ABCDEFULL, 0x70F40596D4A2F05BULL },
   {   1000, 0x0000000000000000ULL, 0x52BD1358F22E9EF7ULL },
   {   1000, 0x9E3779B185EBCA8DULL, 0x3FE6446814FAE5EDULL },
   {   1000, 0x0123456789ABCDEFULL, 0x6FEBFAD7085386E5ULL },
   {   4097, 0x0000000000000000ULL, 0xF617DE30F1C0ABB6ULL },
   {   4097, 0x9E3779B185EBCA8DULL, 0x7A4E1FBFDD1DDED1ULL },
   {   4097, 0x0123456789ABCDEFULL, 0x0EC301EA1B060486ULL },
   { 100000, 0x0000000000000000ULL, 0x2F2257F45994FF6AULL },
   { 100000, 0x9E3779B185EBCA8DULL, 0xA53C361A45679B06ULL },
   { 100000, 0x0123456789ABCDEFULL, 0x19A6D33D882D5112ULL },
};

/* XSUM_XXH3_testdata */
static const vec_t xxh3_spec[] = {
   {    0,       0, 0x2D06800538D394C2ULL },
   {    0, PRIME64, 0xA8A6B918B2F0364AULL },
   {    1,       0, 0xC44BDFF4074EECDBULL },
   {    1, PRIME64, 0x032BE332DD766EF8ULL },
   {    6,       0, 0x27B56A84CD2D7325ULL },
   {    6, PRIME64, 0x84589C116AB59AB9ULL },
   {   12,       0, 0xA713DAF0DFBB77E7ULL },
   {   12, PRIME64, 0xE7303E1B2336DE0EULL },
   {   24,       0, 0xA3FE70BF9D3510EBULL },
   {   24, PRIME64, 0x850E80FC35BDD690ULL },
   {   48,       0, 0x397DA259ECBA1F11ULL },
   {   48, PRIME64, 0xADC2CBAA44ACC616ULL },
   {   80,       0, 0xBCDEFBBB2C47C90AULL },
   {   80, PRIME64, 0xC6DD0CB699532E73ULL },
   {  195,       0, 0xCD94217EE362EC3AULL },
   {  195, PRIME64, 0xBA68003D370CB3D9ULL },
   {  403,       0, 0xCDEB804D65C6DEA4ULL },
   {  403, PRIME64, 0x6259F6ECFD6443FDULL },
   {  512,       0, 0x617E49599013CB6BULL },
   {  512, PRIME64, 0x3CE457DE14C27708ULL },
   { 2048,       0, 0xDD59E2C3A5F038E0ULL },
   { 2048, PRIME64, 0x66F81670669ABABCULL },
   { 2240,       0, 0x6E73A90539CF2948ULL },
   { 2240, PRIME64, 0x757BA8487D1B5247ULL },
   { 2367,       0, 0xCB37AEB9E5D361EDULL },
   { 2367, PRIME64, 0xD2DB3415B942B42AULL },
   {    129, 0x0000000000000000ULL, 0x98F1B0A679A2CA29ULL },
   {    129, 0x9E3779B185EBCA8DULL, 0x21FFFDBCA099C844ULL },
   {    129, 0x0123456789ABCDEFULL, 0x028E1AF162B1D2A9ULL },
   {    240, 0x0000000000000000ULL, 0x81C3C2B67F568CCFULL },
   {    240, 0x9E3779B185EBCA8DULL, 0xCC0F58C27EF3D8EEULL },
   {    240, 0x0123456789ABCDEFULL, 0xFF9A5EA1F3E336C1ULL },
   {    241, 0x0000000000000000ULL, 0xC5A639ECD2030E5EULL },
   {    241, 0x9E3779B185EBCA8DULL, 0xDDA9B0A161D4829AULL },
   {    241, 0x0123456789ABCDEFULL, 0x39D4913D191D5F3DULL },
   {    255, 0x0000000000000000ULL, 0xE98F979F4ED8A197ULL },
   {    255, 0x9E3779B185EBCA8DULL, 0x2ACA7901D9538C75ULL },
   {    255, 0x0123456789ABCDEFULL, 0x028E72AF5FFADEA6ULL },
   {    256, 0x0000000000000000ULL, 0x55DE574AD89D0AC5ULL },
   {    256, 0x9E3779B185EBCA8DULL, 0x4D30234B7A3AA61CULL },
   {    256, 0x0123456789ABCDEFULL, 0x1D48B7293297B67EULL },
   {   1023, 0x0000000000000000ULL, 0x87A8F7B2F2E22496ULL },
   {   1023, 0x9E3779B185EBCA8DULL, 0x0F0F02DE8590E1B5ULL },
   {   1023, 0x0123456789ABCDEFULL, 0x4AF7B4E4D28BE1B5ULL },
   {   1024, 0x0000000000000000ULL, 0xDD85C9B5C1109C5CULL },
   {   1024, 0x9E3779B185EBCA8DULL, 0xEF368A8A2EBABAEFULL },
   {   1024, 0x0123456789ABCDEFULL, 0x409D0F8806FD35EBULL },
   {   1025, 0x0000000000000000ULL, 0xD870C0FA13211C6AULL },
   {   1025, 0x9E3779B185EBCA8DULL, 0x96792BCF9AF88519ULL },
   {   1025, 0x0123456789ABCDEFULL, 0xB09ED51FDD614F0FULL },
   {   1088, 0x0000000000000000ULL, 0xCC1450EA6B52A8F4ULL },
   {   1088, 0x9E3779B185EBCA8DULL, 0x542632D046F3DDE4ULL },
   {   1088, 0x0123456789ABCDEFULL, 0x2B7F77D992CF45C0ULL },
   {   4103, 0x0000000000000000ULL, 0xD6E778C94923EA44ULL },
   {   4103, 0x9E3779B185EBCA8DULL, 0x917A70D1756179ADULL },
   {   4103, 0x0123456789ABCDEFULL, 0x7E60E72087D382EBULL },
   {  16384, 0x0000000000000000ULL, 0xEDD17B079A94769EULL },
   {  16384, 0x9E3779B185EBCA8DULL, 0x53DD5B3F9786A823ULL },
   {  16384, 0x0123456789ABCDEFULL, 0xA73A83F142BDF3C2ULL },
   { 100000, 0x0000000000000000ULL, 0x34D658192A014311ULL },
   { 100000, 0x9E3779B185EBCA8DULL, 0x0682260A8A5AFE82ULL },
   { 100000, 0x0123456789ABCDEFULL, 0xF39E05B863A8BB36ULL },
};

typedef uint64_t (*path_fn)(const uint8_t *in, size_t len, uint64_t seed);

typedef struct
{
   const char *name;
   path_fn     fn;
   int         id;   /* xxh3_path for it */
} path_t;

static unsigned failures;
static unsigned checks;

static uint64_t rng = 88172645463325252ULL;

static size_t next_rand(size_t below)
{
   rng ^= rng << 13;
   rng ^= rng >> 7;
   rng ^= rng << 17;
   return (size_t)(rng % below);
}

/* @len octets at @in through an xxh3_state_t in pieces: of @step each,
 * or, with @step 0, of random sizes up to two blocks */
static uint64_t xxh3_pieces(const uint8_t *in, size_t len, uint64_t seed,
      size_t step)
{
   static xxh3_state_t st;
   size_t at = 0;
   xxh3_reset(&st, seed);
   xxh3_update(&st, in, 0);
   while (at < len)
   {
      size_t k = step ? step : 1 + next_rand(2 * 1024 + 70);
      if (k > len - at)
         k = len - at;
      xxh3_update(&st, in + at, k);
      at += k;
   }
   return xxh3_digest(&st);
}

static void fail(const char *what, unsigned len, uint64_t seed,
      uint64_t got, uint64_t want)
{
   if (failures++ < 20)
      printf("[FAIL] %s, %u octets, seed %016llX: %016llX, not %016llX\n",
            what, len, (unsigned long long)seed, (unsigned long long)got,
            (unsigned long long)want);
}

int main(void)
{
   uint8_t *buf = (uint8_t*)malloc(BUF_LEN + 8);
   path_t   paths[5];
   unsigned np = 0, i, p;
   uint64_t g  = PRIME32;
   size_t   n;

   if (!buf)
      return 2;
   /* XSUM_fillTestBuffer */
   for (n = 0; n < BUF_LEN; n++)
   {
      buf[n] = (uint8_t)(g >> 56);
      g     *= PRIME64;
   }

   paths[np].name = "scalar";
   paths[np].id   = 1;
   paths[np++].fn = xxh3_long_scalar;
#if defined(XXH3_HAVE_SSE2)
   {
      uint64_t simd = cpu_features_get();
      if (simd & RETRO_SIMD_SSE2)
      {
         paths[np].name = "SSE2";
         paths[np].id   = 2;
         paths[np++].fn = xxh3_long_sse2;
      }
#if defined(XXH3_HAVE_AVX2)
      if (simd & RETRO_SIMD_AVX2)
      {
         paths[np].name = "AVX2";
         paths[np].id   = 3;
         paths[np++].fn = xxh3_long_avx2;
      }
#endif
#if defined(XXH3_HAVE_AVX512)
      if (simd & RETRO_SIMD_AVX512)
      {
         paths[np].name = "AVX-512";
         paths[np].id   = 4;
         paths[np++].fn = xxh3_long_avx512;
      }
#endif
   }
#elif defined(XXH3_HAVE_NEON)
   paths[np].name = "NEON";
   paths[np].id   = 0;
   paths[np++].fn = xxh3_long_neon;
#endif

   /* XXH64: whole, and in pieces */
   for (i = 0; i < sizeof(xxh64_spec) / sizeof(xxh64_spec[0]); i++)
   {
      const vec_t  *v = &xxh64_spec[i];
      uint64_t      h = xxh64(buf, v->len, v->seed);
      xxh64_state_t s;
      size_t        at;
      checks++;
      if (h != v->hash)
         fail("xxh64", v->len, v->seed, h, v->hash);
      for (n = 1; n <= 97; n += 3)
      {
         xxh64_reset(&s, v->seed);
         for (at = 0; at < v->len; at += n)
            xxh64_update(&s, buf + at, at + n > v->len ? v->len - at : n);
         checks++;
         if ((h = xxh64_digest(&s)) != v->hash)
            fail("xxh64 in pieces", v->len, v->seed, h, v->hash);
      }
   }

   /* XXH3: the chosen path, then each */
   for (i = 0; i < sizeof(xxh3_spec) / sizeof(xxh3_spec[0]); i++)
   {
      const vec_t *v = &xxh3_spec[i];
      uint64_t     h = xxh3_64(buf, v->len, v->seed);
      checks++;
      if (h != v->hash)
         fail("xxh3_64", v->len, v->seed, h, v->hash);
      if (v->len <= 240)
         continue;
      for (p = 0; p < np; p++)
      {
         checks++;
         if ((h = paths[p].fn(buf, v->len, v->seed)) != v->hash)
            fail(paths[p].name, v->len, v->seed, h, v->hash);
      }
   }

   /* every length to three blocks, at every alignment, every path as
    * the scalar one */
   for (n = 241; n <= 3 * 1024 + 64; n++)
   {
      unsigned off  = (unsigned)(n & 7);
      uint64_t seed = (n & 1) ? 0 : (uint64_t)n * PRIME64;
      uint64_t want = xxh3_long_scalar(buf + off, n, seed);
      for (p = 1; p < np; p++)
      {
         uint64_t h = paths[p].fn(buf + off, n, seed);
         checks++;
         if (h != want)
            fail(paths[p].name, (unsigned)n, seed, h, want);
      }
   }

   /* XXH3 in pieces, through each path's blocks */
   for (p = 0; p < np; p++)
   {
      static const size_t steps[] = { 1, 7, 63, 64, 65, 100, 240, 241,
         255, 256, 257, 1000, 1023, 1024, 1025, 3000, 0 };
      static const size_t after[] = { 7, 63, 64, 65, 255, 256, 257, 300,
         1000 };
      unsigned lead;
#if defined(XXH3_HAVE_SSE2)
      retro_atomic_store_release_int(&xxh3_path, paths[p].id);
#endif
      for (i = 0; i < sizeof(xxh3_spec) / sizeof(xxh3_spec[0]); i++)
      {
         const vec_t *v = &xxh3_spec[i];
         unsigned     k;
         for (k = 0; k < sizeof(steps) / sizeof(steps[0]); k++)
         {
            uint64_t h = xxh3_pieces(buf, v->len, v->seed, steps[k]);
            checks++;
            if (h != v->hash)
               fail("xxh3 in pieces", v->len, v->seed, h, v->hash);
         }
      }
      /* held octets ending at every place in a stripe, then pieces that
       * are held after them, fill the stripe or go where they lie, and
       * leave every count of octets over */
      for (lead = 1; lead <= 64; lead++)
         for (i = 0; i < sizeof(after) / sizeof(after[0]); i++)
         {
            static xxh3_state_t st;
            uint64_t seed = (lead & 1) ? 0 : (uint64_t)lead * PRIME64;
            size_t   len  = 5000 + lead, at, k;
            uint64_t want = xxh3_64(buf, len, seed), h;
            xxh3_reset(&st, seed);
            xxh3_update(&st, buf, lead);
            for (at = lead; at < len; at += k)
            {
               k = after[i] < len - at ? after[i] : len - at;
               xxh3_update(&st, buf + at, k);
            }
            checks++;
            if ((h = xxh3_digest(&st)) != want)
               fail("xxh3 after a held lead", (unsigned)len, seed, h, want);
         }
      for (n = 0; n <= 3 * 1024 + 64; n++)
      {
         uint64_t seed = (n & 1) ? 0 : (uint64_t)n * PRIME64;
         uint64_t want = xxh3_64(buf + (n & 7), n, seed);
         uint64_t h    = xxh3_pieces(buf + (n & 7), n, seed, 0);
         checks++;
         if (h != want)
            fail("xxh3 split anywhere", (unsigned)n, seed, h, want);
      }
   }

   free(buf);
   printf("xxhash_test: XXH3 through");
   for (p = 0; p < np; p++)
      printf(" %s", paths[p].name);
   printf("; %u checks, %u failed\n", checks, failures);
   return failures ? 1 : 0;
}
