/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <boolean.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <retro_inline.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation phosphor2x_get_implementation
#define softfilter_thread_data phosphor2x_softfilter_thread_data
#define filter_data phosphor2x_filter_data
#endif

#define PHOSPHOR2X_SCALE 2

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   int last;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   float phosphor_bleed;
   float scale_add;
   float scale_times;
   float scanrange_low;
   float scanrange_high;
   float phosphor_bloom_8888[256];
   float phosphor_bloom_565[64];
   float scan_range_8888[256];
   float scan_range_565[64];
};

#define clamp8(x) ((x) > 255 ? 255 : ((x < 0) ? 0 : (uint32_t)x))
#define clamp6(x) ((x) > 63 ? 63 : ((x < 0) ? 0 : (uint32_t)x))

#define red_rgb565(x)              (((x) >> 10) & 0x3e)
#define green_rgb565(x)            (((x) >>  5) & 0x3f)
#define blue_rgb565(x)             (((x) <<  1) & 0x3e)
#define red_xrgb8888(x)            (((x) >> 16) & 0xff)
#define green_xrgb8888(x)          (((x) >>  8) & 0xff)
#define blue_xrgb8888(x)           (((x) >>  0) & 0xff)

#define set_red_rgb565(var, x)     (var = ((var) & 0x07FF)   | ((x&0x3e) << 10))
#define set_green_rgb565(var, x)   (var = ((var) & 0xF81F)   | ((x&0x3f) << 5))
#define set_blue_rgb565(var, x)    (var = ((var) & 0xFFE0)   | ((x&0x3e) >> 1))
#define set_red_xrgb8888(var, x)   (var = ((var) & 0x00ffff) | ((x) << 16))
#define set_green_xrgb8888(var, x) (var = ((var) & 0xff00ff) | ((x) <<  8))
#define set_blue_xrgb8888(var, x)  (var = ((var) & 0xffff00) | ((x) <<  0))

#define blend_pixels_xrgb8888(a, b) (((a >> 1) & 0x7f7f7f7f) + ((b >> 1) & 0x7f7f7f7f))
#define blend_pixels_rgb565(a, b) (((a&0xF7DE) >> 1) + ((b&0xF7DE) >> 1))

static INLINE unsigned max_component_xrgb8888(uint32_t color)
{
   unsigned red   = red_xrgb8888(color);
   unsigned green = green_xrgb8888(color);
   unsigned blue  = blue_xrgb8888(color);
   unsigned max   = red;

   max = (green > max) ? green : max;
   max = (blue > max)  ? blue : max;
   return max;
}

static INLINE unsigned max_component_rgb565(uint32_t color)
{
   unsigned red   = red_rgb565(color);
   unsigned green = green_rgb565(color);
   unsigned blue  = blue_rgb565(color);
   unsigned max   = red;

   max = (green > max) ? green : max;
   max = (blue > max)  ? blue : max;
   return max;
}

static void blit_linear_line_xrgb8888(uint32_t * out,
      const uint32_t *in, unsigned width)
{
   unsigned i;

   /* Splat pixels out on the line. */
   for (i = 0; i < width; i++)
      out[i << 1] = in[i];

   /* Blend in-between pixels. */
   for (i = 1; i < (width << 1) - 1; i += 2)
      out[i] = blend_pixels_xrgb8888(out[i - 1], out[i + 1]);

   /* Blend edge pixels against black. */
   out[0] = blend_pixels_xrgb8888(out[0], 0);
   out[(width << 1) - 1] =
      blend_pixels_xrgb8888(out[(width << 1) - 1], 0);
}

static void blit_linear_line_rgb565(uint16_t * out,
      const uint16_t *in, unsigned width)
{
   unsigned i;

   /* Splat pixels out on the line. */
   for (i = 0; i < width; i++)
      out[i << 1] = in[i];

   /* Blend in-between pixels. */
   for (i = 1; i < (width << 1) - 1; i += 2)
      out[i] =
         blend_pixels_rgb565(out[i - 1], out[i + 1]);

   /* Blend edge pixels against black. */
   out[0] = blend_pixels_rgb565(out[0], 0);
   out[(width << 1) - 1] =
      blend_pixels_rgb565(out[(width << 1) - 1], 0);
}

