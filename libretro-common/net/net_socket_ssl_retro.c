/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_socket_ssl_retro.c).
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

/* TLS 1.2 client on the cleanroom crypto (crypto/), behind the
 * ssl_socket_* API. ECDHE over P-256 or P-384, RSA or ECDSA server
 * authentication against net/cacert.h, AES-128-GCM or
 * ChaCha20-Poly1305 records, SHA-256 handshake and PRF; SNI sent.
 * No renegotiation, no resumption, no client certificates. The
 * handshake runs to completion inside ssl_socket_connect() as the
 * other backends' do. */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <crypto/pk.h>
#include <crypto/x509.h>
#include <lrc_hash.h>
#include <retro_miscellaneous.h>

#include "cacert.h"

#define TLS_REC_MAX      16384
#define TLS_REC_OVERHEAD (8 + 16)          /* explicit nonce + tag */
#define TLS_RX_SIZE      (5 + TLS_REC_MAX + 256)
#define TLS_HS_MAX       (64 * 1024)       /* certificate chains */

#define TLS_CT_CCS       20
#define TLS_CT_ALERT     21
#define TLS_CT_HANDSHAKE 22
#define TLS_CT_APPDATA   23

#define TLS_HS_CLIENT_HELLO     1
#define TLS_HS_SERVER_HELLO     2
#define TLS_HS_CERTIFICATE      11
#define TLS_HS_SERVER_KEX       12
#define TLS_HS_SERVER_DONE      14
#define TLS_HS_CLIENT_KEX       16
#define TLS_HS_FINISHED         20

#define TLS_SUITE_ECDHE_ECDSA_AES128_GCM   0xc02b
#define TLS_SUITE_ECDHE_RSA_AES128_GCM     0xc02f
#define TLS_SUITE_ECDHE_RSA_CHACHA20       0xcca8
#define TLS_SUITE_ECDHE_ECDSA_CHACHA20     0xcca9

/* Error codes reported through ssl_socket_last_error(); negative like
 * mbedtls's so callers' "library error" logging stays meaningful. */
#define TLS_ERR_SOCKET      0
#define TLS_ERR_RECORD     -1
#define TLS_ERR_ALERT      -2
#define TLS_ERR_HANDSHAKE  -3
#define TLS_ERR_CIPHER     -4
#define TLS_ERR_CERT       -5
#define TLS_ERR_SIGNATURE  -6
#define TLS_ERR_KEX        -7
#define TLS_ERR_MEMORY     -8
#define TLS_ERR_VERSION    -9

static unsigned tls_verify_mode = 0;   /* 0 required, 1 optional, 2 disabled */

/* The trust bundle is net/cacert.h unless a test swapped one in. */
static const char *tls_trust_pem     = cacert_pem;
static size_t      tls_trust_pem_len = sizeof(cacert_pem);

void ssl_socket_retro_set_trust_pem(const char *pem, size_t len)
{
   tls_trust_pem     = pem ? pem : cacert_pem;
   tls_trust_pem_len = pem ? len : sizeof(cacert_pem);
}

struct ssl_state
{
   uint8_t *rx;             /* raw record buffer */
   uint8_t *hs;             /* handshake message accumulator */
   uint8_t *pt;             /* decrypted application data pending */
   const char *domain;
   size_t   rx_len;
   size_t   hs_len;
   size_t   hs_off;
   size_t   pt_len;
   size_t   pt_off;
   uint64_t cseq;
   uint64_t sseq;
   struct sha256_state transcript;
   int      fd;
   int      last_err;
   unsigned suite;
   unsigned group;          /* 23 P-256, 24 P-384 */
   uint8_t  client_random[32];
   uint8_t  server_random[32];
   uint8_t  master[48];
   uint8_t  cwk[32];
   uint8_t  swk[32];
   uint8_t  civ[12];
   uint8_t  siv[12];
   uint8_t  priv[48];
   uint8_t  premaster[48];
   uint8_t  rx_encrypted;
   uint8_t  tx_encrypted;
   uint8_t  handshake_done;
   uint8_t  closed;
};

/* ---- byte helpers ------------------------------------------------- */

static void tls_put16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)(v >> 8);
   p[1] = (uint8_t)v;
}

static unsigned tls_get16(const uint8_t *p)
{
   return ((unsigned)p[0] << 8) | p[1];
}

