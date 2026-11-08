/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project
 *
 * Adapted from FFmpeg for libretro-common; no libavcodec dependency.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "rwma_internal.h"
#include "rwma_mdct.h"

#define BLOCK_MIN_BITS 7
#define BLOCK_MAX_BITS 11
#define BLOCK_MAX_SIZE 2048
#define BLOCK_NB_SIZES 5
#define HIGH_BAND_MAX_SIZE 16
#define NB_LSP_COEFS 10
#define MAX_CODED_SUPERFRAME_SIZE 32768
#define MAX_CHANNELS 2
#define NOISE_TAB_SIZE 8192
#define LSP_POW_BITS 7
typedef struct rwma_standard_codebook
{
   const rwma_vlc_node *vlc;
   const uint16_t *runs;
   const float *levels;
} rwma_standard_codebook;
typedef struct rwma_standard
{
   int channels, sample_rate, bit_rate, block_align;
   unsigned failed, pending_shift, eof_done_native, delay_left, decoded_frames;
   unsigned frames_left, in_packet, cross_frame, header_end, packet_pos;
   const uint8_t *packet_data;
   rwma_bits packet_reader;
   rwma_mdct mdct;
   float math_storage[2048 * 8];
   const rwma_standard_codebook *codebook[2];
   rwma_bits gb;

   int version;
   int use_bit_reservoir;
   int use_variable_block_len;
   int use_exp_vlc;
   int use_noise_coding;
   int byte_offset_bits;

   int exponent_sizes[BLOCK_NB_SIZES];
   uint16_t exponent_bands[BLOCK_NB_SIZES][25];
   int high_band_start[BLOCK_NB_SIZES];
   int coefs_start;
   int coefs_end[BLOCK_NB_SIZES];
   int exponent_high_sizes[BLOCK_NB_SIZES];
   int exponent_high_bands[BLOCK_NB_SIZES][HIGH_BAND_MAX_SIZE];

   int high_band_coded[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];
   int high_band_values[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];

   int frame_len;
   int frame_len_bits;
   int nb_block_sizes;

   int reset_block_lengths;
   int block_len_bits;
   int next_block_len_bits;
   int prev_block_len_bits;
   int block_len;
   int block_num;
   int block_pos;
   uint8_t ms_stereo;
   uint8_t channel_coded[MAX_CHANNELS];
   int exponents_bsize[MAX_CHANNELS];
   float exponents[MAX_CHANNELS][BLOCK_MAX_SIZE];
   float max_exponent[MAX_CHANNELS];
   float coefs1[MAX_CHANNELS][BLOCK_MAX_SIZE];
   float coefs[MAX_CHANNELS][BLOCK_MAX_SIZE];
   float output[BLOCK_MAX_SIZE * 2];

   const float *windows[BLOCK_NB_SIZES];

   float frame_out[MAX_CHANNELS][BLOCK_MAX_SIZE * 2];

   uint8_t last_superframe[MAX_CODED_SUPERFRAME_SIZE + 0];
   int last_bitoffset;
   int last_superframe_len;
   int exponents_initialized[MAX_CHANNELS];
   float noise_table[NOISE_TAB_SIZE];
   int noise_index;
   float noise_mult;

   float lsp_cos_table[BLOCK_MAX_SIZE];
   float lsp_pow_e_table[256];
   float lsp_pow_m_table1[(1 << LSP_POW_BITS)];
   float lsp_pow_m_table2[(1 << LSP_POW_BITS)];

   int eof_done;

} rwma_standard;
#include "rwma_standard_tables.h"
static int rwma_frame_bits(int rate, int version, unsigned flags)
{
   (void)flags;
   return rate <= 16000                                      ? 9
          : rate <= 22050 || (version == 1 && rate <= 32000) ? 10
                                                             : 11;
}
static int rwma_gain_bits(int gain)
{
   return gain < 15 ? 13 : gain < 32 ? 12 : gain < 40 ? 11 : gain < 45 ? 10 : 9;
}
static void rwma_multiply_add(float *dst, const float *x, const float *w,
                              const float *old, int n)
{
   int i;
   for (i = 0; i < n; i++)
      dst[i] = x[i] * w[i] + old[i];
}
static void rwma_multiply_reverse(float *dst, const float *x, const float *w, int n)
{
   int i;
   for (i = 0; i < n; i++)
      dst[i] = x[i] * w[n - 1 - i];
}
static void rwma_butterfly(float *a, float *b, int n)
{
   int i;
   for (i = 0; i < n; i++)
   {
      float x = a[i], y = b[i];
      a[i] = x + y;
      b[i] = x - y;
   }
}
static void rwma_std_imdct(rwma_standard *s, float *dst, const float *src, int n)
{
   int i;
   rwma_mdct_run(&s->mdct, dst + n / 2, src, n, 1.0f / 32768);
   for (i = 0; i < n / 2; i++)
   {
      dst[i] = -dst[n - 1 - i];
      dst[2 * n - 1 - i] = dst[n + i];
   }
}

static void align_get_bits(rwma_bits *b) { skip_bits(b, (-b->pos) & 7); }

