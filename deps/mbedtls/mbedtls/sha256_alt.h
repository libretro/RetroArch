/* SHA-256 and SHA-224 for mbedtls, over the implementation in
 * libretro-common, which carries the x86 SHA-NI and ARMv8 SHA-2
 * code paths and is the one SHA-256 the tree builds.
 *
 * Supplied through the MBEDTLS_SHA256_ALT hook, so sha256.c drops its
 * own implementation and keeps only the mbedtls_sha256() one-shot,
 * which is written in terms of the entry points below.
 */

#ifndef MBEDTLS_SHA256_ALT_H
#define MBEDTLS_SHA256_ALT_H

#include <stddef.h>

#include <lrc_hash.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sha256_state mbedtls_sha256_context;

void mbedtls_sha256_init(mbedtls_sha256_context *ctx);

void mbedtls_sha256_free(mbedtls_sha256_context *ctx);

void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
      const mbedtls_sha256_context *src);

void mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224);

void mbedtls_sha256_update(mbedtls_sha256_context *ctx,
      const unsigned char *input, size_t ilen);

void mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
      unsigned char output[32]);

void mbedtls_sha256_process(mbedtls_sha256_context *ctx,
      const unsigned char data[64]);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_SHA256_ALT_H */
