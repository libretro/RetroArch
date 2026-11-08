/* MIT: Copyright (c) 2026 Cole Springer and The RetroArch team.
 * C89 adaptation of WaxFlow codec/wma, commit 2f448c9435fd.
 * See COPYING.MIT.WaxFlow and PROVENANCE.md. */
#include "rwma_internal.h"
#include "rwma_mdct.h"
#include "rwma_standard_data.h"

typedef struct std_size
{
   unsigned end, high, count, nhigh;
   unsigned bands[25], low[25], upper[25];
} std_size;

typedef struct rwma_standard
{
   rwma_config config;
   rwma_mdct mdct;
   float *allocation, *exp[2], *coef[2], *spec[2], *acc[2], *block, *noise;
   uint8_t *carry;
   const uint8_t *packet;
   size_t carry_bits, packet_bit;
   double lsp_cos[2048];
   float exp_table[156], maximum[2];
   unsigned exp_length[2], noise_on[2][25];
   int noise_gain[2][25];
   std_size sizes[5];
   unsigned length, length_bits, flags, count, selector, book, noise_at;
   unsigned use_noise, v2, prev, current, next, reset_lengths;
   unsigned failed, pending, in_packet, left, drained;
   float noise_mult;
} rwma_standard;

void rwma_std_free(void *opaque)
{
   rwma_standard *s = (rwma_standard *)opaque;
   if (!s)
      return;
   free(s->carry);
   free(s->allocation);
   free(s);
}

void rwma_std_reset(void *opaque)
{
   rwma_standard *s = (rwma_standard *)opaque;
   unsigned c;
   for (c = 0; c < s->config.channels; c++)
   {
      memset(s->acc[c], 0, 2 * s->length * sizeof(float));
      memset(s->exp[c], 0, s->length * sizeof(float));
      s->exp_length[c] = 0;
      s->maximum[c] = 0;
   }
   s->carry_bits = s->packet_bit = 0;
   s->failed = s->pending = s->in_packet = s->left = s->drained = 0;
   s->noise_at = 0;
   s->packet = NULL;
   s->reset_lengths = 1;
}

static double std_high(unsigned rate, double bps, double weighted, unsigned *noise)
{
   *noise = 1;
   switch (rate)
   {
   case 44100:
      if (weighted < 0.61)
         return 0.4;
      *noise = 0;
      return 1;
   case 22050:
      if (weighted < 0.72)
         return 0.6;
      if (weighted < 1.16)
         return 0.7;
      *noise = 0;
      return 1;
   case 16000:
      return bps > 0.5 ? 0.5 : 0.3;
   case 11025:
      return 0.7;
   case 8000:
      if (bps <= 0.625)
         return 0.5;
      if (bps <= 0.75)
         return 0.65;
      *noise = 0;
      return 1;
   default:
      return bps >= 0.8 ? 0.75 : bps >= 0.6 ? 0.6 : 0.5;
   }
}

