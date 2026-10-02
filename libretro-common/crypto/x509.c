/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x509.c).
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <crypto/x509.h>
#include <crypto/pk.h>
#include <crypto/crypto.h>
#include <lrc_hash.h>
#include <encodings/base64.h>
#include <compat/strl.h>

/* ---- DER ---------------------------------------------------------- */

#define DER_INTEGER    0x02
#define DER_BITSTRING  0x03
#define DER_OCTET      0x04
#define DER_NULL       0x05
#define DER_OID        0x06
#define DER_UTF8       0x0c
#define DER_PRINTABLE  0x13
#define DER_IA5        0x16
#define DER_UTCTIME    0x17
#define DER_GENTIME    0x18
#define DER_SEQUENCE   0x30
#define DER_SET        0x31
#define DER_CTX(n)     (0xa0 | (n))

struct der
{
   const uint8_t *p;
   const uint8_t *end;
};

/* Reads one TLV at *d. On success *d advances past it, @val/@val_len
 * cover the contents. Definite lengths only, no indefinite forms. */
static int der_read(struct der *d, uint8_t tag,
      const uint8_t **val, size_t *val_len)
{
   const uint8_t *p = d->p;
   size_t len;

   if (p >= d->end || *p != tag)
      return -1;
   p++;
   if (p >= d->end)
      return -1;
   if (*p < 0x80)
      len = *p++;
   else
   {
      unsigned n = *p++ & 0x7f;
      if (n == 0 || n > 4 || p + n > d->end)
         return -1;
      len = 0;
      while (n--)
         len = (len << 8) | *p++;
      if (len < 0x80)
         return -1;  /* not minimal: not DER */
   }
   if (len > (size_t)(d->end - p))
      return -1;
   *val     = p;
   *val_len = len;
   d->p     = p + len;
   return 0;
}

static int der_peek(const struct der *d, uint8_t tag)
{
   return d->p < d->end && *d->p == tag;
}

/* Enters a constructed element: @inner spans its contents. */
static int der_enter(struct der *d, uint8_t tag, struct der *inner)
{
   const uint8_t *v;
   size_t vl;
   if (der_read(d, tag, &v, &vl) != 0)
      return -1;
   inner->p   = v;
   inner->end = v + vl;
   return 0;
}

/* Reads an INTEGER as an unsigned big-endian octet string with the
 * leading zero of a positive value stripped. */
static int der_read_uint(struct der *d, const uint8_t **val, size_t *val_len)
{
   if (der_read(d, DER_INTEGER, val, val_len) != 0 || *val_len == 0)
      return -1;
   if (**val & 0x80)
      return -1;   /* negative */
   while (*val_len > 1 && **val == 0)
   {
      (*val)++;
      (*val_len)--;
   }
   return 0;
}

static int der_oid_eq(const uint8_t *v, size_t vl, const uint8_t *oid, size_t ol)
{
   return vl == ol && memcmp(v, oid, ol) == 0;
}

/* OIDs, DER contents octets. */
static const uint8_t oid_rsa_enc[]        = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01};
static const uint8_t oid_rsa_sha256[]     = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0b};
static const uint8_t oid_rsa_sha384[]     = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0c};
static const uint8_t oid_rsa_sha512[]     = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0d};
static const uint8_t oid_ec_pub[]         = {0x2a,0x86,0x48,0xce,0x3d,0x02,0x01};
static const uint8_t oid_p256[]           = {0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07};
static const uint8_t oid_p384[]           = {0x2b,0x81,0x04,0x00,0x22};
static const uint8_t oid_ecdsa_sha256[]   = {0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x02};
static const uint8_t oid_ecdsa_sha384[]   = {0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x03};
static const uint8_t oid_ecdsa_sha512[]   = {0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x04};
static const uint8_t oid_cn[]             = {0x55,0x04,0x03};
static const uint8_t oid_basic_constr[]   = {0x55,0x1d,0x13};
static const uint8_t oid_key_usage[]      = {0x55,0x1d,0x0f};
static const uint8_t oid_ext_key_usage[]   = { 0x55, 0x1d, 0x25 };             /* 2.5.29.37 */
static const uint8_t oid_eku_server_auth[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01 }; /* 1.3.6.1.5.5.7.3.1 */
static const uint8_t oid_eku_any[]         = { 0x55, 0x1d, 0x25, 0x00 };       /* 2.5.29.37.0 */
static const uint8_t oid_subject_key_id[]  = { 0x55, 0x1d, 0x0e };             /* 2.5.29.14 */
static const uint8_t oid_authority_key_id[] = { 0x55, 0x1d, 0x23 };            /* 2.5.29.35 */
static const uint8_t oid_cert_policies[]   = { 0x55, 0x1d, 0x20 };             /* 2.5.29.32 */
static const uint8_t oid_crl_dist[]        = { 0x55, 0x1d, 0x1f };             /* 2.5.29.31 */
static const uint8_t oid_auth_info_access[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01 }; /* 1.3.6.1.5.5.7.1.1 */
static const uint8_t oid_ct_scts[]         = { 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x02 }; /* 1.3.6.1.4.1.11129.2.4.2 */
static const uint8_t oid_san[]            = {0x55,0x1d,0x11};

