/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (pk.c).
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
#include <crypto/pk.h>
#include <crypto/crypto.h>
#include <lrc_hash.h>
#include <retro_atomic.h>

void bn_from_be(bn_word *r, unsigned k, const uint8_t *in, size_t len)
{
   unsigned i;
   size_t   pos = len;

   for (i = 0; i < k; i++)
   {
      bn_word w = 0;
      unsigned b;
      for (b = 0; b < BN_WORD_BYTES && pos > 0; b++)
      {
         pos--;
         w |= (bn_word)in[pos] << (8 * b);
      }
      r[i] = w;
   }
}

void bn_to_be(uint8_t *out, size_t len, const bn_word *a, unsigned k)
{
   size_t   pos = len;
   unsigned i;

   for (i = 0; i < k && pos > 0; i++)
   {
      unsigned b;
      for (b = 0; b < BN_WORD_BYTES && pos > 0; b++)
      {
         pos--;
         out[pos] = (uint8_t)(a[i] >> (8 * b));
      }
   }
   while (pos > 0)
      out[--pos] = 0;
}

void bn_zero(bn_word *r, unsigned k)
{
   memset(r, 0, k * sizeof(bn_word));
}

void bn_copy(bn_word *r, const bn_word *a, unsigned k)
{
   memcpy(r, a, k * sizeof(bn_word));
}

int bn_is_zero(const bn_word *a, unsigned k)
{
   bn_word acc = 0;
   unsigned i;
   for (i = 0; i < k; i++)
      acc |= a[i];
   return acc == 0;
}

int bn_cmp(const bn_word *a, const bn_word *b, unsigned k)
{
   /* Walk from the top; keep the first difference only. */
   bn_word gt = 0, lt = 0;
   unsigned i;
   for (i = k; i-- > 0; )
   {
      bn_word g = (bn_word)(a[i] > b[i]);
      bn_word l = (bn_word)(a[i] < b[i]);
      bn_word undecided = (bn_word)((gt | lt) == 0);
      gt |= g & undecided;
      lt |= l & undecided;
   }
   return (int)gt - (int)lt;
}

bn_word bn_add(bn_word *r, const bn_word *a, const bn_word *b, unsigned k)
{
   bn_dword carry = 0;
   unsigned i;
   for (i = 0; i < k; i++)
   {
      carry += (bn_dword)a[i] + b[i];
      r[i]   = (bn_word)carry;
      carry >>= BN_WORD_BITS;
   }
   return (bn_word)carry;
}

bn_word bn_sub(bn_word *r, const bn_word *a, const bn_word *b, unsigned k)
{
   bn_dword borrow = 0;
   unsigned i;
   for (i = 0; i < k; i++)
   {
      bn_dword d = (bn_dword)a[i] - b[i] - borrow;
      r[i]   = (bn_word)d;
      borrow = (d >> BN_WORD_BITS) & 1;
   }
   return (bn_word)borrow;
}

void bn_select(bn_word *r, bn_word bit, const bn_word *a,
      const bn_word *b, unsigned k)
{
   bn_word mask = 0 - (bit & 1);
   unsigned i;
   for (i = 0; i < k; i++)
      r[i] = (a[i] & mask) | (b[i] & ~mask);
}

void bn_mod_add(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, unsigned k)
{
   bn_word carry  = bn_add(r, a, b, k);
   /* Subtract m and keep that result when there was a carry out or
    * the sum is still >= m. */
   bn_word borrow;
   bn_word t[BN_MAX_WORDS];
   borrow = bn_sub(t, r, m, k);
   bn_select(r, carry | (bn_word)(borrow == 0), t, r, k);
}

void bn_mod_sub(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, unsigned k)
{
   bn_word borrow = bn_sub(r, a, b, k);
   bn_word t[BN_MAX_WORDS];
   bn_add(t, r, m, k);
   bn_select(r, borrow, t, r, k);
}

unsigned bn_bit_length(const bn_word *a, unsigned k)
{
   unsigned i;
   for (i = k; i-- > 0; )
   {
      if (a[i])
      {
         unsigned bits = 0;
         bn_word v    = a[i];
         while (v)
         {
            bits++;
            v >>= 1;
         }
         return i * BN_WORD_BITS + bits;
      }
   }
   return 0;
}

unsigned bn_get_bit(const bn_word *a, unsigned i)
{
   return (a[i / BN_WORD_BITS] >> (i % BN_WORD_BITS)) & 1;
}

bn_word bn_mont_n0(const bn_word *m)
{
   /* -m^-1 mod 2^BN_WORD_BITS by Newton iteration; m odd. The seed
    * m itself is correct to 3 bits and each step doubles that. */
   bn_word x = m[0];
   unsigned i;
   for (i = 0; i < 6; i++)
      x *= 2 - m[0] * x;
   return 0 - x;
}

void bn_mont_r2(bn_word *r2, const bn_word *m, unsigned k)
{
   /* R^2 = 2^(2 bits) mod m. Doubling all the way is 2 * bits modular
    * additions, most of the cost of an RSA verify; instead reach
    * x = 2^(bits + 32) mod m by doubling (33 steps when m has its top
    * bit set, as every RSA modulus and both P-256 primes do), then
    * square in the Montgomery domain: mont_mul(2^(bits + s), same)
    * = 2^(bits + 2s), so s doubles per squaring and log2(bits)
    * squarings reach 2^(2 bits). Any leftover for a bits count that
    * is not a power of two is made up by doubling at the end. */
   bn_word  tmp[BN_MONT_TMP_WORDS(BN_MAX_WORDS)];
   bn_word  n0   = bn_mont_n0(m);
   unsigned bits = BN_WORD_BITS * k;
   unsigned s;
   unsigned i    = 0;

   bn_zero(r2, k);
   if (m[k - 1] >> (BN_WORD_BITS - 1))
   {
      r2[k - 1] = (bn_word)1 << (BN_WORD_BITS - 1);
      i = bits - 1;
   }
   else
      r2[0] = 1;
   /* A doubling is a k-word add, a squaring is k^2 products: for the
    * sizes here 32 doublings cost about one squaring and save five. */
   for (; i < bits + 32; i++)
      bn_mod_add(r2, r2, r2, m, k);

   for (s = 32; 2 * s <= bits; s *= 2)
      bn_mont_sqr(r2, r2, m, n0, k, tmp);
   for (; s < bits; s++)
      bn_mod_add(r2, r2, r2, m, k);
}

void bn_mont_mul(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, bn_word n0, unsigned k, bn_word *tmp)
{
   /* CIOS: tmp has k + 2 words. */
   unsigned i, j;
   memset(tmp, 0, (k + 2) * sizeof(bn_word));

   for (i = 0; i < k; i++)
   {
      bn_dword carry = 0;
      bn_word u;
      for (j = 0; j < k; j++)
      {
         carry += (bn_dword)tmp[j] + (bn_dword)a[j] * b[i];
         tmp[j] = (bn_word)carry;
         carry >>= BN_WORD_BITS;
      }
      carry     += tmp[k];
      tmp[k]     = (bn_word)carry;
      tmp[k + 1] = (bn_word)(carry >> BN_WORD_BITS);

      u     = tmp[0] * n0;
      carry = (bn_dword)tmp[0] + (bn_dword)u * m[0];
      carry >>= BN_WORD_BITS;
      for (j = 1; j < k; j++)
      {
         carry += (bn_dword)tmp[j] + (bn_dword)u * m[j];
         tmp[j - 1] = (bn_word)carry;
         carry >>= BN_WORD_BITS;
      }
      carry     += tmp[k];
      tmp[k - 1] = (bn_word)carry;
      tmp[k]     = tmp[k + 1] + (bn_word)(carry >> BN_WORD_BITS);
   }

   /* Result in tmp[0..k], < 2m: one conditional subtraction. */
   {
      bn_word borrow = bn_sub(r, tmp, m, k);
      /* Keep the subtraction when tmp[k] is set or no borrow. */
      bn_select(r, tmp[k] | (bn_word)(borrow == 0), r, tmp, k);
   }
}

/* Montgomery reduction of the 2k-word product in t, result to r. */
static void bn_mont_reduce(bn_word *r, bn_word *t,
      const bn_word *m, bn_word n0, unsigned k)
{
   unsigned i, j;
   bn_word  extra = 0;

   for (i = 0; i < k; i++)
   {
      bn_word  u     = t[i] * n0;
      bn_dword carry = 0;
      for (j = 0; j < k; j++)
      {
         carry   += (bn_dword)t[i + j] + (bn_dword)u * m[j];
         t[i + j] = (bn_word)carry;
         carry  >>= BN_WORD_BITS;
      }
      for (j = i + k; carry && j < 2 * k; j++)
      {
         carry += t[j];
         t[j]   = (bn_word)carry;
         carry >>= BN_WORD_BITS;
      }
      extra += (bn_word)carry;
   }

   {
      bn_word borrow = bn_sub(r, t + k, m, k);
      bn_select(r, extra | (bn_word)(borrow == 0), r, t + k, k);
   }
}

