/* rchd_hunk_cache: a hunk read with no buffer is read where the decoder
 * built it.
 *
 * An uncompressed version 5 image is written in memory by rchd's own
 * writer - one hunk of zeros, stored as a hole, and a last hunk the
 * image ends partway through - and opened from memory. Every hunk is
 * read three ways and has to be the bytes it was written from:
 *
 *  - into a buffer (rchd_read_hunk_begin with a destination),
 *  - in place (a NULL destination, then rchd_hunk_cache), and
 *  - in place again right after, which the cache answers without
 *    asking for a byte of the image;
 *
 * and a byte read of part of a hunk still lands in its own buffer. A
 * byte read (rchd_read_begin) still refuses a NULL destination.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rchd.h>

#define UNIT   2048u
#define HUNK   (8u * UNIT)
#define HUNKS  6u
/* the image ends 3 units into its last hunk */
#define LOGICAL ((uint64_t)(HUNKS - 1) * HUNK + 3u * UNIT)

static int fails;

#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s\n", what); fails++; } } while (0)

static uint8_t *image;
static size_t   image_len;
static uint8_t  src[HUNKS * HUNK];
static unsigned fed_bytes;

static int sink(void *ctx, uint64_t offset, const void *data, size_t len)
{
   (void)ctx;
   if (offset + len > image_len)
   {
      uint8_t *grown = (uint8_t*)realloc(image, (size_t)(offset + len));
      if (!grown)
         return 0;
      memset(grown + image_len, 0, (size_t)(offset + len) - image_len);
      image     = grown;
      image_len = (size_t)(offset + len);
   }
   memcpy(image + offset, data, len);
   return 1;
}

/* Satisfies whatever the decoder asks for, from the image in memory */
static int drive(rchd_t *chd, int opening)
{
   for (;;)
   {
      rchd_request_t req;
      int err = opening ? rchd_open_step(chd, &req) : rchd_read_step(chd, &req);
      if (err != RCHD_PENDING)
         return err;
      if (req.source != RCHD_SOURCE_SELF || req.offset + req.length > image_len)
         return RCHD_ERROR_DATA;
      fed_bytes += req.length;
      if (rchd_feed(chd, image + req.offset, req.length) != RCHD_OK)
         return RCHD_ERROR_DATA;
   }
}

int main(void)
{
   rchd_writer_t *w;
   rchd_t        *chd;
   uint8_t       *buf;
   uint8_t        part[100];
   unsigned       h, i;

   for (i = 0; i < sizeof(src); i++)
      src[i] = (uint8_t)(i * 7u + (i >> 11));
   /* hunk 2 is all zeros: stored as a hole */
   memset(src + 2 * HUNK, 0, HUNK);
   /* past the logical end the last hunk is padding */
   memset(src + LOGICAL, 0, sizeof(src) - (size_t)LOGICAL);

   w = rchd_write_new(LOGICAL, HUNK, UNIT, sink, NULL);
   CHECK(w != NULL, "a writer");
   if (!w)
      return 1;
   for (h = 0; h < HUNKS; h++)
   {
      uint32_t len = (h == HUNKS - 1) ? (uint32_t)(LOGICAL - (uint64_t)h * HUNK) : HUNK;
      CHECK(rchd_write_hunk(w, src + h * HUNK, len) == RCHD_OK, "a hunk written");
   }
   CHECK(rchd_write_finish(w) == RCHD_OK, "the image finished");
   rchd_write_free(w);

   chd = rchd_new();
   buf = (uint8_t*)malloc(HUNK);
   CHECK(chd && buf, "a decoder");
   if (!chd || !buf)
      return 1;
   CHECK(drive(chd, 1) == RCHD_OK, "the image opens");
   CHECK(rchd_info(chd)->hunk_count == HUNKS, "every hunk");
   CHECK(rchd_hunk_cache(NULL) == NULL, "no decoder, no cache");

   for (h = 0; h < HUNKS; h++)
   {
      char what[96];
      const uint8_t *in_place;
      unsigned before;

      /* into a buffer */
      memset(buf, 0xA5, HUNK);
      CHECK(rchd_read_hunk_begin(chd, h, buf) == RCHD_OK && drive(chd, 0) == RCHD_OK,
            "a hunk read into a buffer");
      sprintf(what, "hunk %u into a buffer is the hunk written", h);
      CHECK(!memcmp(buf, src + h * HUNK, HUNK), what);

      /* in place */
      CHECK(rchd_read_hunk_begin(chd, h, NULL) == RCHD_OK && drive(chd, 0) == RCHD_OK,
            "a hunk read with no buffer");
      in_place = rchd_hunk_cache(chd);
      sprintf(what, "hunk %u in place is the hunk written", h);
      CHECK(in_place && !memcmp(in_place, src + h * HUNK, HUNK), what);
      CHECK(rchd_read_progress(chd) == HUNK, "the read went the whole hunk");

      /* in place again: the cache has it, nothing of the image is asked */
      before = fed_bytes;
      CHECK(rchd_read_hunk_begin(chd, h, NULL) == RCHD_OK && drive(chd, 0) == RCHD_OK
            && rchd_hunk_cache(chd) == in_place && fed_bytes == before,
            "the same hunk again comes from the cache");

      /* part of the hunk, into its own buffer */
      memset(part, 0x5A, sizeof(part));
      CHECK(rchd_read_begin(chd, (uint64_t)h * HUNK + 1000u, part, sizeof(part)) == RCHD_OK
            && drive(chd, 0) == RCHD_OK
            && !memcmp(part, src + h * HUNK + 1000u, sizeof(part)),
            "a byte read still copies into its buffer");
   }

   CHECK(rchd_read_hunk_begin(chd, HUNKS, NULL) == RCHD_ERROR_PARAM,
         "a hunk past the end is refused");
   CHECK(rchd_read_begin(chd, 0, NULL, 16) != RCHD_OK,
         "a byte read with no buffer is refused");

   rchd_free(chd);
   free(buf);
   free(image);
   if (fails)
   {
      printf("rchd_hunk_cache_test: %d FAIL\n", fails);
      return 1;
   }
   printf("rchd_hunk_cache_test: PASS\n");
   return 0;
}
