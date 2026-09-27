/* Run the same binary/build settings on both revisions; lower is better. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "../../../audio/audio_stretch.h"

#define INPUT_FRAMES 8192
#define OUTPUT_FRAMES (INPUT_FRAMES * 4)
static float input_f[INPUT_FRAMES * 8], output_f[OUTPUT_FRAMES * 8];
static int16_t input_i[INPUT_FRAMES * 8], output_i[OUTPUT_FRAMES * 8];

static int process(audio_stretch_stream_t *s, unsigned lane, double tempo,
      size_t *produced)
{
   struct audio_stretch_io io;
   io.input = lane ? (const void*)input_f : (const void*)input_i;
   io.output = lane ? (void*)output_f : (void*)output_i;
   io.input_frames = INPUT_FRAMES; io.output_capacity = OUTPUT_FRAMES;
   if (!audio_stretch_stream_process(s, &io, tempo, true)
         || io.input_used != INPUT_FRAMES) return 0;
   *produced += io.output_frames;
   return 1;
}

int main(int argc, char **argv)
{
   unsigned rate, channels, lane, trial, i;
   double tempo = argc > 1 ? atof(argv[1]) : 2.0;
   double minimum_ms = argc > 2 ? atof(argv[2]) : 50.0;
   if (argc > 3 || !(tempo >= 0.25 && tempo <= 32.0)
         || !(minimum_ms >= 10.0 && minimum_ms <= 2000.0))
   {
      fprintf(stderr, "usage: stretch_bench [tempo:0.25..32 [minimum-ms:10..2000]]\n");
      return 2;
   }
   puts("rate,channels,float,tempo,median_ns_per_input_frame");
   for (rate = 48000; rate <= 192000; rate *= 4)
      for (channels = 2; channels <= 8; channels *= 4)
         for (lane = 0; lane < 2; lane++)
         {
            double timings[7];
            audio_stretch_stream_t *s = audio_stretch_stream_new(rate, channels, lane, 1);
            if (!s) return 2;
            for (i = 0; i < INPUT_FRAMES * channels; i++)
            {
               input_i[i] = (int16_t)(15000 * sin(i * 0.0576 / channels));
               input_f[i] = input_i[i] / 32768.0f;
            }
            for (trial = 0; trial < 7; trial++)
            {
               clock_t start, end;
               size_t calls = 0, produced = 0;
               double elapsed;
               audio_stretch_stream_reset(s);
               for (i = 0; i < 8; i++)
                  if (!process(s, lane, tempo, &produced)) return 3;
               start = clock();
               if (start == (clock_t)-1) return 4;
               do
               {
                  for (i = 0; i < 16; i++)
                     if (!process(s, lane, tempo, &produced)) return 3;
                  calls += 16;
                  end = clock();
                  if (end == (clock_t)-1) return 4;
                  elapsed = (double)(end - start) / CLOCKS_PER_SEC;
               } while (elapsed * 1000.0 < minimum_ms);
               timings[trial] = elapsed * 1.0e9 / ((double)calls * INPUT_FRAMES);
               if (!produced) return 3;
            }
            for (trial = 1; trial < 7; trial++)
            {
               double value = timings[trial];
               unsigned at = trial;
               while (at && timings[at - 1] > value)
               {
                  timings[at] = timings[at - 1]; at--;
               }
               timings[at] = value;
            }
            printf("%u,%u,%u,%.6g,%.3f\n", rate, channels, lane, tempo, timings[3]);
            audio_stretch_stream_free(s);
         }
   return 0;
}