static int rwma_std_init(rwma_standard *s, int flags2)
{
   int channels = s->channels;
   int i;
   float bps1, high_freq;
   float bps;
   int sample_rate1;
   int coef_vlc_table;

   if (s->sample_rate > 50000 || channels > 2 || s->bit_rate <= 0)
      return -1;

   s->frame_len_bits = rwma_frame_bits(s->sample_rate, s->version, 0);
   s->next_block_len_bits = s->frame_len_bits;
   s->prev_block_len_bits = s->frame_len_bits;
   s->block_len_bits = s->frame_len_bits;

   s->frame_len = 1 << s->frame_len_bits;
   if (s->use_variable_block_len)
   {
      int nb_max, nb;
      nb = ((flags2 >> 3) & 3) + 1;
      if ((s->bit_rate / channels) >= 32000)
         nb += 2;
      nb_max = s->frame_len_bits - BLOCK_MIN_BITS;
      if (nb > nb_max)
         nb = nb_max;
      s->nb_block_sizes = nb + 1;
   }
   else
      s->nb_block_sizes = 1;

   s->use_noise_coding = 1;
   high_freq = s->sample_rate * 0.5;

   sample_rate1 = s->sample_rate;
   if (s->version == 2)
   {
      if (sample_rate1 >= 44100)
         sample_rate1 = 44100;
      else if (sample_rate1 >= 22050)
         sample_rate1 = 22050;
      else if (sample_rate1 >= 16000)
         sample_rate1 = 16000;
      else if (sample_rate1 >= 11025)
         sample_rate1 = 11025;
      else if (sample_rate1 >= 8000)
         sample_rate1 = 8000;
   }

   bps = (float)s->bit_rate / (float)(channels * s->sample_rate);
   s->byte_offset_bits = rwma_log2((int)(bps * s->frame_len / 8.0 + 0.5)) + 2;
   if (s->byte_offset_bits + 3 > 32)
   {
      (void)0;
      return RWMA_UNSUPPORTED;
   }

   bps1 = bps;
   if (channels == 2)
      bps1 = bps * 1.6;
   if (sample_rate1 == 44100)
   {
      if (bps1 >= 0.61)
         s->use_noise_coding = 0;
      else
         high_freq = high_freq * 0.4;
   }
   else if (sample_rate1 == 22050)
   {
      if (bps1 >= 1.16)
         s->use_noise_coding = 0;
      else if (bps1 >= 0.72)
         high_freq = high_freq * 0.7;
      else
         high_freq = high_freq * 0.6;
   }
   else if (sample_rate1 == 16000)
   {
      if (bps > 0.5)
         high_freq = high_freq * 0.5;
      else
         high_freq = high_freq * 0.3;
   }
   else if (sample_rate1 == 11025)
      high_freq = high_freq * 0.7;
   else if (sample_rate1 == 8000)
   {
      if (bps <= 0.625)
         high_freq = high_freq * 0.5;
      else if (bps > 0.75)
         s->use_noise_coding = 0;
      else
         high_freq = high_freq * 0.65;
   }
   else
   {
      if (bps >= 0.8)
         high_freq = high_freq * 0.75;
      else if (bps >= 0.6)
         high_freq = high_freq * 0.6;
      else
         high_freq = high_freq * 0.5;
   }
   (void)0;
   (void)0;
   (void)0;
   (void)0;

   {
      int a, b, pos, lpos, k, block_len, i, j, n;
      const uint8_t *table;

      if (s->version == 1)
         s->coefs_start = 3;
      else
         s->coefs_start = 0;
      for (k = 0; k < s->nb_block_sizes; k++)
      {
         block_len = s->frame_len >> k;

         if (s->version == 1)
         {
            lpos = 0;
            for (i = 0; i < 25; i++)
            {
               a = ff_wma_critical_freqs[i];
               b = s->sample_rate;
               pos = ((block_len * 2 * a) + (b >> 1)) / b;
               if (pos > block_len)
                  pos = block_len;
               s->exponent_bands[0][i] = pos - lpos;
               if (pos >= block_len)
               {
                  i++;
                  break;
               }
               lpos = pos;
            }
            s->exponent_sizes[0] = i;
         }
         else
         {

            table = NULL;
            a = s->frame_len_bits - BLOCK_MIN_BITS - k;
            if (a < 3)
            {
               if (s->sample_rate >= 44100)
                  table = exponent_band_44100[a];
               else if (s->sample_rate >= 32000)
                  table = exponent_band_32000[a];
               else if (s->sample_rate >= 22050)
                  table = exponent_band_22050[a];
            }
            if (table)
            {
               n = *table++;
               for (i = 0; i < n; i++)
                  s->exponent_bands[k][i] = table[i];
               s->exponent_sizes[k] = n;
            }
            else
            {
               j = 0;
               lpos = 0;
               for (i = 0; i < 25; i++)
               {
                  a = ff_wma_critical_freqs[i];
                  b = s->sample_rate;
                  pos = ((block_len * 2 * a) + (b << 1)) / (4 * b);
                  pos <<= 2;
                  if (pos > block_len)
                     pos = block_len;
                  if (pos > lpos)
                     s->exponent_bands[k][j++] = pos - lpos;
                  if (pos >= block_len)
                     break;
                  lpos = pos;
               }
               s->exponent_sizes[k] = j;
            }
         }

         s->coefs_end[k] = (s->frame_len - ((s->frame_len * 9) / 100)) >> k;

         s->high_band_start[k] =
             (int)((block_len * 2 * high_freq) / s->sample_rate + 0.5);
         n = s->exponent_sizes[k];
         j = 0;
         pos = 0;
         for (i = 0; i < n; i++)
         {
            int start, end;
            start = pos;
            pos += s->exponent_bands[k][i];
            end = pos;
            if (start < s->high_band_start[k])
               start = s->high_band_start[k];
            if (end > s->coefs_end[k])
               end = s->coefs_end[k];
            if (end > start)
               s->exponent_high_bands[k][j++] = end - start;
         }
         s->exponent_high_sizes[k] = j;
      }
   }

   for (i = 0; i < s->nb_block_sizes; i++)
   {
      (void)0;
      s->windows[i] = s->mdct.windows[s->frame_len_bits - i - 6];
   }

   s->reset_block_lengths = 1;

   if (s->use_noise_coding)
   {

      if (s->use_exp_vlc)
         s->noise_mult = 0.02;
      else
         s->noise_mult = 0.04;

      {
         unsigned int seed;
         float norm;
         seed = 1;
         norm = (1.0 / (float)(2147483648.0)) * sqrt(3) * s->noise_mult;
         for (i = 0; i < NOISE_TAB_SIZE; i++)
         {
            seed = seed * 314159 + 1;
            s->noise_table[i] = (float)((int)seed) * norm;
         }
      }
   }

   coef_vlc_table = 2;
   if (s->sample_rate >= 32000)
   {
      if (bps1 < 0.72)
         coef_vlc_table = 0;
      else if (bps1 < 1.16)
         coef_vlc_table = 1;
   }
   s->codebook[0] = &std_codebooks[coef_vlc_table * 2];
   s->codebook[1] = &std_codebooks[coef_vlc_table * 2 + 1];
   return RWMA_OK;
}
static const float pow_tab[] = {
    1.7782794100389e-04, 2.0535250264571e-04, 2.3713737056617e-04, 2.7384196342644e-04,
    3.1622776601684e-04, 3.6517412725484e-04, 4.2169650342858e-04, 4.8696752516586e-04,
    5.6234132519035e-04, 6.4938163157621e-04, 7.4989420933246e-04, 8.6596432336006e-04,
    1.0000000000000e-03, 1.1547819846895e-03, 1.3335214321633e-03, 1.5399265260595e-03,
    1.7782794100389e-03, 2.0535250264571e-03, 2.3713737056617e-03, 2.7384196342644e-03,
    3.1622776601684e-03, 3.6517412725484e-03, 4.2169650342858e-03, 4.8696752516586e-03,
    5.6234132519035e-03, 6.4938163157621e-03, 7.4989420933246e-03, 8.6596432336006e-03,
    1.0000000000000e-02, 1.1547819846895e-02, 1.3335214321633e-02, 1.5399265260595e-02,
    1.7782794100389e-02, 2.0535250264571e-02, 2.3713737056617e-02, 2.7384196342644e-02,
    3.1622776601684e-02, 3.6517412725484e-02, 4.2169650342858e-02, 4.8696752516586e-02,
    5.6234132519035e-02, 6.4938163157621e-02, 7.4989420933246e-02, 8.6596432336007e-02,
    1.0000000000000e-01, 1.1547819846895e-01, 1.3335214321633e-01, 1.5399265260595e-01,
    1.7782794100389e-01, 2.0535250264571e-01, 2.3713737056617e-01, 2.7384196342644e-01,
    3.1622776601684e-01, 3.6517412725484e-01, 4.2169650342858e-01, 4.8696752516586e-01,
    5.6234132519035e-01, 6.4938163157621e-01, 7.4989420933246e-01, 8.6596432336007e-01,
    1.0000000000000e+00, 1.1547819846895e+00, 1.3335214321633e+00, 1.5399265260595e+00,
    1.7782794100389e+00, 2.0535250264571e+00, 2.3713737056617e+00, 2.7384196342644e+00,
    3.1622776601684e+00, 3.6517412725484e+00, 4.2169650342858e+00, 4.8696752516586e+00,
    5.6234132519035e+00, 6.4938163157621e+00, 7.4989420933246e+00, 8.6596432336007e+00,
    1.0000000000000e+01, 1.1547819846895e+01, 1.3335214321633e+01, 1.5399265260595e+01,
    1.7782794100389e+01, 2.0535250264571e+01, 2.3713737056617e+01, 2.7384196342644e+01,
    3.1622776601684e+01, 3.6517412725484e+01, 4.2169650342858e+01, 4.8696752516586e+01,
    5.6234132519035e+01, 6.4938163157621e+01, 7.4989420933246e+01, 8.6596432336007e+01,
    1.0000000000000e+02, 1.1547819846895e+02, 1.3335214321633e+02, 1.5399265260595e+02,
    1.7782794100389e+02, 2.0535250264571e+02, 2.3713737056617e+02, 2.7384196342644e+02,
    3.1622776601684e+02, 3.6517412725484e+02, 4.2169650342858e+02, 4.8696752516586e+02,
    5.6234132519035e+02, 6.4938163157621e+02, 7.4989420933246e+02, 8.6596432336007e+02,
    1.0000000000000e+03, 1.1547819846895e+03, 1.3335214321633e+03, 1.5399265260595e+03,
    1.7782794100389e+03, 2.0535250264571e+03, 2.3713737056617e+03, 2.7384196342644e+03,
    3.1622776601684e+03, 3.6517412725484e+03, 4.2169650342858e+03, 4.8696752516586e+03,
    5.6234132519035e+03, 6.4938163157621e+03, 7.4989420933246e+03, 8.6596432336007e+03,
    1.0000000000000e+04, 1.1547819846895e+04, 1.3335214321633e+04, 1.5399265260595e+04,
    1.7782794100389e+04, 2.0535250264571e+04, 2.3713737056617e+04, 2.7384196342644e+04,
    3.1622776601684e+04, 3.6517412725484e+04, 4.2169650342858e+04, 4.8696752516586e+04,
    5.6234132519035e+04, 6.4938163157621e+04, 7.4989420933246e+04, 8.6596432336007e+04,
    1.0000000000000e+05, 1.1547819846895e+05, 1.3335214321633e+05, 1.5399265260595e+05,
    1.7782794100389e+05, 2.0535250264571e+05, 2.3713737056617e+05, 2.7384196342644e+05,
    3.1622776601684e+05, 3.6517412725484e+05, 4.2169650342858e+05, 4.8696752516586e+05,
    5.6234132519035e+05, 6.4938163157621e+05, 7.4989420933246e+05, 8.6596432336007e+05,
};
static float pow_m1_4(rwma_standard *s, float x)
{
   union
   {
      float f;
      unsigned int v;
   } u, t;
   unsigned int e, m;
   float a, b;

   u.f = x;
   e = u.v >> 23;
   m = (u.v >> (23 - LSP_POW_BITS)) & ((1 << LSP_POW_BITS) - 1);

   t.v = ((u.v << LSP_POW_BITS) & ((1 << 23) - 1)) | (127 << 23);
   a = s->lsp_pow_m_table1[m];
   b = s->lsp_pow_m_table2[m];
   return s->lsp_pow_e_table[e] * (a + b * t.f);
}

