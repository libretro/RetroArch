/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "softfilter.h"
#include <stdlib.h>
#include "boolean.h"

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation blargg_ntsc_snes_composite_get_implementation
#endif

#include "snes_ntsc/snes_ntsc.h"
#include "snes_ntsc/snes_ntsc.c"

void filter_size(unsigned*, unsigned*);
void filter_render(uint32_t*, uint32_t*, unsigned, const uint16_t*, unsigned, unsigned, unsigned);

struct snes_ntsc_t *ntsc;
snes_ntsc_setup_t setup;
int burst;
int burst_toggle;

static unsigned blargg_ntsc_snes_composite_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned blargg_ntsc_snes_composite_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned blargg_ntsc_snes_composite_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *blargg_ntsc_snes_composite_generic_create(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd)
{
   (void)simd;

   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads = threads;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   return filt;
}

static void blargg_ntsc_snes_composite_initialize(void)
{
   static bool initialized = false;
   if(initialized == true)
      return;
   initialized = true;

   ntsc = (snes_ntsc_t*)malloc(sizeof *ntsc);
   setup = snes_ntsc_composite;
   setup.merge_fields = 1;
   snes_ntsc_init(ntsc, &setup);

   burst = 0;
   burst_toggle = (setup.merge_fields ? 0 : 1);
}

void terminate(void)
{
   if(ntsc)
      free(ntsc);
}

static void blargg_ntsc_snes_composite_generic_output(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   blargg_ntsc_snes_composite_initialize();
   *out_width  = SNES_NTSC_OUT_WIDTH(256);
   *out_height = height;
}

static void blargg_ntsc_snes_composite_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   free(filt->workers);
   free(filt);
}

static void blargg_ntsc_snes_composite_render_rgb565(int width, int height,
      int first, int last,
      uint16_t *input, int pitch, uint16_t *output, int outpitch)
{
   blargg_ntsc_snes_composite_initialize();
   if(!ntsc)
      return;

   pitch >>= 1;
   outpitch >>= 1;

   if(width <= 256)
      snes_ntsc_blit(ntsc, input, pitch, burst, width, height, output, outpitch * 2, first, last);
   else
      snes_ntsc_blit_hires(ntsc, input, pitch, burst, width, height, output, outpitch * 2, first, last);

   burst ^= burst_toggle;
}

static void blargg_ntsc_snes_composite_rgb565(unsigned width, unsigned height,
      int first, int last, uint16_t *src, 
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   blargg_ntsc_snes_composite_render_rgb565(width, height,
         first, last,
         src, src_stride,
         dst, dst_stride);

}

static void blargg_ntsc_snes_composite_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   blargg_ntsc_snes_composite_rgb565(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_RGB565, output, thr->out_pitch / SOFTFILTER_BPP_RGB565);
}

static void blargg_ntsc_snes_composite_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   unsigned i;
   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      // Workers need to know if they can access pixels outside their given buffer.
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = blargg_ntsc_snes_composite_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation blargg_ntsc_snes_composite_generic = {
   blargg_ntsc_snes_composite_generic_input_fmts,
   blargg_ntsc_snes_composite_generic_output_fmts,

   blargg_ntsc_snes_composite_generic_create,
   blargg_ntsc_snes_composite_generic_destroy,

   blargg_ntsc_snes_composite_generic_threads,
   blargg_ntsc_snes_composite_generic_output,
   blargg_ntsc_snes_composite_generic_packets,
   "Blargg NTSC SNES Composite",
   SOFTFILTER_API_VERSION,
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   (void)simd;
   return &blargg_ntsc_snes_composite_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#endif
