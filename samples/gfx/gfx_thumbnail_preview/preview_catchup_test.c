/* preview_catchup_test.c -- behind the clock, the stream drops only what
 * nothing references, and what it still shows is the full decode.
 *
 * The thumbnail scheduler asks the stream to catch up when a frame lands
 * after the next one was due. For H.264 that means passing over
 * non-reference pictures (nal_ref_idc 0); for HEVC, sub-layer
 * non-reference pictures in the highest sub-layer. Nothing predicts from
 * those, so the pictures that remain must decode to exactly what a full
 * run produces - a single differing byte would mean a dropped picture
 * was referenced after all, which is the one way this can go wrong.
 *
 * Two runs over each fixture: the full decode as the reference, then a
 * run with catch-up set from the first frame. Every frame of the second
 * run must appear in the first, byte for byte, in increasing order; and
 * there must be fewer of them, or the mode did nothing. The fixtures
 * carry three non-reference B-frames between references and no
 * B-pyramid, so the second run shows about a third of the first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/image.h>
#include <streams/file_stream.h>
#include <encodings/crc32.h>
#ifdef PREVIEW_THREADED
#include <rthreads/tpool.h>
/* The threaded build decodes every run with four pictures in flight
 * on a pool: the drops must then leave the decoder exactly as they
 * do one picture at a time. */
static void *g_pool;
#endif

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("[FAIL] "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

/* CRC of every frame the stream yields, in order. */
static uint32_t *run(const uint8_t *buf, size_t len, int catchup,
      int *count, unsigned *w, unsigned *h)
{
   void *s = image_transfer_anim_stream_new((void*)buf, len, IMAGE_TYPE_MP4);
   uint32_t *crcs = NULL;
   int n = 0, cap = 0, loops = 0, dur;
   const uint32_t *px;
   *count = 0;
   if (!s)
      return NULL;
   image_transfer_anim_stream_get_info(s, IMAGE_TYPE_MP4, w, h, &n, &loops);
   n = 0;
   image_transfer_anim_stream_set_argb(s, IMAGE_TYPE_MP4, 1);
#ifdef PREVIEW_THREADED
   if (g_pool)
      image_transfer_anim_stream_set_blit_pool(s, IMAGE_TYPE_MP4, g_pool, 4);
#endif
   if (catchup)
      image_transfer_anim_stream_set_catchup(s, IMAGE_TYPE_MP4, 1);
   while ((px = image_transfer_anim_stream_next(s, IMAGE_TYPE_MP4, &dur)))
   {
      if (n >= cap)
      {
         cap  = cap ? cap * 2 : 64;
         crcs = (uint32_t*)realloc(crcs, cap * sizeof(*crcs));
         if (!crcs)
            break;
      }
      crcs[n++] = encoding_crc32(0, (const uint8_t*)px,
            (size_t)*w * *h * sizeof(uint32_t));
      if (n > 4096)
         break; /* a stream that never ends is its own failure */
   }
   image_transfer_anim_stream_free(s, IMAGE_TYPE_MP4);
   *count = n;
   return crcs;
}

static int check_fixture(const char *path)
{
   int64_t len = 0;
   void *buf   = NULL;
   uint32_t *full, *fast;
   int nfull = 0, nfast = 0, i, cursor = 0;
   unsigned w = 0, h = 0, w2 = 0, h2 = 0;
   unsigned had = failures;

   if (!filestream_read_file(path, &buf, &len) || !buf || len <= 0)
   {
      printf("[FAIL] cannot read %s\n", path);
      failures++;
      return 1;
   }
   full = run((const uint8_t*)buf, (size_t)len, 0, &nfull, &w, &h);
   fast = run((const uint8_t*)buf, (size_t)len, 1, &nfast, &w2, &h2);
   free(buf);

   CHECK(full && nfull > 10, "%s: full decode yielded %d frames", path, nfull);
   CHECK(fast && nfast > 0,  "%s: catch-up decode yielded nothing", path);
   CHECK(w == w2 && h == h2, "%s: dimensions differ between runs", path);
   if (!full || !fast)
   {
      free(full);
      free(fast);
      return 1;
   }

   /* Every shown frame is a frame of the full run, in order. */
   for (i = 0; i < nfast; i++)
   {
      while (cursor < nfull && full[cursor] != fast[i])
         cursor++;
      if (cursor >= nfull)
      {
         CHECK(0, "%s: catch-up frame %d matches no frame of the full "
               "decode after index %d - a dropped picture was referenced",
               path, i, i ? cursor : 0);
         break;
      }
      cursor++;
   }
   /* And it dropped something: with three non-reference B-frames per
    * reference the shown count is around a third of the full one. */
   CHECK(nfast < nfull, "%s: catch-up showed all %d frames; nothing was "
         "dropped", path, nfull);
   CHECK(nfast * 2 < nfull, "%s: catch-up showed %d of %d - the droppable "
         "pictures were not all passed over", path, nfast, nfull);

   if (failures == had)
      printf("[pass] %s: %d of %d frames shown, every one a frame of the "
            "full decode\n", path, nfast, nfull);
   free(full);
   free(fast);
   return failures != had;
}

int main(int argc, char **argv)
{
   int i;
   if (argc < 2)
   {
      printf("usage: %s fixture.mp4 [...]\n", argv[0]);
      return 2;
   }
#ifdef PREVIEW_THREADED
   g_pool = tpool_create_with_stack_size(3, 512 * 1024);
#endif
   for (i = 1; i < argc; i++)
      check_fixture(argv[i]);
   printf(failures ? "FAIL\n" : "PASS\n");
   return failures ? 1 : 0;
}