void bn_mont_sqr(bn_word *r, const bn_word *a,
      const bn_word *m, bn_word n0, unsigned k, bn_word *tmp)
{
   unsigned i, j;
   bn_dword carry;

   memset(tmp, 0, 2 * k * sizeof(bn_word));

   /* Off-diagonal products, each once. */
   for (i = 0; i < k; i++)
   {
      carry = 0;
      for (j = i + 1; j < k; j++)
      {
         carry     += (bn_dword)tmp[i + j] + (bn_dword)a[i] * a[j];
         tmp[i + j] = (bn_word)carry;
         carry    >>= BN_WORD_BITS;
      }
      tmp[i + k] = (bn_word)carry;
   }

   /* Double them, then add the squares on the diagonal. */
   carry = 0;
   for (i = 0; i < 2 * k; i++)
   {
      carry += (bn_dword)tmp[i] << 1;
      tmp[i] = (bn_word)carry;
      carry >>= BN_WORD_BITS;
   }
   carry = 0;
   for (i = 0; i < k; i++)
   {
      carry         += (bn_dword)tmp[2 * i] + (bn_dword)a[i] * a[i];
      tmp[2 * i]     = (bn_word)carry;
      carry        >>= BN_WORD_BITS;
      carry         += tmp[2 * i + 1];
      tmp[2 * i + 1] = (bn_word)carry;
      carry        >>= BN_WORD_BITS;
   }

   bn_mont_reduce(r, tmp, m, n0, k);
}

void bn_mod_mul(bn_word *r, const bn_word *a, const bn_word *b,
      const bn_word *m, const bn_word *r2, bn_word n0, unsigned k,
      bn_word *tmp)
{
   /* (a * R) * b * R^-1 = a * b; the intermediate needs its own home,
    * and r may alias a or b. */
   bn_word am[BN_MAX_WORDS];
   bn_mont_mul(am, a, r2, m, n0, k, tmp);
   bn_mont_mul(r, am, b, m, n0, k, tmp);
}

void bn_mod_exp(bn_word *r, const bn_word *base,
      const bn_word *exp, unsigned ek,
      const bn_word *m, const bn_word *r2, bn_word n0, unsigned k,
      bn_word *work)
{
   bn_word *acc  = work;            /* k */
   bn_word *bm   = work + k;        /* k */
   bn_word *t    = work + 2 * k;    /* k */
   bn_word *tmp  = work + 3 * k;    /* k + 2 */
   unsigned  bits = bn_bit_length(exp, ek);
   unsigned  i;

   /* acc = R mod m (Montgomery one) = mont_mul(R^2, 1) */
   bn_zero(t, k);
   t[0] = 1;
   bn_mont_mul(acc, r2, t, m, n0, k, tmp);
   bn_mont_mul(bm, base, r2, m, n0, k, tmp);

   for (i = bits; i-- > 0; )
   {
      bn_mont_sqr(t, acc, m, n0, k, tmp);
      if (bn_get_bit(exp, i))
         bn_mont_mul(acc, t, bm, m, n0, k, tmp);
      else
         bn_copy(acc, t, k);
   }

   bn_zero(t, k);
   t[0] = 1;
   bn_mont_mul(r, acc, t, m, n0, k, tmp);
}

static const uint8_t rsa_di_sha256[19] = {
   0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20
};
static const uint8_t rsa_di_sha384[19] = {
   0x30,0x41,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x02,0x05,0x00,0x04,0x30
};
static const uint8_t rsa_di_sha512[19] = {
   0x30,0x51,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x03,0x05,0x00,0x04,0x40
};

struct rsa_work
{
   bn_word mod[BN_MAX_WORDS];
   bn_word r2[BN_MAX_WORDS];
   bn_word s[BN_MAX_WORDS];
   bn_word exp[BN_MAX_WORDS];
   bn_word m[BN_MAX_WORDS];
   bn_word work[3 * BN_MAX_WORDS + BN_MONT_TMP_WORDS(BN_MAX_WORDS)];
   uint8_t em[BN_MAX_WORDS * BN_WORD_BYTES];
   uint8_t expect[BN_MAX_WORDS * BN_WORD_BYTES];
};

int rsa_pkcs1_verify(const uint8_t *n, size_t n_len,
      const uint8_t *e, size_t e_len,
      enum rsa_hash hash, const uint8_t *digest, size_t digest_len,
      const uint8_t *sig, size_t sig_len)
{
   const uint8_t *di;
   size_t   di_len = 19;
   size_t   hlen;
   size_t   tlen;
   size_t   ps_len;
   unsigned k, ek;
   struct rsa_work *w;
   int ok;

   switch (hash)
   {
      case RSA_HASH_SHA256: di = rsa_di_sha256; hlen = 32; break;
      case RSA_HASH_SHA384: di = rsa_di_sha384; hlen = 48; break;
      case RSA_HASH_SHA512: di = rsa_di_sha512; hlen = 64; break;
      default:
         return -1;
   }
   if (digest_len != hlen)
      return -1;

   while (n_len && n[0] == 0)
   {
      n++;
      n_len--;
   }
   while (e_len && e[0] == 0)
   {
      e++;
      e_len--;
   }
   /* 512-bit floor, 4096-bit ceiling, odd modulus and exponent;
    * the signature is exactly the modulus length (RFC 8017 8.2.2
    * step 1). */
   if (n_len < 64 || n_len > BN_MAX_WORDS * BN_WORD_BYTES || !(n[n_len - 1] & 1))
      return -1;
   if (!e_len || e_len > n_len || !(e[e_len - 1] & 1))
      return -1;
   if (sig_len != n_len)
      return -1;
   tlen = di_len + hlen;
   if (n_len < tlen + 11)
      return -1;

   if (!(w = (struct rsa_work*)malloc(sizeof(*w))))
      return -1;

   k  = (unsigned)((n_len + BN_WORD_BYTES - 1) / BN_WORD_BYTES);
   ek = (unsigned)((e_len + BN_WORD_BYTES - 1) / BN_WORD_BYTES);
   bn_from_be(w->mod, k, n, n_len);
   bn_from_be(w->s,   k, sig, sig_len);
   bn_from_be(w->exp, ek, e, e_len);

   ok = 0;
   /* s < n (step 2a of RSAVP1) */
   if (bn_cmp(w->s, w->mod, k) < 0)
   {
      bn_mont_r2(w->r2, w->mod, k);
      bn_mod_exp(w->m, w->s, w->exp, ek, w->mod, w->r2,
            bn_mont_n0(w->mod), k, w->work);
      bn_to_be(w->em, n_len, w->m, k);

      /* EM = 0x00 || 0x01 || PS (0xff * ps_len, >= 8) || 0x00 || T */
      ps_len = n_len - tlen - 3;
      w->expect[0] = 0x00;
      w->expect[1] = 0x01;
      memset(w->expect + 2, 0xff, ps_len);
      w->expect[2 + ps_len] = 0x00;
      memcpy(w->expect + 3 + ps_len, di, di_len);
      memcpy(w->expect + 3 + ps_len + di_len, digest, hlen);

      ok = crypto_memeq_ct(w->em, w->expect, n_len);
   }

   free(w);
   return ok ? 0 : -1;
}

/* RSASSA-PSS verification (RFC 8017 9.1.2) with MGF1 over the same
 * hash and a salt of the hash length, the form TLS 1.3 signs with.
 * The modular exponentiation is the one rsa_pkcs1_verify() uses. */
static void rsa_hash_of(enum rsa_hash hash, const uint8_t *data, size_t len, uint8_t *out)
{
   if (hash == RSA_HASH_SHA384)
   {
      struct sha512_state st;
      sha512_stream_init(&st, 1);
      sha512_stream_update(&st, data, len);
      sha512_stream_final(&st, out);
   }
   else
   {
      struct sha256_state st;
      sha256_stream_init(&st, 0);
      sha256_stream_update(&st, data, len);
      sha256_stream_final(&st, out);
   }
}

int rsa_pss_verify(const uint8_t *n, size_t n_len,
      const uint8_t *e, size_t e_len,
      enum rsa_hash hash, const uint8_t *digest, size_t digest_len,
      const uint8_t *sig, size_t sig_len)
{
   size_t   hlen, em_len, db_len, i, counter;
   unsigned k, ek;
   struct rsa_work *w;
   uint8_t *db, *h, *db_mask, hprime[64], seed[64 + 4];
   int ok = 0;

   switch (hash)
   {
      case RSA_HASH_SHA256: hlen = 32; break;
      case RSA_HASH_SHA384: hlen = 48; break;
      default:
         return -1;
   }
   if (digest_len != hlen)
      return -1;
   while (n_len && n[0] == 0) { n++; n_len--; }
   while (e_len && e[0] == 0) { e++; e_len--; }
   if (n_len < 64 || n_len > BN_MAX_WORDS * BN_WORD_BYTES || !(n[n_len - 1] & 1))
      return -1;
   if (!e_len || e_len > n_len || !(e[e_len - 1] & 1))
      return -1;
   if (sig_len != n_len)
      return -1;
   em_len = n_len;                       /* emBits = modBits - 1; a full octet count when
                                            the top bit of n is set, which the mask below
                                            handles for any modBits */
   if (em_len < 2 * hlen + 2)
      return -1;
   if (!(w = (struct rsa_work*)malloc(sizeof(*w))))
      return -1;
   k  = (unsigned)((n_len + BN_WORD_BYTES - 1) / BN_WORD_BYTES);
   ek = (unsigned)((e_len + BN_WORD_BYTES - 1) / BN_WORD_BYTES);
   bn_from_be(w->mod, k, n, n_len);
   bn_from_be(w->s,   k, sig, sig_len);
   bn_from_be(w->exp, ek, e, e_len);
   if (bn_cmp(w->s, w->mod, k) >= 0)
      goto done;
   bn_mont_r2(w->r2, w->mod, k);
   bn_mod_exp(w->m, w->s, w->exp, ek, w->mod, w->r2, bn_mont_n0(w->mod), k, w->work);
   bn_to_be(w->em, em_len, w->m, k);

   /* EM = maskedDB || H || 0xbc */
   if (w->em[em_len - 1] != 0xbc)
      goto done;
   db_len  = em_len - hlen - 1;
   db      = w->em;
   h       = w->em + db_len;
   db_mask = w->expect;                  /* scratch of em_len octets */
   /* MGF1(H, db_len) */
   memcpy(seed, h, hlen);
   for (counter = 0, i = 0; i < db_len; counter++)
   {
      uint8_t t[64];
      size_t  take;
      seed[hlen]     = (uint8_t)(counter >> 24);
      seed[hlen + 1] = (uint8_t)(counter >> 16);
      seed[hlen + 2] = (uint8_t)(counter >> 8);
      seed[hlen + 3] = (uint8_t)counter;
      rsa_hash_of(hash, seed, hlen + 4, t);
      take = db_len - i < hlen ? db_len - i : hlen;
      memcpy(db_mask + i, t, take);
      i += take;
   }
   for (i = 0; i < db_len; i++)
      db[i] ^= db_mask[i];
   /* the bits above emBits are cleared: with emBits = modBits - 1 and
    * a modulus whose top octet is used, that is the top bit */
   {
      unsigned top_bits = 0;
      uint8_t  t = n[0];
      while (t) { top_bits++; t >>= 1; }
      /* emBits = 8 * n_len - (8 - top_bits) - 1 */
      {
         unsigned em_bits = 8 * (unsigned)n_len - (8 - top_bits) - 1;
         unsigned clear   = 8 * (unsigned)em_len - em_bits;
         db[0] &= (uint8_t)(0xff >> clear);
      }
   }
   /* DB = PS (zeros) || 0x01 || salt, salt of hlen octets */
   for (i = 0; i < db_len - hlen - 1; i++)
      if (db[i] != 0)
         goto done;
   if (db[db_len - hlen - 1] != 0x01)
      goto done;
   /* H' = Hash(0x00 * 8 || mHash || salt) */
   {
      uint8_t *m2 = w->expect;           /* db_mask no longer needed */
      memset(m2, 0, 8);
      memcpy(m2 + 8, digest, hlen);
      memcpy(m2 + 8 + hlen, db + db_len - hlen, hlen);
      rsa_hash_of(hash, m2, 8 + 2 * hlen, hprime);
   }
   ok = crypto_memeq_ct(h, hprime, hlen);
done:
   free(w);
   return ok ? 0 : -1;
}

