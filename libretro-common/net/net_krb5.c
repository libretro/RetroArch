/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_krb5.c).
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

/* Kerberos 5 enctypes aes128-cts-hmac-sha1-96 and aes256-cts-hmac-
 * sha1-96 (RFC 3961 framework, RFC 3962 AES profile). Everything a
 * KDC exchange and a GSS-API context need from the cipher: key
 * derivation, encryption with the confounder and the truncated HMAC,
 * checksums, string-to-key and the PRF. The protocol itself (AS/TGS
 * exchanges, AP-REQ, GSS wrapping) sits on top in this file. */

#include <stdlib.h>
#include <string.h>

#include <net/net_krb5.h>
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <lrc_hash.h>

#define KRB5_BLOCK 16

/* ---- RFC 3961 5.1: n-fold --------------------------------------- */

/* Fold @in (@len octets) onto @n octets: the input is repeated, each
 * copy rotated 13 bits further, up to the least common multiple of
 * the two lengths, and the copies are summed with end-around carry. */
static unsigned krb5_gcd(unsigned a, unsigned b)
{
   while (b)
   {
      unsigned t = a % b;
      a = b;
      b = t;
   }
   return a;
}

void krb5_nfold(const uint8_t *in, unsigned len, uint8_t *out, unsigned n)
{
   unsigned lcm   = n * len / krb5_gcd(n, len);
   unsigned i;
   unsigned byte  = 0;

   memset(out, 0, n);

   /* octet i of the whole lcm-long expansion is octet (i mod len) of
    * the copy number (i / len), that copy rotated right by 13 * copy
    * bits; walk the expansion from the end so the carry rides along */
   for (i = lcm; i-- > 0;)
   {
      unsigned copy = i / len;
      unsigned pos  = i % len;
      unsigned rot  = (13 * copy) % (len * 8);
      unsigned rb   = rot / 8, rbit = rot % 8;
      /* octet @pos of the rotated copy comes from octets pos-rb and
       * pos-rb-1 of the input, wrapping */
      unsigned src  = (pos + len - rb) % len;
      unsigned prev = (src + len - 1) % len;
      unsigned v;
      if (rbit)
         v = ((in[src] >> rbit) | (in[prev] << (8 - rbit))) & 0xff;
      else
         v = in[src];
      byte += v + out[i % n];
      out[i % n] = (uint8_t)byte;
      byte >>= 8;
   }
   /* end-around carry */
   while (byte)
   {
      for (i = n; i-- > 0;)
      {
         byte += out[i];
         out[i] = (uint8_t)byte;
         byte >>= 8;
         if (!byte)
            break;
      }
   }
}

/* ---- RFC 3961 5.1: DR and DK ------------------------------------ */

/* DR(key, constant): the constant is n-folded to one block, then the
 * cipher is chained on its own output until the key length is filled.
 * CBC-CTS on exactly one block is a bare block encryption, so E() here
 * is the block function. */
static void krb5_dr(const struct krb5_key *key, const uint8_t *constant,
      size_t constant_len, uint8_t *out)
{
   struct aes_ctx aes;
   uint8_t block[KRB5_BLOCK];
   size_t  done = 0;

   if (constant_len == KRB5_BLOCK)
      memcpy(block, constant, KRB5_BLOCK);
   else
      krb5_nfold(constant, (unsigned)constant_len, block, KRB5_BLOCK);

   aes_init(&aes, key->k, key->len);
   while (done < key->len)
   {
      size_t take = key->len - done;
      aes_encrypt_block(&aes, block, block);
      if (take > KRB5_BLOCK)
         take = KRB5_BLOCK;
      memcpy(out + done, block, take);
      done += take;
   }
   crypto_memzero(&aes, sizeof(aes));
   crypto_memzero(block, sizeof(block));
}

void krb5_derive_key(const struct krb5_key *key,
      const uint8_t *constant, size_t constant_len, struct krb5_key *out)
{
   /* random-to-key for AES is the identity */
   out->len     = key->len;
   out->enctype = key->enctype;
   krb5_dr(key, constant, constant_len, out->k);
}

/* The three keys a usage number yields: Kc (checksum, 0x99), Ke
 * (encryption, 0xAA) and Ki (integrity, 0x55). */
static void krb5_usage_key(const struct krb5_key *key, uint32_t usage,
      uint8_t kind, struct krb5_key *out)
{
   uint8_t c[5];
   crypto_store32_be(c, usage);
   c[4] = kind;
   krb5_derive_key(key, c, 5, out);
}

/* ---- RFC 3962 5: CBC with ciphertext stealing ------------------- */

/* Encrypt @len (>= 16) octets, zero IV. Blocks 1..n-1 in CBC; the
 * last, partial block is zero-padded and encrypted, and the last two
 * ciphertext blocks are swapped with the final one cut to the partial
 * length, so the ciphertext is as long as the plaintext. */
static void krb5_cts_encrypt(const struct aes_ctx *aes,
      const uint8_t *in, size_t len, uint8_t *out)
{
   uint8_t prev[KRB5_BLOCK];
   uint8_t last[KRB5_BLOCK];
   size_t  i, n_full = len / KRB5_BLOCK, tail = len % KRB5_BLOCK;

   memset(prev, 0, sizeof(prev));
   if (!tail)
   {
      /* whole blocks: plain CBC, then the last two swapped */
      for (i = 0; i < n_full; i++)
      {
         unsigned j;
         for (j = 0; j < KRB5_BLOCK; j++)
            prev[j] ^= in[i * KRB5_BLOCK + j];
         aes_encrypt_block(aes, prev, prev);
         memcpy(out + i * KRB5_BLOCK, prev, KRB5_BLOCK);
      }
      if (n_full > 1)
      {
         /* swap the last two blocks through temporaries: no copy
          * within @out itself */
         uint8_t cn[KRB5_BLOCK];
         memcpy(last, out + (n_full - 1) * KRB5_BLOCK, KRB5_BLOCK);
         memcpy(cn,   out + (n_full - 2) * KRB5_BLOCK, KRB5_BLOCK);
         memcpy(out + (n_full - 1) * KRB5_BLOCK, cn,   KRB5_BLOCK);
         memcpy(out + (n_full - 2) * KRB5_BLOCK, last, KRB5_BLOCK);
      }
      return;
   }
   /* n_full whole blocks then a partial: the whole ones in CBC, the
    * last whole one is kept aside; the padded partial goes through
    * the cipher into that slot and the kept block is cut in after */
   for (i = 0; i < n_full; i++)
   {
      unsigned j;
      for (j = 0; j < KRB5_BLOCK; j++)
         prev[j] ^= in[i * KRB5_BLOCK + j];
      aes_encrypt_block(aes, prev, prev);
      if (i + 1 < n_full)
         memcpy(out + i * KRB5_BLOCK, prev, KRB5_BLOCK);
   }
   memcpy(last, prev, KRB5_BLOCK);                 /* C_{n-1} */
   memset(prev, 0, sizeof(prev));
   memcpy(prev, in + n_full * KRB5_BLOCK, tail);   /* P_n padded */
   for (i = 0; i < KRB5_BLOCK; i++)
      prev[i] ^= last[i];
   aes_encrypt_block(aes, prev, prev);             /* C_n */
   memcpy(out + (n_full - 1) * KRB5_BLOCK, prev, KRB5_BLOCK);
   memcpy(out + n_full * KRB5_BLOCK, last, tail);
}

