/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (kdf.c).
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
#include <stdlib.h>
#include <stdio.h>
#include <crypto/kdf.h>
#include <crypto/crypto.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

void hmac_sha256_init(struct hmac_sha256_ctx *ctx,
      const uint8_t *key, size_t key_len)
{
   unsigned i;
   uint8_t  k[64];

   memset(k, 0, sizeof(k));
   if (key_len > 64)
   {
      struct sha256_state s;
      sha256_stream_init(&s, 0);
      sha256_stream_update(&s, key, key_len);
      sha256_stream_final(&s, k);
      crypto_memzero(&s, sizeof(s));
   }
   else
      memcpy(k, key, key_len);

   for (i = 0; i < 64; i++)
      k[i] ^= 0x36;
   sha256_stream_init(&ctx->inner, 0);
   sha256_stream_update(&ctx->inner, k, 64);

   for (i = 0; i < 64; i++)
      k[i] ^= 0x36 ^ 0x5c;
   sha256_stream_init(&ctx->outer, 0);
   sha256_stream_update(&ctx->outer, k, 64);

   crypto_memzero(k, sizeof(k));
}

void hmac_sha256_update(struct hmac_sha256_ctx *ctx,
      const uint8_t *data, size_t len)
{
   sha256_stream_update(&ctx->inner, data, len);
}

void hmac_sha256_final(struct hmac_sha256_ctx *ctx, uint8_t *mac)
{
   uint8_t ih[32];
   sha256_stream_final(&ctx->inner, ih);
   sha256_stream_update(&ctx->outer, ih, 32);
   sha256_stream_final(&ctx->outer, mac);
   crypto_memzero(ih, sizeof(ih));
   crypto_memzero(ctx, sizeof(*ctx));
}

void hmac_sha256(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac)
{
   struct hmac_sha256_ctx ctx;
   hmac_sha256_init(&ctx, key, key_len);
   hmac_sha256_update(&ctx, data, len);
   hmac_sha256_final(&ctx, mac);
}

/* HMAC-SHA1: what Kerberos aes*-cts-hmac-sha1-96 (RFC 3962) keys its
 * checksums and its string-to-key PBKDF2 with. Same shape as the
 * SHA-256 one above. */
void hmac_sha1_init(struct hmac_sha1_ctx *ctx,
      const uint8_t *key, size_t key_len)
{
   unsigned i;
   uint8_t  k[64];

   memset(k, 0, sizeof(k));
   if (key_len > 64)
      SHA1Digest(key, key_len, k);
   else
      memcpy(k, key, key_len);

   for (i = 0; i < 64; i++)
      k[i] ^= 0x36;
   sha1_stream_init(&ctx->inner);
   sha1_stream_update(&ctx->inner, k, 64);

   for (i = 0; i < 64; i++)
      k[i] ^= 0x36 ^ 0x5c;
   sha1_stream_init(&ctx->outer);
   sha1_stream_update(&ctx->outer, k, 64);

   crypto_memzero(k, sizeof(k));
}

void hmac_sha1_update(struct hmac_sha1_ctx *ctx,
      const uint8_t *data, size_t len)
{
   sha1_stream_update(&ctx->inner, data, len);
}

void hmac_sha1_final(struct hmac_sha1_ctx *ctx, uint8_t *mac)
{
   uint8_t ih[20];
   sha1_stream_final(&ctx->inner, ih);
   sha1_stream_update(&ctx->outer, ih, 20);
   sha1_stream_final(&ctx->outer, mac);
   crypto_memzero(ih, sizeof(ih));
   crypto_memzero(ctx, sizeof(*ctx));
}

void hmac_sha1(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac)
{
   struct hmac_sha1_ctx ctx;
   hmac_sha1_init(&ctx, key, key_len);
   hmac_sha1_update(&ctx, data, len);
   hmac_sha1_final(&ctx, mac);
}

