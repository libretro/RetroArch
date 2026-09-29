/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (pk.h).
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

#ifndef _LIBRETRO_CRYPTO_PK_H
#define _LIBRETRO_CRYPTO_PK_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Public-key side of the cleanroom stack: fixed-width bignum, RSA
 * PKCS#1 v1.5 verification and P-256 ECDH / ECDSA verification. */

/* Fixed-width multi-precision arithmetic for the public-key code:
 * little-endian arrays of bn_word, every operand of one call the
 * same length k, Montgomery multiplication with double-word products. There
 * is no dynamic sizing and no allocation in here; callers pick k from
 * the modulus and keep the working storage themselves (on the heap
 * for RSA - see rsa.c - since a 4096-bit operand is 512 bytes). */

/* 64-bit limbs with 128-bit products wherever the compiler has them
 * (GCC and Clang on 64-bit targets): a Montgomery multiplication is
 * a quarter of the word products of the 32-bit form. Everywhere else,
 * 32-bit limbs with 64-bit products, which is plain C89. */
#if defined(__SIZEOF_INT128__) && !defined(BN_WORD_32)
typedef uint64_t bn_word;
__extension__ typedef unsigned __int128 bn_dword;
#define BN_WORD_BITS 64
#else
typedef uint32_t bn_word;
typedef uint64_t bn_dword;
#define BN_WORD_BITS 32
#endif
#define BN_WORD_BYTES (BN_WORD_BITS / 8)

/* 4096-bit RSA is the largest key in any CA bundle this tree ships. */
#define BN_MAX_WORDS (4096 / BN_WORD_BITS)

/* Words of scratch bn_mont_mul() and bn_mont_sqr() need for length k. */
#define BN_MONT_TMP_WORDS(k) (2 * (k) + 2)

void     bn_from_be(bn_word *r, unsigned k, const uint8_t *in, size_t len);
void     bn_to_be(uint8_t *out, size_t len, const bn_word *a, unsigned k);
void     bn_zero(bn_word *r, unsigned k);
void     bn_copy(bn_word *r, const bn_word *a, unsigned k);
int      bn_is_zero(const bn_word *a, unsigned k);
/* -1, 0, 1; constant time in k. */
int      bn_cmp(const bn_word *a, const bn_word *b, unsigned k);
bn_word bn_add(bn_word *r, const bn_word *a, const bn_word *b, unsigned k);
bn_word bn_sub(bn_word *r, const bn_word *a, const bn_word *b, unsigned k);
/* r = a + b mod m and r = a - b mod m, for a, b < m. */
void     bn_mod_add(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, unsigned k);
void     bn_mod_sub(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, unsigned k);
/* r = bit ? a : b, without branching on bit. */
void     bn_select(bn_word *r, bn_word bit, const bn_word *a,
      const bn_word *b, unsigned k);
/* Highest set bit + 1, 0 for zero. */
unsigned bn_bit_length(const bn_word *a, unsigned k);
unsigned bn_get_bit(const bn_word *a, unsigned i);

/* Montgomery machinery, R = 2^(BN_WORD_BITS * k). */
bn_word bn_mont_n0(const bn_word *m);
/* r2 = R^2 mod m. Only needs m odd. */
void     bn_mont_r2(bn_word *r2, const bn_word *m, unsigned k);
/* r = a * b * R^-1 mod m, for a, b < m; @tmp has BN_MONT_TMP_WORDS(k). */
void     bn_mont_mul(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, bn_word n0, unsigned k, bn_word *tmp);
/* r = a * a * R^-1 mod m: the off-diagonal products once, doubled,
 * which is what an exponentiation spends most of its time on. */
void     bn_mont_sqr(bn_word *r, const bn_word *a,
      const bn_word *m, bn_word n0, unsigned k, bn_word *tmp);
/* r = a * b mod m (plain domain), through @r2 = R^2 mod m. */
void     bn_mod_mul(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, const bn_word *r2, bn_word n0, unsigned k,
      bn_word *tmp);
/* r = base^exp mod m, plain domain. exp has ek words. Left-to-right
 * binary; exponents here are public (RSA e, curve orders). @work has
 * 3k + BN_MONT_TMP_WORDS(k) words. */
