/* Compare ring search against independently gathered candidate windows. */
#include <stdio.h>
#include "../../../audio/audio_stretch.c"

static unsigned failures, cases;
static uint32_t seed = 1;
static int16_t sample_value(unsigned pattern, unsigned index)
{
   seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
   switch (pattern)
   {
      case 0: return 0;
      case 1: return -32768;
      case 2: return index & 1 ? 32767 : -32768;
      case 3: return index % 17 ? 0 : 32767;
      default: return (int16_t)((int32_t)(seed >> 16) - 32768);
   }
}

static unsigned search_oracle(const audio_stretch_t *s)
{
   float reference_f[512], candidate_f[512];
   int32_t reference_i[512], candidate_i[512];
   unsigned c, f, candidate, selected = 0, distance = 0, best = s->next;
   unsigned begin = s->next > s->radius ? s->next - s->radius : 0;
   unsigned end = s->next + s->radius;
   double energy_f = 0.0, best_f = 0.0;
   uint64_t energy_i = 0;
   int64_t best_i = 0;
   for (c = 0; c < s->channels; c++)
   {
      double ef = 0.0;
      uint64_t ei = 0;
      if (!(s->search_channels & (1u << c))) continue;
      for (f = 0; f < s->hop; f++)
      {
         unsigned at = f * s->channels + c;
         if (s->is_float)
         {
            double v = ((const float*)s->overlap)[at];
            ef += v * v;
         }
         else
         {
            int64_t v = ((const int16_t*)s->overlap)[at];
            ei += (uint64_t)(v * v);
         }
      }
      if (s->is_float ? ef > energy_f : ei > energy_i)
      {
         energy_f = ef; energy_i = ei; selected = c;
      }
   }
   if (s->is_float ? energy_f == 0.0 : energy_i == 0) return best;
   for (f = 0; f < s->hop; f++)
   {
      unsigned at = f * s->channels + selected;
      if (s->is_float) reference_f[f] = ((const float*)s->overlap)[at];
      else reference_i[f] = ((const int16_t*)s->overlap)[at];
   }
   /* Visit the nominal candidate first, then every candidate in order. */
   for (candidate = begin; candidate <= end + 1; candidate++)
   {
      unsigned offset = candidate == begin ? s->next : candidate - 1;
      unsigned d = offset > s->next ? offset - s->next : s->next - offset;
      bool better;
      for (f = 0; f < s->hop; f++)
      {
         unsigned at = ((s->head + offset + f) % s->capacity) * s->channels + selected;
         if (s->is_float) candidate_f[f] = ((const float*)s->ring)[at];
         else candidate_i[f] = ((const int16_t*)s->ring)[at];
      }
      if (s->is_float)
      {
         double score = s->correlation(reference_f, candidate_f, s->hop, energy_f);
         better = candidate == begin || score > best_f || (score == best_f && d < distance);
         if (better) best_f = score;
      }
      else
      {
         int64_t score = wsola_corr_i(reference_i, candidate_i, s->hop);
         better = candidate == begin || score > best_i || (score == best_i && d < distance);
         if (better) best_i = score;
      }
      if (better) { best = offset; distance = d; }
   }
   return best;
}

static void correlation_oracle(void)
{
   int32_t a[513], b[513];
   unsigned pa, pb, i, n, offset, checked = 0, before = failures;
   for (pa = 0; pa < 5; pa++)
      for (pb = 0; pb < 5; pb++)
      {
         for (i = 0; i < 513; i++)
         {
            a[i] = sample_value(pa, i);
            b[i] = sample_value(pb, i);
         }
         for (offset = 0; offset < 2; offset++)
            for (n = 0; n <= 512; n++)
            {
               int64_t expected = wsola_corr_i(a + offset, b + offset, n);
               int64_t actual = astretch_corr_i(a + offset, b + offset, n);
               checked++;
               if (expected != actual)
               {
                  if (failures < 10)
                     printf("correlation mismatch: patterns=%u/%u offset=%u length=%u\n",
                           pa, pb, offset, n);
                  failures++;
               }
            }
      }
   printf("native correlation: %u score cases, %u failures\n", checked, failures - before);
}

int main(void)
{
   static const unsigned rates[] = {8000, 11025, 32000, 44100, 48000, 96000, 192000};
   static const unsigned channels[] = {1, 2, 6, 8, 11};
   unsigned r, ch, lane, head, next, pattern, f, c;
   correlation_oracle();
   for (r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
      for (ch = 0; ch < sizeof(channels) / sizeof(channels[0]); ch++)
         for (lane = 0; lane < 2; lane++)
         {
            audio_stretch_t *s = audio_stretch_new(rates[r], channels[ch], lane, 1);
            if (!s) return 2;
            for (head = 0; head < 3; head++)
               for (next = 0; next < 3; next++)
                  for (pattern = 0; pattern < 5; pattern++)
                  {
                     unsigned actual, expected;
                     s->head = head ? s->capacity - head : 0;
                     s->next = next * s->radius / 2;
                     s->search_channels = pattern % 2 ? 1u << (s->channels - 1)
                        : ((1u << s->channels) - 1) & ~(s->channels > 3 ? 8u : 0u);
                     for (f = 0; f < s->capacity + s->hop; f++)
                        for (c = 0; c < s->channels; c++)
                        {
                           int16_t v = sample_value(pattern, f + c);
                           void *buffer = f < s->capacity ? s->ring : s->overlap;
                           unsigned at = (f < s->capacity ? f : f - s->capacity) * s->channels + c;
                           if (lane) ((float*)buffer)[at] = v / 32768.0f;
                           else ((int16_t*)buffer)[at] = v;
                        }
                     expected = search_oracle(s);
                     actual = astretch_search(s);
                     cases++;
                     if (actual != expected)
                     {
                        if (failures < 10) printf("search mismatch: rate=%u channels=%u float=%u pattern=%u head=%u next=%u expected=%u got=%u\n",
                              rates[r], s->channels, lane, pattern, s->head, s->next, expected, actual);
                        failures++;
                     }
                  }
            audio_stretch_free(s);
         }
   printf("search oracle: %u cases, %u failures\n", cases, failures);
   return failures ? 1 : 0;
}
