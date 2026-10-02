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
 * authentication against net/cacert.h, AES-128/256-GCM or
 * ChaCha20-Poly1305 records, SHA-256 or SHA-384 handshake and PRF as
 * the suite dictates; SNI sent.
 * No renegotiation, no resumption, no client certificates. The
 * handshake runs to completion inside ssl_socket_connect() as the
 * other backends' do. */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <retro_atomic.h>
#include <net/net_socket_ssl.h>
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <crypto/pk.h>
#include <crypto/x509.h>
#include <lrc_hash.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>

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
#define TLS_SUITE_ECDHE_ECDSA_AES256_GCM   0xc02c
#define TLS_SUITE_ECDHE_RSA_AES128_GCM     0xc02f
#define TLS_SUITE_ECDHE_RSA_AES256_GCM     0xc030
#define TLS_SUITE_ECDHE_RSA_CHACHA20       0xcca8
#define TLS_SUITE_ECDHE_ECDSA_CHACHA20     0xcca9
/* TLS 1.3 (RFC 8446), the two SHA-256 suites */
#define TLS13_AES_128_GCM_SHA256           0x1301
#define TLS13_AES_256_GCM_SHA384           0x1302
#define TLS13_CHACHA20_POLY1305_SHA256     0x1303
#define TLS_HS_ENCRYPTED_EXTENSIONS 8
#define TLS_HS_CERTIFICATE_VERIFY   15
#define TLS_HS_KEY_UPDATE           24
#define TLS_HS_NEW_SESSION_TICKET   4

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

/* Verification policy: written from the settings thread, read by
 * every connection thread, so it is an atomic that each connection
 * copies once at creation and keeps for its lifetime. 0 required,
 * 1 optional, 2 disabled. */
static retro_atomic_int_t tls_verify_mode_setting;

/* Session cache for resumption (RFC 5246 7.4.1.2 session ids, RFC 5077
 * tickets): one entry per host, oldest replaced. A resumed handshake
 * skips the certificate chain and the ECDHE exchange; the server
 * proves it holds the master secret from the verified full handshake
 * that produced the entry. Process-wide, never written to disk. */
#define TLS_CACHE_SLOTS   8
#define TLS_CACHE_MAX_AGE (6 * 60 * 60)

struct tls_session
{
   uint8_t *ticket;
   size_t   ticket_len;
   time_t   when;
   unsigned suite;
   size_t   sid_len;
   uint8_t  sid[32];
   uint8_t  master[48];
   /* TLS 1.3: the ticket doubles as the PSK identity, psk is the
    * resumption secret expanded with the ticket nonce */
   uint8_t  is13;
   uint8_t  psk[48];
   uint32_t age_add;
   char     host[256];
};

static struct tls_session tls_cache[TLS_CACHE_SLOTS];

/* Connections are made from several threads (updater, achievements,
 * cloud sync); the cache is the one thing they share. */
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
/* A flag, not an slock: every section it guards is a lookup, a few
 * copies and at most one allocation, nothing that waits on another
 * thread, and with no lock object to make there is none that can fail
 * to be made. */
static retro_atomic_int_t tls_cache_busy;
static void tls_cache_lock(void)
{
   while (!retro_atomic_cas_int(&tls_cache_busy, 0, 1))
      sthread_yield();
}
#define TLS_CACHE_LOCK()   tls_cache_lock()
#define TLS_CACHE_UNLOCK() retro_atomic_store_release_int(&tls_cache_busy, 0)
#else
#define TLS_CACHE_LOCK()   do { } while (0)
#define TLS_CACHE_UNLOCK() do { } while (0)
#endif

static struct tls_session *tls_cache_find(const char *host)
{
   unsigned i;
   if (!host || !*host)
      return NULL;
   for (i = 0; i < TLS_CACHE_SLOTS; i++)
      if (tls_cache[i].when && strcmp(tls_cache[i].host, host) == 0)
      {
         if (time(NULL) - tls_cache[i].when > TLS_CACHE_MAX_AGE)
            return NULL;
         return &tls_cache[i];
      }
   return NULL;
}

static struct tls_session *tls_cache_slot(const char *host)
{
   struct tls_session *e = tls_cache_find(host);
   unsigned i, oldest = 0;
   if (e)
      return e;
   for (i = 0; i < TLS_CACHE_SLOTS; i++)
   {
      if (!tls_cache[i].when)
      {
         oldest = i;
         break;
      }
      if (tls_cache[i].when < tls_cache[oldest].when)
         oldest = i;
   }
   e = &tls_cache[oldest];
   free(e->ticket);
   crypto_memzero(e, sizeof(*e));
   strlcpy(e->host, host, sizeof(e->host));
   return e;
}

/* The trust bundle is net/cacert.h's parts unless a test swapped one
 * in, which is then the single part. */
static const char         *tls_test_pem[1];
static size_t              tls_test_pem_len[1];
static const char *const  *tls_trust_parts = cacert_pem_parts;
static const size_t       *tls_trust_sizes = cacert_pem_sizes;
static unsigned            tls_trust_count = CACERT_PEM_PARTS;

void ssl_socket_retro_set_trust_pem(const char *pem, size_t len)
{
   if (pem)
   {
      tls_test_pem[0]     = pem;
      tls_test_pem_len[0] = len;
      tls_trust_parts     = (const char *const*)tls_test_pem;
      tls_trust_sizes     = tls_test_pem_len;
      tls_trust_count     = 1;
   }
   else
   {
      tls_trust_parts     = cacert_pem_parts;
      tls_trust_sizes     = cacert_pem_sizes;
      tls_trust_count     = CACERT_PEM_PARTS;
   }
}

