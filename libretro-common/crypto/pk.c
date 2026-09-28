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

#define P256_K (256 / BN_WORD_BITS)

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

/* Jacobian point, coordinates in the Montgomery domain of p.
 * Infinity is z == 0. */
struct p256_pt
{
   bn_word x[P256_K];
   bn_word y[P256_K];
   bn_word z[P256_K];
};

/* Curve constants, derived once from the octet strings above. */
struct p256_ctx
{
   bn_word p[P256_K];
   bn_word n[P256_K];
   bn_word r2p[P256_K];
   bn_word r2n[P256_K];
   bn_word one[P256_K];    /* R mod p */
   bn_word b[P256_K];      /* b in Montgomery form */
   bn_word pm2[P256_K];    /* p - 2, inversion exponent */
   bn_word nm2[P256_K];    /* n - 2 */
   struct p256_pt g;
   bn_word n0p;
   bn_word n0n;
   int      ready;
};

static struct p256_ctx p256;

static void p256_init(void)
{
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];
   bn_word two[P256_K];

   if (p256.ready)
      return;

   bn_from_be(p256.p, P256_K, p256_p_be, 32);
   bn_from_be(p256.n, P256_K, p256_n_be, 32);
   p256.n0p = bn_mont_n0(p256.p);
   p256.n0n = bn_mont_n0(p256.n);
   bn_mont_r2(p256.r2p, p256.p, P256_K);
   bn_mont_r2(p256.r2n, p256.n, P256_K);

   bn_zero(two, P256_K);
   two[0] = 1;
   bn_mont_mul(p256.one, p256.r2p, two, p256.p, p256.n0p, P256_K, tmp);

   two[0] = 2;
   bn_sub(p256.pm2, p256.p, two, P256_K);
   bn_sub(p256.nm2, p256.n, two, P256_K);

   bn_from_be(p256.b, P256_K, p256_b_be, 32);
   bn_mont_mul(p256.b, p256.b, p256.r2p, p256.p, p256.n0p, P256_K, tmp);

   bn_from_be(p256.g.x, P256_K, p256_gx_be, 32);
   bn_from_be(p256.g.y, P256_K, p256_gy_be, 32);
   bn_mont_mul(p256.g.x, p256.g.x, p256.r2p, p256.p, p256.n0p, P256_K, tmp);
   bn_mont_mul(p256.g.y, p256.g.y, p256.r2p, p256.p, p256.n0p, P256_K, tmp);
   bn_copy(p256.g.z, p256.one, P256_K);

   /* Every field above is written with the same values by any thread
    * that races in here, so the flag can go last without a lock. */
   p256.ready = 1;
}

/* Field helpers, all in the Montgomery domain. */
static void p256_fmul(bn_word *r, const bn_word *a, const bn_word *b)
{
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];
   bn_mont_mul(r, a, b, p256.p, p256.n0p, P256_K, tmp);
}

static void p256_fsqr(bn_word *r, const bn_word *a)
{
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];
   bn_mont_sqr(r, a, p256.p, p256.n0p, P256_K, tmp);
}
#define p256_fadd(r, a, b) bn_mod_add(r, a, b, p256.p, P256_K)
#define p256_fsub(r, a, b) bn_mod_sub(r, a, b, p256.p, P256_K)

/* r = a^e in the Montgomery domain (a is aR, so is the result). */
static void p256_fexp(bn_word *r, const bn_word *a, const bn_word *e,
      const bn_word *m, bn_word n0, const bn_word *one)
{
   bn_word acc[P256_K];
   bn_word t[P256_K];
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];
   unsigned i;

   bn_copy(acc, one, P256_K);
   for (i = 256; i-- > 0; )
   {
      bn_mont_sqr(t, acc, m, n0, P256_K, tmp);
      if (bn_get_bit(e, i))
         bn_mont_mul(acc, t, a, m, n0, P256_K, tmp);
      else
         bn_copy(acc, t, P256_K);
   }
   bn_copy(r, acc, P256_K);
}

