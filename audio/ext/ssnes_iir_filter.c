#include "ssnes_dsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Simple IIR filter implementation, optimized for SSE3.

#ifdef __SSE3__ // Build with -march=native or -msse3 to let this be detected! D:
#define USE_SSE3
#endif

#ifdef USE_SSE3
#include <pmmintrin.h>
#endif

// Make a test build with standalone main()
//#define TEST

#ifdef TEST
#include <stdio.h>
#include <assert.h>
#endif

#define min(x, y) ((x) < (y) ? (x) : (y))

// Taps needs to be power-of-two.
#define TAPS (128)

// FIR coefficients, up to TAPS taps.
// Basic passthrough.
static const float fir_coeff[] = {
   1.0
};

// IIR coefficients, up to TAPS taps.
static const float iir_coeff[] = {
};

#ifdef USE_SSE3
typedef union
{
   __m128 vec[TAPS / 4 + 1]; // Wraparound for unaligned reads.
   float f[TAPS + 4];
} vec_filt;
#else
typedef float vec_filt[TAPS + 4];
#endif

typedef struct
{
   vec_filt buffer[2];
   vec_filt iir_buffer[2];
   vec_filt fir_coeff;
   vec_filt iir_coeff;

   float out_buffer[4096];
   unsigned buf_ptr;
} dsp_state;

static void* dsp_init(const ssnes_dsp_info_t *info)
{
   (void)info;
   dsp_state *state = calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   memcpy(&state->fir_coeff, fir_coeff, min(sizeof(fir_coeff), TAPS * sizeof(float)));
   memcpy(&state->iir_coeff, iir_coeff, min(sizeof(iir_coeff), TAPS * sizeof(float)));
#ifdef USE_SSE3
   memcpy(state->fir_coeff.f + TAPS, fir_coeff, min(sizeof(fir_coeff), 4 * sizeof(float)));
   memcpy(state->iir_coeff.f + TAPS, iir_coeff, min(sizeof(iir_coeff), 4 * sizeof(float)));
#endif

   return state;
}

#ifdef USE_SSE3
static void calculate_iir(float *out, dsp_state *dsp)
{
   unsigned buf_ptr = (dsp->buf_ptr - 1) & (TAPS - 1);
   unsigned iir_ptr = (TAPS - dsp->buf_ptr) & (TAPS - 1);

   const float * restrict samples_left = dsp->buffer[0].f; 
   const float * restrict samples_right = dsp->buffer[1].f; 
   const float * restrict iir_left = dsp->iir_buffer[0].f; 
   const float * restrict iir_right = dsp->iir_buffer[1].f; 

   __m128 sum_left = _mm_setzero_ps();
   __m128 sum_right = _mm_setzero_ps();

   for (unsigned i = 0; i < TAPS; i += 4)
   {
      __m128 left         = _mm_load_ps(samples_left + i);
      __m128 right        = _mm_load_ps(samples_right + i);
      const __m128 ileft  = _mm_load_ps(iir_left + i);
      const __m128 iright = _mm_load_ps(iir_right + i);

      // Need unaligned reads here.
      const __m128 fir = _mm_loadu_ps(dsp->fir_coeff.f + iir_ptr);
      const __m128 iir = _mm_loadu_ps(dsp->iir_coeff.f + iir_ptr);

      const __m128 fir_res_left  = _mm_mul_ps(fir, left);
      const __m128 fir_res_right = _mm_mul_ps(fir, right);
      const __m128 iir_res_left  = _mm_mul_ps(iir, ileft);
      const __m128 iir_res_right = _mm_mul_ps(iir, iright);

      left  = _mm_add_ps(fir_res_left,  iir_res_left);
      right = _mm_add_ps(fir_res_right, iir_res_right);

      sum_left  = _mm_add_ps(sum_left,  left);
      sum_right = _mm_add_ps(sum_right, right);

      iir_ptr = (iir_ptr + 4) & (TAPS - 1);
   }

   __m128 res = _mm_hadd_ps(sum_left,  sum_right);
   res = _mm_hadd_ps(res, res);

   union
   {
      __m128 vec;
      float f[4];
   } u;
   u.vec = res;

   out[0] = u.f[0];
   out[1] = u.f[1];
   dsp->iir_buffer[0].f[buf_ptr] = u.f[0];
   dsp->iir_buffer[1].f[buf_ptr] = u.f[1];
}
#else
static void calculate_iir(float *out, dsp_state *dsp)
{
   const float * restrict samples_left = dsp->buffer[0]; 
   const float * restrict samples_right = dsp->buffer[1]; 
   const float * restrict iir_left = dsp->iir_buffer[0]; 
   const float * restrict iir_right = dsp->iir_buffer[1]; 
   const float * restrict fir = dsp->fir_coeff;
   const float * restrict iir = dsp->iir_coeff;
   
   unsigned iir_ptr = 0;
   unsigned sample_ptr = dsp->buf_ptr;

   float sum[2] = { 0.0f, 0.0f };
   for (unsigned i = 0; i < TAPS; i++)
   {
      sum[0] += fir[iir_ptr] * samples_left[sample_ptr] + iir[iir_ptr] * iir_left[sample_ptr];
      sum[1] += fir[iir_ptr] * samples_right[sample_ptr] + iir[iir_ptr] * iir_right[sample_ptr];
      iir_ptr++;
      sample_ptr = (sample_ptr + 1) & (TAPS - 1);
   }

   // Stick our output value in the IIR buffer.
   for (unsigned i = 0; i < 2; i++)
   {
      out[i] = sum[i];
      dsp->iir_buffer[i][(dsp->buf_ptr - 1) & (TAPS - 1)] = sum[i];
   }
}
#endif

