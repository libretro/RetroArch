/*
 * Wmapro compatible decoder
 * Copyright (c) 2007 Baptiste Coudurier, Benjamin Larsson, Ulion
 * Copyright (c) 2008 - 2011 Sascha Sommer, Benjamin Larsson
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
#include "rwma_pro_tables.h"
typedef struct rwma_channel
{
   int16_t prev_block_len;
   uint8_t transmit_coefs;
   uint8_t num_subframes;
   uint16_t subframe_len[MAX_SUBFRAMES];
   uint16_t subframe_offset[MAX_SUBFRAMES];
   uint8_t cur_subframe;
   uint16_t decoded_samples;
   uint8_t grouped;
   int quant_step;
   int8_t reuse_sf;
   int8_t scale_factor_step;
   int max_scale_factor;
   int saved_scale_factors[2][MAX_BANDS];
   int8_t scale_factor_idx;
   int *scale_factors;
   uint8_t table_idx;
   float *coeffs;
   uint16_t num_vec_coeffs;
   float *out;
} rwma_channel;

typedef struct rwma_group
{
   uint8_t num_channels;
   int8_t transform;
   int8_t transform_band[MAX_BANDS];
   float decorrelation_matrix[WMAPRO_MAX_CHANNELS * WMAPRO_MAX_CHANNELS];
   float *channel_data[WMAPRO_MAX_CHANNELS];
} rwma_group;

struct rwma_t
{
   void *standard;
   float *storage, *tmp;
   rwma_mdct mdct;
   const float *windows[8];
   rwma_channel channel[WMAPRO_MAX_CHANNELS];
   rwma_group chgroup[WMAPRO_MAX_CHANNELS];
   rwma_bits gb;
   rwma_bits pgb;
   uint8_t *reservoir;
   int block_align, next_packet_start, packet_offset, packet_done, eof_done;
   int packet_sequence_number, skip_packets;
   unsigned codec, sample_rate, pending_shift, failed, max_frame;
   unsigned frame_num;
   int nb_channels, samples_per_frame, len_prefix, log2_frame_size, bits_per_sample;
   int max_num_subframes, max_subframe_len_bit, subframe_len_bits;
   int min_samples_per_subframe, dynamic_range_compression;
   int lfe_channel, skip_frame, packet_loss, parsed_all_subframes;
   int channels_for_cur_subframe, subframe_len, num_bands;
   int transmit_num_vec_coeffs, table_idx, esc_len, num_chgroups;
   int subframe_offset, frame_offset, num_saved_bits, buf_bit_size;
   int trim_start, trim_end, drc_gain;
   int8_t channel_indexes_for_cur_subframe[8];
   int8_t num_sfb[8];
   int16_t sfb_offsets[8][29];
   int8_t sf_offsets[8][8][29];
   int16_t subwoofer_cutoffs[8];
   int16_t *cur_sfb_offsets;
   float sin64[33];
};

static void rwma_imdct(rwma_t *s, float *dst, const float *x, int n)
{
   float scale = (float)(1.0 / (n / 2) / pow(2.0, s->bits_per_sample - 1));
   rwma_mdct_run(&s->mdct, dst, x, n, scale);
}

static int decode_subframe_length(rwma_t *s, int offset)
{
   int frame_len_shift = 0;
   int subframe_len;

   if (offset == s->samples_per_frame - s->min_samples_per_subframe)
      return s->min_samples_per_subframe;

   if (get_bits_left(&s->gb) < 1)
      return RWMA_BAD_DATA;

   if (s->max_subframe_len_bit)
   {
      if (get_bits1(&s->gb))
         frame_len_shift = 1 + get_bits(&s->gb, s->subframe_len_bits - 1);
   }
   else
      frame_len_shift = get_bits(&s->gb, s->subframe_len_bits);

   subframe_len = s->samples_per_frame >> frame_len_shift;

   if (subframe_len < s->min_samples_per_subframe ||
       subframe_len > s->samples_per_frame)
   {
      (void)0;
      return RWMA_BAD_DATA;
   }
   return subframe_len;
}

static int decode_tilehdr(rwma_t *s)
{
   uint16_t num_samples[WMAPRO_MAX_CHANNELS] = {0};
   uint8_t contains_subframe[WMAPRO_MAX_CHANNELS];
   int channels_for_cur_subframe = s->nb_channels;
   int fixed_channel_layout = 0;
   int min_channel_len = 0;
   int c;

   for (c = 0; c < s->nb_channels; c++)
      s->channel[c].num_subframes = 0;

   if (s->max_num_subframes == 1 || get_bits1(&s->gb))
      fixed_channel_layout = 1;

   do
   {
      int subframe_len;

      for (c = 0; c < s->nb_channels; c++)
      {
         if (num_samples[c] == min_channel_len)
         {
            if (fixed_channel_layout || channels_for_cur_subframe == 1 ||
                (min_channel_len == s->samples_per_frame - s->min_samples_per_subframe))
               contains_subframe[c] = 1;
            else
               contains_subframe[c] = get_bits1(&s->gb);
         }
         else
            contains_subframe[c] = 0;
      }

      if ((subframe_len = decode_subframe_length(s, min_channel_len)) <= 0)
         return RWMA_BAD_DATA;

      min_channel_len += subframe_len;
      for (c = 0; c < s->nb_channels; c++)
      {
         rwma_channel *chan = &s->channel[c];

         if (contains_subframe[c])
         {
            if (chan->num_subframes >= MAX_SUBFRAMES)
            {
               (void)0;
               return RWMA_BAD_DATA;
            }
            chan->subframe_len[chan->num_subframes] = subframe_len;
            num_samples[c] += subframe_len;
            ++chan->num_subframes;
            if (num_samples[c] > s->samples_per_frame)
            {
               (void)0;
               return RWMA_BAD_DATA;
            }
         }
         else if (num_samples[c] <= min_channel_len)
         {
            if (num_samples[c] < min_channel_len)
            {
               channels_for_cur_subframe = 0;
               min_channel_len = num_samples[c];
            }
            ++channels_for_cur_subframe;
         }
      }
   } while (min_channel_len < s->samples_per_frame);

   for (c = 0; c < s->nb_channels; c++)
   {
      int i;
      int offset = 0;
      for (i = 0; i < s->channel[c].num_subframes; i++)
      {
         (void)0;
         s->channel[c].subframe_offset[i] = offset;
         offset += s->channel[c].subframe_len[i];
      }
   }

   return 0;
}

static void decode_decorrelation_matrix(rwma_t *s, rwma_group *chgroup)
{
   int i;
   int offset = 0;
   int8_t rotation_offset[WMAPRO_MAX_CHANNELS * WMAPRO_MAX_CHANNELS];
   memset(chgroup->decorrelation_matrix, 0,
          s->nb_channels * s->nb_channels * sizeof(*chgroup->decorrelation_matrix));

   for (i = 0; i < chgroup->num_channels * (chgroup->num_channels - 1) >> 1; i++)
      rotation_offset[i] = get_bits(&s->gb, 6);

   for (i = 0; i < chgroup->num_channels; i++)
      chgroup->decorrelation_matrix[chgroup->num_channels * i + i] =
          get_bits1(&s->gb) ? 1.0 : -1.0;

   for (i = 1; i < chgroup->num_channels; i++)
   {
      int x;
      for (x = 0; x < i; x++)
      {
         int y;
         for (y = 0; y < i + 1; y++)
         {
            float v1 = chgroup->decorrelation_matrix[x * chgroup->num_channels + y];
            float v2 = chgroup->decorrelation_matrix[i * chgroup->num_channels + y];
            int n = rotation_offset[offset + x];
            float sinv;
            float cosv;

            if (n < 32)
            {
               sinv = s->sin64[n];
               cosv = s->sin64[32 - n];
            }
            else
            {
               sinv = s->sin64[64 - n];
               cosv = -s->sin64[n - 32];
            }

            chgroup->decorrelation_matrix[y + x * chgroup->num_channels] =
                (v1 * sinv) - (v2 * cosv);
            chgroup->decorrelation_matrix[y + i * chgroup->num_channels] =
                (v1 * cosv) + (v2 * sinv);
         }
      }
      offset += i;
   }
}

static int decode_channel_transform(rwma_t *s)
{
   int i;

   s->num_chgroups = 0;
   if (s->nb_channels > 1)
   {
      int remaining_channels = s->channels_for_cur_subframe;

      if (get_bits1(&s->gb))
      {
         (void)0;
         return RWMA_UNSUPPORTED;
      }

      for (s->num_chgroups = 0;
           remaining_channels && s->num_chgroups < s->channels_for_cur_subframe;
           s->num_chgroups++)
      {
         rwma_group *chgroup = &s->chgroup[s->num_chgroups];
         float **channel_data = chgroup->channel_data;
         chgroup->num_channels = 0;
         chgroup->transform = 0;

         if (remaining_channels > 2)
         {
            for (i = 0; i < s->channels_for_cur_subframe; i++)
            {
               int channel_idx = s->channel_indexes_for_cur_subframe[i];
               if (!s->channel[channel_idx].grouped && get_bits1(&s->gb))
               {
                  ++chgroup->num_channels;
                  s->channel[channel_idx].grouped = 1;
                  *channel_data++ = s->channel[channel_idx].coeffs;
               }
            }
         }
         else
         {
            chgroup->num_channels = remaining_channels;
            for (i = 0; i < s->channels_for_cur_subframe; i++)
            {
               int channel_idx = s->channel_indexes_for_cur_subframe[i];
               if (!s->channel[channel_idx].grouped)
                  *channel_data++ = s->channel[channel_idx].coeffs;
               s->channel[channel_idx].grouped = 1;
            }
         }

         if (chgroup->num_channels == 2)
         {
            if (get_bits1(&s->gb))
            {
               if (get_bits1(&s->gb))
               {
                  (void)0;
                  return RWMA_UNSUPPORTED;
               }
            }
            else
            {
               chgroup->transform = 1;
               if (s->nb_channels == 2)
               {
                  chgroup->decorrelation_matrix[0] = 1.0;
                  chgroup->decorrelation_matrix[1] = -1.0;
                  chgroup->decorrelation_matrix[2] = 1.0;
                  chgroup->decorrelation_matrix[3] = 1.0;
               }
               else
               {

                  chgroup->decorrelation_matrix[0] = 0.70703125;
                  chgroup->decorrelation_matrix[1] = -0.70703125;
                  chgroup->decorrelation_matrix[2] = 0.70703125;
                  chgroup->decorrelation_matrix[3] = 0.70703125;
               }
            }
         }
         else if (chgroup->num_channels > 2)
         {
            if (get_bits1(&s->gb))
            {
               chgroup->transform = 1;
               if (get_bits1(&s->gb))
               {
                  decode_decorrelation_matrix(s, chgroup);
               }
               else
               {

                  if (chgroup->num_channels > 6)
                  {
                     return RWMA_UNSUPPORTED;
                  }
                  else
                  {
                     memcpy(chgroup->decorrelation_matrix,
                            default_decorrelation[chgroup->num_channels],
                            chgroup->num_channels * chgroup->num_channels *
                                sizeof(*chgroup->decorrelation_matrix));
                  }
               }
            }
         }

         if (chgroup->transform)
         {
            if (!get_bits1(&s->gb))
            {
               int i;

               for (i = 0; i < s->num_bands; i++)
               {
                  chgroup->transform_band[i] = get_bits1(&s->gb);
               }
            }
            else
            {
               memset(chgroup->transform_band, 1, s->num_bands);
            }
         }
         remaining_channels -= chgroup->num_channels;
      }
   }
   return 0;
}

static int decode_coeffs(rwma_t *s, int c)
{

   static const uint32_t fval_tab[16] = {
       0x00000000, 0x3f800000, 0x40000000, 0x40400000, 0x40800000, 0x40a00000,
       0x40c00000, 0x40e00000, 0x41000000, 0x41100000, 0x41200000, 0x41300000,
       0x41400000, 0x41500000, 0x41600000, 0x41700000,
   };
   int vlctable;
   const rwma_vlc_node *vlc;
   rwma_channel *ci = &s->channel[c];
   int rl_mode = 0;
   int cur_coeff = 0;
   int num_zeros = 0;
   const uint16_t *run;
   const float *level;

   (void)0;

   vlctable = get_bits1(&s->gb);
   vlc = coef_vlc[vlctable];

   if (vlctable)
   {
      run = coef1_run;
      level = coef1_level;
   }
   else
   {
      run = coef0_run;
      level = coef0_level;
   }

   while ((s->transmit_num_vec_coeffs || !rl_mode) &&
          (cur_coeff + 3 < ci->num_vec_coeffs))
   {
      uint32_t vals[4];
      int i;
      unsigned int idx;

      idx = rwma_vlc(&s->gb, vec4_vlc);

      if (s->gb.error)
         return RWMA_BAD_DATA;
      if ((int)idx < 0)
      {
         for (i = 0; i < 4; i += 2)
         {
            idx = rwma_vlc(&s->gb, vec2_vlc);
            if (s->gb.error)
               return RWMA_BAD_DATA;
            if ((int)idx < 0)
            {
               uint32_t v0, v1;
               v0 = rwma_vlc(&s->gb, vec1_vlc);
               if (v0 == HUFF_VEC1_SIZE - 1)
                  v0 += rwma_large_value(&s->gb);
               v1 = rwma_vlc(&s->gb, vec1_vlc);
               if (v1 == HUFF_VEC1_SIZE - 1)
                  v1 += rwma_large_value(&s->gb);
               vals[i] = rwma_float_bits(v0);
               vals[i + 1] = rwma_float_bits(v1);
            }
            else
            {
               vals[i] = fval_tab[idx >> 4];
               vals[i + 1] = fval_tab[idx & 0xF];
            }
         }
      }
      else
      {
         vals[0] = fval_tab[idx >> 12];
         vals[1] = fval_tab[(idx >> 8) & 0xF];
         vals[2] = fval_tab[(idx >> 4) & 0xF];
         vals[3] = fval_tab[idx & 0xF];
      }

      for (i = 0; i < 4; i++)
      {
         if (vals[i])
         {
            uint32_t sign = get_bits1(&s->gb) - 1;
            rwma_store_float(&ci->coeffs[cur_coeff], vals[i] ^ sign << 31);
            num_zeros = 0;
         }
         else
         {
            ci->coeffs[cur_coeff] = 0;

            rl_mode |= (++num_zeros > s->subframe_len >> 8);
         }
         ++cur_coeff;
      }
   }

   if (cur_coeff < s->subframe_len)
   {
      int ret;

      memset(&ci->coeffs[cur_coeff], 0,
             sizeof(*ci->coeffs) * (s->subframe_len - cur_coeff));
      ret = rwma_run_level(&s->gb, vlc, level, run, 1, ci->coeffs, cur_coeff,
                           s->subframe_len, s->subframe_len, s->esc_len, 0);
      if (ret < 0)
         return ret;
   }

   return 0;
}

static int decode_scale_factors(rwma_t *s)
{
   int i;

   for (i = 0; i < s->channels_for_cur_subframe; i++)
   {
      int c = s->channel_indexes_for_cur_subframe[i];
      int *sf;
      int *sf_end;
      s->channel[c].scale_factors =
          s->channel[c].saved_scale_factors[!s->channel[c].scale_factor_idx];
      sf_end = s->channel[c].scale_factors + s->num_bands;

      if (s->channel[c].reuse_sf)
      {
         const int8_t *sf_offsets =
             s->sf_offsets[s->table_idx][s->channel[c].table_idx];
         int b;
         for (b = 0; b < s->num_bands; b++)
            s->channel[c].scale_factors[b] =
                s->channel[c]
                    .saved_scale_factors[s->channel[c].scale_factor_idx][*sf_offsets++];
      }

      if (!s->channel[c].cur_subframe || get_bits1(&s->gb))
      {

         if (!s->channel[c].reuse_sf)
         {
            int val;

            s->channel[c].scale_factor_step = get_bits(&s->gb, 2) + 1;
            val = 45 / s->channel[c].scale_factor_step;
            for (sf = s->channel[c].scale_factors; sf < sf_end; sf++)
            {
               val += rwma_vlc(&s->gb, sf_vlc);
               *sf = val;
            }
         }
         else
         {
            int i;

            for (i = 0; i < s->num_bands; i++)
            {
               int idx;
               int skip;
               int val;
               int sign;

               idx = rwma_vlc(&s->gb, sf_rl_vlc);

               if (!idx)
               {
                  uint32_t code = get_bits(&s->gb, 14);
                  val = code >> 6;
                  sign = (code & 1) - 1;
                  skip = (code & 0x3f) >> 1;
               }
               else if (idx == 1)
               {
                  break;
               }
               else
               {
                  skip = scale_rl_run[idx];
                  val = scale_rl_level[idx];
                  sign = get_bits1(&s->gb) - 1;
               }

               i += skip;
               if (i >= s->num_bands)
               {
                  (void)0;
                  return RWMA_BAD_DATA;
               }
               s->channel[c].scale_factors[i] += (val ^ sign) - sign;
            }
         }

         s->channel[c].scale_factor_idx = !s->channel[c].scale_factor_idx;
         s->channel[c].table_idx = s->table_idx;
         s->channel[c].reuse_sf = 1;
      }

      s->channel[c].max_scale_factor = s->channel[c].scale_factors[0];
      for (sf = s->channel[c].scale_factors + 1; sf < sf_end; sf++)
      {
         s->channel[c].max_scale_factor = RWMA_MAX(s->channel[c].max_scale_factor, *sf);
      }
   }
   return 0;
}

static void inverse_channel_transform(rwma_t *s)
{
   int i;

   for (i = 0; i < s->num_chgroups; i++)
   {
      if (s->chgroup[i].transform)
      {
         float data[WMAPRO_MAX_CHANNELS];
         const int num_channels = s->chgroup[i].num_channels;
         float **ch_data = s->chgroup[i].channel_data;
         float **ch_end = ch_data + num_channels;
         const int8_t *tb = s->chgroup[i].transform_band;
         int16_t *sfb;

         for (sfb = s->cur_sfb_offsets; sfb < s->cur_sfb_offsets + s->num_bands; sfb++)
         {
            int y;
            if (*tb++ == 1)
            {

               for (y = sfb[0]; y < RWMA_MIN(sfb[1], s->subframe_len); y++)
               {
                  const float *mat = s->chgroup[i].decorrelation_matrix;
                  const float *data_end = data + num_channels;
                  float *data_ptr = data;
                  float **ch;

                  for (ch = ch_data; ch < ch_end; ch++)
                     *data_ptr++ = (*ch)[y];

                  for (ch = ch_data; ch < ch_end; ch++)
                  {
                     float sum = 0;
                     data_ptr = data;
                     while (data_ptr < data_end)
                        sum += *data_ptr++ * *mat++;

                     (*ch)[y] = sum;
                  }
               }
            }
            else if (s->nb_channels == 2)
            {
               int len = RWMA_MIN(sfb[1], s->subframe_len) - sfb[0];
               rwma_multiply(ch_data[0] + sfb[0], ch_data[0] + sfb[0], 181.0 / 128,
                             len);
               rwma_multiply(ch_data[1] + sfb[0], ch_data[1] + sfb[0], 181.0 / 128,
                             len);
            }
         }
      }
   }
}

static void wmapro_window(rwma_t *s)
{
   int i;
   for (i = 0; i < s->channels_for_cur_subframe; i++)
   {
      int c = s->channel_indexes_for_cur_subframe[i];
      const float *window;
      int winlen = s->channel[c].prev_block_len;
      float *start = s->channel[c].coeffs - (winlen >> 1);

      if (s->subframe_len < winlen)
      {
         start += (winlen - s->subframe_len) >> 1;
         winlen = s->subframe_len;
      }

      window = s->windows[rwma_log2(winlen) - WMAPRO_BLOCK_MIN_BITS];

      winlen >>= 1;

      rwma_window(start, start, start + winlen, window, winlen);

      s->channel[c].prev_block_len = s->subframe_len;
   }
}

static int decode_subframe(rwma_t *s)
{
   int offset = s->samples_per_frame;
   int subframe_len = s->samples_per_frame;
   int i;
   int total_samples = s->samples_per_frame * s->nb_channels;
   int transmit_coeffs = 0;
   int cur_subwoofer_cutoff;

   s->subframe_offset = get_bits_count(&s->gb);

   for (i = 0; i < s->nb_channels; i++)
   {
      s->channel[i].grouped = 0;
      if (offset > s->channel[i].decoded_samples)
      {
         offset = s->channel[i].decoded_samples;
         subframe_len = s->channel[i].subframe_len[s->channel[i].cur_subframe];
      }
   }

   (void)0;

   s->channels_for_cur_subframe = 0;
   for (i = 0; i < s->nb_channels; i++)
   {
      const int cur_subframe = s->channel[i].cur_subframe;

      total_samples -= s->channel[i].decoded_samples;

      if (offset == s->channel[i].decoded_samples &&
          subframe_len == s->channel[i].subframe_len[cur_subframe])
      {
         total_samples -= s->channel[i].subframe_len[cur_subframe];
         s->channel[i].decoded_samples += s->channel[i].subframe_len[cur_subframe];
         s->channel_indexes_for_cur_subframe[s->channels_for_cur_subframe] = i;
         ++s->channels_for_cur_subframe;
      }
   }

   if (!total_samples)
      s->parsed_all_subframes = 1;

   (void)0;

   s->table_idx = rwma_log2(s->samples_per_frame / subframe_len);
   s->num_bands = s->num_sfb[s->table_idx];
   s->cur_sfb_offsets = s->sfb_offsets[s->table_idx];
   cur_subwoofer_cutoff = s->subwoofer_cutoffs[s->table_idx];

   offset += s->samples_per_frame >> 1;

   for (i = 0; i < s->channels_for_cur_subframe; i++)
   {
      int c = s->channel_indexes_for_cur_subframe[i];

      s->channel[c].coeffs = &s->channel[c].out[offset];
   }

   s->subframe_len = subframe_len;
   s->esc_len = rwma_log2(s->subframe_len - 1) + 1;

   if (get_bits1(&s->gb))
   {
      int num_fill_bits;
      if (!(num_fill_bits = get_bits(&s->gb, 2)))
      {
         int len = get_bits(&s->gb, 4);
         num_fill_bits = get_bitsz(&s->gb, len) + 1;
      }

      if (num_fill_bits >= 0)
      {
         if (get_bits_count(&s->gb) + num_fill_bits > s->num_saved_bits)
         {
            (void)0;
            return RWMA_BAD_DATA;
         }

         skip_bits_long(&s->gb, num_fill_bits);
      }
   }

   if (get_bits1(&s->gb))
   {
      (void)0;
      return RWMA_UNSUPPORTED;
   }

   if (decode_channel_transform(s) < 0)
      return RWMA_BAD_DATA;

   for (i = 0; i < s->channels_for_cur_subframe; i++)
   {
      int c = s->channel_indexes_for_cur_subframe[i];
      if ((s->channel[c].transmit_coefs = get_bits1(&s->gb)))
         transmit_coeffs = 1;
   }

   assert(s->subframe_len <= WMAPRO_BLOCK_MAX_SIZE);
   if (transmit_coeffs)
   {
      int step;
      int quant_step = 90 * s->bits_per_sample >> 4;

      if ((s->transmit_num_vec_coeffs = get_bits1(&s->gb)))
      {
         int num_bits = rwma_log2((s->subframe_len + 3) / 4) + 1;
         for (i = 0; i < s->channels_for_cur_subframe; i++)
         {
            int c = s->channel_indexes_for_cur_subframe[i];
            int num_vec_coeffs = get_bits(&s->gb, num_bits) << 2;
            if (num_vec_coeffs > s->subframe_len)
            {
               (void)0;
               return RWMA_BAD_DATA;
            }
            if ((unsigned)(num_vec_coeffs + offset) > s->max_frame * 3 / 2)
               return RWMA_BAD_DATA;
            s->channel[c].num_vec_coeffs = num_vec_coeffs;
         }
      }
      else
      {
         for (i = 0; i < s->channels_for_cur_subframe; i++)
         {
            int c = s->channel_indexes_for_cur_subframe[i];
            s->channel[c].num_vec_coeffs = s->subframe_len;
         }
      }

      step = get_sbits(&s->gb, 6);
      quant_step += step;
      if (step == -32 || step == 31)
      {
         const int sign = (step == 31) - 1;
         int quant = 0;
         while (get_bits_count(&s->gb) + 5 < s->num_saved_bits &&
                (step = get_bits(&s->gb, 5)) == 31)
         {
            quant += 31;
         }
         quant_step += ((quant + step) ^ sign) - sign;
      }
      if (quant_step < 0)
      {
         (void)0;
      }

      if (s->channels_for_cur_subframe == 1)
      {
         s->channel[s->channel_indexes_for_cur_subframe[0]].quant_step = quant_step;
      }
      else
      {
         int modifier_len = get_bits(&s->gb, 3);
         for (i = 0; i < s->channels_for_cur_subframe; i++)
         {
            int c = s->channel_indexes_for_cur_subframe[i];
            s->channel[c].quant_step = quant_step;
            if (get_bits1(&s->gb))
            {
               if (modifier_len)
               {
                  s->channel[c].quant_step += get_bits(&s->gb, modifier_len) + 1;
               }
               else
                  ++s->channel[c].quant_step;
            }
         }
      }

      if (decode_scale_factors(s) < 0)
         return RWMA_BAD_DATA;
   }

   (void)0;

   for (i = 0; i < s->channels_for_cur_subframe; i++)
   {
      int c = s->channel_indexes_for_cur_subframe[i];
      if (s->channel[c].transmit_coefs && get_bits_count(&s->gb) < s->num_saved_bits)
      {
         {
            if (decode_coeffs(s, c) < 0)
               return RWMA_BAD_DATA;
         }
      }
      else
         memset(s->channel[c].coeffs, 0, sizeof(*s->channel[c].coeffs) * subframe_len);
   }

   (void)0;

   if (transmit_coeffs)
   {

      inverse_channel_transform(s);
      for (i = 0; i < s->channels_for_cur_subframe; i++)
      {
         int c = s->channel_indexes_for_cur_subframe[i];
         const int *sf = s->channel[c].scale_factors;
         int b;

         if (c == s->lfe_channel)
            memset(&s->tmp[cur_subwoofer_cutoff], 0,
                   sizeof(*s->tmp) * (subframe_len - cur_subwoofer_cutoff));

         for (b = 0; b < s->num_bands; b++)
         {
            const int end = RWMA_MIN(s->cur_sfb_offsets[b + 1], s->subframe_len);
            const int exp =
                s->channel[c].quant_step - (s->channel[c].max_scale_factor - *sf++) *
                                               s->channel[c].scale_factor_step;
            const float quant = rwma_exp10(exp / 20.0);
            int start = s->cur_sfb_offsets[b];
            rwma_multiply(s->tmp + start, s->channel[c].coeffs + start, quant,
                          end - start);
         }

         rwma_imdct(s, s->channel[c].coeffs, s->tmp, subframe_len);
      }
   }

   wmapro_window(s);

   for (i = 0; i < s->channels_for_cur_subframe; i++)
   {
      int c = s->channel_indexes_for_cur_subframe[i];
      if (s->channel[c].cur_subframe >= s->channel[c].num_subframes)
      {
         (void)0;
         return RWMA_BAD_DATA;
      }
      ++s->channel[c].cur_subframe;
   }

   return 0;
}

static int decode_frame(rwma_t *s, rwma_frame *frame, int *got_frame_ptr)
{
   rwma_bits *gb = &s->gb;
   int more_frames = 0;
   int len = 0;
   int i;

   if (s->len_prefix)
      len = get_bits(gb, s->log2_frame_size);

   (void)0;

   if (decode_tilehdr(s))
   {
      s->packet_loss = 1;
      return 0;
   }

   if (s->nb_channels > 1 && get_bits1(gb))
   {
      if (get_bits1(gb))
      {
         for (i = 0; i < s->nb_channels * s->nb_channels; i++)
            skip_bits(gb, 4);
      }
   }

   if (s->dynamic_range_compression)
   {
      s->drc_gain = get_bits(gb, 8);
      (void)0;
   }

   if (get_bits1(gb))
   {
      if (get_bits1(gb))
         s->trim_start = get_bits(gb, rwma_log2(s->samples_per_frame * 2));

      if (get_bits1(gb))
         s->trim_end = get_bits(gb, rwma_log2(s->samples_per_frame * 2));
   }
   else
   {
      s->trim_start = s->trim_end = 0;
   }

   (void)0;

   s->parsed_all_subframes = 0;
   for (i = 0; i < s->nb_channels; i++)
   {
      s->channel[i].decoded_samples = 0;
      s->channel[i].cur_subframe = 0;
      s->channel[i].reuse_sf = 0;
   }

   while (!s->parsed_all_subframes)
   {
      if (decode_subframe(s) < 0)
      {
         s->packet_loss = 1;
         return 0;
      }
   }

   for (i = 0; i < s->nb_channels; i++)
      frame->planes[i] = s->channel[i].out;
   frame->samples = s->samples_per_frame;
   frame->channels = s->nb_channels;
   frame->sample_rate = s->sample_rate;
   s->pending_shift = 1;

   if (s->skip_frame)
   {
      s->skip_frame = 0;
      *got_frame_ptr = 0;
      frame->samples = 0;
   }
   else
   {
      *got_frame_ptr = 1;
   }

   if (s->len_prefix)
   {
      if (len != (get_bits_count(gb) - s->frame_offset) + 2)
      {

         (void)0;
         s->packet_loss = 1;
         return 0;
      }

      skip_bits_long(gb, len - (get_bits_count(gb) - s->frame_offset) - 1);
   }
   else
   {
      while (get_bits_count(gb) < s->num_saved_bits && get_bits1(gb) == 0)
      {
      }
   }

   more_frames = get_bits1(gb);

   ++s->frame_num;
   return more_frames;
}
static int remaining_bits(rwma_t *s, rwma_bits *b) { return s->buf_bit_size - b->pos; }

static void save_bits(rwma_t *s, rwma_bits *b, int len, int append)
{
   int i, dst, bits;
   if (len <= 0 || len > get_bits_left(b))
   {
      s->packet_loss = 1;
      return;
   }
   if (!append)
   {
      s->frame_offset = b->pos & 7;
      s->num_saved_bits = s->frame_offset;
      bits = s->frame_offset + len;
      if (bits > MAX_FRAMESIZE * 8)
      {
         s->packet_loss = 1;
         return;
      }
      memcpy(s->reservoir, b->data + b->pos / 8, (bits + 7) / 8);
      b->pos += len;
      s->num_saved_bits = bits;
   }
   else
   {
      if (s->num_saved_bits > MAX_FRAMESIZE * 8 - len)
      {
         s->packet_loss = 1;
         return;
      }
      dst = s->num_saved_bits;
      /* Only a split compressed frame needs this concatenation. */
      while (len)
      {
         unsigned take, value, mask;
         if (!(dst & 7) && !(b->pos & 7) && len >= 8)
         {
            int bytes = len / 8;
            memcpy(s->reservoir + dst / 8, b->data + b->pos / 8, bytes);
            dst += bytes * 8;
            b->pos += bytes * 8;
            len -= bytes * 8;
            continue;
         }
         take = RWMA_MIN(len, 8 - (dst & 7));
         value = get_bits(b, take);
         i = 8 - (dst & 7) - take;
         mask = ((1u << take) - 1) << i;
         s->reservoir[dst / 8] =
             (uint8_t)((s->reservoir[dst / 8] & ~mask) | (value << i));
         dst += take;
         len -= take;
      }
      s->num_saved_bits = dst;
   }
   init_get_bits(&s->gb, s->reservoir, s->num_saved_bits);
   skip_bits(&s->gb, s->frame_offset);
}

