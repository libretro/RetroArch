/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_crypto.c).
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

#ifdef CHECK_SHIM
#include "../check_shim.h"
#else
#include <check.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <crypto/pk.h>
#include <crypto/x509.h>
#include "../../net/cacert.h"
#include "test_x509_vectors.h"
#include <lrc_hash.h>

#define SUITE_NAME "crypto"

/* Known-answer tests. AES: FIPS 197 appendix C; CMAC: RFC 4493;
 * GCM: the GCM spec test cases 13/14/16/17/18 plus test case 2;
 * ChaCha20 / Poly1305 / AEAD: RFC 8439 sections 2.4.2, 2.5.2, 2.8.2;
 * HMAC: RFC 4231 case 2; HKDF: RFC 5869 case 1; PBKDF2: the
 * commonly published SHA-256 vectors for "password"/"salt". */

static size_t unhex(uint8_t *out, size_t out_len, const char *hex)
{
   size_t n = 0;
   while (hex[0] && hex[1] && n < out_len)
   {
      unsigned v;
      char tmp[3];
      tmp[0] = hex[0]; tmp[1] = hex[1]; tmp[2] = '\0';
      sscanf(tmp, "%x", &v);
      out[n++] = (uint8_t)v;
      hex += 2;
   }
   return n;
}

static void ck_hex(const uint8_t *got, size_t len, const char *hex)
{
   uint8_t want[256];
   size_t wl = unhex(want, sizeof(want), hex);
   ck_assert_uint_eq(wl, len);
   ck_assert(memcmp(got, want, len) == 0);
}

static const char *aes_pt   = "00112233445566778899aabbccddeeff";
static const char *cmac_key = "2b7e151628aed2a6abf7158809cf4f3c";
static const char *cmac_msg =
   "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
   "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710";
static const char *gcm_key  =
   "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
static const char *gcm_pt   =
   "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
   "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
static const char *gcm_aad  = "feedfacedeadbeeffeedfacedeadbeefabaddad2";
static const char *chacha_pt =
   "Ladies and Gentlemen of the class of '99: If I could offer you "
   "only one tip for the future, sunscreen would be it.";

START_TEST (test_aes_block)
{
   struct aes_ctx ctx;
   uint8_t key[32], pt[16], ct[16];
   unsigned i;
   for (i = 0; i < 32; i++)
      key[i] = (uint8_t)i;
   unhex(pt, 16, aes_pt);

   ck_assert_int_eq(aes_init(&ctx, key, 16), 0);
   aes_encrypt_block(&ctx, pt, ct);
   ck_hex(ct, 16, "69c4e0d86a7b0430d8cdb78070b4c55a");

   ck_assert_int_eq(aes_init(&ctx, key, 24), 0);
   aes_encrypt_block(&ctx, pt, ct);
   ck_hex(ct, 16, "dda97ca4864cdfe06eaf70a0ec0d7191");

   ck_assert_int_eq(aes_init(&ctx, key, 32), 0);
   aes_encrypt_block(&ctx, pt, ct);
   ck_hex(ct, 16, "8ea2b7ca516745bfeafc49904b496089");

   /* in-place */
   memcpy(ct, pt, 16);
   aes_encrypt_block(&ctx, ct, ct);
   ck_hex(ct, 16, "8ea2b7ca516745bfeafc49904b496089");

   ck_assert_int_eq(aes_init(&ctx, key, 20), -1);
}
END_TEST

START_TEST (test_aes_cmac)
{
   struct aes_ctx ctx;
   uint8_t key[16], msg[64], mac[16];
   unhex(key, 16, cmac_key);
   unhex(msg, 64, cmac_msg);
   ck_assert_int_eq(aes_init(&ctx, key, 16), 0);
   aes_cmac(&ctx, msg,  0, mac); ck_hex(mac, 16, "bb1d6929e95937287fa37d129b756746");
   aes_cmac(&ctx, msg, 16, mac); ck_hex(mac, 16, "070a16b46b4d4144f79bdd9dd04a287c");
   aes_cmac(&ctx, msg, 40, mac); ck_hex(mac, 16, "dfa66747de9ae63030ca32611497c827");
   aes_cmac(&ctx, msg, 64, mac); ck_hex(mac, 16, "51f0bebf7e3b9d92fc49741779363cfe");
}
END_TEST