int pbkdf2_hmac_sha1(const uint8_t *password, size_t password_len,
      const uint8_t *salt, size_t salt_len, uint32_t iterations,
      uint8_t *out, size_t out_len)
{
   /* as pbkdf2_hmac_sha256: keyed once, state copied per block */
   struct pbkdf2_sha1_work
   {
      struct hmac_sha1_ctx base;
      struct hmac_sha1_ctx ctx;
   } *w;
   uint8_t  u[20];
   uint8_t  t[20];
   uint8_t  ibe[4];
   uint32_t block = 0;
   size_t   done  = 0;

   if (!iterations)
      return -1;
   if (!(w = (struct pbkdf2_sha1_work*)malloc(sizeof(*w))))
      return -1;

   hmac_sha1_init(&w->base, password, password_len);

   while (done < out_len)
   {
      uint32_t i;
      unsigned j;
      size_t   take;

      block++;
      crypto_store32_be(ibe, block);

      memcpy(&w->ctx, &w->base, sizeof(w->ctx));
      hmac_sha1_update(&w->ctx, salt, salt_len);
      hmac_sha1_update(&w->ctx, ibe, 4);
      hmac_sha1_final(&w->ctx, u);
      memcpy(t, u, 20);

      for (i = 1; i < iterations; i++)
      {
         memcpy(&w->ctx, &w->base, sizeof(w->ctx));
         hmac_sha1_update(&w->ctx, u, 20);
         hmac_sha1_final(&w->ctx, u);
         for (j = 0; j < 20; j++)
            t[j] ^= u[j];
      }

      take = out_len - done;
      if (take > 20)
         take = 20;
      memcpy(out + done, t, take);
      done += take;
   }

   crypto_memzero(w, sizeof(*w));
   free(w);
   crypto_memzero(u, sizeof(u));
   crypto_memzero(t, sizeof(t));
   return 0;
}

void hmac_sha384_init(struct hmac_sha384_ctx *ctx,
      const uint8_t *key, size_t key_len)
{
   unsigned i;
   uint8_t  k[128];

   memset(k, 0, sizeof(k));
   if (key_len > 128)
   {
      struct sha512_state s;
      sha512_stream_init(&s, 1);
      sha512_stream_update(&s, key, key_len);
      sha512_stream_final(&s, k);
   }
   else
      memcpy(k, key, key_len);

   for (i = 0; i < 128; i++)
      k[i] ^= 0x36;
   sha512_stream_init(&ctx->inner, 1);
   sha512_stream_update(&ctx->inner, k, 128);

   for (i = 0; i < 128; i++)
      k[i] ^= 0x36 ^ 0x5c;
   sha512_stream_init(&ctx->outer, 1);
   sha512_stream_update(&ctx->outer, k, 128);

   crypto_memzero(k, sizeof(k));
}

void hmac_sha384_update(struct hmac_sha384_ctx *ctx,
      const uint8_t *data, size_t len)
{
   sha512_stream_update(&ctx->inner, data, len);
}

void hmac_sha384_final(struct hmac_sha384_ctx *ctx, uint8_t *mac)
{
   uint8_t ih[48];
   sha512_stream_final(&ctx->inner, ih);
   sha512_stream_update(&ctx->outer, ih, 48);
   sha512_stream_final(&ctx->outer, mac);
   crypto_memzero(ih, sizeof(ih));
   crypto_memzero(ctx, sizeof(*ctx));
}

void hkdf_sha256_extract(const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len, uint8_t *prk)
{
   static const uint8_t zero[32] = { 0 };
   if (!salt || !salt_len)
   {
      salt     = zero;
      salt_len = sizeof(zero);
   }
   hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
}

