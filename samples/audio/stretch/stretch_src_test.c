/* Exercise the real native transport and sinc backends together. */
#define main stretch_unit_main
#include "stretch_test.c"
#undef main
#include <audio/sinc_resampler.h>
#include <audio/sinc_resampler_int16.h>

union chain_buffer { float f[4096 * AUDIO_STRETCH_MAX_CHANNELS]; int16_t i[4096 * AUDIO_STRETCH_MAX_CHANNELS]; };
static float sink_f[2][65536 * AUDIO_STRETCH_MAX_CHANNELS];
static int16_t sink_i[2][65536 * AUDIO_STRETCH_MAX_CHANNELS];
struct chain
{
   void *resampler[(AUDIO_STRETCH_MAX_CHANNELS + 1) / 2];
   unsigned channels, native, slot;
   size_t source, produced;
   double nominal;
   union chain_buffer pair_in, pair_out, ready;
};
static struct chain chain_state;

static void chain_init(unsigned channels, unsigned native, unsigned slot,
      double ratio, unsigned hq)
{
   unsigned pair;
   struct chain *s = &chain_state;
   memset(s, 0, sizeof(*s));
   s->channels = channels; s->native = native; s->slot = slot; s->nominal = ratio;
   for (pair = 0; pair < (channels + 1) / 2; pair++)
   {
      if (native)
         s->resampler[pair] = sinc_resampler_init_hq(ratio,
               RESAMPLER_QUALITY_NORMAL,
#ifdef AUDIO_STRETCH_SCALAR
               0,
#else
               RESAMPLER_SIMD_SSE,
#endif
               hq);
      else s->resampler[pair] = sinc_resampler_int16_init_hq(ratio,
            SINC_INT16_QUALITY_NORMAL, hq);
      CHECK(s->resampler[pair] != NULL);
   }
}

static void chain_accept(const void *input, size_t frames)
{
   struct chain *s = &chain_state;
   size_t sample = s->native ? sizeof(float) : sizeof(int16_t);
   const char *src = (const char*)input;
   while (frames)
   {
      double ratio = s->nominal * ((s->source / 512) % 2 ? 1.001 : 1.0);
      size_t n = (size_t)((128 - 16) / ratio), output = 0, f, accepted;
      unsigned pair, c;
      if (n > 512 - s->source % 512) n = 512 - s->source % 512;
      if (n > frames) n = frames;
      CHECK(n > 0);
      if (!n) return;
      for (pair = 0; pair < (s->channels + 1) / 2; pair++)
      {
         size_t produced;
         memset(&s->pair_out, 0x5a, sizeof(s->pair_out));
         for (f = 0; f < n; f++)
            for (c = 0; c < 2; c++)
            {
               char *dst = (char*)&s->pair_in + (f * 2 + c) * sample;
               if (pair * 2 + c < s->channels)
                  memcpy(dst, src + (f * s->channels + pair * 2 + c) * sample, sample);
               else memset(dst, 0, sample);
            }
         if (s->native)
         {
            struct resampler_data io;
            io.data_in = s->pair_in.f; io.data_out = s->pair_out.f;
            io.input_frames = n; io.ratio = ratio;
            sinc_resampler.process(s->resampler[pair], &io);
            produced = io.output_frames;
         }
         else
         {
            struct resampler_data_int16 io;
            io.data_in = s->pair_in.i; io.data_out = s->pair_out.i;
            io.input_frames = n; io.ratio = ratio;
            sinc_resampler_int16_process(s->resampler[pair], &io);
            produced = io.output_frames;
         }
         CHECK(produced <= 128);
         if (pair) CHECK(produced == output);
         output = produced;
         for (f = output * 2 * sample; f < sizeof(s->pair_out); f++)
            CHECK(((unsigned char*)&s->pair_out)[f] == 0x5a);
         for (f = 0; f < output; f++)
            for (c = 0; c < 2 && pair * 2 + c < s->channels; c++)
               memcpy((char*)&s->ready + (f * s->channels + 2 * pair + c) * sample,
                     (char*)&s->pair_out + (f * 2 + c) * sample, sample);
      }
      /* Retain device-format output across short accepts. */
      accepted = 0;
      while (accepted < output)
      {
         size_t take = (accepted % 7) + 1;
         char *sink = s->native ? (char*)sink_f[s->slot] : (char*)sink_i[s->slot];
         if (take > output - accepted) take = output - accepted;
         CHECK(s->produced + take <= 65536);
         memcpy(sink + s->produced * s->channels * sample,
               (char*)&s->ready + accepted * s->channels * sample,
               take * s->channels * sample);
         accepted += take; s->produced += take;
      }
      s->source += n; frames -= n; src += n * s->channels * sample;
   }
}