#define OID_EQ(v, vl, o) der_oid_eq(v, vl, o, sizeof(o))

static enum x509_sig_alg x509_sig_alg_from_oid(const uint8_t *v, size_t vl)
{
   if (OID_EQ(v, vl, oid_rsa_sha256))   return X509_SIG_RSA_SHA256;
   if (OID_EQ(v, vl, oid_rsa_sha384))   return X509_SIG_RSA_SHA384;
   if (OID_EQ(v, vl, oid_rsa_sha512))   return X509_SIG_RSA_SHA512;
   if (OID_EQ(v, vl, oid_ecdsa_sha256)) return X509_SIG_ECDSA_SHA256;
   if (OID_EQ(v, vl, oid_ecdsa_sha384)) return X509_SIG_ECDSA_SHA384;
   if (OID_EQ(v, vl, oid_ecdsa_sha512)) return X509_SIG_ECDSA_SHA512;
   return X509_SIG_NONE;
}

/* AlgorithmIdentifier ::= SEQUENCE { OID, parameters OPTIONAL } */
static int der_read_alg(struct der *d, const uint8_t **oid, size_t *oid_len,
      struct der *params)
{
   struct der a;
   if (der_enter(d, DER_SEQUENCE, &a) != 0)
      return -1;
   if (der_read(&a, DER_OID, oid, oid_len) != 0)
      return -1;
   *params = a;
   return 0;
}

/* ---- time --------------------------------------------------------- */

static int x509_digits(const uint8_t *p, unsigned n, unsigned *out)
{
   unsigned v = 0;
   while (n--)
   {
      if (*p < '0' || *p > '9')
         return -1;
      v = v * 10 + (unsigned)(*p++ - '0');
   }
   *out = v;
   return 0;
}

