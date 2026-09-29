/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_krb5.h).
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

#ifndef __LIBRETRO_SDK_NET_KRB5_H
#define __LIBRETRO_SDK_NET_KRB5_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Kerberos 5 for the built-in SMB client: the AES enctypes of
 * RFC 3962 (aes128-cts-hmac-sha1-96, aes256-cts-hmac-sha1-96), which
 * every Active Directory domain and MIT/Heimdal realm offers, on the
 * cleanroom AES and SHA-1. No dependency beyond crypto.c / kdf.c. */

#define KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96 17
#define KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96 18

/* One key with its enctype; @len is 16 or 32. */
struct krb5_key
{
   uint8_t  k[32];
   size_t   len;
   int      enctype;
};

/**
 * krb5_string_to_key:
 * @enctype    : KRB5_ENCTYPE_AES*_CTS_HMAC_SHA1_96
 * @password   : UTF-8 password, not terminated
 * @salt       : normally the realm followed by the principal
 *               components with no separators, as the KDC says
 * @iterations : 4096 unless the KDC's ETYPE-INFO2 gives another
 *
 * RFC 3962 string-to-key: PBKDF2-HMAC-SHA1 then DK(key, "kerberos").
 *
 * Returns: 0 on success, -1 on a bad enctype.
 **/
int krb5_string_to_key(int enctype, const char *password, size_t password_len,
      const char *salt, size_t salt_len, uint32_t iterations,
      struct krb5_key *out);

/**
 * krb5_encrypt:
 * @usage      : key usage number (RFC 4120 7.5.1)
 * @conf       : 16 octets of confounder, fresh random per message
 * @in / @len  : plaintext
 * @out        : at least @len + 16 + 12 octets
 *
 * RFC 3962 encryption: E(Ke, confounder | plaintext) in CBC with
 * ciphertext stealing, then HMAC-SHA1(Ki, confounder | plaintext)
 * truncated to 96 bits. Returns the ciphertext length.
 **/
size_t krb5_encrypt(const struct krb5_key *key, uint32_t usage,
      const uint8_t *conf, const uint8_t *in, size_t len, uint8_t *out);

/**
 * krb5_decrypt:
 * @out        : at least @len - 16 - 12 octets
 * @out_len    : plaintext length on success
 *
 * Returns: 0 on success, -1 on a bad length or checksum.
 **/
int krb5_decrypt(const struct krb5_key *key, uint32_t usage,
      const uint8_t *in, size_t len, uint8_t *out, size_t *out_len);

/**
 * krb5_checksum:
 * @mac        : 12 octets
 *
 * hmac-sha1-96-aes* over @data with Kc for @usage.
 **/
void krb5_checksum(const struct krb5_key *key, uint32_t usage,
      const uint8_t *data, size_t len, uint8_t *mac);

/**
 * krb5_derive_key:
 * @constant   : the usage constant RFC 3961 5.1 spells out, or an
 *               arbitrary octet string; the KRB-FX-CF2 / GSS SMB key
 *               derivations pass their own
 *
 * DK(key, constant) as a key of the same enctype.
 **/
void krb5_derive_key(const struct krb5_key *key,
      const uint8_t *constant, size_t constant_len, struct krb5_key *out);

/**
 * krb5_prf:
 * @out        : 16 octets
 *
 * RFC 3962 pseudo-random function: truncated SHA-1 of @data, run
 * through the cipher under DK(key, "prf").
 **/
void krb5_prf(const struct krb5_key *key, const uint8_t *data, size_t len,
      uint8_t *out);

/* Raw building blocks, exposed for the known-answer tests and for the
 * few places (KRB-FX-CF2, GSS key derivations) that need them bare. */

/* RFC 3961 5.1 n-fold of @len octets onto @n. */
void krb5_nfold(const uint8_t *in, unsigned len, uint8_t *out, unsigned n);

/* RFC 3962 5: AES CBC with ciphertext stealing, zero IV, no confounder
 * or checksum; @len >= 16, @out as long as @in (may alias).
 * Returns -1 on a bad key length or a short input. */
int krb5_raw_cts(const uint8_t *key, size_t key_len, int decrypt,
      const uint8_t *in, size_t len, uint8_t *out);

RETRO_END_DECLS

#endif