struct ssl_state
{
   uint8_t *rx;             /* raw record buffer */
   uint8_t *hs;             /* handshake message accumulator */
   /* decrypted application data pending: a record's plaintext where
    * it was decrypted, in rx - the next record read comes only once
    * this is drained, so it is handed out with no copy of its own */
   const uint8_t *pt;
   const char *domain;
   size_t   rx_len;
   size_t   hs_len;
   size_t   hs_off;
   size_t   pt_len;
   size_t   pt_off;
   uint64_t cseq;
   uint64_t sseq;
   struct sha256_state transcript;
   struct sha512_state transcript384;
   int      fd;
   int      last_err;
   unsigned suite;
   unsigned group;          /* 23 P-256, 24 P-384 */
   uint8_t  client_random[32];
   uint8_t  server_random[32];
   uint8_t  master[48];
   uint8_t  cwk[32];
   uint8_t  swk[32];
   /* AES-GCM key schedules for cwk / swk, built when a key is first
    * used and again only when it changes, not once per record */
   struct tls_gcm_cache
   {
      struct aes_gcm_ctx g;
      uint8_t  key[32];
      unsigned key_len;             /* 0: nothing built yet */
   } wgcm, rgcm;
   uint8_t  civ[12];
   uint8_t  siv[12];
   uint8_t  priv[48];
   uint8_t  premaster[48];
   uint8_t  rx_encrypted;
   uint8_t  tx_encrypted;
   uint8_t  handshake_done;
   uint8_t  closed;
   /* Handshake flights are corked: the client's records go out in one
    * write, since sending ClientKeyExchange, ChangeCipherSpec and
    * Finished as three small writes trips Nagle against the server's
    * delayed ACK and costs a 40 ms stall per full handshake. */
   uint8_t *cork;
   size_t   cork_len;
   uint8_t  corked;
   /* One sealed application-data record not yet fully on the wire,
    * for ssl_socket_send_all_nonblocking(): bytes are sealed as they
    * are accepted, and what the socket did not take waits here.
    * Allocated on first use. */
   uint8_t *tx;
   size_t   tx_len;
   size_t   tx_off;
   uint8_t  resumed;        /* this connection resumed a session */
   uint8_t  expect_ticket;  /* server said it will send a NewSessionTicket */
   uint8_t  offered_sid[32];
   size_t   offered_sid_len;
   /* TLS 1.3 */
   unsigned verify_mode;    /* this connection's copy of the policy */
   uint8_t  v13;            /* the server chose 1.3 */
   uint8_t  pub[65];        /* our P-256 key share */
   uint8_t  xpriv[32];      /* our X25519 key share */
   uint8_t  xpub[32];
   uint8_t  hs_secret[48];  /* handshake secret, for the master derivation */
   uint8_t  c_app[48];      /* client/server application traffic secrets */
   uint8_t  s_app[48];
   uint8_t  s_hs[48];       /* server handshake traffic secret (Finished key) */
   uint8_t  c_hs[48];
   uint8_t  cert_hash[48];  /* transcript hash up to Certificate, for CertificateVerify */
   uint8_t  tls13_peer[65]; /* server key share from the ServerHello */
   size_t   tls13_peer_len;
   uint8_t  master13[48];   /* 1.3 master secret until the resumption secret is taken */
   uint8_t  res_master[48]; /* resumption master secret, for the tickets */
   uint8_t  psk[48];        /* PSK offered in this ClientHello */
   uint8_t  psk_offered;
   uint8_t  psk_accepted;
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

static int tls_suite_is_13(unsigned suite)
{
   return suite == TLS13_AES_128_GCM_SHA256 || suite == TLS13_CHACHA20_POLY1305_SHA256
      || suite == TLS13_AES_256_GCM_SHA384;
}

static int tls_suite_is_chacha(unsigned suite)
{
   return suite == TLS_SUITE_ECDHE_RSA_CHACHA20
       || suite == TLS_SUITE_ECDHE_ECDSA_CHACHA20
       || suite == TLS13_CHACHA20_POLY1305_SHA256;
}

static int tls_suite_is_aes256(unsigned suite)
{
   return suite == TLS_SUITE_ECDHE_RSA_AES256_GCM
       || suite == TLS_SUITE_ECDHE_ECDSA_AES256_GCM
       || suite == TLS13_AES_256_GCM_SHA384;
}

/* The GCM context for @key (cwk or swk), rebuilt only when the key
 * differs from the one it was built from. */
static const struct aes_gcm_ctx *tls_gcm(struct ssl_state *s,
      struct tls_gcm_cache *c, const uint8_t *key)
{
   unsigned klen = tls_suite_is_aes256(s->suite) ? 32 : 16;
   if (c->key_len != klen || memcmp(c->key, key, klen) != 0)
   {
      if (aes_gcm_init(&c->g, key, klen) != 0)
      {
         c->key_len = 0;
         return NULL;
      }
      memcpy(c->key, key, klen);
      c->key_len = klen;
   }
   return &c->g;
}

/* The AES-256-GCM suites run their PRF and transcript on SHA-384. */
static int tls_suite_is_sha384(unsigned suite)
{
   return tls_suite_is_aes256(suite);
}

/* TLS 1.2 PRF: P_<hash>(secret, label || seed), hash per suite. */
static void tls_prf(const struct ssl_state *s,
      const uint8_t *secret, size_t secret_len,
      const char *label, const uint8_t *seed1, size_t seed1_len,
      const uint8_t *seed2, size_t seed2_len, uint8_t *out, size_t out_len)
{
   union
   {
      struct hmac_sha256_ctx h256;
      struct hmac_sha384_ctx h384;
   } h;
   const int use384  = tls_suite_is_sha384(s->suite);
   const size_t hlen = use384 ? 48 : 32;
   uint8_t a[48];
   uint8_t p[48];
   size_t  label_len = strlen(label);

#define TLS_HMAC_INIT()   do { if (use384) hmac_sha384_init(&h.h384, secret, secret_len); else hmac_sha256_init(&h.h256, secret, secret_len); } while (0)
#define TLS_HMAC_UPD(d, l) do { if (use384) hmac_sha384_update(&h.h384, d, l); else hmac_sha256_update(&h.h256, d, l); } while (0)
#define TLS_HMAC_FIN(o)   do { if (use384) hmac_sha384_final(&h.h384, o); else hmac_sha256_final(&h.h256, o); } while (0)

   /* A(1) = HMAC(secret, A(0)), A(0) = label || seed */
   TLS_HMAC_INIT();
   TLS_HMAC_UPD((const uint8_t*)label, label_len);
   TLS_HMAC_UPD(seed1, seed1_len);
   TLS_HMAC_UPD(seed2, seed2_len);
   TLS_HMAC_FIN(a);

   while (out_len)
   {
      size_t take = out_len < hlen ? out_len : hlen;
      TLS_HMAC_INIT();
      TLS_HMAC_UPD(a, hlen);
      TLS_HMAC_UPD((const uint8_t*)label, label_len);
      TLS_HMAC_UPD(seed1, seed1_len);
      TLS_HMAC_UPD(seed2, seed2_len);
      TLS_HMAC_FIN(p);
      memcpy(out, p, take);
      out     += take;
      out_len -= take;

      TLS_HMAC_INIT();
      TLS_HMAC_UPD(a, hlen);
      TLS_HMAC_FIN(a);
   }
#undef TLS_HMAC_INIT
#undef TLS_HMAC_UPD
#undef TLS_HMAC_FIN
   crypto_memzero(a, sizeof(a));
   crypto_memzero(p, sizeof(p));
}

/* Transcript hash so far, in the suite's hash; both digests run until
 * the ServerHello picks one. */
static size_t tls_transcript(const struct ssl_state *s, uint8_t *out)
{
   if (tls_suite_is_sha384(s->suite))
   {
      struct sha512_state t = s->transcript384;
      sha512_stream_final(&t, out);
      return 48;
   }
   else
   {
      struct sha256_state t = s->transcript;
      sha256_stream_final(&t, out);
      return 32;
   }
}

/* ---- record layer ------------------------------------------------- */

/* Sends one record of @type carrying @len octets of @data, encrypting
 * once the client write keys are in force. */
#define TLS_CORK_MAX 1024

static void tls_cork(struct ssl_state *s)
{
   s->corked   = 1;
   s->cork_len = 0;
}

/* Sends what was corked in one write. */
static int tls_uncork(struct ssl_state *s)
{
   int ok = 1;
   s->corked = 0;
   if (s->cork_len)
      ok = socket_send_all_blocking(s->fd, s->cork, s->cork_len, true);
   s->cork_len = 0;
   if (!ok)
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   return 0;
}

/* Seal @len bytes of @type into @rec (room for 5 + len +
 * TLS_REC_OVERHEAD), advancing the write sequence.  *@out_len is the
 * record's length on the wire. */
static int tls_seal_record(struct ssl_state *s, uint8_t type,
      const uint8_t *data, size_t len, uint8_t *rec, size_t *out_len)
{
   size_t   rec_len;

   if (len > TLS_REC_MAX)
      return -1;

   rec[0] = type;
   rec[1] = 3;
   rec[2] = 3;

   if (!s->tx_encrypted)
   {
      tls_put16(rec + 3, (unsigned)len);
      memcpy(rec + 5, data, len);
      rec_len = 5 + len;
   }
   else if (s->v13)
   {
      /* TLS 1.3: outer type application_data, inner = data || type,
       * nonce = iv XOR seq, AAD = the 5-octet header */
      uint8_t aad[5];
      uint8_t nonce[12];
      uint8_t *body = rec + 5;
      size_t  i;

      rec[0] = TLS_CT_APPDATA;
      memcpy(body, data, len);
      body[len] = type;
      rec_len = 5 + len + 1 + 16;
      aad[0] = TLS_CT_APPDATA; aad[1] = 3; aad[2] = 3;
      tls_put16(aad + 3, (unsigned)(rec_len - 5));
      memcpy(nonce, s->civ, 12);
      for (i = 0; i < 8; i++)
         nonce[4 + i] ^= (uint8_t)(s->cseq >> (56 - 8 * i));
      if (tls_suite_is_chacha(s->suite))
      {
         if (aead_encrypt(AEAD_CHACHA20_POLY1305, s->cwk, 32, nonce, 12,
                  aad, 5, body, len + 1, body, body + len + 1, 16) != 0)
            goto fail;
      }
      else
      {
         const struct aes_gcm_ctx *g = tls_gcm(s, &s->wgcm, s->cwk);
         if (!g || aes_gcm_encrypt(g, nonce, 12, aad, 5, body, len + 1,
                     body, body + len + 1) != 0)
            goto fail;
      }
      tls_put16(rec + 3, (unsigned)(rec_len - 5));
      s->cseq++;
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
         const struct aes_gcm_ctx *g = tls_gcm(s, &s->wgcm, s->cwk);
         memcpy(nonce, s->civ, 4);
         crypto_store64_be(nonce + 4, s->cseq);
         memcpy(body, nonce + 4, 8);
         if (!g || aes_gcm_encrypt(g, nonce, 12, aad, 13, data, len,
                     body + 8, body + 8 + len) != 0)
            goto fail;
         rec_len = 5 + 8 + len + 16;
      }
      tls_put16(rec + 3, (unsigned)(rec_len - 5));
      s->cseq++;
   }

   *out_len = rec_len;
   return 0;

fail:
   s->last_err = TLS_ERR_CIPHER;
   return -1;
}

/* Put a pending non-blocking record on the wire before anything else
 * goes out, so records leave in sequence order. */
static int tls_tx_drain_blocking(struct ssl_state *s)
{
   if (s->tx_off >= s->tx_len)
      return 0;
   socket_set_block(s->fd, true);
   if (!socket_send_all_blocking(s->fd, s->tx + s->tx_off,
            s->tx_len - s->tx_off, true))
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   s->tx_off = s->tx_len = 0;
   return 0;
}

static int tls_send_record(struct ssl_state *s, uint8_t type,
      const uint8_t *data, size_t len)
{
   uint8_t *rec;
   size_t   rec_len;
   int      ok;

   if (len > TLS_REC_MAX)
      return -1;
   if (!s->corked && tls_tx_drain_blocking(s) != 0)
      return -1;

   if (!(rec = (uint8_t*)malloc(5 + len + TLS_REC_OVERHEAD)))
   {
      s->last_err = TLS_ERR_MEMORY;
      return -1;
   }
   if (tls_seal_record(s, type, data, len, rec, &rec_len) != 0)
   {
      free(rec);
      return -1;
   }

   if (s->corked)
   {
      if (s->cork_len + rec_len > TLS_CORK_MAX)
      {
         free(rec);
         s->last_err = TLS_ERR_CIPHER;
         return -1;
      }
      memcpy(s->cork + s->cork_len, rec, rec_len);
      s->cork_len += rec_len;
      free(rec);
      return 0;
   }
   ok = socket_send_all_blocking(s->fd, rec, rec_len, true);
   free(rec);
   if (!ok)
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   return 0;
}

