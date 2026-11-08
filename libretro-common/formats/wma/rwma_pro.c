/* MIT: Copyright (c) 2026 Cole Springer and The RetroArch team.
 * C89 adaptation of WaxFlow codec/wmapro (2f448c9435fd).
 * See COPYING.MIT, COPYING.MIT.WaxFlow and PROVENANCE.md. */
#include "rwma_internal.h"
#include "rwma_mdct.h"
#include "rwma_pro_data.h"
#include "rwma_pro_gain.h"
#include "rwma_pro_prefix.h"

typedef struct rwma_lane
{
   float *pcm, *spectrum;
   int lengths[32], count, cursor, placed, decoded, previous;
   int coded, active, limit, step, resolution, maximum;
   int envelope[29], saved[29], reuse, saved_size;
} rwma_lane;
typedef struct rwma_group
{
   int members[8], count, on;
   float matrix[64];
   unsigned char bands[29];
} rwma_group;
struct rwma_t
{
   void *standard;
   float *allocation, *spectrum, *undo;
   rwma_mdct transform;
   rwma_lane lane[8];
   rwma_group group[8];
   rwma_config config;
   uint8_t *carry;
   const uint8_t *packet;
   size_t carry_bits, carry_capacity, packet_at, allocation_floats;
   int edges[8][29], band_count[8], remap[8][8][29];
   float sine[33];
   unsigned frame_length, depth, length_bits, bps, capacity_channels;
   unsigned failed, pending, drop_first, flags, in_packet, more, drained;
   int participants[8], n_participants, n_groups, explicit_limit;
};