static void krb5_cts_decrypt(const struct aes_ctx *aes,
      const uint8_t *in, size_t len, uint8_t *out)
{
   uint8_t prev[KRB5_BLOCK];
   uint8_t cur[KRB5_BLOCK];
   uint8_t d[KRB5_BLOCK];
   uint8_t cn1[KRB5_BLOCK];
   uint8_t cn[KRB5_BLOCK];
   size_t  i, n_full = len / KRB5_BLOCK, tail = len % KRB5_BLOCK;
   unsigned j;

   /* every ciphertext block is copied to @cur before its plaintext is
    * stored, and the swapped pair is taken before the loop, so @out
    * may alias @in */
   memset(prev, 0, sizeof(prev));
   if (!tail)
   {
      if (n_full == 1)
      {
         aes_decrypt_block(aes, in, out);
         return;
      }
      memcpy(cn1, in + (n_full - 1) * KRB5_BLOCK, KRB5_BLOCK); /* C_{n-1} */
      memcpy(cn,  in + (n_full - 2) * KRB5_BLOCK, KRB5_BLOCK); /* C_n */
      for (i = 0; i < n_full; i++)
      {
         if (i == n_full - 2)
            memcpy(cur, cn1, KRB5_BLOCK);
         else if (i == n_full - 1)
            memcpy(cur, cn, KRB5_BLOCK);
         else
            memcpy(cur, in + i * KRB5_BLOCK, KRB5_BLOCK);
         aes_decrypt_block(aes, cur, d);
         for (j = 0; j < KRB5_BLOCK; j++)
            out[i * KRB5_BLOCK + j] = d[j] ^ prev[j];
         memcpy(prev, cur, KRB5_BLOCK);
      }
      return;
   }
   /* the swapped pair: slot n-1 holds C_n, slot n the cut C_{n-1} */
   memcpy(cn, in + (n_full - 1) * KRB5_BLOCK, KRB5_BLOCK);
   memcpy(cn1, in + n_full * KRB5_BLOCK, tail);
   for (i = 0; i + 1 < n_full; i++)
   {
      memcpy(cur, in + i * KRB5_BLOCK, KRB5_BLOCK);
      aes_decrypt_block(aes, cur, d);
      for (j = 0; j < KRB5_BLOCK; j++)
         out[i * KRB5_BLOCK + j] = d[j] ^ prev[j];
      memcpy(prev, cur, KRB5_BLOCK);
   }
   /* D(C_n) = P_n (padded) xor C_{n-1}: its tail supplies the stolen
    * octets of C_{n-1}, its head the partial plaintext */
   aes_decrypt_block(aes, cn, d);
   memcpy(cn1 + tail, d + tail, KRB5_BLOCK - tail);
   for (j = 0; j < tail; j++)
      out[n_full * KRB5_BLOCK + j] = d[j] ^ cn1[j];
   aes_decrypt_block(aes, cn1, d);
   for (j = 0; j < KRB5_BLOCK; j++)
      out[(n_full - 1) * KRB5_BLOCK + j] = d[j] ^ prev[j];
}

/* ---- RFC 3962 4: string-to-key ---------------------------------- */

int krb5_string_to_key(int enctype, const char *password, size_t password_len,
      const char *salt, size_t salt_len, uint32_t iterations,
      struct krb5_key *out)
{
   struct krb5_key tkey;
   static const uint8_t kerberos[8] = { 'k','e','r','b','e','r','o','s' };

   switch (enctype)
   {
      case KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96: tkey.len = 16; break;
      case KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96: tkey.len = 32; break;
      default:
         return -1;
   }
   tkey.enctype = enctype;
   if (pbkdf2_hmac_sha1((const uint8_t*)password, password_len,
            (const uint8_t*)salt, salt_len, iterations ? iterations : 4096,
            tkey.k, tkey.len) != 0)
      return -1;
   krb5_derive_key(&tkey, kerberos, sizeof(kerberos), out);
   crypto_memzero(&tkey, sizeof(tkey));
   return 0;
}

/* ---- RFC 3962 6: encrypt, decrypt, checksum, prf ---------------- */

size_t krb5_encrypt(const struct krb5_key *key, uint32_t usage,
      const uint8_t *conf, const uint8_t *in, size_t len, uint8_t *out)
{
   struct krb5_key ke, ki;
   struct aes_ctx  aes;
   struct hmac_sha1_ctx h;
   uint8_t mac[20];
   size_t  total = KRB5_BLOCK + len;

   /* plaintext = confounder | data, assembled in @out and encrypted in
    * place; the HMAC runs over the same plaintext first */
   krb5_usage_key(key, usage, 0xAA, &ke);
   krb5_usage_key(key, usage, 0x55, &ki);

   hmac_sha1_init(&h, ki.k, ki.len);
   hmac_sha1_update(&h, conf, KRB5_BLOCK);
   hmac_sha1_update(&h, in, len);
   hmac_sha1_final(&h, mac);

   memcpy(out, conf, KRB5_BLOCK);
   memmove(out + KRB5_BLOCK, in, len);
   aes_init(&aes, ke.k, ke.len);
   krb5_cts_encrypt(&aes, out, total, out);
   memcpy(out + total, mac, 12);

   crypto_memzero(&aes, sizeof(aes));
   crypto_memzero(&ke, sizeof(ke));
   crypto_memzero(&ki, sizeof(ki));
   crypto_memzero(mac, sizeof(mac));
   return total + 12;
}

int krb5_decrypt(const struct krb5_key *key, uint32_t usage,
      const uint8_t *in, size_t len, uint8_t *out, size_t *out_len)
{
   struct krb5_key ke, ki;
   struct aes_ctx  aes;
   uint8_t  mac[20];
   uint8_t *plain;
   size_t   clen;
   unsigned i, diff = 0;

   if (len < KRB5_BLOCK + 12)
      return -1;
   clen = len - 12;

   krb5_usage_key(key, usage, 0xAA, &ke);
   krb5_usage_key(key, usage, 0x55, &ki);

   /* decrypt to a scratch copy: the confounder must not reach @out and
    * @out may alias @in */
   if (!(plain = (uint8_t*)malloc(clen)))
      return -1;
   aes_init(&aes, ke.k, ke.len);
   krb5_cts_decrypt(&aes, in, clen, plain);
   hmac_sha1(ki.k, ki.len, plain, clen, mac);
   for (i = 0; i < 12; i++)
      diff |= mac[i] ^ in[clen + i];

   if (!diff)
   {
      memcpy(out, plain + KRB5_BLOCK, clen - KRB5_BLOCK);
      *out_len = clen - KRB5_BLOCK;
   }
   crypto_memzero(plain, clen);
   free(plain);
   crypto_memzero(&aes, sizeof(aes));
   crypto_memzero(&ke, sizeof(ke));
   crypto_memzero(&ki, sizeof(ki));
   return diff ? -1 : 0;
}

void krb5_checksum(const struct krb5_key *key, uint32_t usage,
      const uint8_t *data, size_t len, uint8_t *mac)
{
   struct krb5_key kc;
   uint8_t full[20];
   krb5_usage_key(key, usage, 0x99, &kc);
   hmac_sha1(kc.k, kc.len, data, len, full);
   memcpy(mac, full, 12);
   crypto_memzero(&kc, sizeof(kc));
   crypto_memzero(full, sizeof(full));
}

int krb5_raw_cts(const uint8_t *key, size_t key_len, int decrypt,
      const uint8_t *in, size_t len, uint8_t *out)
{
   struct aes_ctx aes;
   if (len < KRB5_BLOCK || aes_init(&aes, key, key_len) != 0)
      return -1;
   if (decrypt)
      krb5_cts_decrypt(&aes, in, len, out);
   else
      krb5_cts_encrypt(&aes, in, len, out);
   crypto_memzero(&aes, sizeof(aes));
   return 0;
}

void krb5_prf(const struct krb5_key *key, const uint8_t *data, size_t len,
      uint8_t *out)
{
   static const uint8_t prf[3] = { 'p','r','f' };
   struct krb5_key kp;
   struct aes_ctx  aes;
   uint8_t digest[20];

   SHA1Digest(data, len, digest);
   krb5_derive_key(key, prf, sizeof(prf), &kp);
   aes_init(&aes, kp.k, kp.len);
   /* one block: CTS reduces to the cipher */
   aes_encrypt_block(&aes, digest, out);
   crypto_memzero(&aes, sizeof(aes));
   crypto_memzero(&kp, sizeof(kp));
   crypto_memzero(digest, sizeof(digest));
}

/* ======================================================================
 * The protocol: DER, the KDC exchanges and the AP exchange
 * ====================================================================== */

#include <stdio.h>
#include <time.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <compat/strl.h>

/* ---- DER writer ------------------------------------------------ */

/* Content is written forward into a flat buffer; a constructed value
 * is closed by krb5_der_wrap(), which moves what was written since @start
 * up to make room for the tag and its minimal length. Messages here
 * are a few hundred octets, so the moves are nothing. */
struct krb5_der_w
{
   uint8_t *buf;
   size_t   len;
   size_t   cap;
   int      fail;
};