/* Reads one record into s->rx, decrypting in place once the server
 * write keys are in force. On return *type is the content type, and
 * *body / *len the plaintext. */
/* One record, its body in *body: decrypted in place in s->rx, or -
 * when @dst is given and the record's plaintext fits @dst_cap - into
 * @dst, so application data lands where the caller wants it with no
 * copy. The AEADs check the tag before writing anything. */
static int tls_read_record_to(struct ssl_state *s, uint8_t *type,
      const uint8_t **body, size_t *len, uint8_t *dst, size_t dst_cap)
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

   if (!s->rx_encrypted || (s->v13 && *type == TLS_CT_CCS))
   {
      /* a 1.3 server may send a plaintext ChangeCipherSpec for
       * middlebox compatibility; it carries nothing */
      *body = s->rx + 5;
      *len  = rlen;
   }
   else if (s->v13)
   {
      uint8_t aad[5];
      uint8_t nonce[12];
      uint8_t *p = s->rx + 5;
      uint8_t *o;
      size_t  plen, i;
      int     r;

      if (*type != TLS_CT_APPDATA || rlen < 17)
         goto bad;
      plen = rlen - 16;
      o    = (dst && plen <= dst_cap) ? dst : p;
      aad[0] = s->rx[0]; aad[1] = 3; aad[2] = 3;
      tls_put16(aad + 3, (unsigned)rlen);
      memcpy(nonce, s->siv, 12);
      for (i = 0; i < 8; i++)
         nonce[4 + i] ^= (uint8_t)(s->sseq >> (56 - 8 * i));
      if (tls_suite_is_chacha(s->suite))
         r = aead_decrypt(AEAD_CHACHA20_POLY1305, s->swk, 32, nonce, 12,
               aad, 5, p, plen, p + plen, 16, o);
      else
      {
         const struct aes_gcm_ctx *g = tls_gcm(s, &s->rgcm, s->swk);
         r = g ? aes_gcm_decrypt(g, nonce, 12, aad, 5, p, plen, p + plen, o) : -1;
      }
      if (r != 0)
         goto bad;
      s->sseq++;
      /* inner: content || type || zero padding */
      while (plen && o[plen - 1] == 0)
         plen--;
      if (!plen)
         goto bad;
      *type = o[plen - 1];
      *body = o;
      *len  = plen - 1;
   }
   else
   {
      uint8_t aad[13];
      uint8_t nonce[12];
      uint8_t *p = s->rx + 5;
      uint8_t *o;
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

      o = (dst && *type == TLS_CT_APPDATA && plen <= dst_cap) ? dst : p;
      crypto_store64_be(aad, s->sseq);
      aad[8]  = *type;
      aad[9]  = 3;
      aad[10] = 3;
      tls_put16(aad + 11, (unsigned)plen);

      if (tls_suite_is_chacha(s->suite))
      {
         if (aead_decrypt(AEAD_CHACHA20_POLY1305, s->swk, 32, nonce, 12,
                  aad, 13, p, plen, p + plen, 16, o) != 0)
            goto bad;
      }
      else
      {
         const struct aes_gcm_ctx *g = tls_gcm(s, &s->rgcm, s->swk);
         if (!g || aes_gcm_decrypt(g, nonce, 12, aad, 13, p, plen, p + plen, o) != 0)
            goto bad;
      }
      s->sseq++;
      *body = o;
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

static int tls_read_record(struct ssl_state *s, uint8_t *type,
      const uint8_t **body, size_t *len)
{
   return tls_read_record_to(s, type, body, len, NULL, 0);
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
         sha512_stream_update(&s->transcript384, m, 4 + mlen);
            s->hs_off += 4 + mlen;
            return 0;
         }
      }

      /* Pull another record. Consumed messages stay where they are:
       * the parsed Certificate points into this buffer for the rest
       * of the handshake, and a whole handshake fits in it. */
      if (tls_read_record(s, &type, &rb, &rl) != 0)
         return -1;
      /* a 1.3 server's compatibility ChangeCipherSpec carries nothing */
      if (s->v13 && type == TLS_CT_CCS)
         continue;
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
   sha512_stream_update(&s->transcript384, m, 4 + len);
   ret = tls_send_record(s, TLS_CT_HANDSHAKE, m, 4 + len);
   free(m);
   return ret;
}

static int tls13_expand_label_h(size_t hlen, const uint8_t *secret, const char *label,
      const uint8_t *ctx, size_t ctx_len, uint8_t *out, size_t out_len);
static void tls13_hkdf_extract(size_t hlen, const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len, uint8_t *prk);
static void tls13_hmac(size_t hlen, const uint8_t *key, const uint8_t *data,
      size_t len, uint8_t *mac);
static void tls13_empty_hash(size_t hlen, uint8_t *out);