static unsigned tls_get24(const uint8_t *p)
{
   return ((unsigned)p[0] << 16) | ((unsigned)p[1] << 8) | p[2];
}

/* ---- PRF ---------------------------------------------------------- */

/* TLS 1.2 PRF with SHA-256: P_SHA256(secret, label || seed). */
static void tls_prf(const uint8_t *secret, size_t secret_len,
      const char *label, const uint8_t *seed1, size_t seed1_len,
      const uint8_t *seed2, size_t seed2_len, uint8_t *out, size_t out_len)
{
   struct hmac_sha256_ctx h;
   uint8_t a[32];
   uint8_t p[32];
   size_t  label_len = strlen(label);

   /* A(1) = HMAC(secret, A(0)), A(0) = label || seed */
   hmac_sha256_init(&h, secret, secret_len);
   hmac_sha256_update(&h, (const uint8_t*)label, label_len);
   hmac_sha256_update(&h, seed1, seed1_len);
   hmac_sha256_update(&h, seed2, seed2_len);
   hmac_sha256_final(&h, a);

   while (out_len)
   {
      size_t take = out_len < 32 ? out_len : 32;
      hmac_sha256_init(&h, secret, secret_len);
      hmac_sha256_update(&h, a, 32);
      hmac_sha256_update(&h, (const uint8_t*)label, label_len);
      hmac_sha256_update(&h, seed1, seed1_len);
      hmac_sha256_update(&h, seed2, seed2_len);
      hmac_sha256_final(&h, p);
      memcpy(out, p, take);
      out     += take;
      out_len -= take;

      hmac_sha256_init(&h, secret, secret_len);
      hmac_sha256_update(&h, a, 32);
      hmac_sha256_final(&h, a);
   }
   crypto_memzero(a, sizeof(a));
   crypto_memzero(p, sizeof(p));
}

/* ---- record layer ------------------------------------------------- */

static int tls_suite_is_chacha(unsigned suite)
{
   return suite == TLS_SUITE_ECDHE_RSA_CHACHA20
       || suite == TLS_SUITE_ECDHE_ECDSA_CHACHA20;
}

/* Sends one record of @type carrying @len octets of @data, encrypting
 * once the client write keys are in force. */
static int tls_send_record(struct ssl_state *s, uint8_t type,
      const uint8_t *data, size_t len)
{
   uint8_t *rec;
   size_t   rec_len;
   int      ok;

   if (len > TLS_REC_MAX)
      return -1;

   if (!(rec = (uint8_t*)malloc(5 + len + TLS_REC_OVERHEAD)))
   {
      s->last_err = TLS_ERR_MEMORY;
      return -1;
   }
   rec[0] = type;
   rec[1] = 3;
   rec[2] = 3;

   if (!s->tx_encrypted)
   {
      tls_put16(rec + 3, (unsigned)len);
      memcpy(rec + 5, data, len);
      rec_len = 5 + len;
   }
   else
   {
      uint8_t aad[13];
      uint8_t nonce[12];
      uint8_t *body = rec + 5;
      size_t  i;

      crypto_store64_be(aad, s->cseq);
      aad[8] = type;
      aad[9] = 3;
      aad[10] = 3;
      tls_put16(aad + 11, (unsigned)len);

      if (tls_suite_is_chacha(s->suite))
      {
         /* nonce = write_iv XOR (0^4 || seq) */
         memcpy(nonce, s->civ, 12);
         for (i = 0; i < 8; i++)
            nonce[4 + i] ^= (uint8_t)(s->cseq >> (56 - 8 * i));
         if (aead_encrypt(AEAD_CHACHA20_POLY1305, s->cwk, 32, nonce, 12,
                  aad, 13, data, len, body, body + len, 16) != 0)
            goto fail;
         rec_len = 5 + len + 16;
      }
      else
      {
         /* nonce = write_iv (4) || explicit (8) = seq; explicit sent */
         struct aes_gcm_ctx g;
         memcpy(nonce, s->civ, 4);
         crypto_store64_be(nonce + 4, s->cseq);
         memcpy(body, nonce + 4, 8);
         if (aes_gcm_init(&g, s->cwk, 16) != 0
               || aes_gcm_encrypt(&g, nonce, 12, aad, 13, data, len,
                     body + 8, body + 8 + len) != 0)
         {
            crypto_memzero(&g, sizeof(g));
            goto fail;
         }
         crypto_memzero(&g, sizeof(g));
         rec_len = 5 + 8 + len + 16;
      }
      tls_put16(rec + 3, (unsigned)(rec_len - 5));
      s->cseq++;
   }

   ok = socket_send_all_blocking(s->fd, rec, rec_len, true);
   free(rec);
   if (!ok)
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   return 0;

fail:
   free(rec);
   s->last_err = TLS_ERR_CIPHER;
   return -1;
}

