/* Throughput of the cleanroom primitives against the mbedtls the
 * Linux release build links. Numbers are MB/s or ops/s, higher is
 * better; the "x" column is cleanroom over mbedtls. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <crypto/pk.h>
#include <lrc_hash.h>

#include <mbedtls/gcm.h>
#include <mbedtls/chachapoly.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/md.h>
#include <mbedtls/rsa.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/hkdf.h>

#include "../../../libretro-common/test/crypto/test_pk_vectors.h"

static double now(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec * 1e-9;
}

#define BUF (64 * 1024)
static uint8_t buf[BUF], out[BUF + 16], key[32], nonce[12], tag[16];

#define RUN(label, unit, scale, ours_stmt, theirs_stmt) do { \
   double t0, t1, a, b; int i, n; \
   for (n = 1; ; n *= 2) { t0 = now(); for (i = 0; i < n; i++) { ours_stmt; } t1 = now(); if (t1 - t0 > 0.3) break; } \
   a = n * (scale) / (t1 - t0); \
   t0 = now(); for (i = 0; i < n; i++) { theirs_stmt; } t1 = now(); \
   b = n * (scale) / (t1 - t0); \
   printf("%-28s %10.1f %10.1f %s   %5.2fx\n", label, a, b, unit, a / b); \
} while (0)

int main(void)
{
   mbedtls_gcm_context gcm;
   mbedtls_chachapoly_context cp;
   struct aes_gcm_ctx ours_gcm;
   uint8_t d[64], pub[65], shared[32];
   mbedtls_rsa_context rsa;
   mbedtls_mpi N, E;
   mbedtls_ecdh_context ecdh;
   mbedtls_ecdsa_context ecdsa;
   mbedtls_ecp_group grp;
   mbedtls_ecp_point Q;
   mbedtls_mpi r, s, dA;
   static const uint8_t e65537[3] = {1, 0, 1};
   size_t i;

   for (i = 0; i < BUF; i++) buf[i] = (uint8_t)(i * 7 + 1);
   for (i = 0; i < 32; i++) key[i] = (uint8_t)i;
   memset(nonce, 0x42, 12);

   printf("%-28s %10s %10s %-6s %s\n", "op", "cleanroom", "mbedtls", "unit", "ratio");

   mbedtls_gcm_init(&gcm);
   mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
   aes_gcm_init(&ours_gcm, key, 32);
   RUN("AES-256-GCM encrypt 64K", "MB/s", BUF / 1e6,
       aes_gcm_encrypt(&ours_gcm, nonce, 12, NULL, 0, buf, BUF, out, tag),
       mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, BUF, nonce, 12, NULL, 0, buf, out, 16, tag));
   RUN("AES-256-GCM encrypt 64B", "Mop/s", 1e-6,
       aes_gcm_encrypt(&ours_gcm, nonce, 12, NULL, 0, buf, 64, out, tag),
       mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, 64, nonce, 12, NULL, 0, buf, out, 16, tag));
   RUN("AES-256-GCM setkey+enc 64B", "Mop/s", 1e-6,
       aead_encrypt(AEAD_AES256_GCM, key, 32, nonce, 12, NULL, 0, buf, 64, out, tag, 16),
       (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256), mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, 64, nonce, 12, NULL, 0, buf, out, 16, tag)));

   mbedtls_chachapoly_init(&cp);
   mbedtls_chachapoly_setkey(&cp, key);
   RUN("ChaCha20-Poly1305 64K", "MB/s", BUF / 1e6,
       aead_encrypt(AEAD_CHACHA20_POLY1305, key, 32, nonce, 12, NULL, 0, buf, BUF, out, tag, 16),
       mbedtls_chachapoly_encrypt_and_tag(&cp, BUF, nonce, NULL, 0, buf, out, tag));
   RUN("ChaCha20-Poly1305 64B", "Mop/s", 1e-6,
       aead_encrypt(AEAD_CHACHA20_POLY1305, key, 32, nonce, 12, NULL, 0, buf, 64, out, tag, 16),
       mbedtls_chachapoly_encrypt_and_tag(&cp, 64, nonce, NULL, 0, buf, out, tag));

   RUN("SHA-256 64K (lrc_hash)", "MB/s", BUF / 1e6,
       { struct sha256_state st; sha256_stream_init(&st, 0); sha256_stream_update(&st, buf, BUF); sha256_stream_final(&st, d); },
       mbedtls_sha256(buf, BUF, d, 0));
   RUN("SHA-512 64K", "MB/s", BUF / 1e6,
       { struct sha512_state st; sha512_stream_init(&st, 0); sha512_stream_update(&st, buf, BUF); sha512_stream_final(&st, d); },
       mbedtls_sha512(buf, BUF, d, 0));
   RUN("HMAC-SHA256 64B", "Mop/s", 1e-6,
       hmac_sha256(key, 32, buf, 64, d),
       mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 32, buf, 64, d));

   mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);
   mbedtls_mpi_init(&N); mbedtls_mpi_init(&E);
   mbedtls_mpi_read_binary(&N, rsa2048_sha256_n, 256);
   mbedtls_mpi_read_binary(&E, e65537, 3);
   mbedtls_rsa_import(&rsa, &N, NULL, NULL, NULL, &E);
   mbedtls_rsa_complete(&rsa);
   /* A certificate chain verifies each signature under a different
    * key, so the mbedtls side re-imports the key per call as well:
    * its R^2 cache is what a handshake never gets to reuse. */
   RUN("RSA-2048 verify (fresh key)", "kop/s", 1e-3,
       rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3, RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 256),
       (mbedtls_rsa_free(&rsa), mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0), mbedtls_rsa_import(&rsa, &N, NULL, NULL, NULL, &E), mbedtls_rsa_complete(&rsa),
        mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA256, 32, rsa2048_sha256_digest, rsa2048_sha256_sig)));
   RUN("RSA-2048 verify (cached key)", "kop/s", 1e-3,
       rsa_pkcs1_verify(rsa2048_sha256_n, 256, e65537, 3, RSA_HASH_SHA256, rsa2048_sha256_digest, 32, rsa2048_sha256_sig, 256),
       mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA256, 32, rsa2048_sha256_digest, rsa2048_sha256_sig));
   mbedtls_mpi_read_binary(&N, rsa4096_sha512_n, 512);
   mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);
   mbedtls_rsa_import(&rsa, &N, NULL, NULL, NULL, &E);
   mbedtls_rsa_complete(&rsa);
   RUN("RSA-4096 verify (fresh key)", "kop/s", 1e-3,
       rsa_pkcs1_verify(rsa4096_sha512_n, 512, e65537, 3, RSA_HASH_SHA512, rsa4096_sha512_digest, 64, rsa4096_sha512_sig, 512),
       (mbedtls_rsa_free(&rsa), mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0), mbedtls_rsa_import(&rsa, &N, NULL, NULL, NULL, &E), mbedtls_rsa_complete(&rsa),
        mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA512, 64, rsa4096_sha512_digest, rsa4096_sha512_sig)));

   mbedtls_ecp_group_init(&grp); mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
   mbedtls_ecp_point_init(&Q); mbedtls_mpi_init(&r); mbedtls_mpi_init(&s); mbedtls_mpi_init(&dA);
   mbedtls_ecp_point_read_binary(&grp, &Q, ecdh_b_pub, 65);
   mbedtls_mpi_read_binary(&dA, ecdh_a_priv, 32);
   {
      mbedtls_mpi z; mbedtls_mpi_init(&z);
      RUN("P-256 ECDH", "kop/s", 1e-3,
          p256_ecdh(ecdh_a_priv, ecdh_b_pub, shared),
          mbedtls_ecdh_compute_shared(&grp, &z, &Q, &dA, NULL, NULL));
      RUN("P-256 keygen (priv*G)", "kop/s", 1e-3,
          p256_keygen(ecdh_a_priv, pub),
          mbedtls_ecp_mul(&grp, &Q, &dA, &grp.G, NULL, NULL));
   }
   mbedtls_ecp_point_read_binary(&grp, &Q, ecdsa_pub, 65);
   mbedtls_mpi_read_binary(&r, ecdsa_sha256_r, 32);
   mbedtls_mpi_read_binary(&s, ecdsa_sha256_s, 32);
   RUN("P-256 ECDSA verify", "kop/s", 1e-3,
       p256_ecdsa_verify(ecdsa_pub, ecdsa_sha256_digest, 32, ecdsa_sha256_r, ecdsa_sha256_s),
       mbedtls_ecdsa_verify(&grp, ecdsa_sha256_digest, 32, &Q, &r, &s));
   (void)ecdh; (void)ecdsa;
   return 0;
}