/* X25519 (RFC 7748): the Montgomery ladder over 2^255 - 19 on the
 * same Montgomery-form field arithmetic the curves use. Scalars and
 * coordinates are little-endian on the wire; the scalar is clamped as
 * the RFC says and the top bit of a coordinate ignored. */

#if defined(__SIZEOF_INT128__) && !defined(X25519_NO_128)
/* 64-bit machines: arithmetic mod 2^255 - 19 in five 51-bit limbs,
 * products in 128 bits. 2^255 = 19 (mod p), so what a product carries
 * past limb 4 folds back into limb 0 times 19. Limbs stay under 2^52
 * between operations: sums of two reduced values, and f + 2p - g for a
 * difference, fit what a multiply takes. */
__extension__ typedef unsigned __int128 fe51_u128;
typedef uint64_t fe51[5];
#define FE51_MASK (((uint64_t)1 << 51) - 1)

static void fe51_frombytes(fe51 h, const uint8_t *s)
{
   uint64_t w[4];
   unsigned i;
   for (i = 0; i < 4; i++)
      w[i] = (uint64_t)s[8 * i]            | ((uint64_t)s[8 * i + 1] << 8)
           | ((uint64_t)s[8 * i + 2] << 16) | ((uint64_t)s[8 * i + 3] << 24)
           | ((uint64_t)s[8 * i + 4] << 32) | ((uint64_t)s[8 * i + 5] << 40)
           | ((uint64_t)s[8 * i + 6] << 48) | ((uint64_t)s[8 * i + 7] << 56);
   h[0] =  w[0]                       & FE51_MASK;
   h[1] = ((w[0] >> 51) | (w[1] << 13)) & FE51_MASK;
   h[2] = ((w[1] >> 38) | (w[2] << 26)) & FE51_MASK;
   h[3] = ((w[2] >> 25) | (w[3] << 39)) & FE51_MASK;
   h[4] =  (w[3] >> 12)               & FE51_MASK;   /* the top bit ignored */
}

static void fe51_carry(fe51 h)
{
   uint64_t c;
   c = h[0] >> 51; h[0] &= FE51_MASK; h[1] += c;
   c = h[1] >> 51; h[1] &= FE51_MASK; h[2] += c;
   c = h[2] >> 51; h[2] &= FE51_MASK; h[3] += c;
   c = h[3] >> 51; h[3] &= FE51_MASK; h[4] += c;
   c = h[4] >> 51; h[4] &= FE51_MASK; h[0] += c * 19;
   c = h[0] >> 51; h[0] &= FE51_MASK; h[1] += c;
}

/* The canonical value, 0 <= h < p, little-endian. */
static void fe51_tobytes(uint8_t *s, const fe51 f)
{
   fe51     h;
   uint64_t q, w[4];
   unsigned i;
   for (i = 0; i < 5; i++)
      h[i] = f[i];
   fe51_carry(h);
   fe51_carry(h);
   /* h < 2^255 now; subtract p once if h >= p: q = 1 iff h + 19 >= 2^255 */
   q = (h[0] + 19) >> 51;
   q = (h[1] + q) >> 51;
   q = (h[2] + q) >> 51;
   q = (h[3] + q) >> 51;
   q = (h[4] + q) >> 51;
   h[0] += 19 * q;
   h[1] += h[0] >> 51; h[0] &= FE51_MASK;
   h[2] += h[1] >> 51; h[1] &= FE51_MASK;
   h[3] += h[2] >> 51; h[2] &= FE51_MASK;
   h[4] += h[3] >> 51; h[3] &= FE51_MASK;
   h[4] &= FE51_MASK;                    /* drops the 2^255 the +19 made */
   w[0] = h[0]         | (h[1] << 51);
   w[1] = (h[1] >> 13) | (h[2] << 38);
   w[2] = (h[2] >> 26) | (h[3] << 25);
   w[3] = (h[3] >> 39) | (h[4] << 12);
   for (i = 0; i < 32; i++)
      s[i] = (uint8_t)(w[i >> 3] >> (8 * (i & 7)));
}

static void fe51_add(fe51 h, const fe51 f, const fe51 g)
{
   unsigned i;
   for (i = 0; i < 5; i++)
      h[i] = f[i] + g[i];
}

/* f + 2p - g: never negative for reduced limbs */
static void fe51_sub(fe51 h, const fe51 f, const fe51 g)
{
   h[0] = f[0] + (uint64_t)0xfffffffffffdaULL - g[0];
   h[1] = f[1] + (uint64_t)0xffffffffffffeULL - g[1];
   h[2] = f[2] + (uint64_t)0xffffffffffffeULL - g[2];
   h[3] = f[3] + (uint64_t)0xffffffffffffeULL - g[3];
   h[4] = f[4] + (uint64_t)0xffffffffffffeULL - g[4];
}

static void fe51_mul(fe51 h, const fe51 f, const fe51 g)
{
   fe51_u128 r0, r1, r2, r3, r4;
   uint64_t  g1_19 = g[1] * 19, g2_19 = g[2] * 19, g3_19 = g[3] * 19, g4_19 = g[4] * 19;
   uint64_t  c;
   r0 = (fe51_u128)f[0] * g[0] + (fe51_u128)f[1] * g4_19 + (fe51_u128)f[2] * g3_19
      + (fe51_u128)f[3] * g2_19 + (fe51_u128)f[4] * g1_19;
   r1 = (fe51_u128)f[0] * g[1] + (fe51_u128)f[1] * g[0] + (fe51_u128)f[2] * g4_19
      + (fe51_u128)f[3] * g3_19 + (fe51_u128)f[4] * g2_19;
   r2 = (fe51_u128)f[0] * g[2] + (fe51_u128)f[1] * g[1] + (fe51_u128)f[2] * g[0]
      + (fe51_u128)f[3] * g4_19 + (fe51_u128)f[4] * g3_19;
   r3 = (fe51_u128)f[0] * g[3] + (fe51_u128)f[1] * g[2] + (fe51_u128)f[2] * g[1]
      + (fe51_u128)f[3] * g[0] + (fe51_u128)f[4] * g4_19;
   r4 = (fe51_u128)f[0] * g[4] + (fe51_u128)f[1] * g[3] + (fe51_u128)f[2] * g[2]
      + (fe51_u128)f[3] * g[1] + (fe51_u128)f[4] * g[0];
   c = (uint64_t)(r0 >> 51); h[0] = (uint64_t)r0 & FE51_MASK; r1 += c;
   c = (uint64_t)(r1 >> 51); h[1] = (uint64_t)r1 & FE51_MASK; r2 += c;
   c = (uint64_t)(r2 >> 51); h[2] = (uint64_t)r2 & FE51_MASK; r3 += c;
   c = (uint64_t)(r3 >> 51); h[3] = (uint64_t)r3 & FE51_MASK; r4 += c;
   c = (uint64_t)(r4 >> 51); h[4] = (uint64_t)r4 & FE51_MASK;
   h[0] += c * 19;
   c = h[0] >> 51; h[0] &= FE51_MASK; h[1] += c;
}

static void fe51_sq(fe51 h, const fe51 f)
{
   fe51_mul(h, f, f);
}

static void fe51_sqn(fe51 h, const fe51 f, unsigned n)
{
   fe51_sq(h, f);
   while (--n)
      fe51_sq(h, h);
}

