#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation crop_borders_get_implementation
#define softfilter_thread_data crop_borders_softfilter_thread_data
#define filter_data crop_borders_filter_data
#endif

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned width;
   unsigned height;
};

struct filter_data
{
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   float crop_x;
   float crop_y;
};

static unsigned crop_borders_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned crop_borders_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned crop_borders_generic_threads(void *data)
{
   return 1;
}

static void crop_borders_initialize(struct filter_data *filt,
      const struct softfilter_config *config,
      void *userdata)
{
   /* RetroArch wull look at .filt for: crop_borders_crop_x */
   config->get_float(userdata, "crop_x", &filt->crop_x, 0.0f);
   config->get_float(userdata, "crop_y", &filt->crop_y, 0.0f);
}

static void *crop_borders_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt) return NULL;

   if (!(filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }

   filt->in_fmt = in_fmt;

   crop_borders_initialize(filt, config, userdata);

   return filt;
}

static void crop_borders_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width;
   *out_height = height;
}

static void crop_borders_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) return;
   free(filt->workers);
   free(filt);
}

/* Rendering Logic with Float Coordinates */
static void crop_borders_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input = (const uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned in_stride = (unsigned)(thr->in_pitch >> 2);
   unsigned out_stride = (unsigned)(thr->out_pitch >> 2);

   float visible_w = (float)thr->width - (filt->crop_x * 2.0f);
   float visible_h = (float)thr->height - (filt->crop_y * 2.0f);

   if (visible_w <= 0.0f || visible_h <= 0.0f) return;

   float step_x = visible_w / (float)thr->width;
   float step_y = visible_h / (float)thr->height;

   for (unsigned y = 0; y < thr->height; y++)
   {
      unsigned i_y = (unsigned)(filt->crop_y + (y * step_y));
      for (unsigned x = 0; x < thr->width; x++)
      {
         unsigned i_x = (unsigned)(filt->crop_x + (x * step_x));
         output[y * out_stride + x] = input[i_y * in_stride + i_x];
      }
   }
}

static void crop_borders_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input = (const uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned in_stride = (unsigned)(thr->in_pitch >> 1);
   unsigned out_stride = (unsigned)(thr->out_pitch >> 1);

   float visible_w = (float)thr->width - (filt->crop_x * 2.0f);
   float visible_h = (float)thr->height - (filt->crop_y * 2.0f);

   if (visible_w <= 0.0f || visible_h <= 0.0f) return;

   float step_x = visible_w / (float)thr->width;
   float step_y = visible_h / (float)thr->height;

   for (unsigned y = 0; y < thr->height; y++)
   {
      unsigned i_y = (unsigned)(filt->crop_y + (y * step_y));
      for (unsigned x = 0; x < thr->width; x++)
      {
         unsigned i_x = (unsigned)(filt->crop_x + (x * step_x));
         output[y * out_stride + x] = input[i_y * in_stride + i_x];
      }
   }
}

static void crop_borders_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = &filt->workers[0];

   thr->out_data = output;
   thr->in_data  = input;
   thr->out_pitch = output_stride;
   thr->in_pitch  = input_stride;
   thr->width = width;
   thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
      packets[0].work = crop_borders_work_cb_rgb565;
   else
      packets[0].work = crop_borders_work_cb_xrgb8888;

   packets[0].thread_data = thr;
}

static const struct softfilter_implementation crop_borders_generic = {
   crop_borders_generic_input_fmts,
   crop_borders_generic_output_fmts,
   crop_borders_generic_create,
   crop_borders_generic_destroy,
   crop_borders_generic_threads,
   crop_borders_generic_output,
   crop_borders_generic_packets,
   SOFTFILTER_API_VERSION,
   "Crop Borders",
   "crop_borders",
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   (void)simd;
   return &crop_borders_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
