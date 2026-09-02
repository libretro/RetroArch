/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_time_stretch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Provenance and tuning: see <audio/audio_time_stretch.h>. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <audio/audio_time_stretch.h>

static double audio_stretch_clamp(double v, double lo, double hi)
{
   if (v < lo)
      return lo;
   if (v > hi)
      return hi;
   return v;
}

double audio_time_stretch_ratio(double arrival_per_flush, int output_per_flush,
      int input_fill, int target_fill)
{
   double ratio;

   if (output_per_flush <= 0)
      return 1.0;

   ratio = arrival_per_flush / (double)output_per_flush;

   if (target_fill > 0)
   {
      double err = (input_fill - (double)target_fill) / (double)target_fill;
      err        = audio_stretch_clamp(err, -1.0, 1.0);
      ratio     *= 1.0 + (AUDIO_STRETCH_TRIM_GAIN * err);
   }

   return audio_stretch_clamp(ratio,
         AUDIO_STRETCH_MIN_RATIO, AUDIO_STRETCH_MAX_RATIO);
}

int audio_time_stretch_target_input_fill(double arrival_per_flush)
{
   /* Half the ring: the write bound is space = CAPACITY - (w -
    * analysis_pos), so at this target half the ring is still free for
    * writes to land in - far more than a single call (capped at
    * AUDIO_STRETCH_MAX_WRITE) can ever consume. */
   const double cap = AUDIO_STRETCH_INPUT_CAPACITY / 2.0;
   double need      = (2.0 * arrival_per_flush)
                    + AUDIO_STRETCH_SEARCH_RADIUS
                    + AUDIO_STRETCH_FRAME_SIZE;

   /* Clamped as a double before the cast: a pathological arrival estimate
    * would otherwise be undefined behaviour on the conversion to int. The
    * negated comparison also rejects NaN. */
   if (!(need > AUDIO_STRETCH_MIN_TARGET_FILL))
      need = AUDIO_STRETCH_MIN_TARGET_FILL;
   if (need > cap)
      need = cap;

   return (int)need;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AUDIO_STRETCH_IN_MASK  (AUDIO_STRETCH_INPUT_CAPACITY - 1)
#define AUDIO_STRETCH_OUT_MASK (AUDIO_STRETCH_OUTPUT_CAPACITY - 1)

static int16_t audio_stretch_saturate(float v)
{
   long s = (long)floor(v + 0.5);
   if (s > 32767)
      s = 32767;
   if (s < -32768)
      s = -32768;
   return (int16_t)s;
}

static int64_t audio_stretch_max64(int64_t a, int64_t b)
{
   return a > b ? a : b;
}

bool audio_time_stretch_init(audio_time_stretch_t *ts)
{
   int i;
   memset(ts, 0, sizeof(*ts));

   ts->in_l    = (int16_t*)calloc(AUDIO_STRETCH_INPUT_CAPACITY, sizeof(int16_t));
   ts->in_r    = (int16_t*)calloc(AUDIO_STRETCH_INPUT_CAPACITY, sizeof(int16_t));
   ts->in_mono = (float*)calloc(AUDIO_STRETCH_INPUT_CAPACITY, sizeof(float));
   ts->out_l   = (int16_t*)calloc(AUDIO_STRETCH_OUTPUT_CAPACITY, sizeof(int16_t));
   ts->out_r   = (int16_t*)calloc(AUDIO_STRETCH_OUTPUT_CAPACITY, sizeof(int16_t));
   if (!ts->in_l || !ts->in_r || !ts->in_mono || !ts->out_l || !ts->out_r)
   {
      audio_time_stretch_free(ts);
      return false;
   }

   for (i = 0; i < AUDIO_STRETCH_FRAME_SIZE; i++)
      ts->window[i] = (float)
            (0.5 * (1.0 - cos((2.0 * M_PI * i) / AUDIO_STRETCH_FRAME_SIZE)));
   audio_time_stretch_reset(ts);
   return true;
}

void audio_time_stretch_free(audio_time_stretch_t *ts)
{
   free(ts->in_l);
   free(ts->in_r);
   free(ts->in_mono);
   free(ts->out_l);
   free(ts->out_r);
   ts->in_l    = NULL;
   ts->in_r    = NULL;
   ts->in_mono = NULL;
   ts->out_l   = NULL;
   ts->out_r   = NULL;
}

void audio_time_stretch_reset(audio_time_stretch_t *ts)
{
   ts->write_pos     = 0;
   ts->analysis_pos  = 0;
   ts->natural_pos   = 0;
   ts->out_read_pos  = 0;
   ts->out_write_pos = 0;
   ts->primed        = false;
   memset(ts->acc_l, 0, sizeof(ts->acc_l));
   memset(ts->acc_r, 0, sizeof(ts->acc_r));

   /* The search is already bounded to written frames; this makes any
    * future bound slip degrade to silence rather than noise. */
   memset(ts->in_l, 0, AUDIO_STRETCH_INPUT_CAPACITY * sizeof(int16_t));
   memset(ts->in_r, 0, AUDIO_STRETCH_INPUT_CAPACITY * sizeof(int16_t));
   memset(ts->in_mono, 0, AUDIO_STRETCH_INPUT_CAPACITY * sizeof(float));
}

void audio_time_stretch_resync(audio_time_stretch_t *ts)
{
   /* Unlike a reset this keeps the ring: the frames behind the new position
    * are exactly the lookback the search wants, and re-reading the ones ahead
    * of it would replay audio the caller has already emitted. */
   ts->analysis_pos  = ts->write_pos - AUDIO_STRETCH_FRAME_SIZE;
   if (ts->analysis_pos < 0)
      ts->analysis_pos = 0;
   ts->natural_pos   = ts->analysis_pos;
   ts->primed        = false;
   ts->out_read_pos  = 0;
   ts->out_write_pos = 0;
   memset(ts->acc_l, 0, sizeof(ts->acc_l));
   memset(ts->acc_r, 0, sizeof(ts->acc_r));
}

void audio_time_stretch_idle(audio_time_stretch_t *ts, int reserve)
{
   int64_t pos;
   /* One frame is the least the first synthesis can open on; beyond the
    * capacity the write head would lap the read point. */
   if (reserve < AUDIO_STRETCH_FRAME_SIZE)
      reserve = AUDIO_STRETCH_FRAME_SIZE;
   if (reserve > (AUDIO_STRETCH_INPUT_CAPACITY - AUDIO_STRETCH_MAX_WRITE))
      reserve = AUDIO_STRETCH_INPUT_CAPACITY - AUDIO_STRETCH_MAX_WRITE;
   if ((pos = ts->write_pos - reserve) < 0)
      pos = 0;
   /* Forwards only - see the header. */
   if (pos < ts->analysis_pos)
      pos = ts->analysis_pos;
   ts->analysis_pos  = pos;
   ts->natural_pos   = pos;
   /* The accumulator and the output ring hold the tail of the last hop
    * synthesised, which is now older than the reserve; carrying either into
    * the restart would splice it against the new audio. */
   ts->primed        = false;
   ts->out_read_pos  = 0;
   ts->out_write_pos = 0;
   memset(ts->acc_l, 0, sizeof(ts->acc_l));
   memset(ts->acc_r, 0, sizeof(ts->acc_r));
}

int audio_time_stretch_input_fill(const audio_time_stretch_t *ts)
{
   int64_t write_pos;
   int64_t pending;
   write_pos = ts->write_pos;
   pending   = write_pos - ts->analysis_pos;
   if (pending < 0)
      return 0;
   if (pending > AUDIO_STRETCH_INPUT_CAPACITY)
      return AUDIO_STRETCH_INPUT_CAPACITY;
   return (int)pending;
}

int audio_time_stretch_output_fill(const audio_time_stretch_t *ts)
{
   return (int)(ts->out_write_pos - ts->out_read_pos);
}

int audio_time_stretch_write(audio_time_stretch_t *ts,
      const int16_t *frames, int num_frames)
{
   int64_t w;
   int64_t space;
   int     i;

   w     = ts->write_pos;
   space = AUDIO_STRETCH_INPUT_CAPACITY - (w - ts->analysis_pos);

   if (num_frames > AUDIO_STRETCH_MAX_WRITE)
      num_frames = AUDIO_STRETCH_MAX_WRITE;
   if (space < 0)
      space = 0;
   if (num_frames > space)
      num_frames = (int)space;
   if (num_frames <= 0)
      return 0;

   for (i = 0; i < num_frames; i++)
   {
      int     idx = (int)((w + i) & AUDIO_STRETCH_IN_MASK);
      int16_t l   = frames[(i * 2) + 0];
      int16_t r   = frames[(i * 2) + 1];
      ts->in_l[idx]    = l;
      ts->in_r[idx]    = r;
      ts->in_mono[idx] = 0.5f * ((float)l + (float)r);
   }

   ts->write_pos = w + num_frames;
   return num_frames;
}

static bool audio_stretch_can_synthesise(const audio_time_stretch_t *ts,
      int64_t seen_write)
{
   int64_t frame_end;
   int64_t natural_end;
   if ((AUDIO_STRETCH_OUTPUT_CAPACITY - audio_time_stretch_output_fill(ts))
         < AUDIO_STRETCH_SYNTHESIS_HOP)
      return false;
   /* Only the frame itself and the natural-continuation reference need to be
    * present; the search clamps to whatever else is available. Demanding the
    * full radius here would emit no output at all on a short FIFO. */
   frame_end   = ts->analysis_pos + AUDIO_STRETCH_FRAME_SIZE;
   natural_end = ts->natural_pos + AUDIO_STRETCH_SYNTHESIS_HOP;
   return (seen_write >= frame_end) && (seen_write >= natural_end);
}

static double audio_stretch_energy(const audio_time_stretch_t *ts, int64_t pos)
{
   double e = 0.0;
   int    i;
   for (i = 0; i < AUDIO_STRETCH_SYNTHESIS_HOP; i++)
   {
      double v = ts->in_mono[(int)((pos + i) & AUDIO_STRETCH_IN_MASK)];
      e += v * v;
   }
   return e;
}

/* Normalised so the search doesn't just latch onto the loudest candidate. */
static double audio_stretch_score(const audio_time_stretch_t *ts, int64_t pos,
      double ref_energy)
{
   double dot    = 0.0;
   double energy = 0.0;
   int    i;
   for (i = 0; i < AUDIO_STRETCH_SYNTHESIS_HOP; i++)
   {
      double a = ts->in_mono[(int)((pos + i) & AUDIO_STRETCH_IN_MASK)];
      double b = ts->in_mono[(int)((ts->natural_pos + i) & AUDIO_STRETCH_IN_MASK)];
      dot    += a * b;
      energy += a * a;
   }
   return dot / sqrt((energy * ref_energy) + 1.0e-9);
}

static int64_t audio_stretch_find_best_offset(const audio_time_stretch_t *ts,
      int64_t seen_write)
{
   int64_t oldest     = audio_stretch_max64(0,
         seen_write - AUDIO_STRETCH_INPUT_CAPACITY);
   int64_t latest;
   double  ref_energy;
   double  best_score = -1.0e30;
   int     lowest_k   = -AUDIO_STRETCH_SEARCH_RADIUS;
   int     highest_k  = AUDIO_STRETCH_SEARCH_RADIUS;
   int     best_k;
   int     lo;
   int     hi;
   int     k;

   /* natural_pos trails analysis_pos by up to hop + radius - synthesis_hop,
    * which at a high ratio on a near-full ring can fall off the back. The
    * reference would then be overwritten frames, so search nothing instead. */
   if (ts->natural_pos < oldest)
      return ts->analysis_pos;

   ref_energy = audio_stretch_energy(ts, ts->natural_pos);

   /* Masking a negative position wraps it into frames we never wrote. */
   if ((ts->analysis_pos + lowest_k) < oldest)
      lowest_k = (int)(oldest - ts->analysis_pos);

   /* Clamp to what has arrived, so a short FIFO narrows the search. */
   latest = seen_write - AUDIO_STRETCH_FRAME_SIZE;
   if ((ts->analysis_pos + highest_k) > latest)
      highest_k = (int)(latest - ts->analysis_pos);
   if (highest_k < lowest_k)
      highest_k = lowest_k;

   best_k = lowest_k > 0 ? lowest_k : 0;
   if (best_k > highest_k)
      best_k = highest_k;

   for (k = lowest_k; k <= highest_k; k += AUDIO_STRETCH_COARSE_STRIDE)
   {
      double s = audio_stretch_score(ts, ts->analysis_pos + k, ref_energy);
      if (s > best_score)
      {
         best_score = s;
         best_k     = k;
      }
   }

   lo = best_k - AUDIO_STRETCH_FINE_RADIUS;
   hi = best_k + AUDIO_STRETCH_FINE_RADIUS;
   if (lo < lowest_k)
      lo = lowest_k;
   if (hi > highest_k)
      hi = highest_k;
   for (k = lo; k <= hi; k++)
   {
      double s = audio_stretch_score(ts, ts->analysis_pos + k, ref_energy);
      if (s > best_score)
      {
         best_score = s;
         best_k     = k;
      }
   }

   return ts->analysis_pos + best_k;
}

static void audio_stretch_synthesise_hop(audio_time_stretch_t *ts,
      double ratio, int64_t seen_write)
{
   int     hop = (int)floor(AUDIO_STRETCH_SYNTHESIS_HOP * ratio + 0.5);
   int64_t chosen;
   int     i;

   if (hop < 1)
      hop = 1;

   chosen     = ts->primed ? audio_stretch_find_best_offset(ts, seen_write)
                            : ts->analysis_pos;
   ts->primed = true;

   for (i = 0; i < AUDIO_STRETCH_FRAME_SIZE; i++)
   {
      int   idx = (int)((chosen + i) & AUDIO_STRETCH_IN_MASK);
      float w   = ts->window[i];
      ts->acc_l[i] += w * (float)ts->in_l[idx];
      ts->acc_r[i] += w * (float)ts->in_r[idx];
   }

   for (i = 0; i < AUDIO_STRETCH_SYNTHESIS_HOP; i++)
   {
      int idx = (int)(ts->out_write_pos & AUDIO_STRETCH_OUT_MASK);
      ts->out_l[idx] = audio_stretch_saturate(ts->acc_l[i]);
      ts->out_r[idx] = audio_stretch_saturate(ts->acc_r[i]);
      ts->out_write_pos++;
   }

   memmove(ts->acc_l, ts->acc_l + AUDIO_STRETCH_SYNTHESIS_HOP,
         AUDIO_STRETCH_SYNTHESIS_HOP * sizeof(float));
   memmove(ts->acc_r, ts->acc_r + AUDIO_STRETCH_SYNTHESIS_HOP,
         AUDIO_STRETCH_SYNTHESIS_HOP * sizeof(float));
   memset(ts->acc_l + AUDIO_STRETCH_SYNTHESIS_HOP, 0,
         AUDIO_STRETCH_SYNTHESIS_HOP * sizeof(float));
   memset(ts->acc_r + AUDIO_STRETCH_SYNTHESIS_HOP, 0,
         AUDIO_STRETCH_SYNTHESIS_HOP * sizeof(float));

   /* The nominal pointer advances by hop alone; the search only picks which
    * frame to window. Advancing from chosen instead would make consumption
    * hop + E[best_k], which the caller's ratio control cannot see. */
   ts->natural_pos   = chosen + AUDIO_STRETCH_SYNTHESIS_HOP;
   ts->analysis_pos += hop;
}

int audio_time_stretch_read(audio_time_stretch_t *ts, int16_t *frames,
      int num_frames, double ratio)
{
   int64_t seen_write;
   int64_t oldest_usable;
   int     n;
   int     i;

   seen_write = ts->write_pos;

   /* The producer outran us and overwrote frames we still wanted: skip
    * forward rather than read whatever landed on top of them. */
   oldest_usable = (seen_write - AUDIO_STRETCH_INPUT_CAPACITY)
         + AUDIO_STRETCH_SEARCH_RADIUS + AUDIO_STRETCH_FRAME_SIZE;
   if (ts->analysis_pos < oldest_usable)
   {
      ts->analysis_pos = oldest_usable;
      ts->natural_pos  = oldest_usable;
      ts->primed       = false;
   }

   /* Overlap-add reaches full amplitude only once the accumulator carries a
    * whole window, so the first hop after a reposition would ramp up from
    * silence. Run one hop to charge the accumulator and drop its output, so
    * the first frames handed to the caller are already at full amplitude. */
   if (!ts->primed && audio_stretch_can_synthesise(ts, seen_write))
   {
      audio_stretch_synthesise_hop(ts, ratio, seen_write);
      ts->out_read_pos = ts->out_write_pos;
   }

   while ((audio_time_stretch_output_fill(ts) < num_frames)
         && audio_stretch_can_synthesise(ts, seen_write))
      audio_stretch_synthesise_hop(ts, ratio, seen_write);

   n = audio_time_stretch_output_fill(ts);
   if (n > num_frames)
      n = num_frames;
   for (i = 0; i < n; i++)
   {
      int idx = (int)(ts->out_read_pos & AUDIO_STRETCH_OUT_MASK);
      frames[(i * 2) + 0] = ts->out_l[idx];
      frames[(i * 2) + 1] = ts->out_r[idx];
      ts->out_read_pos++;
   }

   return n;
}