/* 121665 f (a24 = (486662 - 2) / 4) */
static void fe51_mul_a24(fe51 h, const fe51 f)
{
   fe51_u128 r;
   uint64_t  c = 0;
   unsigned  i;
   for (i = 0; i < 5; i++)
   {
      r    = (fe51_u128)f[i] * 121665 + c;
      h[i] = (uint64_t)r & FE51_MASK;
      c    = (uint64_t)(r >> 51);
   }
   h[0] += c * 19;
   c = h[0] >> 51; h[0] &= FE51_MASK; h[1] += c;
}

/* z^(p - 2) = z^(2^255 - 21): the inverse, by an addition chain of
 * 254 squarings and 11 multiplies */
static void fe51_invert(fe51 out, const fe51 z)
{
   fe51 z2, z9, z11, z_5, z_10, z_20, z_50, z_100, t;
   fe51_sq(z2, z);                        /* 2 */
   fe51_sqn(t, z2, 2);                    /* 8 */
   fe51_mul(z9, t, z);                    /* 9 */
   fe51_mul(z11, z9, z2);                 /* 11 */
   fe51_sq(t, z11);                       /* 22 */
   fe51_mul(z_5, t, z9);                  /* 2^5 - 1 */
   fe51_sqn(t, z_5, 5);
   fe51_mul(z_10, t, z_5);                /* 2^10 - 1 */
   fe51_sqn(t, z_10, 10);
   fe51_mul(z_20, t, z_10);               /* 2^20 - 1 */
   fe51_sqn(t, z_20, 20);
   fe51_mul(t, t, z_20);                  /* 2^40 - 1 */
   fe51_sqn(t, t, 10);
   fe51_mul(z_50, t, z_10);               /* 2^50 - 1 */
   fe51_sqn(t, z_50, 50);
   fe51_mul(z_100, t, z_50);              /* 2^100 - 1 */
   fe51_sqn(t, z_100, 100);
   fe51_mul(t, t, z_100);                 /* 2^200 - 1 */
   fe51_sqn(t, t, 50);
   fe51_mul(t, t, z_50);                  /* 2^250 - 1 */
   fe51_sqn(t, t, 5);                     /* 2^255 - 32 */
   fe51_mul(out, t, z11);                 /* 2^255 - 21 */
}

static void fe51_cswap(uint64_t swap, fe51 a, fe51 b)
{
   uint64_t mask = (uint64_t)0 - swap;
   unsigned i;
   for (i = 0; i < 5; i++)
   {
      uint64_t t = mask & (a[i] ^ b[i]);
      a[i] ^= t;
      b[i] ^= t;
   }
}

static int x25519_51(uint8_t *out, const uint8_t *scalar, const uint8_t *u_le)
{
   fe51     x1, x2, z2, x3, z3, A, AA, B, BB, E, C, D, DA, CB, t;
   uint64_t swap = 0;
   uint8_t  k[32];
   uint8_t  acc = 0;
   int      i;

   memcpy(k, scalar, 32);
   k[0]  &= 248;
   k[31] &= 127;
   k[31] |= 64;
   fe51_frombytes(x1, u_le);
   x2[0] = 1; x2[1] = x2[2] = x2[3] = x2[4] = 0;
   z2[0] = z2[1] = z2[2] = z2[3] = z2[4] = 0;
   for (i = 0; i < 5; i++)
      x3[i] = x1[i];
   z3[0] = 1; z3[1] = z3[2] = z3[3] = z3[4] = 0;

   for (i = 254; i >= 0; i--)
   {
      uint64_t bit = (uint64_t)((k[i >> 3] >> (i & 7)) & 1);
      swap ^= bit;
      fe51_cswap(swap, x2, x3);
      fe51_cswap(swap, z2, z3);
      swap = bit;

      fe51_add(A, x2, z2);
      fe51_sq(AA, A);
      fe51_sub(B, x2, z2);
      fe51_sq(BB, B);
      fe51_sub(E, AA, BB);
      fe51_add(C, x3, z3);
      fe51_sub(D, x3, z3);
      fe51_mul(DA, D, A);
      fe51_mul(CB, C, B);
      fe51_add(t, DA, CB);
      fe51_sq(x3, t);
      fe51_sub(t, DA, CB);
      fe51_sq(t, t);
      fe51_mul(z3, x1, t);
      fe51_mul(x2, AA, BB);
      fe51_mul_a24(t, E);
      fe51_add(t, AA, t);
      fe51_mul(z2, E, t);
   }
   fe51_cswap(swap, x2, x3);
   fe51_cswap(swap, z2, z3);

   fe51_invert(t, z2);
   fe51_mul(t, x2, t);
   fe51_tobytes(out, t);
   crypto_memzero(k, sizeof(k));
   /* all-zero output means a low-order point: refuse it */
   for (i = 0; i < 32; i++)
      acc |= out[i];
   return acc ? 0 : -1;
}
#else
/* Machines without a 128-bit integer (32-bit ones, MSVC): arithmetic
 * mod 2^255 - 19 in ten limbs of 26 and 25 bits, at bit offsets 0, 26,
 * 51, 77, 102, 128, 153, 179, 204 and 230. Limbs are 32-bit words and
 * every product one 32x32->64 multiply. Limb i times limb j lands in
 * limb i + j; when both are odd the offsets add to one more than that
 * limb's, a factor 2, and past limb 9 the product wraps to limb
 * i + j - 10 times 19 (2^255 = 19 mod p). Inputs to a multiply stay
 * under 2^27 a limb, so 19 g and 2 f fit 32 bits and a column of ten
 * products is under 2^63. All unsigned: no shift of a negative. */
typedef uint32_t fe10[10];
static const unsigned char fe10_off[10]  = { 0, 26, 51, 77, 102, 128, 153, 179, 204, 230 };
static const unsigned char fe10_bits[10] = { 26, 25, 26, 25, 26, 25, 26, 25, 26, 25 };
/* 2p in this radix: 2(2^26 - 19), then 2(2^25 - 1), 2(2^26 - 1), ... */
static const uint32_t fe10_2p[10] = {
   0x7ffffdaU, 0x3fffffeU, 0x7fffffeU, 0x3fffffeU, 0x7fffffeU,
   0x3fffffeU, 0x7fffffeU, 0x3fffffeU, 0x7fffffeU, 0x3fffffeU
};

/* r (column sums under 2^63) carried into limbs h */
static void fe10_carry64(fe10 h, uint64_t *r)
{
   uint64_t c;
   unsigned i;
   for (i = 0; i < 9; i++)
   {
      c         = r[i] >> fe10_bits[i];
      h[i]      = (uint32_t)r[i] & ((1U << fe10_bits[i]) - 1);
      r[i + 1] += c;
   }
   c     = r[9] >> 25;
   h[9]  = (uint32_t)r[9] & ((1U << 25) - 1);
   c     = h[0] + 19 * c;
   h[0]  = (uint32_t)c & ((1U << 26) - 1);
   h[1] += (uint32_t)(c >> 26);
}

static void fe10_carry(fe10 h)
{
   uint64_t r[10];
   unsigned i;
   for (i = 0; i < 10; i++)
      r[i] = h[i];
   fe10_carry64(h, r);
}

static void fe10_frombytes(fe10 h, const uint8_t *s)
{
   uint64_t w[4];
   unsigned i;
   for (i = 0; i < 4; i++)
      w[i] = (uint64_t)s[8 * i]            | ((uint64_t)s[8 * i + 1] << 8)
           | ((uint64_t)s[8 * i + 2] << 16) | ((uint64_t)s[8 * i + 3] << 24)
           | ((uint64_t)s[8 * i + 4] << 32) | ((uint64_t)s[8 * i + 5] << 40)
           | ((uint64_t)s[8 * i + 6] << 48) | ((uint64_t)s[8 * i + 7] << 56);
   for (i = 0; i < 10; i++)
   {
      /* the limb's bits from its offset, across two words at most */
      unsigned word = fe10_off[i] >> 6, sh = fe10_off[i] & 63;
      uint64_t v    = w[word] >> sh;
      if (sh + fe10_bits[i] > 64)
         v |= w[word + 1] << (64 - sh);
      h[i] = (uint32_t)v & ((1U << fe10_bits[i]) - 1);
   }
   /* limb 9 is 25 bits from bit 230: bit 255 is left out */
}

/* The canonical value, 0 <= h < p, little-endian. */
static void fe10_tobytes(uint8_t *s, const fe10 f)
{
   fe10     h;
   uint64_t w[4];
   uint32_t q;
   unsigned i;
   for (i = 0; i < 10; i++)
      h[i] = f[i];
   fe10_carry(h);
   fe10_carry(h);
   /* h < 2^255; q = 1 iff h + 19 >= 2^255, i.e. h >= p */
   q = (h[0] + 19) >> 26;
   for (i = 1; i < 10; i++)
      q = (h[i] + q) >> fe10_bits[i];
   h[0] += 19 * q;
   for (i = 0; i < 9; i++)
   {
      h[i + 1] += h[i] >> fe10_bits[i];
      h[i]     &= (1U << fe10_bits[i]) - 1;
   }
   h[9] &= (1U << 25) - 1;            /* drops the 2^255 the +19 made */
   w[0] = w[1] = w[2] = w[3] = 0;
   for (i = 0; i < 10; i++)
   {
      unsigned word = fe10_off[i] >> 6, sh = fe10_off[i] & 63;
      w[word] |= (uint64_t)h[i] << sh;
      if (sh + fe10_bits[i] > 64)
         w[word + 1] |= (uint64_t)h[i] >> (64 - sh);
   }
   for (i = 0; i < 32; i++)
      s[i] = (uint8_t)(w[i >> 3] >> (8 * (i & 7)));
}

