#include "softfilter.h"
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation paa_get_implementation
#define softfilter_thread_data paa_softfilter_thread_data
#define filter_data paa_filter_data
#endif

struct softfilter_thread_data {
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned width;
   unsigned height;
};

struct filter_data {
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
};

/* Support the 2 RetroArch formats  */
static unsigned paa_query_input_formats(void) {
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned paa_query_output_formats(unsigned input_format) {
   return input_format;
}

static unsigned paa_query_num_threads(void *data) {
   return 1;
}

static void *paa_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata) {
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt) return NULL;
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt = in_fmt;
   return filt;
}

static void paa_destroy(void *data) {
   struct filter_data *filt = (struct filter_data*)data;
   if (filt) { free(filt->workers); free(filt); }
}

static void paa_query_output_size(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height) {
   *out_width = width << 1;
   *out_height = height << 1;
}

/* Helper RGB565 */
static INLINE uint16_t mix_565(uint16_t c1, uint16_t c2) {
   return (((c1 & 0xF7DE) >> 1) + ((c2 & 0xF7DE) >> 1));
}

/* Helper XRGB8888 */
static INLINE uint32_t mix_8888(uint32_t c1, uint32_t c2) {
   return (((c1 & 0xFEFEFEFE) >> 1) + ((c2 & 0xFEFEFEFE) >> 1));
}

/* Logic for XRGB8888 */
static void paa_work_cb_xrgb8888(void *data, void *thread_data)
{
   uint32_t x, y;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *in  = (const uint32_t*)thr->in_data;
   uint32_t *out       = (uint32_t*)thr->out_data;
   uint32_t in_stride  = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride = (uint32_t)(thr->out_pitch >> 2);

   for (y = 0; y < thr->height; y++)
   {
      for (x = 0; x < thr->width; x++)
      {
         uint32_t p = in[y * in_stride + x];
         uint32_t *out_ptr = out + (y * 2 * out_stride) + (x * 2);
         uint32_t n = (y > 0) ? in[(y-1)*in_stride + x] : p;
         uint32_t s = (y < thr->height-1) ? in[(y+1)*in_stride + x] : p;
         uint32_t w = (x > 0) ? in[y*in_stride + (x-1)] : p;
         uint32_t e = (x < thr->width-1) ? in[y*in_stride + (x+1)] : p;

         out_ptr[0] = p; out_ptr[1] = p;
         out_ptr[out_stride] = p; out_ptr[out_stride + 1] = p;

         if (n != p && w != p && n == w)
            out_ptr[0] = mix_8888(p, n);
         if (n != p && e != p && n == e)
            out_ptr[1] = mix_8888(p, n);
         if (s != p && w != p && s == w)
            out_ptr[out_stride] = mix_8888(p, s);
         if (s != p && e != p && s == e)
            out_ptr[out_stride + 1] = mix_8888(p, s);
      }
   }
}

/* Logic for RGB565 */
static void paa_work_cb_rgb565(void *data, void *thread_data)
{
   uint32_t x, y;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *in = (const uint16_t*)thr->in_data;
   uint16_t *out      = (uint16_t*)thr->out_data;
   uint16_t in_stride = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride = (uint16_t)(thr->out_pitch >> 1);

   for (y = 0; y < thr->height; y++)
   {
      for (x = 0; x < thr->width; x++)
      {
         uint16_t p = in[y * in_stride + x];
         uint16_t *out_ptr = out + (y * 2 * out_stride) + (x * 2);
         uint16_t n = (y > 0) ? in[(y-1)*in_stride + x] : p;
         uint16_t s = (y < thr->height-1) ? in[(y+1)*in_stride + x] : p;
         uint16_t w = (x > 0) ? in[y*in_stride + (x-1)] : p;
         uint16_t e = (x < thr->width-1) ? in[y*in_stride + (x+1)] : p;

         out_ptr[0] = p; out_ptr[1] = p;
         out_ptr[out_stride] = p; out_ptr[out_stride + 1] = p;

         if (n != p && w != p && n == w)
            out_ptr[0] = mix_565(p, n);
         if (n != p && e != p && n == e)
            out_ptr[1] = mix_565(p, n);
         if (s != p && w != p && s == w)
            out_ptr[out_stride] = mix_565(p, s);
         if (s != p && e != p && s == e)
            out_ptr[out_stride + 1] = mix_565(p, s);
      }
   }
}

static void paa_get_work_packets(void *data,
		struct softfilter_work_packet *packets,
		void *output, size_t output_stride,
		const void *input, unsigned width, unsigned height,
		size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = &filt->workers[0];
   thr->out_data = output; thr->in_data = input;
   thr->out_pitch = output_stride; thr->in_pitch = input_stride;
   thr->width = width; thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
      packets[0].work = paa_work_cb_xrgb8888;
   else if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
      packets[0].work = paa_work_cb_rgb565;
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation paa_impl = {
   paa_query_input_formats, paa_query_output_formats,
   paa_create, paa_destroy, paa_query_num_threads,
   paa_query_output_size, paa_get_work_packets,
   SOFTFILTER_API_VERSION, "Pixel Art AA", "pixel_art_aa",
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd) {
   return &paa_impl;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
