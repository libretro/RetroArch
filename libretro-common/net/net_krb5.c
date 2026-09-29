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
