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

/* Widest decode window, in samples either side (Atari). */
#define NTSC_MAX_TAPS 8

/* Scratch for one worker: the decoded I/Q/Y lines, then the composite
 * line padded by the widest window on each side and its two carrier
 * products. Held per thread rather than on the stack, which on Vita is
 * the 64 KB rthreads asks for. */
#define NTSC_LINE_INTS     (NTSC_MAX_WIDTH * 2)
#define NTSC_PAD_INTS      (NTSC_MAX_WIDTH * 2 + 2 * NTSC_MAX_TAPS)
#define NTSC_SCRATCH_BYTES ((3 * NTSC_LINE_INTS + 3 * NTSC_PAD_INTS) \
      * sizeof(int))

struct softfilter_thread_data {
   void       *out_data;
   const void *in_data;
   size_t      out_pitch;
   size_t      in_pitch;
   unsigned    width;
   unsigned    height;
   int         first;
   void       *scratch;
   int        *lineI;
   int        *lineQ;
   int        *lineY;
   int        *cpad;
   int        *prodI;
   int        *prodQ;
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

/* The decode window's half-width in samples (taps) and its divisor are
 * fixed per system, so the window pass is built once per width: the
 * divide by a constant becomes a multiply or shift, as the compiler
 * picks, with C's truncating result unchanged. */
#define NTSC_WINDOW_PASS(name, TAPS) \
static void name(const int *cpad, const int *prodI, const int *prodQ, \
      unsigned ow, int artifacts, int c64, int *lineI, int *lineQ, \
      int *lineY) \
{ \
   unsigned x; \
   int accI = 0, accQ = 0, j; \
   /* The window for x covers positions x - TAPS .. x + TAPS - 1, which \
    * sit at x .. x + 2 * TAPS - 1 in the padded arrays. */ \
   for (j = 0; j < 2 * (TAPS); j++) \
   { \
      accI += prodI[j]; \
      accQ += prodQ[j]; \
   } \
   for (x = 0; x < ow; x++) \
   { \
      const int *c = cpad + x + (TAPS); \
      int notchedY = c64 \
         ? (c[-1] + (c[0] * 2) + c[1]) >> 2 \
         : (c[-2] + (c[-1] * 4) + (c[0] * 6) + (c[1] * 4) + c[2]) >> 4; \
      lineI[x] = accI / ((TAPS) * 256); \
      lineQ[x] = accQ / ((TAPS) * 256); \
      lineY[x] = ((notchedY * (256 - artifacts)) + (c[0] * artifacts)) >> 8; \
      accI    += prodI[x + 2 * (TAPS)] - prodI[x]; \
      accQ    += prodQ[x + 2 * (TAPS)] - prodQ[x]; \
   } \
}

NTSC_WINDOW_PASS(ntsc_window_pass4, 4)
NTSC_WINDOW_PASS(ntsc_window_pass6, 6)
NTSC_WINDOW_PASS(ntsc_window_pass8, 8)

static void ntsc_process_line(const struct filter_data *filt, 
      const struct softfilter_thread_data *thr, unsigned y, void *dst_ptr) {
   unsigned width = thr->width;
   unsigned ow = width * 2;
   int phases = filt->phase_count;
   
   // C64 uses 2-phase steps per output pixel if sampled at 8 phases
   int phase_step = (filt->c64_mode) ? 2 : 4; 
   int frame_offset = (filt->nes_frame_count % 2) * (phases / 2);
   int line_mult = (filt->pal_mode) ? (phase_step + 1) : phase_step;
   int line_phase = (frame_offset + ((thr->first + (int)y) * line_mult)) % phases;
   int taps = (filt->atari_mode) ? 8 : (filt->c64_mode ? 4 : 6);
   /* Composite samples padded by 'taps' on each side with the edge
    * sample repeated (the clamp the window and the notch read through),
    * and each padded position's sample times the carrier at that
    * position: a window sum then needs no clamp, no modulo and no
    * multiply. */
   int *lineI = thr->lineI, *lineQ = thr->lineQ, *lineY = thr->lineY;
   int *cpad  = thr->cpad, *prodI = thr->prodI, *prodQ = thr->prodQ;
   int n      = (int)ow + 2 * taps;
   int ph, j;

   /* Encode: Y plus the chroma on the carrier, two samples per pixel,
    * the carrier advancing phase_step per sample. */
   ph = line_phase;
   if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      const uint16_t *in = (const uint16_t*)thr->in_data + y * (thr->in_pitch / 2);
      for (unsigned x = 0; x < width; x++) {
         int r, g, b, Y, I, Q;
         uint16_t p = in[x];
         r = (p >> 11) & 0x1f; g = (p >> 5) & 0x3f; b = p & 0x1f;
         /* Widen with the top bits copied down, so full scale is 255. */
         r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
         rgb_to_yiq(r, g, b, &Y, &I, &Q);
         cpad[taps + 2 * x]     = Y + ((I * filt->lut_cos[ph] + Q * filt->lut_sin[ph]) >> 8);
         if ((ph += phase_step) >= phases) ph -= phases;
         cpad[taps + 2 * x + 1] = Y + ((I * filt->lut_cos[ph] + Q * filt->lut_sin[ph]) >> 8);
         if ((ph += phase_step) >= phases) ph -= phases;
      }
   } else {
      const uint32_t *in = (const uint32_t*)thr->in_data + y * (thr->in_pitch / 4);
      for (unsigned x = 0; x < width; x++) {
         int r, g, b, Y, I, Q;
         uint32_t p = in[x];
         r = (p >> 16) & 0xFF; g = (p >> 8) & 0xFF; b = p & 0xFF;
         rgb_to_yiq(r, g, b, &Y, &I, &Q);
         cpad[taps + 2 * x]     = Y + ((I * filt->lut_cos[ph] + Q * filt->lut_sin[ph]) >> 8);
         if ((ph += phase_step) >= phases) ph -= phases;
         cpad[taps + 2 * x + 1] = Y + ((I * filt->lut_cos[ph] + Q * filt->lut_sin[ph]) >> 8);
         if ((ph += phase_step) >= phases) ph -= phases;
      }
   }
   for (j = 0; j < taps; j++) {
      cpad[j]                = cpad[taps];
      cpad[taps + ow + j]    = cpad[taps + ow - 1];
   }