static void p256_pt_double(struct p256_pt *r, const struct p256_pt *a)
{
   /* dbl-2001-b, a = -3 */
   bn_word delta[P256_K], gamma[P256_K], beta[P256_K], alpha[P256_K], t1[P256_K], t2[P256_K];

   p256_fsqr(delta, a->z);
   p256_fsqr(gamma, a->y);
   p256_fmul(beta, a->x, gamma);
   p256_fsub(t1, a->x, delta);
   p256_fadd(t2, a->x, delta);
   p256_fmul(alpha, t1, t2);
   p256_fadd(t1, alpha, alpha);
   p256_fadd(alpha, t1, alpha);             /* 3 (x - d)(x + d) */

   p256_fsqr(t1, alpha);
   p256_fadd(t2, beta, beta);
   p256_fadd(t2, t2, t2);
   p256_fadd(t2, t2, t2);                   /* 8 beta */
   p256_fsub(t1, t1, t2);                   /* x3 = alpha^2 - 8 beta */

   p256_fadd(t2, a->y, a->z);
   p256_fsqr(t2, t2);
   p256_fsub(t2, t2, gamma);
   p256_fsub(r->z, t2, delta);              /* z3 = (y + z)^2 - gamma - delta */

   p256_fadd(t2, beta, beta);
   p256_fadd(t2, t2, t2);                   /* 4 beta */
   p256_fsub(t2, t2, t1);                   /* 4 beta - x3 */
   p256_fmul(t2, alpha, t2);
   p256_fsqr(gamma, gamma);
   p256_fadd(gamma, gamma, gamma);
   p256_fadd(gamma, gamma, gamma);
   p256_fadd(gamma, gamma, gamma);          /* 8 gamma^2 */
   p256_fsub(r->y, t2, gamma);
   bn_copy(r->x, t1, P256_K);
}

static void p256_pt_add(struct p256_pt *r, const struct p256_pt *a,
      const struct p256_pt *b)
{
   /* add-2007-bl */
   bn_word z1z1[P256_K], z2z2[P256_K], u1[P256_K], u2[P256_K], s1[P256_K], s2[P256_K], h[P256_K], rr[P256_K];
   bn_word t[P256_K];

   if (bn_is_zero(a->z, P256_K))
   {
      *r = *b;
      return;
   }
   if (bn_is_zero(b->z, P256_K))
   {
      *r = *a;
      return;
   }

   p256_fsqr(z1z1, a->z);
   p256_fsqr(z2z2, b->z);
   p256_fmul(u1, a->x, z2z2);
   p256_fmul(u2, b->x, z1z1);
   p256_fmul(s1, a->y, b->z);
   p256_fmul(s1, s1, z2z2);
   p256_fmul(s2, b->y, a->z);
   p256_fmul(s2, s2, z1z1);
   p256_fsub(h, u2, u1);
   p256_fsub(rr, s2, s1);

   if (bn_is_zero(h, P256_K))
   {
      if (bn_is_zero(rr, P256_K))
         p256_pt_double(r, a);
      else
      {
         bn_zero(r->x, P256_K);
         bn_zero(r->y, P256_K);
         bn_zero(r->z, P256_K);
      }
      return;
   }

   p256_fadd(rr, rr, rr);                   /* r = 2 (s2 - s1) */
   p256_fadd(t, h, h);
   p256_fsqr(t, t);                         /* i = (2h)^2 */
   p256_fmul(u2, h, t);                     /* j = h i */
   p256_fmul(u1, u1, t);                    /* v = u1 i */

   p256_fsqr(t, rr);
   p256_fsub(t, t, u2);
   p256_fsub(t, t, u1);
   p256_fsub(t, t, u1);                     /* x3 = r^2 - j - 2v */

   p256_fsub(u1, u1, t);                    /* v - x3 */
   p256_fmul(u1, rr, u1);
   p256_fmul(s1, s1, u2);
   p256_fadd(s1, s1, s1);                   /* 2 s1 j */
   p256_fsub(r->y, u1, s1);

   p256_fadd(s2, a->z, b->z);
   p256_fsqr(s2, s2);
   p256_fsub(s2, s2, z1z1);
   p256_fsub(s2, s2, z2z2);
   p256_fmul(r->z, s2, h);
   bn_copy(r->x, t, P256_K);
}