int rwma_std_create(void **out, const rwma_config *cfg)
{
   rwma_standard *s;
   unsigned i, c, k, rate, extra_at, offset;
   uint32_t seed = 1;
   double bps, weighted, high;
   float *p;
   *out = NULL;
   if (cfg->channels > 2 || !cfg->block_align || cfg->block_align > 32768 ||
       !cfg->bit_rate || cfg->bit_rate > 10000000 || cfg->sample_rate > 48000)
      return RWMA_UNSUPPORTED;
   s = (rwma_standard *)calloc(1, sizeof(*s));
   if (!s)
      return RWMA_NO_MEMORY;
   s->config = *cfg;
   s->config.extra = NULL;
   s->config.extra_size = 0;
   s->v2 = cfg->codec == RWMA_V2;
   extra_at = s->v2 ? 4 : 2;
   s->flags = cfg->extra && cfg->extra_size >= extra_at + 2
                  ? cfg->extra[extra_at] | ((unsigned)cfg->extra[extra_at + 1] << 8)
                  : 0;
   if (s->v2 && s->flags == 13)
      s->flags &= ~4u;
   s->length_bits = cfg->sample_rate <= 16000 ? 9
                    : cfg->sample_rate <= 22050 || (!s->v2 && cfg->sample_rate <= 32000)
                        ? 10
                        : 11;
   s->length = 1u << s->length_bits;
   s->count = 1;
   if (s->flags & 4)
   {
      unsigned depth = ((s->flags >> 3) & 3) + 1;
      if (cfg->bit_rate / cfg->channels >= 32000)
         depth += 2;
      s->count = RWMA_MIN(depth, s->length_bits - 7) + 1;
      s->selector = rwma_log2(s->count - 1) + 1;
   }
   bps = (double)cfg->bit_rate / (cfg->sample_rate * cfg->channels);
   weighted = bps * (cfg->channels == 2 ? 1.6 : 1.0);
   s->book =
       cfg->sample_rate >= 32000 && weighted < 1.16 ? (weighted < 0.72 ? 0 : 2) : 4;
   offset = (unsigned)(bps * s->length / 8 + 0.5);
   if (rwma_log2(offset ? offset : 1) + 5 > 31)
   {
      rwma_std_free(s);
      return RWMA_BAD_DATA;
   }
   rate = cfg->sample_rate;
   if (s->v2)
   {
      static const unsigned rates[] = {44100, 22050, 16000, 11025, 8000};
      for (i = 0; i < 5; i++)
         if (rate >= rates[i])
         {
            rate = rates[i];
            break;
         }
   }
   high = std_high(rate, bps, weighted, &s->use_noise);
   s->noise_mult = s->flags & 1 ? 0.02f : 0.04f;
   for (k = 0; k < s->count; k++)
   {
      std_size *z = &s->sizes[k];
      unsigned n = s->length >> k, previous = 0;
      int row = (int)s->length_bits - 7 - (int)k;
      z->end = (s->length - s->length * 9 / 100) >> k;
      z->high = (unsigned)(n * high + 0.5);
      if (s->v2 && cfg->sample_rate >= 22050 && row >= 0 && row < 3)
      {
         const uint16_t *bands = cfg->sample_rate >= 44100   ? std_bands44100[row]
                                 : cfg->sample_rate >= 32000 ? std_bands32000[row]
                                                             : std_bands22050[row];
         z->count = bands[0];
         for (i = 0; i < z->count; i++)
            z->bands[i] = bands[i + 1];
      }
      else
         for (i = 0; i < 25; i++)
         {
            unsigned end =
                s->v2
                    ? (n * 2 * std_freqs[i] + 2 * cfg->sample_rate) /
                          (4 * cfg->sample_rate) * 4
                    : (unsigned)((double)n * 2 * std_freqs[i] / cfg->sample_rate + 0.5);
            end = RWMA_MIN(end, n);
            if (end > previous)
               z->bands[z->count++] = end - previous;
            previous = end;
            if (end >= n)
               break;
         }
      previous = 0;
      for (i = 0; i < z->count; i++)
      {
         unsigned low = previous > z->high ? previous : z->high;
         unsigned end = RWMA_MIN(previous + z->bands[i], z->end);
         if (low < end)
         {
            z->low[z->nhigh] = low;
            z->upper[z->nhigh++] = end;
         }
         previous += z->bands[i];
      }
      if (!z->count || previous < z->end || previous > n)
      {
         rwma_std_free(s);
         return RWMA_BAD_DATA;
      }
   }
   s->allocation =
       (float *)calloc(s->length * (10 + 5 * cfg->channels) + 8192, sizeof(float));
   if (!s->allocation)
   {
      rwma_std_free(s);
      return RWMA_NO_MEMORY;
   }
   if (s->flags & 2)
   {
      s->carry = (uint8_t *)calloc(1024 * 1024, 1);
      if (!s->carry)
      {
         rwma_std_free(s);
         return RWMA_NO_MEMORY;
      }
   }
   p = s->allocation;
   rwma_mdct_init(&s->mdct, p, s->length);
   p += 8 * s->length;
   for (c = 0; c < cfg->channels; c++)
   {
      s->exp[c] = p;
      p += s->length;
      s->coef[c] = p;
      p += s->length;
      s->spec[c] = p;
      p += s->length;
      s->acc[c] = p;
      p += 2 * s->length;
   }
   s->block = p;
   p += 2 * s->length;
   s->noise = p;
   for (i = 0; i < 8192; i++)
   {
      double value;
      seed = seed * 314159u + 1;
      value = seed <= INT_MAX ? (double)seed : (double)seed - 4294967296.0;
      s->noise[i] = (float)(value / 2147483648.0 * sqrt(3.0) * s->noise_mult);
   }
   for (i = 0; i < 156; i++)
      s->exp_table[i] = (float)pow(10.0, ((int)i - 60) / 16.0);
   for (i = 0; i < s->length; i++)
      s->lsp_cos[i] = 2 * cos(RWMA_PI * i / s->length);
   rwma_std_reset(s);
   *out = s;
   return RWMA_OK;
}