int hkdf_sha256_expand(const uint8_t *prk, size_t prk_len,
      const uint8_t *info, size_t info_len,
      uint8_t *okm, size_t okm_len)
{
   struct hmac_sha256_ctx ctx;
   uint8_t t[32];
   size_t  tlen = 0;
   size_t  done = 0;
   uint8_t n    = 0;

   if (okm_len > 255 * 32)
      return -1;

   while (done < okm_len)
   {
      size_t take;
      n++;
      hmac_sha256_init(&ctx, prk, prk_len);
      hmac_sha256_update(&ctx, t, tlen);
      hmac_sha256_update(&ctx, info, info_len);
      hmac_sha256_update(&ctx, &n, 1);
      hmac_sha256_final(&ctx, t);
      tlen = 32;

      take = okm_len - done;
      if (take > 32)
         take = 32;
      memcpy(okm + done, t, take);
      done += take;
   }

   crypto_memzero(t, sizeof(t));
   return 0;
}

void hmac_sha384(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac)
{
   struct hmac_sha384_ctx ctx;
   hmac_sha384_init(&ctx, key, key_len);
   hmac_sha384_update(&ctx, data, len);
   hmac_sha384_final(&ctx, mac);
}

/* HKDF over SHA-384, for the TLS 1.3 SHA-384 suite. */
void hkdf_sha384_extract(const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len, uint8_t *prk)
{
   static const uint8_t zero[48] = { 0 };
   if (!salt || !salt_len)
   {
      salt     = zero;
      salt_len = sizeof(zero);
   }
   hmac_sha384(salt, salt_len, ikm, ikm_len, prk);
}

int hkdf_sha384_expand(const uint8_t *prk, size_t prk_len,
      const uint8_t *info, size_t info_len,
      uint8_t *okm, size_t okm_len)
{
   struct hmac_sha384_ctx ctx;
   uint8_t t[48];
   size_t  tlen = 0;
   size_t  done = 0;
   uint8_t n    = 0;

   if (okm_len > 255 * 48)
      return -1;

   while (done < okm_len)
   {
      size_t take;
      n++;
      hmac_sha384_init(&ctx, prk, prk_len);
      hmac_sha384_update(&ctx, t, tlen);
      hmac_sha384_update(&ctx, info, info_len);
      hmac_sha384_update(&ctx, &n, 1);
      hmac_sha384_final(&ctx, t);
      tlen = 48;

      take = okm_len - done;
      if (take > 48)
         take = 48;
      memcpy(okm + done, t, take);
      done += take;
   }

   crypto_memzero(t, sizeof(t));
   return 0;
}

int hkdf_sha256(const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len,
      const uint8_t *info, size_t info_len,
      uint8_t *okm, size_t okm_len)
{
   uint8_t prk[32];
   int ret;
   hkdf_sha256_extract(salt, salt_len, ikm, ikm_len, prk);
   ret = hkdf_sha256_expand(prk, sizeof(prk), info, info_len, okm, okm_len);
   crypto_memzero(prk, sizeof(prk));
   return ret;
}

int pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
      const uint8_t *salt, size_t salt_len, uint32_t iterations,
      uint8_t *out, size_t out_len)
{
   /* The password-keyed HMAC is the same for every block and every
    * iteration; key it once and copy the keyed state. Two HMAC
    * contexts are 1.5 KiB, most of a small console thread stack, so
    * they live on the heap for the duration - a single allocation
    * against thousands of compression rounds. */
   struct pbkdf2_work
   {
      struct hmac_sha256_ctx base;
      struct hmac_sha256_ctx ctx;
   } *w;
   uint8_t  u[32];
   uint8_t  t[32];
   uint8_t  ibe[4];
   uint32_t block = 0;
   size_t   done  = 0;

   if (!iterations)
      return -1;
   if (!(w = (struct pbkdf2_work*)malloc(sizeof(*w))))
      return -1;

   hmac_sha256_init(&w->base, password, password_len);

   while (done < out_len)
   {
      uint32_t i;
      unsigned j;
      size_t   take;

      block++;
      crypto_store32_be(ibe, block);

      memcpy(&w->ctx, &w->base, sizeof(w->ctx));
      hmac_sha256_update(&w->ctx, salt, salt_len);
      hmac_sha256_update(&w->ctx, ibe, 4);
      hmac_sha256_final(&w->ctx, u);
      memcpy(t, u, 32);

      for (i = 1; i < iterations; i++)
      {
         memcpy(&w->ctx, &w->base, sizeof(w->ctx));
         hmac_sha256_update(&w->ctx, u, 32);
         hmac_sha256_final(&w->ctx, u);
         for (j = 0; j < 32; j++)
            t[j] ^= u[j];
      }

      take = out_len - done;
      if (take > 32)
         take = 32;
      memcpy(out + done, t, take);
      done += take;
   }

   crypto_memzero(w, sizeof(*w));
   free(w);
   crypto_memzero(u, sizeof(u));
   crypto_memzero(t, sizeof(t));
   return 0;
}

