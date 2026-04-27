#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation dedither_get_implementation
#define softfilter_thread_data dedither_softfilter_thread_data
#define filter_data dedither_filter_data
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

static unsigned dedither_generic_input_fmts(void) {
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned dedither_generic_output_fmts(unsigned input_fmts) {
   return input_fmts;
}

static unsigned dedither_generic_threads(void *data) {
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *dedither_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata) {
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt) return NULL;
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1; 
   filt->in_fmt  = in_fmt;
   return filt;
}

static void dedither_generic_destroy(void *data) {
   struct filter_data *filt = (struct filter_data*)data;
   if (filt) { free(filt->workers); free(filt); }
}

static void dedither_generic_output(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height) {
   *out_width = width; *out_height = height;
}

/* Color comparison with threshold */
static inline int pix_equal(uint32_t c1, uint32_t c2, int threshold) {
   int r = abs((int)((c1 >> 16) & 0xFF) - (int)((c2 >> 16) & 0xFF));
   int g = abs((int)((c1 >> 8) & 0xFF) - (int)((c2 >> 8) & 0xFF));
   int b = abs((int)(c1 & 0xFF) - (int)(c2 & 0xFF));
   return (r + g + b) < threshold;
}

/* XRGB8888 Kernel - 2D Dither Detection (6x6) */
static void dedither_work_cb_xrgb8888(void *data, void *thread_data) {
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *in = (const uint32_t*)thr->in_data;
   uint32_t *out      = (uint32_t*)thr->out_data;
   uint32_t in_stride = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride = (uint32_t)(thr->out_pitch >> 2);
   const int threshold = 40;

   for (uint32_t y = 0; y < thr->height; ++y) {
      for (uint32_t x = 0; x < thr->width; ++x) {
         /* Check safety bounds for 6-pixel horizontal and vertical patterns */
         if (x >= 2 && x < thr->width - 3 && y >= 2 && y < thr->height - 3) {
            
            const uint32_t *line = in + y * in_stride;
            
            /* Horizontal samples (Row y) */
            uint32_t h1 = line[x - 2]; uint32_t h2 = line[x - 1];
            uint32_t h3 = line[x];     /* Center Pixel */
            uint32_t h4 = line[x + 1]; uint32_t h5 = line[x + 2];
            uint32_t h6 = line[x + 3];

            /* Vertical samples (Column x) */
            uint32_t v1 = (line - 2 * in_stride)[x];
            uint32_t v2 = (line - 1 * in_stride)[x];
            uint32_t v4 = (line + 1 * in_stride)[x];
            uint32_t v5 = (line + 2 * in_stride)[x];
            uint32_t v6 = (line + 3 * in_stride)[x];

            /* Pattern check: A-B-A-B-A-B */
            int h_dit = (pix_equal(h1, h3, threshold) && pix_equal(h3, h5, threshold) &&
                         pix_equal(h2, h4, threshold) && pix_equal(h4, h6, threshold) &&
                         !pix_equal(h3, h4, threshold));

            int v_dit = (pix_equal(v1, h3, threshold) && pix_equal(h3, v5, threshold) &&
                         pix_equal(v2, v4, threshold) && pix_equal(v4, v6, threshold) &&
                         !pix_equal(h3, v4, threshold));

            if (h_dit || v_dit) {
               uint32_t avg;
               if (h_dit)
                  avg = (((h2 & 0xFEFEFEFE) >> 1) + ((h4 & 0xFEFEFEFE) >> 1));
               else
                  avg = (((v2 & 0xFEFEFEFE) >> 1) + ((v4 & 0xFEFEFEFE) >> 1));
               
               /* Apply soft blend (1:2:1 weighting) */
               out[y * out_stride + x] = (((avg & 0xFEFEFEFE) >> 1) + ((h3 & 0xFEFEFEFE) >> 1));
            } else {
               out[y * out_stride + x] = h3; /* Keep original sharp pixel */
            }
         } else {
            out[y * out_stride + x] = in[y * in_stride + x];
         }
      }
   }
}

/* RGB565 Kernel - 2D Dither Detection (6x6) */
static void dedither_work_cb_rgb565(void *data, void *thread_data) {
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *in = (const uint16_t*)thr->in_data;
   uint16_t *out      = (uint16_t*)thr->out_data;
   uint16_t in_stride = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride = (uint16_t)(thr->out_pitch >> 1);

   for (uint32_t y = 0; y < thr->height; ++y) {
      for (uint32_t x = 0; x < thr->width; ++x) {
         if (x >= 2 && x < thr->width - 3 && y >= 2 && y < thr->height - 3) {
            const uint16_t *line = in + y * in_stride;
            uint16_t h1 = line[x - 2]; uint16_t h2 = line[x - 1];
            uint16_t h3 = line[x];     uint16_t h4 = line[x + 1];
            uint16_t h5 = line[x + 2]; uint16_t h6 = line[x + 3];

            uint16_t v1 = (line - 2 * in_stride)[x]; uint16_t v2 = (line - 1 * in_stride)[x];
            uint16_t v4 = (line + 1 * in_stride)[x]; uint16_t v5 = (line + 2 * in_stride)[x];
            uint16_t v6 = (line + 3 * in_stride)[x];

            int h_dit = (h1 == h3 && h3 == h5 && h2 == h4 && h4 == h6 && h3 != h4);
            int v_dit = (v1 == h3 && h3 == v5 && v2 == v4 && v4 == v6 && h3 != v2);

            if (h_dit || v_dit) {
               uint16_t avg_s = (h_dit) ? (((h2 & 0xF7DE) >> 1) + ((h4 & 0xF7DE) >> 1)) : (((v2 & 0xF7DE) >> 1) + ((v4 & 0xF7DE) >> 1));
               out[y * out_stride + x] = (((avg_s & 0xF7DE) >> 1) + ((h3 & 0xF7DE) >> 1));
            } else out[y * out_stride + x] = h3;
         } else out[y * out_stride + x] = in[y * in_stride + x];
      }
   }
}

static void dedither_generic_packets(void *data, struct softfilter_work_packet *packets,
      void *output, size_t output_stride, const void *input, unsigned width, unsigned height, size_t input_stride) {
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = &filt->workers[0];
   thr->out_data = output; thr->in_data = input;
   thr->out_pitch = output_stride; thr->in_pitch = input_stride;
   thr->width = width; thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
      packets[0].work = dedither_work_cb_xrgb8888;
   else if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
      packets[0].work = dedither_work_cb_rgb565;
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation dedither_generic = {
   dedither_generic_input_fmts, dedither_generic_output_fmts,
   dedither_generic_create, dedither_generic_destroy,
   dedither_generic_threads, dedither_generic_output, dedither_generic_packets,
   SOFTFILTER_API_VERSION, "Master De-Dither 2D (6x6)", "dedither",
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd) {
   (void)simd; return &dedither_generic;
}