static float std_noise(rwma_standard *s)
{
   float value = s->noise[s->noise_at];
   s->noise_at = (s->noise_at + 1) & 8191;
   return value;
}

static int std_exponents(rwma_standard *s, rwma_reader *r, unsigned c,
                         const std_size *z, unsigned n)
{
   unsigned i, j, at = 0;
   float maximum = 0;
   if (s->flags & 1)
   {
      int index = s->v2 ? 36 : (int)rwma_read(r, 5) + 10;
      for (i = 0; i < z->count; i++)
      {
         float value;
         if (i || s->v2)
            index += rwma_symbol(r, std_scale) - 60;
         if (r->error || index < -60 || index > 95)
            return RWMA_BAD_DATA;
         value = s->exp_table[index + 60];
         if (value > maximum)
            maximum = value;
         for (j = 0; j < z->bands[i]; j++)
            s->exp[c][at++] = value;
      }
      memset(s->exp[c] + at, 0, (n - at) * sizeof(float));
   }
   else
   {
      double lsp[10];
      for (j = 0; j < 10; j++)
         lsp[j] = std_lsp[j][rwma_read(r, j == 0 || j >= 8 ? 3 : 4)];
      for (i = 0; i < n; i++)
      {
         double p = 0.5, q = 0.5, w = s->lsp_cos[i], energy;
         float value;
         for (j = 1; j < 10; j += 2)
         {
            q *= w - lsp[j - 1];
            p *= w - lsp[j];
         }
         energy = p * p * (2 - w) + q * q * (2 + w);
         if (energy <= 0)
            return RWMA_BAD_DATA;
         value = (float)(1 / sqrt(sqrt(energy)));
         s->exp[c][i] = value;
         if (value > maximum)
            maximum = value;
      }
   }
   s->maximum[c] = maximum;
   s->exp_length[c] = n;
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}

static float std_exp(const rwma_standard *s, unsigned c, unsigned i, unsigned n)
{
   return s->exp[c][i * s->exp_length[c] / n];
}

static double std_power(const rwma_standard *s, unsigned c, unsigned lo, unsigned hi,
                        unsigned n)
{
   unsigned i;
   double sum = 0;
   for (i = lo; i < hi; i++)
   {
      double value = std_exp(s, c, i, n);
      sum += value * value;
   }
   return sum / (hi - lo);
}