static void krb5_der_put(struct krb5_der_w *w, const uint8_t *p, size_t n)
{
   if (w->len + n > w->cap)
   {
      w->fail = 1;
      return;
   }
   memcpy(w->buf + w->len, p, n);
   w->len += n;
}

static void krb5_der_put_byte(struct krb5_der_w *w, uint8_t b)
{
   krb5_der_put(w, &b, 1);
}

static size_t krb5_der_len_size(size_t n)
{
   if (n < 128)
      return 1;
   if (n < 256)
      return 2;
   if (n < 65536)
      return 3;
   return 4;
}

/* Close a value: the octets from @start to the end become the content
 * of a TLV with @tag (already including the class / constructed bits). */
static void krb5_der_wrap(struct krb5_der_w *w, size_t start, uint8_t tag)
{
   size_t n   = w->len - start;
   size_t hdr = 1 + krb5_der_len_size(n);
   uint8_t *p;
   if (w->fail || w->len + hdr > w->cap)
   {
      w->fail = 1;
      return;
   }
   memmove(w->buf + start + hdr, w->buf + start, n);
   p    = w->buf + start;
   *p++ = tag;
   if (n < 128)
      *p++ = (uint8_t)n;
   else if (n < 256)
   {
      *p++ = 0x81; *p++ = (uint8_t)n;
   }
   else if (n < 65536)
   {
      *p++ = 0x82; *p++ = (uint8_t)(n >> 8); *p++ = (uint8_t)n;
   }
   else
   {
      *p++ = 0x83; *p++ = (uint8_t)(n >> 16); *p++ = (uint8_t)(n >> 8); *p++ = (uint8_t)n;
   }
   w->len += hdr;
}

#define KRB5_DER_SEQ        0x30
#define KRB5_DER_INT        0x02
#define KRB5_DER_BITSTR     0x03
#define KRB5_DER_OCTSTR     0x04
#define KRB5_DER_GENSTR     0x1b
#define KRB5_DER_GENTIME    0x18
#define KRB5_DER_CTX(n)     (uint8_t)(0xa0 | (n))     /* [n] explicit */
#define KRB5_DER_APP(n)     (uint8_t)(0x60 | (n))     /* [APPLICATION n] */

static void krb5_der_int32(struct krb5_der_w *w, int32_t v)
{
   size_t start = w->len;
   uint8_t b[4];
   unsigned n = 4, i;
   crypto_store32_be(b, (uint32_t)v);
   /* minimal two's complement */
   while (n > 1 && ((b[4 - n] == 0 && !(b[5 - n] & 0x80))
            || (b[4 - n] == 0xff && (b[5 - n] & 0x80))))
      n--;
   for (i = 4 - n; i < 4; i++)
      krb5_der_put_byte(w, b[i]);
   krb5_der_wrap(w, start, KRB5_DER_INT);
}

/* UInt32 values above INT32_MAX need a leading zero */
static void krb5_der_uint32(struct krb5_der_w *w, uint32_t v)
{
   size_t start = w->len;
   uint8_t b[5];
   unsigned n, i;
   b[0] = 0;
   crypto_store32_be(b + 1, v);
   n = 5;
   while (n > 1 && b[5 - n] == 0 && !(b[6 - n] & 0x80))
      n--;
   for (i = 5 - n; i < 5; i++)
      krb5_der_put_byte(w, b[i]);
   krb5_der_wrap(w, start, KRB5_DER_INT);
}

static void krb5_der_octets(struct krb5_der_w *w, uint8_t tag, const uint8_t *p, size_t n)
{
   size_t start = w->len;
   krb5_der_put(w, p, n);
   krb5_der_wrap(w, start, tag);
}

static void krb5_der_string(struct krb5_der_w *w, const char *s)
{
   krb5_der_octets(w, KRB5_DER_GENSTR, (const uint8_t*)s, strlen(s));
}

/* Wrap the last value written since @start in [n] */
static void krb5_der_ctx(struct krb5_der_w *w, size_t start, unsigned n)
{
   krb5_der_wrap(w, start, KRB5_DER_CTX(n));
}

/* KerberosTime "YYYYMMDDHHMMSSZ" from a UTC time_t. */
static void krb5_civil_from_days(int64_t z, int *y, unsigned *m, unsigned *d)
{
   int64_t era, doe, yoe, mp;
   z  += 719468;
   era = (z >= 0 ? z : z - 146096) / 146097;
   doe = z - era * 146097;
   yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
   *y  = (int)(yoe + era * 400);
   {
      int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
      mp  = (5 * doy + 2) / 153;
      *d  = (unsigned)(doy - (153 * mp + 2) / 5 + 1);
      *m  = (unsigned)(mp < 10 ? mp + 3 : mp - 9);
   }
   if (*m <= 2)
      (*y)++;
}

static void krb5_time_string(int64_t t, char *out)
{
   int y; unsigned m, d;
   int64_t days = t / 86400, secs = t % 86400;
   if (secs < 0)
   {
      secs += 86400;
      days--;
   }
   krb5_civil_from_days(days, &y, &m, &d);
   out[0]  = (char)('0' + (y / 1000) % 10); out[1] = (char)('0' + (y / 100) % 10);
   out[2]  = (char)('0' + (y / 10) % 10);   out[3] = (char)('0' + y % 10);
   out[4]  = (char)('0' + m / 10);          out[5] = (char)('0' + m % 10);
   out[6]  = (char)('0' + d / 10);          out[7] = (char)('0' + d % 10);
   out[8]  = (char)('0' + (int)(secs / 36000));       out[9]  = (char)('0' + (int)(secs / 3600 % 10));
   out[10] = (char)('0' + (int)(secs % 3600 / 600));  out[11] = (char)('0' + (int)(secs % 3600 / 60 % 10));
   out[12] = (char)('0' + (int)(secs % 60 / 10));     out[13] = (char)('0' + (int)(secs % 60 % 10));
   out[14] = 'Z';
   out[15] = '\0';
}

static void krb5_der_time(struct krb5_der_w *w, int64_t t)
{
   char s[16];
   krb5_time_string(t, s);
   krb5_der_octets(w, KRB5_DER_GENTIME, (const uint8_t*)s, 15);
}

/* PrincipalName: name-type [0], name-string [1] SEQUENCE OF string.
 * @name is "a/b/c" style; the separator splits components. */
static void krb5_der_principal(struct krb5_der_w *w, int type, const char *name)
{
   size_t seq = w->len, s;
   const char *p = name;
   s = w->len; krb5_der_int32(w, type); krb5_der_ctx(w, s, 0);
   s = w->len;
   {
      size_t inner = w->len;
      while (*p)
      {
         const char *e = strchr(p, '/');
         size_t   n    = e ? (size_t)(e - p) : strlen(p);
         krb5_der_octets(w, KRB5_DER_GENSTR, (const uint8_t*)p, n);
         p += n;
         if (*p == '/')
            p++;
      }
      krb5_der_wrap(w, inner, KRB5_DER_SEQ);
   }
   krb5_der_ctx(w, s, 1);
   krb5_der_wrap(w, seq, KRB5_DER_SEQ);
}

/* EncryptedData: etype [0], (kvno [1] if @kvno), cipher [2] */
static void krb5_der_encrypted(struct krb5_der_w *w, int etype, uint32_t kvno,
      const uint8_t *cipher, size_t len)
{
   size_t seq = w->len, s;
   s = w->len; krb5_der_int32(w, etype); krb5_der_ctx(w, s, 0);
   if (kvno)
   {
      s = w->len; krb5_der_uint32(w, kvno); krb5_der_ctx(w, s, 1);
   }
   s = w->len; krb5_der_octets(w, KRB5_DER_OCTSTR, cipher, len); krb5_der_ctx(w, s, 2);
   krb5_der_wrap(w, seq, KRB5_DER_SEQ);
}

/* ---- DER reader ------------------------------------------------ */

struct krb5_der_r
{
   const uint8_t *p;
   const uint8_t *end;
};

/* Read one TLV at the cursor: returns its tag, sets @c to its content
 * and advances past it. 0 on any malformation. */
