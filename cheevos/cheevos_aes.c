/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* rcheevos' AES interface (deps/rcheevos/src/rhash/aes.h) on
 * libretro-common's cipher, which runs on AES instructions where the
 * CPU has them and decrypts CBC several blocks at a time. The context
 * keeps the 16-octet key where rcheevos keeps its schedule, and its
 * counter as rcheevos does: a 128-bit big-endian number, stepped once
 * per keystream block, a partial block included. */

#include <string.h>

#include <crypto/crypto.h>

#include "../deps/rcheevos/src/rhash/aes.h"

void AES_init_ctx(struct AES_ctx *ctx, const uint8_t key[AES_KEYLEN])
{
   memcpy(ctx->RoundKey, key, AES_KEYLEN);
}

void AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t key[AES_KEYLEN],
      const uint8_t iv[AES_BLOCKLEN])
{
   memcpy(ctx->RoundKey, key, AES_KEYLEN);
   memcpy(ctx->Iv, iv, AES_BLOCKLEN);
}

void AES_ctx_set_iv(struct AES_ctx *ctx, const uint8_t iv[AES_BLOCKLEN])
{
   memcpy(ctx->Iv, iv, AES_BLOCKLEN);
}

void AES_CBC_decrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, size_t length)
{
   struct aes_ctx a;
   aes_init(&a, ctx->RoundKey, AES_KEYLEN);
   aes_cbc_decrypt(&a, ctx->Iv, buf, buf, length);
   crypto_memzero(&a, sizeof(a));
}

void AES_CTR_xcrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, size_t length)
{
   struct aes_ctx a;
   aes_init(&a, ctx->RoundKey, AES_KEYLEN);
   /* aes_ctr_crypt() steps the low 32 bits; runs end where they wrap,
    * and the carry goes on into the octets above */
   while (length)
   {
      uint32_t lo     = ((uint32_t)ctx->Iv[12] << 24)
                      | ((uint32_t)ctx->Iv[13] << 16)
                      | ((uint32_t)ctx->Iv[14] <<  8)
                      |  (uint32_t)ctx->Iv[15];
      uint64_t blocks = ((uint64_t)1 << 32) - lo;
      size_t   n      = length;
      if ((uint64_t)(n / AES_BLOCKLEN) >= blocks)
         n = (size_t)(blocks * AES_BLOCKLEN);
      aes_ctr_crypt(&a, ctx->Iv, buf, buf, n);
      if ((uint64_t)((n + AES_BLOCKLEN - 1) / AES_BLOCKLEN) == blocks)
      {
         int i;
         for (i = 11; i >= 0 && ++ctx->Iv[i] == 0; i--) { }
      }
      buf    += n;
      length -= n;
   }
   crypto_memzero(&a, sizeof(a));
}
