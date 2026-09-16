/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <audio/wsola_search.h>
#include "audio_stretch.h"

struct audio_stretch
{
   unsigned channels, hop, radius, capacity, head, count, next;
   unsigned pending, read, tail_end, tail_read;
   uint32_t fraction, search_channels;
   bool is_float, started, draining, source_gap, gap_reported;
   size_t bytes, frame_bytes;
   void *ring, *overlap, *output, *reference, *search, *window;
   wsola_corr_func_t correlation;
};

static unsigned astretch_index(const audio_stretch_t *s, unsigned offset)
{
   return (s->head + offset) % s->capacity;
}

static void astretch_copy(const audio_stretch_t *s, void *dst,
      unsigned offset, unsigned frames)
{
   unsigned index = astretch_index(s, offset);
   unsigned first = s->capacity - index;
   if (first > frames) first = frames;
   memcpy(dst, (const char*)s->ring + index * s->frame_bytes,
         first * s->frame_bytes);
   if (frames > first)
      memcpy((char*)dst + first * s->frame_bytes, s->ring,
            (frames - first) * s->frame_bytes);
}

/* These gathered values are native int16, unlike the shared helper's
 * wider stereo sums. Keep all accumulation and normalization exact. */
static INLINE int64_t astretch_corr_i(const int32_t *a, const int32_t *b,
      unsigned n)
{
#if WSOLA_HAVE_SSE2 && !defined(AUDIO_STRETCH_SCALAR)
   unsigned i = 0;
   int64_t dots[2], dot;
   uint64_t energies[2], energy, root;
   __m128i zero = _mm_setzero_si128();
   __m128i overflow = _mm_set1_epi32(INT32_MIN);
   __m128i sum = zero, squares = zero;
   for (; i + 8 <= n; i += 8)
   {
      __m128i x = _mm_packs_epi32(
            _mm_loadu_si128((const __m128i*)(a + i)),
            _mm_loadu_si128((const __m128i*)(a + i + 4)));
      __m128i y = _mm_packs_epi32(
            _mm_loadu_si128((const __m128i*)(b + i)),
            _mm_loadu_si128((const __m128i*)(b + i + 4)));
      __m128i d = _mm_madd_epi16(x, y);
      __m128i e = _mm_madd_epi16(y, y);
      /* Two (-32768 * -32768) products yield +2^31. This is the only
       * overflowing pair; its INT32_MIN encoding must zero-extend. */
      __m128i sign = _mm_andnot_si128(_mm_cmpeq_epi32(d, overflow),
            _mm_srai_epi32(d, 31));
      sum = _mm_add_epi64(sum, _mm_unpacklo_epi32(d, sign));
      sum = _mm_add_epi64(sum, _mm_unpackhi_epi32(d, sign));
      squares = _mm_add_epi64(squares, _mm_unpacklo_epi32(e, zero));
      squares = _mm_add_epi64(squares, _mm_unpackhi_epi32(e, zero));
   }
   _mm_storeu_si128((__m128i*)dots, sum);
   _mm_storeu_si128((__m128i*)energies, squares);
   dot = dots[0] + dots[1];
   energy = energies[0] + energies[1];
   for (; i < n; i++)
   {
      dot += (int64_t)a[i] * b[i];
      energy += (uint64_t)((int64_t)b[i] * b[i]);
   }
   root = wsola_isqrt64(energy);
   return (dot * 65536) / (int64_t)(root ? root : 1);
#else
   return wsola_corr_i(a, b, n);
#endif
}