/* Reads one record into s->rx, decrypting in place once the server
 * write keys are in force. On return *type is the content type, and
 * *body / *len the plaintext. */
static int tls_read_record(struct ssl_state *s, uint8_t *type,
      const uint8_t **body, size_t *len)
{
   size_t rlen;

   if (socket_receive_all_blocking(s->fd, s->rx, 5) <= 0)
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   rlen = tls_get16(s->rx + 3);
   if (s->rx[1] != 3 || rlen > TLS_REC_MAX + 256 || rlen == 0)
   {
      s->last_err = TLS_ERR_RECORD;
      return -1;
   }
   if (socket_receive_all_blocking(s->fd, s->rx + 5, rlen) <= 0)
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   *type = s->rx[0];

   if (!s->rx_encrypted)
   {
      *body = s->rx + 5;
      *len  = rlen;
   }
   else
   {
      uint8_t aad[13];
      uint8_t nonce[12];
      uint8_t *p = s->rx + 5;
      size_t  plen;
      size_t  i;

      if (tls_suite_is_chacha(s->suite))
      {
         if (rlen < 16)
            goto bad;
         plen = rlen - 16;
         memcpy(nonce, s->siv, 12);
         for (i = 0; i < 8; i++)
            nonce[4 + i] ^= (uint8_t)(s->sseq >> (56 - 8 * i));
      }
      else
      {
         if (rlen < 8 + 16)
            goto bad;
         plen = rlen - 8 - 16;
         memcpy(nonce, s->siv, 4);
         memcpy(nonce + 4, p, 8);
         p += 8;
      }

      crypto_store64_be(aad, s->sseq);
      aad[8]  = *type;
      aad[9]  = 3;
      aad[10] = 3;
      tls_put16(aad + 11, (unsigned)plen);

      if (tls_suite_is_chacha(s->suite))
      {
         if (aead_decrypt(AEAD_CHACHA20_POLY1305, s->swk, 32, nonce, 12,
                  aad, 13, p, plen, p + plen, 16, p) != 0)
            goto bad;
      }
      else
      {
         struct aes_gcm_ctx g;
         int r = aes_gcm_init(&g, s->swk, 16);
         if (r == 0)
            r = aes_gcm_decrypt(&g, nonce, 12, aad, 13, p, plen, p + plen, p);
         crypto_memzero(&g, sizeof(g));
         if (r != 0)
            goto bad;
      }
      s->sseq++;
      *body = p;
      *len  = plen;
   }

   if (*type == TLS_CT_ALERT)
   {
      /* close_notify is a clean end; anything else is an error. */
      s->closed = 1;
      if (*len < 2 || (*body)[1] != 0)
         s->last_err = TLS_ERR_ALERT;
      return -1;
   }
   return 0;

bad:
   s->last_err = TLS_ERR_CIPHER;
   return -1;
}

static int tls_send_alert(struct ssl_state *s, uint8_t level, uint8_t desc)
{
   uint8_t a[2];
   a[0] = level;
   a[1] = desc;
   return tls_send_record(s, TLS_CT_ALERT, a, 2);
}

/* ---- handshake messages ------------------------------------------- */

/* Next handshake message from the accumulator, reading records until
 * one is complete; hashed into the transcript. */
