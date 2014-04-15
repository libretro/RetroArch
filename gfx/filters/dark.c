// Compile: gcc -o dark.so -shared dark.c -std=c99 -O3 -Wall -pedantic -fPIC

#include "softfilter.h"
#include <stdlib.h>

static unsigned impl_input_fmts(void)
{
   return SOFTFILTER_FMT_XRGB8888;
}

static unsigned impl_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

struct thread_data
{
   uint32_t *out_data;
   const uint32_t *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned width;
   unsigned height;
};

struct filter_data
{
   unsigned threads;
   struct thread_data *workers;
};

static unsigned impl_threads(void *data)
{
   struct filter_data *filt = data;
   return filt->threads;;
}

static void *impl_create(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd)
{
   (void)simd;

   if (in_fmt != SOFTFILTER_FMT_XRGB8888 || out_fmt != SOFTFILTER_FMT_XRGB8888)
      return NULL;

   struct filter_data *filt = calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   filt->workers = calloc(threads, sizeof(struct thread_data));
   filt->threads = threads;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   return filt;
}

static void impl_output(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width;
   *out_height = height;
}

static void impl_destroy(void *data)
{
   struct filter_data *filt = data;
   free(filt->workers);
   free(filt);
}

static void work_cb(void *data, void *thread_data)
{
   struct thread_data *thr = thread_data;
   const uint32_t *input = thr->in_data;
   uint32_t *output = thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   for (unsigned y = 0; y < height; y++, input += thr->in_pitch >> 2, output += thr->out_pitch >> 2)
      for (unsigned x = 0; x < width; x++)
         output[x] = (input[x] >> 2) & (0x3f * 0x01010101);
}

static void impl_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = data;
   for (unsigned i = 0; i < filt->threads; i++)
   {
      struct thread_data *thr = &filt->workers[i];
      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint32_t*)output + y_start * (output_stride >> 2);
      thr->in_data = (const uint32_t*)input + y_start * (input_stride >> 2);
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      packets[i].work = work_cb;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation impl = {
   .query_input_formats = impl_input_fmts,
   .query_output_formats = impl_output_fmts,
   .create = impl_create,
   .destroy = impl_destroy,
   .query_num_threads = impl_threads,
   .query_output_size = impl_output,
   .get_work_packets = impl_packets,

   .ident = "Dark",
   .api_version = SOFTFILTER_API_VERSION,
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   (void)simd;
   return &impl;
}