static unsigned astretch_search(audio_stretch_t *s)
{
   unsigned c, f, selected = 0, begin, end, candidate, best, distance;
   unsigned index;
   double energy_f = 0.0, score_f = 0.0;
   uint64_t energy_i = 0;
   int64_t score_i = 0;
   begin = s->next > s->radius ? s->next - s->radius : 0;
   end = s->next + s->radius;
   best = s->next;
   distance = 0;
   for (c = 0; c < s->channels; c++)
   {
      if (!(s->search_channels & (1u << c))) continue;
      if (s->is_float)
      {
         double energy = 0.0;
         const float *p = (const float*)s->overlap + c;
         for (f = 0; f < s->hop; f++)
         {
            double v = p[f * s->channels];
            energy += v * v;
         }
         if (energy > energy_f) { energy_f = energy; selected = c; }
      }
      else
      {
         uint64_t energy = 0;
         const int16_t *p = (const int16_t*)s->overlap + c;
         for (f = 0; f < s->hop; f++)
         {
            int64_t v = p[f * s->channels];
            energy += (uint64_t)(v * v);
         }
         if (energy > energy_i) { energy_i = energy; selected = c; }
      }
   }
   if (s->is_float ? energy_f == 0.0 : energy_i == 0)
      return best;
   index = astretch_index(s, begin);
   for (f = 0; f < end - begin + s->hop; f++)
   {
      if (s->is_float)
         ((float*)s->search)[f] = ((const float*)s->ring)[index * s->channels + selected];
      else
         ((int32_t*)s->search)[f] = ((const int16_t*)s->ring)[index * s->channels + selected];
      if (++index == s->capacity) index = 0;
   }
   for (f = 0; f < s->hop; f++)
   {
      if (s->is_float)
         ((float*)s->reference)[f] = ((const float*)s->overlap)[f * s->channels + selected];
      else
         ((int32_t*)s->reference)[f] = ((const int16_t*)s->overlap)[f * s->channels + selected];
   }
   /* Seed at the nominal position so silence/equal scores cannot drift. */
   if (s->is_float)
      score_f = s->correlation((const float*)s->reference,
            (const float*)s->search + best - begin, s->hop, energy_f);
   else
      score_i = astretch_corr_i((const int32_t*)s->reference,
            (const int32_t*)s->search + best - begin, s->hop);
   for (candidate = begin; candidate <= end; candidate++)
   {
      unsigned d = candidate > s->next ? candidate - s->next : s->next - candidate;
      bool better;
      if (s->is_float)
      {
         double score = s->correlation((const float*)s->reference,
               (const float*)s->search + candidate - begin, s->hop, energy_f);
         better = score > score_f || (score == score_f && d < distance);
         if (better) score_f = score;
      }
      else
      {
         int64_t score = astretch_corr_i((const int32_t*)s->reference,
               (const int32_t*)s->search + candidate - begin, s->hop);
         better = score > score_i || (score == score_i && d < distance);
         if (better) score_i = score;
      }
      if (better) { best = candidate; distance = d; }
   }
   return best;
}

static void astretch_synthesize(audio_stretch_t *s, void *dst, unsigned start)
{
   unsigned f, c;
   unsigned a = astretch_index(s, start);
   unsigned b = astretch_index(s, start + s->hop);
   for (f = 0; f < s->hop; f++)
   {
      if (s->is_float)
      {
         float w = ((const float*)s->window)[f];
         for (c = 0; c < s->channels; c++)
         {
            unsigned o = f * s->channels + c;
            float previous = ((float*)s->overlap)[o];
            float current = ((const float*)s->ring)[a * s->channels + c];
            ((float*)dst)[o] = previous * (1.0f - w) + current * w;
            ((float*)s->overlap)[o] = ((const float*)s->ring)[b * s->channels + c];
         }
      }
      else
      {
         unsigned w = ((const uint16_t*)s->window)[f];
         for (c = 0; c < s->channels; c++)
         {
            unsigned o = f * s->channels + c;
            int64_t sum = (int64_t)((int16_t*)s->overlap)[o] * (32768 - w)
               + (int64_t)((const int16_t*)s->ring)[a * s->channels + c] * w;
            /* Convex Q15 mix; nearest, half away from zero, no clipping. */
            ((int16_t*)dst)[o] = (int16_t)(sum < 0
                  ? -((-sum + 16384) / 32768) : (sum + 16384) / 32768);
            ((int16_t*)s->overlap)[o] = ((const int16_t*)s->ring)[b * s->channels + c];
         }
      }
      if (++a == s->capacity) a = 0;
      if (++b == s->capacity) b = 0;
   }
}