static void wma_lsp_to_curve_init(rwma_standard *s, int frame_len)
{
   float wdel, a, b;
   int i, e, m;

   wdel = RWMA_PI / frame_len;
   for (i = 0; i < frame_len; i++)
      s->lsp_cos_table[i] = 2.0f * cos(wdel * i);

   for (i = 0; i < 256; i++)
   {
      e = i - 126;
      s->lsp_pow_e_table[i] = pow(2.0, e * -0.25);
   }

   b = 1.0;
   for (i = (1 << LSP_POW_BITS) - 1; i >= 0; i--)
   {
      m = (1 << LSP_POW_BITS) + i;
      a = (float)m * (0.5 / (1 << LSP_POW_BITS));
      a = 1 / sqrt(sqrt(a));
      s->lsp_pow_m_table1[i] = 2 * a - b;
      s->lsp_pow_m_table2[i] = b - a;
      b = a;
   }
}

static void wma_lsp_to_curve(rwma_standard *s, float *out, float *val_max_ptr, int n,
                             float *lsp)
{
   int i, j;
   float p, q, w, v, val_max;

   val_max = 0;
   for (i = 0; i < n; i++)
   {
      p = 0.5f;
      q = 0.5f;
      w = s->lsp_cos_table[i];
      for (j = 1; j < NB_LSP_COEFS; j += 2)
      {
         q *= w - lsp[j - 1];
         p *= w - lsp[j];
      }
      p *= p * (2.0f - w);
      q *= q * (2.0f + w);
      v = p + q;
      v = pow_m1_4(s, v);
      if (v > val_max)
         val_max = v;
      out[i] = v;
   }
   *val_max_ptr = val_max;
}