static void gcm_case(const char *iv_hex, const char *want_hex)
{
   struct aes_gcm_ctx g;
   uint8_t key[32], iv[64], pt[60], aad[20], ct[60], tag[16], dec[60];
   size_t ivl = unhex(iv, sizeof(iv), iv_hex);
   unhex(key, 32, gcm_key);
   unhex(pt, 60, gcm_pt);
   unhex(aad, 20, gcm_aad);
   ck_assert_int_eq(aes_gcm_init(&g, key, 32), 0);
   ck_assert_int_eq(aes_gcm_encrypt(&g, iv, ivl, aad, 20, pt, 60, ct, tag), 0);
   {
      uint8_t both[76];
      memcpy(both, ct, 60);
      memcpy(both + 60, tag, 16);
      ck_hex(both, 76, want_hex);
   }
   ck_assert_int_eq(aes_gcm_decrypt(&g, iv, ivl, aad, 20, ct, 60, tag, dec), 0);
   ck_assert(memcmp(dec, pt, 60) == 0);
   tag[3] ^= 1;
   memset(dec, 0xa5, 60);
   ck_assert_int_eq(aes_gcm_decrypt(&g, iv, ivl, aad, 20, ct, 60, tag, dec), -1);
   ck_assert_uint_eq(dec[0], 0xa5);
}

START_TEST (test_aes_gcm)
{
   struct aes_gcm_ctx g;
   uint8_t key[32], iv[12], zero[16], ct[16], tag[16];
   memset(key, 0, 32); memset(iv, 0, 12); memset(zero, 0, 16);

   ck_assert_int_eq(aes_gcm_init(&g, key, 32), 0);
   ck_assert_int_eq(aes_gcm_encrypt(&g, iv, 12, NULL, 0, NULL, 0, ct, tag), 0);
   ck_hex(tag, 16, "530f8afbc74536b9a963b4f1c4cb738b");
   ck_assert_int_eq(aes_gcm_encrypt(&g, iv, 12, NULL, 0, zero, 16, ct, tag), 0);
   ck_hex(ct,  16, "cea7403d4d606b6e074ec5d3baf39d18");
   ck_hex(tag, 16, "d0d1c8a799996bf0265b98b5d48ab919");

   ck_assert_int_eq(aes_gcm_init(&g, key, 16), 0);
   ck_assert_int_eq(aes_gcm_encrypt(&g, iv, 12, NULL, 0, zero, 16, ct, tag), 0);
   ck_hex(ct,  16, "0388dace60b6a392f328c2b971b2fe78");
   ck_hex(tag, 16, "ab6e47d42cec13bdf53a67b21257bddf");

   gcm_case("cafebabefacedbaddecaf888",
      "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa"
      "8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662"
      "76fc6ece0f4e1768cddf8853bb2d551b");
   gcm_case("cafebabefacedbad",
      "c3762df1ca787d32ae47c13bf19844cbaf1ae14d0b976afac52ff7d79bba9de0"
      "feb582d33934a4f0954cc2363bc73f7862ac430e64abe499f47c9b1f"
      "3a337dbf46a792c45e454913fe2ea8f2");
   gcm_case("9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728"
            "c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
      "5a8def2f0c9e53f1f75d7853659e2a20eeb2b22aafde6419a058ab4f6f746bf4"
      "0fc0c3b780f244452da3ebf1c5d82cdea2418997200ef82e44ae7e3f"
      "a44a8266ee1c8eb0c8b5d4cf5ae9f19a");
}
END_TEST

START_TEST (test_chacha20)
{
   uint8_t key[32], nonce[12], ct[114], dec[114];
   unsigned i;
   for (i = 0; i < 32; i++)
      key[i] = (uint8_t)i;
   unhex(nonce, 12, "000000000000004a00000000");
   chacha20_xor(key, nonce, 1, (const uint8_t*)chacha_pt, ct, 114);
   ck_hex(ct, 114,
      "6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0b"
      "f91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d8"
      "07ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab7793736"
      "5af90bbf74a35be6b40b8eedf2785e42874d");
   chacha20_xor(key, nonce, 1, ct, dec, 114);
   ck_assert(memcmp(dec, chacha_pt, 114) == 0);
}
END_TEST