audio_stretch_t *audio_stretch_new(unsigned rate, unsigned channels,
      bool is_float, uint32_t search_channels)
{
   audio_stretch_t *s;
   unsigned hop, radius, capacity, f;
   size_t sample, native_bytes, bytes;
   char *p;
   if (rate < 8000 || rate > 192000 || !channels
         || channels > AUDIO_STRETCH_MAX_CHANNELS
         || !search_channels || (search_channels >> channels)) return NULL;
   hop = (rate + 187) / 375;
   radius = hop * 4;
   capacity = 2 * hop + 2 * radius;
   sample = is_float ? sizeof(float) : sizeof(int16_t);
   native_bytes = (capacity + 2 * hop) * channels * sample;
   /* Round the native region up for float/int32 search alignment. */
   native_bytes = (native_bytes + 3) & ~(size_t)3;
   bytes = sizeof(*s) + native_bytes + (2 * hop + 2 * radius) * sizeof(int32_t)
      + hop * sample;
   s = (audio_stretch_t*)calloc(1, bytes);
   if (!s) return NULL;
   s->channels = channels; s->hop = hop; s->radius = radius;
   s->capacity = capacity; s->is_float = is_float;
   s->search_channels = search_channels;
   s->bytes = bytes; s->frame_bytes = channels * sample;
   p = (char*)(s + 1);
   s->ring = p;
   s->overlap = p + capacity * s->frame_bytes;
   s->output = p + (capacity + hop) * s->frame_bytes;
   s->reference = p + native_bytes;
   s->search = (char*)s->reference + hop * sizeof(int32_t);
   s->window = (char*)s->search + (hop + 2 * radius) * sizeof(int32_t);
   s->correlation = wsola_corr_get(WSOLA_SIMD_SCALAR);
#if !defined(AUDIO_STRETCH_SCALAR)
#if WSOLA_HAVE_SSE2
   s->correlation = wsola_corr_get(WSOLA_SIMD_SSE2);
#elif WSOLA_HAVE_NEON
   s->correlation = wsola_corr_get(WSOLA_SIMD_NEON);
#endif
#endif
   for (f = 0; f < hop; f++)
   {
      if (is_float) ((float*)s->window)[f] = (float)f / hop;
      else ((uint16_t*)s->window)[f] = (uint16_t)((f * 32768u) / hop);
   }
   return s;
}

void audio_stretch_free(audio_stretch_t *s) { free(s); }
size_t audio_stretch_storage(const audio_stretch_t *s) { return s ? s->bytes : 0; }
unsigned audio_stretch_hop(const audio_stretch_t *s) { return s ? s->hop : 0; }
void audio_stretch_reset(audio_stretch_t *s)
{
   if (!s) return;
   s->head = s->count = s->next = s->pending = s->read = 0;
   s->fraction = 0;
   s->tail_end = s->tail_read = 0;
   s->started = s->draining = s->source_gap = s->gap_reported = false;
}

bool audio_stretch_process(audio_stretch_t *s, struct audio_stretch_io *io,
      double tempo)
{
   uint64_t bits, step;
   if (!io) return false;
   io->input_used = io->output_frames = 0;
   memcpy(&bits, &tempo, sizeof(bits));
   if (!s || s->draining || bits >= UINT64_C(0x7ff0000000000000) || tempo < 0.25 || tempo > 32.0
         || (!io->input && io->input_frames) || (!io->output && io->output_capacity)
         || io->input_frames > (size_t)-1 / s->frame_bytes
         || io->output_capacity > (size_t)-1 / s->frame_bytes) return false;
   step = (uint64_t)(tempo * s->hop * 4294967296.0 + 0.5);
   while (io->output_frames < io->output_capacity)
   {
      size_t remaining = io->output_capacity - io->output_frames;
      void *dst = (char*)io->output + io->output_frames * s->frame_bytes;
      unsigned needed, start;
      if (s->pending)
      {
         size_t n = s->pending < remaining ? s->pending : remaining;
         memcpy(dst, (const char*)s->output + s->read * s->frame_bytes, n * s->frame_bytes);
         s->pending -= (unsigned)n; s->read += (unsigned)n;
         io->output_frames += n;
         continue;
      }
      if (s->started && s->next > s->radius)
      {
         unsigned drop = s->next - s->radius;
         size_t skip;
         if (drop > s->count) drop = s->count;
         if (drop > s->tail_end) s->source_gap = true;
         s->tail_end = drop < s->tail_end ? s->tail_end - drop : 0;
         s->head = astretch_index(s, drop);
         s->count -= drop; s->next -= drop;
         skip = s->next - s->radius;
         if (skip > io->input_frames - io->input_used) skip = io->input_frames - io->input_used;
         if (skip) s->source_gap = true;
         s->next -= (unsigned)skip; io->input_used += skip;
         if (s->next > s->radius) break;
      }
      needed = (s->started ? s->next + s->radius : 0) + 2 * s->hop;
      while (s->count < needed && io->input_used < io->input_frames)
      {
         unsigned index = astretch_index(s, s->count);
         size_t n = needed - s->count;
         if (n > s->capacity - index) n = s->capacity - index;
         if (n > io->input_frames - io->input_used) n = io->input_frames - io->input_used;
         memcpy((char*)s->ring + index * s->frame_bytes,
               (const char*)io->input + io->input_used * s->frame_bytes, n * s->frame_bytes);
         s->count += (unsigned)n; io->input_used += n;
      }
      if (s->count < needed) break;
      start = s->started ? astretch_search(s) : 0;
      if (remaining < s->hop) dst = s->output;
      if (s->started) astretch_synthesize(s, dst, start);
      else
      {
         astretch_copy(s, dst, 0, s->hop);
         astretch_copy(s, s->overlap, s->hop, s->hop);
         s->started = true;
      }
      s->tail_end = start + 2 * s->hop;
      s->source_gap = false;
      if (remaining < s->hop) { s->pending = s->hop; s->read = 0; }
      else io->output_frames += s->hop;
      {
         uint64_t advance = step + s->fraction;
         s->next += (unsigned)(advance >> 32);
         s->fraction = (uint32_t)advance;
      }
   }
   return true;
}

