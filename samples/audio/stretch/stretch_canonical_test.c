/* A sparse canonical frame must make the same timing decisions as its
 * compact native layout, including when the search channel is slot ten. */
#define main stretch_unit_main
#include "stretch_test.c"
#undef main

union native_block
{
   float f[FRAMES * AUDIO_STRETCH_MAX_CHANNELS];
   int16_t i[FRAMES * AUDIO_STRETCH_MAX_CHANNELS];
};
static union native_block source[2], result[2];

static void compare_frames(unsigned channels, const unsigned *slots,
      unsigned native, size_t frames)
{
   size_t f, sample = native ? sizeof(float) : sizeof(int16_t);
   unsigned c, bit;
   for (f = 0; f < frames; f++)
   {
      for (c = 0; c < channels; c++)
         CHECK(memcmp((char*)&result[0] + (f * channels + c) * sample,
                  (char*)&result[1] + (f * AUDIO_STRETCH_MAX_CHANNELS + slots[c]) * sample,
                  sample) == 0);
      for (bit = 0; bit < AUDIO_STRETCH_MAX_CHANNELS; bit++)
      {
         for (c = 0; c < channels && slots[c] != bit; c++) { }
         if (c == channels)
         {
            if (native) CHECK(result[1].f[f * AUDIO_STRETCH_MAX_CHANNELS + bit] == 0.0f);
            else CHECK(result[1].i[f * AUDIO_STRETCH_MAX_CHANNELS + bit] == 0);
         }
      }
   }
}

static void canonical_case(unsigned rate, unsigned channels,
      const unsigned *slots, unsigned native, double tempo, unsigned last)
{
   audio_stretch_stream_t *stream[2];
   uint32_t masks[2] = {0, 0};
   size_t used = 0, sample = native ? sizeof(float) : sizeof(int16_t);
   unsigned c, f, stage, iteration = 0;
   struct audio_stretch_io io[2];
   struct audio_stretch_drain_io drain[2];
   memset(source, 0, sizeof(source));
   for (c = 0; c < channels; c++)
   {
      if ((last && c == channels - 1) || (!last && slots[c] != 3))
      {
         masks[0] |= 1u << c;
         masks[1] |= 1u << slots[c];
      }
      for (f = 0; f < FRAMES; f++)
      {
         /* Independent channels, dominant LFE excluded from the search. */
         int16_t value = (int16_t)((f * (37 + c * 6) + f / 31) % 12001 - 6000);
         if (slots[c] == 3) value = f % 2 ? 30000 : -30000;
         if (native)
         {
            source[0].f[f * channels + c] = value / 32768.0f;
            source[1].f[f * AUDIO_STRETCH_MAX_CHANNELS + slots[c]] = value / 32768.0f;
         }
         else
         {
            source[0].i[f * channels + c] = value;
            source[1].i[f * AUDIO_STRETCH_MAX_CHANNELS + slots[c]] = value;
         }
      }
   }
   stream[0] = audio_stretch_stream_new(rate, channels, native, masks[0]);
   stream[1] = audio_stretch_stream_new(rate, AUDIO_STRETCH_MAX_CHANNELS, native, masks[1]);
   CHECK(stream[0] && stream[1]);
   if (!stream[0] || !stream[1]) abort();
   guarded = 1;
   for (stage = 0; stage < 4; stage++)
      while (used < (stage + 1) * (FRAMES / 4))
      {
         static const size_t budgets[] = {0, 1, 13, 257};
         size_t count = (stage + 1) * (FRAMES / 4) - used;
         size_t budget = budgets[iteration++ % 4];
         if (count > 71) count = 71;
         memset(result, 0x5a, sizeof(result));
         for (c = 0; c < 2; c++)
         {
            unsigned width = c ? AUDIO_STRETCH_MAX_CHANNELS : channels;
            io[c].input = (char*)&source[c] + used * width * sample;
            io[c].input_frames = count;
            io[c].output = &result[c]; io[c].output_capacity = budget;
            CHECK(audio_stretch_stream_process(stream[c], &io[c], tempo, (5 >> stage) & 1));
            CHECK(((unsigned char*)&result[c])[budget * width * sample] == 0x5a);
         }
         CHECK(io[0].input_used == io[1].input_used);
         CHECK(io[0].output_frames == io[1].output_frames);
         compare_frames(channels, slots, native, io[0].output_frames);
         CHECK(!budget || io[0].input_used || io[0].output_frames);
         used += io[0].input_used;
         if (iteration > 100000) abort();
      }
   do
   {
      for (c = 0; c < 2; c++)
      {
         drain[c].output = &result[c]; drain[c].output_capacity = 13;
         CHECK(audio_stretch_stream_flush(stream[c], &drain[c]));
      }
      CHECK(drain[0].complete == drain[1].complete);
      CHECK(drain[0].output_frames == drain[1].output_frames);
      compare_frames(channels, slots, native, drain[0].output_frames);
      if (++iteration > 100000) abort();
   } while (!drain[0].complete);
   for (c = 0; c < 2; c++)
   {
      unsigned width = c ? AUDIO_STRETCH_MAX_CHANNELS : channels;
      audio_stretch_stream_reset(stream[c]);
      CHECK(audio_stretch_stream_quiescent(stream[c]));
      io[c].input = &source[c]; io[c].input_frames = 257;
      io[c].output = &result[c]; io[c].output_capacity = 257;
      CHECK(audio_stretch_stream_process(stream[c], &io[c], 1, false));
      CHECK(io[c].input_used == 257 && io[c].output_frames == 257);
      CHECK(memcmp(&source[c], &result[c], 257 * width * sample) == 0);
   }
   guarded = 0;
   audio_stretch_stream_free(stream[0]); audio_stretch_stream_free(stream[1]);
}

int main(void)
{
   static const unsigned slots[][8] = {{0, 10}, {0, 1, 2, 3, 9, 10}, {0, 1, 2, 3, 4, 5, 9, 10}};
   static const unsigned widths[] = {2, 6, 8}, rates[] = {8000, 44100, 192000};
   static const double tempos[] = {0.25, 1.37, 4, 32};
   unsigned r, w, n, t, last, cases = 0;
   for (r = 0; r < 3; r++)
      for (w = 0; w < 3; w++)
         for (n = 0; n < 2; n++)
            for (t = 0; t < 4; t++)
               for (last = 0; last < 2; last++, cases++)
                  canonical_case(rates[r], widths[w], slots[w], n, tempos[t], last);
   CHECK(heap_calls == 0);
   printf("canonical stretch: %u cases, %u failures, %u guarded heap calls\n", cases, failures, heap_calls);
   return failures != 0;
}