static void bleed_phosphors_xrgb8888(void *data,
      uint32_t *scanline, unsigned width)
{
   unsigned x;
   struct filter_data *filt = (struct filter_data*)data;

   /* Red phosphor */
   for (x = 0; x < width; x += 2)
   {
      unsigned r = red_xrgb8888(scanline[x]);
      unsigned r_set = clamp8(r * filt->phosphor_bleed *
            filt->phosphor_bloom_8888[r]);
      set_red_xrgb8888(scanline[x + 1], r_set);
   }

   /* Green phosphor */
   for (x = 0; x < width; x++)
   {
      unsigned g = green_xrgb8888(scanline[x]);
      unsigned g_set = clamp8((g >> 1) + 0.5 * g *
            filt->phosphor_bleed * filt->phosphor_bloom_8888[g]);
      set_green_xrgb8888(scanline[x], g_set);
   }

   /* Blue phosphor */
   set_blue_xrgb8888(scanline[0], 0);
   for (x = 1; x < width; x += 2)
   {
      unsigned b = blue_xrgb8888(scanline[x]);
      unsigned b_set = clamp8(b * filt->phosphor_bleed *
            filt->phosphor_bloom_8888[b]);
      set_blue_xrgb8888(scanline[x + 1], b_set);
   }
}

static void bleed_phosphors_rgb565(void *data,
      uint16_t *scanline, unsigned width)
{
   unsigned x;
   struct filter_data *filt = (struct filter_data*)data;

   /* Red phosphor */
   for (x = 0; x < width; x += 2)
   {
      unsigned r = red_rgb565(scanline[x]);
      unsigned r_set = clamp6(r * filt->phosphor_bleed *
            filt->phosphor_bloom_565[r]);
      set_red_rgb565(scanline[x + 1], r_set);
   }

   /* Green phosphor */
   for (x = 0; x < width; x++)
   {
      unsigned g = green_rgb565(scanline[x]);
      unsigned g_set = clamp6((g >> 1) + 0.5 * g *
            filt->phosphor_bleed * filt->phosphor_bloom_565[g]);
      set_green_rgb565(scanline[x], g_set);
   }

   /* Blue phosphor */
   set_blue_rgb565(scanline[0], 0);
   for (x = 1; x < width; x += 2)
   {
      unsigned b = blue_rgb565(scanline[x]);
      unsigned b_set = clamp6(b * filt->phosphor_bleed *
            filt->phosphor_bloom_565[b]);
      set_blue_rgb565(scanline[x + 1], b_set);
   }
}

static unsigned phosphor2x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned phosphor2x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned phosphor2x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *phosphor2x_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   unsigned i;
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));

   (void)simd;
   (void)out_fmt;
   (void)max_width;
   (void)max_height;
   (void)config;
   (void)userdata;

   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)
      calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }

   filt->phosphor_bleed = 0.78;
   filt->scale_add = 1.0;
   filt->scale_times = 0.8;
   filt->scanrange_low = 0.5;
   filt->scanrange_high = 0.65;

#if 0
   /* Initialize lookup tables: */
   phosphorBloom = (scaleTimes .*
         linspace(0, 1, 255) .^ (1/2.2)) + scaleAdd;
   /* Not exactly sure of order of operations here ... */
#endif
   for (i = 0; i < 256; i++)
   {
      filt->phosphor_bloom_8888[i] =
         filt->scale_times * powf((float)i / 255.0f, 1.0f/2.2f) +
         filt->scale_add;
      filt->scan_range_8888[i] =
         filt->scanrange_low + i *
         (filt->scanrange_high - filt->scanrange_low) / 255.0f;
   }
   for (i = 0; i < 64; i++)
   {
      filt->phosphor_bloom_565[i] =
         filt->scale_times * powf((float)i / 31.0f, 1.0f/2.2f)
         + filt->scale_add;
      filt->scan_range_565[i] =
         filt->scanrange_low + i *
         (filt->scanrange_high - filt->scanrange_low) / 31.0f;
   }

   return filt;
}

static void phosphor2x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   (void)data;
   *out_width = width * PHOSPHOR2X_SCALE;
   *out_height = height * PHOSPHOR2X_SCALE;
}