START_TEST (test_poly1305)
{
   struct poly1305_ctx p;
   uint8_t key[32], tag[16];
   const char *msg = "Cryptographic Forum Research Group";
   unhex(key, 32, "85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b");
   poly1305_auth(tag, (const uint8_t*)msg, 34, key);
   ck_hex(tag, 16, "a8061dc1305136c6c22b8baf0c0127a9");

   /* same thing fed in odd-sized pieces */
   poly1305_init(&p, key);
   poly1305_update(&p, (const uint8_t*)msg, 7);
   poly1305_update(&p, (const uint8_t*)msg + 7, 20);
   poly1305_update(&p, (const uint8_t*)msg + 27, 7);
   poly1305_final(&p, tag);
   ck_hex(tag, 16, "a8061dc1305136c6c22b8baf0c0127a9");
}
END_TEST

START_TEST (test_aead)
{
   uint8_t key[32], nonce[12], aad[12], ct[114], tag[16], dec[114];
   unsigned i;
   for (i = 0; i < 32; i++)
      key[i] = (uint8_t)(0x80 + i);
   unhex(nonce, 12, "070000004041424344454647");
   unhex(aad, 12, "50515253c0c1c2c3c4c5c6c7");

   ck_assert_int_eq(aead_encrypt(AEAD_CHACHA20_POLY1305, key, 32, nonce, 12,
         aad, 12, (const uint8_t*)chacha_pt, 114, ct, tag, 16), 0);
   ck_hex(ct, 114,
      "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d6"
      "3dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b36"
      "92ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc"
      "3ff4def08e4b7a9de576d26586cec64b6116");
   ck_hex(tag, 16, "1ae10b594f09e26a7e902ecbd0600691");
   ck_assert_int_eq(aead_decrypt(AEAD_CHACHA20_POLY1305, key, 32, nonce, 12,
         aad, 12, ct, 114, tag, 16, dec), 0);
   ck_assert(memcmp(dec, chacha_pt, 114) == 0);
   aad[0] ^= 1;
   ck_assert_int_eq(aead_decrypt(AEAD_CHACHA20_POLY1305, key, 32, nonce, 12,
         aad, 12, ct, 114, tag, 16, dec), -2);
   aad[0] ^= 1;

   /* AES-256-GCM through the same front, in place, round trip */
   memcpy(ct, chacha_pt, 114);
   ck_assert_int_eq(aead_encrypt(AEAD_AES256_GCM, key, 32, nonce, 12,
         aad, 12, ct, 114, ct, tag, 16), 0);
   ck_assert(memcmp(ct, chacha_pt, 114) != 0);
   ck_assert_int_eq(aead_decrypt(AEAD_AES256_GCM, key, 32, nonce, 12,
         aad, 12, ct, 114, tag, 16, ct), 0);
   ck_assert(memcmp(ct, chacha_pt, 114) == 0);

   /* parameter checks */
   ck_assert_int_eq(aead_encrypt(AEAD_AES256_GCM, key, 16, nonce, 12,
         aad, 12, ct, 114, ct, tag, 16), -1);
   ck_assert_int_eq(aead_encrypt(AEAD_AES256_GCM, key, 32, nonce, 8,
         aad, 12, ct, 114, ct, tag, 16), -1);
   ck_assert_int_eq(aead_encrypt((enum aead_alg)7, key, 32, nonce, 12,
         aad, 12, ct, 114, ct, tag, 16), -1);
}
END_TEST

