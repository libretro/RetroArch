/* preview_decode_bench.c -- frames per second through the preview
 * stream, per codec and per thread count.
 *
 * The animated-thumbnail decoders parallelise three ways - colour
 * conversion in row bands, VP9 tile columns, HEVC wavefront rows - and
 * the question "is 4K fast enough" has been answered by feel. This
 * answers it in numbers: for each file given, decode every frame
 * through the same stream the thumbnail uses, at 1, 2 and 4 threads,
 * and print frames per second, the megapixels per second that
 * implies, and the speed-up over one thread. With --catchup the run is
 * made with the stream told it is behind, so the gain from dropping
 * non-reference pictures is measured beside the gain from threads.
 *
 * Not a test: it prints and exits 0 unless a decode fails outright.
 * Numbers from a sanitized build mean nothing; build it plain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/image.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <rthreads/tpool.h>

static enum image_type_enum type_of(const char *path)
{
   const char *dot = strrchr(path, '.');
   if (!dot)
      return IMAGE_TYPE_NONE;
   if (!strcmp(dot, ".mp4") || !strcmp(dot, ".m4v") || !strcmp(dot, ".mov"))
      return IMAGE_TYPE_MP4;
   if (!strcmp(dot, ".webm") || !strcmp(dot, ".mkv"))
      return IMAGE_TYPE_WEBM;
   if (!strcmp(dot, ".webp"))
      return IMAGE_TYPE_WEBP;
   if (!strcmp(dot, ".png"))
      return IMAGE_TYPE_PNG;
   return IMAGE_TYPE_NONE;
}

/* Decode the whole file once; returns frames decoded, or -1. */
static int run(const uint8_t *buf, size_t len, enum image_type_enum type,
      unsigned threads, int catchup, unsigned *w, unsigned *h,
      int64_t *usec)
{
   void *s = image_transfer_anim_stream_new((void*)buf, len, type);
   tpool_t *pool = NULL;
   int n = 0, nf = 0, loops = 0, dur;
   int64_t t0;
   const uint32_t *px;

   if (!s)
      return -1;
   image_transfer_anim_stream_get_info(s, type, w, h, &nf, &loops);
   if (threads > 1)
   {
      pool = tpool_create_with_stack_size((size_t)(threads - 1), 512 * 1024);
      if (pool)
         image_transfer_anim_stream_set_blit_pool(s, type, pool, threads);
   }
   if (catchup)
      image_transfer_anim_stream_set_catchup(s, type, 1);

   t0 = cpu_features_get_time_usec();
   while (1)
   {
      int64_t f0 = cpu_features_get_time_usec(), f1;
      px = image_transfer_anim_stream_next(s, type, &dur);
      f1 = cpu_features_get_time_usec();
      if (!px)
         break;
      n++;
      /* BENCH_LOG: name the frames that took longer than a third of a
       * second, which is what a viewer sees as a stall. */
      if (getenv("BENCH_LOG") && f1 - f0 > 300000)
         fprintf(stderr, "      frame %d took %.0f ms\n", n,
               (double)(f1 - f0) / 1000.0);
      if (n > 100000)
         break;
      /* BENCH_MAX: stop after this many frames, for profiling a start. */
      if (getenv("BENCH_MAX") && n >= atoi(getenv("BENCH_MAX")))
         break;
   }
   *usec = cpu_features_get_time_usec() - t0;

   image_transfer_anim_stream_free(s, type);
   if (pool)
      tpool_destroy(pool);
   return n;
}

int main(int argc, char **argv)
{
   int catchup = 0, i, rc = 0;
   static const unsigned counts[] = { 1, 2, 4 };

   if (argc < 2)
   {
      printf("usage: %s [--catchup] file...\n", argv[0]);
      return 2;
   }
   for (i = 1; i < argc; i++)
   {
      int64_t len = 0;
      void *buf   = NULL;
      enum image_type_enum type;
      double base_fps = 0.0;
      unsigned c;

      if (!strcmp(argv[i], "--catchup"))
      {
         catchup = 1;
         continue;
      }
      type = type_of(argv[i]);
      if (type == IMAGE_TYPE_NONE)
      {
         printf("%s: unknown type\n", argv[i]);
         continue;
      }
      if (!filestream_read_file(argv[i], &buf, &len) || !buf || len <= 0)
      {
         printf("%s: cannot read\n", argv[i]);
         rc = 1;
         continue;
      }
      printf("%s%s\n", argv[i], catchup ? " (catch-up: droppable pictures skipped)" : "");
      for (c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
      {
         unsigned w = 0, h = 0;
         int64_t us = 0;
         int n = run((const uint8_t*)buf, (size_t)len, type, counts[c],
               catchup, &w, &h, &us);
         double fps, mpps;
         if (n < 0)
         {
            printf("   %u thread(s): decode failed\n", counts[c]);
            rc = 1;
            continue;
         }
         fps  = us > 0 ? (double)n * 1e6 / (double)us : 0.0;
         mpps = fps * (double)w * (double)h / 1e6;
         if (counts[c] == 1)
            base_fps = fps;
         printf("   %u thread(s): %4d frames %ux%u in %6.0f ms = %6.1f fps, "
               "%6.1f Mpix/s, x%.2f\n", counts[c], n, w, h,
               (double)us / 1000.0, fps, mpps,
               base_fps > 0.0 ? fps / base_fps : 1.0);
      }
      free(buf);
   }
   return rc;
}
