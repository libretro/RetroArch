/* Compare a queued native WSOLA/SRC chain to source-position controls applied
 * directly, including discontinuities crossed without reading their audio. */
#define STRETCH_CHAIN_EMBEDDED
#include "stretch_src_test.c"
#include <retro_spsc.h>
#include "../../../audio/audio_pipeline_layout.h"

static bool epoch_reset(unsigned stage) { return stage % 4 == 0 || stage == 5; }
static uint32_t epoch_control(unsigned stage)
{
   return stage % 4 == 1 ? AUDIO_PIPELINE_STRETCH | 90112
      : stage % 4 == 2 ? AUDIO_PIPELINE_STRETCH
         | (stage < 4 ? 262144 : stage < 8 ? 2097152 : 16384) : 65536;
}
static unsigned epoch_layout(unsigned stage, unsigned channels)
{
   static const unsigned layouts[] = {3, 63, 255};
   return channels == 2 ? 3 : layouts[stage % 3];
}

static size_t epoch_run(unsigned channels, unsigned native, double ratio,
      unsigned hq, unsigned capacity, unsigned queued, unsigned discard)
{
   audio_pipeline_layout_t epochs;
   retro_spsc_t ring;
   audio_stretch_stream_t *stream = audio_stretch_stream_new(48000, channels, native, 1);
   union chain_buffer block, input;
   size_t frame = channels * (native ? sizeof(float) : sizeof(int16_t));
   const char *source = native ? (const char*)input_f : (const char*)input_i;
   size_t used = 0, published = 0, count, taken, iterations = 0;
   size_t origin = SIZE_MAX - 127;
   unsigned previous = (unsigned)-1, last_layout = 3;
   uint32_t serial = 0;
   bool complete = false;
   CHECK(stream != NULL);
   if (!stream || !retro_spsc_init(&ring, 4096)) abort();
   audio_pipeline_layout_init(&epochs, 3);
   retro_atomic_size_init(&ring.head, origin);
   retro_atomic_size_init(&ring.tail, origin);
   ring.cached_head = ring.cached_tail = origin;
   CHECK(audio_stretch_stream_bind(stream, &block, capacity));
   chain_init(channels, native, queued, ratio, hq);
   guarded = 1;
   while (used < FRAMES)
   {
      size_t n;
      unsigned stage = (unsigned)(used / 512);
      uint32_t control = epoch_control(stage);
      const void *view;
      bool reset = false, layout_changed;
      if (++iterations > 100000) abort();
      if (queued)
         while (published < FRAMES)
         {
            unsigned producer_stage = (unsigned)(published / 512);
            uint32_t request = epoch_control(producer_stage);
            n = retro_spsc_write_avail(&ring) / frame;
            if (n > 113) n = 113;
            if (n > 512 - published % 512) n = 512 - published % 512;
            if (!n) break;
            if (!audio_pipeline_layout_publish_transport(&epochs,
                     origin + published * frame, epoch_layout(producer_stage, channels),
                     request & AUDIO_PIPELINE_TEMPO_MASK,
                     (request & AUDIO_PIPELINE_STRETCH) != 0,
                     published % 512 == 0 && epoch_reset(producer_stage))) break;
            CHECK(retro_spsc_write_frames(&ring, source + published * frame, n, frame) == n);
            published += n;
         }
      view = audio_stretch_stream_peek(stream, &count);
      if (count)
      {
         if (count > 7) count = 7;
         chain_accept(view, count);
         CHECK(audio_stretch_stream_consume(stream, count));
         continue;
      }
      n = 512 - used % 512;
      if (queued && n > retro_spsc_read_avail(&ring) / frame)
         n = retro_spsc_read_avail(&ring) / frame;
      CHECK(n > 0);
      if (!n) abort();
      if (discard && (stage == 4 || stage == 5))
      {
         if (queued) CHECK(retro_spsc_skip(&ring, n * frame));
         used += n;
         continue;
      }
      if (queued)
      {
         size_t bytes = audio_pipeline_layout_limit_transport(&epochs,
               origin + used * frame, n * frame, ring.capacity);
         CHECK(bytes > 0 && bytes % frame == 0);
         n = bytes / frame;
         CHECK(epochs.current_control == control);
         CHECK(epochs.current_layout == epoch_layout(stage, channels));
         control = epochs.current_control;
         reset = serial != epochs.reset_serial;
         serial = epochs.reset_serial;
         previous = stage;
      }
      else if (previous != stage)
      {
         unsigned first = previous == (unsigned)-1 ? 0 : previous + 1;
         unsigned k;
         for (k = first; k <= stage; k++)
            if (epoch_reset(k)) reset = true;
         previous = stage;
      }
      layout_changed = last_layout != epoch_layout(stage, channels);
      last_layout = epoch_layout(stage, channels);
      if (layout_changed && !reset)
      {
         bool drained = false;
         while (!drained)
         {
            CHECK(audio_stretch_stream_finish_limit(stream, &drained, 13));
            view = audio_stretch_stream_peek(stream, &count);
            if (count)
            {
               if (count > 7) count = 7;
               chain_accept(view, count);
               CHECK(audio_stretch_stream_consume(stream, count));
            }
         }
      }
      if (reset || layout_changed)
      {
         unsigned pair;
         audio_stretch_stream_reset(stream);
         for (pair = 0; pair < (channels + 1) / 2; pair++)
            if (native) sinc_resampler.reset(chain_state.resampler[pair]);
            else sinc_resampler_int16_reset(chain_state.resampler[pair]);
         chain_state.source = 0;
      }
      if (n > 71) n = 71;
      if (queued) CHECK(retro_spsc_peek(&ring, &input, n * frame) == n * frame);
      CHECK(audio_stretch_stream_push_limit(stream,
               queued ? (const void*)&input : (const void*)(source + used * frame),
               n, &taken, (double)(control & AUDIO_PIPELINE_TEMPO_MASK) / 65536.0,
               (control & AUDIO_PIPELINE_STRETCH) != 0, 13));
      if (queued && taken) CHECK(retro_spsc_skip(&ring, taken * frame));
      used += taken;
   }
   while (!complete)
   {
      const void *view;
      CHECK(audio_stretch_stream_finish_limit(stream, &complete, 13));
      view = audio_stretch_stream_peek(stream, &count);
      if (count)
      {
         if (count > 7) count = 7;
         chain_accept(view, count);
         CHECK(audio_stretch_stream_consume(stream, count));
      }
   }
   if (queued) CHECK(!retro_spsc_read_avail(&ring));
   guarded = 0;
   audio_stretch_stream_free(stream);
   retro_spsc_free(&ring);
   return chain_free();
}