bool audio_stretch_drain(audio_stretch_t *s, struct audio_stretch_drain_io *io)
{
   size_t n, remaining;
   char *dst;
   if (!io) return false;
   io->output_frames = 0;
   io->gap_offset = (size_t)-1;
   io->complete = false;
   if (!s || (!io->output && io->output_capacity)
         || io->output_capacity > (size_t)-1 / s->frame_bytes) return false;
   if (!io->output_capacity)
   {
      io->complete = !s->pending && (!s->started || s->tail_read == s->hop)
         && s->count == s->tail_end;
      return true;
   }
   if (!s->draining)
   {
      s->draining = true;
      s->head = astretch_index(s, s->tail_end);
      s->count -= s->tail_end;
      s->tail_end = 0;
   }
   dst = (char*)io->output;
   remaining = io->output_capacity;
   n = s->pending < remaining ? s->pending : remaining;
   if (n)
   {
      memcpy(dst, (const char*)s->output + s->read * s->frame_bytes, n * s->frame_bytes);
      s->pending -= (unsigned)n; s->read += (unsigned)n;
      io->output_frames += n; remaining -= n; dst += n * s->frame_bytes;
   }
   if (s->started && remaining)
   {
      n = s->hop - s->tail_read;
      if (n > remaining) n = remaining;
      if (n)
      {
         memcpy(dst, (const char*)s->overlap + s->tail_read * s->frame_bytes, n * s->frame_bytes);
         s->tail_read += (unsigned)n;
         io->output_frames += n; remaining -= n; dst += n * s->frame_bytes;
      }
   }
   if (!s->pending && (!s->started || s->tail_read == s->hop))
   {
      if (s->source_gap && !s->gap_reported)
      {
         io->gap_offset = io->output_frames;
         s->gap_reported = true;
      }
      n = s->count < remaining ? s->count : remaining;
      if (n)
      {
         astretch_copy(s, dst, 0, (unsigned)n);
         s->head = astretch_index(s, (unsigned)n);
         s->count -= (unsigned)n;
         io->output_frames += n;
      }
   }
   io->complete = !s->pending && (!s->started || s->tail_read == s->hop) && !s->count;
   return true;
}

bool audio_stretch_crossfade(void *output, const void *outgoing,
      const void *incoming, size_t frames, unsigned channels, bool is_float,
      unsigned offset, unsigned total)
{
   unsigned denominator, weight, step, carry, remainder, c;
   size_t f, sample;
   if (!channels || channels > AUDIO_STRETCH_MAX_CHANNELS
         || !total || total > 65536
         || offset > total || frames > total - offset
         || (frames && (!output || !outgoing || !incoming))) return false;
   if (!frames) return true;
   denominator = total > 1 ? total - 1 : 1;
   weight = total > 1 ? (offset * UINT32_C(65536)) / denominator : 65536;
   remainder = total > 1 ? (offset * UINT32_C(65536)) % denominator : 0;
   step = 65536 / denominator;
   carry = 65536 % denominator;
   sample = 0;
   for (f = 0; f < frames; f++)
   {
      if (!weight || weight == 65536)
      {
         size_t bytes = channels * (is_float ? sizeof(float) : sizeof(int16_t));
         const char *src = (const char*)(weight ? incoming : outgoing)
            + f * bytes;
         char *dst = (char*)output + f * bytes;
         if (dst != src) memcpy(dst, src, bytes);
         sample += channels;
      }
      else if (is_float)
      {
         float b = (float)weight * (1.0f / 65536.0f);
         float a = 1.0f - b;
         for (c = 0; c < channels; c++, sample++)
            ((float*)output)[sample] = ((const float*)outgoing)[sample] * a
               + ((const float*)incoming)[sample] * b;
      }
      else
         for (c = 0; c < channels; c++, sample++)
         {
            int64_t value = (int64_t)((const int16_t*)outgoing)[sample] * (65536 - weight)
               + (int64_t)((const int16_t*)incoming)[sample] * weight;
            ((int16_t*)output)[sample] = (int16_t)(value < 0
                  ? -((-value + 32768) / 65536) : (value + 32768) / 65536);
         }
      weight += step;
      remainder += carry;
      if (remainder >= denominator) { remainder -= denominator; weight++; }
   }
   return true;
}

