/* Bodies for the MBEDTLS_SHA1_ALT and MBEDTLS_SHA256_ALT hooks.
 *
 * Each entry point is the matching libretro-common one, so mbedtls
 * hashes through the same code as the rest of the tree and inherits
 * its runtime dispatch onto SHA-NI or the ARMv8 SHA instructions.
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <string.h>

#include <lrc_hash.h>

#if defined(MBEDTLS_SHA256_ALT)
#include "mbedtls/sha256.h"

void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
   memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
   if (!ctx)
      return;
   memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
      const mbedtls_sha256_context *src)
{
   *dst = *src;
}

void mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224)
{
   sha256_stream_init(ctx, is224 ? 1u : 0u);
}

void mbedtls_sha256_update(mbedtls_sha256_context *ctx,
      const unsigned char *input, size_t ilen)
{
   sha256_stream_update(ctx, (const uint8_t *)input, ilen);
}

void mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
      unsigned char output[32])
{
   /* Seven words for SHA-224 and eight for SHA-256, which is what the
    * caller reads in each case. */
   sha256_stream_final(ctx, (uint8_t *)output);
}

void mbedtls_sha256_process(mbedtls_sha256_context *ctx,
      const unsigned char data[64])
{
   /* One block straight into the state, message length untouched:
    * the caller doing this is driving the padding itself. */
   sha256_stream_block(ctx, (const uint8_t *)data);
}
#endif /* MBEDTLS_SHA256_ALT */

#if defined(MBEDTLS_SHA1_ALT)
#include "mbedtls/sha1.h"

void mbedtls_sha1_init(mbedtls_sha1_context *ctx)
{
   memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha1_free(mbedtls_sha1_context *ctx)
{
   if (!ctx)
      return;
   memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha1_clone(mbedtls_sha1_context *dst,
      const mbedtls_sha1_context *src)
{
   *dst = *src;
}

void mbedtls_sha1_starts(mbedtls_sha1_context *ctx)
{
   sha1_stream_init(ctx);
}

void mbedtls_sha1_update(mbedtls_sha1_context *ctx,
      const unsigned char *input, size_t ilen)
{
   sha1_stream_update(ctx, (const uint8_t *)input, ilen);
}

void mbedtls_sha1_finish(mbedtls_sha1_context *ctx,
      unsigned char output[20])
{
   /* This entry point returns nothing, so a stream that was marked bad
    * yields a zero digest rather than whatever the buffer held. */
   if (!sha1_stream_final(ctx, (uint8_t *)output))
      memset(output, 0, 20);
}

void mbedtls_sha1_process(mbedtls_sha1_context *ctx,
      const unsigned char data[64])
{
   sha1_stream_block(ctx, (const uint8_t *)data);
}
#endif /* MBEDTLS_SHA1_ALT */