static int std_reconstruct(rwma_standard *s, unsigned c, const std_size *z, unsigned n,
                           unsigned gain)
{
   unsigned i, j, ptr = 0, start = s->v2 ? 0 : 3, lo = RWMA_MIN(z->high, z->end);
   int last = -1;
   double norm = 2.0 / n / 32768, factor, reference = 0;
   float mult;
   if (!s->v2)
      norm *= sqrt(n / 2.0);
   factor = pow(10.0, gain / 20.0) / s->maximum[c] * norm;
   mult = (float)factor;
   if (!rwma_finite(mult))
      return RWMA_BAD_DATA;
   for (i = 0; i < start; i++)
      s->spec[c][i] = s->use_noise ? std_noise(s) * std_exp(s, c, i, n) * mult : 0;
   for (i = start; i < lo; i++)
   {
      float value = s->coef[c][ptr++];
      if (s->use_noise)
         value += std_noise(s);
      s->spec[c][i] = value * std_exp(s, c, i, n) * mult;
   }
   if (s->use_noise)
   {
      for (j = 0; j < z->nhigh; j++)
         if (s->noise_on[c][j])
            last = (int)j;
      if (last >= 0)
         reference = std_power(s, c, z->low[last], z->upper[last], n);
      for (j = 0; j < z->nhigh; j++)
      {
         if (!s->noise_on[c][j])
            for (i = z->low[j]; i < z->upper[j]; i++)
               s->spec[c][i] =
                   (s->coef[c][ptr++] + std_noise(s)) * std_exp(s, c, i, n) * mult;
         else
         {
            float g =
                (float)(sqrt(std_power(s, c, z->low[j], z->upper[j], n) / reference) *
                        pow(10.0, s->noise_gain[c][j] / 20.0) /
                        (s->maximum[c] * s->noise_mult) * norm);
            if (!rwma_finite(g))
               return RWMA_BAD_DATA;
            for (i = z->low[j]; i < z->upper[j]; i++)
               s->spec[c][i] = std_noise(s) * std_exp(s, c, i, n) * g;
         }
      }
      for (i = z->end; i < n; i++)
         s->spec[c][i] = std_noise(s) * (std_exp(s, c, z->end - 1, n) * mult);
   }
   else
   {
      /* With noise disabled the codeable span reaches past highStart. */
      for (i = lo; i < z->end; i++)
         s->spec[c][i] = s->coef[c][ptr++] * std_exp(s, c, i, n) * mult;
      memset(s->spec[c] + z->end, 0, (n - z->end) * sizeof(float));
   }
   return RWMA_OK;
}

static void std_transform(rwma_standard *s, unsigned coded[2], unsigned ms, unsigned n,
                          unsigned at)
{
   unsigned c, i, prev = 1u << s->prev, next = 1u << s->next;
   const float *window = s->mdct.windows[rwma_log2(n) - 6];
   for (c = 0; c < s->config.channels; c++)
   {
      float *dst = s->acc[c] + s->length / 2 + at - n / 2;
      if (coded[c])
      {
         rwma_mdct_run(&s->mdct, s->block + n / 2, s->spec[c], n, 1);
         for (i = 0; i < n / 2; i++)
            s->block[n / 2 - 1 - i] = -s->block[n / 2 + i];
         for (i = 0; i < n / 2; i++)
            s->block[3 * n / 2 + i] = s->block[3 * n / 2 - 1 - i];
      }
      else if (!(ms && c == 1))
         memset(s->block, 0, 2 * n * sizeof(float));
      if (n <= prev)
         for (i = 0; i < n; i++)
            dst[i] += s->block[i] * window[i];
      else
      {
         unsigned m = (n - prev) / 2;
         const float *w = s->mdct.windows[s->prev - 6];
         for (i = 0; i < prev; i++)
            dst[m + i] += s->block[m + i] * w[i];
         memcpy(dst + m + prev, s->block + m + prev, (n - m - prev) * sizeof(float));
      }
      if (n <= next)
         for (i = 0; i < n; i++)
            dst[n + i] = s->block[n + i] * window[n - 1 - i];
      else
      {
         unsigned m = (n - next) / 2;
         const float *w = s->mdct.windows[s->next - 6];
         memcpy(dst + n, s->block + n, m * sizeof(float));
         for (i = 0; i < next; i++)
            dst[n + m + i] = s->block[n + m + i] * w[next - 1 - i];
         memset(dst + n + m + next, 0, (n - m - next) * sizeof(float));
      }
   }
}