struct audio_stretch_transition
{
   size_t frame_bytes;
   unsigned channels, capacity, head, count, fade_total, fade_offset;
   bool is_float, flushing;
};

audio_stretch_transition_t *audio_stretch_transition_new(unsigned channels,
      bool is_float, unsigned tail_frames)
{
   audio_stretch_transition_t *s;
   size_t frame;
   if (!channels || channels > AUDIO_STRETCH_MAX_CHANNELS
         || !tail_frames || tail_frames > 65536)
      return NULL;
   frame = channels * (is_float ? sizeof(float) : sizeof(int16_t));
   s = (audio_stretch_transition_t*)calloc(1, sizeof(*s) + tail_frames * frame);
   if (!s) return NULL;
   s->channels = channels; s->capacity = tail_frames;
   s->frame_bytes = frame; s->is_float = is_float;
   return s;
}

void audio_stretch_transition_free(audio_stretch_transition_t *s) { free(s); }

void audio_stretch_transition_reset(audio_stretch_transition_t *s)
{
   if (!s) return;
   s->head = s->count = s->fade_total = s->fade_offset = 0;
   s->flushing = false;
}

bool audio_stretch_transition_boundary(audio_stretch_transition_t *s)
{
   if (!s || s->flushing || s->fade_total) return false;
   s->fade_total = s->count;
   s->fade_offset = 0;
   return true;
}

/* Pop old frames, or append new frames, using at most two native copies. */
static void astretch_transition_copy(audio_stretch_transition_t *s,
      void *buffer, unsigned frames, bool append)
{
   unsigned index = append ? (s->head + s->count) % s->capacity : s->head;
   unsigned first = s->capacity - index;
   char *ring = (char*)(s + 1);
   if (first > frames) first = frames;
   if (append)
   {
      memcpy(ring + index * s->frame_bytes, buffer, first * s->frame_bytes);
      if (frames > first)
         memcpy(ring, (char*)buffer + first * s->frame_bytes,
               (frames - first) * s->frame_bytes);
      s->count += frames;
   }
   else
   {
      memcpy(buffer, ring + index * s->frame_bytes, first * s->frame_bytes);
      if (frames > first)
         memcpy((char*)buffer + first * s->frame_bytes, ring,
               (frames - first) * s->frame_bytes);
      s->count -= frames;
      s->head = (s->head + frames) % s->capacity;
   }
}

bool audio_stretch_transition_process(audio_stretch_transition_t *s,
      struct audio_stretch_io *io)
{
   size_t n, old, direct, available, space;
   const char *src;
   char *dst;
   if (!io) return false;
   io->input_used = io->output_frames = 0;
   if (!s || s->flushing || (!io->input && io->input_frames)
         || (!io->output && io->output_capacity)
         || io->input_frames > (size_t)-1 / s->frame_bytes
         || io->output_capacity > (size_t)-1 / s->frame_bytes) return false;
   if (!io->output_capacity || !io->input_frames) return true;
   src = (const char*)io->input; dst = (char*)io->output;
   available = io->input_frames; space = io->output_capacity;
   while (s->fade_total && available && space)
   {
      n = s->capacity - s->head;
      if (n > s->count) n = s->count;
      if (n > available) n = available;
      if (n > space) n = space;
      audio_stretch_crossfade(dst, (char*)(s + 1) + s->head * s->frame_bytes,
            src, n, s->channels, s->is_float, s->fade_offset, s->fade_total);
      s->head = (s->head + (unsigned)n) % s->capacity;
      s->count -= (unsigned)n; s->fade_offset += (unsigned)n;
      if (!s->count) s->fade_total = s->fade_offset = 0;
      src += n * s->frame_bytes; dst += n * s->frame_bytes;
      available -= n; space -= n;
      io->input_used += n; io->output_frames += n;
   }
   if (!available || !space) return true;
   n = s->capacity - s->count;
   n = available > n ? available - n : 0;
   if (n > space) n = space;
   old = n < s->count ? n : s->count;
   if (old) astretch_transition_copy(s, dst, (unsigned)old, false);
   direct = n - old;
   if (direct)
   {
      memcpy(dst + old * s->frame_bytes, src, direct * s->frame_bytes);
      src += direct * s->frame_bytes;
      available -= direct; io->input_used += direct;
   }
   io->output_frames += n;
   n = s->capacity - s->count;
   if (n > available) n = available;
   if (n) astretch_transition_copy(s, (void*)src, (unsigned)n, true);
   io->input_used += n;
   return true;
}

