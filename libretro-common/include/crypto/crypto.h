/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crypto.h).
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

#ifndef _LIBRETRO_CRYPTO_H
#define _LIBRETRO_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <retro_inline.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Built only under HAVE_CRYPTO (Makefile.common, griffin); the small
 * consoles leave it out and anything optional on top of it - the
 * keychain, the cleanroom TLS, SMB and NFS clients - is gated on it.
 *
 * Symmetric primitives: constant-time helpers, AES (block, CTR, GCM,
 * CMAC), ChaCha20, Poly1305 and the AEAD front over them. Hashes are
 * in lrc_hash.h, key derivation in kdf.h, public key in pk.h. */

/* Byte-order helpers and the two operations every primitive here
 * needs to get right on secret data: comparing without an early exit,
 * and clearing a buffer in a way the optimiser may not drop. */

static INLINE uint32_t crypto_load32_be(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static INLINE void crypto_store32_be(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >>  8);
   p[3] = (uint8_t)(v);
}

static INLINE uint64_t crypto_load64_be(const uint8_t *p)
{
   return ((uint64_t)crypto_load32_be(p) << 32) | crypto_load32_be(p + 4);
}

static INLINE void crypto_store64_be(uint8_t *p, uint64_t v)
{
   crypto_store32_be(p,     (uint32_t)(v >> 32));
   crypto_store32_be(p + 4, (uint32_t)(v));
}