static void p256_pt_select(struct p256_pt *r, bn_word bit,
      const struct p256_pt *a, const struct p256_pt *b)
{
   bn_select(r->x, bit, a->x, b->x, P256_K);
   bn_select(r->y, bit, a->y, b->y, P256_K);
   bn_select(r->z, bit, a->z, b->z, P256_K);
}

/* r = k * a. Double-and-always-add with a select, so the sequence of
 * field operations does not depend on the scalar; what still does is
 * the infinity test inside p256_pt_add for the leading zero bits. */
static void p256_pt_mul(struct p256_pt *r, const bn_word *k,
      const struct p256_pt *a)
{
   struct p256_pt acc, t;
   unsigned i;

   bn_zero(acc.x, P256_K);
   bn_zero(acc.y, P256_K);
   bn_zero(acc.z, P256_K);

   for (i = 256; i-- > 0; )
   {
      p256_pt_double(&acc, &acc);
      p256_pt_add(&t, &acc, a);
      p256_pt_select(&acc, bn_get_bit(k, i), &t, &acc);
   }
   *r = acc;
}

/* Affine x, y (plain domain) from Jacobian. Returns -1 at infinity. */
static int p256_pt_affine(bn_word *x, bn_word *y, const struct p256_pt *a)
{
   bn_word zi[P256_K], zi2[P256_K], one[P256_K];
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];

   if (bn_is_zero(a->z, P256_K))
      return -1;

   p256_fexp(zi, a->z, p256.pm2, p256.p, p256.n0p, p256.one);
   p256_fsqr(zi2, zi);
   p256_fmul(x, a->x, zi2);
   if (y)
   {
      p256_fmul(zi, zi, zi2);
      p256_fmul(y, a->y, zi);
   }

   /* Out of the Montgomery domain. */
   bn_zero(one, P256_K);
   one[0] = 1;
   bn_mont_mul(x, x, one, p256.p, p256.n0p, P256_K, tmp);
   if (y)
      bn_mont_mul(y, y, one, p256.p, p256.n0p, P256_K, tmp);
   return 0;
}

/* Parse 0x04 || X || Y, check both coordinates < p and the curve
 * equation y^2 = x^3 - 3x + b. */
static int p256_pt_load(struct p256_pt *r, const uint8_t *pub)
{
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];
   bn_word lhs[P256_K], rhs[P256_K], t[P256_K];

   if (pub[0] != 0x04)
      return -1;

   bn_from_be(r->x, P256_K, pub + 1, 32);
   bn_from_be(r->y, P256_K, pub + 33, 32);
   if (bn_cmp(r->x, p256.p, P256_K) >= 0 || bn_cmp(r->y, p256.p, P256_K) >= 0)
      return -1;

   bn_mont_mul(r->x, r->x, p256.r2p, p256.p, p256.n0p, P256_K, tmp);
   bn_mont_mul(r->y, r->y, p256.r2p, p256.p, p256.n0p, P256_K, tmp);
   bn_copy(r->z, p256.one, P256_K);

   p256_fsqr(lhs, r->y);
   p256_fsqr(rhs, r->x);
   p256_fmul(rhs, rhs, r->x);
   p256_fadd(t, r->x, r->x);
   p256_fadd(t, t, r->x);
   p256_fsub(rhs, rhs, t);
   p256_fadd(rhs, rhs, p256.b);
   return bn_cmp(lhs, rhs, P256_K) == 0 ? 0 : -1;
}