static void fe10_add(fe10 h, const fe10 f, const fe10 g)
{
   unsigned i;
   for (i = 0; i < 10; i++)
      h[i] = f[i] + g[i];
}

static void fe10_sub(fe10 h, const fe10 f, const fe10 g)
{
   unsigned i;
   for (i = 0; i < 10; i++)
      h[i] = f[i] + fe10_2p[i] - g[i];
   fe10_carry(h);
}

static void fe10_mul(fe10 h, const fe10 f, const fe10 g)
{
   uint32_t f2[10], g19[10];
   uint64_t r[10];
   unsigned i;
   for (i = 0; i < 10; i++)
   {
      f2[i]  = 2 * f[i];
      g19[i] = 19 * g[i];
   }
   r[0] = (uint64_t)f[0] * g[0]
        + (uint64_t)f2[1] * g19[9]
        + (uint64_t)f[2] * g19[8]
        + (uint64_t)f2[3] * g19[7]
        + (uint64_t)f[4] * g19[6]
        + (uint64_t)f2[5] * g19[5]
        + (uint64_t)f[6] * g19[4]
        + (uint64_t)f2[7] * g19[3]
        + (uint64_t)f[8] * g19[2]
        + (uint64_t)f2[9] * g19[1];
   r[1] = (uint64_t)f[0] * g[1]
        + (uint64_t)f[1] * g[0]
        + (uint64_t)f[2] * g19[9]
        + (uint64_t)f[3] * g19[8]
        + (uint64_t)f[4] * g19[7]
        + (uint64_t)f[5] * g19[6]
        + (uint64_t)f[6] * g19[5]
        + (uint64_t)f[7] * g19[4]
        + (uint64_t)f[8] * g19[3]
        + (uint64_t)f[9] * g19[2];
   r[2] = (uint64_t)f[0] * g[2]
        + (uint64_t)f2[1] * g[1]
        + (uint64_t)f[2] * g[0]
        + (uint64_t)f2[3] * g19[9]
        + (uint64_t)f[4] * g19[8]
        + (uint64_t)f2[5] * g19[7]
        + (uint64_t)f[6] * g19[6]
        + (uint64_t)f2[7] * g19[5]
        + (uint64_t)f[8] * g19[4]
        + (uint64_t)f2[9] * g19[3];
   r[3] = (uint64_t)f[0] * g[3]
        + (uint64_t)f[1] * g[2]
        + (uint64_t)f[2] * g[1]
        + (uint64_t)f[3] * g[0]
        + (uint64_t)f[4] * g19[9]
        + (uint64_t)f[5] * g19[8]
        + (uint64_t)f[6] * g19[7]
        + (uint64_t)f[7] * g19[6]
        + (uint64_t)f[8] * g19[5]
        + (uint64_t)f[9] * g19[4];
   r[4] = (uint64_t)f[0] * g[4]
        + (uint64_t)f2[1] * g[3]
        + (uint64_t)f[2] * g[2]
        + (uint64_t)f2[3] * g[1]
        + (uint64_t)f[4] * g[0]
        + (uint64_t)f2[5] * g19[9]
        + (uint64_t)f[6] * g19[8]
        + (uint64_t)f2[7] * g19[7]
        + (uint64_t)f[8] * g19[6]
        + (uint64_t)f2[9] * g19[5];
   r[5] = (uint64_t)f[0] * g[5]
        + (uint64_t)f[1] * g[4]
        + (uint64_t)f[2] * g[3]
        + (uint64_t)f[3] * g[2]
        + (uint64_t)f[4] * g[1]
        + (uint64_t)f[5] * g[0]
        + (uint64_t)f[6] * g19[9]
        + (uint64_t)f[7] * g19[8]
        + (uint64_t)f[8] * g19[7]
        + (uint64_t)f[9] * g19[6];
   r[6] = (uint64_t)f[0] * g[6]
        + (uint64_t)f2[1] * g[5]
        + (uint64_t)f[2] * g[4]
        + (uint64_t)f2[3] * g[3]
        + (uint64_t)f[4] * g[2]
        + (uint64_t)f2[5] * g[1]
        + (uint64_t)f[6] * g[0]
        + (uint64_t)f2[7] * g19[9]
        + (uint64_t)f[8] * g19[8]
        + (uint64_t)f2[9] * g19[7];
   r[7] = (uint64_t)f[0] * g[7]
        + (uint64_t)f[1] * g[6]
        + (uint64_t)f[2] * g[5]
        + (uint64_t)f[3] * g[4]
        + (uint64_t)f[4] * g[3]
        + (uint64_t)f[5] * g[2]
        + (uint64_t)f[6] * g[1]
        + (uint64_t)f[7] * g[0]
        + (uint64_t)f[8] * g19[9]
        + (uint64_t)f[9] * g19[8];
   r[8] = (uint64_t)f[0] * g[8]
        + (uint64_t)f2[1] * g[7]
        + (uint64_t)f[2] * g[6]
        + (uint64_t)f2[3] * g[5]
        + (uint64_t)f[4] * g[4]
        + (uint64_t)f2[5] * g[3]
        + (uint64_t)f[6] * g[2]
        + (uint64_t)f2[7] * g[1]
        + (uint64_t)f[8] * g[0]
        + (uint64_t)f2[9] * g19[9];
   r[9] = (uint64_t)f[0] * g[9]
        + (uint64_t)f[1] * g[8]
        + (uint64_t)f[2] * g[7]
        + (uint64_t)f[3] * g[6]
        + (uint64_t)f[4] * g[5]
        + (uint64_t)f[5] * g[4]
        + (uint64_t)f[6] * g[3]
        + (uint64_t)f[7] * g[2]
        + (uint64_t)f[8] * g[1]
        + (uint64_t)f[9] * g[0];
   fe10_carry64(h, r);
}

static void fe10_sq(fe10 h, const fe10 f)
{
   fe10_mul(h, f, f);
}

static void fe10_sqn(fe10 h, const fe10 f, unsigned n)
{
   fe10_sq(h, f);
   while (--n)
      fe10_sq(h, h);
}

static void fe10_mul_a24(fe10 h, const fe10 f)
{
   uint64_t r[10];
   unsigned i;
   for (i = 0; i < 10; i++)
      r[i] = (uint64_t)f[i] * 121665;
   fe10_carry64(h, r);
}

/* z^(2^255 - 21), the same addition chain as the 51-bit form */
static void fe10_invert(fe10 out, const fe10 z)
{
   fe10 z2, z9, z11, z_5, z_10, z_20, z_50, z_100, t;
   fe10_sq(z2, z);
   fe10_sqn(t, z2, 2);
   fe10_mul(z9, t, z);
   fe10_mul(z11, z9, z2);
   fe10_sq(t, z11);
   fe10_mul(z_5, t, z9);
   fe10_sqn(t, z_5, 5);
   fe10_mul(z_10, t, z_5);
   fe10_sqn(t, z_10, 10);
   fe10_mul(z_20, t, z_10);
   fe10_sqn(t, z_20, 20);
   fe10_mul(t, t, z_20);
   fe10_sqn(t, t, 10);
   fe10_mul(z_50, t, z_10);
   fe10_sqn(t, z_50, 50);
   fe10_mul(z_100, t, z_50);
   fe10_sqn(t, z_100, 100);
   fe10_mul(t, t, z_100);
   fe10_sqn(t, t, 50);
   fe10_mul(t, t, z_50);
   fe10_sqn(t, t, 5);
   fe10_mul(out, t, z11);
}

static void fe10_cswap(uint32_t swap, fe10 a, fe10 b)
{
   uint32_t mask = (uint32_t)0 - swap;
   unsigned i;
   for (i = 0; i < 10; i++)
   {
      uint32_t t = mask & (a[i] ^ b[i]);
      a[i] ^= t;
      b[i] ^= t;
   }
}

static int x25519_10(uint8_t *out, const uint8_t *scalar, const uint8_t *u_le)
{
   fe10     x1, x2, z2, x3, z3, A, AA, B, BB, E, C, D, DA, CB, t;
   uint32_t swap = 0;
   uint8_t  k[32];
   uint8_t  acc = 0;
   int      i;

   memcpy(k, scalar, 32);
   k[0]  &= 248;
   k[31] &= 127;
   k[31] |= 64;
   fe10_frombytes(x1, u_le);
   for (i = 0; i < 10; i++)
   {
      x2[i] = z2[i] = z3[i] = 0;
      x3[i] = x1[i];
   }
   x2[0] = 1;
   z3[0] = 1;

   for (i = 254; i >= 0; i--)
   {
      uint32_t bit = (uint32_t)((k[i >> 3] >> (i & 7)) & 1);
      swap ^= bit;
      fe10_cswap(swap, x2, x3);
      fe10_cswap(swap, z2, z3);
      swap = bit;

      fe10_add(A, x2, z2);
      fe10_sq(AA, A);
      fe10_sub(B, x2, z2);
      fe10_sq(BB, B);
      fe10_sub(E, AA, BB);
      fe10_add(C, x3, z3);
      fe10_sub(D, x3, z3);
      fe10_mul(DA, D, A);
      fe10_mul(CB, C, B);
      fe10_add(t, DA, CB);
      fe10_sq(x3, t);
      fe10_sub(t, DA, CB);
      fe10_sq(t, t);
      fe10_mul(z3, x1, t);
      fe10_mul(x2, AA, BB);
      fe10_mul_a24(t, E);
      fe10_add(t, AA, t);
      fe10_mul(z2, E, t);
   }
   fe10_cswap(swap, x2, x3);
   fe10_cswap(swap, z2, z3);

   fe10_invert(t, z2);
   fe10_mul(t, x2, t);
   fe10_tobytes(out, t);
   crypto_memzero(k, sizeof(k));
   for (i = 0; i < 32; i++)
      acc |= out[i];
   return acc ? 0 : -1;
}
#endif

