/* preview_blit_bands_test.c - the threaded decode is the
 * single-threaded one, frame for frame.
 *
 * Decodes every displayed frame of an MP4 or WebM fixture twice
 * through the real video stream: once entirely on the calling
 * thread, once with a thread pool set on the stream - the colour
 * conversion split into as many row bands as the splitter allows
 * and, for a VP9 stream coded in tile columns, the columns decoded
 * side by side - both straight into a caller-owned frame
 * (set_output). Every frame must compare equal byte for byte: the
 * threads only divide the work, the pixels they produce must not
 * depend on the division - and the threaded pass must decode the same
 * number of frames. Pool threads refusing work (a torn-down pool) and
 * frames too short to split both fall back to the calling thread, so
 * this is also the check that the fallback and the split agree. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <formats/rmp4_video.h>
#include <formats/rwebm_video.h>
#include <formats/image_blit_bands.h>
#include <rthreads/tpool.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>
#include <formats/rh265.h>

#define BANDS 8

/* The two containers behind one shape. */
typedef struct
{
   rmp4_video_stream_t  *mp4;
   rwebm_video_stream_t *webm;
} vstream;

static bool vs_open(vstream *v, const char *path, const uint8_t *buf, size_t len)
{
   v->mp4  = NULL;
   v->webm = NULL;
   if (string_ends_with(path, ".webm"))
      v->webm = rwebm_video_stream_open(buf, len);
   else
      v->mp4  = rmp4_video_stream_open(buf, len);
   return v->mp4 || v->webm;
}
static void vs_close(vstream *v)
{
   if (v->mp4)  rmp4_video_stream_close(v->mp4);
   if (v->webm) rwebm_video_stream_close(v->webm);
}
static void vs_info(vstream *v, unsigned *w, unsigned *h, int *n, int *l)
{
   if (v->mp4)  rmp4_video_stream_get_info(v->mp4, w, h, n, l);
   else         rwebm_video_stream_get_info(v->webm, w, h, n, l);
}
static void vs_rewind(vstream *v)
{
   if (v->mp4)  rmp4_video_stream_rewind(v->mp4);
   else         rwebm_video_stream_rewind(v->webm);
}
static void vs_argb(vstream *v, int argb)
{
   if (v->mp4)  rmp4_video_stream_set_argb(v->mp4, argb);
   else         rwebm_video_stream_set_argb(v->webm, argb);
}
static void vs_output(vstream *v, uint32_t *out)
{
   if (v->mp4)  rmp4_video_stream_set_output(v->mp4, out);
   else         rwebm_video_stream_set_output(v->webm, out);
}
static void vs_pool(vstream *v, void *pool, unsigned bands)
{
   if (v->mp4)  rmp4_video_stream_set_blit_pool(v->mp4, pool, bands);
   else         rwebm_video_stream_set_blit_pool(v->webm, pool, bands);
   /* the banded decode also rotates the HEVC decoder's contexts: the
    * pictures still decode one after the other, so the frames must
    * match the one-thread decode to the byte - a difference is
    * picture state left in the decoder rather than the context */
   if (v->mp4 && bands > 1)
   {
      void *h265 = rmp4_video_stream_h265(v->mp4);
      if (h265)
         rh265_video_set_contexts((rh265_video*)h265, 4);
      /* and, RH265_PUBLISH_DELAY set, every row's publication held
       * back at random so the readers wait for their rows */
      if (getenv("RH265_PUBLISH_DELAY"))
         rh265_video_set_publish_delay(atoi(getenv("RH265_PUBLISH_DELAY")));
   }
}
static const uint32_t *vs_next(vstream *v, int *dur)
{
   if (v->mp4)  return rmp4_video_stream_next(v->mp4, dur);
   return rwebm_video_stream_next(v->webm, dur);
}

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
static int decode_pass(vstream *s, uint32_t *frames,
      unsigned w, unsigned h, int max, tpool_t *pool, unsigned bands)
{
   int n = 0, dur;
   vs_pool(s, pool, bands);
   while (n < max)
   {
      uint32_t *dst = frames + (size_t)n * w * h;
      vs_output(s, dst);
      if (!vs_next(s, &dur))
         break;
      n++;
   }
   vs_output(s, NULL);
   vs_pool(s, NULL, 1);
   return n;
}

int main(int argc, char **argv)
{
   size_t len = 0;
   uint8_t *buf;
   vstream s;
   unsigned w = 0, h = 0;
   int num_frames = 0, loops = 0, max, n1, n2, i;
   uint32_t *a, *b;
   tpool_t *pool;
   int bad = 0;
   int64_t t0, t1, t2, t3;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s fixture.mp4|fixture.webm\n", argv[0]);
      return 2;
   }
   if (!(buf = slurp(argv[1], &len)))
   {
      fprintf(stderr, "cannot read %s\n", argv[1]);
      return 2;
   }
   if (!vs_open(&s, argv[1], buf, len))
   {
      fprintf(stderr, "not a video stream: %s\n", argv[1]);
      return 2;
   }
   vs_info(&s, &w, &h, &num_frames, &loops);
   max = num_frames > 0 && num_frames < 64 ? num_frames : 64;
   a   = (uint32_t*)calloc((size_t)max * w * h, sizeof(uint32_t));
   b   = (uint32_t*)calloc((size_t)max * w * h, sizeof(uint32_t));
   if (!a || !b)
      return 2;

   /* Both orders, so the ARGB and RGBA blit rows are each compared.
    * The two passes are timed as well: the ratio is what the threads
    * buy on this machine for this stream, printed for whoever runs
    * the test by hand - the check itself is only the comparison. */
   vs_argb(&s, 1);
   t0 = cpu_features_get_time_usec();
   n1 = decode_pass(&s, a, w, h, max, NULL, 1);
   t1 = cpu_features_get_time_usec();
   vs_rewind(&s);
   pool = tpool_create(BANDS - 1);
   if (!pool)
      return 2;
   t2 = cpu_features_get_time_usec();
   n2 = decode_pass(&s, b, w, h, max, pool, BANDS);
   t3 = cpu_features_get_time_usec();
   if (n1 > 0 && n2 > 0)
      printf("[time] %.2f ms/frame on one thread, %.2f ms/frame on %u\n",
            (double)(t1 - t0) / 1000.0 / n1,
            (double)(t3 - t2) / 1000.0 / n2, BANDS);

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
      printf("[pass] %s: %d frames of %ux%u identical on one thread and on %u\n",
            argv[1], n1, w, h, BANDS);

   /* And back on one thread after the pool: the stream keeps working
    * once its pool is withdrawn. */
   vs_rewind(&s);
   vs_argb(&s, 0);
   n1 = decode_pass(&s, a, w, h, max, NULL, 1);
   vs_rewind(&s);
   n2 = decode_pass(&s, b, w, h, max, pool, BANDS);
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
   vs_close(&s);
   free(a);
   free(b);
   free(buf);
   /* The HEVC row counter every reference read consults: on one thread
    * and on the wavefront a reference is complete before it is read, so
    * no read may find its rows short. A short read would be a wrong
    * counter or a wrong bound, and a decoder with pictures in flight
    * would then wait for a row it had been handed already. */
   if (rh265_video_ref_wait_misses())
   {
      printf("[FAIL] %d HEVC reference reads short of their rows\n",
            rh265_video_ref_wait_misses());
      bad = 1;
   }
   printf(bad ? "FAIL\n" : "PASS\n");
   return bad;
}
