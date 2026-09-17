/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_blit_bands.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <formats/image_blit_bands.h>

#ifdef HAVE_THREADS
#include <rthreads/tpool.h>
#endif

/* Fewer rows than this per band and the hand-off costs more than the
 * rows; a 480-line frame splits in two, a 1080-line one in up to
 * eight. */
#define IMAGE_BLIT_BAND_MIN_ROWS 128
#define IMAGE_BLIT_BANDS_MAX     8

#ifdef HAVE_THREADS
typedef struct
{
   image_blit_rows_t fn;
   void *ctx;
   unsigned row0;
   unsigned rows;
} image_blit_band_t;

static void image_blit_band_run(void *arg)
{
   image_blit_band_t *b = (image_blit_band_t*)arg;
   b->fn(b->ctx, b->row0, b->rows);
}
#endif

void image_blit_bands(void *pool, unsigned bands, unsigned h,
      unsigned align, image_blit_rows_t fn, void *ctx)
{
#ifdef HAVE_THREADS
   image_blit_band_t band[IMAGE_BLIT_BANDS_MAX];
   unsigned i, row, per;

   if (!align)
      align = 1;
   if (bands > IMAGE_BLIT_BANDS_MAX)
      bands = IMAGE_BLIT_BANDS_MAX;
   if (pool && bands > 1 && h / bands < IMAGE_BLIT_BAND_MIN_ROWS)
      bands = h / IMAGE_BLIT_BAND_MIN_ROWS;

   if (!pool || bands <= 1)
   {
      fn(ctx, 0, h);
      return;
   }

   /* Every band but the last starts on an aligned row; the last takes
    * whatever remains. */
   per = ((h / bands) / align) * align;
   for (i = 0, row = 0; i < bands; i++)
   {
      band[i].fn   = fn;
      band[i].ctx  = ctx;
      band[i].row0 = row;
      band[i].rows = (i == bands - 1) ? h - row : per;
      row         += band[i].rows;
   }

   for (i = 0; i < bands - 1; i++)
   {
      /* A refused post (the pool is being torn down) is run here: the
       * frame is always complete when this returns. */
      if (!tpool_add_work((tpool_t*)pool, image_blit_band_run, &band[i]))
         band[i].fn(band[i].ctx, band[i].row0, band[i].rows);
   }
   band[bands - 1].fn(ctx, band[bands - 1].row0, band[bands - 1].rows);
   tpool_wait((tpool_t*)pool);
#else
   (void)pool; (void)bands; (void)align;
   fn(ctx, 0, h);
#endif
}