int x25519(uint8_t *out, const uint8_t *scalar, const uint8_t *u_le)
{
#if defined(__SIZEOF_INT128__) && !defined(X25519_NO_128)
   return x25519_51(out, scalar, u_le);
#else
   return x25519_10(out, scalar, u_le);
#endif
}

int x25519_base(uint8_t *out, const uint8_t *scalar)
{
   uint8_t nine[32] = {9};
   return x25519(out, scalar, nine);
}

/* Largest curve here is P-384. */
#define EC_MAX_K (384 / BN_WORD_BITS)

static const uint8_t p256_p_be[32] = {
   0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};
static const uint8_t p256_n_be[32] = {
   0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51
};
static const uint8_t p256_b_be[32] = {
   0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,
   0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b
};
static const uint8_t p256_gx_be[32] = {
   0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
   0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96
};
static const uint8_t p256_gy_be[32] = {
   0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
   0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5
};

static const uint8_t p384_p_be[48] = {
   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
   0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff
};
static const uint8_t p384_n_be[48] = {
   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc7,0x63,0x4d,0x81,0xf4,0x37,0x2d,0xdf,
   0x58,0x1a,0x0d,0xb2,0x48,0xb0,0xa7,0x7a,0xec,0xec,0x19,0x6a,0xcc,0xc5,0x29,0x73
};
static const uint8_t p384_b_be[48] = {
   0xb3,0x31,0x2f,0xa7,0xe2,0x3e,0xe7,0xe4,0x98,0x8e,0x05,0x6b,0xe3,0xf8,0x2d,0x19,
   0x18,0x1d,0x9c,0x6e,0xfe,0x81,0x41,0x12,0x03,0x14,0x08,0x8f,0x50,0x13,0x87,0x5a,
   0xc6,0x56,0x39,0x8d,0x8a,0x2e,0xd1,0x9d,0x2a,0x85,0xc8,0xed,0xd3,0xec,0x2a,0xef
};
static const uint8_t p384_gx_be[48] = {
   0xaa,0x87,0xca,0x22,0xbe,0x8b,0x05,0x37,0x8e,0xb1,0xc7,0x1e,0xf3,0x20,0xad,0x74,
   0x6e,0x1d,0x3b,0x62,0x8b,0xa7,0x9b,0x98,0x59,0xf7,0x41,0xe0,0x82,0x54,0x2a,0x38,
   0x55,0x02,0xf2,0x5d,0xbf,0x55,0x29,0x6c,0x3a,0x54,0x5e,0x38,0x72,0x76,0x0a,0xb7
};
static const uint8_t p384_gy_be[48] = {
   0x36,0x17,0xde,0x4a,0x96,0x26,0x2c,0x6f,0x5d,0x9e,0x98,0xbf,0x92,0x92,0xdc,0x29,
   0xf8,0xf4,0x1d,0xbd,0x28,0x9a,0x14,0x7c,0xe9,0xda,0x31,0x13,0xb5,0xf0,0xb8,0xc0,
   0x0a,0x60,0xb1,0xce,0x1d,0x7e,0x81,0x9d,0x7a,0x43,0x1d,0x7c,0x90,0xea,0x0e,0x5f
};

/* Jacobian point, coordinates in the Montgomery domain of p.
 * Infinity is z == 0. */
struct ec_pt
{
   bn_word x[EC_MAX_K];
   bn_word y[EC_MAX_K];
   bn_word z[EC_MAX_K];
};

/* Curve constants, derived once from the octet strings above. */
struct ec_curve
{
   bn_word p[EC_MAX_K];
   bn_word n[EC_MAX_K];
   bn_word r2p[EC_MAX_K];
   bn_word r2n[EC_MAX_K];
   bn_word one[EC_MAX_K];    /* R mod p */
   bn_word b[EC_MAX_K];      /* b in Montgomery form */
   bn_word pm2[EC_MAX_K];    /* p - 2, inversion exponent */
   bn_word nm2[EC_MAX_K];    /* n - 2 */
   struct ec_pt g;
   bn_word  n0p;
   bn_word  n0n;
   const uint8_t *p_be, *n_be, *b_be, *gx_be, *gy_be;
   unsigned k;          /* words per element */
   unsigned bits;       /* 256 or 384 */
   unsigned bytes;      /* 32 or 48 */
   retro_atomic_int_t ready;
};

static struct ec_curve ec_p256 = { {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
   { {0}, {0}, {0} }, 0, 0,
   p256_p_be, p256_n_be, p256_b_be, p256_gx_be, p256_gy_be,
   256 / BN_WORD_BITS, 256, 32, 0 };
static struct ec_curve ec_p384 = { {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
   { {0}, {0}, {0} }, 0, 0,
   p384_p_be, p384_n_be, p384_b_be, p384_gx_be, p384_gy_be,
   384 / BN_WORD_BITS, 384, 48, 0 };

/* The curve constants are computed on first use. Two threads making
 * their first connections at once both get here; without a lock one
 * can publish the flag while the other is still rewriting the same
 * fields, and a third reads a half-written constant. The flag is an
 * acquire load on the fast path, the computation runs under a lock,
 * and the flag is stored with release once every field is in place. */
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
static retro_atomic_ptr_t ec_lock_ptr;
static slock_t *ec_lock_get(void)
{
   slock_t *l = (slock_t*)retro_atomic_load_acquire_ptr(&ec_lock_ptr);
   if (!l)
   {
      slock_t *fresh = slock_new();
      if (retro_atomic_cas_ptr(&ec_lock_ptr, NULL, fresh))
         l = fresh;
      else
      {
         slock_free(fresh);
         l = (slock_t*)retro_atomic_load_acquire_ptr(&ec_lock_ptr);
      }
   }
   return l;
}
#define EC_LOCK()   slock_lock(ec_lock_get())
#define EC_UNLOCK() slock_unlock(ec_lock_get())
#else
#define EC_LOCK()   do { } while (0)
#define EC_UNLOCK() do { } while (0)
#endif

static void ec_init(struct ec_curve *cv)
{
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];
   bn_word two[EC_MAX_K];
   const unsigned k = cv->k;

   if (retro_atomic_load_acquire_int(&cv->ready))
      return;
   EC_LOCK();
   if (retro_atomic_load_acquire_int(&cv->ready))
   {
      EC_UNLOCK();
      return;
   }

   bn_from_be(cv->p, k, cv->p_be, cv->bytes);
   bn_from_be(cv->n, k, cv->n_be, cv->bytes);
   cv->n0p = bn_mont_n0(cv->p);
   cv->n0n = bn_mont_n0(cv->n);
   bn_mont_r2(cv->r2p, cv->p, k);
   bn_mont_r2(cv->r2n, cv->n, k);

   bn_zero(two, k);
   two[0] = 1;
   bn_mont_mul(cv->one, cv->r2p, two, cv->p, cv->n0p, k, tmp);

   two[0] = 2;
   bn_sub(cv->pm2, cv->p, two, k);
   bn_sub(cv->nm2, cv->n, two, k);

   bn_from_be(cv->b, k, cv->b_be, cv->bytes);
   bn_mont_mul(cv->b, cv->b, cv->r2p, cv->p, cv->n0p, k, tmp);

   bn_from_be(cv->g.x, k, cv->gx_be, cv->bytes);
   bn_from_be(cv->g.y, k, cv->gy_be, cv->bytes);
   bn_mont_mul(cv->g.x, cv->g.x, cv->r2p, cv->p, cv->n0p, k, tmp);
   bn_mont_mul(cv->g.y, cv->g.y, cv->r2p, cv->p, cv->n0p, k, tmp);
   bn_copy(cv->g.z, cv->one, k);

   retro_atomic_store_release_int(&cv->ready, 1);
   EC_UNLOCK();
}

/* Field helpers, all in the Montgomery domain. */
/* The word count is a run-time field, which stops the compiler from
 * unrolling the limb loops; P-256 is the curve on the TLS fast path,
 * so its length is spelled as a constant on that branch and the
 * compiler clones the Montgomery routines for it. */
#define EC_P256_K (256 / BN_WORD_BITS)

static void p256_fmul(const struct ec_curve *cv, bn_word *r,
      const bn_word *a, const bn_word *b)
{
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];
   if (cv->k == EC_P256_K)
      bn_mont_mul(r, a, b, cv->p, cv->n0p, EC_P256_K, tmp);
   else
      bn_mont_mul(r, a, b, cv->p, cv->n0p, cv->k, tmp);
}

static void p256_fsqr(const struct ec_curve *cv, bn_word *r, const bn_word *a)
{
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];
   if (cv->k == EC_P256_K)
      bn_mont_sqr(r, a, cv->p, cv->n0p, EC_P256_K, tmp);
   else
      bn_mont_sqr(r, a, cv->p, cv->n0p, cv->k, tmp);
}

static void p256_fadd(const struct ec_curve *cv, bn_word *r,
      const bn_word *a, const bn_word *b)
{
   if (cv->k == EC_P256_K)
      bn_mod_add(r, a, b, cv->p, EC_P256_K);
   else
      bn_mod_add(r, a, b, cv->p, cv->k);
}

static void p256_fsub(const struct ec_curve *cv, bn_word *r,
      const bn_word *a, const bn_word *b)
{
   if (cv->k == EC_P256_K)
      bn_mod_sub(r, a, b, cv->p, EC_P256_K);
   else
      bn_mod_sub(r, a, b, cv->p, cv->k);
}