static void rwma_view_bits(rwma_t *s, rwma_bits *b, int len)
{
   s->frame_offset = b->pos;
   s->num_saved_bits = b->pos + len;
   s->gb = *b;
   s->gb.size = s->num_saved_bits;
   skip_bits(b, len);
}

static int decode_packet(rwma_t *s, rwma_frame *frame, int *got_frame_ptr,
                         const rwma_packet *avpkt)
{
   rwma_bits *gb = &s->pgb;
   const uint8_t *buf = avpkt->data;
   int buf_size = avpkt->size;
   int num_bits_prev_frame;
   int packet_sequence_number;
   int ret, ch;

   *got_frame_ptr = 0;

   if (!buf_size)
   {
      int i;

      s->packet_done = 0;
      if (s->eof_done)
         return 0;

      for (i = 0; i < s->nb_channels; i++)
      {
         memset(s->channel[i].out + s->samples_per_frame / 2, 0,
                s->samples_per_frame / 2 * sizeof(float));
         frame->planes[i] = s->channel[i].out;
      }
      s->eof_done = 1;
      s->packet_done = 1;
      *got_frame_ptr = 1;
      return 0;
   }
   else if (s->packet_done || s->packet_loss)
   {
      s->packet_done = 0;

      if (s->codec == RWMA_PRO && buf_size < s->block_align)
      {
         (void)0;
         s->packet_loss = 1;
         return RWMA_BAD_DATA;
      }

      if (s->codec == RWMA_PRO)
      {
         s->next_packet_start = buf_size - s->block_align;
         buf_size = s->block_align;
      }
      else
      {
         s->next_packet_start = buf_size - RWMA_MIN(buf_size, s->block_align);
         buf_size = RWMA_MIN(buf_size, s->block_align);
      }
      s->buf_bit_size = buf_size << 3;

      ret = init_get_bits8(gb, buf, buf_size);
      if (ret < 0)
         return ret;
      if (s->codec != 99)
      {
         packet_sequence_number = get_bits(gb, 4);
         skip_bits(gb, 2);
      }
      else
      {
         (void)get_bits(gb, 6);
         (void)0;
         packet_sequence_number = 0;
      }

      num_bits_prev_frame = get_bits(gb, s->log2_frame_size);
      if (s->codec != RWMA_PRO)
      {
         skip_bits(gb, 3);
         s->skip_packets = get_bits(gb, 8);
         (void)0;
      }

      (void)0;

      if (s->codec == RWMA_PRO && !s->packet_loss &&
          ((s->packet_sequence_number + 1) & 0xF) != packet_sequence_number)
      {
         s->packet_loss = 1;
         (void)0;
      }
      s->packet_sequence_number = packet_sequence_number;

      if (num_bits_prev_frame > 0)
      {
         int remaining_packet_bits = s->buf_bit_size - get_bits_count(gb);
         if (num_bits_prev_frame >= remaining_packet_bits)
         {
            num_bits_prev_frame = remaining_packet_bits;
            s->packet_done = 1;
         }

         save_bits(s, gb, num_bits_prev_frame, 1);
         (void)0;

         if (!s->packet_loss)
            decode_frame(s, frame, got_frame_ptr);
      }
      else if (s->num_saved_bits - s->frame_offset)
      {
         (void)0;
      }

      if (s->packet_loss)
      {

         s->num_saved_bits = 0;
         s->packet_loss = 0;
      }
   }
   else
   {
      int frame_size;

      if (avpkt->size < s->next_packet_start)
      {
         s->packet_loss = 1;
         return RWMA_BAD_DATA;
      }

      s->buf_bit_size = (avpkt->size - s->next_packet_start) << 3;
      ret = init_get_bits8(gb, avpkt->data, avpkt->size - s->next_packet_start);
      if (ret < 0)
         return ret;
      skip_bits(gb, s->packet_offset);
      if (s->len_prefix && remaining_bits(s, gb) > s->log2_frame_size &&
          (frame_size = show_bits(gb, s->log2_frame_size)) &&
          frame_size <= remaining_bits(s, gb))
      {
         rwma_view_bits(s, gb, frame_size);
         if (!s->packet_loss)
            s->packet_done = !decode_frame(s, frame, got_frame_ptr);
      }
      else if (!s->len_prefix && s->num_saved_bits > get_bits_count(&s->gb))
      {

         s->packet_done = !decode_frame(s, frame, got_frame_ptr);
      }
      else
      {
         s->packet_done = 1;
      }
   }

   if (remaining_bits(s, gb) < 0)
   {
      (void)0;
      s->packet_loss = 1;
   }

   if (s->packet_done && !s->packet_loss && remaining_bits(s, gb) > 0)
   {

      save_bits(s, gb, remaining_bits(s, gb), 0);
   }

   s->packet_offset = get_bits_count(gb) & 7;
   if (s->packet_loss)
      return RWMA_BAD_DATA;

   if (s->trim_start && s->codec == RWMA_PRO)
   {
      if ((unsigned)s->trim_start < frame->samples)
      {
         for (ch = 0; ch < (int)frame->channels; ch++)
            frame->planes[ch] += s->trim_start;

         frame->samples -= s->trim_start;
      }
      else
      {
         *got_frame_ptr = 0;
      }

      s->trim_start = 0;
   }

   if (s->trim_end && s->codec == RWMA_PRO)
   {
      if ((unsigned)s->trim_end < frame->samples)
      {
         frame->samples -= s->trim_end;
      }
      else
      {
         *got_frame_ptr = 0;
      }

      s->trim_end = 0;
   }

   return get_bits_count(gb) >> 3;
}
static void rwma_shift(rwma_t *s)
{
   int c;
   if (!s->pending_shift)
      return;
   for (c = 0; c < s->nb_channels; c++)
      memmove(s->channel[c].out, s->channel[c].out + s->samples_per_frame,
              (s->samples_per_frame / 2) * sizeof(float));
   s->pending_shift = 0;
}