static int tls_send_client_hello(struct ssl_state *s)
{
   /* AES-GCM first where the CPU has AES instructions; ChaCha20-
    * Poly1305 first where AES runs in software (3DS, Vita, older
    * phones), as it costs several times less there. Servers that
    * weigh the client's order pick accordingly. */
   static const uint8_t suites_aes[] = {
      0x13, 0x01, 0x13, 0x03, 0x13, 0x02,              /* TLS 1.3 */
      0xc0, 0x2b, 0xc0, 0x2f, 0xcc, 0xa9, 0xcc, 0xa8, 0xc0, 0x2c, 0xc0, 0x30
   };
   static const uint8_t suites_chacha[] = {
      0x13, 0x03, 0x13, 0x01, 0x13, 0x02,              /* TLS 1.3 */
      0xcc, 0xa9, 0xcc, 0xa8, 0xc0, 0x2b, 0xc0, 0x2f, 0xc0, 0x2c, 0xc0, 0x30
   };
   const uint8_t *suites = crypto_aes_hw() ? suites_aes : suites_chacha;
   uint8_t *h;
   uint8_t *p;
   uint8_t  iplit[16];
   /* No server_name for an IP address: RFC 6066 3 allows host names
    * only, and the certificate is checked against the address anyway. */
   size_t   dlen = (s->domain && !x509_parse_ip(s->domain, iplit))
      ? strlen(s->domain) : 0;
   size_t   ext_len;
   int      ret;
   const struct tls_session *cached;
   size_t   ticket_len;
   uint8_t *ticket = NULL;
   size_t   cached_sid_len = 0;
   uint8_t  cached_sid[32];
   uint8_t  cached_is13 = 0;
   unsigned cached_suite13 = 0;
   size_t   bhlen = 32;          /* binder hash length: the ticket's suite */
   uint32_t obf_age = 0;

   /* the cache entry is copied out here, so nothing below reads it
    * once another thread may be rewriting the slot */
   TLS_CACHE_LOCK();
   cached     = tls_cache_find(s->domain);
   ticket_len = (cached && cached->ticket) ? cached->ticket_len : 0;
   if (ticket_len && (ticket = (uint8_t*)malloc(ticket_len)))
      memcpy(ticket, cached->ticket, ticket_len);
   else
      ticket_len = 0;
   if (cached && cached->sid_len)
   {
      memcpy(cached_sid, cached->sid, cached->sid_len);
      cached_sid_len = cached->sid_len;
   }
   if (cached && cached->is13 && ticket_len)
   {
      cached_is13 = 1;
      cached_suite13 = cached->suite;
      memcpy(s->psk, cached->psk, 48);
      obf_age = (uint32_t)((time(NULL) - cached->when) * 1000) + cached->age_add;
   }
   TLS_CACHE_UNLOCK();
   s->psk_offered  = 0;
   s->psk_accepted = 0;

   if (dlen > 255)
      dlen = 0;

   /* our key share, P-256: 1.3 uses it as is, 1.2 too when the server
    * picks that group */
   if (crypto_random_bytes(s->priv, 32) != 0 || p256_keygen(s->priv, s->pub) != 0
         || crypto_random_bytes(s->xpriv, 32) != 0 || x25519_base(s->xpub, s->xpriv) != 0)
   {
      free(ticket);
      s->last_err = TLS_ERR_KEX;
      return -1;
   }
   if (!(h = (uint8_t*)malloc(400 + dlen + ticket_len)))
   {
      free(ticket);
      s->last_err = TLS_ERR_MEMORY;
      return -1;
   }
   p = h;
   *p++ = 3; *p++ = 3;                             /* client_version */
   memcpy(p, s->client_random, 32); p += 32;
   /* session id: the cached one, or a random one - alongside a ticket
    * so the server can echo it to signal 1.2 resumption, and in any
    * case for 1.3 middlebox compatibility, where the server echoes it */
   s->offered_sid_len = 0;
   if (cached_sid_len)
   {
      memcpy(s->offered_sid, cached_sid, cached_sid_len);
      s->offered_sid_len = cached_sid_len;
   }
   else
   {
      crypto_random_bytes(s->offered_sid, 32);
      s->offered_sid_len = 32;
   }
   *p++ = (uint8_t)s->offered_sid_len;
   memcpy(p, s->offered_sid, s->offered_sid_len); p += s->offered_sid_len;
   tls_put16(p, sizeof(suites_aes)); p += 2;
   memcpy(p, suites, sizeof(suites_aes)); p += sizeof(suites_aes);
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
      /* supported_groups: x25519, secp256r1, secp384r1 */
      tls_put16(e, 10); e += 2; tls_put16(e, 8); e += 2;
      tls_put16(e, 6);  e += 2; tls_put16(e, 29); e += 2; tls_put16(e, 23); e += 2; tls_put16(e, 24); e += 2;
      /* ec_point_formats: uncompressed */
      tls_put16(e, 11); e += 2; tls_put16(e, 2); e += 2; *e++ = 1; *e++ = 0;
      /* signature_algorithms */
      tls_put16(e, 13); e += 2; tls_put16(e, 16); e += 2; tls_put16(e, 14); e += 2;
      tls_put16(e, 0x0403); e += 2;   /* ecdsa_secp256r1_sha256 */
      tls_put16(e, 0x0503); e += 2;   /* ecdsa_secp384r1_sha384 */
      tls_put16(e, 0x0804); e += 2;   /* rsa_pss_rsae_sha256 (1.3) */
      tls_put16(e, 0x0805); e += 2;   /* rsa_pss_rsae_sha384 (1.3) */
      tls_put16(e, 0x0401); e += 2;   /* rsa_pkcs1_sha256 */
      tls_put16(e, 0x0501); e += 2;   /* rsa_pkcs1_sha384 */
      tls_put16(e, 0x0601); e += 2;   /* rsa_pkcs1_sha512 */
      /* supported_versions: 1.3 then 1.2 */
      tls_put16(e, 43); e += 2; tls_put16(e, 5); e += 2; *e++ = 4;
      tls_put16(e, 0x0304); e += 2; tls_put16(e, 0x0303); e += 2;
      /* key_share: x25519 and secp256r1, so no server needs a retry */
      tls_put16(e, 51); e += 2; tls_put16(e, 2 + 4 + 32 + 4 + 65); e += 2;
      tls_put16(e, 4 + 32 + 4 + 65); e += 2;
      tls_put16(e, 29); e += 2; tls_put16(e, 32); e += 2;
      memcpy(e, s->xpub, 32); e += 32;
      tls_put16(e, 23); e += 2; tls_put16(e, 65); e += 2;
      memcpy(e, s->pub, 65); e += 65;
      /* renegotiation_info: initial, empty */
      tls_put16(e, 0xff01); e += 2; tls_put16(e, 1); e += 2; *e++ = 0;
      /* session_ticket (1.2): the cached ticket, or empty to say we
       * take them; a 1.3 ticket goes in pre_shared_key instead */
      tls_put16(e, 35); e += 2;
      if (ticket_len && !cached_is13)
      {
         tls_put16(e, (unsigned)ticket_len); e += 2;
         memcpy(e, ticket, ticket_len);
         e += ticket_len;
      }
      else
      {
         tls_put16(e, 0); e += 2;
      }
      /* psk_key_exchange_modes: psk_dhe_ke */
      tls_put16(e, 45); e += 2; tls_put16(e, 2); e += 2; *e++ = 1; *e++ = 1;
      if (cached_is13)
      {
         /* pre_shared_key, last: one identity (the ticket, obfuscated
          * age) and one binder, filled in once the rest is hashed */
         bhlen = cached_suite13 == TLS13_AES_256_GCM_SHA384 ? 48 : 32;
         tls_put16(e, 41); e += 2;
         tls_put16(e, (unsigned)(2 + 2 + ticket_len + 4 + 2 + 1 + bhlen)); e += 2;
         tls_put16(e, (unsigned)(2 + ticket_len + 4)); e += 2;
         tls_put16(e, (unsigned)ticket_len); e += 2;
         memcpy(e, ticket, ticket_len); e += ticket_len;
         e[0] = (uint8_t)(obf_age >> 24); e[1] = (uint8_t)(obf_age >> 16);
         e[2] = (uint8_t)(obf_age >> 8);  e[3] = (uint8_t)obf_age; e += 4;
         tls_put16(e, (unsigned)(bhlen + 1)); e += 2;   /* binders list */
         *e++ = (uint8_t)bhlen;
         memset(e, 0, bhlen); e += bhlen;   /* binder placeholder */
         s->psk_offered = 1;
      }

      ext_len = (size_t)(e - ext_start);
      tls_put16(p, (unsigned)ext_len);
      p = e;
   }

   if (s->psk_offered)
   {
      /* binder = HMAC(binder_key, Hash(header || ClientHello[..binders]))
       * with binder_key from the PSK's early secret */
      static const uint8_t zeros[48] = {0};
      uint8_t early[48], bkey[48], fkey[48], th[48], hdr[4], empty_hash[48];
      size_t  body_len = (size_t)(p - h), trunc = body_len - (bhlen + 3);
      hdr[0] = TLS_HS_CLIENT_HELLO;
      hdr[1] = (uint8_t)(body_len >> 16); hdr[2] = (uint8_t)(body_len >> 8); hdr[3] = (uint8_t)body_len;
      if (bhlen == 48)
      {
         struct sha512_state t = s->transcript384;
         sha512_stream_update(&t, hdr, 4);
         sha512_stream_update(&t, h, trunc);
         sha512_stream_final(&t, th);
      }
      else
      {
         struct sha256_state t = s->transcript;
         sha256_stream_update(&t, hdr, 4);
         sha256_stream_update(&t, h, trunc);
         sha256_stream_final(&t, th);
      }
      tls13_hkdf_extract(bhlen, zeros, bhlen, s->psk, bhlen, early);
      tls13_empty_hash(bhlen, empty_hash);
      if (tls13_expand_label_h(bhlen, early, "res binder", empty_hash, bhlen, bkey, bhlen) != 0
            || tls13_expand_label_h(bhlen, bkey, "finished", NULL, 0, fkey, bhlen) != 0)
      {
         free(h); free(ticket);
         s->last_err = TLS_ERR_KEX;
         return -1;
      }
      tls13_hmac(bhlen, fkey, th, bhlen, h + trunc + 3);
      crypto_memzero(early, 48); crypto_memzero(bkey, 48); crypto_memzero(fkey, 48);
   }
   ret = tls_send_handshake(s, TLS_HS_CLIENT_HELLO, h, (size_t)(p - h));
   free(h);
   free(ticket);
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
         && suite != TLS_SUITE_ECDHE_RSA_CHACHA20 && suite != TLS_SUITE_ECDHE_ECDSA_CHACHA20
         && suite != TLS_SUITE_ECDHE_ECDSA_AES256_GCM && suite != TLS_SUITE_ECDHE_RSA_AES256_GCM
         && !tls_suite_is_13(suite))
      goto bad;
   if (b[35 + sid + 2] != 0)
      goto bad;   /* compression */
   s->suite = suite;

   /* TLS 1.3: supported_versions says 0x0304, key_share carries the
    * server's share. A HelloRetryRequest (a ServerHello with the
    * fixed random) asks for another group; we offer P-256 only, which
    * every 1.3 server takes, so it is treated as a failure. */
   s->v13 = 0;
   s->tls13_peer_len = 0;
   if (len > 35 + sid + 3 + 2)
   {
      static const uint8_t hrr_random[8] = { 0xcf, 0x21, 0xad, 0x74, 0xe5, 0x9a, 0x61, 0x11 };
      size_t off  = 35 + sid + 3;
      size_t elen = tls_get16(b + off);
      off += 2;
      while (elen >= 4 && off + 4 <= len)
      {
         unsigned etype = tls_get16(b + off);
         size_t   edata = tls_get16(b + off + 2);
         const uint8_t *ed = b + off + 4;
         if (off + 4 + edata > len)
            goto bad;
         if (etype == 43 && edata == 2 && tls_get16(ed) == 0x0304)
            s->v13 = 1;
         if (etype == 41 && edata == 2 && tls_get16(ed) == 0 && s->psk_offered)
            s->psk_accepted = 1;
         if (etype == 51 && edata >= 4)
         {
            unsigned grp = tls_get16(ed);
            size_t   kl  = tls_get16(ed + 2);
            if (((grp == 23 && kl == 65) || (grp == 29 && kl == 32)) && 4 + kl == edata)
            {
               memcpy(s->tls13_peer, ed + 4, kl);
               s->tls13_peer_len = kl;
               s->group          = grp;
            }
         }
         off  += 4 + edata;
         elen  = (elen >= 4 + edata) ? elen - 4 - edata : 0;
      }
      if (memcmp(s->server_random, hrr_random, 8) == 0)
      {
         s->last_err = TLS_ERR_KEX;
         return -1;
      }
   }
   if (s->v13)
   {
      if (!tls_suite_is_13(suite) || !s->tls13_peer_len
            || sid != s->offered_sid_len || memcmp(b + 35, s->offered_sid, sid) != 0)
         goto bad;
      s->resumed = 0;
      s->expect_ticket = 0;
      return 0;
   }
   if (tls_suite_is_13(suite))
      goto bad;

   /* Resumption: the offered id echoed back, and the cached suite. */
   s->resumed = 0;
   if (sid && sid == s->offered_sid_len && memcmp(b + 35, s->offered_sid, sid) == 0)
   {
      const struct tls_session *cached;
      TLS_CACHE_LOCK();
      cached = tls_cache_find(s->domain);
      if (cached && cached->suite == suite)
      {
         memcpy(s->master, cached->master, 48);
         s->resumed = 1;
      }
      TLS_CACHE_UNLOCK();
      if (!s->resumed)
         goto bad;
   }
   /* extensions: session_ticket (35) present means a ticket follows */
   s->expect_ticket = 0;
   if (len > 35 + sid + 3 + 2)
   {
      size_t off  = 35 + sid + 3;
      size_t elen = tls_get16(b + off);
      off += 2;
      while (elen >= 4 && off + 4 <= len)
      {
         unsigned etype = tls_get16(b + off);
         size_t   edata = tls_get16(b + off + 2);
         if (etype == 35)
            s->expect_ticket = 1;
         off  += 4 + edata;
         elen  = (elen >= 4 + edata) ? elen - 4 - edata : 0;
      }
   }
   if (!s->resumed && sid)
   {
      /* a server-issued id for the cache once the handshake completes */
      memcpy(s->offered_sid, b + 35, sid);
      s->offered_sid_len = sid;
   }
   else if (!s->resumed)
      s->offered_sid_len = 0;
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

   if (s->verify_mode == 2)
   {
      ssl_socket_log_verify_disabled(s->domain);
      return 0;
   }
   x509_trust_load_pem_parts(tls_trust_parts, tls_trust_sizes, tls_trust_count);
   info[0] = '\0';
   if (x509_verify_chain(ders, lens, n, s->domain, time(NULL), info, sizeof(info)) != 0)
   {
      ssl_socket_log_verify_fail(s->verify_mode == 0, s->domain, info);
      if (s->verify_mode == 0)
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
   if ((s->group != 23 && s->group != 24 && s->group != 29)
         || (s->group == 23 && point_len != 65)
         || (s->group == 24 && point_len != 97)
         || (s->group == 29 && point_len != 32)
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

   /* hash: sig_alg high octet is the hash (4 = sha256, 5 = 384, 6 = 512);
    * the rsa_pss_rsae_* schemes (0x0804 / 0x0805) are an exception, and
    * a 1.2 server may pick them once they are offered for 1.3 */
   {
      uint8_t hash = (uint8_t)(sig_alg >> 8);
      uint8_t sigt = (uint8_t)sig_alg;
      int     pss  = 0;
      if (sig_alg == 0x0804) { hash = 4; pss = 1; }
      else if (sig_alg == 0x0805) { hash = 5; pss = 1; }
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

      if (pss && leaf->key_type == X509_KEY_RSA)
         ok = rsa_pss_verify(leaf->rsa_n, leaf->rsa_n_len,
               leaf->rsa_e, leaf->rsa_e_len,
               dlen == 32 ? RSA_HASH_SHA256 : RSA_HASH_SHA384,
               digest, dlen, sig, sig_len);
      else if (!pss && sigt == 1 && leaf->key_type == X509_KEY_RSA)
         ok = rsa_pkcs1_verify(leaf->rsa_n, leaf->rsa_n_len,
               leaf->rsa_e, leaf->rsa_e_len,
               dlen == 32 ? RSA_HASH_SHA256 : dlen == 48 ? RSA_HASH_SHA384 : RSA_HASH_SHA512,
               digest, dlen, sig, sig_len);
      else if (!pss && sigt == 3 && (leaf->key_type == X509_KEY_P256 || leaf->key_type == X509_KEY_P384))
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
   size_t  slen = s->group == 24 ? 48 : 32;
   size_t  plen = s->group == 23 ? 65 : s->group == 24 ? 97 : 32;
   unsigned tries;
   int r;

   if (s->group == 29)
   {
      /* X25519: the share made with the ClientHello, shared secret
       * straight from the ladder */
      if (peer_len != 32 || x25519(s->premaster, s->xpriv, peer) != 0)
      {
         s->last_err = TLS_ERR_KEX;
         return -1;
      }
      crypto_memzero(s->xpriv, sizeof(s->xpriv));
      msg[0] = 32;
      memcpy(msg + 1, s->xpub, 32);
      return tls_send_handshake(s, TLS_HS_CLIENT_KEX, msg, 33);
   }
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

/* NewSessionTicket (RFC 5077): lifetime, then the opaque ticket. */
static int tls_recv_new_session_ticket(struct ssl_state *s)
{
   uint8_t type;
   const uint8_t *b;
   size_t len, tlen;
   struct tls_session *e;

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != 4 || len < 6)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   tlen = tls_get16(b + 4);
   if (6 + tlen != len || tlen == 0 || tlen > 16384)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   TLS_CACHE_LOCK();
   e = tls_cache_slot(s->domain);
   if (e)
   {
      uint8_t *t = (uint8_t*)malloc(tlen);
      if (t)
      {
         memcpy(t, b + 6, tlen);
         free(e->ticket);
         e->ticket     = t;
         e->ticket_len = tlen;
         e->when       = time(NULL);
      }
   }
   TLS_CACHE_UNLOCK();
   return 0;
}

/* Master secret and suite into the cache once a full handshake has
 * finished; the server's session id too, when it gave one. */
static void tls_cache_store(struct ssl_state *s)
{
   struct tls_session *e;
   TLS_CACHE_LOCK();
   e = tls_cache_slot(s->domain);
   if (e)
   {
      memcpy(e->master, s->master, 48);
      e->is13    = 0;
      e->suite   = s->suite;
      e->sid_len = s->offered_sid_len;
      memcpy(e->sid, s->offered_sid, s->offered_sid_len);
      e->when    = time(NULL);
   }
   TLS_CACHE_UNLOCK();
}

/* Key block from a master secret already in s->master. */
static void tls_expand_keys(struct ssl_state *s)
{
   uint8_t key_block[2 * 32 + 2 * 12];
   size_t  key_len = (tls_suite_is_chacha(s->suite) || tls_suite_is_aes256(s->suite)) ? 32 : 16;
   size_t  iv_len  = tls_suite_is_chacha(s->suite) ? 12 : 4;
   const uint8_t *p = key_block;

   tls_prf(s, s->master, 48, "key expansion",
         s->server_random, 32, s->client_random, 32,
         key_block, 2 * key_len + 2 * iv_len);
   memcpy(s->cwk, p, key_len); p += key_len;
   memcpy(s->swk, p, key_len); p += key_len;
   memcpy(s->civ, p, iv_len);  p += iv_len;
   memcpy(s->siv, p, iv_len);
   crypto_memzero(key_block, sizeof(key_block));
}

static void tls_derive_keys(struct ssl_state *s)
{
   size_t slen = s->group == 24 ? 48 : 32;
   tls_prf(s, s->premaster, slen, "master secret",
         s->client_random, 32, s->server_random, 32, s->master, 48);
   crypto_memzero(s->premaster, sizeof(s->premaster));
   tls_expand_keys(s);
}

static int tls_send_finished(struct ssl_state *s)
{
   uint8_t hash[48];
   uint8_t verify[12];
   size_t  hlen = tls_transcript(s, hash);
   tls_prf(s, s->master, 48, "client finished", hash, hlen, NULL, 0, verify, 12);
   return tls_send_handshake(s, TLS_HS_FINISHED, verify, 12);
}

static int tls_recv_finished(struct ssl_state *s)
{
   uint8_t hash[48];
   uint8_t verify[12];
   uint8_t type;
   const uint8_t *b;
   size_t len;
   size_t hlen;

   /* Expected value is over the transcript before this message. */
   hlen = tls_transcript(s, hash);
   tls_prf(s, s->master, 48, "server finished", hash, hlen, NULL, 0, verify, 12);

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_FINISHED || len != 12 || !crypto_memeq_ct(b, verify, 12))
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   return 0;
}

/* ---- TLS 1.3 (RFC 8446) ------------------------------------------- */

/* The 1.3 suites hash with SHA-256, except AES-256-GCM-SHA384. */
static size_t tls13_hlen(const struct ssl_state *s)
{
   return s->suite == TLS13_AES_256_GCM_SHA384 ? 48 : 32;
}

static void tls13_hkdf_extract(size_t hlen, const uint8_t *salt, size_t salt_len,
      const uint8_t *ikm, size_t ikm_len, uint8_t *prk)
{
   if (hlen == 48)
      hkdf_sha384_extract(salt, salt_len, ikm, ikm_len, prk);
   else
      hkdf_sha256_extract(salt, salt_len, ikm, ikm_len, prk);
}

static void tls13_hmac(size_t hlen, const uint8_t *key, const uint8_t *data,
      size_t len, uint8_t *mac)
{
   if (hlen == 48)
      hmac_sha384(key, hlen, data, len, mac);
   else
      hmac_sha256(key, hlen, data, len, mac);
}

/* Hash of the transcript so far, for the suite's hash. */
static void tls13_transcript_hash(const struct ssl_state *s, uint8_t *out)
{
   if (tls13_hlen(s) == 48)
   {
      struct sha512_state t = s->transcript384;
      sha512_stream_final(&t, out);
   }
   else
   {
      struct sha256_state t = s->transcript;
      sha256_stream_final(&t, out);
   }
}

static void tls13_empty_hash(size_t hlen, uint8_t *out)
{
   if (hlen == 48)
   {
      struct sha512_state e;
      sha512_stream_init(&e, 1);
      sha512_stream_final(&e, out);
   }
   else
   {
      struct sha256_state e;
      sha256_stream_init(&e, 0);
      sha256_stream_final(&e, out);
   }
}

/* HKDF-Expand-Label(secret, label, context, len) over the suite's hash;
 * @hlen is the hash and secret length. */
static int tls13_expand_label_h(size_t hlen, const uint8_t *secret, const char *label,
      const uint8_t *ctx, size_t ctx_len, uint8_t *out, size_t out_len)
{
   uint8_t info[2 + 1 + 6 + 32 + 1 + 255];   /* a ticket nonce may be 255 octets */
   size_t  llen = strlen(label), n = 0;
   if (llen > 32 || ctx_len > 255)
      return -1;
   tls_put16(info, (unsigned)out_len);            n += 2;
   info[n++] = (uint8_t)(6 + llen);
   memcpy(info + n, "tls13 ", 6);                 n += 6;
   memcpy(info + n, label, llen);                 n += llen;
   info[n++] = (uint8_t)ctx_len;
   if (ctx_len)
   {
      memcpy(info + n, ctx, ctx_len);
      n += ctx_len;
   }
   if (hlen == 48)
      return hkdf_sha384_expand(secret, 48, info, n, out, out_len);
   return hkdf_sha256_expand(secret, 32, info, n, out, out_len);
}

/* the connection's suite decides the hash */
static int tls13_expand_label(const struct ssl_state *s, const uint8_t *secret,
      const char *label, const uint8_t *ctx, size_t ctx_len, uint8_t *out, size_t out_len)
{
   return tls13_expand_label_h(tls13_hlen(s), secret, label, ctx, ctx_len, out, out_len);
}

/* Derive-Secret(secret, label, transcript-so-far) */
static int tls13_derive_secret(const struct ssl_state *s, const uint8_t *secret,
      const char *label, uint8_t *out)
{
   uint8_t th[48];
   size_t  hlen = tls13_hlen(s);
   tls13_transcript_hash(s, th);
   return tls13_expand_label(s, secret, label, th, hlen, out, hlen);
}

/* Traffic keys from a traffic secret into the given key / iv slots. */
static int tls13_traffic_keys(struct ssl_state *s, const uint8_t *secret,
      uint8_t *key, uint8_t *iv)
{
   size_t klen = (tls_suite_is_chacha(s->suite) || tls_suite_is_aes256(s->suite)) ? 32 : 16;
   if (tls13_expand_label(s, secret, "key", NULL, 0, key, klen) != 0
         || tls13_expand_label(s, secret, "iv", NULL, 0, iv, 12) != 0)
      return -1;
   return 0;
}

/* The handshake secrets once the ServerHello is in: shared secret from
 * the key share, then the client and server handshake traffic keys. */
static int tls13_handshake_keys(struct ssl_state *s, const uint8_t *peer, size_t peer_len)
{
   static const uint8_t zeros[48] = {0};
   uint8_t shared[32], early[48], derived[48];
   uint8_t empty_hash[48];
   size_t  hlen = tls13_hlen(s);

   if (s->group == 29)
   {
      if (peer_len != 32 || x25519(shared, s->xpriv, peer) != 0)
         return -1;
   }
   else if (peer_len != 65 || p256_ecdh(s->priv, peer, shared) != 0)
      return -1;
   crypto_memzero(s->xpriv, sizeof(s->xpriv));
   crypto_memzero(s->priv, sizeof(s->priv));
   /* early_secret = Extract(0, PSK) - or Extract(0, 0) without one;
    * derived = Derive-Secret(early, "derived", "") */
   tls13_hkdf_extract(hlen, zeros, hlen, s->psk_accepted ? s->psk : zeros, hlen, early);
   tls13_empty_hash(hlen, empty_hash);
   if (tls13_expand_label(s, early, "derived", empty_hash, hlen, derived, hlen) != 0)
      return -1;
   tls13_hkdf_extract(hlen, derived, hlen, shared, 32, s->hs_secret);
   crypto_memzero(shared, sizeof(shared));
   if (tls13_derive_secret(s, s->hs_secret, "c hs traffic", s->c_hs) != 0
         || tls13_derive_secret(s, s->hs_secret, "s hs traffic", s->s_hs) != 0)
      return -1;
   /* the server encrypts from here; we switch to our handshake keys
    * when we send our Finished */
   if (tls13_traffic_keys(s, s->s_hs, s->swk, s->siv) != 0)
      return -1;
   s->rx_encrypted = 1;
   s->sseq         = 0;
   return 0;
}

/* Application traffic secrets, from the transcript through the
 * server's Finished. */
static int tls13_application_secrets(struct ssl_state *s)
{
   static const uint8_t zeros[48] = {0};
   uint8_t derived[48], master[48], empty_hash[48];
   size_t  hlen = tls13_hlen(s);
   tls13_empty_hash(hlen, empty_hash);
   if (tls13_expand_label(s, s->hs_secret, "derived", empty_hash, hlen, derived, hlen) != 0)
      return -1;
   tls13_hkdf_extract(hlen, derived, hlen, zeros, hlen, master);
   if (tls13_derive_secret(s, master, "c ap traffic", s->c_app) != 0
         || tls13_derive_secret(s, master, "s ap traffic", s->s_app) != 0)
      return -1;
   memcpy(s->master13, master, hlen);
   crypto_memzero(master, sizeof(master));
   crypto_memzero(derived, sizeof(derived));
   return 0;
}

/* verify_data = HMAC(Expand-Label(base, "finished", "", 32), transcript) */
static int tls13_finished_mac(const struct ssl_state *s, const uint8_t *base, uint8_t *out)
{
   uint8_t fk[48], th[48];
   size_t  hlen = tls13_hlen(s);
   if (tls13_expand_label(s, base, "finished", NULL, 0, fk, hlen) != 0)
      return -1;
   tls13_transcript_hash(s, th);
   tls13_hmac(hlen, fk, th, hlen, out);
   crypto_memzero(fk, sizeof(fk));
   return 0;
}

/* Certificate (1.3 shape): request context, then entries of cert +
 * extensions. Verifies the chain and leaves the leaf key in @leaf. */
static int tls13_recv_certificate(struct ssl_state *s, struct x509_cert *leaf)
{
   uint8_t type;
   const uint8_t *b, *ders[16];
   size_t len, off, total, lens[16], n = 0;
   char info[128];

   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_CERTIFICATE || len < 4 || b[0] != 0)
      goto bad;
   total = tls_get24(b + 1);
   if (total + 4 != len)
      goto bad;
   off = 4;
   while (off + 3 <= len && n < 16)
   {
      size_t cl = tls_get24(b + off), el;
      off += 3;
      if (cl == 0 || off + cl + 2 > len)
         goto bad;
      ders[n] = b + off;
      lens[n] = cl;
      n++;
      off += cl;
      el   = tls_get16(b + off);
      off += 2 + el;
      if (off > len)
         goto bad;
   }
   if (!n || x509_parse(leaf, ders[0], lens[0]) != 0 || leaf->key_type == X509_KEY_NONE)
   {
      s->last_err = TLS_ERR_CERT;
      return -1;
   }
   if (s->verify_mode == 2)
   {
      ssl_socket_log_verify_disabled(s->domain);
      return 0;
   }
   x509_trust_load_pem_parts(tls_trust_parts, tls_trust_sizes, tls_trust_count);
   info[0] = '\0';
   if (x509_verify_chain(ders, lens, n, s->domain, time(NULL), info, sizeof(info)) != 0)
   {
      ssl_socket_log_verify_fail(s->verify_mode == 0, s->domain, info);
      if (s->verify_mode == 0)
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

/* CertificateVerify: the signature covers 64 spaces, the context
 * string, a zero and the transcript hash through Certificate. */
static int tls13_recv_certificate_verify(struct ssl_state *s, const struct x509_cert *leaf)
{
   static const char ctx[] = "TLS 1.3, server CertificateVerify";
   uint8_t type;
   const uint8_t *b, *sig;
   size_t len, sig_len;
   unsigned alg;
   uint8_t content[64 + 33 + 1 + 48];
   uint8_t digest[64];
   size_t  dlen, clen, hlen = tls13_hlen(s);
   int ok = -1;

   /* transcript hash before this message: the accumulator has not been
    * fed the CertificateVerify yet, so take it now */
   tls13_transcript_hash(s, s->cert_hash);
   clen = 64 + 33 + 1 + hlen;
   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_CERTIFICATE_VERIFY || len < 4)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   alg     = tls_get16(b);
   sig_len = tls_get16(b + 2);
   sig     = b + 4;
   if (4 + sig_len != len)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   memset(content, 0x20, 64);
   memcpy(content + 64, ctx, 33);
   content[64 + 33] = 0;
   memcpy(content + 64 + 33 + 1, s->cert_hash, hlen);

   if (alg == 0x0804 || alg == 0x0403)           /* SHA-256 based */
   {
      struct sha256_state h;
      sha256_stream_init(&h, 0);
      sha256_stream_update(&h, content, clen);
      sha256_stream_final(&h, digest);
      dlen = 32;
   }
   else if (alg == 0x0805 || alg == 0x0503)      /* SHA-384 based */
   {
      struct sha512_state h;
      sha512_stream_init(&h, 1);
      sha512_stream_update(&h, content, clen);
      sha512_stream_final(&h, digest);
      dlen = 48;
   }
   else
   {
      s->last_err = TLS_ERR_SIGNATURE;
      return -1;
   }
   if ((alg == 0x0804 || alg == 0x0805) && leaf->key_type == X509_KEY_RSA)
      ok = rsa_pss_verify(leaf->rsa_n, leaf->rsa_n_len, leaf->rsa_e, leaf->rsa_e_len,
            dlen == 32 ? RSA_HASH_SHA256 : RSA_HASH_SHA384, digest, dlen, sig, sig_len);
   else if ((alg == 0x0403 && leaf->key_type == X509_KEY_P256)
         || (alg == 0x0503 && leaf->key_type == X509_KEY_P384))
      ok = x509_verify_ecdsa_digest(leaf, digest, dlen, sig, sig_len);
   if (ok != 0)
   {
      s->last_err = TLS_ERR_SIGNATURE;
      return -1;
   }
   return 0;
}

static int tls13_recv_finished(struct ssl_state *s)
{
   uint8_t type, expect[48];
   const uint8_t *b;
   size_t len, hlen = tls13_hlen(s);
   /* the expected MAC is over the transcript before this message */
   if (tls13_finished_mac(s, s->s_hs, expect) != 0)
      return -1;
   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_FINISHED || len != hlen || !crypto_memeq_ct(b, expect, hlen))
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   return 0;
}

