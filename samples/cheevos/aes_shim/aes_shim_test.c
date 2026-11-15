/* Shared AES adapter: frozen output from the removed implementation's
 * deterministic CBC/CTR corpus, chaining and full-width counter carries. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <lrc_hash.h>

#include "../../../deps/rcheevos/src/rhash/aes.h"

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
   MD5_CTX cbc, ctr;
   unsigned char digest[16];
   char hex[33];
   MD5_Init(&cbc);
   MD5_Init(&ctr);

   for (it = 0; it < 2000; it++)
   {
      size_t   len = 16 * (rnd() % 300);
      size_t   cut = len ? 16 * (rnd() % (len / 16 + 1)) : 0;
      fill(key, 16);
      fill(iv, 16);
      fill(a, len);
      memcpy(b, a, len);
      AES_init_ctx_iv(&got, key, iv);
      AES_init_ctx_iv(&ref, key, iv);
      AES_CBC_decrypt_buffer(&got, a, cut);
      AES_CBC_decrypt_buffer(&got, a + cut, len - cut);
      AES_CBC_decrypt_buffer(&ref, b, len);
      check(!memcmp(a, b, len), "CBC plaintext", it);
      check(!memcmp(got.Iv, ref.Iv, 16), "CBC chaining value", it);
      MD5_Update(&cbc, a, (unsigned long)len);
      MD5_Update(&cbc, got.Iv, 16);
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
      memcpy(ref.Iv, iv, 16);
      for (run = 0; run < 3; run++)
      {
         size_t len = (rnd() & 1) ? rnd() % 1200 : 16 * (rnd() % 75);
         fill(a, len);
         AES_CTR_xcrypt_buffer(&got, a, len);
         {
            size_t blocks = (len + 15) / 16;
            while (blocks--)
            {
               int i;
               for (i = 15; i >= 0; i--)
                  if (++ref.Iv[i])
                     break;
            }
         }
         MD5_Update(&ctr, a, (unsigned long)len);
         MD5_Update(&ctr, got.Iv, 16);
         check(!memcmp(got.Iv, ref.Iv, 16), "CTR counter", it);
      }
   }

   MD5_Final(digest, &cbc);
   hash_digest_to_hex(hex, digest, sizeof(digest));
   check(!strcmp(hex, "70fa5351a7ccc4c7ce35bcd95800aa3d"), "frozen CBC corpus", 0);
   MD5_Final(digest, &ctr);
   hash_digest_to_hex(hex, digest, sizeof(digest));
   check(!strcmp(hex, "133f1d7a8f97dd0130c3cecd91b3db24"), "frozen CTR corpus", 0);
   {
      static const uint8_t zero[16] = {0};
      static const uint8_t encrypted[16] = {
         0x66,0xe9,0x4b,0xd4,0xef,0x8a,0x2c,0x3b,
         0x88,0x4c,0xfa,0x59,0xca,0x34,0x2b,0x2e
      };
      AES_init_ctx(&got, zero);
      AES_ctx_set_iv(&got, zero);
      memcpy(a, encrypted, 16);
      AES_CBC_decrypt_buffer(&got, a, 16);
      check(!memcmp(a, zero, 16), "known AES-128 CBC block", 0);
      AES_ctx_set_iv(&got, zero);
      AES_CTR_xcrypt_buffer(&got, a, 16);
      check(!memcmp(a, encrypted, 16), "known AES-128 CTR block", 0);
   }
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("aes_shim_test: known blocks, frozen CBC/CTR corpus and carries passed\n");
   return 0;
}