void rwma_free(rwma_t *s)
{
   if (s)
   {
      rwma_standard_free(s->standard);
      free(s->storage);
      free(s);
   }
}

void rwma_reset(rwma_t *s)
{
   int c;
   if (!s)
      return;
   if (s->standard)
   {
      rwma_standard_reset(s->standard);
      return;
   }
   for (c = 0; c < s->nb_channels; c++)
   {
      float *out = s->channel[c].out;
      memset(&s->channel[c], 0, sizeof(s->channel[c]));
      s->channel[c].out = out;
      memset(out, 0, s->max_frame * 3 / 2 * sizeof(float));
      s->channel[c].prev_block_len = s->samples_per_frame;
   }
   s->pending_shift = s->failed = s->frame_num = 0;
   s->trim_start = s->trim_end = 0;
   s->skip_frame = s->codec != RWMA_XMA_RAW;
   s->packet_loss = 0;
   s->packet_done = 1;
   s->eof_done = s->num_saved_bits = s->frame_offset = s->packet_offset = 0;
}

int rwma_create(rwma_t **out, const rwma_config *cfg)
{
   rwma_t *s;
   float *p;
   unsigned i, c, possible, channel_mask = 0, bits;
   unsigned decode_flags = 0x10d6;
   size_t floats;
   if (!out)
      return RWMA_BAD_DATA;
   *out = NULL;
   if (!cfg || cfg->channels < 1 || cfg->channels > 8 || cfg->sample_rate < 8000 ||
       cfg->sample_rate > 192000)
      return RWMA_BAD_DATA;
   if (cfg->codec != RWMA_XMA_RAW && cfg->codec != RWMA_PRO && cfg->codec != RWMA_V1 &&
       cfg->codec != RWMA_V2)
      return RWMA_UNSUPPORTED;
   if (cfg->codec == RWMA_V1 || cfg->codec == RWMA_V2)
   {
      int ret;
      s = (rwma_t *)calloc(1, sizeof(*s));
      if (!s)
         return RWMA_NO_MEMORY;
      s->codec = cfg->codec;
      ret = rwma_standard_create(&s->standard, cfg);
      if (ret)
      {
         rwma_free(s);
         return ret;
      }
      *out = s;
      return RWMA_OK;
   }
   if (cfg->codec == RWMA_XMA_RAW && cfg->channels > 2)
      return RWMA_UNSUPPORTED;
   if (cfg->codec == RWMA_PRO &&
       (!cfg->extra || cfg->extra_size < 18 || !cfg->block_align ||
        cfg->block_align > MAX_FRAMESIZE))
      return RWMA_BAD_DATA;
   s = (rwma_t *)calloc(1, sizeof(*s));
   if (!s)
      return RWMA_NO_MEMORY;
   s->codec = cfg->codec;
   s->sample_rate = cfg->sample_rate;
   s->nb_channels = cfg->channels;
   s->bits_per_sample = 16;
   s->samples_per_frame = s->max_frame = 512;
   s->len_prefix = 1;
   s->log2_frame_size = 15;
   s->max_num_subframes = 4;
   s->max_subframe_len_bit = 1;
   s->subframe_len_bits = 2;
   s->min_samples_per_subframe = 128;
   s->dynamic_range_compression = decode_flags & 0x80;
   s->lfe_channel = -1;
   possible = 3;
   if (cfg->codec == RWMA_PRO)
   {
      unsigned bps = cfg->extra[0] | (cfg->extra[1] << 8);
      decode_flags = cfg->extra[14] | (cfg->extra[15] << 8);
      channel_mask = cfg->extra[2] | ((uint32_t)cfg->extra[3] << 8) |
                     ((uint32_t)cfg->extra[4] << 16) | ((uint32_t)cfg->extra[5] << 24);
      if (bps < 1 || bps > 32)
      {
         rwma_free(s);
         return RWMA_UNSUPPORTED;
      }
      if (channel_mask)
      {
         unsigned mask = channel_mask, channels = 0;
         while (mask)
         {
            channels += mask & 1;
            mask >>= 1;
         }
         if (channels != cfg->channels)
         {
            rwma_free(s);
            return RWMA_BAD_DATA;
         }
      }
      s->bits_per_sample = bps;
      bits = cfg->sample_rate <= 16000   ? 9
             : cfg->sample_rate <= 22050 ? 10
             : cfg->sample_rate <= 48000 ? 11
             : cfg->sample_rate <= 96000 ? 12
                                         : 13;
      switch (decode_flags & 6)
      {
      case 2:
         bits++;
         break;
      case 4:
         bits--;
         break;
      case 6:
         bits -= 2;
         break;
      }
      if (bits > 13)
      {
         rwma_free(s);
         return RWMA_UNSUPPORTED;
      }
      s->samples_per_frame = s->max_frame = 1u << bits;
      s->block_align = cfg->block_align;
      s->log2_frame_size = rwma_log2(cfg->block_align) + 4;
      s->len_prefix = decode_flags & 0x40;
      bits = (decode_flags & 0x38) >> 3;
      if (bits > 5)
      {
         rwma_free(s);
         return RWMA_BAD_DATA;
      }
      possible = bits + 1;
      s->max_num_subframes = 1u << bits;
      s->max_subframe_len_bit = s->max_num_subframes == 16 || s->max_num_subframes == 4;
      s->subframe_len_bits = rwma_log2(bits) + 1;
      s->min_samples_per_subframe = s->samples_per_frame / s->max_num_subframes;
      if (s->min_samples_per_subframe < 64)
      {
         rwma_free(s);
         return RWMA_BAD_DATA;
      }
      s->dynamic_range_compression = decode_flags & 0x80;
      if (channel_mask & 8)
      {
         unsigned mask;
         for (mask = 1; mask < 16; mask <<= 1)
            if (channel_mask & mask)
               s->lfe_channel++;
      }
   }
   /* Per-channel overlap/output, transform scratch and immutable plans. */
   floats = s->max_frame * cfg->channels * 3 / 2 + s->max_frame * 9;
   if (cfg->codec == RWMA_PRO)
      floats += MAX_FRAMESIZE / sizeof(float);
   s->storage = (float *)calloc(floats, sizeof(float));
   if (!s->storage)
   {
      rwma_free(s);
      return RWMA_NO_MEMORY;
   }
   p = s->storage;
   for (c = 0; c < cfg->channels; c++)
   {
      s->channel[c].out = p;
      p += s->max_frame * 3 / 2;
   }
   s->tmp = p;
   p += s->max_frame;
   rwma_mdct_init(&s->mdct, p, s->max_frame);
   p += 8 * s->max_frame;
   for (i = 0; i < 8; i++)
      s->windows[i] = s->mdct.windows[i];
   assert((size_t)(p - s->storage) <= floats);
   if (cfg->codec == RWMA_PRO)
   {
      s->reservoir = (uint8_t *)p;
      p += MAX_FRAMESIZE / sizeof(float);
   }
   assert((size_t)(p - s->storage) <= floats);
   for (i = 0; i < 33; i++)
      s->sin64[i] = (float)sin(i * RWMA_PI / 64.0);
   for (i = 0; i < possible; i++)
   {
      unsigned x, band = 1;
      unsigned sub = s->samples_per_frame >> i;
      unsigned rate = cfg->sample_rate > 44100   ? 48000
                      : cfg->sample_rate > 32000 ? 44100
                      : cfg->sample_rate > 24000 ? 32000
                                                 : 24000;
      if (cfg->codec == RWMA_PRO)
         rate = cfg->sample_rate;
      s->sfb_offsets[i][0] = 0;
      for (x = 0; x < 28 && s->sfb_offsets[i][band - 1] < (int)sub; x++)
      {
         int offset = ((sub * 2 * critical_freq[x]) / rate + 2) & ~3;
         if (offset > s->sfb_offsets[i][band - 1])
            s->sfb_offsets[i][band++] = offset;
         if (offset >= (int)sub)
            break;
      }
      s->sfb_offsets[i][band - 1] = sub;
      s->num_sfb[i] = band - 1;
      s->subwoofer_cutoffs[i] =
          RWMA_MIN(sub, RWMA_MAX(4, (440 * sub + 3 * (cfg->sample_rate / 2) - 1) /
                                        cfg->sample_rate));
   }
   for (i = 0; i < possible; i++)
      for (c = 0; c < (unsigned)s->num_sfb[i]; c++)
      {
         unsigned x;
         int offset = ((s->sfb_offsets[i][c] + s->sfb_offsets[i][c + 1] - 1) << i) >> 1;
         for (x = 0; x < possible; x++)
         {
            unsigned v = 0;
            while ((s->sfb_offsets[x][v + 1] << x) < offset && v < 27)
               v++;
            s->sf_offsets[i][x][c] = v;
         }
      }
   (void)channel_mask;
   rwma_reset(s);
   *out = s;
   return RWMA_OK;
}

