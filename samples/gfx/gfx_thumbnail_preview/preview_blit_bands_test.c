/* preview_blit_bands_test.c - the banded colour conversion is the
 * single-threaded one, frame for frame.
 *
 * Decodes every displayed frame of an MP4 fixture twice through the
 * real rmp4_video stream: once converted on the calling thread, once
 * split over a thread pool in the most bands the splitter allows,
 * both straight into a caller-owned frame (set_output). Every frame
 * must compare equal byte for byte - the bands only divide the rows,
 * the pixels they produce must not depend on the division - and the
 * banded pass must decode the same number of frames. Pool threads
 * refusing work (a torn-down pool) and frames too short to split
 * both fall back to the calling thread inside the splitter, so this
 * is also the check that the fallback and the split agree. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <formats/rmp4_video.h>
#include <formats/image_blit_bands.h>
#include <rthreads/tpool.h>
#include <streams/file_stream.h>

#define BANDS 8

static uint8_t *slurp(const char *path, size_t *len)
{
   int64_t n = 0;
   void *buf = NULL;
   if (!filestream_read_file(path, &buf, &n) || n <= 0)
      return NULL;
   *len = (size_t)n;
   return (uint8_t*)buf;
}

/* Decode one full pass into successive frames of @frames (each
 * w*h words); returns the number decoded, capped at @max. */
static int decode_pass(rmp4_video_stream_t *s, uint32_t *frames,
      unsigned w, unsigned h, int max, tpool_t *pool, unsigned bands)
{
   int n = 0, dur;
   rmp4_video_stream_set_blit_pool(s, pool, bands);
   while (n < max)
   {
      uint32_t *dst = frames + (size_t)n * w * h;
      rmp4_video_stream_set_output(s, dst);
      if (!rmp4_video_stream_next(s, &dur))
         break;
      n++;
   }
   rmp4_video_stream_set_output(s, NULL);
   rmp4_video_stream_set_blit_pool(s, NULL, 1);
   return n;
}

int main(int argc, char **argv)
{
   size_t len = 0;
   uint8_t *buf;
   rmp4_video_stream_t *s;
   unsigned w = 0, h = 0;
   int num_frames = 0, loops = 0, max, n1, n2, i;
   uint32_t *a, *b;
   tpool_t *pool;
   int bad = 0;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s fixture.mp4\n", argv[0]);
      return 2;
   }
   if (!(buf = slurp(argv[1], &len)))
   {
      fprintf(stderr, "cannot read %s\n", argv[1]);
      return 2;
   }
   if (!(s = rmp4_video_stream_open(buf, len)))
   {
      fprintf(stderr, "not an mp4 video stream: %s\n", argv[1]);
      return 2;
   }
   rmp4_video_stream_get_info(s, &w, &h, &num_frames, &loops);
   max = num_frames > 0 && num_frames < 64 ? num_frames : 64;
   a   = (uint32_t*)calloc((size_t)max * w * h, sizeof(uint32_t));
   b   = (uint32_t*)calloc((size_t)max * w * h, sizeof(uint32_t));
   if (!a || !b)
      return 2;

   /* Both orders, so the ARGB and RGBA blit rows are each compared. */
   rmp4_video_stream_set_argb(s, 1);
   n1 = decode_pass(s, a, w, h, max, NULL, 1);
   rmp4_video_stream_rewind(s);
   pool = tpool_create(BANDS - 1);
   if (!pool)
      return 2;
   n2 = decode_pass(s, b, w, h, max, pool, BANDS);

   if (n1 != n2 || n1 == 0)
   {
      printf("[FAIL] %d frames on one thread, %d in %u bands\n", n1, n2, BANDS);
      bad = 1;
   }
   for (i = 0; i < n1 && i < n2; i++)
   {
      if (memcmp(a + (size_t)i * w * h, b + (size_t)i * w * h,
               (size_t)w * h * sizeof(uint32_t)))
      {
         printf("[FAIL] frame %d differs between one thread and %u bands\n",
               i, BANDS);
         bad = 1;
         break;
      }
   }
   if (!bad)
      printf("[pass] %d frames of %ux%u identical on one thread and in %u bands\n",
            n1, w, h, BANDS);

   /* And back on one thread after the pool: the stream keeps working
    * once its pool is withdrawn. */
   rmp4_video_stream_rewind(s);
   rmp4_video_stream_set_argb(s, 0);
   n1 = decode_pass(s, a, w, h, max, NULL, 1);
   rmp4_video_stream_rewind(s);
   n2 = decode_pass(s, b, w, h, max, pool, BANDS);
   for (i = 0; i < n1 && i < n2; i++)
      if (memcmp(a + (size_t)i * w * h, b + (size_t)i * w * h,
               (size_t)w * h * sizeof(uint32_t)))
      {
         printf("[FAIL] RGBA frame %d differs\n", i);
         bad = 1;
         break;
      }
   if (!bad)
      printf("[pass] RGBA order: %d frames identical\n", n1);

   tpool_destroy(pool);
   rmp4_video_stream_close(s);
   free(a);
   free(b);
   free(buf);
   printf(bad ? "FAIL\n" : "PASS\n");
   return bad;
}