static int krb5_der_next(struct krb5_der_r *r, uint8_t *tag, struct krb5_der_r *c)
{
   size_t n;
   const uint8_t *p = r->p;
   if (p + 2 > r->end)
      return 0;
   *tag = *p++;
   if ((*tag & 0x1f) == 0x1f)
      return 0;                       /* high tag numbers never occur */
   n = *p++;
   if (n & 0x80)
   {
      unsigned k = n & 0x7f, i;
      if (k == 0 || k > 4 || p + k > r->end)
         return 0;
      n = 0;
      for (i = 0; i < k; i++)
         n = (n << 8) | *p++;
   }
   if (n > (size_t)(r->end - p))
      return 0;
   c->p   = p;
   c->end = p + n;
   r->p   = p + n;
   return 1;
}

/* The next TLV, which must carry @want */
static int krb5_der_expect(struct krb5_der_r *r, uint8_t want, struct krb5_der_r *c)
{
   uint8_t tag;
   if (!krb5_der_next(r, &tag, c) || tag != want)
      return 0;
   return 1;
}

/* Peek: is the next TLV tagged @want? */
static int krb5_der_peek(const struct krb5_der_r *r, uint8_t want)
{
   return r->p < r->end && *r->p == want;
}

/* [n] explicit: enter the context tag and return its single content
 * TLV's content in @c, its tag in @tag. */
static int krb5_der_ctx_get(struct krb5_der_r *r, unsigned n, uint8_t *tag, struct krb5_der_r *c)
{
   struct krb5_der_r outer;
   if (!krb5_der_expect(r, KRB5_DER_CTX(n), &outer))
      return 0;
   if (!krb5_der_next(&outer, tag, c) || outer.p != outer.end)
      return 0;
   return 1;
}

static int krb5_der_get_int(struct krb5_der_r *c, int32_t *v)
{
   size_t n = (size_t)(c->end - c->p);
   int32_t acc;
   size_t i;
   if (n == 0 || n > 5)
      return 0;
   acc = (c->p[0] & 0x80) ? -1 : 0;
   for (i = 0; i < n; i++)
      acc = (int32_t)(((uint32_t)acc << 8) | c->p[i]);
   *v = acc;
   return 1;
}

static int krb5_der_ctx_int(struct krb5_der_r *r, unsigned n, int32_t *v)
{
   uint8_t tag;
   struct krb5_der_r c;
   return krb5_der_ctx_get(r, n, &tag, &c) && tag == KRB5_DER_INT && krb5_der_get_int(&c, v);
}

/* [n] wrapping an OCTET STRING / string: content span */
static int krb5_der_ctx_octets(struct krb5_der_r *r, unsigned n, uint8_t want, struct krb5_der_r *c)
{
   uint8_t tag;
   return krb5_der_ctx_get(r, n, &tag, c) && tag == want;
}

/* Skip an optional [n] */
static void krb5_der_ctx_skip(struct krb5_der_r *r, unsigned n)
{
   struct krb5_der_r c;
   if (krb5_der_peek(r, KRB5_DER_CTX(n)))
      krb5_der_expect(r, KRB5_DER_CTX(n), &c);
}

/* EncryptedData -> etype, kvno, cipher span */
static int krb5_der_get_encrypted(struct krb5_der_r *r, int32_t *etype, struct krb5_der_r *cipher)
{
   struct krb5_der_r seq;
   if (!krb5_der_expect(r, KRB5_DER_SEQ, &seq) || !krb5_der_ctx_int(&seq, 0, etype))
      return 0;
   krb5_der_ctx_skip(&seq, 1);
   return krb5_der_ctx_octets(&seq, 2, KRB5_DER_OCTSTR, cipher);
}

/* EncryptionKey -> krb5_key */
static int krb5_der_get_key(struct krb5_der_r *r, struct krb5_key *k)
{
   struct krb5_der_r seq, val;
   int32_t t;
   if (!krb5_der_expect(r, KRB5_DER_SEQ, &seq) || !krb5_der_ctx_int(&seq, 0, &t)
         || !krb5_der_ctx_octets(&seq, 1, KRB5_DER_OCTSTR, &val))
      return 0;
   k->len = (size_t)(val.end - val.p);
   if ((t != KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96 && t != KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96)
         || k->len != (t == KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96 ? 16u : 32u))
      return 0;
   memcpy(k->k, val.p, k->len);
   k->enctype = t;
   return 1;
}


/* ---- context ---------------------------------------------------- */

#define KRB5_MSG_AS_REQ   10
#define KRB5_MSG_AS_REP   11
#define KRB5_MSG_TGS_REQ  12
#define KRB5_MSG_TGS_REP  13
#define KRB5_MSG_AP_REQ   14
#define KRB5_MSG_AP_REP   15
#define KRB5_MSG_ERROR    30

#define KRB5_PA_TGS_REQ        1
#define KRB5_PA_ENC_TIMESTAMP  2
#define KRB5_PA_ETYPE_INFO2    19

#define KRB5_NT_PRINCIPAL      1
#define KRB5_NT_SRV_INST       2

#define KRB5_ERR_PREAUTH_REQUIRED 25

/* Key usages, RFC 4120 7.5.1 */
#define KU_PA_ENC_TS        1
#define KU_TICKET           2
#define KU_AS_REP           3
#define KU_TGS_REQ_AUTH_CKSUM 6
#define KU_TGS_REQ_AUTH     7
#define KU_TGS_REP_SESSION  8      /* no subkey in the authenticator */
#define KU_TGS_REP_SUBKEY   9
#define KU_AP_REQ_AUTH_CKSUM 10
#define KU_AP_REQ_AUTH      11
#define KU_AP_REP           12

#define KRB5_MAX_MSG 8192

struct krb5_ctx
{
   char     kdc[256];
   char     realm[128];
   char     client[128];
   uint16_t port;
   unsigned timeout;
   int64_t  time_offset;           /* KDC clock minus ours, from KRB-ERROR */
   char     error[128];
   /* the TGT */
   struct krb5_key tgt_key;
   uint8_t *tgt;                   /* Ticket, DER */
   size_t   tgt_len;
   /* the last service ticket */
   struct krb5_key svc_key;
   uint8_t *svc;
   size_t   svc_len;
   char     svc_name[128];
   /* the GSS context */
   struct krb5_key gss_key;        /* subkey we sent in the authenticator */
   uint32_t seq;
   int      ap_done;
};

static void krb5_err(struct krb5_ctx *c, const char *msg)
{
   strlcpy(c->error, msg, sizeof(c->error));
}

/* "KDC error N (why)" without snprintf, for the strict C89 lane */
static void krb5_kdc_err(struct krb5_ctx *c, int32_t code, const char *why)
{
   char num[16];
   int  i = 15;
   uint32_t v = (uint32_t)(code < 0 ? -code : code);
   num[i] = '\0';
   do
   {
      num[--i] = (char)('0' + v % 10);
      v /= 10;
   } while (v && i);
   if (code < 0 && i)
      num[--i] = '-';
   strlcpy(c->error, "KDC error ", sizeof(c->error));
   strlcat(c->error, num + i, sizeof(c->error));
   if (why)
      strlcat(c->error, why, sizeof(c->error));
}

struct krb5_ctx *krb5_new(void)
{
   struct krb5_ctx *c = (struct krb5_ctx*)calloc(1, sizeof(*c));
   if (c)
   {
      c->port    = 88;
      c->timeout = 10;
   }
   return c;
}

void krb5_free(struct krb5_ctx *c)
{
   if (!c)
      return;
   free(c->tgt);
   free(c->svc);
   crypto_memzero(c, sizeof(*c));
   free(c);
}

const char *krb5_get_error(const struct krb5_ctx *c)
{
   return c->error;
}

static int64_t krb5_now(const struct krb5_ctx *c)
{
   return (int64_t)time(NULL) + c->time_offset;
}

/* ---- transport: TCP with the 4-octet length ---------------------- */

