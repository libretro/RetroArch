/* Unit tests for the stretch-ratio helpers.
 *
 * Build:  cc -O2 -std=c89 -Wall -Wextra test_time_stretch_ratio.c \
 *            ../audio_time_stretch.c -I ../../include -lm \
 *            -o test_time_stretch_ratio */

#include <stdio.h>
#include <math.h>

#include <audio/audio_time_stretch.h>

static int failures = 0;

static void check(const char *what, double got, double want)
{
   if (fabs(got - want) > 1e-9)
   {
      printf("FAIL %-16s got %.9f want %.9f\n", what, got, want);
      failures++;
   }
   else
      printf("ok   %-16s %.9f\n", what, got);
}

static void check_int(const char *what, int got, int want)
{
   if (got != want)
   {
      printf("FAIL %-16s got %d want %d\n", what, got, want);
      failures++;
   }
   else
      printf("ok   %-16s %d\n", what, got);
}

int main(void)
{
   /* Steady state: arrival equals consumption and the ring sits on target,
    * so nothing needs stretching. */
   check("unity",      audio_time_stretch_ratio(512.0,  512, 4096, 4096), 1.0);

   /* 3x fast-forward delivers three times the input for the same output. */
   check("3x",         audio_time_stretch_ratio(1536.0, 512, 4096, 4096), 3.0);

   /* Slow-motion delivers less; the ratio drops below 1 and the stretcher
    * expands rather than pitching down. */
   check("half",       audio_time_stretch_ratio(256.0,  512, 4096, 4096), 0.5);

   /* A ring at twice its target lands the error term at exactly +1 without
    * needing the clamp to cut anything, so this alone does not prove the
    * clamp works - just that the ratio rises by the trim gain when the
    * (unclamped) error already happens to be 1. */
   check("full ring",  audio_time_stretch_ratio(512.0,  512, 8192, 4096),
         1.0 + AUDIO_STRETCH_TRIM_GAIN);

   /* A ring at eight times its target has to be clamped down to the same
    * +1 the case above reached without clamping - this is the case that
    * actually exercises the cut, and it must land on the identical ratio. */
   check("over-full ring", audio_time_stretch_ratio(512.0, 512, 32768, 4096),
         1.0 + AUDIO_STRETCH_TRIM_GAIN);

   /* An empty ring clamps to -1 and the ratio falls, so the ring refills. */
   check("empty ring", audio_time_stretch_ratio(512.0,  512,    0, 4096),
         1.0 - AUDIO_STRETCH_TRIM_GAIN);

   /* Both ends are clamped: a wild arrival estimate cannot drive the
    * synthesis hop out of range. */
   check("max clamp",  audio_time_stretch_ratio(1.0e9,  512, 4096, 4096),
         AUDIO_STRETCH_MAX_RATIO);
   check("min clamp",  audio_time_stretch_ratio(0.0,    512, 4096, 4096),
         AUDIO_STRETCH_MIN_RATIO);

   /* Degenerate inputs return unity rather than dividing by zero. */
   check("no output",  audio_time_stretch_ratio(512.0,    0, 4096, 4096), 1.0);
   check("no target",  audio_time_stretch_ratio(512.0,  512, 99999,   0), 1.0);

   /* A slow arrival still keeps the floor buffered. */
   check_int("fill floor",  audio_time_stretch_target_input_fill(100.0),
         AUDIO_STRETCH_MIN_TARGET_FILL);

   /* Above the floor the target tracks arrival: two flushes' worth, plus
    * enough for the search window to have somewhere to look. */
   check_int("fill scaled", audio_time_stretch_target_input_fill(4000.0),
         (2 * 4000) + AUDIO_STRETCH_SEARCH_RADIUS + AUDIO_STRETCH_FRAME_SIZE);

   /* Capped at half the ring, leaving the producer room to write. Also
    * guards against a huge arrival estimate overflowing the int cast. */
   check_int("fill cap",    audio_time_stretch_target_input_fill(1.0e9),
         AUDIO_STRETCH_INPUT_CAPACITY / 2);

   printf("\n%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
   return failures ? 1 : 0;
}