static void decode_exp_lsp(rwma_standard *s, int ch)
{
   float lsp_coefs[NB_LSP_COEFS];
   int val, i;

   for (i = 0; i < NB_LSP_COEFS; i++)
   {
      if (i == 0 || i >= 8)
         val = get_bits(&s->gb, 3);
      else
         val = get_bits(&s->gb, 4);
      lsp_coefs[i] = ff_wma_lsp_codebook[i][val];
   }

   wma_lsp_to_curve(s, s->exponents[ch], &s->max_exponent[ch], s->block_len, lsp_coefs);
}

static int decode_exp_vlc(rwma_standard *s, int ch)
{
   int last_exp, n, code;
   const uint16_t *ptr;
   float v, max_scale;
   float *q, *q_end;
   const float *ptab = pow_tab + 60;

   ptr = s->exponent_bands[s->frame_len_bits - s->block_len_bits];
   q = s->exponents[ch];
   q_end = q + s->block_len;
   max_scale = 0;
   if (s->version == 1)
   {
      last_exp = get_bits(&s->gb, 5) + 10;
      v = ptab[last_exp];
      max_scale = v;
      n = *ptr++;
      if (n < 1 || n > q_end - q)
         return RWMA_BAD_DATA;
      while (n--)
         *q++ = v;
   }
   else
      last_exp = 36;

   while (q < q_end)
   {
      code = rwma_vlc(&s->gb, std_exp_vlc);

      last_exp += code - 60;
      if ((unsigned)last_exp + 60 >= RWMA_ARRAY_SIZE(pow_tab))
      {
         (void)0;
         return RWMA_BAD_DATA;
      }
      v = ptab[last_exp];
      if (v > max_scale)
         max_scale = v;
      n = *ptr++;
      if (n < 1 || n > q_end - q)
         return RWMA_BAD_DATA;
      while (n--)
         *q++ = v;
   }
   s->max_exponent[ch] = max_scale;
   return 0;
}

