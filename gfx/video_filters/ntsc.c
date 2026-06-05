#include "softfilter.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation ntsc_get_implementation
#define softfilter_thread_data        ntsc_softfilter_thread_data
#define filter_data                   ntsc_filter_data
#endif

#define NTSC_SCALE_X 2
#define NTSC_SCALE_Y 1
#define NTSC_MAX_WIDTH 1024
#define PHASE_MAX 16 // Max phases for Atari 2600

typedef struct { int Y, I, Q; } yiq_t;

struct softfilter_thread_data {
   void       *out_data;
   const void *in_data;
   size_t      out_pitch;
   size_t      in_pitch;
   unsigned    width;
   unsigned    height;
   int         first;
};

struct filter_data {
   unsigned                        threads;
   struct softfilter_thread_data  *workers;
   unsigned    in_fmt;
   int         hue_cos, hue_sin;
   int         saturation, sharpness, artifacts;
   int         huesat_identity, pal_mode, atari_mode, c64_mode;
   int         phase_count, nes_frame_count;
   int         lut_sin[PHASE_MAX], lut_cos[PHASE_MAX];
};

static inline void rgb_to_yiq(int r, int g, int b, int *Y, int *I, int *Q) {
    *Y = (  77*r + 150*g +  29*b) >> 8;
    *I = ( 157*r - 132*g -  26*b) >> 8;
    *Q = ( -38*r -  74*g + 112*b) >> 8;
}

static inline void yiq_to_rgb(int Y, int I, int Q, int *r, int *g, int *b) {
    int rv = Y + ((292*I) >> 8);
    int gv = Y - ((149*I) >> 8) - ((101*Q) >> 8);
    int bv = Y + ((520*Q) >> 8);
    *r = (rv < 0) ? 0 : (rv > 255 ? 255 : rv);
    *g = (gv < 0) ? 0 : (gv > 255 ? 255 : gv);
    *b = (bv < 0) ? 0 : (bv > 255 ? 255 : bv);
}