bool audio_stretch_transition_flush(audio_stretch_transition_t *s,
      struct audio_stretch_drain_io *io)
{
   size_t n;
   if (!io) return false;
   io->output_frames = 0; io->gap_offset = (size_t)-1; io->complete = false;
   if (!s || (!io->output && io->output_capacity)
         || io->output_capacity > (size_t)-1 / s->frame_bytes) return false;
   if (io->output_capacity)
   {
      s->flushing = true;
      if (s->fade_offset) s->count = 0;
      s->fade_total = s->fade_offset = 0;
      n = s->count < io->output_capacity ? s->count : io->output_capacity;
      if (n) astretch_transition_copy(s, io->output, (unsigned)n, false);
      io->output_frames = n;
   }
   io->complete = !s->count || s->fade_offset != 0;
   return true;
}

enum astretch_stream_phase
{
   ASTRETCH_STREAM_RAW, ASTRETCH_STREAM_ACTIVE, ASTRETCH_STREAM_EXIT,
   ASTRETCH_STREAM_JOIN, ASTRETCH_STREAM_FLUSH, ASTRETCH_STREAM_DONE
};

struct audio_stretch_stream
{
   audio_stretch_t *engine;
   audio_stretch_transition_t *transition;
   size_t frame_bytes, count, read, gap;
   void *bound_output;
   const void *bound_source;
   size_t bound_capacity, bound_count, bound_read;
   unsigned hop;
   enum astretch_stream_phase phase;
   bool eof;
};

void audio_stretch_stream_free(audio_stretch_stream_t *s)
{
   if (!s) return;
   audio_stretch_free(s->engine);
   audio_stretch_transition_free(s->transition);
   free(s);
}

audio_stretch_stream_t *audio_stretch_stream_new(unsigned rate, unsigned channels,
      bool is_float, uint32_t search_channels)
{
   audio_stretch_stream_t *s;
   unsigned hop;
   size_t frame;
   if (rate < 8000 || rate > 192000 || !channels
         || channels > AUDIO_STRETCH_MAX_CHANNELS
         || !search_channels || (search_channels >> channels)) return NULL;
   hop = (rate + 187) / 375;
   frame = channels * (is_float ? sizeof(float) : sizeof(int16_t));
   s = (audio_stretch_stream_t*)calloc(1, sizeof(*s) + hop * frame);
   if (!s) return NULL;
   s->engine = audio_stretch_new(rate, channels, is_float, search_channels);
   s->transition = audio_stretch_transition_new(channels, is_float, hop);
   if (!s->engine || !s->transition) { audio_stretch_stream_free(s); return NULL; }
   s->hop = hop; s->frame_bytes = frame; s->gap = (size_t)-1;
   return s;
}

void audio_stretch_stream_reset(audio_stretch_stream_t *s)
{
   if (!s) return;
   audio_stretch_reset(s->engine);
   audio_stretch_transition_reset(s->transition);
   s->count = s->read = 0; s->gap = (size_t)-1;
   s->phase = ASTRETCH_STREAM_RAW; s->eof = false;
   s->bound_count = s->bound_read = 0;
   s->bound_source = NULL;
}

bool audio_stretch_stream_quiescent(const audio_stretch_stream_t *s)
{
   return !s || (s->phase == ASTRETCH_STREAM_RAW && !s->bound_count
         && !s->count && !s->eof);
}

bool audio_stretch_stream_needs_input(const audio_stretch_stream_t *s)
{
   const audio_stretch_t *engine;
   unsigned needed;
   if (!s || s->eof || s->bound_read < s->bound_count || s->read < s->count)
      return false;
   if (s->phase == ASTRETCH_STREAM_RAW) return true;
   if (s->phase != ASTRETCH_STREAM_ACTIVE) return false;
   engine = s->engine;
   if (engine->pending || engine->draining) return false;
   /* Dropping an exhausted search prefix reduces count and next equally. */
   needed = (engine->started ? engine->next + engine->radius : 0)
      + 2 * engine->hop;
   return engine->count < needed;
}