static void wma_window(rwma_standard *s, float *out)
{
   float *in = s->output;
   int block_len, bsize, n;

   if (s->block_len_bits <= s->prev_block_len_bits)
   {
      block_len = s->block_len;
      bsize = s->frame_len_bits - s->block_len_bits;

      rwma_multiply_add(out, in, s->windows[bsize], out, block_len);
   }
   else
   {
      block_len = 1 << s->prev_block_len_bits;
      n = (s->block_len - block_len) / 2;
      bsize = s->frame_len_bits - s->prev_block_len_bits;

      rwma_multiply_add(out + n, in + n, s->windows[bsize], out + n, block_len);

      memcpy(out + n + block_len, in + n + block_len, n * sizeof(float));
   }

   out += s->block_len;
   in += s->block_len;

   if (s->block_len_bits <= s->next_block_len_bits)
   {
      block_len = s->block_len;
      bsize = s->frame_len_bits - s->block_len_bits;

      rwma_multiply_reverse(out, in, s->windows[bsize], block_len);
   }
   else
   {
      block_len = 1 << s->next_block_len_bits;
      n = (s->block_len - block_len) / 2;
      bsize = s->frame_len_bits - s->next_block_len_bits;

      memcpy(out, in, n * sizeof(float));

      rwma_multiply_reverse(out + n, in + n, s->windows[bsize], block_len);

      memset(out + n + block_len, 0, n * sizeof(float));
   }
}

