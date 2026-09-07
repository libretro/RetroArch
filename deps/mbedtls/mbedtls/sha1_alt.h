/* SHA-1 for mbedtls, over the implementation in libretro-common,
 * which carries the x86 SHA-NI and ARMv8 SHA-1 code paths and is the
 * one SHA-1 the tree builds.
 *
 * Supplied through the MBEDTLS_SHA1_ALT hook, so sha1.c drops its own
 * implementation and keeps only the mbedtls_sha1() one-shot, which is
 * written in terms of the entry points below.
 */

#ifndef MBEDTLS_SHA1_ALT_H
#define MBEDTLS_SHA1_ALT_H

#include <stddef.h>

#include <lrc_hash.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sha1_state mbedtls_sha1_context;

void mbedtls_sha1_init(mbedtls_sha1_context *ctx);

void mbedtls_sha1_free(mbedtls_sha1_context *ctx);

void mbedtls_sha1_clone(mbedtls_sha1_context *dst,
      const mbedtls_sha1_context *src);

void mbedtls_sha1_starts(mbedtls_sha1_context *ctx);

void mbedtls_sha1_update(mbedtls_sha1_context *ctx,
      const unsigned char *input, size_t ilen);

void mbedtls_sha1_finish(mbedtls_sha1_context *ctx,
      unsigned char output[20]);

void mbedtls_sha1_process(mbedtls_sha1_context *ctx,
      const unsigned char data[64]);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_SHA1_ALT_H */