static void dsp_process(void *data, ssnes_dsp_output_t *output, const ssnes_dsp_input_t *input)
{
   dsp_state *dsp = data;
   output->should_resample = SSNES_TRUE;
   output->frames = input->frames;
   output->samples = dsp->out_buffer;

   for (unsigned i = 0; i < input->frames; i++)
   {
#ifdef USE_SSE3
      dsp->buffer[0].f[dsp->buf_ptr] = input->samples[(i << 1) + 0];
      dsp->buffer[1].f[dsp->buf_ptr] = input->samples[(i << 1) + 1];
#else
      dsp->buffer[0][dsp->buf_ptr] = input->samples[(i << 1) + 0];
      dsp->buffer[1][dsp->buf_ptr] = input->samples[(i << 1) + 1];
#endif

      calculate_iir(&dsp->out_buffer[i << 1], dsp);
      dsp->buf_ptr = (dsp->buf_ptr - 1) & (TAPS - 1);
   }
}

static void dsp_config(void *data)
{
   (void)data;
   // Normally we unhide a GUI window or something, 
   // but we're just going to print to the log instead.
   fprintf(stderr, "DSP_CONFIG\n");
}

static void dsp_free(void *data)
{
   free(data);
}

const ssnes_dsp_plugin_t dsp_plug = {
   .init = dsp_init,
   .process = dsp_process,
   .free = dsp_free,
   .config = dsp_config,
   .api_version = SSNES_DSP_API_VERSION,
   .ident = "IIR filter"
};

SSNES_API_EXPORT const ssnes_dsp_plugin_t*
   SSNES_API_CALLTYPE ssnes_dsp_plugin_init(void)
{
   return &dsp_plug;
}

#ifdef TEST
int main(void)
{
   const ssnes_dsp_plugin_t *plug = ssnes_dsp_plugin_init();
   assert(plug);

   ssnes_dsp_info_t info;
   void *handle = plug->init(&info); // Info isn't used.
   assert(handle);

   int16_t buf[64];
   float fbuf[64];

   for (;;)
   {
      size_t rd = fread(buf, sizeof(int16_t), 64, stdin); 
      for (unsigned i = 0; i < rd; i++)
         fbuf[i] = (float)buf[i];

      ssnes_dsp_input_t input = {
         .samples = fbuf,
         .frames = rd >> 1
      };
      ssnes_dsp_output_t output;
      plug->process(handle, &output, &input);

      for (unsigned i = 0; i < output.frames << 1; i++)
      {
         int32_t sample = (int32_t)output.samples[i];
         buf[i] = (sample > 0x7fff) ? 0x7fff : ((sample < -0x8000) ? -0x8000 : (int16_t)sample);
      }

      fwrite(buf, sizeof(int16_t), output.frames << 1, stdout);

      if (rd < 64)
         break;
   }

   plug->free(handle);
}
#endif