#ifndef STRETCH_EPOCH_EMBEDDED
int main(void)
{
   unsigned channels, native, hq, cap, discard, ratio, cases = 0;
   for (channels = 2; channels <= 11; channels += 9)
      for (native = 0; native < 2; native++)
         for (hq = 0; hq < 2; hq++)
            for (cap = 13; cap <= 257; cap += 244)
               for (discard = 0; discard < 2; discard++)
                  for (ratio = 1; ratio <= 2; ratio++)
                  {
                     size_t a, b, frame = channels * (native ? sizeof(float) : sizeof(int16_t));
                     fill(channels);
                     a = epoch_run(channels, native, ratio, hq, cap, 0, discard);
                     b = epoch_run(channels, native, ratio, hq, cap, 1, discard);
                     CHECK(a == b);
                     CHECK(memcmp(native ? (void*)sink_f[0] : (void*)sink_i[0],
                              native ? (void*)sink_f[1] : (void*)sink_i[1], a * frame) == 0);
                     cases++;
                     printf("  case %u/64 channels=%u native=%u hq=%u cap=%u discard=%u ratio=%u\n",
                           cases, channels, native, hq, cap, discard, ratio);
                     fflush(stdout);
                  }
   CHECK(heap_calls == 0);
   printf("queued stretch + sinc: %u cases, %u failures, %u guarded heap calls\n", cases, failures, heap_calls);
   return failures != 0;
}

#endif