static int tls13_send_finished(struct ssl_state *s)
{
   uint8_t mac[48];
   if (tls13_finished_mac(s, s->c_hs, mac) != 0)
      return -1;
   return tls_send_handshake(s, TLS_HS_FINISHED, mac, tls13_hlen(s));
}

/* The whole 1.3 handshake after the ServerHello has been read and its
 * key share handed over. */
static int tls13_handshake(struct ssl_state *s, const uint8_t *peer, size_t peer_len)
{
   struct x509_cert leaf;
   uint8_t type;
   const uint8_t *b;
   size_t len;
   static const uint8_t ccs = 1;

   if (tls13_handshake_keys(s, peer, peer_len) != 0)
   {
      s->last_err = TLS_ERR_KEX;
      return -1;
   }
   /* EncryptedExtensions: nothing we act on */
   if (tls_read_handshake(s, &type, &b, &len) != 0)
      return -1;
   if (type != TLS_HS_ENCRYPTED_EXTENSIONS)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   if (s->psk_accepted)
   {
      /* resumed: the server proves the PSK with its Finished alone */
      s->resumed = 1;
      if (tls13_recv_finished(s) != 0)                 return -1;
   }
   else
   {
      if (tls13_recv_certificate(s, &leaf) != 0)       return -1;
      if (tls13_recv_certificate_verify(s, &leaf) != 0) return -1;
      if (tls13_recv_finished(s) != 0)                 return -1;
   }

   /* application secrets come from the transcript through the server
    * Finished; our own Finished goes out under the handshake keys */
   if (tls13_application_secrets(s) != 0)              return -1;
   tls_cork(s);
   /* middlebox-compatibility ChangeCipherSpec, in the clear */
   s->tx_encrypted = 0;
   if (tls_send_record(s, TLS_CT_CCS, &ccs, 1) != 0)   return -1;
   if (tls13_traffic_keys(s, s->c_hs, s->cwk, s->civ) != 0) return -1;
   s->tx_encrypted = 1;
   s->cseq         = 0;
   if (tls13_send_finished(s) != 0)                    return -1;
   if (tls_uncork(s) != 0)                             return -1;

   /* resumption master secret: the transcript through our Finished */
   if (tls13_derive_secret(s, s->master13, "res master", s->res_master) != 0)
      return -1;
   crypto_memzero(s->master13, sizeof(s->master13));
   crypto_memzero(s->psk, sizeof(s->psk));

   /* both sides on application keys */
   if (tls13_traffic_keys(s, s->c_app, s->cwk, s->civ) != 0
         || tls13_traffic_keys(s, s->s_app, s->swk, s->siv) != 0)
      return -1;
   s->cseq = s->sseq = 0;
   crypto_memzero(s->hs_secret, sizeof(s->hs_secret));
   crypto_memzero(s->c_hs, sizeof(s->c_hs));
   crypto_memzero(s->s_hs, sizeof(s->s_hs));
   s->handshake_done = 1;
   s->hs_len = s->hs_off = 0;
   return 0;
}