static int pro_length(rwma_t *d, rwma_reader *r, int position)
{
   unsigned shift = 0, max = 1u << d->depth;
   int length;
   if (position == (int)(d->frame_length - d->frame_length / max))
      return d->frame_length / max;
   if (max == 4 || max == 16)
   {
      if (rwma_read(r, 1))
         shift = 1 + rwma_read(r, rwma_log2(d->depth));
   }
   else
      shift = rwma_read(r, rwma_log2(d->depth) + 1);
   if (r->error || shift > d->depth)
      return -1;
   length = d->frame_length >> shift;
   return position + length > (int)d->frame_length ? -1 : length;
}
static int pro_tiles(rwma_t *d, rwma_reader *r)
{
   int i, c, steps, front = 0, waiting = d->config.channels, uniform;
   unsigned char take[8];
   for (c = 0; c < (int)d->config.channels; c++)
   {
      rwma_lane *l = &d->lane[c];
      l->count = l->cursor = l->placed = l->decoded = l->reuse = 0;
   }
   uniform = d->depth == 0 || rwma_read(r, 1);
   for (steps = 0; front < (int)d->frame_length; steps++)
   {
      int length,
          last = front == (int)(d->frame_length - (d->frame_length >> d->depth));
      if (steps >= 32 * (int)d->config.channels)
         return RWMA_BAD_DATA;
      for (c = 0; c < (int)d->config.channels; c++)
         take[c] = d->lane[c].placed == front &&
                   (uniform || waiting == 1 || last || rwma_read(r, 1));
      length = pro_length(d, r, front);
      if (length < 0 || r->error)
         return RWMA_BAD_DATA;
      front += length;
      for (c = 0; c < (int)d->config.channels; c++)
      {
         rwma_lane *l = &d->lane[c];
         if (take[c])
         {
            if (l->count >= 32 || l->placed + length > (int)d->frame_length)
               return RWMA_BAD_DATA;
            l->lengths[l->count++] = length;
            l->placed += length;
         }
         else if (l->placed <= front)
         {
            if (l->placed < front)
            {
               front = l->placed;
               waiting = 0;
            }
            waiting++;
         }
      }
   }
   for (i = 0; i < (int)d->config.channels; i++)
      if (d->lane[i].placed != (int)d->frame_length)
         return RWMA_BAD_DATA;
   return RWMA_OK;
}
static unsigned pro_large(rwma_reader *r)
{
   unsigned n = 8;
   if (rwma_read(r, 1))
   {
      n = 16;
      if (rwma_read(r, 1))
      {
         n = 24;
         if (rwma_read(r, 1))
            n = 31;
      }
   }
   return rwma_read(r, n);
}
static int pro_envelope(rwma_t *d, rwma_reader *r, rwma_lane *l, int k)
{
   int b, nb = d->band_count[k];
   memset(l->envelope, 0, sizeof(l->envelope));
   if (l->reuse)
      for (b = 0; b < nb; b++)
         l->envelope[b] = l->saved[d->remap[k][l->saved_size][b]];
   if (l->cursor == 0 || rwma_read(r, 1))
   {
      if (!l->reuse)
      {
         int value;
         l->resolution = rwma_read(r, 2) + 1;
         value = 45 / l->resolution;
         for (b = 0; b < nb; b++)
         {
            int symbol = rwma_symbol_prefix(r, scaleDelta, scaleDelta_prefix);
            if (symbol < 0)
               return RWMA_BAD_DATA;
            value += symbol - 60;
            l->envelope[b] = value;
         }
      }
      else
         for (b = 0; b < nb; b++)
         {
            int skip, value, positive,
                symbol = rwma_symbol_prefix(r, scaleRunLevel, scaleRunLevel_prefix);
            if (symbol < 0)
               return RWMA_BAD_DATA;
            if (symbol == 1)
               break;
            if (symbol == 0)
            {
               unsigned packed = rwma_read(r, 14);
               skip = (packed >> 1) & 31;
               value = packed >> 6;
               positive = packed & 1;
            }
            else
            {
               skip = scaleRunLevelRuns[symbol];
               value = scaleRunLevelLevels[symbol];
               positive = rwma_read(r, 1);
            }
            b += skip;
            if (b >= nb)
               return RWMA_BAD_DATA;
            l->envelope[b] += positive ? value : -value;
         }
      memcpy(l->saved, l->envelope, nb * sizeof(int));
      l->saved_size = k;
      l->reuse = 1;
   }
   l->maximum = l->envelope[0];
   for (b = 1; b < nb; b++)
      if (l->envelope[b] > l->maximum)
         l->maximum = l->envelope[b];
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}
static int pro_quant(rwma_t *d, rwma_reader *r, int k, int length)
{
   int p, delta, step, more, width;
   d->explicit_limit = rwma_read(r, 1);
   for (p = 0; p < d->n_participants; p++)
   {
      rwma_lane *l = &d->lane[d->participants[p]];
      l->limit = d->explicit_limit
                     ? (int)(rwma_read(r, rwma_log2((length + 3) / 4) + 1) * 4)
                     : length;
      if (l->limit > length)
         return RWMA_BAD_DATA;
   }
   delta = rwma_read(r, 6);
   if (delta >= 32)
      delta -= 64;
   step = (90 * d->bps) / 16 + delta;
   if (delta == 31 || delta == -32)
   {
      int extension = 0;
      do
      {
         more = rwma_read(r, 5);
         extension += more;
         if (r->error || extension > (1 << 20))
            return RWMA_BAD_DATA;
      } while (more == 31);
      step += delta == 31 ? extension : -extension;
   }
   width = d->n_participants == 1 ? 0 : rwma_read(r, 3);
   for (p = 0; p < d->n_participants; p++)
   {
      rwma_lane *l = &d->lane[d->participants[p]];
      l->step = step;
      if (d->n_participants > 1 && rwma_read(r, 1))
         l->step += 1 + (width ? rwma_read(r, width) : 0);
   }
   for (p = 0; p < d->n_participants; p++)
      if (pro_envelope(d, r, &d->lane[d->participants[p]], k))
         return RWMA_BAD_DATA;
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}
static RWMA_INLINE int pro_vectors(rwma_reader *r, unsigned magnitude[4])
{
   int v, h, j;
   v = rwma_symbol_prefix(r, vec4, vec4_prefix);
   if (v < 0)
      return RWMA_BAD_DATA;
   if (v)
   {
      unsigned packed = v - 1;
      for (j = 0; j < 4; j++)
         magnitude[j] = (packed >> (12 - 4 * j)) & 15;
      return RWMA_OK;
   }
   for (h = 0; h < 2; h++)
   {
      v = rwma_symbol_prefix(r, vec2, vec2_prefix);
      if (v < 0)
         return RWMA_BAD_DATA;
      if (v)
      {
         magnitude[2 * h] = (v - 1) >> 4;
         magnitude[2 * h + 1] = (v - 1) & 15;
      }
      else
         for (j = 0; j < 2; j++)
         {
            v = rwma_symbol_prefix(r, vec1, vec1_prefix);
            if (v < 0)
               return RWMA_BAD_DATA;
            magnitude[2 * h + j] = (unsigned)v + (v == 100 ? pro_large(r) : 0);
         }
   }
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}
static int pro_coefficients(rwma_t *d, rwma_reader *r, rwma_lane *l, int length)
{
   int book = rwma_read(r, 1), cur = 0, zeros = 0, sparse = 0, j;
   const rwma_book *codes = book ? coef1 : coef0;
   const uint16_t *runs = book ? run1 : run0;
   const float *levels = book ? level1 : level0;
   while ((d->explicit_limit || !sparse) && cur + 3 < l->limit)
   {
      unsigned magnitude[4], signs, count;
      if (pro_vectors(r, magnitude))
         return RWMA_BAD_DATA;
      count = !!magnitude[0] + !!magnitude[1] + !!magnitude[2] + !!magnitude[3];
      signs = rwma_read(r, count);
      for (j = 0; j < 4; j++, cur++)
      {
         float value = (float)magnitude[j];
         if (magnitude[j])
         {
            value = (signs & (1u << --count)) ? value : -value;
            zeros = 0;
         }
         else if (++zeros > (length >> 8))
            sparse = 1;
         l->spectrum[cur] = value;
      }
   }
   memset(l->spectrum + cur, 0, (length - cur) * sizeof(float));
   for (; cur < length; cur++)
   {
      int symbol = rwma_symbol_prefix(r, codes, book ? coef1_prefix : coef0_prefix);
      float value;
      if (symbol < 0)
         return RWMA_BAD_DATA;
      if (symbol == 1)
         break;
      if (symbol > 1)
      {
         cur += runs[symbol];
         value = levels[symbol];
      }
      else
      {
         value = (float)pro_large(r);
         if (rwma_read(r, 1))
         {
            if (!rwma_read(r, 1))
               cur += rwma_read(r, 2) + 1;
            else if (rwma_read(r, 1))
               return RWMA_BAD_DATA;
            else
               cur += rwma_read(r, rwma_log2(length - 1) + 1) + 4;
         }
      }
      if (cur >= length || r->error)
         return RWMA_BAD_DATA;
      l->spectrum[cur] = rwma_read(r, 1) ? value : -value;
   }
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}
static int pro_groups(rwma_t *d, rwma_reader *r, int k)
{
   int remaining[8], rest[8], n = d->n_participants, p, i, x, y, g;
   memcpy(remaining, d->participants, n * sizeof(int));
   d->n_groups = 0;
   if (d->config.channels == 1)
      return RWMA_OK;
   if (rwma_read(r, 1))
      return RWMA_UNSUPPORTED;
   for (g = 0; n && g < d->n_participants; g++)
   {
      rwma_group *group = &d->group[g];
      int count = 0;
      memset(group, 0, sizeof(*group));
      for (p = 0; p < n; p++)
      {
         if (n <= 2 || rwma_read(r, 1))
            group->members[group->count++] = remaining[p];
         else
            rest[count++] = remaining[p];
      }
      memcpy(remaining, rest, count * sizeof(int));
      n = count;
      if (group->count == 2)
      {
         if (!rwma_read(r, 1))
         {
            float a = d->config.channels == 2 ? 1.0f : 181.0f / 256;
            group->on = 1;
            group->matrix[0] = a;
            group->matrix[1] = -a;
            group->matrix[2] = a;
            group->matrix[3] = a;
         }
         else if (rwma_read(r, 1))
            return RWMA_UNSUPPORTED;
      }
      else if (group->count > 2 && rwma_read(r, 1))
      {
         int width = group->count;
         group->on = 1;
         if (rwma_read(r, 1))
         {
            int angles[28], index = 0;
            for (i = 0; i < width * (width - 1) / 2; i++)
               angles[i] = rwma_read(r, 6);
            for (i = 0; i < width; i++)
               group->matrix[i * width + i] = rwma_read(r, 1) ? 1.0f : -1.0f;
            for (i = 1; i < width; i++)
            {
               for (x = 0; x < i; x++)
               {
                  int angle = angles[index + x];
                  float sine = angle < 32 ? d->sine[angle] : d->sine[64 - angle];
                  float cosine =
                      angle < 32 ? d->sine[32 - angle] : -d->sine[angle - 32];
                  for (y = 0; y <= i; y++)
                  {
                     float a = group->matrix[x * width + y],
                           b = group->matrix[i * width + y];
                     group->matrix[x * width + y] = a * sine - b * cosine;
                     group->matrix[i * width + y] = a * cosine + b * sine;
                  }
               }
               index += i;
            }
         }
         else
         {
            if (width > 6)
               return RWMA_UNSUPPORTED;
            memcpy(group->matrix, fixed_matrix[width], width * width * sizeof(float));
         }
      }
      if (group->on)
      {
         int all = rwma_read(r, 1);
         for (i = 0; i < d->band_count[k]; i++)
            group->bands[i] = all || rwma_read(r, 1);
      }
      d->n_groups++;
   }
   return n || r->error ? RWMA_BAD_DATA : RWMA_OK;
}
static void pro_mix(rwma_t *d, int k, int length)
{
   int g, b, i, j, y;
   for (g = 0; g < d->n_groups; g++)
   {
      rwma_group *group = &d->group[g];
      int live = 0;
      if (!group->on)
         continue;
      for (i = 0; i < group->count; i++)
         live |= d->lane[group->members[i]].coded;
      if (live)
         for (i = 0; i < group->count; i++)
            d->lane[group->members[i]].active = 1;
      if (group->count == 2)
      {
         float *left = d->lane[group->members[0]].spectrum;
         float *right = d->lane[group->members[1]].spectrum;
         for (b = 0; b < d->band_count[k]; b++)
         {
            int end = RWMA_MIN(d->edges[k][b + 1], length);
            y = d->edges[k][b];
#ifdef RWMA_MDCT_SSE2
            if (group->bands[b])
            {
               __m128 m0 = _mm_set1_ps(group->matrix[0]);
               __m128 m1 = _mm_set1_ps(group->matrix[1]);
               __m128 m2 = _mm_set1_ps(group->matrix[2]);
               __m128 m3 = _mm_set1_ps(group->matrix[3]);
               __m128 zero = _mm_setzero_ps();
               for (; y + 3 < end; y += 4)
               {
                  __m128 a = _mm_loadu_ps(left + y);
                  __m128 z = _mm_loadu_ps(right + y);
                  _mm_storeu_ps(left + y,
                                _mm_add_ps(_mm_add_ps(zero, _mm_mul_ps(a, m0)),
                                           _mm_mul_ps(z, m1)));
                  _mm_storeu_ps(right + y,
                                _mm_add_ps(_mm_add_ps(zero, _mm_mul_ps(a, m2)),
                                           _mm_mul_ps(z, m3)));
               }
            }
            else if (d->config.channels == 2)
            {
               __m128 gain = _mm_set1_ps(181.0f / 128);
               for (; y + 3 < end; y += 4)
               {
                  _mm_storeu_ps(left + y, _mm_mul_ps(_mm_loadu_ps(left + y), gain));
                  _mm_storeu_ps(right + y, _mm_mul_ps(_mm_loadu_ps(right + y), gain));
               }
            }
#endif
            for (; y < end; y++)
            {
               float a = left[y], z = right[y];
               if (group->bands[b])
               {
                  left[y] = (0.0f + a * group->matrix[0]) + z * group->matrix[1];
                  right[y] = (0.0f + a * group->matrix[2]) + z * group->matrix[3];
               }
               else if (d->config.channels == 2)
               {
                  left[y] *= 181.0f / 128;
                  right[y] *= 181.0f / 128;
               }
            }
         }
         continue;
      }
      for (b = 0; b < d->band_count[k]; b++)
         for (y = d->edges[k][b]; y < d->edges[k][b + 1] && y < length; y++)
         {
            float vector[8];
            if (!group->bands[b])
            {
               if (d->config.channels == 2)
                  for (i = 0; i < group->count; i++)
                     d->lane[group->members[i]].spectrum[y] *= 181.0f / 128;
               continue;
            }
            for (i = 0; i < group->count; i++)
               vector[i] = d->lane[group->members[i]].spectrum[y];
            for (i = 0; i < group->count; i++)
            {
               float sum = 0;
               for (j = 0; j < group->count; j++)
                  sum += vector[j] * group->matrix[i * group->count + j];
               d->lane[group->members[i]].spectrum[y] = sum;
            }
         }
   }
}
static int pro_subframe(rwma_t *d, rwma_reader *r, int offset, int length)
{
   int p, b, y, k = rwma_log2(d->frame_length / length), any = 0;
   if ((unsigned)k > d->depth)
      return RWMA_BAD_DATA;
   /* The raw-XMA fill field is bounded by the supplied frame bit span. */
   if (rwma_read(r, 1))
   {
      unsigned fill = rwma_read(r, 2);
      if (!fill)
      {
         unsigned w = rwma_read(r, 4);
         fill = rwma_read(r, w) + 1;
      }
      if (fill > r->end - r->pos)
      {
         r->error = 1;
         return RWMA_BAD_DATA;
      }
      r->pos += fill;
   }
   if (rwma_read(r, 1))
      return RWMA_UNSUPPORTED;
   if (pro_groups(d, r, k))
      return RWMA_BAD_DATA;
   for (p = 0; p < d->n_participants; p++)
   {
      rwma_lane *l = &d->lane[d->participants[p]];
      l->spectrum = l->pcm + d->frame_length / 2 + offset;
      l->active = l->coded = rwma_read(r, 1);
      any |= l->coded;
   }
   if (any && pro_quant(d, r, k, length))
      return RWMA_BAD_DATA;
   for (p = 0; p < d->n_participants; p++)
   {
      rwma_lane *l = &d->lane[d->participants[p]];
      if (l->coded)
      {
         if (pro_coefficients(d, r, l, length))
            return RWMA_BAD_DATA;
      }
      else
         memset(l->spectrum, 0, length * sizeof(float));
   }
   pro_mix(d, k, length);
   for (p = 0; p < d->n_participants; p++)
   {
      rwma_lane *l = &d->lane[d->participants[p]];
      int overlap = RWMA_MIN(l->previous, length), base = d->frame_length / 2 + offset;
      const float *window = d->transform.windows[rwma_log2(overlap) - 6];
      if (l->active)
      {
         for (b = 0; b < d->band_count[k]; b++)
         {
            int exponent = l->step - (l->maximum - l->envelope[b]) * l->resolution;
            float gain;
            if (exponent > 400)
               return RWMA_BAD_DATA;
            gain = exponent >= -512 ? pro_gain[exponent + 512]
                                    : (float)pow(10.0, exponent / 20.0);
            y = d->edges[k][b];
#ifdef RWMA_MDCT_SSE2
            {
               __m128 factor = _mm_set1_ps(gain);
               for (; y + 3 < d->edges[k][b + 1]; y += 4)
                  _mm_storeu_ps(d->spectrum + y,
                                _mm_mul_ps(_mm_loadu_ps(l->spectrum + y), factor));
            }
#endif
            for (; y < d->edges[k][b + 1]; y++)
               d->spectrum[y] = l->spectrum[y] * gain;
         }
         rwma_mdct_run(&d->transform, l->spectrum, d->spectrum, length,
                       (d->bps == 16 ? 0.00006103515625f : 0.0000002384185791015625f) /
                           length);
      }
      else
         memset(l->spectrum, 0, length * sizeof(float));
      y = 0;
#ifdef RWMA_MDCT_SSE2
      for (; y + 3 < overlap / 2; y += 4)
      {
         int a = base - overlap / 2 + y, z = base + overlap / 2 - 4 - y;
         __m128 x = _mm_loadu_ps(l->pcm + a), v = _mm_loadu_ps(l->pcm + z);
         __m128 low = _mm_loadu_ps(window + y);
         __m128 high = _mm_loadu_ps(window + overlap - 4 - y), value;
         v = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 1, 2, 3));
         high = _mm_shuffle_ps(high, high, _MM_SHUFFLE(0, 1, 2, 3));
         value = _mm_add_ps(_mm_mul_ps(x, low), _mm_mul_ps(v, high));
         _mm_storeu_ps(l->pcm + a, _mm_sub_ps(_mm_mul_ps(x, high), _mm_mul_ps(v, low)));
         _mm_storeu_ps(l->pcm + z,
                       _mm_shuffle_ps(value, value, _MM_SHUFFLE(0, 1, 2, 3)));
      }