START_TEST (test_hmac_hkdf_pbkdf2)
{
   uint8_t mac[32], ikm[22], salt[13], info[10], okm[42], prk[32], dk[32];
   struct hmac_sha256_ctx h;
   unsigned i;

   hmac_sha256((const uint8_t*)"Jefe", 4,
         (const uint8_t*)"what do ya want for nothing?", 28, mac);
   ck_hex(mac, 32, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

   /* streaming, split mid-message */
   hmac_sha256_init(&h, (const uint8_t*)"Jefe", 4);
   hmac_sha256_update(&h, (const uint8_t*)"what do ya", 10);
   hmac_sha256_update(&h, (const uint8_t*)" want for nothing?", 18);
   hmac_sha256_final(&h, mac);
   ck_hex(mac, 32, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

   /* key longer than a block goes through the hash */
   {
      uint8_t longkey[131];
      memset(longkey, 0xaa, sizeof(longkey));
      hmac_sha256(longkey, sizeof(longkey),
            (const uint8_t*)"Test Using Larger Than Block-Size Key - Hash Key First", 54, mac);
      ck_hex(mac, 32, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
   }

   memset(ikm, 0x0b, 22);
   for (i = 0; i < 13; i++) salt[i] = (uint8_t)i;
   for (i = 0; i < 10; i++) info[i] = (uint8_t)(0xf0 + i);
   hkdf_sha256_extract(salt, 13, ikm, 22, prk);
   ck_hex(prk, 32, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");
   ck_assert_int_eq(hkdf_sha256(salt, 13, ikm, 22, info, 10, okm, 42), 0);
   ck_hex(okm, 42, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
   ck_assert_int_eq(hkdf_sha256_expand(prk, 32, info, 10, okm, 255 * 32 + 1), -1);

   ck_assert_int_eq(pbkdf2_hmac_sha256((const uint8_t*)"password", 8,
         (const uint8_t*)"salt", 4, 1, dk, 32), 0);
   ck_hex(dk, 32, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
   ck_assert_int_eq(pbkdf2_hmac_sha256((const uint8_t*)"password", 8,
         (const uint8_t*)"salt", 4, 4096, dk, 32), 0);
   ck_hex(dk, 32, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
   ck_assert_int_eq(pbkdf2_hmac_sha256((const uint8_t*)"password", 8,
         (const uint8_t*)"salt", 4, 0, dk, 32), -1);
}
END_TEST

START_TEST (test_drbg)
{
   struct drbg_ctx a, b;
   uint8_t seed[32], o1[200], o2[200], o3[200], zero[200];
   memset(seed, 0x42, 32);
   memset(zero, 0, 200);

   ck_assert_int_eq(drbg_init(&a, seed, 32), 0);
   ck_assert_int_eq(drbg_init(&b, seed, 32), 0);
   ck_assert_int_eq(drbg_generate(&a, o1, 200), 0);
   ck_assert_int_eq(drbg_generate(&b, o2, 200), 0);
   ck_assert(memcmp(o1, o2, 200) == 0);
   ck_assert(memcmp(o1, zero, 200) != 0);
   /* key erasure: the next call does not repeat the last */
   ck_assert_int_eq(drbg_generate(&a, o3, 200), 0);
   ck_assert(memcmp(o1, o3, 200) != 0);
   /* a different seed diverges */
   seed[0] ^= 1;
   ck_assert_int_eq(drbg_init(&b, seed, 32), 0);
   ck_assert_int_eq(drbg_generate(&b, o2, 200), 0);
   ck_assert(memcmp(o1, o2, 200) != 0);
   drbg_free(&a);
   ck_assert_int_eq(drbg_generate(&a, o1, 16), -1);
   drbg_free(&b);

   /* platform entropy is live and not stuck */
   ck_assert_int_eq(crypto_random_bytes(o1, 64), 0);
   ck_assert_int_eq(crypto_random_bytes(o2, 64), 0);
   ck_assert(memcmp(o1, o2, 64) != 0);
   ck_assert(memcmp(o1, zero, 64) != 0);
   ck_assert_int_eq(drbg_init(&a, NULL, 0), 0);
   ck_assert_int_eq(drbg_generate(&a, o1, 64), 0);
   ck_assert(memcmp(o1, zero, 64) != 0);
   drbg_free(&a);
}
END_TEST

START_TEST (test_util)
{
   uint8_t a[8] = {1,2,3,4,5,6,7,8};
   uint8_t b[8] = {1,2,3,4,5,6,7,8};
   ck_assert_int_eq(crypto_memeq_ct(a, b, 8), 1);
   ck_assert_int_eq(crypto_memeq_ct(a, b, 0), 1);
   b[7] = 9;
   ck_assert_int_eq(crypto_memeq_ct(a, b, 8), 0);
   ck_assert_int_eq(crypto_memeq_ct(a, b, 7), 1);
   crypto_memzero(a, 8);
   ck_assert_int_eq(crypto_memeq_ct(a, "\0\0\0\0\0\0\0\0", 8), 1);
}
END_TEST

/* Vectors in test_pk_vectors.h were produced with a reference
 * implementation (python-cryptography / OpenSSL): fresh RSA-2048,
 * RSA-4096 and P-256 keys signing one message, and one P-256 ECDH
 * exchange. Regenerating them is fine; nothing depends on the values. */
#include "test_pk_vectors.h"

static const uint8_t e65537[3] = { 0x01, 0x00, 0x01 };
static const uint8_t e3[1]     = { 0x03 };

static void hex_to_pk(uint8_t *out, const char *hex, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      unsigned v;
      char t[3];
      t[0] = hex[2 * i]; t[1] = hex[2 * i + 1]; t[2] = 0;
      sscanf(t, "%x", &v);
      out[i] = (uint8_t)v;
   }
}

START_TEST (test_sha512)
{
   struct sha512_state s;
   uint8_t d[64], want[64];
   uint8_t *big;
   size_t i;

   sha512_stream_init(&s, 1);
   sha512_stream_update(&s, (const uint8_t*)"abc", 3);
   sha512_stream_final(&s, d);
   hex_to_pk(want, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7", 48);
   ck_assert(memcmp(d, want, 48) == 0);

   sha512_stream_init(&s, 0);
   sha512_stream_update(&s, (const uint8_t*)"abc", 3);
   sha512_stream_final(&s, d);
   hex_to_pk(want, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", 64);
   ck_assert(memcmp(d, want, 64) == 0);

   /* 112 octets: the length no longer fits in the first block */
   sha512_stream_init(&s, 0);
   for (i = 0; i < 112; i++)
      sha512_stream_update(&s, (const uint8_t*)"a", 1);
   sha512_stream_final(&s, d);
   hex_to_pk(want, "c01d080efd492776a1c43bd23dd99d0a2e626d481e16782e75d54c2503b5dc32bd05f0f1ba33e568b88fd2d970929b719ecbb152f58f130a407c8830604b70ca", 64);
   ck_assert(memcmp(d, want, 64) == 0);

   big = (uint8_t*)malloc(1000000);
   ck_assert_ptr_nonnull(big);
   memset(big, 'a', 1000000);
   sha512_stream_init(&s, 1);
   sha512_stream_update(&s, big, 777);
   sha512_stream_update(&s, big + 777, 1000000 - 777);
   sha512_stream_final(&s, d);
   hex_to_pk(want, "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985", 48);
   ck_assert(memcmp(d, want, 48) == 0);
   free(big);
}
END_TEST

START_TEST (test_bn)
{
   /* 128-bit modulus 2^128 - 159 (a prime), so k = 128 / BN_WORD_BITS
    * whichever limb width is compiled in. Expected values come from
    * Python's pow() / arbitrary-precision arithmetic. */
   enum { KW = 128 / BN_WORD_BITS };
   bn_word a[KW], b[KW], m[KW], r2[KW], r[KW], e[KW];
   bn_word tmp[BN_MONT_TMP_WORDS(KW)];
   bn_word work[3 * KW + BN_MONT_TMP_WORDS(KW)];
   uint8_t be[16], want[16];

   hex_to_pk(be, "ffffffffffffffffffffffffffffff61", 16);
   bn_from_be(m, KW, be, 16);
   hex_to_pk(be, "00000010000000000000000000000003", 16);   /* 2^100 + 3 */
   bn_from_be(a, KW, be, 16);
   hex_to_pk(be, "00000000000000400000000000000005", 16);   /* 2^70 + 5 */
   bn_from_be(b, KW, be, 16);

   bn_mont_r2(r2, m, KW);
   bn_mod_mul(r, a, b, m, r2, bn_mont_n0(m), KW, tmp);
   bn_to_be(be, 16, r, KW);
   hex_to_pk(want, "00000050000000c000027c000000000f", 16);
   ck_assert(memcmp(be, want, 16) == 0);

   /* 3^5 mod m */
   bn_zero(a, KW); a[0] = 3;
   bn_zero(e, KW); e[0] = 5;
   bn_mod_exp(r, a, e, KW, m, r2, bn_mont_n0(m), KW, work);
   bn_to_be(be, 16, r, KW);
   hex_to_pk(want, "000000000000000000000000000000f3", 16);
   ck_assert(memcmp(be, want, 16) == 0);

   /* octet conversion round trip, short and long buffers */
   bn_from_be(b, KW, be + 14, 2);
   ck_assert_int_eq(bn_cmp(b, r, KW), 0);
   bn_to_be(be, 3, r, KW);
   ck_assert_uint_eq(be[2], 243u);
   ck_assert_uint_eq(be[0], 0u);

   ck_assert_int_eq(bn_cmp(m, r, KW), 1);
   ck_assert_int_eq(bn_cmp(r, m, KW), -1);
   ck_assert_uint_eq(bn_bit_length(m, KW), 128u);
   ck_assert_uint_eq(bn_bit_length(r, KW), 8u);
   ck_assert_uint_eq(bn_get_bit(r, 7), 1u);
   ck_assert_uint_eq(bn_get_bit(r, 2), 0u);
   bn_zero(a, KW);
   ck_assert(bn_is_zero(a, KW));
   ck_assert_uint_eq(bn_bit_length(a, KW), 0u);
}
END_TEST

START_TEST (test_rsa)
{
   uint8_t bad[512];

   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 256), 0);
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha384_n, 256, e65537, 3,
         RSA_HASH_SHA384, rsa2048_sha384_digest, 48, rsa2048_sha384_sig, 256), 0);
   ck_assert_int_eq(rsa_pkcs1_verify(rsa4096_sha512_n, 512, e65537, 3,
         RSA_HASH_SHA512, rsa4096_sha512_digest, 64, rsa4096_sha512_sig, 512), 0);
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_e3_n, 256, e3, 1,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_e3_sig, 256), 0);

   /* wrong hash algorithm for the DigestInfo */
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA384, rsa2048_sha384_digest, 48, rsa2048_sha256_sig, 256), -1);
   /* wrong digest */
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha384_digest, 32, rsa2048_sha256_sig, 256), -1);
   /* wrong key */
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha384_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 256), -1);
   /* flipped signature bit */
   memcpy(bad, rsa2048_sha256_sig, 256);
   bad[100] ^= 0x10;
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, bad, 256), -1);
   /* signature >= n */
   memcpy(bad, rsa2048_sha256_n, 256);
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, bad, 256), -1);
   /* length mismatch, even modulus, even exponent, digest length */
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 255), -1);
   memcpy(bad, rsa2048_sha256_n, 256);
   bad[255] &= 0xfe;
   ck_assert_int_eq(rsa_pkcs1_verify(bad, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 256), -1);
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, (const uint8_t*)"\x02", 1,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 256), -1);
   ck_assert_int_eq(rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3,
         RSA_HASH_SHA256, rsa2048_sha256_digest, 31, rsa2048_sha256_sig, 256), -1);
}
END_TEST