/* r = a^e in the Montgomery domain (a is aR, so is the result). */
static void p256_fexp(const struct ec_curve *cv, bn_word *r,
      const bn_word *a, const bn_word *e,
      const bn_word *m, bn_word n0, const bn_word *one)
{
   bn_word acc[EC_MAX_K];
   bn_word t[EC_MAX_K];
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];
   unsigned i;

   bn_copy(acc, one, cv->k);
   for (i = cv->bits; i-- > 0; )
   {
      bn_mont_sqr(t, acc, m, n0, cv->k, tmp);
      if (bn_get_bit(e, i))
         bn_mont_mul(acc, t, a, m, n0, cv->k, tmp);
      else
         bn_copy(acc, t, cv->k);
   }
   bn_copy(r, acc, cv->k);
}

static void p256_pt_double(const struct ec_curve *cv, struct ec_pt *r, const struct ec_pt *a)
{
   /* dbl-2001-b, a = -3 */
   bn_word delta[EC_MAX_K], gamma[EC_MAX_K], beta[EC_MAX_K], alpha[EC_MAX_K], t1[EC_MAX_K], t2[EC_MAX_K];

   p256_fsqr(cv, delta, a->z);
   p256_fsqr(cv, gamma, a->y);
   p256_fmul(cv, beta, a->x, gamma);
   p256_fsub(cv, t1, a->x, delta);
   p256_fadd(cv, t2, a->x, delta);
   p256_fmul(cv, alpha, t1, t2);
   p256_fadd(cv, t1, alpha, alpha);
   p256_fadd(cv, alpha, t1, alpha);             /* 3 (x - d)(x + d) */

   p256_fsqr(cv, t1, alpha);
   p256_fadd(cv, t2, beta, beta);
   p256_fadd(cv, t2, t2, t2);
   p256_fadd(cv, t2, t2, t2);                   /* 8 beta */
   p256_fsub(cv, t1, t1, t2);                   /* x3 = alpha^2 - 8 beta */

   p256_fadd(cv, t2, a->y, a->z);
   p256_fsqr(cv, t2, t2);
   p256_fsub(cv, t2, t2, gamma);
   p256_fsub(cv, r->z, t2, delta);              /* z3 = (y + z)^2 - gamma - delta */

   p256_fadd(cv, t2, beta, beta);
   p256_fadd(cv, t2, t2, t2);                   /* 4 beta */
   p256_fsub(cv, t2, t2, t1);                   /* 4 beta - x3 */
   p256_fmul(cv, t2, alpha, t2);
   p256_fsqr(cv, gamma, gamma);
   p256_fadd(cv, gamma, gamma, gamma);
   p256_fadd(cv, gamma, gamma, gamma);
   p256_fadd(cv, gamma, gamma, gamma);          /* 8 gamma^2 */
   p256_fsub(cv, r->y, t2, gamma);
   bn_copy(r->x, t1, cv->k);
}

static void p256_pt_add(const struct ec_curve *cv, struct ec_pt *r, const struct ec_pt *a,
      const struct ec_pt *b)
{
   /* add-2007-bl */
   bn_word z1z1[EC_MAX_K], z2z2[EC_MAX_K], u1[EC_MAX_K], u2[EC_MAX_K], s1[EC_MAX_K], s2[EC_MAX_K], h[EC_MAX_K], rr[EC_MAX_K];
   bn_word t[EC_MAX_K];

   if (bn_is_zero(a->z, cv->k))
   {
      *r = *b;
      return;
   }
   if (bn_is_zero(b->z, cv->k))
   {
      *r = *a;
      return;
   }

   p256_fsqr(cv, z1z1, a->z);
   p256_fsqr(cv, z2z2, b->z);
   p256_fmul(cv, u1, a->x, z2z2);
   p256_fmul(cv, u2, b->x, z1z1);
   p256_fmul(cv, s1, a->y, b->z);
   p256_fmul(cv, s1, s1, z2z2);
   p256_fmul(cv, s2, b->y, a->z);
   p256_fmul(cv, s2, s2, z1z1);
   p256_fsub(cv, h, u2, u1);
   p256_fsub(cv, rr, s2, s1);

   if (bn_is_zero(h, cv->k))
   {
      if (bn_is_zero(rr, cv->k))
         p256_pt_double(cv, r, a);
      else
      {
         bn_zero(r->x, cv->k);
         bn_zero(r->y, cv->k);
         bn_zero(r->z, cv->k);
      }
      return;
   }

   p256_fadd(cv, rr, rr, rr);                   /* r = 2 (s2 - s1) */
   p256_fadd(cv, t, h, h);
   p256_fsqr(cv, t, t);                         /* i = (2h)^2 */
   p256_fmul(cv, u2, h, t);                     /* j = h i */
   p256_fmul(cv, u1, u1, t);                    /* v = u1 i */

   p256_fsqr(cv, t, rr);
   p256_fsub(cv, t, t, u2);
   p256_fsub(cv, t, t, u1);
   p256_fsub(cv, t, t, u1);                     /* x3 = r^2 - j - 2v */

   p256_fsub(cv, u1, u1, t);                    /* v - x3 */
   p256_fmul(cv, u1, rr, u1);
   p256_fmul(cv, s1, s1, u2);
   p256_fadd(cv, s1, s1, s1);                   /* 2 s1 j */
   p256_fsub(cv, r->y, u1, s1);

   p256_fadd(cv, s2, a->z, b->z);
   p256_fsqr(cv, s2, s2);
   p256_fsub(cv, s2, s2, z1z1);
   p256_fsub(cv, s2, s2, z2z2);
   p256_fmul(cv, r->z, s2, h);
   bn_copy(r->x, t, cv->k);
}

static void p256_pt_select(const struct ec_curve *cv, struct ec_pt *r, bn_word bit,
      const struct ec_pt *a, const struct ec_pt *b)
{
   bn_select(r->x, bit, a->x, b->x, cv->k);
   bn_select(r->y, bit, a->y, b->y, cv->k);
   bn_select(r->z, bit, a->z, b->z, cv->k);
}

/* r = k * a. Double-and-always-add with a select, so the sequence of
 * field operations does not depend on the scalar; what still does is
 * the infinity test inside p256_pt_add for the leading zero bits. The
 * fallback for p256_pt_mul() when its table cannot be had. */
static void p256_pt_mul_bits(const struct ec_curve *cv, struct ec_pt *r, const bn_word *k,
      const struct ec_pt *a)
{
   struct ec_pt acc, t;
   unsigned i;

   bn_zero(acc.x, cv->k);
   bn_zero(acc.y, cv->k);
   bn_zero(acc.z, cv->k);

   for (i = cv->bits; i-- > 0; )
   {
      p256_pt_double(cv, &acc, &acc);
      p256_pt_add(cv, &t, &acc, a);
      p256_pt_select(cv, &acc, bn_get_bit(k, i), &t, &acc);
   }
   *r = acc;
}

/* Four scalar bits from bit 4w up. */
static unsigned ec_digit4(const bn_word *k, unsigned w)
{
   return (unsigned)(bn_get_bit(k, 4 * w)
         | (bn_get_bit(k, 4 * w + 1) << 1)
         | (bn_get_bit(k, 4 * w + 2) << 2)
         | (bn_get_bit(k, 4 * w + 3) << 3));
}

/* 1..15 times a in t[1..15]; t[0] unused. */
static void ec_table4(const struct ec_curve *cv, struct ec_pt *t, const struct ec_pt *a)
{
   unsigned j;
   t[1] = *a;
   p256_pt_double(cv, &t[2], a);
   for (j = 3; j < 16; j++)
      p256_pt_add(cv, &t[j], &t[j - 1], a);
}

/* r = k * a, four bits at a time: per window four doublings and one
 * addition, where the bit-by-bit form does one addition per bit. The
 * table entry is chosen by scanning all fifteen with a select, and a
 * zero digit adds t[1] anyway and throws the sum away, so neither the
 * memory touched nor the field operations follow the scalar, save the
 * infinity test for its leading zero digits, as before. The table
 * (fifteen points, 2 KiB for P-384) is on the heap: the stack budget. */
static void p256_pt_mul(const struct ec_curve *cv, struct ec_pt *r, const bn_word *k,
      const struct ec_pt *a)
{
   struct ec_pt *t = (struct ec_pt*)malloc(16 * sizeof(*t));
   struct ec_pt  acc, sel, sum;
   unsigned      w, j;

   if (!t)
   {
      p256_pt_mul_bits(cv, r, k, a);
      return;
   }
   ec_table4(cv, t, a);
   bn_zero(acc.x, cv->k);
   bn_zero(acc.y, cv->k);
   bn_zero(acc.z, cv->k);
   for (w = cv->bits / 4; w-- > 0; )
   {
      unsigned d = ec_digit4(k, w);
      p256_pt_double(cv, &acc, &acc);
      p256_pt_double(cv, &acc, &acc);
      p256_pt_double(cv, &acc, &acc);
      p256_pt_double(cv, &acc, &acc);
      sel = t[1];
      for (j = 2; j < 16; j++)
      {
         bn_word x  = (bn_word)(j ^ d);
         bn_word eq = (bn_word)1 ^ ((x | ((bn_word)0 - x)) >> (BN_WORD_BITS - 1));
         p256_pt_select(cv, &sel, eq, &t[j], &sel);
      }
      p256_pt_add(cv, &sum, &acc, &sel);
      {
         bn_word nz = ((bn_word)d | ((bn_word)0 - (bn_word)d)) >> (BN_WORD_BITS - 1);
         p256_pt_select(cv, &acc, nz, &sum, &acc);
      }
   }
   *r = acc;
   crypto_memzero(t, 16 * sizeof(*t));
   crypto_memzero(&sel, sizeof(sel));
   crypto_memzero(&sum, sizeof(sum));
   free(t);
}