static int krb5_exchange(struct krb5_ctx *c, const uint8_t *req, size_t len,
      uint8_t *rep, size_t *rep_len)
{
   struct addrinfo *addr = NULL;
   uint8_t hdr[4];
   int fd, to = (int)c->timeout * 1000;
   uint32_t n;

   fd = socket_init((void**)&addr, c->port, c->kdc, SOCKET_TYPE_STREAM, AF_UNSPEC);
   if (fd < 0 || !addr)
   {
      if (addr)
         freeaddrinfo_retro(addr);
      krb5_err(c, "cannot resolve KDC");
      return -1;
   }
   if (!socket_connect_with_timeout(fd, addr, to))
   {
      freeaddrinfo_retro(addr);
      socket_close(fd);
      krb5_err(c, "KDC connect failed");
      return -1;
   }
   freeaddrinfo_retro(addr);
   socket_set_block(fd, false);
   crypto_store32_be(hdr, (uint32_t)len);
   if (!socket_send_all_blocking_with_timeout(fd, hdr, 4, to, true)
         || !socket_send_all_blocking_with_timeout(fd, req, len, to, true)
         || !socket_receive_all_blocking_with_timeout(fd, hdr, 4, to))
   {
      socket_close(fd);
      krb5_err(c, "KDC send/receive failed");
      return -1;
   }
   n = crypto_load32_be(hdr) & 0x7fffffff;
   if (n > *rep_len || !socket_receive_all_blocking_with_timeout(fd, rep, n, to))
   {
      socket_close(fd);
      krb5_err(c, "KDC reply too large or truncated");
      return -1;
   }
   socket_close(fd);
   *rep_len = n;
   return 0;
}

/* ---- KRB-ERROR --------------------------------------------------- */

/* Parse a KRB-ERROR: the code, the KDC's time (for our offset) and,
 * for preauth-required, the ETYPE-INFO2 salt and enctype. */
static int krb5_parse_error(struct krb5_ctx *c, struct krb5_der_r *r, int32_t *code,
      char *salt, size_t salt_cap, int32_t *etype)
{
   struct krb5_der_r app, seq, t, edata;
   int32_t v;
   *etype = 0;
   salt[0] = '\0';
   if (!krb5_der_expect(r, KRB5_DER_APP(30), &app) || !krb5_der_expect(&app, KRB5_DER_SEQ, &seq))
      return 0;
   if (!krb5_der_ctx_int(&seq, 0, &v) || !krb5_der_ctx_int(&seq, 1, &v))
      return 0;
   krb5_der_ctx_skip(&seq, 2);
   krb5_der_ctx_skip(&seq, 3);
   if (krb5_der_ctx_octets(&seq, 4, KRB5_DER_GENTIME, &t) && t.end - t.p == 15)
   {
      /* the KDC's idea of now, to correct clock skew on the retry */
      const char *s = (const char*)t.p;
      int y = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
      unsigned mo = (s[4]-'0')*10 + (s[5]-'0'), d = (s[6]-'0')*10 + (s[7]-'0');
      unsigned h = (s[8]-'0')*10 + (s[9]-'0'), mi = (s[10]-'0')*10 + (s[11]-'0'), se = (s[12]-'0')*10 + (s[13]-'0');
      int64_t yy = y - (mo <= 2), era = (yy >= 0 ? yy : yy - 399) / 400, yoe = yy - era * 400;
      int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
      int64_t days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
      c->time_offset = days * 86400 + h * 3600 + mi * 60 + se - (int64_t)time(NULL);
   }
   krb5_der_ctx_skip(&seq, 5);
   if (!krb5_der_ctx_int(&seq, 6, code))
      return 0;
   krb5_der_ctx_skip(&seq, 7); krb5_der_ctx_skip(&seq, 8);
   krb5_der_ctx_skip(&seq, 9); krb5_der_ctx_skip(&seq, 10); krb5_der_ctx_skip(&seq, 11);
   if (*code == KRB5_ERR_PREAUTH_REQUIRED && krb5_der_ctx_octets(&seq, 12, KRB5_DER_OCTSTR, &edata))
   {
      /* METHOD-DATA: SEQUENCE OF PA-DATA; find ETYPE-INFO2 and take
       * the first entry with an enctype we speak */
      struct krb5_der_r md, pa, val, ent, inner;
      int32_t ptype;
      if (!krb5_der_expect(&edata, KRB5_DER_SEQ, &md))
         return 1;
      while (md.p < md.end)
      {
         if (!krb5_der_expect(&md, KRB5_DER_SEQ, &pa) || !krb5_der_ctx_int(&pa, 1, &ptype)
               || !krb5_der_ctx_octets(&pa, 2, KRB5_DER_OCTSTR, &val))
            break;
         if (ptype != KRB5_PA_ETYPE_INFO2 || !krb5_der_expect(&val, KRB5_DER_SEQ, &inner))
            continue;
         while (inner.p < inner.end && !*etype)
         {
            int32_t et;
            struct krb5_der_r sv;
            if (!krb5_der_expect(&inner, KRB5_DER_SEQ, &ent) || !krb5_der_ctx_int(&ent, 0, &et))
               break;
            if (et != KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96 && et != KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96)
               continue;
            *etype = et;
            if (krb5_der_peek(&ent, KRB5_DER_CTX(1)) && krb5_der_ctx_octets(&ent, 1, KRB5_DER_GENSTR, &sv))
            {
               size_t n = (size_t)(sv.end - sv.p);
               if (n < salt_cap)
               {
                  memcpy(salt, sv.p, n);
                  salt[n] = '\0';
               }
            }
         }
      }
   }
   return 1;
}

/* ---- KDC-REQ ----------------------------------------------------- */

/* The req-body, shared by AS and TGS: options, cname (AS only), realm,
 * sname, till, nonce, etypes. Returned as a span in @w for the TGS
 * checksum. */