int rwma_decode_packet(rwma_t *s, const uint8_t *data, size_t bytes, size_t *consumed,
                       rwma_frame *frame)
{
   rwma_packet packet;
   int got = 0, ret;
   if (!frame || !consumed)
      return RWMA_BAD_DATA;
   memset(frame, 0, sizeof(*frame));
   *consumed = 0;
   if (s && s->standard)
      return rwma_standard_decode(s->standard, data, bytes, consumed, frame);
   if (!s || s->codec != RWMA_PRO || s->failed || (!data && bytes) ||
       bytes > INT_MAX / 8)
      return RWMA_BAD_DATA;
   if (!bytes && !s->frame_num)
      return RWMA_OK;
   rwma_shift(s);
   frame->samples = s->samples_per_frame;
   frame->channels = s->nb_channels;
   frame->sample_rate = s->sample_rate;
   packet.data = data;
   packet.size = bytes;
   s->gb.error = s->pgb.error = 0;
   ret = decode_packet(s, frame, &got, &packet);
   if (ret < 0 || s->gb.error || s->pgb.error || (got && !rwma_frame_valid(frame)))
   {
      s->failed = 1;
      memset(frame, 0, sizeof(*frame));
      return RWMA_BAD_DATA;
   }
   if (!got)
      memset(frame, 0, sizeof(*frame));
   *consumed = ret;
   return RWMA_OK;
}