static int wma_decode_block(rwma_standard *s)
{
   int channels = s->channels;
   int n, v, a, bsize, i, j;
   int coef_nb_bits, total_gain;
   int nb_coefs[MAX_CHANNELS];
   float mdct_norm;
   int ch;

   assert(channels <= MAX_CHANNELS);

   if (s->use_variable_block_len)
   {
      n = rwma_log2(s->nb_block_sizes - 1) + 1;

      if (s->reset_block_lengths)
      {
         s->reset_block_lengths = 0;
         v = get_bits(&s->gb, n);
         if (v >= s->nb_block_sizes)
         {
            (void)0;
            return RWMA_BAD_DATA;
         }
         s->prev_block_len_bits = s->frame_len_bits - v;
         v = get_bits(&s->gb, n);
         if (v >= s->nb_block_sizes)
         {
            (void)0;
            return RWMA_BAD_DATA;
         }
         s->block_len_bits = s->frame_len_bits - v;
      }
      else
      {

         s->prev_block_len_bits = s->block_len_bits;
         s->block_len_bits = s->next_block_len_bits;
      }
      v = get_bits(&s->gb, n);
      if (v >= s->nb_block_sizes)
      {
         (void)0;
         return RWMA_BAD_DATA;
      }
      s->next_block_len_bits = s->frame_len_bits - v;
   }
   else
   {

      s->next_block_len_bits = s->frame_len_bits;
      s->prev_block_len_bits = s->frame_len_bits;
      s->block_len_bits = s->frame_len_bits;
   }

   if (s->frame_len_bits - s->block_len_bits >= s->nb_block_sizes)
   {
      (void)0;
      return RWMA_BAD_DATA;
   }

   s->block_len = 1 << s->block_len_bits;
   if ((s->block_pos + s->block_len) > s->frame_len)
   {
      (void)0;
      return RWMA_BAD_DATA;
   }

   if (channels == 2)
      s->ms_stereo = get_bits1(&s->gb);
   v = 0;
   for (ch = 0; ch < channels; ch++)
   {
      a = get_bits1(&s->gb);
      s->channel_coded[ch] = a;
      v |= a;
   }

   bsize = s->frame_len_bits - s->block_len_bits;

   if (!v)
      goto next;

   total_gain = 1;
   for (;;)
   {
      if (get_bits_left(&s->gb) < 7)
      {
         (void)0;
         return RWMA_BAD_DATA;
      }
      a = get_bits(&s->gb, 7);
      total_gain += a;
      if (a != 127)
         break;
   }

   coef_nb_bits = rwma_gain_bits(total_gain);

   n = s->coefs_end[bsize] - s->coefs_start;
   for (ch = 0; ch < channels; ch++)
      nb_coefs[ch] = n;

   if (s->use_noise_coding)
   {
      for (ch = 0; ch < channels; ch++)
      {
         if (s->channel_coded[ch])
         {
            int n;
            n = s->exponent_high_sizes[bsize];
            for (i = 0; i < n; i++)
            {
               const unsigned a = get_bits1(&s->gb);
               s->high_band_coded[ch][i] = a;

               if (a)
                  nb_coefs[ch] -= s->exponent_high_bands[bsize][i];
            }
         }
      }
      for (ch = 0; ch < channels; ch++)
      {
         if (s->channel_coded[ch])
         {
            int n, val;

            n = s->exponent_high_sizes[bsize];
            val = (int)0x80000000;
            for (i = 0; i < n; i++)
            {
               if (s->high_band_coded[ch][i])
               {
                  if (val == (int)0x80000000)
                  {
                     val = get_bits(&s->gb, 7) - 19;
                  }
                  else
                  {
                     val += rwma_vlc(&s->gb, std_hgain_vlc);
                  }
                  s->high_band_values[ch][i] = val;
               }
            }
         }
      }
   }

   if ((s->block_len_bits == s->frame_len_bits) || get_bits1(&s->gb))
   {
      for (ch = 0; ch < channels; ch++)
      {
         if (s->channel_coded[ch])
         {
            if (s->use_exp_vlc)
            {
               if (decode_exp_vlc(s, ch) < 0)
                  return RWMA_BAD_DATA;
            }
            else
            {
               decode_exp_lsp(s, ch);
            }
            s->exponents_bsize[ch] = bsize;
            s->exponents_initialized[ch] = 1;
         }
      }
   }

   for (ch = 0; ch < channels; ch++)
   {
      if (s->channel_coded[ch] && !s->exponents_initialized[ch])
         return RWMA_BAD_DATA;
   }

   for (ch = 0; ch < channels; ch++)
   {
      if (s->channel_coded[ch])
      {
         int tindex;
         float *ptr = &s->coefs1[ch][0];
         int ret;

         tindex = (ch == 1 && s->ms_stereo);
         memset(ptr, 0, s->block_len * sizeof(float));
         ret = rwma_run_level(&s->gb, s->codebook[tindex]->vlc,
                              s->codebook[tindex]->levels, s->codebook[tindex]->runs, 0,
                              ptr, 0, nb_coefs[ch], s->block_len, s->frame_len_bits,
                              coef_nb_bits);
         if (ret < 0)
            return ret;
      }
      if (s->version == 1 && channels >= 2)
         align_get_bits(&s->gb);
   }

   {
      int n4 = s->block_len / 2;
      mdct_norm = 1.0 / (float)n4;
      if (s->version == 1)
         mdct_norm *= sqrt(n4);
   }

   for (ch = 0; ch < channels; ch++)
   {
      if (s->channel_coded[ch])
      {
         float *coefs1;
         float *coefs, *exponents, mult, mult1, noise;
         int n, n1, last_high_band, esize;
         float exp_power[HIGH_BAND_MAX_SIZE];

         coefs1 = s->coefs1[ch];
         exponents = s->exponents[ch];
         esize = s->exponents_bsize[ch];
         mult = rwma_exp10(total_gain * 0.05) / s->max_exponent[ch];
         mult *= mdct_norm;
         coefs = s->coefs[ch];
         if (s->use_noise_coding)
         {
            mult1 = mult;

            for (i = 0; i < s->coefs_start; i++)
            {
               *coefs++ = s->noise_table[s->noise_index] *
                          exponents[i << bsize >> esize] * mult1;
               s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
            }

            n1 = s->exponent_high_sizes[bsize];

            exponents =
                s->exponents[ch] + (s->high_band_start[bsize] << bsize >> esize);
            last_high_band = 0;
            for (j = 0; j < n1; j++)
            {
               n = s->exponent_high_bands[s->frame_len_bits - s->block_len_bits][j];
               if (s->high_band_coded[ch][j])
               {
                  float e2, v;
                  e2 = 0;
                  for (i = 0; i < n; i++)
                  {
                     v = exponents[i << bsize >> esize];
                     e2 += v * v;
                  }
                  exp_power[j] = e2 / n;
                  last_high_band = j;
                  (void)0;
               }
               exponents += n << bsize >> esize;
            }

            exponents = s->exponents[ch] + (s->coefs_start << bsize >> esize);
            for (j = -1; j < n1; j++)
            {
               if (j < 0)
                  n = s->high_band_start[bsize] - s->coefs_start;
               else
                  n = s->exponent_high_bands[s->frame_len_bits - s->block_len_bits][j];
               if (j >= 0 && s->high_band_coded[ch][j])
               {

                  mult1 = sqrt(exp_power[j] / exp_power[last_high_band]);

                  mult1 = mult1 * rwma_exp10(s->high_band_values[ch][j] * 0.05);
                  mult1 = mult1 / (s->max_exponent[ch] * s->noise_mult);
                  mult1 *= mdct_norm;
                  for (i = 0; i < n; i++)
                  {
                     noise = s->noise_table[s->noise_index];
                     s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                     *coefs++ = noise * exponents[i << bsize >> esize] * mult1;
                  }
                  exponents += n << bsize >> esize;
               }
               else
               {

                  for (i = 0; i < n; i++)
                  {
                     noise = s->noise_table[s->noise_index];
                     s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                     *coefs++ =
                         ((*coefs1++) + noise) * exponents[i << bsize >> esize] * mult;
                  }
                  exponents += n << bsize >> esize;
               }
            }

            n = s->block_len - s->coefs_end[bsize];
            mult1 = mult * exponents[(-(1 << bsize)) >> esize];
            for (i = 0; i < n; i++)
            {
               *coefs++ = s->noise_table[s->noise_index] * mult1;
               s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
            }
         }
         else
         {

            for (i = 0; i < s->coefs_start; i++)
               *coefs++ = 0.0;
            n = nb_coefs[ch];
            for (i = 0; i < n; i++)
               *coefs++ = coefs1[i] * exponents[i << bsize >> esize] * mult;
            n = s->block_len - s->coefs_end[bsize];
            for (i = 0; i < n; i++)
               *coefs++ = 0.0;
         }
      }
   }

   if (s->ms_stereo && s->channel_coded[1])
   {

      if (!s->channel_coded[0])
      {
         (void)0;
         memset(s->coefs[0], 0, sizeof(float) * s->block_len);
         s->channel_coded[0] = 1;
      }

      rwma_butterfly(s->coefs[0], s->coefs[1], s->block_len);
   }

next:

   for (ch = 0; ch < channels; ch++)
   {
      int n4, index;

      n4 = s->block_len / 2;
      if (s->channel_coded[ch])
         rwma_std_imdct(s, s->output, s->coefs[ch], s->block_len);
      else if (!(s->ms_stereo && ch == 1))
         memset(s->output, 0, sizeof(s->output));

      index = (s->frame_len / 2) + s->block_pos - n4;
      wma_window(s, &s->frame_out[ch][index]);
   }

   s->block_num++;
   s->block_pos += s->block_len;
   if (s->block_pos >= s->frame_len)
      return 1;
   else
      return 0;
}