static void krb5_der_req_body(struct krb5_der_w *w, const char *cname, const char *realm,
      int stype, const char *sname, int64_t till, uint32_t nonce)
{
   size_t seq = w->len, s;
   static const uint8_t opts[5] = { 0x00, 0x40, 0x80, 0x00, 0x00 }; /* forwardable, renewable... keep minimal: canonicalize off */
   static const uint8_t opts_plain[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
   s = w->len; krb5_der_octets(w, KRB5_DER_BITSTR, cname ? opts_plain : opts_plain, 5); krb5_der_ctx(w, s, 0);
   (void)opts;
   if (cname)
   {
      s = w->len; krb5_der_principal(w, KRB5_NT_PRINCIPAL, cname); krb5_der_ctx(w, s, 1);
   }
   s = w->len; krb5_der_string(w, realm); krb5_der_ctx(w, s, 2);
   s = w->len; krb5_der_principal(w, stype, sname); krb5_der_ctx(w, s, 3);
   s = w->len; krb5_der_time(w, till); krb5_der_ctx(w, s, 5);
   s = w->len; krb5_der_uint32(w, nonce); krb5_der_ctx(w, s, 7);
   s = w->len;
   {
      size_t inner = w->len;
      krb5_der_int32(w, KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96);
      krb5_der_int32(w, KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96);
      krb5_der_wrap(w, inner, KRB5_DER_SEQ);
   }
   krb5_der_ctx(w, s, 8);
   krb5_der_wrap(w, seq, KRB5_DER_SEQ);
}

/* PA-DATA ::= SEQUENCE { padata-type [1], padata-value [2] } */
static void krb5_der_padata(struct krb5_der_w *w, int type, const uint8_t *val, size_t len)
{
   size_t seq = w->len, s;
   s = w->len; krb5_der_int32(w, type); krb5_der_ctx(w, s, 1);
   s = w->len; krb5_der_octets(w, KRB5_DER_OCTSTR, val, len); krb5_der_ctx(w, s, 2);
   krb5_der_wrap(w, seq, KRB5_DER_SEQ);
}

/* Decrypt and read an EncKDCRepPart: session key, and the sname it is
 * for; tags 25 (AS) and 26 (TGS) are both accepted for either. */
static int krb5_read_kdc_rep_part(struct krb5_ctx *c, const struct krb5_key *k,
      uint32_t usage, const struct krb5_der_r *cipher, struct krb5_key *session)
{
   uint8_t *plain;
   size_t   plen, clen = (size_t)(cipher->end - cipher->p);
   struct krb5_der_r r, app, seq;
   uint8_t  tag;
   int      ok = 0;

   if (!(plain = (uint8_t*)malloc(clen)))
      return 0;
   if (krb5_decrypt(k, usage, cipher->p, clen, plain, &plen) != 0)
   {
      krb5_err(c, "KDC reply does not decrypt: wrong password or key");
      free(plain);
      return 0;
   }
   r.p = plain; r.end = plain + plen;
   if (krb5_der_next(&r, &tag, &app) && (tag == KRB5_DER_APP(25) || tag == KRB5_DER_APP(26))
         && krb5_der_expect(&app, KRB5_DER_SEQ, &seq))
   {
      struct krb5_der_r kseq;
      if (krb5_der_expect(&seq, KRB5_DER_CTX(0), &kseq) && krb5_der_get_key(&kseq, session))
         ok = 1;
   }
   crypto_memzero(plain, plen);
   free(plain);
   if (!ok)
      krb5_err(c, "bad KDC reply part");
   return ok;
}

/* Parse a KDC-REP: the ticket (copied out) and the enc-part span. */
static int krb5_parse_kdc_rep(struct krb5_der_r *r, int msg, uint8_t **ticket,
      size_t *ticket_len, int32_t *etype, struct krb5_der_r *cipher)
{
   struct krb5_der_r app, seq, t;
   int32_t v;
   uint8_t *copy;
   if (!krb5_der_expect(r, KRB5_DER_APP(msg), &app) || !krb5_der_expect(&app, KRB5_DER_SEQ, &seq))
      return 0;
   if (!krb5_der_ctx_int(&seq, 0, &v) || v != 5 || !krb5_der_ctx_int(&seq, 1, &v) || v != msg)
      return 0;
   krb5_der_ctx_skip(&seq, 2);
   krb5_der_ctx_skip(&seq, 3);
   krb5_der_ctx_skip(&seq, 4);
   /* [5] Ticket: keep the whole APPLICATION 1 TLV */
   {
      struct krb5_der_r outer;
      const uint8_t *start;
      if (!krb5_der_expect(&seq, KRB5_DER_CTX(5), &outer))
         return 0;
      start = outer.p;
      if (!krb5_der_expect(&outer, KRB5_DER_APP(1), &t))
         return 0;
      *ticket_len = (size_t)(outer.p - start);
      if (!(copy = (uint8_t*)malloc(*ticket_len)))
         return 0;
      memcpy(copy, start, *ticket_len);
      *ticket = copy;
   }
   if (!krb5_der_expect(&seq, KRB5_DER_CTX(6), &t) || !krb5_der_get_encrypted(&t, etype, cipher))
   {
      free(copy);
      *ticket = NULL;
      return 0;
   }
   return 1;
}

int krb5_set_kdc(struct krb5_ctx *c, const char *realm, const char *kdc, uint16_t port)
{
   strlcpy(c->realm, realm, sizeof(c->realm));
   strlcpy(c->kdc, kdc, sizeof(c->kdc));
   if (port)
      c->port = port;
   return 0;
}

void krb5_set_timeout(struct krb5_ctx *c, unsigned seconds)
{
   c->timeout = seconds;
}

/* ---- AS exchange ------------------------------------------------- */

/* Send one AS-REQ, with PA-ENC-TIMESTAMP when @pa_key is set. */
static int krb5_as_req_once(struct krb5_ctx *c, const char *user,
      const struct krb5_key *pa_key, uint8_t *rep, size_t *rep_len, uint32_t nonce)
{
   uint8_t *buf = (uint8_t*)malloc(KRB5_MAX_MSG);
   struct krb5_der_w w;
   size_t   seq, s;
   char     sname[160];
   int      rc;

   if (!buf)
      return -1;
   w.buf = buf; w.len = 0; w.cap = KRB5_MAX_MSG; w.fail = 0;

   seq = w.len;
   s = w.len; krb5_der_int32(&w, 5); krb5_der_ctx(&w, s, 1);
   s = w.len; krb5_der_int32(&w, KRB5_MSG_AS_REQ); krb5_der_ctx(&w, s, 2);
   if (pa_key)
   {
      /* PA-ENC-TIMESTAMP: EncryptedData of PA-ENC-TS-ENC { patimestamp
       * [0], pausec [1] } under the user's key, usage 1 */
      uint8_t ts[64], conf[16], enc[128], pad[200];
      struct krb5_der_w tw, ew;
      size_t n, e;
      tw.buf = ts; tw.len = 0; tw.cap = sizeof(ts); tw.fail = 0;
      e = tw.len;
      { size_t q = tw.len; krb5_der_time(&tw, krb5_now(c)); krb5_der_ctx(&tw, q, 0); }
      { size_t q = tw.len; krb5_der_int32(&tw, 0); krb5_der_ctx(&tw, q, 1); }
      krb5_der_wrap(&tw, e, KRB5_DER_SEQ);
      crypto_random_bytes(conf, 16);
      n = krb5_encrypt(pa_key, KU_PA_ENC_TS, conf, ts, tw.len, enc);
      ew.buf = pad; ew.len = 0; ew.cap = sizeof(pad); ew.fail = 0;
      krb5_der_encrypted(&ew, pa_key->enctype, 0, enc, n);
      s = w.len;
      {
         size_t inner = w.len;
         krb5_der_padata(&w, KRB5_PA_ENC_TIMESTAMP, pad, ew.len);
         krb5_der_wrap(&w, inner, KRB5_DER_SEQ);
      }
      krb5_der_ctx(&w, s, 3);
   }
   strlcpy(sname, "krbtgt/", sizeof(sname));
   strlcat(sname, c->realm, sizeof(sname));
   s = w.len;
   krb5_der_req_body(&w, user, c->realm, KRB5_NT_SRV_INST, sname, krb5_now(c) + 10 * 3600, nonce);
   krb5_der_ctx(&w, s, 4);
   krb5_der_wrap(&w, seq, KRB5_DER_SEQ);
   krb5_der_wrap(&w, 0, KRB5_DER_APP(KRB5_MSG_AS_REQ));
   if (w.fail)
   {
      free(buf);
      krb5_err(c, "request too large");
      return -1;
   }
   rc = krb5_exchange(c, buf, w.len, rep, rep_len);
   free(buf);
   return rc;
}

int krb5_get_tgt(struct krb5_ctx *c, const char *user, const char *password)
{
   uint8_t *rep = (uint8_t*)malloc(KRB5_MAX_MSG);
   size_t   rep_len;
   struct krb5_der_r r, cipher;
   struct krb5_key ukey;
   int32_t  etype, code;
   char     salt[256];
   uint32_t nonce;
   int      tries;

   if (!rep)
      return -1;
   strlcpy(c->client, user, sizeof(c->client));
   crypto_random_bytes((uint8_t*)&nonce, 4);
   nonce &= 0x7fffffff;

   /* the default salt, used when the KDC returns none */
   strlcpy(salt, c->realm, sizeof(salt));
   strlcat(salt, user, sizeof(salt));
   {
      char *p = strchr(salt + strlen(c->realm), '/');
      while (p)
      {
         memmove(p, p + 1, strlen(p));
         p = strchr(p, '/');
      }
   }
   etype = KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96;

   /* first without preauth; a KDC that wants it answers with the salt
    * and enctype to use, then once more with the encrypted timestamp */
   for (tries = 0; tries < 2; tries++)
   {
      struct krb5_key *pa = NULL;
      rep_len = KRB5_MAX_MSG;
      if (tries)
      {
         if (krb5_string_to_key(etype, password, strlen(password), salt, strlen(salt), 4096, &ukey) != 0)
            break;
         pa = &ukey;
      }
      if (krb5_as_req_once(c, user, pa, rep, &rep_len, nonce) != 0)
         break;
      r.p = rep; r.end = rep + rep_len;
      if (krb5_der_peek(&r, KRB5_DER_APP(30)))
      {
         char nsalt[256];
         int32_t netype;
         if (!krb5_parse_error(c, &r, &code, nsalt, sizeof(nsalt), &netype))
         {
            krb5_err(c, "bad KRB-ERROR");
            break;
         }
         if (code == KRB5_ERR_PREAUTH_REQUIRED && !tries)
         {
            if (netype)
               etype = netype;
            if (nsalt[0])
               strlcpy(salt, nsalt, sizeof(salt));
            continue;
         }
         krb5_kdc_err(c, code, code == 24 ? " (wrong password)" : code == 6 ? " (no such user)" : NULL);
         break;
      }
      /* AS-REP: the user's key decrypts the enc-part (usage 3) */
      {
         uint8_t *ticket = NULL;
         size_t   tlen;
         int32_t  rep_etype;
         if (!krb5_parse_kdc_rep(&r, KRB5_MSG_AS_REP, &ticket, &tlen, &rep_etype, &cipher))
         {
            krb5_err(c, "bad AS-REP");
            break;
         }
         if (!tries || rep_etype != ukey.enctype)
         {
            /* no preauth happened (or a different enctype): derive now */
            if (krb5_string_to_key(rep_etype, password, strlen(password), salt, strlen(salt), 4096, &ukey) != 0)
            {
               free(ticket);
               krb5_err(c, "unsupported enctype");
               break;
            }
         }
         if (!krb5_read_kdc_rep_part(c, &ukey, KU_AS_REP, &cipher, &c->tgt_key))
         {
            free(ticket);
            break;
         }
         free(c->tgt);
         c->tgt     = ticket;
         c->tgt_len = tlen;
         crypto_memzero(&ukey, sizeof(ukey));
         free(rep);
         return 0;
      }
   }
   crypto_memzero(&ukey, sizeof(ukey));
   free(rep);
   return -1;
}

/* ---- AP-REQ builder (shared by TGS and the service) --------------- */

/* Authenticator ::= [APPLICATION 2] SEQUENCE { vno [0] 5, crealm [1],
 * cname [2], cksum [3] opt, cusec [4], ctime [5], subkey [6] opt,
 * seq-number [7] opt }. Encrypted under @key with @usage into an
 * AP-REQ [APPLICATION 14] wrapping @ticket. @cksum (type 16 / 15, or
 * the GSS 0x8003 one) is optional. */
static int krb5_build_ap_req(struct krb5_ctx *c, struct krb5_der_w *w,
      const uint8_t *ticket, size_t ticket_len, const struct krb5_key *key,
      uint32_t usage, int32_t cksumtype, const uint8_t *cksum, size_t cksum_len,
      const struct krb5_key *subkey, uint32_t seqnum, int mutual)
{
   /* authenticator, its ciphertext and the EncryptedData around it:
    * one heap scratch rather than 1.7 KiB of stack */
   uint8_t *scratch = (uint8_t*)malloc(512 + 600 + 640);
   uint8_t *auth, *enc, *ed, conf[16];
   struct krb5_der_w a, e;
   size_t   s, seq, n, top = w->len;
   int      ok;
   static const uint8_t opt_mutual[5] = { 0x00, 0x20, 0x00, 0x00, 0x00 };
   static const uint8_t opt_none[5]   = { 0x00, 0x00, 0x00, 0x00, 0x00 };

   if (!scratch)
      return 0;
   auth = scratch; enc = scratch + 512; ed = scratch + 512 + 600;
   a.buf = auth; a.len = 0; a.cap = 512; a.fail = 0;
   seq = a.len;
   s = a.len; krb5_der_int32(&a, 5); krb5_der_ctx(&a, s, 0);
   s = a.len; krb5_der_string(&a, c->realm); krb5_der_ctx(&a, s, 1);
   s = a.len; krb5_der_principal(&a, KRB5_NT_PRINCIPAL, c->client); krb5_der_ctx(&a, s, 2);
   if (cksum)
   {
      size_t cs;
      s = a.len;
      cs = a.len;
      { size_t q = a.len; krb5_der_int32(&a, cksumtype); krb5_der_ctx(&a, q, 0); }
      { size_t q = a.len; krb5_der_octets(&a, KRB5_DER_OCTSTR, cksum, cksum_len); krb5_der_ctx(&a, q, 1); }
      krb5_der_wrap(&a, cs, KRB5_DER_SEQ);
      krb5_der_ctx(&a, s, 3);
   }
   s = a.len; krb5_der_int32(&a, 0); krb5_der_ctx(&a, s, 4);
   s = a.len; krb5_der_time(&a, krb5_now(c)); krb5_der_ctx(&a, s, 5);
   if (subkey)
   {
      size_t ks;
      s = a.len;
      ks = a.len;
      { size_t q = a.len; krb5_der_int32(&a, subkey->enctype); krb5_der_ctx(&a, q, 0); }
      { size_t q = a.len; krb5_der_octets(&a, KRB5_DER_OCTSTR, subkey->k, subkey->len); krb5_der_ctx(&a, q, 1); }
      krb5_der_wrap(&a, ks, KRB5_DER_SEQ);
      krb5_der_ctx(&a, s, 6);
   }
   s = a.len; krb5_der_uint32(&a, seqnum); krb5_der_ctx(&a, s, 7);
   krb5_der_wrap(&a, seq, KRB5_DER_SEQ);
   krb5_der_wrap(&a, 0, KRB5_DER_APP(2));
   if (a.fail)
   {
      free(scratch);
      return 0;
   }

   crypto_random_bytes(conf, 16);
   n = krb5_encrypt(key, usage, conf, auth, a.len, enc);
   e.buf = ed; e.len = 0; e.cap = 640; e.fail = 0;
   krb5_der_encrypted(&e, key->enctype, 0, enc, n);

   seq = w->len;
   s = w->len; krb5_der_int32(w, 5); krb5_der_ctx(w, s, 0);
   s = w->len; krb5_der_int32(w, KRB5_MSG_AP_REQ); krb5_der_ctx(w, s, 1);
   s = w->len; krb5_der_octets(w, KRB5_DER_BITSTR, mutual ? opt_mutual : opt_none, 5); krb5_der_ctx(w, s, 2);
   s = w->len; krb5_der_put(w, ticket, ticket_len); krb5_der_ctx(w, s, 3);
   s = w->len; krb5_der_put(w, ed, e.len); krb5_der_ctx(w, s, 4);
   krb5_der_wrap(w, seq, KRB5_DER_SEQ);
   krb5_der_wrap(w, top, KRB5_DER_APP(KRB5_MSG_AP_REQ));
   ok = !w->fail && !e.fail;
   crypto_memzero(scratch, 512 + 600 + 640);
   free(scratch);
   return ok;
}

/* ---- TGS exchange ------------------------------------------------ */

int krb5_get_service_ticket(struct krb5_ctx *c, const char *service)
{
   uint8_t *buf = (uint8_t*)malloc(KRB5_MAX_MSG), *rep;
   uint8_t  body[1024], ck[12];
   struct krb5_der_w w, bw;
   size_t   seq, s, rep_len = KRB5_MAX_MSG;
   uint32_t nonce;
   struct krb5_der_r r, cipher;
   int rc = -1;

   if (!buf)
      return -1;
   if (!c->tgt)
   {
      free(buf);
      krb5_err(c, "no TGT");
      return -1;
   }
   rep = buf + KRB5_MAX_MSG / 2;
   rep_len = KRB5_MAX_MSG / 2;
   crypto_random_bytes((uint8_t*)&nonce, 4);
   nonce &= 0x7fffffff;

   /* the req-body first: its checksum (usage 6) goes in the authenticator */
   bw.buf = body; bw.len = 0; bw.cap = sizeof(body); bw.fail = 0;
   krb5_der_req_body(&bw, NULL, c->realm, KRB5_NT_SRV_INST, service, krb5_now(c) + 10 * 3600, nonce);
   if (bw.fail)
   {
      free(buf);
      return -1;
   }
   krb5_checksum(&c->tgt_key, KU_TGS_REQ_AUTH_CKSUM, body, bw.len, ck);

   w.buf = buf; w.len = 0; w.cap = KRB5_MAX_MSG / 2; w.fail = 0;
   seq = w.len;
   s = w.len; krb5_der_int32(&w, 5); krb5_der_ctx(&w, s, 1);
   s = w.len; krb5_der_int32(&w, KRB5_MSG_TGS_REQ); krb5_der_ctx(&w, s, 2);
   /* padata: PA-TGS-REQ = AP-REQ of the TGT */
   s = w.len;
   {
      size_t inner = w.len, pa = w.len, ap;
      { size_t q = w.len; krb5_der_int32(&w, KRB5_PA_TGS_REQ); krb5_der_ctx(&w, q, 1); }
      ap = w.len;
      if (!krb5_build_ap_req(c, &w, c->tgt, c->tgt_len, &c->tgt_key, KU_TGS_REQ_AUTH,
               c->tgt_key.enctype == KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96 ? 16 : 15,
               ck, 12, NULL, 0, 0))
      {
         free(buf);
         krb5_err(c, "request too large");
         return -1;
      }
      krb5_der_wrap(&w, ap, KRB5_DER_OCTSTR);
      krb5_der_ctx(&w, ap, 2);
      krb5_der_wrap(&w, pa, KRB5_DER_SEQ);
      krb5_der_wrap(&w, inner, KRB5_DER_SEQ);
   }
   krb5_der_ctx(&w, s, 3);
   s = w.len; krb5_der_put(&w, body, bw.len); krb5_der_ctx(&w, s, 4);
   krb5_der_wrap(&w, seq, KRB5_DER_SEQ);
   krb5_der_wrap(&w, 0, KRB5_DER_APP(KRB5_MSG_TGS_REQ));
   if (w.fail || krb5_exchange(c, buf, w.len, rep, &rep_len) != 0)
   {
      if (w.fail)
         krb5_err(c, "request too large");
      free(buf);
      return -1;
   }
   r.p = rep; r.end = rep + rep_len;
   if (krb5_der_peek(&r, KRB5_DER_APP(30)))
   {
      int32_t code, et;
      char salt[8];
      if (krb5_parse_error(c, &r, &code, salt, sizeof(salt), &et))
         krb5_kdc_err(c, code, code == 7 ? " (no such service principal)" : NULL);
      else
         krb5_err(c, "bad KRB-ERROR");
      free(buf);
      return -1;
   }
   {
      uint8_t *ticket = NULL;
      size_t   tlen;
      int32_t  et;
      if (!krb5_parse_kdc_rep(&r, KRB5_MSG_TGS_REP, &ticket, &tlen, &et, &cipher))
         krb5_err(c, "bad TGS-REP");
      else if (krb5_read_kdc_rep_part(c, &c->tgt_key, KU_TGS_REP_SESSION, &cipher, &c->svc_key))
      {
         free(c->svc);
         c->svc     = ticket;
         c->svc_len = tlen;
         strlcpy(c->svc_name, service, sizeof(c->svc_name));
         rc = 0;
      }
      else
         free(ticket);
   }
   free(buf);
   return rc;
}

/* ---- the AP exchange, as a GSS-API token (RFC 4121) -------------- */

/* The initial context token: 0x60 len { OID 1.2.840.113554.1.2.2,
 * TOK_ID 01 00, AP-REQ }. The authenticator carries the 0x8003
 * checksum with channel bindings and the flags, and a fresh subkey
 * that becomes the context key. */
static const uint8_t krb5_gss_oid[11] = { 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x01, 0x02, 0x02 };

#define GSS_C_MUTUAL_FLAG    0x02
#define GSS_C_REPLAY_FLAG    0x04
#define GSS_C_SEQUENCE_FLAG  0x08
#define GSS_C_CONF_FLAG      0x10
#define GSS_C_INTEG_FLAG     0x20

int krb5_gss_init_token(struct krb5_ctx *c, uint8_t *out, size_t cap, size_t *out_len)
{
   struct krb5_der_w w;
   uint8_t cb[24];
   uint32_t flags = GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG
      | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG;
   size_t s;

   if (!c->svc)
   {
      krb5_err(c, "no service ticket");
      return -1;
   }
   /* the context key: a subkey of the service ticket's enctype */
   c->gss_key.enctype = c->svc_key.enctype;
   c->gss_key.len     = c->svc_key.len;
   crypto_random_bytes(c->gss_key.k, c->gss_key.len);
   crypto_random_bytes((uint8_t*)&c->seq, 4);
   c->seq &= 0x3fffffff;

   /* RFC 4121 4.1.1: Lgth=16, Bnd = MD5 of zero channel bindings
    * (16 zero octets as sent by everyone), Flags, little-endian */
   memset(cb, 0, sizeof(cb));
   cb[0] = 16;
   /* MD5 of the 20-octet all-zero bindings: fixed constant */
   {
      static const uint8_t md5_zero20[16] = { 0x44, 0x10, 0x18, 0x52, 0x52, 0x08, 0x45, 0x77, 0x05, 0xbf, 0x09, 0xa8, 0xee, 0x3c, 0x10, 0x93 };
      /* (computed once by MD5 of 20 zero bytes; kept literal so no MD5
       * dependency sits in this file) */
      memcpy(cb + 4, md5_zero20, 16);
   }
   cb[20] = (uint8_t)flags; cb[21] = (uint8_t)(flags >> 8); cb[22] = 0; cb[23] = 0;

   w.buf = out; w.len = 0; w.cap = cap; w.fail = 0;
   krb5_der_put(&w, krb5_gss_oid, sizeof(krb5_gss_oid));
   krb5_der_put_byte(&w, 0x01); krb5_der_put_byte(&w, 0x00);        /* TOK_ID KRB_AP_REQ */
   s = w.len;
   if (!krb5_build_ap_req(c, &w, c->svc, c->svc_len, &c->svc_key, KU_AP_REQ_AUTH,
            0x8003, cb, sizeof(cb), &c->gss_key, c->seq, 1))
   {
      krb5_err(c, "token too large");
      return -1;
   }
   (void)s;
   krb5_der_wrap(&w, 0, 0x60);
   if (w.fail)
      return -1;
   *out_len = w.len;
   return 0;
}

/* The reply: 0x60 { OID, TOK_ID 02 00, AP-REP }. The EncAPRepPart
 * (usage 12, under the service session key) may carry the acceptor's
 * subkey, which then replaces ours as the context key (RFC 4121 says
 * the acceptor subkey wins when present). */
int krb5_gss_accept_token(struct krb5_ctx *c, const uint8_t *in, size_t len)
{
   struct krb5_der_r r, tok, app, seq, ed, cipher;
   int32_t v, et;
   uint8_t *plain;
   size_t plen, clen;
   int ok = 0;

   r.p = in; r.end = in + len;
   if (!krb5_der_expect(&r, 0x60, &tok) || tok.end - tok.p < 13
         || memcmp(tok.p, krb5_gss_oid, sizeof(krb5_gss_oid)) != 0
         || tok.p[11] != 0x02 || tok.p[12] != 0x00)
   {
      krb5_err(c, "not a GSS AP-REP token");
      return -1;
   }
   tok.p += 13;
   if (!krb5_der_expect(&tok, KRB5_DER_APP(KRB5_MSG_AP_REP), &app) || !krb5_der_expect(&app, KRB5_DER_SEQ, &seq)
         || !krb5_der_ctx_int(&seq, 0, &v) || !krb5_der_ctx_int(&seq, 1, &v)
         || !krb5_der_expect(&seq, KRB5_DER_CTX(2), &ed) || !krb5_der_get_encrypted(&ed, &et, &cipher))
   {
      krb5_err(c, "bad AP-REP");
      return -1;
   }
   clen = (size_t)(cipher.end - cipher.p);
   if (!(plain = (uint8_t*)malloc(clen)))
      return -1;
   if (krb5_decrypt(&c->svc_key, KU_AP_REP, cipher.p, clen, plain, &plen) == 0)
   {
      struct krb5_der_r pr, papp, pseq, t;
      pr.p = plain; pr.end = plain + plen;
      if (krb5_der_expect(&pr, KRB5_DER_APP(27), &papp) && krb5_der_expect(&papp, KRB5_DER_SEQ, &pseq)
            && krb5_der_ctx_octets(&pseq, 0, KRB5_DER_GENTIME, &t) && krb5_der_ctx_int(&pseq, 1, &v))
      {
         ok = 1;
         if (krb5_der_peek(&pseq, KRB5_DER_CTX(2)))
         {
            struct krb5_der_r ks;
            struct krb5_key sub;
            if (krb5_der_expect(&pseq, KRB5_DER_CTX(2), &ks) && krb5_der_get_key(&ks, &sub))
               c->gss_key = sub;
            else
               ok = 0;
         }
      }
   }
   else
      krb5_err(c, "AP-REP does not verify");
   crypto_memzero(plain, clen);
   free(plain);
   if (!ok)
      return -1;
   c->ap_done = 1;
   return 0;
}

int krb5_gss_session_key(const struct krb5_ctx *c, uint8_t *out, size_t *len)
{
   if (!c->ap_done)
      return -1;
   memcpy(out, c->gss_key.k, c->gss_key.len);
   *len = c->gss_key.len;
   return 0;
}

const char *krb5_realm(const struct krb5_ctx *c)
{
   return c->realm;
}