#if defined(_XBOX)
#include <xtl.h>
#elif defined(_WIN32) && !defined(__WINRT__)
#include <windows.h>
#elif defined(__WINRT__)
#include <windows.h>
#include <bcrypt.h>
#elif defined(_3DS)
#include <3ds/types.h>
#include <3ds/services/ps.h>
#elif defined(VITA)
#include <psp2/kernel/rng.h>
#elif defined(HAVE_LIBNX)
#include <switch.h>
#elif defined(PSP) || defined(WIIU) || defined(PS2) || defined(GEKKO) || defined(__PS3__) || defined(__PSL1GHT__)
#include <features/features_cpu.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#if defined(__linux__)
#include <sys/syscall.h>
/* Hidden by unistd.h under -ansi; same declaration features_cpu.c
 * carries for the same reason. */
extern long syscall(long number, ...);
#endif
#endif

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
/* Both are advapi32 exports; resolved at run time so that neither the
 * link line nor the oldest SDK the tree targets (_WIN32_WINNT=0x0400)
 * has to know about them. RtlGenRandom (XP and later) first, the
 * CryptoAPI provider as the fallback for NT4 / 9x era hosts. */
typedef BOOLEAN (WINAPI *rtl_gen_random_t)(PVOID, ULONG);
typedef BOOL (WINAPI *crypt_acquire_context_t)(void**, const char*, const char*, DWORD, DWORD);
typedef BOOL (WINAPI *crypt_gen_random_t)(void*, DWORD, BYTE*);
typedef BOOL (WINAPI *crypt_release_context_t)(void*, DWORD);

#define CRYPTO_PROV_RSA_FULL     1
#define CRYPTO_CRYPT_VERIFYCONTEXT 0xF0000000
#define CRYPTO_CRYPT_SILENT      0x00000040

static int crypto_random_win32(uint8_t *buf, size_t len)
{
   HMODULE advapi = GetModuleHandleA("advapi32.dll");
   rtl_gen_random_t gen;
   crypt_acquire_context_t acquire;
   crypt_gen_random_t      cgen;
   crypt_release_context_t release;
   void *prov = NULL;
   int   ret  = -1;

   if (!advapi)
      advapi = LoadLibraryA("advapi32.dll");
   if (!advapi)
      return -1;

   gen = (rtl_gen_random_t)GetProcAddress(advapi, "SystemFunction036");
   if (gen)
   {
      /* ULONG per call; loop for the pathological caller. */
      while (len)
      {
         ULONG chunk = (len > 0x7fffffff) ? 0x7fffffff : (ULONG)len;
         if (!gen(buf, chunk))
            break;
         buf += chunk;
         len -= chunk;
      }
      if (!len)
         return 0;
   }

   acquire = (crypt_acquire_context_t)GetProcAddress(advapi, "CryptAcquireContextA");
   cgen    = (crypt_gen_random_t)GetProcAddress(advapi, "CryptGenRandom");
   release = (crypt_release_context_t)GetProcAddress(advapi, "CryptReleaseContext");
   if (!acquire || !cgen || !release)
      return -1;

   if (!acquire(&prov, NULL, NULL, CRYPTO_PROV_RSA_FULL,
            CRYPTO_CRYPT_VERIFYCONTEXT | CRYPTO_CRYPT_SILENT))
      return -1;
   if (cgen(prov, (DWORD)len, buf))
      ret = 0;
   release(prov, 0);
   return ret;
}
#endif