#endif
      for (; y < overlap / 2; y++)
      {
         int a = base - overlap / 2 + y, z = base + overlap / 2 - 1 - y;
         float x = l->pcm[a], v = l->pcm[z], low = window[y],
               high = window[overlap - 1 - y];
         l->pcm[a] = x * high - v * low;
         l->pcm[z] = x * low + v * high;
      }
      l->previous = length;
      l->decoded += length;
      l->cursor++;
   }
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}
static int pro_frame(rwma_t *d, rwma_reader *r, size_t extent, rwma_frame *out)
{
   int c, steps, offset, length;
   unsigned declared = rwma_read(r, d->length_bits);
   if ((extent ? declared != extent : declared != d->config.block_align * 8) ||
       declared <= d->length_bits)
      return RWMA_BAD_DATA;
   if (pro_tiles(d, r))
      return RWMA_BAD_DATA;
   if (d->config.channels > 1 && rwma_read(r, 1) && rwma_read(r, 1))
      return RWMA_UNSUPPORTED;
   if (d->flags & 128)
      (void)rwma_read(r, 8);
   if (rwma_read(r, 1))
   {
      if (rwma_read(r, 1))
         out->trim_start = rwma_read(r, rwma_log2(d->frame_length) + 1);
      if (rwma_read(r, 1))
         out->trim_end = rwma_read(r, rwma_log2(d->frame_length) + 1);
   }
   for (steps = 0; steps <= 32 * (int)d->config.channels; steps++)
   {
      offset = d->frame_length;
      for (c = 0; c < (int)d->config.channels; c++)
         offset = RWMA_MIN(offset, d->lane[c].decoded);
      if (offset == (int)d->frame_length)
         break;
      length = 0;
      d->n_participants = 0;
      for (c = 0; c < (int)d->config.channels; c++)
      {
         rwma_lane *l = &d->lane[c];
         if (l->decoded != offset)
            continue;
         if (l->cursor >= l->count)
            return RWMA_BAD_DATA;
         if (!length)
            length = l->lengths[l->cursor];
         if (l->lengths[l->cursor] == length)
            d->participants[d->n_participants++] = c;
      }
      if (!length || pro_subframe(d, r, offset, length))
         return RWMA_BAD_DATA;
   }
   if (steps > 32 * (int)d->config.channels || r->error || (extent && r->pos >= r->end))
      return RWMA_BAD_DATA;
   if (extent)
      r->pos = r->end - 1;
   else
      (void)rwma_read(r, 1);
   d->more = rwma_read(r, 1);
   if (r->error)
      return RWMA_BAD_DATA;
   for (c = 0; c < (int)d->config.channels; c++)
   {
      unsigned i;
      out->planes[c] = d->lane[c].pcm;
      i = 0;
#ifdef RWMA_MDCT_SSE2
      for (; i + 3 < d->frame_length; i += 4)
      {
         __m128i mask = _mm_set1_epi32(0x7f800000);
         __m128i v = _mm_castps_si128(_mm_loadu_ps(out->planes[c] + i));
         if (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_and_si128(v, mask), mask)))
            return RWMA_BAD_DATA;
      }