START_TEST (test_p256)
{
   uint8_t pub[65], shared[32], bad[65], sig[32];

   /* keygen matches the reference public key */
   ck_assert_int_eq(p256_keygen(ecdh_a_priv, pub), 0);
   ck_assert(memcmp(pub, ecdh_a_pub, 65) == 0);

   /* ECDH agrees with the reference */
   ck_assert_int_eq(p256_ecdh(ecdh_a_priv, ecdh_b_pub, shared), 0);
   ck_assert(memcmp(shared, ecdh_shared, 32) == 0);

   /* invalid peer points: off curve, x >= p, wrong prefix */
   memcpy(bad, ecdh_b_pub, 65);
   bad[40] ^= 1;
   ck_assert_int_eq(p256_ecdh(ecdh_a_priv, bad, shared), -1);
   memcpy(bad, ecdh_b_pub, 65);
   memset(bad + 1, 0xff, 32);
   ck_assert_int_eq(p256_ecdh(ecdh_a_priv, bad, shared), -1);
   memcpy(bad, ecdh_b_pub, 65);
   bad[0] = 0x02;
   ck_assert_int_eq(p256_ecdh(ecdh_a_priv, bad, shared), -1);
   /* zero and >= n scalars */
   memset(bad, 0, 32);
   ck_assert_int_eq(p256_keygen(bad, pub), -1);
   memset(bad, 0xff, 32);
   ck_assert_int_eq(p256_keygen(bad, pub), -1);

   /* ECDSA, SHA-256 and a SHA-384 digest (truncated to 32) */
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha256_digest, 32,
         ecdsa_sha256_r, ecdsa_sha256_s), 0);
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha384_digest, 48,
         ecdsa_sha384_r, ecdsa_sha384_s), 0);
   /* wrong digest, wrong key, tampered r and s */
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha384_digest, 48,
         ecdsa_sha256_r, ecdsa_sha256_s), -1);
   ck_assert_int_eq(p256_ecdsa_verify(ecdh_a_pub, ecdsa_sha256_digest, 32,
         ecdsa_sha256_r, ecdsa_sha256_s), -1);
   memcpy(sig, ecdsa_sha256_r, 32);
   sig[31] ^= 1;
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha256_digest, 32,
         sig, ecdsa_sha256_s), -1);
   memcpy(sig, ecdsa_sha256_s, 32);
   sig[0] ^= 0x80;
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha256_digest, 32,
         ecdsa_sha256_r, sig), -1);
   memset(sig, 0, 32);
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha256_digest, 32,
         sig, ecdsa_sha256_s), -1);
}
END_TEST