static int std_frame(rwma_standard *s, rwma_reader *r, rwma_frame *out)
{
   unsigned at = 0, c, i, j;
   while (at < s->length)
   {
      unsigned coded[2] = {0, 0}, ms = 0, any = 0, n, gain = 1, width, transmit, k;
      const std_size *z;
      if (!s->selector)
         s->prev = s->current = s->next = s->length_bits;
      else
      {
         unsigned selected;
         s->prev = s->current;
         s->current = s->next;
         if (s->reset_lengths)
         {
            selected = rwma_read(r, s->selector);
            if (selected >= s->count)
               return RWMA_BAD_DATA;
            s->prev = s->length_bits - selected;
            selected = rwma_read(r, s->selector);
            if (selected >= s->count)
               return RWMA_BAD_DATA;
            s->current = s->length_bits - selected;
         }
         selected = rwma_read(r, s->selector);
         if (selected >= s->count)
            return RWMA_BAD_DATA;
         s->next = s->length_bits - selected;
      }
      s->reset_lengths = 0;
      n = 1u << s->current;
      k = s->length_bits - s->current;
      if (n > s->length - at || r->error)
         return RWMA_BAD_DATA;
      z = &s->sizes[k];
      if (s->config.channels == 2)
         ms = rwma_read(r, 1);
      for (c = 0; c < s->config.channels; c++)
      {
         coded[c] = rwma_read(r, 1);
         any |= coded[c];
      }
      if (any)
      {
         unsigned part;
         do
         {
            part = rwma_read(r, 7);
            gain += part;
            if (gain > 4096 || r->error)
               return RWMA_BAD_DATA;
         } while (part == 127);
         width = gain < 15 ? 13 : gain < 32 ? 12 : gain < 40 ? 11 : gain < 45 ? 10 : 9;
         if (s->use_noise)
         {
            for (c = 0; c < s->config.channels; c++)
               if (coded[c])
                  for (j = 0; j < z->nhigh; j++)
                     s->noise_on[c][j] = rwma_read(r, 1);
            for (c = 0; c < s->config.channels; c++)
               if (coded[c])
               {
                  int value = 0, first = 1;
                  for (j = 0; j < z->nhigh; j++)
                     if (s->noise_on[c][j])
                     {
                        if (first)
                        {
                           value = (int)rwma_read(r, 7) - 19;
                           first = 0;
                        }
                        else
                        {
                           int symbol = rwma_symbol(r, std_gain);
                           if (symbol < 0)
                              return RWMA_BAD_DATA;
                           value += std_gain_delta[symbol];
                        }
                        s->noise_gain[c][j] = value;
                     }
               }
         }
         transmit = n == s->length || rwma_read(r, 1);
         for (c = 0; c < s->config.channels; c++)
            if (coded[c])
            {
               if (transmit)
               {
                  if (std_exponents(s, r, c, z, n))
                     return RWMA_BAD_DATA;
               }
               else if (!s->exp_length[c])
                  return RWMA_BAD_DATA;
            }
         for (c = 0; c < s->config.channels; c++)
         {
            if (coded[c])
            {
               unsigned end = z->end - (s->v2 ? 0 : 3), offset = 0,
                        book = s->book + (ms && c == 1);
               memset(s->coef[c], 0, n * sizeof(float));
               if (s->use_noise)
                  for (j = 0; j < z->nhigh; j++)
                     if (s->noise_on[c][j])
                        end -= z->upper[j] - z->low[j];
               if (end > n)
                  return RWMA_BAD_DATA;
               while (offset < end)
               {
                  int symbol = rwma_symbol(r, std_books[book]);
                  unsigned run, level;
                  if (symbol < 0)
                     return RWMA_BAD_DATA;
                  if (symbol == 1)
                     break;
                  if (!symbol)
                  {
                     level = rwma_read(r, width);
                     run = rwma_read(r, s->length_bits);
                  }
                  else
                  {
                     run = std_run[book][symbol];
                     level = std_level[book][symbol];
                  }
                  offset += run;
                  if (offset >= end || r->error)
                     return RWMA_BAD_DATA;
                  s->coef[c][offset++] = rwma_read(r, 1) ? (float)level : -(float)level;
               }
            }
            if (!s->v2 && s->config.channels == 2)
               r->pos = (r->pos + 7) & ~(size_t)7;
         }
         for (c = 0; c < s->config.channels; c++)
            if (coded[c])
               if (std_reconstruct(s, c, z, n, gain))
                  return RWMA_BAD_DATA;
         if (ms && coded[1])
         {
            if (!coded[0])
            {
               memset(s->spec[0], 0, n * sizeof(float));
               coded[0] = 1;
            }
            for (i = 0; i < n; i++)
            {
               float a = s->spec[0][i], b = s->spec[1][i];
               s->spec[0][i] = a + b;
               s->spec[1][i] = a - b;
            }
         }
      }
      if (r->error || r->pos > r->end)
         return RWMA_BAD_DATA;
      std_transform(s, coded, ms, n, at);
      at += n;
   }
   out->samples = s->length;
   out->channels = s->config.channels;
   out->sample_rate = s->config.sample_rate;
   for (c = 0; c < s->config.channels; c++)
   {
      out->planes[c] = s->acc[c];
      for (i = 0; i < s->length; i++)
         if (!rwma_finite(s->acc[c][i]))
            return RWMA_BAD_DATA;
   }
   s->pending = 1;
   return RWMA_OK;
}

