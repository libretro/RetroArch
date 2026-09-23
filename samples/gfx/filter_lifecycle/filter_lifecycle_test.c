/* The softfilter worker pool's lifecycle, under the ownership order
 * free() relies on: pool joined first, then packets, impl_data and
 * plugin libraries - the resources workers touch must outlive the
 * last worker.
 *
 * Compiles the shipping video_filter.c with the builtin filters and
 * a forced thread count, then walks the full lifecycle repeatedly:
 * create (Normal2x, XRGB8888, 4 worker threads), process frames
 * whose output is exactly checkable (Normal2x maps every source
 * pixel to a 2x2 block), free. The create/free cycling is the
 * regression surface for the teardown reorder: every cycle joins a
 * live pool before touching what the workers borrow, and any
 * misorder that lets a worker outlive packets/impl_data shows up
 * under this loop as a crash or a wrong block. A final lane creates
 * with a nonexistent filter path and expects a clean NULL - the
 * error path that runs free() on a partially built object.
 *
 *   samples/gfx/filter_lifecycle/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>

#include "../../../gfx/video_filter.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

#define SRC_W 96
#define SRC_H 64

static uint32_t src[SRC_W * SRC_H];
static uint32_t dst[(SRC_W * 2) * (SRC_H * 2)];

static uint32_t px(unsigned x, unsigned y)
{
   return 0xFF000000u | (x * 2654435761u + y * 40503u);
}

int main(int argc, char *argv[])
{
   unsigned cycle;
   const char *filt_path = "../../../gfx/video_filters/Normal2x.filt";

   (void)argc;
   (void)argv;

   for (cycle = 0; cycle < 8; cycle++)
   {
      rarch_softfilter_t *filt;
      unsigned ow = 0, oh = 0, x, y;

      filt = rarch_softfilter_new(filt_path, 4,
            RETRO_PIXEL_FORMAT_XRGB8888, VIDEO_SCALE_PACK(SRC_W, SRC_H));
      CHECK(filt != NULL, "cycle %u: create failed", cycle);
      if (!filt)
         break;

      {
         unsigned od = 0;
         rarch_softfilter_get_output_size(filt, &od,
            VIDEO_SCALE_PACK(SRC_W, SRC_H));
         ow = VIDEO_SCALE_W(od);
         oh = VIDEO_SCALE_H(od);
      }
      CHECK(ow == SRC_W * 2 && oh == SRC_H * 2,
            "cycle %u: output size %ux%u, want %ux%u",
            cycle, ow, oh, SRC_W * 2, SRC_H * 2);

      for (y = 0; y < SRC_H; y++)
         for (x = 0; x < SRC_W; x++)
            src[y * SRC_W + x] = px(x + cycle, y);

      memset(dst, 0, sizeof(dst));
      rarch_softfilter_process(filt, dst, ow * sizeof(uint32_t),
            src, VIDEO_SCALE_PACK(SRC_W, SRC_H), SRC_W * sizeof(uint32_t));

      /* Every source pixel is a 2x2 block in the output; spot the
       * corners and a scatter so all worker slices are covered. */
      for (y = 0; y < SRC_H; y += 7)
         for (x = 0; x < SRC_W; x += 5)
         {
            uint32_t want = px(x + cycle, y);
            uint32_t a    = dst[(y * 2)     * ow + x * 2];
            uint32_t b    = dst[(y * 2)     * ow + x * 2 + 1];
            uint32_t c    = dst[(y * 2 + 1) * ow + x * 2];
            uint32_t d    = dst[(y * 2 + 1) * ow + x * 2 + 1];
            if (a != want || b != want || c != want || d != want)
            {
               CHECK(0, "cycle %u: block (%u,%u) wrong", cycle, x, y);
               y = SRC_H; /* one report per cycle */
               break;
            }
         }

      /* A second frame through the same pool - workers are reused,
       * not one-shot. */
      rarch_softfilter_process(filt, dst, ow * sizeof(uint32_t),
            src, VIDEO_SCALE_PACK(SRC_W, SRC_H), SRC_W * sizeof(uint32_t));

      rarch_softfilter_free(filt);
   }

   /* Error path: free() of a partially built filter must be clean. */
   CHECK(rarch_softfilter_new("/nonexistent/no_such.filt", 4,
            RETRO_PIXEL_FORMAT_XRGB8888, VIDEO_SCALE_PACK(SRC_W, SRC_H)) == NULL,
         "bogus path: expected NULL");

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("filter_lifecycle: all lanes passed (8 cycles, 4 workers)\n");
   return 0;
}