   /* Demodulate: the window multiplies each position's sample by the
    * carrier at that position (position = padded index - taps, phase
    * line_phase + position * phase_step, wrapped into [0, phases)). */
   ph = (line_phase - taps * phase_step) % phases;
   if (ph < 0)
      ph += phases;
   for (j = 0; j < n; j++) {
      prodI[j] = cpad[j] * filt->lut_cos[ph];
      prodQ[j] = cpad[j] * filt->lut_sin[ph];
      if ((ph += phase_step) >= phases) ph -= phases;
   }

   switch (taps) {
      case 4:
         ntsc_window_pass4(cpad, prodI, prodQ, ow, filt->artifacts,
               filt->c64_mode, lineI, lineQ, lineY);
         break;
      case 8:
         ntsc_window_pass8(cpad, prodI, prodQ, ow, filt->artifacts,
               filt->c64_mode, lineI, lineQ, lineY);
         break;
      default:
         ntsc_window_pass6(cpad, prodI, prodQ, ow, filt->artifacts,
               filt->c64_mode, lineI, lineQ, lineY);
         break;
   }

   for (unsigned x = 0; x < ow; x++) {
      int Y = lineY[x], I = lineI[x], Q = lineQ[x];
      if (filt->sharpness > 0) {
         int x_m1 = (x > 0) ? (int)x - 1 : 0, x_p1 = (x < ow - 1) ? (int)x + 1 : (int)ow - 1;
         int edge = (Y * 2) - (lineY[x_m1] + lineY[x_p1]);
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

static void ntsc_destroy(void *data);

static void ntsc_work_cb(void *data, void *thread_data) {
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   for (unsigned y = 0; y < thr->height; y++) {
      void *dst = (uint8_t*)thr->out_data + (y * thr->out_pitch);
      ntsc_process_line(filt, thr, y, dst);
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
   if (!filt->workers) {
      free(filt);
      return NULL;
   }
   for (unsigned i = 0; i < threads; i++) {
      struct softfilter_thread_data *thr = &filt->workers[i];
      /* one block per worker, so the line buffers stay adjacent
       * rather than landing in six allocations */
      if (!(thr->scratch = calloc(1, NTSC_SCRATCH_BYTES))) {
         ntsc_destroy(filt);
         return NULL;
      }
      thr->lineI     = (int*)thr->scratch;
      thr->lineQ     = thr->lineI + NTSC_LINE_INTS;
      thr->lineY     = thr->lineQ + NTSC_LINE_INTS;
      thr->cpad      = thr->lineY + NTSC_LINE_INTS;
      thr->prodI     = thr->cpad  + NTSC_PAD_INTS;
      thr->prodQ     = thr->prodI + NTSC_PAD_INTS;
   }
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
      thr->width = (width > NTSC_MAX_WIDTH) ? NTSC_MAX_WIDTH : width;
      thr->height = y_end - y_start;
      thr->first = (int)y_start;
      packets[i].work = ntsc_work_cb;
      packets[i].thread_data = thr;
   }
}

static void ntsc_destroy(void *data) {
   struct filter_data *f = (struct filter_data*)data;
   if (!f)
      return;
   if (f->workers) {
      for (unsigned i = 0; i < f->threads; i++)
         free(f->workers[i].scratch);
      free(f->workers);
   }
   free(f);
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