static void std_roll(rwma_standard *s)
{
   unsigned c;
   if (!s->pending)
      return;
   for (c = 0; c < s->config.channels; c++)
   {
      memmove(s->acc[c], s->acc[c] + s->length, s->length * sizeof(float));
      memset(s->acc[c] + s->length, 0, s->length * sizeof(float));
   }
   s->pending = 0;
}

static int std_save(rwma_standard *s, rwma_reader *r, size_t bits)
{
   return rwma_append(s->carry, &s->carry_bits, 8u * 1024 * 1024, r, bits);
}
int rwma_std_decode(void *opaque, const uint8_t *data, size_t bytes, size_t *used,
                    rwma_frame *out)
{
   rwma_standard *s = (rwma_standard *)opaque;
   rwma_reader r;
   unsigned step;
   if (s->failed)
      return RWMA_BAD_DATA;
   std_roll(s);
   if (!bytes)
   {
      if (s->in_packet)
         return RWMA_BAD_DATA;
      s->drained = 1;
      return RWMA_OK;
   }
   if (s->drained || !data || bytes < s->config.block_align ||
       (s->in_packet && data != s->packet))
      return RWMA_BAD_DATA;
   r.p = data;
   r.pos = s->packet_bit;
   r.end = s->config.block_align * 8;
   r.error = 0;
   if (!(s->flags & 2))
   {
      r.pos = 0;
      if (std_frame(s, &r, out))
         goto invalid;
      *used = s->config.block_align;
      return RWMA_OK;
   }
   if (!s->in_packet)
   {
      double bps =
          (double)s->config.bit_rate / (s->config.channels * s->config.sample_rate);
      unsigned value = (unsigned)(bps * s->length / 8 + 0.5),
               width = rwma_log2(value ? value : 1) + 5;
      unsigned offset;
      r.pos = 4;
      s->left = rwma_read(&r, 4);
      offset = rwma_read(&r, width);
      if (r.error || offset > r.end - r.pos)
         goto invalid;
      s->packet = data;
      s->in_packet = 1;
      s->packet_bit = r.pos + offset;
      if (!s->carry_bits)
      {
         if (!s->left)
            goto invalid;
         s->left--;
         s->reset_lengths = 1;
      }
      if (!s->left)
      {
         if (!s->carry_bits)
            r.pos = s->packet_bit;
         if (std_save(s, &r, r.end - r.pos))
            goto invalid;
         s->in_packet = 0;
         *used = s->config.block_align;
         return RWMA_OK;
      }
      if (s->carry_bits)
      {
         rwma_reader joined;
         if (std_save(s, &r, offset))
            goto invalid;
         joined.p = s->carry;
         joined.pos = 0;
         joined.end = s->carry_bits;
         joined.error = 0;
         if (std_frame(s, &joined, out))
            goto invalid;
         s->carry_bits = 0;
         s->left--;
         s->reset_lengths = 1;
         if (out->samples)
            goto produced;
         std_roll(s);
      }
   }
   for (step = 0; step < 16 && s->left; step++)
   {
      r.pos = s->packet_bit;
      if (std_frame(s, &r, out))
         goto invalid;
      s->packet_bit = r.pos;
      s->left--;
      if (out->samples)
         goto produced;
      std_roll(s);
   }
produced:
   if (!s->left)
   {
      r.pos = s->packet_bit;
      s->carry_bits = 0;
      if (std_save(s, &r, r.end - r.pos))
         goto invalid;
      s->in_packet = 0;
      *used = s->config.block_align;
   }
   return RWMA_OK;
invalid:
   s->failed = 1;
   memset(out, 0, sizeof(*out));
   return RWMA_BAD_DATA;
}