int rwma_decode_xma(rwma_t *s, const uint8_t *data, size_t bytes, size_t bit_offset,
                    size_t bit_count, rwma_frame *frame)
{
   int got = 0;
   if (!frame)
      return RWMA_BAD_DATA;
   memset(frame, 0, sizeof(*frame));
   if (!s || s->codec != RWMA_XMA_RAW || s->failed || !data || bytes > INT_MAX / 8 ||
       bit_offset > bytes * 8 || bit_count < 18 || bit_count > bytes * 8 - bit_offset)
      return RWMA_BAD_DATA;
   rwma_shift(s);
   s->gb.data = data;
   s->gb.pos = bit_offset;
   s->gb.size = bit_offset + bit_count;
   s->gb.error = 0;
   s->frame_offset = bit_offset;
   s->num_saved_bits = s->gb.size;
   s->packet_loss = 0;
   if (show_bits(&s->gb, 15) != bit_count)
      return RWMA_BAD_DATA;
   decode_frame(s, frame, &got);
   if (s->gb.error || s->packet_loss || !got || !rwma_frame_valid(frame))
   {
      s->failed = 1;
      memset(frame, 0, sizeof(*frame));
      return RWMA_BAD_DATA;
   }
   frame->trim_start = s->trim_start;
   frame->trim_end = s->trim_end;
   return RWMA_OK;
}