START_TEST (test_p384)
{
   uint8_t pub[97], shared[48], bad[97], sig[48];

   ck_assert_int_eq(p384_keygen(ecdh384_a_priv, pub), 0);
   ck_assert(memcmp(pub, ecdh384_a_pub, 97) == 0);
   ck_assert_int_eq(p384_ecdh(ecdh384_a_priv, ecdh384_b_pub, shared), 0);
   ck_assert(memcmp(shared, ecdh384_shared, 48) == 0);
   memcpy(bad, ecdh384_b_pub, 97);
   bad[50] ^= 1;
   ck_assert_int_eq(p384_ecdh(ecdh384_a_priv, bad, shared), -1);

   ck_assert_int_eq(p384_ecdsa_verify(ecdsa384_pub, ecdsa384_digest, 48,
         ecdsa384_r, ecdsa384_s), 0);
   memcpy(sig, ecdsa384_s, 48);
   sig[47] ^= 1;
   ck_assert_int_eq(p384_ecdsa_verify(ecdsa384_pub, ecdsa384_digest, 48,
         ecdsa384_r, sig), -1);
   ck_assert_int_eq(p384_ecdsa_verify(ecdsa384_pub, ecdsa_sha256_digest, 32,
         ecdsa384_r, ecdsa384_s), -1);
   /* the P-256 vectors still hold after the curve-generic rewrite */
   ck_assert_int_eq(p256_ecdsa_verify(ecdsa_pub, ecdsa_sha256_digest, 32,
         ecdsa_sha256_r, ecdsa_sha256_s), 0);
}
END_TEST