static void ntsc_process_line(const struct filter_data *filt, 
      const struct softfilter_thread_data *thr, unsigned y, 
      int *cbuf, int *lineI, int *lineQ, int *lineY, yiq_t *yiq_cache,
      void *dst_ptr) {
   unsigned width = thr->width;
   unsigned ow = width * 2;
   int phases = filt->phase_count;
   
   // C64 uses 2-phase steps per output pixel if sampled at 8 phases
   int phase_step = (filt->c64_mode) ? 2 : 4; 
   int frame_offset = (filt->nes_frame_count % 2) * (phases / 2);
   int line_mult = (filt->pal_mode) ? (phase_step + 1) : phase_step;
   int line_phase = (frame_offset + ((thr->first + (int)y) * line_mult)) % phases;

   for (unsigned x = 0; x < width; x++) {
      int r, g, b, Y, I, Q;
      if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
         uint16_t p = ((uint16_t*)thr->in_data)[y * (thr->in_pitch/2) + x];
         r = ((p >> 11) & 0x1f) << 3; g = ((p >> 5) & 0x3f) << 2; b = (p & 0x1f) << 3;
      } else {
         uint32_t p = ((uint32_t*)thr->in_data)[y * (thr->in_pitch/4) + x];
         r = (p >> 16) & 0xFF; g = (p >> 8) & 0xFF; b = p & 0xFF;
      }
      rgb_to_yiq(r, g, b, &Y, &I, &Q);
      yiq_cache[x].Y = Y; yiq_cache[x].I = I; yiq_cache[x].Q = Q;
      
      for (int p_idx = 0; p_idx < 2; p_idx++) {
         int ph = (line_phase + (x * phase_step * 2) + (p_idx * phase_step)) % phases;
         cbuf[x * 2 + p_idx] = Y + ((I * filt->lut_cos[ph] + Q * filt->lut_sin[ph]) >> 8);
      }
   }

   for (unsigned x = 0; x < ow; x++) {
      int accI = 0, accQ = 0;
      int taps = (filt->atari_mode) ? 8 : (filt->c64_mode ? 4 : 6); 
      
      for (int t = -taps; t < taps; t++) {
         int idx = (x + t < 0) ? 0 : (x + t >= (int)ow ? (int)ow - 1 : x + t);
         int ph = (line_phase + (x * phase_step) + (t * phase_step)) % phases;
         accI += cbuf[idx] * filt->lut_cos[ph];
         accQ += cbuf[idx] * filt->lut_sin[ph];
      }
      lineI[x] = accI / (taps * 256); lineQ[x] = accQ / (taps * 256);

      int i_m2 = (x > 1) ? (int)x - 2 : 0, i_m1 = (x > 0) ? (int)x - 1 : 0;
      int i_p1 = (x < ow - 1) ? (int)x + 1 : (int)ow - 1, i_p2 = (x < ow - 2) ? (int)x + 2 : (int)ow - 1;
      
      // Notch filter optimized for system-specific bandwidth
      int notchedY = (filt->c64_mode) ? 
         (cbuf[i_m1] + (cbuf[x] << 1) + cbuf[i_p1]) >> 2 :
         (cbuf[i_m2] + (cbuf[i_m1] << 2) + (cbuf[x] * 6) + (cbuf[i_p1] << 2) + cbuf[i_p2]) >> 4;

      lineY[x] = ((notchedY * (256 - filt->artifacts)) + (cbuf[x] * filt->artifacts)) >> 8;
   }

   for (unsigned x = 0; x < ow; x++) {
      int Y = lineY[x], I = lineI[x], Q = lineQ[x];
      if (filt->sharpness > 0) {
         int x_m1 = (x > 0) ? (int)x - 1 : 0, x_p1 = (x < ow - 1) ? (int)x + 1 : (int)ow - 1;
         int edge = (Y << 1) - (lineY[x_m1] + lineY[x_p1]);
         Y += (edge * filt->sharpness) >> 9;
         Y = (Y < 0) ? 0 : (Y > 255 ? 255 : Y);
      }
      if (!filt->huesat_identity) {
         int Ir = (I * filt->hue_cos - Q * filt->hue_sin) >> 8;
         int Qr = (I * filt->hue_sin + Q * filt->hue_cos) >> 8;
         I = (Ir * filt->saturation) >> 8; Q = (Qr * filt->saturation) >> 8;
      }
      int r, g, b;
      yiq_to_rgb(Y, I, Q, &r, &g, &b);
      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         ((uint16_t*)dst_ptr)[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
      else
         ((uint32_t*)dst_ptr)[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
   }
}

static void ntsc_work_cb(void *data, void *thread_data) {
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   int cbuf[NTSC_MAX_WIDTH * 2], lineI[NTSC_MAX_WIDTH * 2], lineQ[NTSC_MAX_WIDTH * 2], lineY[NTSC_MAX_WIDTH * 2];
   yiq_t yiq_cache[NTSC_MAX_WIDTH];
   for (unsigned y = 0; y < thr->height; y++) {
      void *dst = (uint8_t*)thr->out_data + (y * thr->out_pitch);
      ntsc_process_line(filt, thr, y, cbuf, lineI, lineQ, lineY, yiq_cache, dst);
   }
}

static void *ntsc_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt, unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata) {
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt) return NULL;
   float h = 0.0f, s = 1.0f, sh = 0.0f, art = 0.5f, pal = 0.0f, atari = 0.0f, c64 = 0.0f;
   if (config) {
      config->get_float(userdata, "hue", &h, 0.0f);
      config->get_float(userdata, "saturation", &s, 1.0f);
      config->get_float(userdata, "sharpness", &sh, 0.0f);
      config->get_float(userdata, "artifacts", &art, 0.5f);
      config->get_float(userdata, "pal_mode", &pal, 0.0f);
      config->get_float(userdata, "atari_mode", &atari, 0.0f);
      config->get_float(userdata, "c64_mode", &c64, 0.0f);
   }
   filt->in_fmt = in_fmt;
   filt->pal_mode = (pal != 0.0f);
   filt->atari_mode = (atari != 0.0f);
   filt->c64_mode = (c64 != 0.0f);

   // Determine phase count: Atari (16), NES (12), C64/Generic (8)
   if (filt->atari_mode) filt->phase_count = 16;
   else if (filt->c64_mode) filt->phase_count = 8;
   else filt->phase_count = 12;

   filt->hue_cos = (int)(cos(h * M_PI / 180.0) * 256.0);
   filt->hue_sin = (int)(sin(h * M_PI / 180.0) * 256.0);
   filt->saturation = (int)(s * 256.0);
   filt->sharpness = (int)(sh * 256.0f);
   filt->artifacts = (int)(art * 256.0f);
   filt->huesat_identity = (h == 0.0f && s == 1.0f);
   for (int i = 0; i < filt->phase_count; i++) {
      float rad = (float)(2.0 * M_PI * i / filt->phase_count);
      filt->lut_sin[i] = (int)(sin(rad) * 256.0f);
      filt->lut_cos[i] = (int)(cos(rad) * 256.0f);
   }
   filt->threads = threads;
   filt->workers = (struct softfilter_thread_data*)calloc(threads, sizeof(struct softfilter_thread_data));
   return filt;
}

static void ntsc_packets(void *data, struct softfilter_work_packet *packets,
      void *output, size_t output_stride, const void *input, unsigned width,
      unsigned height, size_t input_stride) {
   struct filter_data *filt = (struct filter_data*)data;
   filt->nes_frame_count++;
   for (unsigned i = 0; i < filt->threads; i++) {
      struct softfilter_thread_data *thr = &filt->workers[i];
      unsigned y_start = (height * i) / filt->threads, y_end = (height * (i + 1)) / filt->threads;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_data = (uint8_t*)output + y_start * output_stride;
      thr->in_pitch = input_stride; thr->out_pitch = output_stride;
      thr->width = width; thr->height = y_end - y_start;
      thr->first = (int)y_start;
      packets[i].work = ntsc_work_cb;
      packets[i].thread_data = thr;
   }
}

static void ntsc_destroy(void *data) { 
   struct filter_data *f = (struct filter_data*)data; 
   if (f) { free(f->workers); free(f); }
}

static void ntsc_output(void *data, unsigned *ow, unsigned *oh, unsigned w, unsigned h) { *ow = w*2; *oh = h; }
static unsigned ntsc_query_num_threads(void *data) { return ((struct filter_data*)data)->threads; }
static unsigned ntsc_input_fmts(void) { return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565; }
static unsigned ntsc_output_fmts(unsigned fmt) { return fmt; }

static const struct softfilter_implementation ntsc_impl = {
   ntsc_input_fmts, ntsc_output_fmts, ntsc_create, ntsc_destroy,
   ntsc_query_num_threads, ntsc_output, ntsc_packets, SOFTFILTER_API_VERSION, "NTSC-Multi-System", "ntsc",
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd) { 
   (void)simd; return &ntsc_impl; 
}