/* Days since 1970-01-01 for a proleptic Gregorian date. */
static long x509_days_from_civil(long y, unsigned m, unsigned d)
{
   long era, yoe, doy, doe;
   y  -= m <= 2;
   era = (y >= 0 ? y : y - 399) / 400;
   yoe = y - era * 400;
   doy = (153 * (long)(m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
   doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
   return era * 146097 + doe - 719468;
}

static int der_read_time(struct der *d, time_t *t)
{
   const uint8_t *v;
   size_t   vl;
   unsigned y, mo, da, h, mi, s;
   int      is_gen;

   if (der_peek(d, DER_UTCTIME))
   {
      if (der_read(d, DER_UTCTIME, &v, &vl) != 0 || vl != 13)
         return -1;
      is_gen = 0;
   }
   else
   {
      if (der_read(d, DER_GENTIME, &v, &vl) != 0 || vl != 15)
         return -1;
      is_gen = 1;
   }
   if (v[vl - 1] != 'Z')
      return -1;
   if (is_gen)
   {
      if (x509_digits(v, 4, &y) != 0)
         return -1;
      v += 4;
   }
   else
   {
      if (x509_digits(v, 2, &y) != 0)
         return -1;
      y += (y < 50) ? 2000 : 1900;
      v += 2;
   }
   if (x509_digits(v, 2, &mo) || x509_digits(v + 2, 2, &da)
         || x509_digits(v + 4, 2, &h) || x509_digits(v + 6, 2, &mi)
         || x509_digits(v + 8, 2, &s))
      return -1;
   if (mo < 1 || mo > 12 || da < 1 || da > 31 || h > 23 || mi > 59 || s > 60)
      return -1;
   {
      /* long is 32 bits on Windows: past 2038-01-19 the seconds no
       * longer fit it, so widen first, and where time_t itself is 32
       * bits saturate rather than wrap a far-future notAfter into the
       * past (a leaf good until 2046 must not read as expired). */
      int64_t secs = (int64_t)x509_days_from_civil((long)y, mo, da) * 86400
            + (int64_t)h * 3600 + (int64_t)mi * 60 + (int64_t)s;
      if (sizeof(time_t) < 8)
      {
         if (secs > 0x7fffffff)
            secs = 0x7fffffff;
         else if (secs < -0x7fffffff - 1)
            secs = -0x7fffffff - 1;
      }
      *t = (time_t)secs;
   }
   return 0;
}

/* ---- certificate -------------------------------------------------- */

static int x509_parse_spki(struct x509_cert *c, struct der *d)
{
   struct der spki, params;
   const uint8_t *oid, *v;
   size_t oid_len, vl;

   if (der_enter(d, DER_SEQUENCE, &spki) != 0)
      return -1;
   if (der_read_alg(&spki, &oid, &oid_len, &params) != 0)
      return -1;
   if (der_read(&spki, DER_BITSTRING, &v, &vl) != 0 || vl < 2 || v[0] != 0)
      return -1;
   v++;
   vl--;

   if (OID_EQ(oid, oid_len, oid_rsa_enc))
   {
      struct der k;
      struct der kd;
      kd.p   = v;
      kd.end = v + vl;
      if (der_enter(&kd, DER_SEQUENCE, &k) != 0)
         return -1;
      if (der_read_uint(&k, &c->rsa_n, &c->rsa_n_len) != 0)
         return -1;
      if (der_read_uint(&k, &c->rsa_e, &c->rsa_e_len) != 0)
         return -1;
      if (c->rsa_n_len >= 64 && c->rsa_n_len <= 512)
         c->key_type = X509_KEY_RSA;
   }
   else if (OID_EQ(oid, oid_len, oid_ec_pub))
   {
      const uint8_t *curve;
      size_t curve_len;
      if (der_read(&params, DER_OID, &curve, &curve_len) != 0)
         return -1;
      if (OID_EQ(curve, curve_len, oid_p256) && vl == 65 && v[0] == 0x04)
      {
         c->ec_point = v;
         c->key_type = X509_KEY_P256;
      }
      else if (OID_EQ(curve, curve_len, oid_p384) && vl == 97 && v[0] == 0x04)
      {
         c->ec_point = v;
         c->key_type = X509_KEY_P384;
      }
   }
   return 0;
}

/* Finds the commonName in a Name: SEQUENCE OF SET OF SEQUENCE { OID, value }. */
static void x509_find_cn(struct x509_cert *c)
{
   struct der name, rdn, atv;
   name.p   = c->subject;
   name.end = c->subject + c->subject_len;
   /* subject points at the whole SEQUENCE TLV; step inside it. */
   if (der_enter(&name, DER_SEQUENCE, &name) != 0)
      return;
   while (name.p < name.end)
   {
      if (der_enter(&name, DER_SET, &rdn) != 0)
         return;
      while (rdn.p < rdn.end)
      {
         const uint8_t *oid, *v;
         size_t oid_len, vl;
         if (der_enter(&rdn, DER_SEQUENCE, &atv) != 0)
            return;
         if (der_read(&atv, DER_OID, &oid, &oid_len) != 0)
            return;
         if (OID_EQ(oid, oid_len, oid_cn) && atv.p < atv.end)
         {
            uint8_t tag = *atv.p;
            if ((tag == DER_UTF8 || tag == DER_PRINTABLE || tag == DER_IA5)
                  && der_read(&atv, tag, &v, &vl) == 0)
            {
               c->cn     = v;
               c->cn_len = vl;
            }
            return;
         }
      }
   }
}

static int x509_parse_extensions(struct x509_cert *c, struct der *d)
{
   struct der exts, ext;
   if (der_enter(d, DER_SEQUENCE, &exts) != 0)
      return -1;
   while (exts.p < exts.end)
   {
      const uint8_t *oid, *v;
      size_t oid_len, vl;
      struct der val;
      int critical = 0;
      if (der_enter(&exts, DER_SEQUENCE, &ext) != 0)
         return -1;
      if (der_read(&ext, DER_OID, &oid, &oid_len) != 0)
         return -1;
      if (der_peek(&ext, 0x01))
      {
         if (der_read(&ext, 0x01, &v, &vl) != 0 || vl != 1)   /* critical BOOLEAN */
            return -1;
         critical = v[0] != 0;
      }
      if (der_read(&ext, DER_OCTET, &v, &vl) != 0)
         return -1;
      val.p   = v;
      val.end = v + vl;

      /* An extension marked critical that this verifier does not
       * understand makes the certificate unusable (RFC 5280 4.2):
       * the ones it acts on are handled below, a few others are
       * known to be safe to skip, everything else is refused. */
      if (critical
            && !OID_EQ(oid, oid_len, oid_basic_constr)
            && !OID_EQ(oid, oid_len, oid_key_usage)
            && !OID_EQ(oid, oid_len, oid_san)
            && !OID_EQ(oid, oid_len, oid_ext_key_usage))
         return -1;

      if (OID_EQ(oid, oid_len, oid_basic_constr))
      {
         struct der bc;
         if (der_enter(&val, DER_SEQUENCE, &bc) != 0)
            return -1;
         c->is_ca = 0;
         if (der_peek(&bc, 0x01))
         {
            const uint8_t *b;
            size_t bl;
            if (der_read(&bc, 0x01, &b, &bl) != 0 || bl != 1)
               return -1;
            c->is_ca = b[0] ? 1 : 0;
         }
         if (der_peek(&bc, DER_INTEGER))
         {
            const uint8_t *pl;
            size_t pll;
            if (der_read_uint(&bc, &pl, &pll) != 0 || pll > 2)
               return -1;
            c->path_len = (int)(pll == 2 ? (pl[0] << 8) | pl[1] : pl[0]);
         }
      }
      else if (OID_EQ(oid, oid_len, oid_key_usage))
      {
         const uint8_t *b;
         size_t bl;
         if (der_read(&val, DER_BITSTRING, &b, &bl) != 0 || bl < 2)
            return -1;
         c->key_usage = b[1];
      }
      else if (OID_EQ(oid, oid_len, oid_san))
      {
         const uint8_t *s;
         size_t sl;
         if (der_read(&val, DER_SEQUENCE, &s, &sl) != 0)
            return -1;
         c->san     = s;
         c->san_len = sl;
      }
      else if (OID_EQ(oid, oid_len, oid_ext_key_usage))
      {
         /* ExtKeyUsageSyntax: a sequence of OIDs; note whether
          * serverAuth (or anyExtendedKeyUsage) is among them */
         struct der eku;
         if (der_enter(&val, DER_SEQUENCE, &eku) != 0)
            return -1;
         c->eku = X509_EKU_OTHER;
         while (eku.p < eku.end)
         {
            const uint8_t *eo;
            size_t el;
            if (der_read(&eku, DER_OID, &eo, &el) != 0)
               return -1;
            if (OID_EQ(eo, el, oid_eku_server_auth) || OID_EQ(eo, el, oid_eku_any))
               c->eku = X509_EKU_SERVER_AUTH;
         }
      }
      else
      {
         /* known, not acted on: subject / authority key id, policies,
          * CRL points, AIA, SCTs and any other non-critical one */
         (void)oid_subject_key_id; (void)oid_authority_key_id; (void)oid_cert_policies;
         (void)oid_crl_dist; (void)oid_auth_info_access; (void)oid_ct_scts;
      }
   }
   return 0;
}

int x509_parse(struct x509_cert *c, const uint8_t *der, size_t len)
{
   struct der d, cert, tbs, params;
   const uint8_t *v, *oid, *sig_oid;
   size_t vl, oid_len, sig_oid_len;
   const uint8_t *tbs_start;

   memset(c, 0, sizeof(*c));
   c->is_ca     = -1;
   c->path_len  = -1;
   c->key_usage = -1;
   c->eku       = X509_EKU_ABSENT;
   c->der       = der;
   c->der_len   = len;

   d.p   = der;
   d.end = der + len;
   if (der_enter(&d, DER_SEQUENCE, &cert) != 0 || d.p != d.end)
      return -1;

   tbs_start = cert.p;
   if (der_enter(&cert, DER_SEQUENCE, &tbs) != 0)
      return -1;
   c->tbs     = tbs_start;
   c->tbs_len = (size_t)(cert.p - tbs_start);

   /* version [0] EXPLICIT INTEGER, default v1 */
   if (der_peek(&tbs, DER_CTX(0)))
   {
      struct der ver;
      if (der_enter(&tbs, DER_CTX(0), &ver) != 0
            || der_read(&ver, DER_INTEGER, &v, &vl) != 0)
         return -1;
   }
   if (der_read(&tbs, DER_INTEGER, &v, &vl) != 0)     /* serial */
      return -1;
   if (der_read_alg(&tbs, &oid, &oid_len, &params) != 0)
      return -1;

   c->issuer = tbs.p;
   if (der_read(&tbs, DER_SEQUENCE, &v, &vl) != 0)
      return -1;
   c->issuer_len = (size_t)(tbs.p - c->issuer);

   {
      struct der validity;
      if (der_enter(&tbs, DER_SEQUENCE, &validity) != 0)
         return -1;
      if (der_read_time(&validity, &c->not_before) != 0
            || der_read_time(&validity, &c->not_after) != 0)
         return -1;
   }

   c->subject = tbs.p;
   if (der_read(&tbs, DER_SEQUENCE, &v, &vl) != 0)
      return -1;
   c->subject_len = (size_t)(tbs.p - c->subject);
   x509_find_cn(c);

   if (x509_parse_spki(c, &tbs) != 0)
      return -1;

   /* issuerUniqueID [1], subjectUniqueID [2], extensions [3] */
   if (der_peek(&tbs, 0x81) && der_read(&tbs, 0x81, &v, &vl) != 0)
      return -1;
   if (der_peek(&tbs, 0x82) && der_read(&tbs, 0x82, &v, &vl) != 0)
      return -1;
   if (der_peek(&tbs, DER_CTX(3)))
   {
      struct der e;
      if (der_enter(&tbs, DER_CTX(3), &e) != 0
            || x509_parse_extensions(c, &e) != 0)
         return -1;
   }

   /* signatureAlgorithm must repeat the one inside the TBS */
   if (der_read_alg(&cert, &sig_oid, &sig_oid_len, &params) != 0)
      return -1;
   if (sig_oid_len != oid_len || memcmp(sig_oid, oid, oid_len) != 0)
      return -1;
   c->sig_alg = x509_sig_alg_from_oid(sig_oid, sig_oid_len);

   if (der_read(&cert, DER_BITSTRING, &v, &vl) != 0 || vl < 2 || v[0] != 0)
      return -1;
   c->sig     = v + 1;
   c->sig_len = vl - 1;
   return cert.p == cert.end ? 0 : -1;
}

/* ---- signatures --------------------------------------------------- */

static size_t x509_digest(enum x509_sig_alg alg,
      const uint8_t *data, size_t len, uint8_t *out)
{
   switch (alg)
   {
      case X509_SIG_RSA_SHA256:
      case X509_SIG_ECDSA_SHA256:
      {
         struct sha256_state s;
         sha256_stream_init(&s, 0);
         sha256_stream_update(&s, data, len);
         sha256_stream_final(&s, out);
         return 32;
      }
      case X509_SIG_RSA_SHA384:
      case X509_SIG_ECDSA_SHA384:
      {
         struct sha512_state s;
         sha512_stream_init(&s, 1);
         sha512_stream_update(&s, data, len);
         sha512_stream_final(&s, out);
         return 48;
      }
      case X509_SIG_RSA_SHA512:
      case X509_SIG_ECDSA_SHA512:
      {
         struct sha512_state s;
         sha512_stream_init(&s, 0);
         sha512_stream_update(&s, data, len);
         sha512_stream_final(&s, out);
         return 64;
      }
      default:
         break;
   }
   return 0;
}

int x509_verify_ecdsa_digest(const struct x509_cert *key,
      const uint8_t *digest, size_t digest_len,
      const uint8_t *sig, size_t sig_len)
{
   /* ECDSA-Sig-Value ::= SEQUENCE { r INTEGER, s INTEGER } */
   struct der d, sv;
   const uint8_t *r, *s;
   size_t rl, sl, n;
   uint8_t rb[48], sb[48];
   if (key->key_type == X509_KEY_P256)
      n = 32;
   else if (key->key_type == X509_KEY_P384)
      n = 48;
   else
      return -1;
   d.p   = sig;
   d.end = sig + sig_len;
   if (der_enter(&d, DER_SEQUENCE, &sv) != 0
         || der_read_uint(&sv, &r, &rl) != 0
         || der_read_uint(&sv, &s, &sl) != 0
         || rl > n || sl > n || sv.p != sv.end)
      return -1;
   memset(rb, 0, n);
   memset(sb, 0, n);
   memcpy(rb + n - rl, r, rl);
   memcpy(sb + n - sl, s, sl);
   if (n == 32)
      return p256_ecdsa_verify(key->ec_point, digest, digest_len, rb, sb);
   return p384_ecdsa_verify(key->ec_point, digest, digest_len, rb, sb);
}

int x509_verify_signature(const struct x509_cert *c,
      const struct x509_cert *issuer)
{
   uint8_t digest[64];
   size_t  dlen = x509_digest(c->sig_alg, c->tbs, c->tbs_len, digest);

   if (!dlen)
      return -1;

   switch (c->sig_alg)
   {
      case X509_SIG_RSA_SHA256:
      case X509_SIG_RSA_SHA384:
      case X509_SIG_RSA_SHA512:
      {
         enum rsa_hash h = (dlen == 32) ? RSA_HASH_SHA256
                         : (dlen == 48) ? RSA_HASH_SHA384 : RSA_HASH_SHA512;
         if (issuer->key_type != X509_KEY_RSA)
            return -1;
         return rsa_pkcs1_verify(issuer->rsa_n, issuer->rsa_n_len,
               issuer->rsa_e, issuer->rsa_e_len, h, digest, dlen,
               c->sig, c->sig_len);
      }
      case X509_SIG_ECDSA_SHA256:
      case X509_SIG_ECDSA_SHA384:
      case X509_SIG_ECDSA_SHA512:
         return x509_verify_ecdsa_digest(issuer, digest, dlen, c->sig, c->sig_len);
      default:
         break;
   }
   return -1;
}

/* ---- hostname ----------------------------------------------------- */

static int x509_name_match(const uint8_t *pat, size_t pl, const char *host)
{
   size_t hl = strlen(host);
   size_t i;

   if (pl >= 2 && pat[0] == '*' && pat[1] == '.')
   {
      /* wildcard: the whole leftmost label, at least one more label */
      const char *dot = strchr(host, '.');
      if (!dot || dot == host)
         return -1;
      pat += 1;
      pl  -= 1;
      host = dot;
      hl   = strlen(host);
      if (!strchr(host + 1, '.'))
         return -1;   /* *.com */
   }
   if (pl != hl)
      return -1;
   for (i = 0; i < pl; i++)
   {
      int a = pat[i], b = (unsigned char)host[i];
      if (a >= 'A' && a <= 'Z') a += 32;
      if (b >= 'A' && b <= 'Z') b += 32;
      if (a != b)
         return -1;
   }
   return 0;
}

static int x509_hexval(int c)
{
   if (c >= '0' && c <= '9') return c - '0';
   if (c >= 'a' && c <= 'f') return c - 'a' + 10;
   if (c >= 'A' && c <= 'F') return c - 'A' + 10;
   return -1;
}

/* Dotted IPv4 at @s up to @end into 4 bytes. */
static int x509_parse_ipv4(const char *s, const char *end, uint8_t *out)
{
   int i;
   for (i = 0; i < 4; i++)
   {
      unsigned v = 0;
      int      digits = 0;
      while (s < end && *s >= '0' && *s <= '9')
      {
         if (digits && v == 0)
            return -1;            /* no leading zeros: not octal, not ambiguous */
         v = v * 10 + (unsigned)(*s++ - '0');
         if (++digits > 3 || v > 255)
            return -1;
      }
      if (!digits)
         return -1;
      out[i] = (uint8_t)v;
      if (i < 3)
      {
         if (s >= end || *s != '.')
            return -1;
         s++;
      }
   }
   return s == end ? 0 : -1;
}

int x509_parse_ip(const char *host, uint8_t out[16])
{
   const char *end;
   uint8_t     a[16];
   int         n = 0, gap = -1;
   const char *s;

   if (!host || !*host)
      return 0;
   if (!strchr(host, ':'))
      return x509_parse_ipv4(host, host + strlen(host), out) == 0 ? 4 : 0;

   /* IPv6, with or without brackets; a zone ("%eth0") is not part of
    * the address. */
   s   = host + (*host == '[');
   end = s + strcspn(s, "]%");
   if (*host == '[' && !strchr(s, ']'))
      return 0;
   if (end - s >= 2 && s[0] == ':' && s[1] == ':')
   {
      gap = 0;
      s  += 2;
   }
   else if (*s == ':')
      return 0;
   while (s < end)
   {
      unsigned v = 0;
      int      digits = 0;
      const char *start = s;
      while (s < end && x509_hexval(*s) >= 0)
      {
         v = (v << 4) | (unsigned)x509_hexval(*s++);
         if (++digits > 4)
            return 0;
      }
      if (s < end && *s == '.')
      {
         /* trailing dotted IPv4: the last 32 bits */
         if (n > 12 || x509_parse_ipv4(start, end, a + n) != 0)
            return 0;
         n += 4;
         s  = end;
         break;
      }
      if (!digits || n > 14)
         return 0;
      a[n++] = (uint8_t)(v >> 8);
      a[n++] = (uint8_t)v;
      if (s == end)
         break;
      if (*s++ != ':')
         return 0;
      if (s < end && *s == ':')
      {
         if (gap >= 0)
            return 0;
         gap = n;
         s++;
      }
      else if (s == end)
         return 0;                /* trailing single colon */
   }
   if (gap >= 0)
   {
      int fill = 16 - n;
      if (fill < 2)
         return 0;
      memmove(a + gap + fill, a + gap, (size_t)(n - gap));
      memset(a + gap, 0, (size_t)fill);
      n = 16;
   }
   if (n != 16)
      return 0;
   memcpy(out, a, 16);
   return 16;
}

int x509_match_hostname(const struct x509_cert *c, const char *host)
{
   uint8_t ip[16];
   int     iplen;

   if (!host || !*host)
      return -1;

   /* An IP address matches only an iPAddress entry, byte for byte; never
    * a dNSName or the CN, which would let a name certificate whose CN
    * happens to read "10.0.0.1" vouch for that address (RFC 6125 6.2.1,
    * RFC 9525 6.3). */
   if ((iplen = x509_parse_ip(host, ip)) != 0)
   {
      struct der san;
      if (!c->san)
         return -1;
      san.p   = c->san;
      san.end = c->san + c->san_len;
      while (san.p < san.end)
      {
         const uint8_t *v;
         size_t vl;
         uint8_t tag = *san.p;
         if (der_read(&san, tag, &v, &vl) != 0)
            return -1;
         if (tag == 0x87 && vl == (size_t)iplen && !memcmp(v, ip, vl))
            return 0;
      }
      return -1;
   }

   if (c->san)
   {
      struct der san;
      san.p   = c->san;
      san.end = c->san + c->san_len;
      while (san.p < san.end)
      {
         const uint8_t *v;
         size_t vl;
         uint8_t tag = *san.p;
         if (der_read(&san, tag, &v, &vl) != 0)
            return -1;
         if (tag == 0x82 && x509_name_match(v, vl, host) == 0)   /* dNSName */
            return 0;
      }
      return -1;
   }
   if (c->cn && x509_name_match(c->cn, c->cn_len, host) == 0)
      return 0;
   return -1;
}

/* ---- trust store -------------------------------------------------- */

struct x509_anchor
{
   uint8_t *der;
   size_t   der_len;
   struct x509_cert cert;
};

/* The anchors loaded from one source. A store is never changed once
 * published: a reload builds a new one and swaps it in, and the old one
 * is freed by whoever drops the last reference to it, so a chain walk
 * runs over its own reference with no lock held. */
struct x509_store
{
   struct x509_anchor *anchors;
   unsigned count;
   unsigned cap;
   const char *src;       /* the source it was loaded from */
   size_t src_len;
   unsigned refs;         /* under the lock: the published slot's + walks' */
};

static struct x509_store *x509_store_cur = NULL;

/* The store is process-wide and TLS handshakes run on several threads
 * (updater, achievements, cloud sync), the first of them building it.
 * A store is built with no lock held; a flag guards only swapping one
 * in and taking or dropping a reference, a few loads and stores, so a
 * reload never frees a store a walk is still reading and there is no
 * lock object to create on first use. */
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
static retro_atomic_int_t x509_store_busy;
static void x509_store_lock(void)
{
   while (!retro_atomic_cas_int(&x509_store_busy, 0, 1))
      sthread_yield();
}
#define X509_LOCK()   x509_store_lock()
#define X509_UNLOCK() retro_atomic_store_release_int(&x509_store_busy, 0)
#else
#define X509_LOCK()   do { } while (0)
#define X509_UNLOCK() do { } while (0)
#endif

static void x509_store_free(struct x509_store *st)
{
   unsigned i;
   if (!st)
      return;
   for (i = 0; i < st->count; i++)
      free(st->anchors[i].der);
   free(st->anchors);
   free(st);
}

/* Under the lock. Nonzero when that was the last reference. */
static int x509_store_unref_locked(struct x509_store *st)
{
   return st && --st->refs == 0;
}

/* Under the lock: @st (NULL: none) replaces the published store. The
 * old one comes back to be freed outside the lock, or NULL while a walk
 * still holds it. */
static struct x509_store *x509_store_publish_locked(struct x509_store *st)
{
   struct x509_store *old = x509_store_cur;
   if (st)
      st->refs = 1;
   x509_store_cur = st;
   return x509_store_unref_locked(old) ? old : NULL;
}

void x509_trust_free(void)
{
   struct x509_store *old;
   X509_LOCK();
   old = x509_store_publish_locked(NULL);
   X509_UNLOCK();
   x509_store_free(old);
}

/* Every certificate in @pem added to @st's anchors. */
static int x509_store_append_pem(struct x509_store *st, const char *pem, size_t len)
{
   static const char begin[] = "-----BEGIN CERTIFICATE-----";
   static const char end_[]  = "-----END CERTIFICATE-----";
   const char *p   = pem;
   const char *e   = pem + len;

   for (;;)
   {
      const char *b, *stop, *q;
      char   *clean;
      size_t  n;
      int     der_len = 0;
      uint8_t *der;
      struct x509_anchor *a;

      b = NULL;
      for (q = p; q + sizeof(begin) - 1 <= e; q++)
         if (memcmp(q, begin, sizeof(begin) - 1) == 0)
         {
            b = q + sizeof(begin) - 1;
            break;
         }
      if (!b)
         break;
      stop = NULL;
      for (q = b; q + sizeof(end_) - 1 <= e; q++)
         if (memcmp(q, end_, sizeof(end_) - 1) == 0)
         {
            stop = q;
            break;
         }
      if (!stop)
         break;
      p = stop + sizeof(end_) - 1;

      /* base64 body without the line breaks */
      if (!(clean = (char*)malloc((size_t)(stop - b) + 1)))
         return -1;
      n = 0;
      for (q = b; q < stop; q++)
         if (*q != '\n' && *q != '\r' && *q != ' ' && *q != '\t')
            clean[n++] = *q;
      clean[n] = '\0';
      der = unbase64(clean, (int)n, &der_len);
      free(clean);
      if (!der || der_len <= 0)
      {
         free(der);
         continue;
      }

      if (st->count == st->cap)
      {
         struct x509_anchor *grown;
         unsigned cap = st->cap ? st->cap * 2 : 64;
         grown = (struct x509_anchor*)realloc(st->anchors, cap * sizeof(*grown));
         if (!grown)
         {
            free(der);
            return -1;
         }
         st->anchors = grown;
         st->cap     = cap;
      }
      a          = &st->anchors[st->count];
      a->der     = der;
      a->der_len = (size_t)der_len;
      if (x509_parse(&a->cert, der, (size_t)der_len) != 0
            || a->cert.key_type == X509_KEY_NONE)
      {
         /* Unparseable, or a key this code cannot use (P-384 roots):
          * not an anchor here. */
         free(der);
         continue;
      }
      st->count++;
   }
   return (int)st->count;
}

/* The anchors in @parts, built into a new published store unless the
 * one published was loaded from the same source already - the path
 * every handshake after the first takes. Two first uses at once both
 * build; the second to finish keeps the first's. A single PEM is a
 * source of one part; @src and @src_len name the source. */
static int x509_store_is(const struct x509_store *st, const char *src,
      size_t src_len)
{
   return st && st->src == src && st->src_len == src_len && st->anchors;
}

static int x509_trust_load(const char *const *parts, const size_t *lens,
      unsigned count, const char *src, size_t src_len)
{
   struct x509_store *st, *old;
   unsigned i;
   int      n = 0;

   X509_LOCK();
   if (x509_store_is(x509_store_cur, src, src_len))
      n = (int)x509_store_cur->count;
   X509_UNLOCK();
   if (n)
      return n;

   if (!(st = (struct x509_store*)calloc(1, sizeof(*st))))
      n = -1;
   for (i = 0; i < count && n >= 0; i++)
      n = x509_store_append_pem(st, parts[i], lens[i]);

   X509_LOCK();
   if (n >= 0 && x509_store_is(x509_store_cur, src, src_len))
   {
      n   = (int)x509_store_cur->count;
      old = NULL;   /* another caller's build won; ours goes */
   }
   else if (n >= 0)
   {
      st->src     = src;
      st->src_len = src_len;
      old         = x509_store_publish_locked(st);
      st          = NULL;
   }
   else
      old         = x509_store_publish_locked(NULL);
   X509_UNLOCK();
   x509_store_free(old);
   x509_store_free(st);
   return n;
}

int x509_trust_load_pem(const char *pem, size_t len)
{
   return x509_trust_load(&pem, &len, 1, pem, len);
}

int x509_trust_load_pem_parts(const char *const *parts, const size_t *lens,
      unsigned count)
{
   /* the part table is the source: loaded once, as a single one is */
   return x509_trust_load(parts, lens, count, (const char*)parts, count);
}

/* ---- chain -------------------------------------------------------- */

static void x509_info(char *info, size_t info_len, const char *msg)
{
   if (info && info_len)
      strlcpy(info, msg, info_len);
}

static int x509_name_eq(const struct x509_cert *a, const struct x509_cert *b)
{
   return a->subject_len == b->issuer_len
      && memcmp(a->subject, b->issuer, a->subject_len) == 0;
}

int x509_verify_chain(const uint8_t **ders, const size_t *lens, unsigned n,
      const char *host, time_t now, char *info, size_t info_len)
{
   /* Parsed chain on the heap: one x509_cert is ~200 bytes and a
    * server may send a dozen. */
   struct x509_cert *certs;
   const struct x509_cert *cur;
   struct x509_store *st = NULL;
   unsigned i, depth;
   unsigned used = 0;   /* bitmask of chain certs already on the path */
   int ret = -1;

   if (!n || n > 16)
   {
      x509_info(info, info_len, "no certificate");
      return -1;
   }
   if (!(certs = (struct x509_cert*)malloc(n * sizeof(*certs))))
      return -1;

   for (i = 0; i < n; i++)
      if (x509_parse(&certs[i], ders[i], lens[i]) != 0)
      {
         x509_info(info, info_len, "malformed certificate");
         goto done;
      }

   if (host && x509_match_hostname(&certs[0], host) != 0)
   {
      x509_info(info, info_len, "hostname mismatch");
      goto done;
   }
   /* Leaf policy: an extendedKeyUsage without serverAuth (a client
    * or code-signing certificate) does not authenticate a server; a
    * keyUsage without digitalSignature cannot sign the handshake,
    * which is the only way an ECDHE suite uses the key. A CA
    * certificate is not a server certificate either. */
   if (certs[0].eku == X509_EKU_OTHER)
   {
      x509_info(info, info_len, "certificate not for server authentication");
      goto done;
   }
   if (certs[0].key_usage >= 0 && !(certs[0].key_usage & X509_KU_DIGITAL_SIGNATURE))
   {
      x509_info(info, info_len, "certificate key cannot sign");
      goto done;
   }
   if (certs[0].is_ca == 1 && n > 1)
   {
      x509_info(info, info_len, "server certificate is a CA");
      goto done;
   }

   /* The walk reads the store through its own reference: a reload
    * meanwhile swaps in a new one and leaves this one to the walk. */
   X509_LOCK();
   if ((st = x509_store_cur))
      st->refs++;
   X509_UNLOCK();

   cur  = &certs[0];
   used = 1;
   for (depth = 0; depth < 16; depth++)
   {
      const struct x509_cert *issuer = NULL;
      unsigned j;

      if (now < cur->not_before || now > cur->not_after)
      {
         x509_info(info, info_len, "certificate expired or not yet valid");
         goto done;
      }
      if (depth > 0)
      {
         if (cur->is_ca != 1)
         {
            x509_info(info, info_len, "intermediate is not a CA");
            goto done;
         }
         if (cur->key_usage >= 0 && !(cur->key_usage & X509_KU_KEY_CERT_SIGN))
         {
            x509_info(info, info_len, "intermediate cannot sign certificates");
            goto done;
         }
         if (cur->path_len >= 0 && (int)(depth - 1) > cur->path_len)
         {
            x509_info(info, info_len, "path length constraint violated");
            goto done;
         }
      }

      /* A trust anchor issuing this one ends the walk. */
      for (j = 0; st && j < st->count; j++)
      {
         const struct x509_cert *a = &st->anchors[j].cert;
         if (x509_name_eq(a, cur) && x509_verify_signature(cur, a) == 0)
         {
            ret = 0;
            goto done;
         }
      }

      /* Otherwise the next certificate in the chain that names itself
       * as issuer and whose signature checks out. */
      for (j = 1; j < n; j++)
      {
         if (used & (1u << j))
            continue;
         if (x509_name_eq(&certs[j], cur) && x509_verify_signature(cur, &certs[j]) == 0)
         {
            issuer = &certs[j];
            used  |= 1u << j;
            break;
         }
      }
      if (!issuer)
      {
         x509_info(info, info_len, (st && st->count)
               ? "issuer not found or signature invalid"
               : "no trust anchors loaded");
         goto done;
      }
      cur = issuer;
   }
   x509_info(info, info_len, "chain too long");

done:
   if (st)
   {
      int last;
      X509_LOCK();
      last = x509_store_unref_locked(st);
      X509_UNLOCK();
      if (last)
         x509_store_free(st);
   }
   free(certs);
   return ret;
}
