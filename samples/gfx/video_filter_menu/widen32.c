/* A test softfilter for the video filter menu harness: RGB565 in,
 * XRGB8888 out at twice the size, each pixel widened exactly (the top
 * bits repeated into the low ones). Deterministic, so a frame drawn
 * through it can be compared across a driver set up again. */
#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

struct widen32_work
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned width;
   unsigned height;
};

struct widen32_data
{
   struct widen32_work work;
};

static unsigned widen32_input_fmts(void) { return SOFTFILTER_FMT_RGB565; }

static unsigned widen32_output_fmts(unsigned input_fmts)
{
   (void)input_fmts;
   return SOFTFILTER_FMT_XRGB8888;
}

static unsigned widen32_threads(void *data) { (void)data; return 1; }

static void *widen32_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   (void)config; (void)in_fmt; (void)out_fmt; (void)max_width;
   (void)max_height; (void)threads; (void)simd; (void)userdata;
   return calloc(1, sizeof(struct widen32_data));
}

static void widen32_destroy(void *data) { free(data); }

static void widen32_output(void *data, unsigned *out_width,
      unsigned *out_height, unsigned width, unsigned height)
{
   (void)data;
   *out_width  = width  << 1;
   *out_height = height << 1;
}

static void widen32_work_cb(void *data, void *thread_data)
{
   struct widen32_work *w = (struct widen32_work*)thread_data;
   const uint16_t *in     = (const uint16_t*)w->in_data;
   uint32_t *out          = (uint32_t*)w->out_data;
   size_t in_stride       = w->in_pitch  >> 1;
   size_t out_stride      = w->out_pitch >> 2;
   unsigned x, y;
   (void)data;

   for (y = 0; y < w->height; y++)
   {
      uint32_t *row = out + (size_t)(y << 1) * out_stride;
      for (x = 0; x < w->width; x++)
      {
         uint16_t c = in[(size_t)y * in_stride + x];
         uint32_t r = (c >> 11) & 0x1f;
         uint32_t g = (c >>  5) & 0x3f;
         uint32_t b =  c        & 0x1f;
         uint32_t p = ((r << 3 | r >> 2) << 16)
                    | ((g << 2 | g >> 4) <<  8)
                    |  (b << 3 | b >> 2);
         row[2 * x]     = p;
         row[2 * x + 1] = p;
      }
      memcpy(row + out_stride, row, (size_t)(w->width << 1) * sizeof(uint32_t));
   }
}

static void widen32_packets(void *data, struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct widen32_data *filt = (struct widen32_data*)data;
   filt->work.out_data  = output;
   filt->work.in_data   = input;
   filt->work.out_pitch = output_stride;
   filt->work.in_pitch  = input_stride;
   filt->work.width     = width;
   filt->work.height    = height;
   packets[0].work        = widen32_work_cb;
   packets[0].thread_data = &filt->work;
}

static const struct softfilter_implementation widen32_impl = {
   widen32_input_fmts,
   widen32_output_fmts,
   widen32_create,
   widen32_destroy,
   widen32_threads,
   widen32_output,
   widen32_packets,
   SOFTFILTER_API_VERSION,
   "Widen32",
   "widen32",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &widen32_impl;
}