START_TEST (test_x509)
{
   struct x509_cert c, r, in;
   const uint8_t *chain[3];
   size_t lens[3];
   char info[128];

   ck_assert_int_eq(x509_parse(&r, x509_root, sizeof(x509_root)), 0);
   ck_assert_int_eq(r.key_type, X509_KEY_RSA);
   ck_assert_int_eq(r.sig_alg, X509_SIG_RSA_SHA384);
   ck_assert_int_eq(r.is_ca, 1);
   ck_assert(r.key_usage & X509_KU_KEY_CERT_SIGN);
   ck_assert_int_eq(x509_parse(&in, x509_inter, sizeof(x509_inter)), 0);
   ck_assert_int_eq(in.key_type, X509_KEY_P256);
   ck_assert_int_eq(in.path_len, 0);
   ck_assert_int_eq(x509_parse(&c, x509_leaf, sizeof(x509_leaf)), 0);
   ck_assert_int_eq(c.key_type, X509_KEY_RSA);
   ck_assert_int_eq(c.sig_alg, X509_SIG_ECDSA_SHA256);
   ck_assert_int_eq(c.is_ca, 0);
   ck_assert(c.not_before < X509_TEST_NOW && c.not_after > X509_TEST_NOW);

   /* signatures: RSA-SHA384 self-signed root, RSA-SHA256 over the
    * intermediate, ECDSA-SHA256 over the leaf */
   ck_assert_int_eq(x509_verify_signature(&r, &r), 0);
   ck_assert_int_eq(x509_verify_signature(&in, &r), 0);
   ck_assert_int_eq(x509_verify_signature(&c, &in), 0);
   ck_assert_int_eq(x509_verify_signature(&c, &r), -1);
   ck_assert_int_eq(x509_parse(&c, x509_badsig, sizeof(x509_badsig)), 0);
   ck_assert_int_eq(x509_verify_signature(&c, &in), -1);

   /* hostnames */
   ck_assert_int_eq(x509_parse(&c, x509_leaf, sizeof(x509_leaf)), 0);
   ck_assert_int_eq(x509_match_hostname(&c, "example.com"), 0);
   ck_assert_int_eq(x509_match_hostname(&c, "EXAMPLE.com"), 0);
   ck_assert_int_eq(x509_match_hostname(&c, "a.wild.example.com"), 0);
   ck_assert_int_eq(x509_match_hostname(&c, "wild.example.com"), -1);
   ck_assert_int_eq(x509_match_hostname(&c, "a.b.wild.example.com"), -1);
   ck_assert_int_eq(x509_match_hostname(&c, "www.example.com"), -1);
   ck_assert_int_eq(x509_match_hostname(&c, "example.co"), -1);
   ck_assert_int_eq(x509_parse(&c, x509_cnonly, sizeof(x509_cnonly)), 0);
   ck_assert_int_eq(c.sig_alg, X509_SIG_ECDSA_SHA512);
   ck_assert_int_eq(x509_match_hostname(&c, "cn.example.com"), 0);

   /* chain against the test root as the only anchor */
   ck_assert_int_eq(x509_trust_load_pem(x509_root_pem, sizeof(x509_root_pem) - 1), 1);
   chain[0] = x509_leaf;  lens[0] = sizeof(x509_leaf);
   chain[1] = x509_inter; lens[1] = sizeof(x509_inter);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 2, "example.com", X509_TEST_NOW, info, sizeof(info)), 0);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 2, NULL, X509_TEST_NOW, info, sizeof(info)), 0);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 2, "other.com", X509_TEST_NOW, info, sizeof(info)), -1);
   ck_assert_str_eq(info, "hostname mismatch");
   ck_assert_int_eq(x509_verify_chain(chain, lens, 1, "example.com", X509_TEST_NOW, info, sizeof(info)), -1);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 2, "example.com", X509_TEST_NOW + 200L * 86400, info, sizeof(info)), -1);
   ck_assert_str_eq(info, "certificate expired or not yet valid");
   chain[0] = x509_expired; lens[0] = sizeof(x509_expired);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 2, "expired.example.com", X509_TEST_NOW, info, sizeof(info)), -1);
   chain[0] = x509_badsig; lens[0] = sizeof(x509_badsig);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 2, "example.com", X509_TEST_NOW, info, sizeof(info)), -1);
   ck_assert_str_eq(info, "issuer not found or signature invalid");
   /* intermediate has pathLen 0: a CA below it is refused */
   chain[0] = x509_deep; lens[0] = sizeof(x509_deep);
   chain[1] = x509_sub2; lens[1] = sizeof(x509_sub2);
   chain[2] = x509_inter; lens[2] = sizeof(x509_inter);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 3, "deep.example.com", X509_TEST_NOW, info, sizeof(info)), -1);
   ck_assert_str_eq(info, "path length constraint violated");
   /* the chain may arrive with the root included, or out of order */
   chain[0] = x509_leaf;  lens[0] = sizeof(x509_leaf);
   chain[1] = x509_root;  lens[1] = sizeof(x509_root);
   chain[2] = x509_inter; lens[2] = sizeof(x509_inter);
   ck_assert_int_eq(x509_verify_chain(chain, lens, 3, "example.com", X509_TEST_NOW, info, sizeof(info)), 0);

   /* the shipped bundle: every anchor with an RSA or P-256 key parses */
   {
      int n = x509_trust_load_pem(cacert_pem, sizeof(cacert_pem));
      ck_assert_int_eq(n, 155);   /* every certificate in the bundle */
      ck_assert_int_eq(x509_trust_load_pem(cacert_pem, sizeof(cacert_pem)), n);
      ck_assert_int_eq(x509_verify_chain(chain, lens, 3, "example.com", X509_TEST_NOW, info, sizeof(info)), -1);
   }
   x509_trust_free();
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");
   tcase_set_timeout(tc_core, 60);
   tcase_add_test(tc_core, test_aes_block);
   tcase_add_test(tc_core, test_aes_cmac);
   tcase_add_test(tc_core, test_aes_gcm);
   tcase_add_test(tc_core, test_chacha20);
   tcase_add_test(tc_core, test_poly1305);
   tcase_add_test(tc_core, test_aead);
   tcase_add_test(tc_core, test_hmac_hkdf_pbkdf2);
   tcase_add_test(tc_core, test_drbg);
   tcase_add_test(tc_core, test_util);
   tcase_add_test(tc_core, test_sha512);
   tcase_add_test(tc_core, test_bn);
   tcase_add_test(tc_core, test_rsa);
   tcase_add_test(tc_core, test_p256);
   tcase_add_test(tc_core, test_p384);
   tcase_add_test(tc_core, test_x509);
   suite_add_tcase(s, tc_core);
   return s;
}

int main(void)
{
   int num_fail;
   Suite *s = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