/* A post-handshake message on the application stream: tickets are
 * dropped (no PSK resumption), a KeyUpdate rotates the server keys and
 * is answered with our own when asked. */
static int tls13_post_handshake(struct ssl_state *s, const uint8_t *msg, size_t len)
{
   if (len < 4)
      return -1;
   switch (msg[0])
   {
      case TLS_HS_NEW_SESSION_TICKET:
      {
         /* lifetime(4) age_add(4) nonce<0..255> ticket<1..2^16-1> exts */
         const uint8_t *b = msg + 4;
         size_t bl = len - 4, nl, tl;
         uint32_t age_add;
         uint8_t psk[48];
         struct tls_session *e;
         if (bl < 9)
            return -1;
         age_add = ((uint32_t)b[4] << 24) | ((uint32_t)b[5] << 16) | ((uint32_t)b[6] << 8) | b[7];
         nl = b[8];
         if (bl < 9 + nl + 2)
            return -1;
         tl = tls_get16(b + 9 + nl);
         if (!tl || tl > 16384 || bl < 9 + nl + 2 + tl)
            return -1;
         /* PSK = Expand-Label(res_master, "resumption", nonce, 32) */
         if (tls13_expand_label(s, s->res_master, "resumption", b + 9, nl, psk, tls13_hlen(s)) != 0)
            return -1;
         TLS_CACHE_LOCK();
         e = tls_cache_slot(s->domain);
         if (e)
         {
            uint8_t *t = (uint8_t*)malloc(tl);
            if (t)
            {
               memcpy(t, b + 9 + nl + 2, tl);
               free(e->ticket);
               e->ticket     = t;
               e->ticket_len = tl;
               e->is13       = 1;
               e->suite      = s->suite;
               e->sid_len    = 0;
               e->age_add    = age_add;
               memcpy(e->psk, psk, 48);
               e->when       = time(NULL);
            }
         }
         TLS_CACHE_UNLOCK();
         crypto_memzero(psk, sizeof(psk));
         return 0;
      }
      case TLS_HS_KEY_UPDATE:
      {
         uint8_t next[48];
         size_t  hlen = tls13_hlen(s);
         if (len != 5)
            return -1;
         if (tls13_expand_label(s, s->s_app, "traffic upd", NULL, 0, next, hlen) != 0)
            return -1;
         memcpy(s->s_app, next, hlen);
         if (tls13_traffic_keys(s, s->s_app, s->swk, s->siv) != 0)
            return -1;
         s->sseq = 0;
         if (msg[4] == 1)
         {
            /* update_requested: rotate ours too, announcing it first */
            uint8_t ku[1] = {0};
            if (tls_send_handshake(s, TLS_HS_KEY_UPDATE, ku, 1) != 0)
               return -1;
            if (tls13_expand_label(s, s->c_app, "traffic upd", NULL, 0, next, hlen) != 0)
               return -1;
            memcpy(s->c_app, next, hlen);
            if (tls13_traffic_keys(s, s->c_app, s->cwk, s->civ) != 0)
               return -1;
            s->cseq = 0;
         }
         return 0;
      }
      default:
         return -1;
   }
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
   sha512_stream_init(&s->transcript384, 1);
   if (crypto_random_bytes(s->client_random, 32) != 0)
   {
      s->last_err = TLS_ERR_KEX;
      return -1;
   }

   if (tls_send_client_hello(s) != 0)                   return -1;
   if (tls_recv_server_hello(s) != 0)                   return -1;

   if (s->v13)
      return tls13_handshake(s, s->tls13_peer, s->tls13_peer_len);

   if (s->resumed)
   {
      /* Abbreviated: the server goes first with the cached master. */
      tls_expand_keys(s);
      if (s->expect_ticket && tls_recv_new_session_ticket(s) != 0)
         return -1;
      if (tls_read_record(s, &type, &b, &len) != 0)     return -1;
      if (type != TLS_CT_CCS || len != 1 || b[0] != 1)
      {
         s->last_err = TLS_ERR_HANDSHAKE;
         return -1;
      }
      s->rx_encrypted = 1;
      s->sseq         = 0;
      if (tls_recv_finished(s) != 0)                    return -1;
      tls_cork(s);
      if (tls_send_record(s, TLS_CT_CCS, &ccs, 1) != 0) return -1;
      s->tx_encrypted = 1;
      s->cseq         = 0;
      if (tls_send_finished(s) != 0)                    return -1;
      if (tls_uncork(s) != 0)                           return -1;
      tls_cache_store(s);
      s->handshake_done = 1;
      s->hs_len = s->hs_off = 0;
      return 0;
   }

   if (tls_recv_certificate(s, &leaf) != 0)             return -1;
   if (tls_recv_server_kex(s, &leaf, peer, &peer_len) != 0) return -1;
   if (tls_recv_server_done(s) != 0)                    return -1;
   tls_cork(s);
   if (tls_send_client_kex(s, peer, peer_len) != 0)     return -1;
   tls_derive_keys(s);

   if (tls_send_record(s, TLS_CT_CCS, &ccs, 1) != 0)    return -1;
   s->tx_encrypted = 1;
   s->cseq         = 0;
   if (tls_send_finished(s) != 0)                       return -1;
   if (tls_uncork(s) != 0)                              return -1;

   /* NewSessionTicket when announced, then the server's
    * ChangeCipherSpec and its Finished under the new keys. */
   if (s->expect_ticket && tls_recv_new_session_ticket(s) != 0)
      return -1;
   if (tls_read_record(s, &type, &b, &len) != 0)        return -1;
   if (type != TLS_CT_CCS || len != 1 || b[0] != 1)
   {
      s->last_err = TLS_ERR_HANDSHAKE;
      return -1;
   }
   s->rx_encrypted = 1;
   s->sseq         = 0;
   if (tls_recv_finished(s) != 0)                       return -1;
   tls_cache_store(s);

   s->handshake_done = 1;
   /* The accumulator is done; application data has its own buffer. */
   s->hs_len = s->hs_off = 0;
   return 0;
}