static int tls_read_handshake(struct ssl_state *s, uint8_t *msg_type,
      const uint8_t **body, size_t *len)
{
   for (;;)
   {
      uint8_t type;
      const uint8_t *rb;
      size_t rl;

      if (s->hs_len - s->hs_off >= 4)
      {
         const uint8_t *m = s->hs + s->hs_off;
         size_t mlen = tls_get24(m + 1);
         if (s->hs_len - s->hs_off >= 4 + mlen)
         {
            *msg_type = m[0];
            *body     = m + 4;
            *len      = mlen;
            sha256_stream_update(&s->transcript, m, 4 + mlen);
            s->hs_off += 4 + mlen;
            return 0;
         }
      }

      /* Pull another record. Consumed messages stay where they are:
       * the parsed Certificate points into this buffer for the rest
       * of the handshake, and a whole handshake fits in it. */
      if (tls_read_record(s, &type, &rb, &rl) != 0)
         return -1;
      if (type != TLS_CT_HANDSHAKE)
      {
         s->last_err = TLS_ERR_HANDSHAKE;
         return -1;
      }
      if (s->hs_len + rl > TLS_HS_MAX)
      {
         s->last_err = TLS_ERR_HANDSHAKE;
         return -1;
      }
      memcpy(s->hs + s->hs_len, rb, rl);
      s->hs_len += rl;
   }
}

static int tls_send_handshake(struct ssl_state *s, uint8_t msg_type,
      const uint8_t *body, size_t len)
{
   uint8_t *m;
   int ret;
   if (!(m = (uint8_t*)malloc(4 + len)))
   {
      s->last_err = TLS_ERR_MEMORY;
      return -1;
   }
   m[0] = msg_type;
   m[1] = (uint8_t)(len >> 16);
   m[2] = (uint8_t)(len >> 8);
   m[3] = (uint8_t)len;
   memcpy(m + 4, body, len);
   sha256_stream_update(&s->transcript, m, 4 + len);
   ret = tls_send_record(s, TLS_CT_HANDSHAKE, m, 4 + len);
   free(m);
   return ret;
}

static int tls_send_client_hello(struct ssl_state *s)
{
   static const uint8_t suites[] = {
      0xc0, 0x2b, 0xc0, 0x2f, 0xcc, 0xa9, 0xcc, 0xa8
   };
   uint8_t *h;
   uint8_t *p;
   size_t   dlen = s->domain ? strlen(s->domain) : 0;
   size_t   ext_len;
   int      ret;

   if (dlen > 255)
      dlen = 0;

   if (!(h = (uint8_t*)malloc(200 + dlen)))
   {
      s->last_err = TLS_ERR_MEMORY;
      return -1;
   }
   p = h;
   *p++ = 3; *p++ = 3;                             /* client_version */
   memcpy(p, s->client_random, 32); p += 32;
   *p++ = 0;                                       /* session id */
   tls_put16(p, sizeof(suites)); p += 2;
   memcpy(p, suites, sizeof(suites)); p += sizeof(suites);
   *p++ = 1; *p++ = 0;                             /* null compression */

   {
      uint8_t *ext_start = p + 2;
      uint8_t *e = ext_start;

      if (dlen)
      {
         /* server_name: one host_name entry */
         tls_put16(e, 0);                e += 2;
         tls_put16(e, (unsigned)(dlen + 5)); e += 2;
         tls_put16(e, (unsigned)(dlen + 3)); e += 2;
         *e++ = 0;
         tls_put16(e, (unsigned)dlen);   e += 2;
         memcpy(e, s->domain, dlen);     e += dlen;
      }
      /* supported_groups: secp256r1, secp384r1 */
      tls_put16(e, 10); e += 2; tls_put16(e, 6); e += 2;
      tls_put16(e, 4);  e += 2; tls_put16(e, 23); e += 2; tls_put16(e, 24); e += 2;
      /* ec_point_formats: uncompressed */
      tls_put16(e, 11); e += 2; tls_put16(e, 2); e += 2; *e++ = 1; *e++ = 0;
      /* signature_algorithms */
      tls_put16(e, 13); e += 2; tls_put16(e, 12); e += 2; tls_put16(e, 10); e += 2;
      tls_put16(e, 0x0403); e += 2;   /* ecdsa_secp256r1_sha256 */
      tls_put16(e, 0x0503); e += 2;   /* ecdsa_secp384r1_sha384 */
      tls_put16(e, 0x0401); e += 2;   /* rsa_pkcs1_sha256 */
      tls_put16(e, 0x0501); e += 2;   /* rsa_pkcs1_sha384 */
      tls_put16(e, 0x0601); e += 2;   /* rsa_pkcs1_sha512 */
      /* renegotiation_info: initial, empty */
      tls_put16(e, 0xff01); e += 2; tls_put16(e, 1); e += 2; *e++ = 0;

      ext_len = (size_t)(e - ext_start);
      tls_put16(p, (unsigned)ext_len);
      p = e;
   }

   ret = tls_send_handshake(s, TLS_HS_CLIENT_HELLO, h, (size_t)(p - h));
   free(h);
   return ret;
}