/* r = u1 g + u2 q in one pass (Shamir's trick), four bits of each
 * scalar per window: the doublings are shared. For signature checks,
 * where every input is public, so the additions may skip zero digits. */
static void p256_pt_mul2(const struct ec_curve *cv, struct ec_pt *r,
      const bn_word *u1, const struct ec_pt *g, const bn_word *u2, const struct ec_pt *q)
{
   struct ec_pt *tg = (struct ec_pt*)malloc(32 * sizeof(*tg));
   struct ec_pt *tq;
   struct ec_pt  acc;
   unsigned      w;

   if (!tg)
   {
      struct ec_pt b;
      p256_pt_mul_bits(cv, r, u1, g);
      p256_pt_mul_bits(cv, &b, u2, q);
      p256_pt_add(cv, r, r, &b);
      return;
   }
   tq = tg + 16;
   ec_table4(cv, tg, g);
   ec_table4(cv, tq, q);
   bn_zero(acc.x, cv->k);
   bn_zero(acc.y, cv->k);
   bn_zero(acc.z, cv->k);
   for (w = cv->bits / 4; w-- > 0; )
   {
      unsigned d1 = ec_digit4(u1, w), d2 = ec_digit4(u2, w);
      p256_pt_double(cv, &acc, &acc);
      p256_pt_double(cv, &acc, &acc);
      p256_pt_double(cv, &acc, &acc);
      p256_pt_double(cv, &acc, &acc);
      if (d1)
         p256_pt_add(cv, &acc, &acc, &tg[d1]);
      if (d2)
         p256_pt_add(cv, &acc, &acc, &tq[d2]);
   }
   *r = acc;
   free(tg);
}

/* Affine x, y (plain domain) from Jacobian. Returns -1 at infinity. */
static int p256_pt_affine(const struct ec_curve *cv, bn_word *x, bn_word *y, const struct ec_pt *a)
{
   bn_word zi[EC_MAX_K], zi2[EC_MAX_K], one[EC_MAX_K];
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];

   if (bn_is_zero(a->z, cv->k))
      return -1;

   p256_fexp(cv, zi, a->z, cv->pm2, cv->p, cv->n0p, cv->one);
   p256_fsqr(cv, zi2, zi);
   p256_fmul(cv, x, a->x, zi2);
   if (y)
   {
      p256_fmul(cv, zi, zi, zi2);
      p256_fmul(cv, y, a->y, zi);
   }

   /* Out of the Montgomery domain. */
   bn_zero(one, cv->k);
   one[0] = 1;
   bn_mont_mul(x, x, one, cv->p, cv->n0p, cv->k, tmp);
   if (y)
      bn_mont_mul(y, y, one, cv->p, cv->n0p, cv->k, tmp);
   return 0;
}

/* Parse 0x04 || X || Y, check both coordinates < p and the curve
 * equation y^2 = x^3 - 3x + b. */
static int p256_pt_load(const struct ec_curve *cv, struct ec_pt *r, const uint8_t *pub)
{
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];
   bn_word lhs[EC_MAX_K], rhs[EC_MAX_K], t[EC_MAX_K];

   if (pub[0] != 0x04)
      return -1;

   bn_from_be(r->x, cv->k, pub + 1, cv->bytes);
   bn_from_be(r->y, cv->k, pub + 1 + cv->bytes, cv->bytes);
   if (bn_cmp(r->x, cv->p, cv->k) >= 0 || bn_cmp(r->y, cv->p, cv->k) >= 0)
      return -1;

   bn_mont_mul(r->x, r->x, cv->r2p, cv->p, cv->n0p, cv->k, tmp);
   bn_mont_mul(r->y, r->y, cv->r2p, cv->p, cv->n0p, cv->k, tmp);
   bn_copy(r->z, cv->one, cv->k);

   p256_fsqr(cv, lhs, r->y);
   p256_fsqr(cv, rhs, r->x);
   p256_fmul(cv, rhs, rhs, r->x);
   p256_fadd(cv, t, r->x, r->x);
   p256_fadd(cv, t, t, r->x);
   p256_fsub(cv, rhs, rhs, t);
   p256_fadd(cv, rhs, rhs, cv->b);
   return bn_cmp(lhs, rhs, cv->k) == 0 ? 0 : -1;
}

static int p256_scalar_load(const struct ec_curve *cv, bn_word *k, const uint8_t *s)
{
   bn_from_be(k, cv->k, s, cv->bytes);
   if (bn_is_zero(k, cv->k) || bn_cmp(k, cv->n, cv->k) >= 0)
      return -1;
   return 0;
}

static int ec_keygen(const struct ec_curve *cv, const uint8_t *priv, uint8_t *pub)
{
   bn_word k[EC_MAX_K], x[EC_MAX_K], y[EC_MAX_K];
   struct ec_pt q;
   int ret;

   if (p256_scalar_load(cv, k, priv) != 0)
      return -1;
   p256_pt_mul(cv, &q, k, &cv->g);
   ret = p256_pt_affine(cv, x, y, &q);
   crypto_memzero(k, sizeof(k));
   crypto_memzero(&q, sizeof(q));
   if (ret != 0)
      return -1;
   pub[0] = 0x04;
   bn_to_be(pub + 1, cv->bytes, x, cv->k);
   bn_to_be(pub + 1 + cv->bytes, cv->bytes, y, cv->k);
   return 0;
}

static int ec_ecdh(const struct ec_curve *cv, const uint8_t *priv, const uint8_t *peer, uint8_t *shared)
{
   bn_word k[EC_MAX_K], x[EC_MAX_K];
   struct ec_pt q;
   int ret;

   if (p256_scalar_load(cv, k, priv) != 0 || p256_pt_load(cv, &q, peer) != 0)
      return -1;
   p256_pt_mul(cv, &q, k, &q);
   ret = p256_pt_affine(cv, x, NULL, &q);
   crypto_memzero(k, sizeof(k));
   crypto_memzero(&q, sizeof(q));
   if (ret != 0)
      return -1;
   bn_to_be(shared, cv->bytes, x, cv->k);
   crypto_memzero(x, sizeof(x));
   return 0;
}

static int ec_ecdsa_verify(const struct ec_curve *cv, const uint8_t *pub,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *r, const uint8_t *s)
{
   bn_word rr[EC_MAX_K], ss[EC_MAX_K], e[EC_MAX_K], w[EC_MAX_K], u1[EC_MAX_K], u2[EC_MAX_K], x[EC_MAX_K];
   bn_word tmp[BN_MONT_TMP_WORDS(EC_MAX_K)];
   bn_word one[EC_MAX_K];
   struct ec_pt q, a;

   if (p256_pt_load(cv, &q, pub) != 0)
      return -1;
   if (p256_scalar_load(cv, rr, r) != 0 || p256_scalar_load(cv, ss, s) != 0)
      return -1;

   /* e = leftmost 256 bits of the digest, reduced once. */
   bn_from_be(e, cv->k, digest, digest_len > cv->bytes ? cv->bytes : digest_len);
   if (bn_cmp(e, cv->n, cv->k) >= 0)
      bn_sub(e, e, cv->n, cv->k);

   /* w = s^-1 mod n: s^(n-2) in the Montgomery domain of n, then
    * back out, so the multiplies below run in the plain domain. */
   bn_mont_mul(w, ss, cv->r2n, cv->n, cv->n0n, cv->k, tmp);
   bn_zero(one, cv->k);
   one[0] = 1;
   bn_mont_mul(x, cv->r2n, one, cv->n, cv->n0n, cv->k, tmp);  /* R mod n */
   p256_fexp(cv, w, w, cv->nm2, cv->n, cv->n0n, x);
   bn_mont_mul(w, w, one, cv->n, cv->n0n, cv->k, tmp);

   bn_mod_mul(u1, e,  w, cv->n, cv->r2n, cv->n0n, cv->k, tmp);
   bn_mod_mul(u2, rr, w, cv->n, cv->r2n, cv->n0n, cv->k, tmp);

   p256_pt_mul2(cv, &a, u1, &cv->g, u2, &q);
   if (p256_pt_affine(cv, x, NULL, &a) != 0)
      return -1;
   if (bn_cmp(x, cv->n, cv->k) >= 0)
      bn_sub(x, x, cv->n, cv->k);
   return bn_cmp(x, rr, cv->k) == 0 ? 0 : -1;
}


int p256_keygen(const uint8_t *priv, uint8_t *pub)
{
   ec_init(&ec_p256);
   return ec_keygen(&ec_p256, priv, pub);
}

int p256_ecdh(const uint8_t *priv, const uint8_t *peer, uint8_t *shared)
{
   ec_init(&ec_p256);
   return ec_ecdh(&ec_p256, priv, peer, shared);
}

int p256_ecdsa_verify(const uint8_t *pub,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *r, const uint8_t *s)
{
   ec_init(&ec_p256);
   return ec_ecdsa_verify(&ec_p256, pub, digest, digest_len, r, s);
}

int p384_keygen(const uint8_t *priv, uint8_t *pub)
{
   ec_init(&ec_p384);
   return ec_keygen(&ec_p384, priv, pub);
}

int p384_ecdh(const uint8_t *priv, const uint8_t *peer, uint8_t *shared)
{
   ec_init(&ec_p384);
   return ec_ecdh(&ec_p384, priv, peer, shared);
}

int p384_ecdsa_verify(const uint8_t *pub,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *r, const uint8_t *s)
{
   ec_init(&ec_p384);
   return ec_ecdsa_verify(&ec_p384, pub, digest, digest_len, r, s);
}