void     bn_mod_exp(bn_word *r, const bn_word *base,
      const bn_word *exp, unsigned ek,
      const bn_word *m, const bn_word *r2, bn_word n0, unsigned k,
      bn_word *work);

/* RSASSA-PKCS1-v1_5 signature verification (RFC 8017 8.2.2), the
 * only RSA operation a TLS client with a CA bundle needs. Public-key
 * operands only, so nothing here is secret. */

enum rsa_hash
{
   RSA_HASH_SHA256 = 0,
   RSA_HASH_SHA384 = 1,
   RSA_HASH_SHA512 = 2
};

/**
 * rsa_pkcs1_verify:
 * @n, @n_len         : Modulus, big-endian, up to 512 octets (4096 bits).
 * @e, @e_len         : Public exponent, big-endian.
 * @digest            : The already computed hash of the signed data,
 *                      32/48/64 octets for @hash.
 * @sig, @sig_len     : Signature, big-endian, @sig_len == the modulus
 *                      length after leading zeros.
 *
 * Returns: 0 when the signature is valid, -1 otherwise (bad key or
 * parameter, or verification failure; the two are not distinguished
 * on purpose).
 **/
int rsa_pkcs1_verify(const uint8_t *n, size_t n_len,
      const uint8_t *e, size_t e_len,
      enum rsa_hash hash, const uint8_t *digest, size_t digest_len,
      const uint8_t *sig, size_t sig_len);

/**
 * rsa_pss_verify:
 * RSASSA-PSS with MGF1 over the same hash and a salt of the hash
 * length (TLS 1.3 rsa_pss_rsae_*). Arguments as rsa_pkcs1_verify().
 **/
int rsa_pss_verify(const uint8_t *n, size_t n_len,
      const uint8_t *e, size_t e_len,
      enum rsa_hash hash, const uint8_t *digest, size_t digest_len,
      const uint8_t *sig, size_t sig_len);

/**
 * x25519:
 * RFC 7748 X25519: @out = @scalar * @u_le, all 32 octets little-endian
 * as on the wire; the scalar is clamped here. Returns -1 when the
 * result is the all-zero point (a low-order input), 0 otherwise.
 * x25519_base() uses the base point 9.
 **/
int x25519(uint8_t *out, const uint8_t *scalar, const uint8_t *u_le);
int x25519_base(uint8_t *out, const uint8_t *scalar);

/* NIST P-256 (secp256r1): ECDH for the TLS key exchange and ECDSA
 * signature verification for certificates. Points on the wire are
 * uncompressed SEC 1 encodings, 0x04 || X || Y. Field and scalar
 * arithmetic run through bn's Montgomery machinery at k = 8. */

#define P256_SCALAR_SIZE 32
#define P256_POINT_SIZE  65

/**
 * p256_keygen:
 * @priv              : 32 random octets; rejected when 0 or >= n, so
 *                      the caller draws again (probability ~2^-32).
 * @pub               : 65 octets out.
 *
 * Returns: 0 on success, -1 on an unusable scalar.
 **/
int p256_keygen(const uint8_t *priv, uint8_t *pub);

/**
 * p256_ecdh:
 * @shared            : 32 octets out, the x coordinate.
 *
 * Validates @peer (on the curve, coordinates in range) first.
 *
 * Returns: 0 on success, -1 on a bad key or a result at infinity.
 **/
int p256_ecdh(const uint8_t *priv, const uint8_t *peer, uint8_t *shared);

/**
 * p256_ecdsa_verify:
 * @digest            : Message hash; the leftmost 32 octets are used
 *                      (FIPS 186-4 6.4), shorter digests are allowed.
 * @r, @s             : 32 octets each, big-endian.
 *
 * Returns: 0 when valid, -1 otherwise.
 **/
int p256_ecdsa_verify(const uint8_t *pub,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *r, const uint8_t *s);

/* NIST P-384 (secp384r1), the same operations on 48-octet scalars and
 * 97-octet points. Needed for the CA roots that carry P-384 keys. */
#define P384_SCALAR_SIZE 48
#define P384_POINT_SIZE  97

int p384_keygen(const uint8_t *priv, uint8_t *pub);
int p384_ecdh(const uint8_t *priv, const uint8_t *peer, uint8_t *shared);
int p384_ecdsa_verify(const uint8_t *pub,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *r, const uint8_t *s);

RETRO_END_DECLS

#endif