int crypto_random_bytes(uint8_t *buf, size_t len)
{
   if (!len)
      return 0;

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
   return crypto_random_win32(buf, len);
#elif defined(__WINRT__)
   return (BCryptGenRandom(NULL, buf, (ULONG)len,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) ? 0 : -1;
#elif defined(_3DS)
   PS_GenerateRandomBytes(buf, len);
   return 0;
#elif defined(VITA)
   while (len)
   {
      size_t chunk = (len > 64) ? 64 : len;
      if (sceKernelGetRandomNumber(buf, chunk) < 0)
         return -1;
      buf += chunk;
      len -= chunk;
   }
   return 0;
#elif defined(HAVE_LIBNX)
   randomGet(buf, len);
   return 0;
#elif defined(PSP) || defined(WIIU) || defined(PS2) || defined(GEKKO) || defined(__PS3__) || defined(__PSL1GHT__) || defined(_XBOX)
   /* No kernel RNG on these; stir the clock, a call counter and two
    * stack/heap addresses through HKDF. Weak, and the only thing
    * available. The keychain does not rest on it: its nonces only
    * have to be unique per install, which a counter-mixed clock is. */
   {
      static uint32_t calls = 0;
      uint8_t  seed[32];
      uint64_t t;
#if defined(_XBOX)
      t = (uint64_t)GetTickCount();
#else
      t = (uint64_t)cpu_features_get_time_usec();
#endif
      calls++;
      crypto_store64_le(seed,      t);
      crypto_store32_le(seed +  8, calls);
      crypto_store64_le(seed + 12, (uint64_t)(uintptr_t)buf);
      crypto_store64_le(seed + 20, (uint64_t)(uintptr_t)&t);
      crypto_store32_le(seed + 28, (uint32_t)len);
      return hkdf_sha256(NULL, 0, seed, sizeof(seed),
            (const uint8_t*)"rng", 3, buf, len);
   }
#else
   {
      size_t off = 0;
#if defined(__linux__) && defined(SYS_getrandom)
      /* getrandom() first: works in a chroot or sandbox with no
       * /dev. EINTR is retried, anything else falls through. */
      while (off < len)
      {
         long r = syscall(SYS_getrandom, buf + off, len - off, 0);
         if (r < 0)
         {
            if (errno == EINTR)
               continue;
            break;
         }
         off += (size_t)r;
      }
      if (off == len)
         return 0;
      off = 0;
#endif
      {
         int fd = open("/dev/urandom", O_RDONLY);
         if (fd < 0)
            return -1;
         while (off < len)
         {
            ssize_t r = read(fd, buf + off, len - off);
            if (r < 0)
            {
               if (errno == EINTR)
                  continue;
               break;
            }
            if (r == 0)
               break;
            off += (size_t)r;
         }
         close(fd);
         return (off == len) ? 0 : -1;
      }
   }
#endif
}

static int drbg_mix(struct drbg_ctx *ctx, const uint8_t *fresh, size_t fresh_len,
      const uint8_t *extra, size_t extra_len)
{
   /* new key || nonce = HKDF(salt = old key, ikm = fresh || extra) */
   uint8_t prk[32];
   uint8_t okm[DRBG_KEY_SIZE + DRBG_NONCE_SIZE];
   struct hmac_sha256_ctx h;

   hmac_sha256_init(&h, ctx->key, sizeof(ctx->key));
   hmac_sha256_update(&h, fresh, fresh_len);
   if (extra && extra_len)
      hmac_sha256_update(&h, extra, extra_len);
   hmac_sha256_final(&h, prk);

   if (hkdf_sha256_expand(prk, sizeof(prk),
            (const uint8_t*)"drbg", 4, okm, sizeof(okm)) != 0)
      return -1;

   memcpy(ctx->key,   okm,                 DRBG_KEY_SIZE);
   memcpy(ctx->nonce, okm + DRBG_KEY_SIZE, DRBG_NONCE_SIZE);
   ctx->counter      = 0;
   ctx->since_reseed = 0;
   ctx->seeded       = 1;

   crypto_memzero(prk, sizeof(prk));
   crypto_memzero(okm, sizeof(okm));
   return 0;
}

int drbg_reseed(struct drbg_ctx *ctx, const uint8_t *extra, size_t extra_len)
{
   uint8_t fresh[48];
   int ret;
   if (crypto_random_bytes(fresh, sizeof(fresh)) != 0)
      return -1;
   ret = drbg_mix(ctx, fresh, sizeof(fresh), extra, extra_len);
   crypto_memzero(fresh, sizeof(fresh));
   return ret;
}

int drbg_init(struct drbg_ctx *ctx, const uint8_t *seed, size_t seed_len)
{
   memset(ctx, 0, sizeof(*ctx));
   if (seed)
      return drbg_mix(ctx, seed, seed_len, NULL, 0);
   return drbg_reseed(ctx, NULL, 0);
}

int drbg_generate(struct drbg_ctx *ctx, uint8_t *out, size_t len)
{
   uint8_t block[CHACHA20_BLOCK_SIZE];

   if (!ctx->seeded)
      return -1;
   if (ctx->since_reseed + len > DRBG_RESEED_INTERVAL)
      if (drbg_reseed(ctx, NULL, 0) != 0)
         return -1;

   ctx->since_reseed += len;

   /* First block of every call is the next key (fast key erasure),
    * output starts at the block after it. */
   chacha20_block(ctx->key, ctx->nonce, ctx->counter, block);
   ctx->counter++;
   memcpy(ctx->key, block, DRBG_KEY_SIZE);

   while (len)
   {
      size_t n = (len < CHACHA20_BLOCK_SIZE) ? len : CHACHA20_BLOCK_SIZE;
      chacha20_block(ctx->key, ctx->nonce, ctx->counter, block);
      ctx->counter++;
      memcpy(out, block, n);
      out += n;
      len -= n;
   }

   /* Ratchet again so the key that produced this output is gone. */
   chacha20_block(ctx->key, ctx->nonce, ctx->counter, block);
   ctx->counter++;
   memcpy(ctx->key, block, DRBG_KEY_SIZE);

   crypto_memzero(block, sizeof(block));
   return 0;
}

void drbg_free(struct drbg_ctx *ctx)
{
   crypto_memzero(ctx, sizeof(*ctx));
}


/* ---- MD4 (RFC 1320) ---------------------------------------------- */

#define MD4_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD4_G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define MD4_H(x, y, z) ((x) ^ (y) ^ (z))

static void md4_block(uint32_t *st, const uint8_t *p)
{
   uint32_t x[16];
   uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
   unsigned i;
   for (i = 0; i < 16; i++)
      x[i] = crypto_load32_le(p + 4 * i);

#define R1(a, b, c, d, k, s) a = crypto_rotl32(a + MD4_F(b, c, d) + x[k], s)
#define R2(a, b, c, d, k, s) a = crypto_rotl32(a + MD4_G(b, c, d) + x[k] + 0x5a827999u, s)
#define R3(a, b, c, d, k, s) a = crypto_rotl32(a + MD4_H(b, c, d) + x[k] + 0x6ed9eba1u, s)
   R1(a,b,c,d, 0,3); R1(d,a,b,c, 1,7); R1(c,d,a,b, 2,11); R1(b,c,d,a, 3,19);
   R1(a,b,c,d, 4,3); R1(d,a,b,c, 5,7); R1(c,d,a,b, 6,11); R1(b,c,d,a, 7,19);
   R1(a,b,c,d, 8,3); R1(d,a,b,c, 9,7); R1(c,d,a,b,10,11); R1(b,c,d,a,11,19);
   R1(a,b,c,d,12,3); R1(d,a,b,c,13,7); R1(c,d,a,b,14,11); R1(b,c,d,a,15,19);
   R2(a,b,c,d, 0,3); R2(d,a,b,c, 4,5); R2(c,d,a,b, 8,9); R2(b,c,d,a,12,13);
   R2(a,b,c,d, 1,3); R2(d,a,b,c, 5,5); R2(c,d,a,b, 9,9); R2(b,c,d,a,13,13);
   R2(a,b,c,d, 2,3); R2(d,a,b,c, 6,5); R2(c,d,a,b,10,9); R2(b,c,d,a,14,13);
   R2(a,b,c,d, 3,3); R2(d,a,b,c, 7,5); R2(c,d,a,b,11,9); R2(b,c,d,a,15,13);
   R3(a,b,c,d, 0,3); R3(d,a,b,c, 8,9); R3(c,d,a,b, 4,11); R3(b,c,d,a,12,15);
   R3(a,b,c,d, 2,3); R3(d,a,b,c,10,9); R3(c,d,a,b, 6,11); R3(b,c,d,a,14,15);
   R3(a,b,c,d, 1,3); R3(d,a,b,c, 9,9); R3(c,d,a,b, 5,11); R3(b,c,d,a,13,15);
   R3(a,b,c,d, 3,3); R3(d,a,b,c,11,9); R3(c,d,a,b, 7,11); R3(b,c,d,a,15,15);
#undef R1
#undef R2
#undef R3
   st[0] += a; st[1] += b; st[2] += c; st[3] += d;
   crypto_memzero(x, sizeof(x));
}

void md4(const uint8_t *data, size_t len, uint8_t *digest)
{
   uint32_t st[4];
   uint8_t  tail[128];
   size_t   rem, off = 0, i;
   uint64_t bits = (uint64_t)len * 8;

   st[0] = 0x67452301u; st[1] = 0xefcdab89u;
   st[2] = 0x98badcfeu; st[3] = 0x10325476u;

   while (len - off >= 64)
   {
      md4_block(st, data + off);
      off += 64;
   }
   rem = len - off;
   memset(tail, 0, sizeof(tail));
   memcpy(tail, data + off, rem);
   tail[rem] = 0x80;
   i = (rem < 56) ? 56 : 120;
   crypto_store64_le(tail + i, bits);
   md4_block(st, tail);
   if (i == 120)
      md4_block(st, tail + 64);

   for (i = 0; i < 4; i++)
      crypto_store32_le(digest + 4 * i, st[i]);
   crypto_memzero(tail, sizeof(tail));
   crypto_memzero(st, sizeof(st));
}

/* ---- HMAC-MD5 ----------------------------------------------------- */

void hmac_md5(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac)
{
   MD5_CTX  ctx;
   uint8_t  k[64];
   uint8_t  ih[16];
   unsigned i;

   memset(k, 0, sizeof(k));
   if (key_len > 64)
   {
      MD5_Init(&ctx);
      MD5_Update(&ctx, key, (unsigned long)key_len);
      MD5_Final(k, &ctx);
   }
   else
      memcpy(k, key, key_len);

   for (i = 0; i < 64; i++)
      k[i] ^= 0x36;
   MD5_Init(&ctx);
   MD5_Update(&ctx, k, 64);
   MD5_Update(&ctx, data, (unsigned long)len);
   MD5_Final(ih, &ctx);

   for (i = 0; i < 64; i++)
      k[i] ^= 0x36 ^ 0x5c;
   MD5_Init(&ctx);
   MD5_Update(&ctx, k, 64);
   MD5_Update(&ctx, ih, 16);
   MD5_Final(mac, &ctx);

   crypto_memzero(k, sizeof(k));
   crypto_memzero(ih, sizeof(ih));
   crypto_memzero(&ctx, sizeof(ctx));
}

/* ---- SP 800-108 counter mode ------------------------------------- */

void kbkdf_hmac_sha256(const uint8_t *ki, size_t ki_len,
      const uint8_t *label, size_t label_len,
      const uint8_t *context, size_t context_len,
      uint8_t *out, size_t out_len)
{
   struct hmac_sha256_ctx h;
   uint8_t  be[4];
   uint8_t  block[32];
   uint32_t i    = 0;
   size_t   done = 0;
   const uint8_t zero = 0;

   while (done < out_len)
   {
      size_t take = out_len - done < 32 ? out_len - done : 32;
      i++;
      hmac_sha256_init(&h, ki, ki_len);
      crypto_store32_be(be, i);
      hmac_sha256_update(&h, be, 4);
      hmac_sha256_update(&h, label, label_len);
      hmac_sha256_update(&h, &zero, 1);
      hmac_sha256_update(&h, context, context_len);
      crypto_store32_be(be, (uint32_t)(out_len * 8));
      hmac_sha256_update(&h, be, 4);
      hmac_sha256_final(&h, block);
      memcpy(out + done, block, take);
      done += take;
   }
   crypto_memzero(block, sizeof(block));
}

/* ---- NTOWFv2 ------------------------------------------------------ */

/* UTF-8 to UTF-16LE into a caller buffer of @cap code units;
 * returns the octet count or 0 when it does not fit / is not UTF-8. */
static void crypto_store16_le_pair(uint8_t *p, uint16_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

static size_t ntlm_utf16le(const char *s, uint8_t *out, size_t cap,
      int upper)
{
   size_t n = 0;
   while (*s)
   {
      uint32_t cp = utf8_walk(&s);
      if (cp == 0 || cp > 0x10ffff)
         return 0;
      if (upper && cp >= 'a' && cp <= 'z')
         cp -= 32;
      if (cp >= 0x10000)
      {
         if (n + 2 > cap)
            return 0;
         cp -= 0x10000;
         crypto_store16_le_pair(out + 2 * n, (uint16_t)(0xd800 | (cp >> 10)));
         crypto_store16_le_pair(out + 2 * n + 2, (uint16_t)(0xdc00 | (cp & 0x3ff)));
         n += 2;
      }
      else
      {
         if (n + 1 > cap)
            return 0;
         crypto_store16_le_pair(out + 2 * n, (uint16_t)cp);
         n += 1;
      }
   }
   return n * 2;
}

int ntlm_ntowf_v2(const char *password, const char *user,
      const char *domain, uint8_t *out)
{
   /* 255 code points each, as UTF-16 with surrogates: 1020 octets. */
   uint8_t *buf = (uint8_t*)malloc(2 * 1020);
   uint8_t  nt[16];
   size_t   plen, ulen, dlen;
   if (!buf)
      return -1;

   plen = ntlm_utf16le(password, buf, 510, 0);
   if (!plen && *password)
   {
      free(buf);
      return -1;
   }
   md4(buf, plen, nt);

   ulen = ntlm_utf16le(user, buf, 510, 1);
   dlen = ntlm_utf16le(domain, buf + ulen, 510, 0);
   if ((!ulen && *user) || (!dlen && *domain))
   {
      crypto_memzero(buf, 2 * 1020);
      free(buf);
      return -1;
   }
   hmac_md5(nt, 16, buf, ulen + dlen, out);
   crypto_memzero(nt, sizeof(nt));
   crypto_memzero(buf, 2 * 1020);
   free(buf);
   return 0;
}