#endif
      for (; i < d->frame_length; i++)
         if (!rwma_finite(out->planes[c][i]))
            return RWMA_BAD_DATA;
   }
   out->samples = d->frame_length;
   out->channels = d->config.channels;
   out->sample_rate = d->config.sample_rate;
   d->pending = 1;
   return RWMA_OK;
}
void rwma_reset(rwma_t *d)
{
   unsigned c;
   if (d)
      d->drained = 0;
   if (!d)
      return;
   if (d->standard)
   {
      rwma_std_reset(d->standard);
      return;
   }
   for (c = 0; c < d->config.channels; c++)
   {
      float *pcm = d->lane[c].pcm;
      memset(&d->lane[c], 0, sizeof(d->lane[c]));
      d->lane[c].pcm = pcm;
      memset(pcm, 0, d->frame_length * 3 / 2 * sizeof(float));
      d->lane[c].previous = d->frame_length;
   }
   d->pending = d->failed = d->in_packet = 0;
   d->carry_bits = 0;
   d->packet = NULL;
   d->drop_first = d->config.codec == RWMA_PRO;
}
void rwma_free(rwma_t *d)
{
   if (d)
   {
      rwma_std_free(d->standard);
      free(d->carry);
      free(d->allocation);
      free(d);
   }
}
int rwma_parse_wave(rwma_config *cfg, const uint8_t *p, size_t bytes)
{
   unsigned tag, extra;
   uint32_t rate, average;
   if (!cfg)
      return RWMA_BAD_DATA;
   memset(cfg, 0, sizeof(*cfg));
   if (!p || bytes < 18)
      return RWMA_BAD_DATA;
   tag = p[0] | ((unsigned)p[1] << 8);
   cfg->codec = tag == 0x160 ? RWMA_V1 : tag == 0x161 ? RWMA_V2 :
         tag == 0x162 ? RWMA_PRO : 0;
   if (!cfg->codec)
      return RWMA_UNSUPPORTED;
   rate = p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) |
         ((uint32_t)p[7] << 24);
   average = p[8] | ((uint32_t)p[9] << 8) | ((uint32_t)p[10] << 16) |
         ((uint32_t)p[11] << 24);
   cfg->channels = p[2] | ((unsigned)p[3] << 8);
   cfg->block_align = p[12] | ((unsigned)p[13] << 8);
   if (rate < 8000 || rate > 192000 || !cfg->channels || cfg->channels > 8 ||
         !average || average > UINT_MAX / 8 || !cfg->block_align)
   {
      memset(cfg, 0, sizeof(*cfg));
      return RWMA_BAD_DATA;
   }
   cfg->sample_rate = rate;
   cfg->bit_rate = average * 8;
   extra = p[16] | ((unsigned)p[17] << 8);
   cfg->extra = p + 18;
   cfg->extra_size = RWMA_MIN(bytes - 18, extra);
   return RWMA_OK;
}
static int pro_setup_bands(rwma_t *d)
{
   unsigned k, j, b;
   for (k = 0; k <= d->depth; k++)
   {
      unsigned length = d->frame_length >> k, rate = d->config.sample_rate, n = 1;
      if (d->config.codec == RWMA_XMA_RAW)
         rate = rate > 44100   ? 48000
                : rate > 32000 ? 44100
                : rate > 24000 ? 32000
                               : 24000;
      d->edges[k][0] = 0;
      for (j = 0; j < 28; j++)
      {
         unsigned edge = (length * 2 * bandEdgeFreqs[j] / rate + 2) & ~3u;
         if (edge > (unsigned)d->edges[k][n - 1])
            d->edges[k][n++] = edge;
         if (edge >= length)
            break;
      }
      if (n < 2 || n > 29)
      {
         return RWMA_BAD_DATA;
      }
      d->edges[k][n - 1] = length;
      d->band_count[k] = n - 1;
   }
   for (k = 0; k <= d->depth; k++)
      for (j = 0; j <= d->depth; j++)
         for (b = 0; b < (unsigned)d->band_count[k]; b++)
         {
            unsigned at = 0, mid = ((d->edges[k][b] + d->edges[k][b + 1] - 1) << k) / 2;
            while (at + 1 < (unsigned)d->band_count[j] &&
                   ((unsigned)d->edges[j][at + 1] << j) < mid)
               at++;
            d->remap[k][j][b] = at;
         }
   return RWMA_OK;
}