static void astretch_stream_pump(audio_stretch_stream_t *s,
      struct audio_stretch_io *io, double tempo, bool active)
{
   while (io->output_frames < io->output_capacity)
   {
      struct audio_stretch_io part;
      struct audio_stretch_drain_io drain;
      size_t n;
      part.input = io->input ? (const char*)io->input + io->input_used * s->frame_bytes : NULL;
      part.input_frames = io->input_frames - io->input_used;
      part.output = (char*)io->output + io->output_frames * s->frame_bytes;
      part.output_capacity = io->output_capacity - io->output_frames;
      if (s->gap == s->read)
      {
         /* A drain reports at most one boundary after all active output. */
         audio_stretch_transition_boundary(s->transition);
         s->gap = (size_t)-1;
      }
      if (s->read < s->count)
      {
         n = s->count - s->read;
         if (s->gap != (size_t)-1 && n > s->gap - s->read) n = s->gap - s->read;
         part.input = (char*)(s + 1) + s->read * s->frame_bytes;
         part.input_frames = n;
         audio_stretch_transition_process(s->transition, &part);
         s->read += part.input_used; io->output_frames += part.output_frames;
         continue;
      }
      s->read = s->count = 0;
      if (s->phase == ASTRETCH_STREAM_ACTIVE && (!active || s->eof))
         s->phase = ASTRETCH_STREAM_EXIT;
      if (s->phase == ASTRETCH_STREAM_RAW)
      {
         if (s->eof) { s->phase = ASTRETCH_STREAM_DONE; break; }
         if (active) { s->phase = ASTRETCH_STREAM_ACTIVE; continue; }
         n = part.input_frames < part.output_capacity ? part.input_frames : part.output_capacity;
         if (n) memcpy(part.output, part.input, n * s->frame_bytes);
         io->input_used += n; io->output_frames += n;
         break;
      }
      if (s->phase == ASTRETCH_STREAM_ACTIVE)
      {
         audio_stretch_process(s->engine, &part, tempo);
         io->input_used += part.input_used;
         io->output_frames += part.output_frames;
         break;
      }
      else if (s->phase == ASTRETCH_STREAM_EXIT)
      {
         drain.output = s + 1; drain.output_capacity = s->hop;
         audio_stretch_drain(s->engine, &drain);
         s->count = drain.output_frames; s->gap = drain.gap_offset;
         if (drain.complete) s->phase = ASTRETCH_STREAM_JOIN;
      }
      else if (s->phase == ASTRETCH_STREAM_JOIN)
      {
         if (s->transition->fade_total && !s->eof)
         {
            if (part.input_frames > s->transition->count)
               part.input_frames = s->transition->count;
            if (!part.input_frames) break;
            audio_stretch_transition_process(s->transition, &part);
            io->input_used += part.input_used; io->output_frames += part.output_frames;
         }
         else s->phase = ASTRETCH_STREAM_FLUSH;
      }
      else if (s->phase == ASTRETCH_STREAM_FLUSH)
      {
         drain.output = part.output; drain.output_capacity = part.output_capacity;
         audio_stretch_transition_flush(s->transition, &drain);
         io->output_frames += drain.output_frames;
         if (drain.complete)
         {
            audio_stretch_reset(s->engine);
            audio_stretch_transition_reset(s->transition);
            s->phase = s->eof ? ASTRETCH_STREAM_DONE : ASTRETCH_STREAM_RAW;
         }
      }
      else break;
   }
}

static bool astretch_stream_process(audio_stretch_stream_t *s,
      struct audio_stretch_io *io, double tempo, bool active)
{
   uint64_t bits;
   if (!io) return false;
   io->input_used = io->output_frames = 0;
   memcpy(&bits, &tempo, sizeof(bits));
   if (!s || s->eof || bits >= UINT64_C(0x7ff0000000000000)
         || tempo < 0.25 || tempo > 32.0
         || (!io->input && io->input_frames) || (!io->output && io->output_capacity)
         || io->input_frames > (size_t)-1 / s->frame_bytes
         || io->output_capacity > (size_t)-1 / s->frame_bytes) return false;
   if (io->output_capacity && !active && s->phase == ASTRETCH_STREAM_ACTIVE)
      s->phase = ASTRETCH_STREAM_EXIT;
   astretch_stream_pump(s, io, tempo, active);
   return true;
}

static bool astretch_stream_flush(audio_stretch_stream_t *s,
      struct audio_stretch_drain_io *io)
{
   struct audio_stretch_io part;
   if (!io) return false;
   io->output_frames = 0; io->gap_offset = (size_t)-1; io->complete = false;
   if (!s || (!io->output && io->output_capacity)
         || io->output_capacity > (size_t)-1 / s->frame_bytes) return false;
   if (io->output_capacity)
   {
      s->eof = true;
      memset(&part, 0, sizeof(part));
      part.output = io->output; part.output_capacity = io->output_capacity;
      astretch_stream_pump(s, &part, 1.0, false);
      io->output_frames = part.output_frames;
   }
   io->complete = s->phase == ASTRETCH_STREAM_DONE;
   return true;
}

bool audio_stretch_stream_process(audio_stretch_stream_t *s,
      struct audio_stretch_io *io, double tempo, bool active)
{
   if (s && s->bound_output)
   {
      if (io) io->input_used = io->output_frames = 0;
      return false;
   }
   return astretch_stream_process(s, io, tempo, active);
}