/* ---- public API --------------------------------------------------- */

void ssl_socket_set_verify_mode(unsigned mode)
{
   retro_atomic_store_release_int(&tls_verify_mode_setting, (int)mode);
}

void* ssl_socket_init(int fd, const char *domain)
{
   struct ssl_state *s = (struct ssl_state*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->verify_mode = (unsigned)retro_atomic_load_acquire_int(&tls_verify_mode_setting);
   /* No Nagle on a TLS socket. Everything RetroArch does over TLS is
    * request-then-reply, and with Nagle the first request after a
    * resumed 1.2 handshake waits behind the unacknowledged Finished
    * for the server's delayed ACK: 40 ms on every such connection. */
#ifdef TCP_NODELAY
   {
      int one = 1;
      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
   }
#endif
   s->fd     = fd;
   /* A copy: the connection can outlive the caller's string - net_http
    * pools it after the transfer that opened it is gone, and a TLS 1.3
    * ticket arriving later is filed under this name. */
   s->domain = domain ? strdup(domain) : NULL;
   s->rx     = (uint8_t*)malloc(TLS_RX_SIZE);
   s->hs     = (uint8_t*)malloc(TLS_HS_MAX);
   s->cork   = (uint8_t*)malloc(TLS_CORK_MAX);
   if (!s->rx || !s->hs || !s->cork || (domain && !s->domain))
   {
      free(s->cork);
      free(s->rx);
      free(s->hs);
      free((char*)s->domain);
      free(s);
      return NULL;
   }
   return s;
}

unsigned ssl_socket_retro_version(void *state_data)
{
   const struct ssl_state *s = (const struct ssl_state*)state_data;
   if (!s || !s->handshake_done)
      return 0;
   return s->v13 ? 0x0304 : 0x0303;
}

unsigned ssl_socket_retro_suite(void *state_data)
{
   const struct ssl_state *s = (const struct ssl_state*)state_data;
   if (!s || !s->handshake_done)
      return 0;
   return s->suite;
}

int ssl_socket_retro_was_resumed(void *state_data)
{
   const struct ssl_state *s = (const struct ssl_state*)state_data;
   return s && s->handshake_done && s->resumed;
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

/* Points s->pt at the next application record's plaintext: in @dst
 * when given and the record fits @dst_cap, else in s->rx. */
static int tls_fill(struct ssl_state *s, uint8_t *dst, size_t dst_cap)
{
   uint8_t type;
   const uint8_t *b;
   size_t len;

   for (;;)
   {
      if (tls_read_record_to(s, &type, &b, &len, dst, dst_cap) != 0)
         return -1;
      if (type == TLS_CT_APPDATA)
      {
         s->pt     = b;
         s->pt_len = len;
         s->pt_off = 0;
         return 0;
      }
      /* A 1.3 server sends tickets and key updates on the application
       * stream; anything else after the handshake (a HelloRequest, a
       * CCS) is ignored. Alerts were handled in tls_read_record. */
      if (s->v13 && type == TLS_CT_HANDSHAKE)
      {
         size_t off = 0;
         while (off + 4 <= len)
         {
            size_t ml = tls_get24(b + off + 1);
            if (off + 4 + ml > len || tls13_post_handshake(s, b + off, 4 + ml) != 0)
            {
               s->last_err = TLS_ERR_HANDSHAKE;
               return -1;
            }
            off += 4 + ml;
         }
      }
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
         if (tls_fill(s, data, len) != 0)
            return -1;
         if (s->pt == data)
         {
            /* decrypted in place in the caller's buffer */
            s->pt_off = s->pt_len;
            data     += s->pt_len;
            len      -= s->pt_len;
         }
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
      /* The end of the stream - close_notify, or the connection gone -
       * is -1 with *error set, as a plain socket and the library
       * backends report it: net_http ends a body that runs to the close
       * on it. A 0 here would read as "nothing yet", and such a body
       * would never finish. */
      if (s->closed)
      {
         *error = true;
         return -1;
      }
      if (!socket_wait(s->fd, &rd, &wr, 0) || !rd)
         return 0;
      socket_set_block(s->fd, true);
      if (tls_fill(s, data, len) != 0)
      {
         *error = true;
         return -1;
      }
      if (s->pt == data)
      {
         /* decrypted in place in the caller's buffer */
         s->pt_off = s->pt_len;
         return (ssize_t)s->pt_len;
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
   if (tls_tx_drain_blocking(s) != 0)
      return -1;
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

/* Push the pending record as far as the socket takes it now.
 * 1: nothing left pending, 0: the socket is full, -1: error. */
static int tls_tx_push(struct ssl_state *s)
{
   ssize_t n;
   if (s->tx_off >= s->tx_len)
      return 1;
   socket_set_block(s->fd, false);
   if ((n = socket_send_all_nonblocking(s->fd, s->tx + s->tx_off,
               s->tx_len - s->tx_off, true)) < 0)
   {
      s->last_err = TLS_ERR_SOCKET;
      return -1;
   }
   s->tx_off += (size_t)n;
   return s->tx_off >= s->tx_len;
}

ssize_t ssl_socket_send_all_nonblocking(void *state_data, const void *data_,
      size_t len, bool no_signal)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   const uint8_t *data = (const uint8_t*)data_;
   size_t taken        = 0;
   (void)no_signal;

   if (!s->tx && !(s->tx = (uint8_t*)malloc(5 + TLS_REC_MAX + TLS_REC_OVERHEAD)))
   {
      s->last_err = TLS_ERR_MEMORY;
      return -1;
   }

   /* Seal a record only once the previous one is fully on the wire, so
    * at most one record is ever held back; what is taken is sealed,
    * and what the socket has not taken yet goes out on the next call
    * or ssl_socket_flush_nonblocking(). */
   for (;;)
   {
      size_t n;
      int    r = tls_tx_push(s);
      if (r < 0)
         return -1;
      if (!r || taken == len)
         break;
      n = len - taken;
      if (n > TLS_REC_MAX)
         n = TLS_REC_MAX;
      if (tls_seal_record(s, TLS_CT_APPDATA, data + taken, n,
               s->tx, &s->tx_len) != 0)
         return -1;
      s->tx_off = 0;
      taken    += n;
   }
   return (ssize_t)taken;
}

int ssl_socket_flush_nonblocking(void *state_data)
{
   return tls_tx_push((struct ssl_state*)state_data);
}

void ssl_socket_close(void *state_data)
{
   struct ssl_state *s = (struct ssl_state*)state_data;
   /* Not behind a half-sent record: the alert would have to wait for
    * it, and a peer that stopped reading is exactly when a transfer is
    * abandoned mid-upload. */
   if (s->handshake_done && !s->closed && s->tx_off >= s->tx_len)
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
   crypto_memzero(&s->wgcm, sizeof(s->wgcm));
   crypto_memzero(&s->rgcm, sizeof(s->rgcm));
   free(s->rx);
   free(s->hs);
   free(s->cork);
   free(s->tx);
   free((char*)s->domain);
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