static void phosphor2x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;

   if (!filt)
      return;

   free(filt->workers);
   free(filt);
}

static void phosphor2x_generic_xrgb8888(void *data,
      unsigned width, unsigned height,
      int first, int last, uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned y;
   struct filter_data *filt = (struct filter_data*)data;

   (void)first;
   (void)last;

   memset(dst, 0, height * dst_stride);

   for (y = 0; y < height; y++)
   {
      unsigned x;
      uint32_t *scan_out      = NULL;
      const uint32_t *in_line = (const uint32_t*)(src + y * (src_stride));

      /* output in a scanlines fashion. */
      uint32_t *out_line      = (uint32_t*)(dst + y * (dst_stride) * 2);

      /* Bilinear stretch horizontally. */
      blit_linear_line_xrgb8888(out_line, in_line, width);

      /* Mask 'n bleed phosphors */
      bleed_phosphors_xrgb8888(filt, out_line, width << 1);

      /* Apply scanlines */

      scan_out = (uint32_t*)out_line + (dst_stride);

      for (x = 0; x < (width << 1); x++)
      {
         unsigned max = max_component_xrgb8888(out_line[x]);
         set_red_xrgb8888(scan_out[x],
               (uint32_t)(filt->scan_range_8888[max] *
                  red_xrgb8888(out_line[x])));
         set_green_xrgb8888(scan_out[x],
               (uint32_t)(filt->scan_range_8888[max] *
                  green_xrgb8888(out_line[x])));
         set_blue_xrgb8888(scan_out[x],
               (uint32_t)(filt->scan_range_8888[max] *
                  blue_xrgb8888(out_line[x])));
      }
   }
}

static void phosphor2x_generic_rgb565(void *data,
      unsigned width, unsigned height,
      int first, int last, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned y;
   struct filter_data *filt = (struct filter_data*)data;

   (void)first;
   (void)last;

   memset(dst, 0, height * dst_stride);

   for (y = 0; y < height; y++)
   {
      unsigned x;
      /* Output in a scanlines fashion. */
      uint16_t *scan_out      = NULL;
      uint16_t      *out_line = (uint16_t*)(dst + y * (dst_stride) * 2);
      const uint16_t *in_line = (const uint16_t*)(src + y * (src_stride));

      /* Bilinear stretch horizontally. */
      blit_linear_line_rgb565(out_line, in_line, width);

      /* Mask 'n bleed phosphors. */
      bleed_phosphors_rgb565(filt, out_line, width << 1);

      /* Apply scanlines. */
      scan_out = (uint16_t*)(out_line + (dst_stride));

      for (x = 0; x < (width << 1); x++)
      {
         unsigned max = max_component_rgb565(out_line[x]);
         set_red_rgb565(scan_out[x],
               (uint16_t)(filt->scan_range_565[max] *
                  red_rgb565(out_line[x])));
         set_green_rgb565(scan_out[x],
               (uint16_t)(filt->scan_range_565[max] *
                  green_rgb565(out_line[x])));
         set_blue_rgb565(scan_out[x],
               (uint16_t)(filt->scan_range_565[max] *
                  blue_rgb565(out_line[x])));
      }
   }
}

static void phosphor2x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint32_t *input                    = (uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;

   phosphor2x_generic_xrgb8888(data, width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_XRGB8888),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888));
}

static void phosphor2x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint16_t *input =  (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   phosphor2x_generic_rgb565(data, width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
}

static void phosphor2x_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   unsigned i;
   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr =
         (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start * PHOSPHOR2X_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      /* Workers need to know if they can access pixels
       * outside their given buffer. */
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = phosphor2x_work_cb_rgb565;
#if 0
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB4444)
         packets[i].work = phosphor2x_work_cb_rgb4444;
#endif
      if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = phosphor2x_work_cb_xrgb8888;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation phosphor2x_generic = {
   phosphor2x_generic_input_fmts,
   phosphor2x_generic_output_fmts,

   phosphor2x_generic_create,
   phosphor2x_generic_destroy,

   phosphor2x_generic_threads,
   phosphor2x_generic_output,
   phosphor2x_generic_packets,
   SOFTFILTER_API_VERSION,
   "Phosphor2x",
   "phosphor2x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &phosphor2x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
