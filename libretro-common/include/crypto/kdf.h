/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (kdf.h).
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

#ifndef _LIBRETRO_CRYPTO_KDF_H
#define _LIBRETRO_CRYPTO_KDF_H

#include <stdint.h>
#include <stddef.h>
#include <lrc_hash.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Keyed hashing and key derivation over SHA-256, plus the DRBG and
 * the platform entropy source it seeds from. */

/* HMAC-SHA256 (RFC 2104) over the streaming SHA-256 in lrc_hash. */

#define HMAC_SHA256_SIZE 32

struct hmac_sha256_ctx
{
   struct sha256_state inner;
   struct sha256_state outer;
};

void hmac_sha256_init(struct hmac_sha256_ctx *ctx,
      const uint8_t *key, size_t key_len);
void hmac_sha256_update(struct hmac_sha256_ctx *ctx,
      const uint8_t *data, size_t len);
/**
 * hmac_sha256_final:
 * @mac               : 32 octets out. The context is cleared.
 **/
void hmac_sha256_final(struct hmac_sha256_ctx *ctx, uint8_t *mac);

/* One-shot. */
void hmac_sha256(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac);

/* HMAC-SHA384, for the TLS PRF of the SHA-384 cipher suites. */
#define HMAC_SHA384_SIZE 48

/* HMAC-SHA1 and PBKDF2-HMAC-SHA1 (RFC 2898), for the Kerberos AES
 * enctypes of RFC 3962, whose checksum and string-to-key are SHA-1
 * based. Not for new designs of our own. */
struct hmac_sha1_ctx
{
   struct sha1_state inner;
   struct sha1_state outer;
};

void hmac_sha1_init(struct hmac_sha1_ctx *ctx,
      const uint8_t *key, size_t key_len);
void hmac_sha1_update(struct hmac_sha1_ctx *ctx,
      const uint8_t *data, size_t len);
/* @mac is 20 octets; the context is wiped. */
void hmac_sha1_final(struct hmac_sha1_ctx *ctx, uint8_t *mac);
void hmac_sha1(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac);

int pbkdf2_hmac_sha1(const uint8_t *password, size_t password_len,
      const uint8_t *salt, size_t salt_len, uint32_t iterations,
      uint8_t *out, size_t out_len);

struct hmac_sha384_ctx
{
   struct sha512_state inner;
   struct sha512_state outer;
};

void hmac_sha384_init(struct hmac_sha384_ctx *ctx,
      const uint8_t *key, size_t key_len);
void hmac_sha384_update(struct hmac_sha384_ctx *ctx,
      const uint8_t *data, size_t len);
void hmac_sha384_final(struct hmac_sha384_ctx *ctx, uint8_t *mac);
void hmac_sha384(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac);

/* HKDF-SHA256 (RFC 5869). */

/**
 * hkdf_sha256_extract:
 * @salt              : May be NULL/0 (a zero key is used).
 * @prk               : 32 octets out.
 **/
void hkdf_sha256_extract(const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len, uint8_t *prk);

/**
 * hkdf_sha256_expand:
 * @okm_len           : At most 255 * 32.
 *
 * Returns: 0 on success, -1 when @okm_len is too large.
 **/
int hkdf_sha256_expand(const uint8_t *prk, size_t prk_len,
      const uint8_t *info, size_t info_len,
      uint8_t *okm, size_t okm_len);
/* The same over SHA-384 (48-octet PRK, 48 octets per block). */
void hkdf_sha384_extract(const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len, uint8_t *prk);
int hkdf_sha384_expand(const uint8_t *prk, size_t prk_len,
      const uint8_t *info, size_t info_len,
      uint8_t *okm, size_t okm_len);

/* Extract then expand. */
int hkdf_sha256(const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len,
      const uint8_t *info, size_t info_len,
      uint8_t *okm, size_t okm_len);

/* PBKDF2-HMAC-SHA256 (RFC 8018). */

/**
 * pbkdf2_hmac_sha256:
 * @iterations        : At least 1.
 *
 * Returns: 0 on success, -1 on a zero iteration count or out of memory.
 **/
int pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
      const uint8_t *salt, size_t salt_len, uint32_t iterations,
      uint8_t *out, size_t out_len);

/**
 * crypto_random_bytes:
 *
 * Fills @buf from the platform entropy source (BCrypt / RtlGenRandom
 * / CryptGenRandom on Windows, getentropy or /dev/urandom on POSIX,
 * the kernel RNG on 3DS, Vita, Switch, PSP, Wii U and PS2).
 *
 * Returns: 0 on success, -1 when no source produced @len octets.
 **/
int crypto_random_bytes(uint8_t *buf, size_t len);

/* Deterministic random bit generator on ChaCha20 with fast key
 * erasure: every generate call takes its next key from the keystream
 * it produces, so a state captured later cannot reproduce output
 * handed out earlier. Reseeds from crypto_random_bytes() after
 * DRBG_RESEED_INTERVAL octets. Not thread-safe; one per thread or
 * caller-locked. */

#define DRBG_KEY_SIZE   32
#define DRBG_NONCE_SIZE 12

struct drbg_ctx
{
   uint64_t since_reseed;
   uint32_t counter;
   uint8_t  key[DRBG_KEY_SIZE];
   uint8_t  nonce[DRBG_NONCE_SIZE];
   uint8_t  seeded;
};

#define DRBG_RESEED_INTERVAL ((uint64_t)1 << 30)

/**
 * drbg_init:
 * @seed              : Optional entropy; NULL seeds from
 *                      crypto_random_bytes(). Any length is mixed
 *                      through HKDF, so a caller with a nonce or
 *                      personalisation string can append it.
 *
 * Returns: 0 on success, -1 when no entropy was available.
 **/
int drbg_init(struct drbg_ctx *ctx, const uint8_t *seed, size_t seed_len);

/**
 * drbg_reseed:
 *
 * Mixes fresh platform entropy (and @extra, if any) into the key.
 **/
int drbg_reseed(struct drbg_ctx *ctx, const uint8_t *extra, size_t extra_len);

/**
 * drbg_generate:
 *
 * Returns: 0 on success, -1 when the context is unseeded or a
 * scheduled reseed found no entropy.
 **/
int drbg_generate(struct drbg_ctx *ctx, uint8_t *out, size_t len);

void drbg_free(struct drbg_ctx *ctx);

/* NTLMv2 needs two things nothing else here does: MD4 (RFC 1320),
 * the hash NT password hashes are still made of, and HMAC-MD5 over
 * the MD5 in lrc_hash. They exist for that one protocol. */
#define MD4_DIGEST_SIZE 16
void md4(const uint8_t *data, size_t len, uint8_t *digest);

#define HMAC_MD5_SIZE 16
void hmac_md5(const uint8_t *key, size_t key_len,
      const uint8_t *data, size_t len, uint8_t *mac);

/**
 * kbkdf_hmac_sha256:
 *
 * SP 800-108 KDF in counter mode with HMAC-SHA256: K(i) = HMAC(Ki,
 * [i]_32 || label || 0x00 || context || [L]_32), the form SMB 3.x
 * derives its signing, encryption and application keys with. The
 * separator octet is included; @label and @context are passed as
 * SMB spells them (including their trailing NUL).
 **/
void kbkdf_hmac_sha256(const uint8_t *ki, size_t ki_len,
      const uint8_t *label, size_t label_len,
      const uint8_t *context, size_t context_len,
      uint8_t *out, size_t out_len);

/**
 * ntlm_ntowf_v2:
 * @password, @user, @domain: UTF-8. The user name is upper-cased
 *                      (ASCII) as NTOWFv2 requires; the domain is not.
 * @out               : 16 octets: HMAC-MD5(MD4(UTF-16LE(password)),
 *                      UTF-16LE(UPPER(user) || domain)).
 *
 * Returns: 0 on success, -1 on a string that is not valid UTF-8 or is
 * longer than 255 code points.
 **/
int ntlm_ntowf_v2(const char *password, const char *user,
      const char *domain, uint8_t *out);

RETRO_END_DECLS

#endif
