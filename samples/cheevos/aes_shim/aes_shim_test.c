/* cheevos/cheevos_aes.c serves rcheevos' AES interface on
 * libretro-common's cipher. Against rcheevos' own AES: CBC decryption
 * continued across calls, and CTR from counters whose low words, and
 * the octets above them, are about to carry, in runs that end on and
 * off a block boundary - the counter left after each the same, so the
 * next run goes on from it. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../../../deps/rcheevos/src/rhash/aes.h"

void ref_AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t *key,
      const uint8_t *iv);
void ref_AES_CBC_decrypt_buffer(struct AES_ctx *ctx, uint8_t *buf,
      size_t length);
void ref_AES_CTR_xcrypt_buffer(struct AES_ctx *ctx, uint8_t *buf,
      size_t length);

static uint32_t seed = 12345;
static int      failures;

static uint32_t rnd(void)
{
   seed = seed * 1103515245u + 12345u;
   return seed >> 8;
}

static void fill(uint8_t *p, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      p[i] = (uint8_t)rnd();
}

static void check(int ok, const char *what, unsigned it)
{
   if (!ok)
   {
      printf("FAIL: %s, case %u\n", what, it);
      failures++;
   }
}

int main(void)
{
   static uint8_t a[5000], b[5000];
   struct AES_ctx got, ref;
   uint8_t  key[16], iv[16];
   unsigned it;

   for (it = 0; it < 2000; it++)
   {
      size_t   len = 16 * (rnd() % 300);
      size_t   cut = len ? 16 * (rnd() % (len / 16 + 1)) : 0;
      fill(key, 16);
      fill(iv, 16);
      fill(a, len);
      memcpy(b, a, len);
      AES_init_ctx_iv(&got, key, iv);
      ref_AES_init_ctx_iv(&ref, key, iv);
      AES_CBC_decrypt_buffer(&got, a, cut);
      AES_CBC_decrypt_buffer(&got, a + cut, len - cut);
      ref_AES_CBC_decrypt_buffer(&ref, b, len);
      check(!memcmp(a, b, len), "CBC plaintext", it);
      check(!memcmp(got.Iv, ref.Iv, 16), "CBC chaining value", it);
   }

   for (it = 0; it < 4000; it++)
   {
      unsigned run;
      fill(key, 16);
      fill(iv, 16);
      /* the low word a few blocks from wrapping, sometimes with the
       * octets above it at 0xff too */
      if (it & 1)
      {
         uint32_t lo = 0xffffffffu - (rnd() % 40);
         iv[12] = (uint8_t)(lo >> 24);
         iv[13] = (uint8_t)(lo >> 16);
         iv[14] = (uint8_t)(lo >>  8);
         iv[15] = (uint8_t)lo;
         if (it & 2)
         {
            /* a run of 0xff ending under the low word */
            size_t from = rnd() % 12;
            memset(iv + from, 0xff, 12 - from);
         }
         if (it % 7 == 0)
            memset(iv, 0xff, 12);
      }
      AES_init_ctx_iv(&got, key, iv);
      ref_AES_init_ctx_iv(&ref, key, iv);
      for (run = 0; run < 3; run++)
      {
         size_t len = (rnd() & 1) ? rnd() % 1200 : 16 * (rnd() % 75);
         fill(a, len);
         memcpy(b, a, len);
         AES_CTR_xcrypt_buffer(&got, a, len);
         ref_AES_CTR_xcrypt_buffer(&ref, b, len);
         check(!memcmp(a, b, len), "CTR keystream", it);
         check(!memcmp(got.Iv, ref.Iv, 16), "CTR counter", it);
      }
   }

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("aes_shim_test: CBC and CTR match rcheevos' AES\n");
   return 0;
}