static int tls_recv_server_hello(struct ssl_state *s)
{
   uint8_t type;
   const uint8_t *b;
   size_t len, sid;
   unsigned suite;

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_SERVER_HELLO || len < 38)
      goto bad;
   if (b[0] != 3 || b[1] != 3)
   {
      s->last_err = TLS_ERR_VERSION;
      return -1;
   }
   memcpy(s->server_random, b + 2, 32);
   sid = b[34];
   if (len < 35 + sid + 3)
      goto bad;
   suite = tls_get16(b + 35 + sid);
   if (suite != TLS_SUITE_ECDHE_ECDSA_AES128_GCM && suite != TLS_SUITE_ECDHE_RSA_AES128_GCM
         && suite != TLS_SUITE_ECDHE_RSA_CHACHA20 && suite != TLS_SUITE_ECDHE_ECDSA_CHACHA20)
      goto bad;
   if (b[35 + sid + 2] != 0)
      goto bad;   /* compression */
   s->suite = suite;
   return 0;

bad:
   s->last_err = TLS_ERR_HANDSHAKE;
   return -1;
}

/* Certificate: 3-octet total length, then 3-octet length + DER each.
 * Parsed into an array of pointers into the accumulator; the leaf's
 * key is what verifies the ServerKeyExchange. */
static int tls_recv_certificate(struct ssl_state *s, struct x509_cert *leaf)
{
   uint8_t type;
   const uint8_t *b;
   const uint8_t *ders[16];
   size_t lens[16];
   size_t len, total, off;
   unsigned n = 0;
   char info[128];

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_CERTIFICATE || len < 3)
      goto bad;
   total = tls_get24(b);
   if (total + 3 != len)
      goto bad;
   off = 3;
   while (off + 3 <= len && n < 16)
   {
      size_t cl = tls_get24(b + off);
      off += 3;
      if (cl == 0 || off + cl > len)
         goto bad;
      ders[n] = b + off;
      lens[n] = cl;
      n++;
      off += cl;
   }
   if (!n)
      goto bad;
   if (x509_parse(leaf, ders[0], lens[0]) != 0 || leaf->key_type == X509_KEY_NONE)
   {
      s->last_err = TLS_ERR_CERT;
      return -1;
   }

   if (tls_verify_mode == 2)
   {
      ssl_socket_log_verify_disabled(s->domain);
      return 0;
   }
   x509_trust_load_pem(tls_trust_pem, tls_trust_pem_len);
   info[0] = '\0';
   if (x509_verify_chain(ders, lens, n, s->domain, time(NULL), info, sizeof(info)) != 0)
   {
      ssl_socket_log_verify_fail(tls_verify_mode == 0, s->domain, info);
      if (tls_verify_mode == 0)
      {
         s->last_err = TLS_ERR_CERT;
         return -1;
      }
   }
   return 0;

bad:
   s->last_err = TLS_ERR_HANDSHAKE;
   return -1;
}

/* ServerKeyExchange: ECParameters (named curve) || point || signed
 * over client_random || server_random || params. */
