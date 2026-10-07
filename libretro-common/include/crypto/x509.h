/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x509.h).
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

#ifndef _LIBRETRO_CRYPTO_X509_H
#define _LIBRETRO_CRYPTO_X509_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* X.509 for a TLS client: parse a DER certificate, load the PEM trust
 * bundle, and verify a server chain against it. Keys are RSA (up to
 * 4096 bits), P-256 and P-384; signatures are RSA PKCS#1 v1.5 and ECDSA over
 * SHA-256/384/512. Names are compared as DER octet strings. */

/* extendedKeyUsage as far as a TLS client cares */
#define X509_EKU_ABSENT      0   /* no extension: any use */
#define X509_EKU_SERVER_AUTH 1   /* serverAuth or anyExtendedKeyUsage listed */
#define X509_EKU_OTHER       2   /* present, without either */

enum x509_key_type
{
   X509_KEY_NONE = 0,
   X509_KEY_RSA,
   X509_KEY_P256,
   X509_KEY_P384
};

enum x509_sig_alg
{
   X509_SIG_NONE = 0,
   X509_SIG_RSA_SHA256,
   X509_SIG_RSA_SHA384,
   X509_SIG_RSA_SHA512,
   X509_SIG_ECDSA_SHA256,
   X509_SIG_ECDSA_SHA384,
   X509_SIG_ECDSA_SHA512
};

/* Every pointer below points into the DER the certificate was parsed
 * from, which the caller keeps alive for as long as the struct. */
struct x509_cert
{
   const uint8_t *der;        size_t der_len;
   const uint8_t *tbs;        size_t tbs_len;    /* signed part */
   const uint8_t *subject;    size_t subject_len;
   const uint8_t *issuer;     size_t issuer_len;
   const uint8_t *sig;        size_t sig_len;
   const uint8_t *rsa_n;      size_t rsa_n_len;
   const uint8_t *rsa_e;      size_t rsa_e_len;
   const uint8_t *san;        size_t san_len;    /* SubjectAltName value */
   const uint8_t *cn;         size_t cn_len;     /* subject commonName */
   const uint8_t *ec_point;                      /* 65 or 97 octets */
   time_t not_before;
   time_t not_after;
   enum x509_key_type key_type;
   enum x509_sig_alg  sig_alg;
   int  is_ca;          /* basicConstraints cA; -1 when absent */
   int  path_len;       /* basicConstraints pathLenConstraint; -1 none */
   int  key_usage;      /* keyUsage bits, -1 when absent */
   int  eku;            /* X509_EKU_*: may this key authenticate a server? */
};

#define X509_KU_DIGITAL_SIGNATURE 0x80
#define X509_KU_KEY_CERT_SIGN     0x04

/**
 * x509_parse:
 *
 * Returns: 0 on success, -1 on malformed DER. An unsupported key or
 * signature algorithm parses (type NONE) so a chain can still be
 * walked past it and reported.
 **/
int x509_parse(struct x509_cert *c, const uint8_t *der, size_t len);

/**
 * x509_verify_signature:
 *
 * Checks @c's signature under @issuer's key.
 *
 * Returns: 0 when valid, -1 otherwise.
 **/
int x509_verify_signature(const struct x509_cert *c,
      const struct x509_cert *issuer);

/**
 * x509_verify_ecdsa_digest:
 *
 * Checks a DER ECDSA-Sig-Value @sig over an already computed @digest
 * under @key's P-256 or P-384 key. What x509_verify_signature() uses
 * for ECDSA, exposed for the TLS ServerKeyExchange signature.
 *
 * Returns: 0 when valid, -1 otherwise.
 **/
int x509_verify_ecdsa_digest(const struct x509_cert *key,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *sig, size_t sig_len);

/**
 * x509_match_hostname:
 *
 * dNSName entries of the subjectAltName, with a wildcard allowed as
 * the whole leftmost label; the subject commonName only when there is
 * no subjectAltName at all.  An IP address (see x509_parse_ip()) matches
 * only an iPAddress entry, never a dNSName or the commonName.
 *
 * Returns: 0 on a match, -1 otherwise.
 **/
int x509_match_hostname(const struct x509_cert *c, const char *host);

/**
 * x509_parse_ip:
 *
 * @host as an IP address: dotted IPv4 (4 bytes, no leading zeros), or
 * IPv6 with "::" compression and an optional trailing dotted IPv4,
 * brackets and a "%zone" allowed (16 bytes).  Returns 4 or 16 with the
 * address in @out, 0 when @host is not an IP literal - a host name.
 **/
int x509_parse_ip(const char *host, uint8_t out[16]);

/**
 * x509_trust_load_pem:
 * @pem, @len         : Concatenated PEM certificates (cacert.h).
 *
 * Replaces the trust store with every certificate in @pem whose key
 * this code can use. Idempotent per bundle: a second call with the
 * same pointer and length is a no-op.
 *
 * Returns: number of anchors loaded, -1 out of memory.
 **/
int x509_trust_load_pem(const char *pem, size_t len);

/**
 * x509_trust_load_pem_parts:
 *
 * As x509_trust_load_pem() for a bundle kept in @count parts, each a
 * run of whole certificates (net/cacert.h, split for compilers that
 * refuse long string literals): all of them are loaded into the one
 * trust store, the table standing for the source.
 *
 * Returns: the number of anchors loaded, or -1 on failure.
 **/
int x509_trust_load_pem_parts(const char *const *parts, const size_t *lens,
      unsigned count);

void x509_trust_free(void);

/**
 * x509_verify_chain:
 * @ders, @lens, @n   : The server's chain, leaf first, as sent.
 * @host              : Expected hostname, or NULL to skip the check.
 * @now               : Current time for the validity check.
 * @info, @info_len   : On failure, a short reason for logging.
 *
 * Builds the path leaf -> ... -> anchor using the chain's own
 * certificates and the trust store, checking signatures, validity,
 * basicConstraints and keyUsage on the way.
 *
 * Returns: 0 when the leaf chains to a trust anchor and matches
 * @host, -1 otherwise.
 **/
int x509_verify_chain(const uint8_t **ders, const size_t *lens, unsigned n,
      const char *host, time_t now, char *info, size_t info_len);

RETRO_END_DECLS

#endif