bool audio_stretch_stream_flush(audio_stretch_stream_t *s,
      struct audio_stretch_drain_io *io)
{
   if (s && s->bound_output)
   {
      if (io)
      {
         io->output_frames = 0; io->gap_offset = (size_t)-1; io->complete = false;
      }
      return false;
   }
   return astretch_stream_flush(s, io);
}

bool audio_stretch_stream_bind(audio_stretch_stream_t *s,
      void *output, size_t capacity)
{
   if (!s || s->phase != ASTRETCH_STREAM_RAW || s->eof || s->bound_count
         || (!!output != !!capacity)
         || capacity > (size_t)-1 / s->frame_bytes) return false;
   s->bound_output = output; s->bound_capacity = capacity;
   s->bound_read = 0;
   return true;
}

const void *audio_stretch_stream_peek(const audio_stretch_stream_t *s,
      size_t *frames)
{
   if (!frames) return NULL;
   *frames = s ? s->bound_count - s->bound_read : 0;
   return *frames ? (const char*)(s->bound_source ? s->bound_source : s->bound_output)
      + s->bound_read * s->frame_bytes : NULL;
}

bool audio_stretch_stream_consume(audio_stretch_stream_t *s, size_t frames)
{
   if (!s || !s->bound_output || frames > s->bound_count - s->bound_read) return false;
   s->bound_read += frames;
   if (s->bound_read == s->bound_count)
   {
      s->bound_read = s->bound_count = 0;
      s->bound_source = NULL;
   }
   return true;
}

bool audio_stretch_stream_push_limit(audio_stretch_stream_t *s,
      const void *input, size_t frames, size_t *used, double tempo, bool active,
      size_t limit)
{
   struct audio_stretch_io io;
   if (!used) return false;
   *used = 0;
   if (!s || !s->bound_output) return false;
   io.input = input; io.input_frames = frames; io.output = s->bound_output;
   io.output_capacity = s->bound_count ? 0
      : (limit < s->bound_capacity ? limit : s->bound_capacity);
   if (!astretch_stream_process(s, &io, tempo, active)) return false;
   /* A pending output block must not cancel a requested exit. */
   if (limit && !active && s->phase == ASTRETCH_STREAM_ACTIVE)
      s->phase = ASTRETCH_STREAM_EXIT;
   *used = io.input_used;
   if (!s->bound_count) s->bound_count = io.output_frames;
   return true;
}

bool audio_stretch_stream_push(audio_stretch_stream_t *s,
      const void *input, size_t frames, size_t *used, double tempo, bool active)
{
   return audio_stretch_stream_push_limit(s, input, frames, used, tempo, active,
         (size_t)-1);
}

bool audio_stretch_stream_push_view_limit(audio_stretch_stream_t *s,
      const void *input, size_t frames, size_t *used, double tempo, bool active,
      size_t limit)
{
   struct audio_stretch_io io;
   if (active || !audio_stretch_stream_quiescent(s) || !limit)
      return audio_stretch_stream_push_limit(s, input, frames, used,
            tempo, active, limit);
   if (!used) return false;
   *used = 0;
   if (!s || !s->bound_output) return false;
   /* Validate without copying or changing transport state. */
   io.input = input; io.input_frames = frames;
   io.output = s->bound_output; io.output_capacity = 0;
   if (!astretch_stream_process(s, &io, tempo, active)) return false;
   if (frames > limit) frames = limit;
   if (frames > s->bound_capacity) frames = s->bound_capacity;
   s->bound_source = frames ? input : NULL;
   s->bound_count = frames;
   *used = frames;
   return true;
}

bool audio_stretch_stream_finish_limit(audio_stretch_stream_t *s,
      bool *complete, size_t limit)
{
   struct audio_stretch_drain_io io;
   if (!complete) return false;
   *complete = false;
   if (!s || !s->bound_output) return false;
   if (!limit)
   {
      *complete = s->phase == ASTRETCH_STREAM_DONE && !s->bound_count;
      return true;
   }
   s->eof = true;
   if (s->bound_count) return true;
   io.output = s->bound_output;
   io.output_capacity = limit < s->bound_capacity ? limit : s->bound_capacity;
   astretch_stream_flush(s, &io);
   s->bound_count = io.output_frames;
   *complete = io.complete && !s->bound_count;
   return true;
}

bool audio_stretch_stream_finish(audio_stretch_stream_t *s, bool *complete)
{
   return audio_stretch_stream_finish_limit(s, complete, (size_t)-1);
}