static int tls_recv_server_kex(struct ssl_state *s, const struct x509_cert *leaf,
      uint8_t *peer, size_t *peer_len)
{
   uint8_t type;
   const uint8_t *b, *params, *sig;
   size_t len, plen, sig_len, point_len;
   unsigned sig_alg;
   uint8_t digest[64];
   size_t  dlen;
   int ok = -1;

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_SERVER_KEX || len < 4)
      goto bad;
   params = b;
   if (b[0] != 3)                 /* named_curve */
      goto bad;
   s->group  = tls_get16(b + 1);
   point_len = b[3];
   if ((s->group != 23 && s->group != 24)
         || (s->group == 23 && point_len != 65)
         || (s->group == 24 && point_len != 97)
         || len < 4 + point_len + 4)
      goto bad;
   memcpy(peer, b + 4, point_len);
   *peer_len = point_len;
   plen      = 4 + point_len;

   sig_alg = tls_get16(b + plen);
   sig_len = tls_get16(b + plen + 2);
   sig     = b + plen + 4;
   if (plen + 4 + sig_len != len)
      goto bad;

   /* hash: sig_alg high octet is the hash (4 = sha256, 5 = 384, 6 = 512) */
   {
      uint8_t hash = (uint8_t)(sig_alg >> 8);
      uint8_t sigt = (uint8_t)sig_alg;
      if (hash == 4)
      {
         struct sha256_state h;
         sha256_stream_init(&h, 0);
         sha256_stream_update(&h, s->client_random, 32);
         sha256_stream_update(&h, s->server_random, 32);
         sha256_stream_update(&h, params, plen);
         sha256_stream_final(&h, digest);
         dlen = 32;
      }
      else if (hash == 5 || hash == 6)
      {
         struct sha512_state h;
         sha512_stream_init(&h, hash == 5);
         sha512_stream_update(&h, s->client_random, 32);
         sha512_stream_update(&h, s->server_random, 32);
         sha512_stream_update(&h, params, plen);
         sha512_stream_final(&h, digest);
         dlen = hash == 5 ? 48 : 64;
      }
      else
         goto bad;

      if (sigt == 1 && leaf->key_type == X509_KEY_RSA)
         ok = rsa_pkcs1_verify(leaf->rsa_n, leaf->rsa_n_len,
               leaf->rsa_e, leaf->rsa_e_len,
               dlen == 32 ? RSA_HASH_SHA256 : dlen == 48 ? RSA_HASH_SHA384 : RSA_HASH_SHA512,
               digest, dlen, sig, sig_len);
      else if (sigt == 3 && (leaf->key_type == X509_KEY_P256 || leaf->key_type == X509_KEY_P384))
         ok = x509_verify_ecdsa_digest(leaf, digest, dlen, sig, sig_len);
   }
   if (ok != 0)
   {
      s->last_err = TLS_ERR_SIGNATURE;
      return -1;
   }
   return 0;

bad:
   s->last_err = TLS_ERR_KEX;
   return -1;
}

static int tls_recv_server_done(struct ssl_state *s)
{
   uint8_t type;
   const uint8_t *b;
   size_t len;
   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_SERVER_DONE || len != 0)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   return 0;
}

static int tls_send_client_kex(struct ssl_state *s, const uint8_t *peer,
      size_t peer_len)
{
   uint8_t msg[1 + 97];
   uint8_t pub[97];
   size_t  slen = s->group == 23 ? 32 : 48;
   size_t  plen = s->group == 23 ? 65 : 97;
   unsigned tries;
   int r;

   for (tries = 0; tries < 8; tries++)
   {
      if (crypto_random_bytes(s->priv, slen) != 0)
      {
         s->last_err = TLS_ERR_KEX;
         return -1;
      }
      r = s->group == 23 ? p256_keygen(s->priv, pub) : p384_keygen(s->priv, pub);
      if (r == 0)
         break;
   }
   if (tries == 8)
   {
      s->last_err = TLS_ERR_KEX;
      return -1;
   }
   r = s->group == 23 ? p256_ecdh(s->priv, peer, s->premaster)
                      : p384_ecdh(s->priv, peer, s->premaster);
   crypto_memzero(s->priv, sizeof(s->priv));
   if (r != 0 || peer_len != plen)
   {
      s->last_err = TLS_ERR_KEX;
      return -1;
   }

   msg[0] = (uint8_t)plen;
   memcpy(msg + 1, pub, plen);
   return tls_send_handshake(s, TLS_HS_CLIENT_KEX, msg, 1 + plen);
}

static void tls_derive_keys(struct ssl_state *s)
{
   uint8_t key_block[2 * 32 + 2 * 12];
   size_t  key_len = tls_suite_is_chacha(s->suite) ? 32 : 16;
   size_t  iv_len  = tls_suite_is_chacha(s->suite) ? 12 : 4;
   size_t  slen    = s->group == 23 ? 32 : 48;
   const uint8_t *p = key_block;

   tls_prf(s->premaster, slen, "master secret",
         s->client_random, 32, s->server_random, 32, s->master, 48);
   crypto_memzero(s->premaster, sizeof(s->premaster));
   tls_prf(s->master, 48, "key expansion",
         s->server_random, 32, s->client_random, 32,
         key_block, 2 * key_len + 2 * iv_len);

   memcpy(s->cwk, p, key_len); p += key_len;
   memcpy(s->swk, p, key_len); p += key_len;
   memcpy(s->civ, p, iv_len);  p += iv_len;
   memcpy(s->siv, p, iv_len);
   crypto_memzero(key_block, sizeof(key_block));
}