int rwma_create(rwma_t **out, const rwma_config *cfg)
{
   rwma_t *d;
   unsigned c, flags = 0x10d6;
   size_t floats;
   float *p;
   if (!out)
      return RWMA_BAD_DATA;
   *out = NULL;
   if (!cfg || !cfg->channels || cfg->channels > 8 || cfg->sample_rate < 8000 ||
       cfg->sample_rate > 192000)
      return RWMA_BAD_DATA;
   if (cfg->codec == RWMA_V1 || cfg->codec == RWMA_V2)
   {
      int status;
      d = (rwma_t *)calloc(1, sizeof(*d));
      if (!d)
         return RWMA_NO_MEMORY;
      status = rwma_std_create(&d->standard, cfg);
      if (status)
      {
         free(d);
         return status;
      }
      d->config.codec = cfg->codec;
      *out = d;
      return RWMA_OK;
   }
   if (cfg->codec != RWMA_XMA_RAW && cfg->codec != RWMA_PRO)
      return RWMA_UNSUPPORTED;
   if (cfg->codec == RWMA_XMA_RAW && cfg->channels > 2)
      return RWMA_UNSUPPORTED;
   if (cfg->codec == RWMA_PRO && (!cfg->extra || cfg->extra_size < 18 ||
                                  !cfg->block_align || cfg->block_align > 32768))
      return RWMA_BAD_DATA;
   d = (rwma_t *)calloc(1, sizeof(*d));
   if (!d)
      return RWMA_NO_MEMORY;
   d->config = *cfg;
   d->capacity_channels = cfg->channels;
   d->frame_length = 512;
   d->bps = 16;
   d->length_bits = 15;
   if (cfg->codec == RWMA_PRO)
   {
      unsigned power = cfg->sample_rate <= 16000   ? 9
                       : cfg->sample_rate <= 22050 ? 10
                       : cfg->sample_rate <= 48000 ? 11
                       : cfg->sample_rate <= 96000 ? 12
                                                   : 13;
      flags = cfg->extra[14] | ((unsigned)cfg->extra[15] << 8);
      d->bps = cfg->extra[0] | ((unsigned)cfg->extra[1] << 8);
      switch (flags & 6)
      {
      case 2:
         power++;
         break;
      case 4:
         power--;
         break;
      case 6:
         power -= 2;
         break;
      }
      if (power > 13 || (d->bps != 16 && d->bps != 24) || !(flags & 64))
      {
         free(d);
         return RWMA_UNSUPPORTED;
      }
      d->frame_length = 1u << power;
      d->length_bits = rwma_log2(cfg->block_align) + 4;
   }
   d->depth = (flags >> 3) & 7;
   d->flags = flags;
   d->config.extra = NULL;
   d->config.extra_size = 0;
   if (d->depth > 5 || (d->frame_length >> d->depth) < 64)
   {
      free(d);
      return RWMA_BAD_DATA;
   }
   /* 8N transform plan + N quantised spectrum + 1.5N output per channel. */
   floats = d->frame_length * 9 + d->frame_length * cfg->channels * 3 / 2;
   if (cfg->codec == RWMA_PRO)
      floats += d->frame_length * cfg->channels / 2;
   d->allocation_floats = floats;
   d->allocation = (float *)calloc(floats, sizeof(float));
   if (!d->allocation)
   {
      free(d);
      return RWMA_NO_MEMORY;
   }
   if (cfg->codec == RWMA_PRO)
   {
      d->carry_capacity = (size_t)cfg->block_align * 8 * 16;
      d->carry = (uint8_t *)calloc((d->carry_capacity + 7) / 8, 1);
      if (!d->carry)
      {
         rwma_free(d);
         return RWMA_NO_MEMORY;
      }
   }
   p = d->allocation;
   for (c = 0; c < cfg->channels; c++)
   {
      d->lane[c].pcm = p;
      p += d->frame_length * 3 / 2;
   }
   d->spectrum = p;
   p += d->frame_length;
   rwma_mdct_init(&d->transform, p, d->frame_length);
   if (cfg->codec == RWMA_PRO)
      d->undo = d->allocation + floats - d->frame_length * cfg->channels / 2;
   for (c = 0; c <= 32; c++)
      d->sine[c] = (float)sin(c * RWMA_PI / 64);
   if (pro_setup_bands(d))
   {
      rwma_free(d);
      return RWMA_BAD_DATA;
   }
   rwma_reset(d);
   *out = d;
   return RWMA_OK;
}
int rwma_decode_xma(rwma_t *d, const uint8_t *data, size_t bytes, size_t offset,
                    size_t bits, rwma_frame *out)
{
   rwma_reader reader;
   unsigned c;
   int status;
   if (!out)
      return RWMA_BAD_DATA;
   memset(out, 0, sizeof(*out));
   if (!d || d->config.codec != RWMA_XMA_RAW || d->failed || d->drained || !data ||
       bytes > INT_MAX / 8 || offset > bytes * 8 || bits < 18 ||
       bits > bytes * 8 - offset)
      return RWMA_BAD_DATA;
   if (d->pending)
      for (c = 0; c < d->config.channels; c++)
         memmove(d->lane[c].pcm, d->lane[c].pcm + d->frame_length,
                 d->frame_length / 2 * sizeof(float));
   reader.p = data;
   reader.pos = offset;
   reader.end = offset + bits;
   reader.error = 0;
   status = pro_frame(d, &reader, bits, out);
   if (status)
   {
      d->failed = 1;
      memset(out, 0, sizeof(*out));
   }
   return status;
}
int rwma_drain_xma(rwma_t *d, rwma_frame *out)
{
   unsigned c;
   if (!out)
      return RWMA_BAD_DATA;
   memset(out, 0, sizeof(*out));
   if (!d || d->config.codec != RWMA_XMA_RAW || d->failed)
      return RWMA_BAD_DATA;
   if (d->drained || !d->pending)
      return RWMA_OK;
   for (c = 0; c < d->config.channels; c++)
   {
      rwma_lane *lane = &d->lane[c];
      unsigned y, length = lane->previous;
      float *tail = lane->pcm + d->frame_length;
      const float *window = d->transform.windows[rwma_log2(length) - 6];
      for (y = 0; y < length / 2; y++)
         tail[d->frame_length / 2 - length / 2 + y] *= window[length - 1 - y];
      out->planes[c] = tail;
   }
   out->samples = d->frame_length / 2;
   out->channels = d->config.channels;
   out->sample_rate = d->config.sample_rate;
   d->drained = 1;
   return RWMA_OK;
}
static void pro_roll(rwma_t *d)
{
   unsigned c;
   if (!d->pending)
      return;
   for (c = 0; c < d->config.channels; c++)
      memmove(d->lane[c].pcm, d->lane[c].pcm + d->frame_length,
              d->frame_length / 2 * sizeof(float));
   d->pending = 0;
}
static int pro_save(rwma_t *d, rwma_reader *r, size_t count)
{
   return rwma_append(d->carry, &d->carry_bits, d->carry_capacity, r, count);
}
static void pro_finish(rwma_t *d, rwma_reader *r, size_t *used)
{
   d->carry_bits = 0;
   if (pro_save(d, r, r->end - r->pos))
      d->failed = 1;
   d->in_packet = 0;
   d->packet = NULL;
   *used = d->config.block_align;
}
/* A clamped prefix has no end offset: retry only after restoring its overlap. */
static int pro_open_frame(rwma_t *d, rwma_reader *reader, rwma_frame *out)
{
   int previous[8], status;
   unsigned c;
   size_t count = d->frame_length / 2;
   for (c = 0; c < d->config.channels; c++)
   {
      memcpy(d->undo + c * count, d->lane[c].pcm, count * sizeof(float));
      previous[c] = d->lane[c].previous;
   }
   status = pro_frame(d, reader, 0, out);
   if (!status)
      return RWMA_OK;
   for (c = 0; c < d->config.channels; c++)
   {
      memcpy(d->lane[c].pcm, d->undo + c * count, count * sizeof(float));
      d->lane[c].previous = previous[c];
   }
   d->pending = 0;
   memset(out, 0, sizeof(*out));
   return reader->error ? 1 : status;
}
int rwma_decode_packet(rwma_t *d, const uint8_t *data, size_t bytes, size_t *used,
                       rwma_frame *out)
{
   rwma_reader r;
   unsigned step;
   int carried = 0;
   if (!used || !out)
      return RWMA_BAD_DATA;
   *used = 0;
   memset(out, 0, sizeof(*out));
   if (!d || d->failed)
      return RWMA_BAD_DATA;
   if (d->standard)
      return rwma_std_decode(d->standard, data, bytes, used, out);
   if (d->config.codec != RWMA_PRO)
      return RWMA_UNSUPPORTED;
   if (!bytes)
      return d->in_packet ? RWMA_BAD_DATA : RWMA_OK;
   if (!data || bytes < d->config.block_align)
      return RWMA_BAD_DATA;
   if (d->in_packet && data != d->packet)
      return RWMA_BAD_DATA;
   pro_roll(d);
   r.p = data;
   r.pos = 0;
   r.end = d->config.block_align * 8;
   r.error = 0;
   if (!d->in_packet)
   {
      unsigned previous, available, continuation;
      (void)rwma_read(&r, 6);
      previous = rwma_read(&r, d->length_bits);
      if (r.error)
         goto invalid;
      available = (unsigned)(r.end - r.pos);
      continuation = RWMA_MIN(previous, available);
      d->packet = data;
      d->in_packet = 1;
      d->packet_at = r.pos + continuation;
      if (previous && d->carry_bits)
      {
         rwma_reader joined;
         unsigned length;
         if (pro_save(d, &r, continuation))
            goto invalid;
         joined.p = d->carry;
         joined.pos = 0;
         joined.end = d->carry_bits;
         joined.error = 0;
         length = rwma_read(&joined, d->length_bits);
         if (length <= d->length_bits)
            goto invalid;
         joined.pos = 0;
         if (length == d->config.block_align * 8)
         {
            int status = pro_open_frame(d, &joined, out);
            if (status < 0)
               goto invalid;
            if (status)
            {
               if (previous <= available)
                  goto invalid;
               d->in_packet = 0;
               d->packet = NULL;
               *used = d->config.block_align;
               return RWMA_OK;
            }
         }
         else
         {
            if (length > d->carry_bits)
            {
               if (previous <= available)
                  goto invalid;
               d->in_packet = 0;
               d->packet = NULL;
               *used = d->config.block_align;
               return RWMA_OK;
            }
            joined.end = length;
            if (pro_frame(d, &joined, length, out))
               goto invalid;
         }
         d->carry_bits = 0;
         if (d->drop_first)
         {
            d->drop_first = 0;
            memset(out, 0, sizeof(*out));
            pro_roll(d);
         }
         else
         {
            carried = 1;
            goto produced;
         }
      }
      else
         d->carry_bits = 0;
      if (previous > available)
      {
         d->in_packet = 0;
         d->packet = NULL;
         *used = d->config.block_align;
         return RWMA_OK;
      }
   }
   for (step = 0; step < 256; step++)
   {
      size_t start;
      unsigned length;
      r.pos = d->packet_at;
      if (r.pos > r.end)
         goto invalid;
      if (r.end - r.pos <= d->length_bits)
      {
         pro_finish(d, &r, used);
         return d->failed ? RWMA_BAD_DATA : RWMA_OK;
      }
      start = r.pos;
      length = rwma_read(&r, d->length_bits);
      r.pos = start;
      if (!length || length > r.end - start)
      {
         pro_finish(d, &r, used);
         return d->failed ? RWMA_BAD_DATA : RWMA_OK;
      }
      {
         rwma_reader frame = r;
         frame.end = start + length;
         if (pro_frame(d, &frame, length, out))
            goto invalid;
      }
      d->packet_at = start + length;
      if (d->drop_first)
      {
         d->drop_first = 0;
         memset(out, 0, sizeof(*out));
         pro_roll(d);
         if (!d->more)
         {
            r.pos = d->packet_at;
            pro_finish(d, &r, used);
            return RWMA_OK;
         }
         continue;
      }
   produced:
      if (out->trim_start + out->trim_end < out->samples)
      {
         unsigned c;
         for (c = 0; c < out->channels; c++)
            out->planes[c] += out->trim_start;
         out->samples -= out->trim_start + out->trim_end;
      }
      else
         out->samples = 0;
      out->trim_start = out->trim_end = 0;
      if (!d->more && !carried)
      {
         r.pos = d->packet_at;
         pro_finish(d, &r, used);
      }
      return RWMA_OK;
   }
invalid:
   d->failed = 1;
   memset(out, 0, sizeof(*out));
   return RWMA_BAD_DATA;
}