static INLINE uint32_t crypto_load32_le(const uint8_t *p)
{
   return  (uint32_t)p[0]        | ((uint32_t)p[1] <<  8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static INLINE void crypto_store32_le(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v);
   p[1] = (uint8_t)(v >>  8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static INLINE void crypto_store64_le(uint8_t *p, uint64_t v)
{
   crypto_store32_le(p,     (uint32_t)(v));
   crypto_store32_le(p + 4, (uint32_t)(v >> 32));
}

static INLINE uint32_t crypto_rotl32(uint32_t v, unsigned n)
{
   return (v << n) | (v >> (32 - n));
}

/**
 * crypto_memeq_ct:
 *
 * Returns: 1 when the two buffers are equal, 0 otherwise. Runs in
 * time depending only on @len.
 **/
int crypto_memeq_ct(const void *a, const void *b, size_t len);

/**
 * crypto_memzero:
 *
 * Clears @len octets through a volatile pointer so that a dead-store
 * elimination pass cannot remove it.
 **/
void crypto_memzero(void *p, size_t len);

/* AES (FIPS 197), encryption direction only. Every mode this tree
 * needs - CTR, GCM, CMAC, and the CCM SMB3 will want - runs the
 * forward cipher, so no inverse cipher is carried. */

#define AES_BLOCK_SIZE 16

struct aes_ctx
{
   uint32_t rk[60];
   unsigned rounds;
};

struct aes_gcm_ctx
{
   struct aes_ctx aes;
   /* H = E_K(0^128), as two big-endian 64-bit halves, and the 4-bit
    * multiplication tables the scalar GHASH walks. */
   uint64_t h_hi;
   uint64_t h_lo;
   uint64_t hh[16];
   uint64_t hl[16];
};

/**
 * aes_init:
 * @key_len           : 16, 24 or 32 octets.
 *
 * Returns: 0 on success, -1 on an unsupported key length.
 **/
int aes_init(struct aes_ctx *ctx, const uint8_t *key, size_t key_len);

void aes_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out);

/**
 * aes_decrypt_block:
 *
 * Inverse cipher on one block with the same context aes_init() made;
 * no separate decryption schedule is needed. Serves CBC and CTS
 * (Kerberos); the AEAD modes above never decrypt with the cipher.
 **/
void aes_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out);

/**
 * aes_ctr_crypt:
 * @counter           : 16-octet counter block. The low 32 bits (big
 *                      endian, octets 12..15) are incremented per
 *                      block, as GCM does; the block is updated in
 *                      place so a caller can continue a stream.
 *
 * Encrypts or decrypts @len octets (the two are the same operation).
 * @in and @out may alias. Partial trailing blocks are allowed but a
 * continuation must start on a block boundary.
 **/
/**
 * crypto_aes_hw:
 *
 * Returns: 1 when AES runs on instructions made for it here (AES-NI
 * with PCLMULQDQ, or the ARMv8 AES and PMULL), 0 when it runs in
 * software - where ChaCha20-Poly1305 is several times cheaper.
 **/
int crypto_aes_hw(void);

void aes_ctr_crypt(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len);

/**
 * aes_cmac:
 *
 * CMAC (RFC 4493 / SP 800-38B) of @len octets under @ctx, 16-octet
 * tag to @mac.
 **/
void aes_cmac(const struct aes_ctx *ctx,
      const uint8_t *msg, size_t len, uint8_t *mac);

/**
 * aes_ccm_encrypt:
 * @nonce_len         : 7 to 13 octets (SMB3 uses 11).
 * @tag_len           : 4 to 16, even.
 *
 * CCM (RFC 3610) under a plain AES context. @ct may alias @pt.
 *
 * Returns: 0 on success, -1 on a bad nonce or tag length or a message
 * too long for the nonce.
 **/
int aes_ccm_encrypt(const struct aes_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag, size_t tag_len);

/**
 * aes_ccm_decrypt:
 *
 * Returns: 0 on success, -1 on a bad parameter or authentication
 * failure (the plaintext is cleared in that case: CCM cannot check
 * the tag before decrypting).
 **/
int aes_ccm_decrypt(const struct aes_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, size_t tag_len, uint8_t *pt);

/**
 * aes_gcm_init:
 *
 * Returns: 0 on success, -1 on an unsupported key length.
 **/
int aes_gcm_init(struct aes_gcm_ctx *ctx,
      const uint8_t *key, size_t key_len);

/**
 * aes_gcm_encrypt:
 * @nonce             : Any length; 12 octets is the fast path.
 * @tag               : 16 octets out.
 *
 * @ct may alias @pt. Returns 0.
 **/
int aes_gcm_encrypt(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag);

/**
 * aes_gcm_decrypt:
 * @tag               : 16 octets in.
 *
 * The tag is checked in constant time before anything is written to
 * @pt; @pt may alias @ct.
 *
 * Returns: 0 on success, -1 on authentication failure (nothing is
 * written to @pt).
 **/
int aes_gcm_decrypt(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, uint8_t *pt);

/* ChaCha20 (RFC 8439): 256-bit key, 96-bit nonce, 32-bit block counter. */

#define CHACHA20_KEY_SIZE   32
#define CHACHA20_NONCE_SIZE 12
#define CHACHA20_BLOCK_SIZE 64

/**
 * chacha20_block:
 *
 * Writes the 64-octet keystream block for @counter to @out.
 **/
void chacha20_block(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, uint8_t *out);

/**
 * chacha20_xor:
 *
 * XORs @len octets of keystream starting at block @counter into
 * @in -> @out (encrypt and decrypt are the same operation; the two
 * may alias). A continuation must start on a 64-octet boundary.
 **/
void chacha20_xor(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, const uint8_t *in, uint8_t *out, size_t len);

/* Poly1305 (RFC 8439) one-time authenticator, 32-octet key, 16-octet
 * tag. Arithmetic is 26-bit limbs on 64-bit products, with no
 * data-dependent branches. */

#define POLY1305_KEY_SIZE 32
#define POLY1305_TAG_SIZE 16

struct poly1305_ctx
{
   uint32_t r[5];
   uint32_t h[5];
   uint32_t pad[4];
   size_t   leftover;
   uint8_t  buffer[16];
   uint8_t  final;
};

void poly1305_init(struct poly1305_ctx *ctx, const uint8_t *key);
void poly1305_update(struct poly1305_ctx *ctx,
      const uint8_t *m, size_t len);
void poly1305_final(struct poly1305_ctx *ctx, uint8_t *tag);

/* One-shot. */
void poly1305_auth(uint8_t *tag, const uint8_t *m, size_t len,
      const uint8_t *key);

/* Authenticated encryption with associated data: ChaCha20-Poly1305
 * (RFC 8439) and AES-256-GCM behind one call. Both take a 32-octet key,
 * a 12-octet nonce and produce a 16-octet tag. */

enum aead_alg
{
   AEAD_CHACHA20_POLY1305 = 0,
   AEAD_AES256_GCM        = 1
};

#define AEAD_KEY_SIZE   32
#define AEAD_NONCE_SIZE 12
#define AEAD_TAG_SIZE   16

/**
 * aead_encrypt:
 *
 * @ct may alias @pt.
 *
 * Returns: 0 on success, -1 on a bad algorithm, key or nonce length.
 **/
int aead_encrypt(enum aead_alg alg,
      const uint8_t *key, size_t key_len,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag, size_t tag_len);

/**
 * aead_decrypt:
 *
 * Verifies @tag in constant time before writing any plaintext;
 * @pt may alias @ct.
 *
 * Returns: 0 on success, -1 on a bad parameter, -2 on authentication
 * failure (nothing written to @pt).
 **/
int aead_decrypt(enum aead_alg alg,
      const uint8_t *key, size_t key_len,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, size_t tag_len,
      uint8_t *pt);

RETRO_END_DECLS

#endif