static int tls_send_finished(struct ssl_state *s)
{
   struct sha256_state t = s->transcript;
   uint8_t hash[32];
   uint8_t verify[12];
   sha256_stream_final(&t, hash);
   tls_prf(s->master, 48, "client finished", hash, 32, NULL, 0, verify, 12);
   return tls_send_handshake(s, TLS_HS_FINISHED, verify, 12);
}

static int tls_recv_finished(struct ssl_state *s)
{
   struct sha256_state t = s->transcript;
   uint8_t hash[32];
   uint8_t verify[12];
   uint8_t type;
   const uint8_t *b;
   size_t len;

   /* Expected value is over the transcript before this message. */
   sha256_stream_final(&t, hash);
   tls_prf(s->master, 48, "server finished", hash, 32, NULL, 0, verify, 12);

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_FINISHED || len != 12 || !crypto_memeq_ct(b, verify, 12))
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   return 0;
}

static int tls_handshake(struct ssl_state *s)
{
   struct x509_cert leaf;
   uint8_t peer[97];
   size_t  peer_len = 0;
   uint8_t type;
   const uint8_t *b;
   size_t  len;
   static const uint8_t ccs = 1;

   sha256_stream_init(&s->transcript, 0);
   if (crypto_random_bytes(s->client_random, 32) != 0)
   {
      s->last_err = TLS_ERR_KEX;
      return -1;
   }

   if (tls_send_client_hello(s) != 0)                   return -1;
   if (tls_recv_server_hello(s) != 0)                   return -1;
   if (tls_recv_certificate(s, &leaf) != 0)             return -1;
   if (tls_recv_server_kex(s, &leaf, peer, &peer_len) != 0) return -1;
   if (tls_recv_server_done(s) != 0)                    return -1;
   if (tls_send_client_kex(s, peer, peer_len) != 0)     return -1;
   tls_derive_keys(s);

   if (tls_send_record(s, TLS_CT_CCS, &ccs, 1) != 0)    return -1;
   s->tx_encrypted = 1;
   s->cseq         = 0;
   if (tls_send_finished(s) != 0)                       return -1;

   /* Server ChangeCipherSpec, then its Finished under the new keys. */
   if (tls_read_record(s, &type, &b, &len) != 0)        return -1;
   if (type != TLS_CT_CCS || len != 1 || b[0] != 1)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   s->rx_encrypted = 1;
   s->sseq         = 0;
   if (tls_recv_finished(s) != 0)                       return -1;

   s->handshake_done = 1;
   /* The accumulator is done; application data has its own buffer. */
   s->hs_len = s->hs_off = 0;
   return 0;
}

/* ---- public API --------------------------------------------------- */

void ssl_socket_set_verify_mode(unsigned mode)
{
   tls_verify_mode = mode;
}

void* ssl_socket_init(int fd, const char *domain)
{
   struct ssl_state *s = (struct ssl_state*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->fd     = fd;
   s->domain = domain;
   s->rx     = (uint8_t*)malloc(TLS_RX_SIZE);
   s->hs     = (uint8_t*)malloc(TLS_HS_MAX);
   s->pt     = (uint8_t*)malloc(TLS_REC_MAX);
   if (!s->rx || !s->hs || !s->pt)
   {
      free(s->rx);
      free(s->hs);
      free(s->pt);
      free(s);
      return NULL;
   }
   return s;
}

int ssl_socket_last_error(void *state_data)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   return s ? s->last_err : 0;
}

int ssl_socket_connect(void *state_data,
      void *data, bool timeout_enable, bool nonblock)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   (void)nonblock;

   if (timeout_enable)
   {
      if (!socket_connect_with_timeout(s->fd, data, 5000))
         return -1;
      if (!socket_set_block(s->fd, true))
         return -1;
   }
   else if (socket_connect(s->fd, data))
      return -1;

   if (tls_handshake(s) != 0)
   {
      if (!s->closed)
         tls_send_alert(s, 2, 40);   /* fatal handshake_failure */
      return -1;
   }
   return 1;
}

