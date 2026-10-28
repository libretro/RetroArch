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
   struct softfilter_thread_data *workers;
   unsigned threads;
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
   /* What the float expressions below give for every input they can
    * see, each worked out once at create with the same expression, so
    * the lines look the results up instead of converting, multiplying
    * and truncating per channel per pixel. bleed_rb is the red and blue
    * bleed of a channel value, bleed_g the green; scan is the scanline
    * level of a channel value c in a pixel whose brightest channel is
    * max, at [max][c]. */
   uint8_t bleed_rb_8888[256];
   uint8_t bleed_g_8888[256];
   uint8_t bleed_rb_565[64];
   uint8_t bleed_g_565[64];
   uint8_t scan_8888[256][256];
   uint8_t scan_565[64][64];
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
   if (!filt)
      return NULL;
   if (!(filt->workers = (struct softfilter_thread_data*)
      calloc(threads, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->threads        = threads;
   filt->in_fmt         = in_fmt;

   filt->phosphor_bleed = 0.78;
   filt->scale_add      = 1.0;
   filt->scale_times    = 0.8;
   filt->scanrange_low  = 0.5;
   filt->scanrange_high = 0.65;

   for (i = 0; i < 256; i++)
   {
      filt->phosphor_bloom_8888[i] =
         filt->scale_times * powf((float)i / 255.0f, 1.0f/2.2f) +
         filt->scale_add;
      filt->scan_range_8888[i]     =
         filt->scanrange_low + i *
         (filt->scanrange_high - filt->scanrange_low) / 255.0f;
   }
   for (i = 0; i < 64; i++)
   {
      filt->phosphor_bloom_565[i] =
         filt->scale_times * powf((float)i / 31.0f, 1.0f/2.2f)
         + filt->scale_add;
      filt->scan_range_565[i]     =
         filt->scanrange_low + i *
         (filt->scanrange_high - filt->scanrange_low) / 31.0f;
   }

   /* The lookup tables, from the expressions the lines used to
    * evaluate per pixel, operand types included. */
   for (i = 0; i < 256; i++)
   {
      unsigned v = i, c;
      unsigned r_set = clamp8(v * filt->phosphor_bleed *
            filt->phosphor_bloom_8888[v]);
      unsigned g_set = clamp8((v >> 1) + 0.5 * v *
            filt->phosphor_bleed * filt->phosphor_bloom_8888[v]);
      filt->bleed_rb_8888[i] = (uint8_t)r_set;
      filt->bleed_g_8888[i]  = (uint8_t)g_set;
      for (c = 0; c < 256; c++)
         filt->scan_8888[i][c] = (uint8_t)(uint32_t)
            (filt->scan_range_8888[i] * c);
   }
   for (i = 0; i < 64; i++)
   {
      unsigned v = i;
      int c;
      unsigned r_set = clamp6(v * filt->phosphor_bleed *
            filt->phosphor_bloom_565[v]);
      unsigned g_set = clamp6((v >> 1) + 0.5 * v *
            filt->phosphor_bleed * filt->phosphor_bloom_565[v]);
      filt->bleed_rb_565[i] = (uint8_t)r_set;
      filt->bleed_g_565[i]  = (uint8_t)g_set;
      for (c = 0; c < 64; c++)
         filt->scan_565[i][c] = (uint8_t)(uint16_t)
            (filt->scan_range_565[i] * c);
   }

   return filt;
}

static void phosphor2x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width * PHOSPHOR2X_SCALE;
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

/* One line pair in a single pass. The line is the source stretched to
 * twice its width (even pixels the source pixels, the first blended with
 * black; odd pixels the blend of the two either side, the last black),
 * then the phosphor bleed: each odd pixel takes the bled red of the even
 * pixel before it, each even pixel after the first the bled blue of the
 * odd pixel before it (the first one's blue is cleared), and every green
 * is run through its table. The scanline below takes each finished
 * pixel's levels. Each output pixel needs only the stretched pixel
 * before it, so it is built in a register with the set_ macros, and the
 * two lines are written once. */
#define PHOSPHOR2X_LINE(TYPE, FMT, RB, GT, SCAN) \
   { \
      const uint8_t *rb = filt->RB; \
      const uint8_t *gt = filt->GT; \
      uint32_t prev_odd = 0; /* the stretched pixel before this one */ \
      unsigned i; \
      for (i = 0; i < width; i++) \
      { \
         uint32_t cur    = in_line[i]; \
         uint32_t s_even = i ? cur : blend_pixels_##FMT(cur, 0); \
         uint32_t s_odd  = (i + 1 < width) \
            ? blend_pixels_##FMT(cur, (uint32_t)in_line[i + 1]) : 0; \
         uint32_t ev     = s_even; \
         uint32_t od     = s_odd; \
         set_green_##FMT(ev, (uint32_t)gt[green_##FMT(ev)]); \
         set_blue_##FMT(ev, i ? (uint32_t)rb[blue_##FMT(prev_odd)] : 0); \
         set_red_##FMT(od, (uint32_t)rb[red_##FMT(s_even)]); \
         set_green_##FMT(od, (uint32_t)gt[green_##FMT(od)]); \
         out_line[2 * i]     = (TYPE)ev; \
         out_line[2 * i + 1] = (TYPE)od; \
         { \
            const uint8_t *le = filt->SCAN[max_component_##FMT(ev)]; \
            const uint8_t *lo = filt->SCAN[max_component_##FMT(od)]; \
            uint32_t se = 0, so = 0; \
            set_red_##FMT(se,   (uint32_t)le[red_##FMT(ev)]); \
            set_green_##FMT(se, (uint32_t)le[green_##FMT(ev)]); \
            set_blue_##FMT(se,  (uint32_t)le[blue_##FMT(ev)]); \
            set_red_##FMT(so,   (uint32_t)lo[red_##FMT(od)]); \
            set_green_##FMT(so, (uint32_t)lo[green_##FMT(od)]); \
            set_blue_##FMT(so,  (uint32_t)lo[blue_##FMT(od)]); \
            scan_out[2 * i]     = (TYPE)se; \
            scan_out[2 * i + 1] = (TYPE)so; \
         } \
         prev_odd = s_odd; \
      } \
   }

static void phosphor2x_generic_xrgb8888(void *data,
      unsigned width, unsigned height,
      int first, int last, uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned y;
   struct filter_data *filt = (struct filter_data*)data;

   for (y = 0; y < height; y++)
   {
      const uint32_t *in_line = src + y * src_stride;
      /* Output in a scanlines fashion. */
      uint32_t *out_line      = dst + y * dst_stride * 2;
      uint32_t *scan_out      = out_line + dst_stride;
      PHOSPHOR2X_LINE(uint32_t, xrgb8888, bleed_rb_8888, bleed_g_8888,
            scan_8888)
   }
}

static void phosphor2x_generic_rgb565(void *data,
      unsigned width, unsigned height,
      int first, int last, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned y;
   struct filter_data *filt = (struct filter_data*)data;

   for (y = 0; y < height; y++)
   {
      const uint16_t *in_line = src + y * src_stride;
      /* Output in a scanlines fashion. */
      uint16_t *out_line      = dst + y * dst_stride * 2;
      uint16_t *scan_out      = out_line + dst_stride;
      PHOSPHOR2X_LINE(uint16_t, rgb565, bleed_rb_565, bleed_g_565,
            scan_565)
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
   uint16_t *input                    =  (uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;
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

      unsigned y_start       = (height * i) / filt->threads;
      unsigned y_end         = (height * (i + 1)) / filt->threads;
      thr->out_data          = (uint8_t*)output + y_start * PHOSPHOR2X_SCALE * output_stride;
      thr->in_data           = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch         = output_stride;
      thr->in_pitch          = input_stride;
      thr->width             = width;
      thr->height            = y_end - y_start;

      /* Workers need to know if they can access pixels
       * outside their given buffer. */
      thr->first             = y_start;
      thr->last              = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work     = phosphor2x_work_cb_rgb565;
      if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work     = phosphor2x_work_cb_xrgb8888;
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
   return &phosphor2x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