static int p256_scalar_load(bn_word *k, const uint8_t *s)
{
   bn_from_be(k, P256_K, s, 32);
   if (bn_is_zero(k, P256_K) || bn_cmp(k, p256.n, P256_K) >= 0)
      return -1;
   return 0;
}

int p256_keygen(const uint8_t *priv, uint8_t *pub)
{
   bn_word k[P256_K], x[P256_K], y[P256_K];
   struct p256_pt q;
   int ret;

   p256_init();
   if (p256_scalar_load(k, priv) != 0)
      return -1;
   p256_pt_mul(&q, k, &p256.g);
   ret = p256_pt_affine(x, y, &q);
   crypto_memzero(k, sizeof(k));
   crypto_memzero(&q, sizeof(q));
   if (ret != 0)
      return -1;
   pub[0] = 0x04;
   bn_to_be(pub + 1, 32, x, P256_K);
   bn_to_be(pub + 33, 32, y, P256_K);
   return 0;
}

int p256_ecdh(const uint8_t *priv, const uint8_t *peer, uint8_t *shared)
{
   bn_word k[P256_K], x[P256_K];
   struct p256_pt q;
   int ret;

   p256_init();
   if (p256_scalar_load(k, priv) != 0 || p256_pt_load(&q, peer) != 0)
      return -1;
   p256_pt_mul(&q, k, &q);
   ret = p256_pt_affine(x, NULL, &q);
   crypto_memzero(k, sizeof(k));
   crypto_memzero(&q, sizeof(q));
   if (ret != 0)
      return -1;
   bn_to_be(shared, 32, x, P256_K);
   crypto_memzero(x, sizeof(x));
   return 0;
}

int p256_ecdsa_verify(const uint8_t *pub,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *r, const uint8_t *s)
{
   bn_word rr[P256_K], ss[P256_K], e[P256_K], w[P256_K], u1[P256_K], u2[P256_K], x[P256_K];
   bn_word tmp[BN_MONT_TMP_WORDS(P256_K)];
   bn_word one[P256_K];
   struct p256_pt q, a, b;

   p256_init();
   if (p256_pt_load(&q, pub) != 0)
      return -1;
   if (p256_scalar_load(rr, r) != 0 || p256_scalar_load(ss, s) != 0)
      return -1;

   /* e = leftmost 256 bits of the digest, reduced once. */
   bn_from_be(e, P256_K, digest, digest_len > 32 ? 32 : digest_len);
   if (bn_cmp(e, p256.n, P256_K) >= 0)
      bn_sub(e, e, p256.n, P256_K);

   /* w = s^-1 mod n: s^(n-2) in the Montgomery domain of n, then
    * back out, so the multiplies below run in the plain domain. */
   bn_mont_mul(w, ss, p256.r2n, p256.n, p256.n0n, P256_K, tmp);
   bn_zero(one, P256_K);
   one[0] = 1;
   bn_mont_mul(x, p256.r2n, one, p256.n, p256.n0n, P256_K, tmp);  /* R mod n */
   p256_fexp(w, w, p256.nm2, p256.n, p256.n0n, x);
   bn_mont_mul(w, w, one, p256.n, p256.n0n, P256_K, tmp);

   bn_mod_mul(u1, e,  w, p256.n, p256.r2n, p256.n0n, P256_K, tmp);
   bn_mod_mul(u2, rr, w, p256.n, p256.r2n, p256.n0n, P256_K, tmp);

   p256_pt_mul(&a, u1, &p256.g);
   p256_pt_mul(&b, u2, &q);
   p256_pt_add(&a, &a, &b);
   if (p256_pt_affine(x, NULL, &a) != 0)
      return -1;
   if (bn_cmp(x, p256.n, P256_K) >= 0)
      bn_sub(x, x, p256.n, P256_K);
   return bn_cmp(x, rr, P256_K) == 0 ? 0 : -1;
}

