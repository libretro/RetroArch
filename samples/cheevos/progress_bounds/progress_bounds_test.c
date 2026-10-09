/* The rcheevos runtime reads achievement progress back from a save
 * state or a netplay host, so the bytes are not its own. Each blob here
 * sits in a heap buffer of exactly its size: under ASan, any read past
 * the bytes it was given fails the run, and a chunk length that wraps
 * the offset would loop forever (the caller's timeout ends that). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <rc_runtime.h>
#include <rc_error.h>
#include <lrc_hash.h>

#define MARKER     0x0A504152u /* RAP\n */
#define CHUNK_VARS 0x53524156u
#define CHUNK_DONE 0x454E4F44u

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* @len bytes of @src, in a buffer of exactly that size */
static int read_exact(rc_runtime_t *rt, const uint8_t *src, uint32_t len)
{
   int      rc;
   uint8_t *b = (uint8_t*)malloc(len ? len : 1);
   if (!b)
      return RC_OUT_OF_MEMORY;
   memcpy(b, src, len);
   rc = rc_runtime_deserialize_progress_sized(rt, b, len, NULL);
   free(b);
   return rc;
}

int main(void)
{
   rc_runtime_t rt;
   uint8_t     *blob;
   uint32_t     size, n;
   uint8_t      crafted[64];
   int          rc;

   setvbuf(stdout, NULL, _IONBF, 0);
   rc_runtime_init(&rt);
   size = rc_runtime_progress_size(&rt, NULL);
   blob = (uint8_t*)malloc(size);
   if (!blob || rc_runtime_serialize_progress_sized(blob, size, &rt, NULL) != RC_OK)
   {
      printf("FAIL progress_bounds_test: could not serialize\n");
      return 1;
   }

   rc = read_exact(&rt, blob, size);
   CHECK(rc == RC_OK, "the runtime's own progress read back as %d", rc);
   printf("   ok   the runtime's own progress reads back\n");

   for (n = 0; n < size; n++)
   {
      rc = read_exact(&rt, blob, n);
      CHECK(rc != RC_OK, "progress cut to %u of %u bytes was accepted", n, size);
   }
   printf("   ok   progress cut short anywhere is refused, read only within its bytes\n");

   /* an unknown chunk, the last chunk's header, and four bytes of the
    * digest after it, those four right: the comparison goes on past them */
   {
      MD5_CTX ctx;
      uint8_t md5[16];
      memset(crafted, 0, sizeof(crafted));
      put32(crafted + 0,  MARKER);
      put32(crafted + 4,  0x58585858u); /* XXXX */
      put32(crafted + 8,  4);
      put32(crafted + 16, CHUNK_DONE);
      put32(crafted + 20, 0);
      MD5_Init(&ctx);
      MD5_Update(&ctx, crafted, 24);
      MD5_Final(md5, &ctx);
      memcpy(crafted + 24, md5, 4);
      rc = read_exact(&rt, crafted, 28);
   }
   CHECK(rc != RC_OK, "a digest cut short was accepted");
   printf("   ok   a digest cut short is refused without reading past it\n");

   /* a chunk whose length takes the offset round past the end, and
    * back to before itself: the same chunk, read again and again */
   memset(crafted, 0, sizeof(crafted));
   put32(crafted + 0, MARKER);
   put32(crafted + 4, CHUNK_VARS);
   put32(crafted + 8, 0xFFFFFFF8u);
   rc = read_exact(&rt, crafted, 32);
   CHECK(rc != RC_OK, "a chunk length that wraps was accepted");
   printf("   ok   a chunk length that wraps the offset is refused\n");

   free(blob);
   rc_runtime_destroy(&rt);
   if (failures)
   {
      printf("FAIL progress_bounds_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS progress_bounds_test\n");
   return 0;
}