/* Fills s->pt with the next application record. */
static int tls_fill(struct ssl_state *s)
{
   uint8_t type;
   const uint8_t *b;
   size_t len;

   for (;;)
   {
      if (tls_read_record(s, &type, &b, &len) != 0)
         return -1;
      if (type == TLS_CT_APPDATA)
      {
         memcpy(s->pt, b, len);
         s->pt_len = len;
         s->pt_off = 0;
         return 0;
      }
      /* Anything else after the handshake (a HelloRequest, a CCS) is
       * ignored; alerts were handled in tls_read_record. */
   }
}

int ssl_socket_receive_all_blocking(void *state_data, void *data_, size_t len)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   uint8_t *data       = (uint8_t*)data_;

   socket_set_block(s->fd, true);
   while (len)
   {
      size_t avail = s->pt_len - s->pt_off;
      if (!avail)
      {
         if (tls_fill(s) != 0)
            return -1;
         continue;
      }
      if (avail > len)
         avail = len;
      memcpy(data, s->pt + s->pt_off, avail);
      s->pt_off += avail;
      data      += avail;
      len       -= avail;
   }
   return 1;
}

ssize_t ssl_socket_receive_all_nonblocking(void *state_data,
      bool *error, void *data_, size_t len)
{
   /* Application reads through net_http go a record at a time: hand
    * back what is already decrypted, and only touch the socket when
    * it has a whole record waiting. */
   struct ssl_state *s = (struct ssl_state*)state_data;
   uint8_t *data       = (uint8_t*)data_;
   size_t   got        = 0;
   bool     rd         = true;
   bool     wr         = false;

   *error = false;
   if (s->pt_len == s->pt_off)
   {
      if (s->closed)
         return 0;
      if (!socket_wait(s->fd, &rd, &wr, 0) || !rd)
         return 0;
      socket_set_block(s->fd, true);
      if (tls_fill(s) != 0)
      {
         if (!s->closed)
            *error = true;
         return s->closed ? 0 : -1;
      }
   }
   got = s->pt_len - s->pt_off;
   if (got > len)
      got = len;
   memcpy(data, s->pt + s->pt_off, got);
   s->pt_off += got;
   return (ssize_t)got;
}

int ssl_socket_send_all_blocking(void *state_data, const void *data_,
      size_t len, bool no_signal)
{
   struct ssl_state *s  = (struct ssl_state*)state_data;
   const uint8_t *data  = (const uint8_t*)data_;
   (void)no_signal;

   socket_set_block(s->fd, true);
   while (len)
   {
      size_t n = len < TLS_REC_MAX ? len : TLS_REC_MAX;
      if (tls_send_record(s, TLS_CT_APPDATA, data, n) != 0)
         return -1;
      data += n;
      len  -= n;
   }
   return 1;
}

ssize_t ssl_socket_send_all_nonblocking(void *state_data, const void *data_,
      size_t len, bool no_signal)
{
   /* A record is written whole or not at all; the socket is blocking
    * for the duration of the write, as the other backends do. */
   if (ssl_socket_send_all_blocking(state_data, data_, len, no_signal) < 0)
      return -1;
   return (ssize_t)len;
}

void ssl_socket_close(void *state_data)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   if (s->handshake_done && !s->closed)
      tls_send_alert(s, 1, 0);   /* warning close_notify */
   socket_close(s->fd);
}

void ssl_socket_free(void *state_data)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   if (!s)
      return;
   crypto_memzero(s->master, sizeof(s->master));
   crypto_memzero(s->cwk, sizeof(s->cwk));
   crypto_memzero(s->swk, sizeof(s->swk));
   free(s->rx);
   free(s->hs);
   free(s->pt);
   free(s);
}

/* Weak no-op logging hooks so libretro-common links standalone;
 * RetroArch's network/tls_log.c provides the real ones. Under griffin
 * both are in one unit, where a weak twin would be a redefinition, and
 * MSVC has no weak symbols. */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(HAVE_GRIFFIN)
__attribute__((weak))
void ssl_socket_log_verify_fail(int mode_required, const char *domain,
      const char *verify_info)
{
   (void)mode_required;
   (void)domain;
   (void)verify_info;
}

__attribute__((weak))
void ssl_socket_log_verify_disabled(const char *domain)
{
   (void)domain;
}
#endif