static int wma_decode_frame(rwma_standard *s, float **samples, int samples_offset)
{

   s->block_num = 0;
   s->block_pos = 0;
   for (;;)
   {
      int ret = wma_decode_block(s);
      if (ret < 0)
         return ret;
      if (ret)
         break;
   }

   (void)samples;
   (void)samples_offset;
   s->pending_shift = 1;
   return 0;
}
void rwma_standard_free(void *opaque) { free(opaque); }

void rwma_standard_reset(void *opaque)
{
   rwma_standard *s = (rwma_standard *)opaque;
   memset(s->frame_out, 0, sizeof(s->frame_out));
   memset(s->exponents_initialized, 0, sizeof(s->exponents_initialized));
   s->last_superframe_len = s->last_bitoffset = 0;
   s->noise_index = s->eof_done_native = s->failed = s->pending_shift = 0;
   s->in_packet = s->frames_left = s->cross_frame = 0;
   s->decoded_frames = 0;
   s->delay_left = 2 * s->frame_len;
   s->reset_block_lengths = 1;
   s->block_len_bits = s->prev_block_len_bits = s->next_block_len_bits =
       s->frame_len_bits;
}

int rwma_standard_create(void **out, const rwma_config *cfg)
{
   rwma_standard *s;
   unsigned flags, offset, c;
   int ret;
   *out = NULL;
   offset = cfg->codec == RWMA_V1 ? 2 : 4;
   if (cfg->channels > 2 || cfg->channels == 0 || cfg->sample_rate > 50000 ||
       cfg->sample_rate < 8000 || cfg->bit_rate == 0 || cfg->bit_rate > INT_MAX ||
       cfg->block_align < 1 || cfg->block_align > MAX_CODED_SUPERFRAME_SIZE ||
       !cfg->extra || cfg->extra_size < offset + 2)
      return RWMA_BAD_DATA;
   s = (rwma_standard *)calloc(1, sizeof(*s));
   if (!s)
      return RWMA_NO_MEMORY;
   s->channels = cfg->channels;
   s->sample_rate = cfg->sample_rate;
   s->bit_rate = cfg->bit_rate;
   s->block_align = cfg->block_align;
   s->version = cfg->codec == RWMA_V1 ? 1 : 2;
   flags = cfg->extra[offset] | (cfg->extra[offset + 1] << 8);
   s->use_exp_vlc = flags & 1;
   s->use_bit_reservoir = flags & 2;
   s->use_variable_block_len = flags & 4;
   if (s->version == 2 && cfg->extra_size >= 8 && flags == 0xd)
      s->use_variable_block_len = 0;
   rwma_mdct_init(&s->mdct, s->math_storage, 2048);
   ret = rwma_std_init(s, flags);
   if (ret < 0)
   {
      free(s);
      return ret;
   }
   if (!s->use_exp_vlc)
      wma_lsp_to_curve_init(s, s->frame_len);
   for (c = 0; c < 2; c++)
      s->max_exponent[c] = 1;
   rwma_standard_reset(s);
   *out = s;
   return RWMA_OK;
}