int rwma_xma_capture(const rwma_t *d, rwma_xma_view *out)
{
   unsigned c;
   if (!out)
      return RWMA_BAD_DATA;
   memset(out, 0, sizeof(*out));
   if (!d)
      return RWMA_BAD_DATA;
   if (d->config.codec != RWMA_XMA_RAW)
      return RWMA_UNSUPPORTED;
   out->state.version = 1;
   out->state.sample_rate = d->config.sample_rate;
   out->state.channels = d->config.channels;
   out->state.failed = d->failed != 0;
   out->state.pending = !d->failed && d->pending;
   out->state.drained = !d->failed && d->drained;
   for (c = 0; c < d->config.channels; c++)
   {
      out->state.previous[c] =
          d->failed ? d->frame_length : (unsigned)d->lane[c].previous;
      if (out->state.pending)
      {
         out->history[c] = d->lane[c].pcm + d->frame_length;
         out->history_samples[c] = d->frame_length / 2;
      }
   }
   return RWMA_OK;
}

int rwma_xma_restore(rwma_t *d, const rwma_xma_view *source)
{
   rwma_xma_view view;
   uintptr_t storage;
   size_t storage_bytes;
   unsigned c, samples;
   if (!d || !source)
      return RWMA_BAD_DATA;
   if (d->config.codec != RWMA_XMA_RAW)
      return RWMA_UNSUPPORTED;
   view = *source;
   if (view.state.version != 1 || view.state.sample_rate != d->config.sample_rate ||
       view.state.channels != d->config.channels || view.state.pending > 1 ||
       view.state.drained > view.state.pending || view.state.failed > 1 ||
       (view.state.failed && view.state.pending))
      return RWMA_BAD_DATA;
   samples = view.state.pending ? d->frame_length / 2 : 0;
   storage = (uintptr_t)d->allocation;
   storage_bytes = d->allocation_floats * sizeof(float);
   for (c = 0; c < 2; c++)
   {
      unsigned i;
      uintptr_t address;
      size_t bytes = samples * sizeof(float);
      if (c >= view.state.channels)
      {
         if (view.state.previous[c] || view.history[c] || view.history_samples[c])
            return RWMA_BAD_DATA;
         continue;
      }
      if (view.history_samples[c] != samples || (!!view.history[c] != !!samples) ||
          (view.state.previous[c] != 128 && view.state.previous[c] != 256 &&
           view.state.previous[c] != 512) ||
          (!view.state.pending && view.state.previous[c] != 512))
         return RWMA_BAD_DATA;
      if (!samples)
         continue;
      address = (uintptr_t)view.history[c];
      if (address % sizeof(float) || address > UINTPTR_MAX - bytes)
         return RWMA_BAD_DATA;
      if (address < storage + storage_bytes && address + bytes > storage &&
          view.history[c] != d->lane[c].pcm + d->frame_length)
         return RWMA_BAD_DATA;
      for (i = 0; i < samples; i++)
         if (!rwma_finite(view.history[c][i]))
            return RWMA_BAD_DATA;
   }
   for (c = 0; c < view.state.channels; c++)
      if (samples)
         memmove(d->lane[c].pcm + d->frame_length, view.history[c],
                 samples * sizeof(float));
   for (c = 0; c < view.state.channels; c++)
   {
      rwma_lane *lane = &d->lane[c];
      float *pcm = lane->pcm;
      memset(lane, 0, sizeof(*lane));
      lane->pcm = pcm;
      lane->previous = view.state.previous[c];
      if (!samples)
         memset(pcm, 0, d->frame_length * 3 / 2 * sizeof(float));
   }
   d->pending = view.state.pending;
   d->drained = view.state.drained;
   d->failed = view.state.failed;
   return RWMA_OK;
}

int rwma_xma_configure(rwma_t *d, unsigned rate, unsigned channels)
{
   unsigned previous_rate;
   if (!d)
      return RWMA_BAD_DATA;
   if (d->config.codec != RWMA_XMA_RAW || !channels ||
       channels > d->capacity_channels ||
       (rate != 24000 && rate != 32000 && rate != 44100 && rate != 48000))
      return RWMA_UNSUPPORTED;
   if (rate == d->config.sample_rate && channels == d->config.channels)
      return RWMA_OK;
   previous_rate = d->config.sample_rate;
   d->config.sample_rate = rate;
   if (pro_setup_bands(d))
   {
      d->config.sample_rate = previous_rate;
      (void)pro_setup_bands(d);
      return RWMA_BAD_DATA;
   }
   d->config.channels = channels;
   rwma_reset(d);
   return RWMA_OK;
}