static size_t chain_free(void)
{
   unsigned p;
   for (p = 0; p < (chain_state.channels + 1) / 2; p++)
      if (chain_state.native) sinc_resampler.free(chain_state.resampler[p]);
      else sinc_resampler_int16_free(chain_state.resampler[p]);
   return chain_state.produced;
}

#ifndef STRETCH_CHAIN_EMBEDDED
int main(void)
{
   const double ratios[] = {0.75, 2.0, 4.0};
   unsigned channels, native, ratio, hq, capacity;
   for (channels = 2; channels <= AUDIO_STRETCH_MAX_CHANNELS; channels += channels == 2 ? 6 : 3)
      for (native = 0; native < 2; native++)
         for (ratio = 0; ratio < 3; ratio++)
            for (hq = (native + ratio) & 1; hq < 2; hq += 2)
            {
               size_t reference, frames, frame = channels * (native ? sizeof(float) : sizeof(int16_t));
               fill(channels);
               frames = adapter_run(48000, channels, native, 2.0, 5, 0, 0, FRAMES / 4);
               chain_init(channels, native, 0, ratios[ratio], hq);
               guarded = 1;
               chain_accept(native ? (void*)output_f[0] : (void*)output_i[0], frames);
               guarded = 0;
               reference = chain_free();
               capacity = ((channels + native + ratio) & 1) ? 13 : 257;
               {
                  audio_stretch_stream_t *stream = audio_stretch_stream_new(48000, channels, native, 1);
                  union chain_buffer block;
                  const char *input = native ? (const char*)input_f : (const char*)input_i;
                  size_t used = 0, n, count, produced, budget, pending;
                  unsigned stage;
                  bool complete = false;
                  const void *view;
                  CHECK(audio_stretch_stream_bind(stream, &block, capacity));
                  chain_init(channels, native, 1, ratios[ratio], hq);
                  guarded = 1;
                  /* Abandon an old stream with both SRC history and pending
                   * native output, then reuse every allocation. */
                  CHECK(audio_stretch_stream_push(stream, input, 512, &n, 2.0, true));
                  view = audio_stretch_stream_peek(stream, &count);
                  CHECK(count > 0);
                  chain_accept(view, count > 7 ? 7 : count);
                  CHECK(!audio_stretch_stream_quiescent(stream));
                  audio_stretch_stream_reset(stream);
                  CHECK(audio_stretch_stream_quiescent(stream));
                  CHECK(!audio_stretch_stream_peek(stream, &count) && !count);
                  for (stage = 0; stage < (channels + 1) / 2; stage++)
                     if (native) sinc_resampler.reset(chain_state.resampler[stage]);
                     else sinc_resampler_int16_reset(chain_state.resampler[stage]);
                  chain_state.source = chain_state.produced = 0;
                  for (stage = 0; stage < 4; stage++)
                     while (used < (stage + 1) * (FRAMES / 4))
                     {
                        size_t limit = (stage + 1) * (FRAMES / 4) - used;
                        if (limit > 71) limit = 71;
                        audio_stretch_stream_peek(stream, &pending);
                        budget = 1 + used % 23;
                        CHECK(audio_stretch_stream_push_limit(stream, input + used * frame,
                                 limit, &n, 2.0, (5 >> stage) & 1, budget));
                        used += n;
                        view = audio_stretch_stream_peek(stream, &count);
                        CHECK(pending || count <= budget);
                        if (count)
                        {
                           if (count > 17) count = 17;
                           chain_accept(view, count);
                           CHECK(audio_stretch_stream_consume(stream, count));
                        }
                        CHECK(n || count);
                     }
                  while (!complete)
                  {
                     audio_stretch_stream_peek(stream, &pending);
                     CHECK(audio_stretch_stream_finish_limit(stream, &complete, 7));
                     view = audio_stretch_stream_peek(stream, &count);
                     CHECK(pending || count <= 7);
                     if (count)
                     {
                        if (count > 11) count = 11;
                        chain_accept(view, count);
                        CHECK(audio_stretch_stream_consume(stream, count));
                     }
                  }
                  CHECK(chain_state.source == frames);
                  guarded = 0;
                  produced = chain_free();
                  CHECK(produced == reference);
                  CHECK(memcmp(native ? (void*)sink_f[0] : (void*)sink_i[0],
                           native ? (void*)sink_f[1] : (void*)sink_i[1], reference * frame) == 0);
                  audio_stretch_stream_free(stream);
               }
            }
   CHECK(heap_calls == 0);
   printf("stretch + sinc: %u failures, %u guarded heap calls\n", failures, heap_calls);
   return failures != 0;
}

#endif