static void rwma_standard_shift(rwma_standard *s)
{
   int c;
   if (!s->pending_shift)
      return;
   for (c = 0; c < s->channels; c++)
      memmove(s->frame_out[c], s->frame_out[c] + s->frame_len,
              s->frame_len * sizeof(float));
   s->pending_shift = 0;
}

int rwma_standard_decode(void *opaque, const uint8_t *data, size_t bytes,
                         size_t *consumed, rwma_frame *frame)
{
   rwma_standard *s = (rwma_standard *)opaque;
   unsigned c, block, nb_frames;
   int ret, bit_offset, pos, len;
   rwma_bits new_reader;
   if (!s || s->failed || (!data && bytes) || bytes > INT_MAX / 8)
      return RWMA_BAD_DATA;
   rwma_standard_shift(s);
   if (!bytes)
   {
      if (s->in_packet)
         return RWMA_BAD_DATA;
      if (s->eof_done_native || !s->decoded_frames)
         return RWMA_OK;
      s->eof_done_native = 1;
      goto output;
   }
   block = s->block_align;
   if (bytes < block)
      return RWMA_BAD_DATA;
   if (!s->in_packet)
   {
      init_get_bits8(&new_reader, data, block);
      s->packet_pos = 0;
      s->cross_frame = 0;
      nb_frames = 1;
      if (s->use_bit_reservoir)
      {
         skip_bits(&new_reader, 4);
         nb_frames = get_bits(&new_reader, 4);
         if (!nb_frames)
         {
            if (!s->last_superframe_len ||
                s->last_superframe_len > MAX_CODED_SUPERFRAME_SIZE - (int)block + 1)
               goto bad;
            memcpy(s->last_superframe + s->last_superframe_len, data + 1, block - 1);
            s->last_superframe_len += block - 1;
            *consumed = block;
            return RWMA_OK;
         }
         if (!s->last_superframe_len)
            nb_frames--;
         bit_offset = get_bits(&new_reader, s->byte_offset_bits + 3);
         if (new_reader.error || bit_offset > get_bits_left(&new_reader))
            goto bad;
         pos = new_reader.pos + bit_offset;
         if (s->last_superframe_len)
         {
            rwma_bits *b = &new_reader;
            unsigned dst = s->last_superframe_len * 8;
            if (dst > MAX_CODED_SUPERFRAME_SIZE * 8u - bit_offset)
               goto bad;
            len = bit_offset;
            while (len)
            {
               unsigned take = RWMA_MIN(len, 8), value = get_bits(b, take);
               s->last_superframe[dst / 8] = (uint8_t)(value << (8 - take));
               dst += take;
               len -= take;
            }
            init_get_bits(&s->gb, s->last_superframe,
                          s->last_superframe_len * 8 + bit_offset);
            skip_bits(&s->gb, s->last_bitoffset);
            s->cross_frame = 1;
         }
         init_get_bits8(&new_reader, data, block);
         skip_bits(&new_reader, pos);
         s->reset_block_lengths = 1;
      }
      s->packet_reader = new_reader;
      s->packet_data = data;
      s->frames_left = nb_frames;
      s->in_packet = 1;
   }
   else if (data != s->packet_data)
      return RWMA_BAD_DATA;
   if (!s->frames_left)
      goto packet_end;
   if (!s->cross_frame)
      s->gb = s->packet_reader;
   ret = wma_decode_frame(s, NULL, 0);
   if (ret < 0 || s->gb.error)
      goto bad;
   s->decoded_frames++;
   if (s->cross_frame)
      s->cross_frame = 0;
   else
      s->packet_reader = s->gb;
   s->frames_left--;
   if (s->frames_left)
      goto output;
packet_end:
   if (s->use_bit_reservoir)
   {
      pos = s->packet_reader.pos;
      s->last_bitoffset = pos & 7;
      pos >>= 3;
      len = block - pos;
      if (len < 0 || len > MAX_CODED_SUPERFRAME_SIZE)
         goto bad;
      s->last_superframe_len = len;
      memcpy(s->last_superframe, data + pos, len);
   }
   s->in_packet = 0;
   s->packet_data = NULL;
   *consumed = block;
   if (!s->pending_shift)
      return RWMA_OK;
output:
   frame->samples = s->frame_len;
   frame->channels = s->channels;
   frame->sample_rate = s->sample_rate;
   frame->trim_start = RWMA_MIN(s->delay_left, frame->samples);
   s->delay_left -= frame->trim_start;
   for (c = 0; c < (unsigned)s->channels; c++)
      frame->planes[c] = s->frame_out[c];
   if (!rwma_frame_valid(frame))
   {
      memset(frame, 0, sizeof(*frame));
      goto bad;
   }
   return RWMA_OK;
bad:
   s->failed = 1;
   return RWMA_BAD_DATA;
}
