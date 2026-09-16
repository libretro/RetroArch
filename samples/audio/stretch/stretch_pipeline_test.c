#define STRETCH_EPOCH_EMBEDDED
#include "stretch_epoch_test.c"
#include "../../../audio/audio_pipeline_stretch.h"
#include <rthreads/rthreads.h>
#include <retro_timers.h>

/* Yield inside the spin waits.
 *
 * The waits below are pure busy loops.  On a machine with more than
 * one core that is what is wanted - the point is to hammer the
 * producer/consumer handoff, not to test a scheduler - but on a
 * uniprocessor it is a livelock: the spinning thread holds the CPU for
 * its whole quantum while the thread it waits on never runs, so
 * progress comes only from preemption and the stress loops take
 * effectively forever.  Single-core CI runners and containers are
 * common enough to matter; this test used to hang on them.
 *
 * Spin a short while first and only then yield: on a multi-core host
 * the other thread is normally already running and the wait resolves
 * inside the spin budget, so the yield costs nothing there, while on a
 * uniprocessor the loop still hands the CPU over instead of holding
 * it.  retro_sleep(0) is a yield on every platform rthreads supports. */
#define STRESS_SPIN_BUDGET 64

#define STRESS_YIELD(counter) \
   do { \
      if (++(counter) >= STRESS_SPIN_BUDGET) \
      { \
         (counter) = 0; \
         retro_sleep(0); \
      } \
   } while (0)

static size_t pipeline_run(unsigned channels, unsigned native, double ratio,
      unsigned hq, unsigned capacity, unsigned discard)
{
   audio_pipeline_layout_t epochs;
   retro_spsc_t ring;
   audio_pipeline_stretch_t *stage;
   struct audio_pipeline_stretch_block block;
   union chain_buffer output;
   size_t frame = channels * (native ? sizeof(float) : sizeof(int16_t));
   const char *source = native ? (const char*)input_f : (const char*)input_i;
   size_t used = 0, published = 0, pending = 0, iterations = 0;
   size_t origin = SIZE_MAX - 127;
   uint32_t serial = 0;
   bool complete = false;
   if (!retro_spsc_init(&ring, 4096)) abort();
   retro_atomic_size_init(&ring.head, origin);
   retro_atomic_size_init(&ring.tail, origin);
   ring.cached_head = ring.cached_tail = origin;
   audio_pipeline_layout_init(&epochs, 3);
   memset(&output, 0x5a, sizeof(output));
   stage = audio_pipeline_stretch_new(48000, channels, native, 1,
         &ring, &epochs, &output, capacity);
   CHECK(stage != NULL);
   if (!stage) abort();
   chain_init(channels, native, 1, ratio, hq);
   guarded = 1;
   while (!complete)
   {
      size_t before, accepted;
      unsigned segment = (unsigned)(used / 512);
      if (++iterations > 1000000) abort();
      while (published < FRAMES)
      {
         unsigned producer = (unsigned)(published / 512);
         uint32_t request = epoch_control(producer);
         size_t n = retro_spsc_write_avail(&ring) / frame;
         if (n > 113) n = 113;
         if (n > 512 - published % 512) n = 512 - published % 512;
         if (!n) break;
         if (!audio_pipeline_layout_publish_transport(&epochs,
                  origin + published * frame, epoch_layout(producer, channels),
                  request & AUDIO_PIPELINE_TEMPO_MASK,
                  (request & AUDIO_PIPELINE_STRETCH) != 0,
                  published % 512 == 0 && epoch_reset(producer))) break;
         CHECK(retro_spsc_write_frames(&ring, source + published * frame, n, frame) == n);
         published += n;
      }
      if (!pending && discard && (segment == 4 || segment == 5))
      {
         size_t n = retro_spsc_read_avail(&ring) / frame;
         if (n > 512 - used % 512) n = 512 - used % 512;
         CHECK(n > 0 && audio_pipeline_stretch_discard(stage, n));
         used += n;
         continue;
      }
      before = retro_atomic_load_relaxed_size(&ring.tail);
      if (used == FRAMES)
         CHECK(audio_pipeline_stretch_finish(stage, 13, &block, &complete));
      else
         CHECK(audio_pipeline_stretch_next(stage, 71, 13, &block));
      CHECK(((const uint8_t*)&output)[capacity * frame] == 0x5a);
      CHECK(block.input_used <= 71);
      CHECK(retro_atomic_load_relaxed_size(&ring.tail) - before == block.input_used * frame);
      used += block.input_used;
      if (serial != block.reset_serial)
      {
         unsigned pair;
         for (pair = 0; pair < (channels + 1) / 2; pair++)
            if (native) sinc_resampler.reset(chain_state.resampler[pair]);
            else sinc_resampler_int16_reset(chain_state.resampler[pair]);
         chain_state.source = 0;
         serial = block.reset_serial;
      }
      accepted = block.frames > 7 ? 7 : block.frames;
      pending = block.frames - accepted;
      before = retro_atomic_load_relaxed_size(&ring.tail);
      if (accepted) chain_accept(block.data, accepted);
      CHECK(audio_pipeline_stretch_consume(stage, accepted));
      CHECK(retro_atomic_load_relaxed_size(&ring.tail) - before
            == (block.passthrough ? accepted * frame : 0));
      used += (retro_atomic_load_relaxed_size(&ring.tail) - before) / frame;
      CHECK(used <= FRAMES);
   }
   CHECK(used == FRAMES && !retro_spsc_read_avail(&ring));
   guarded = 0;
   audio_pipeline_stretch_free(stage);
   retro_spsc_free(&ring);
   return chain_free();
}

static void direct_contracts(unsigned channels, unsigned native, unsigned wrapped)
{
   retro_spsc_t ring;
   audio_pipeline_layout_t metadata;
   audio_pipeline_stretch_t *stage;
   struct audio_pipeline_stretch_block block, again;
   union chain_buffer output, data;
   size_t sample = native ? sizeof(float) : sizeof(int16_t);
   size_t frame = channels * sample, origin, tail, accepted = 0, i;
   const void *direct;
   bool complete;
   CHECK(retro_spsc_init(&ring, 256));
   origin = wrapped == 2 ? 1 : wrapped ? SIZE_MAX - sample + 1 : 0;
   retro_atomic_size_init(&ring.head, origin);
   retro_atomic_size_init(&ring.tail, origin);
   ring.cached_head = ring.cached_tail = origin;
   audio_pipeline_layout_init(&metadata, 3);
   memset(&output, 0x5a, sizeof(output));
   /* Opaque native payloads: inactive processing must preserve every bit. */
   for (i = 0; i < 4 * frame; i++) ((uint8_t*)&data)[i] = (uint8_t)(i * 31);
   stage = audio_pipeline_stretch_new(48000, channels, native, 1,
         &ring, &metadata, &output, 16);
   CHECK(stage != NULL);
   if (!stage) abort();
   guarded = 1;
   CHECK(retro_spsc_write_frames(&ring, &data, 4, frame) == 4);
   retro_spsc_read_begin(&ring, &direct);
   retro_spsc_read_end(&ring, 0);
   CHECK(audio_pipeline_stretch_next(stage, 4, 4, &block));
   CHECK(block.frames == (wrapped ? 1 : 4) && block.input_used == 0 && block.passthrough);
   CHECK((block.data == direct) == !wrapped);
   tail = retro_atomic_load_relaxed_size(&ring.tail);
   CHECK(audio_pipeline_stretch_next(stage, 4, 0, &again));
   CHECK(!again.data && !again.frames && !again.input_used);
   CHECK(retro_atomic_load_relaxed_size(&ring.tail) == tail);
   CHECK(!audio_pipeline_stretch_discard(stage, 5));
   CHECK(!audio_pipeline_stretch_consume(stage, block.frames + 1));
   CHECK(!audio_pipeline_stretch_finish(stage, 4, &again, &complete));
   CHECK(audio_pipeline_stretch_next(stage, 0, 1, &again));
   CHECK(again.data == block.data && again.frames == 1 && !again.input_used);
   CHECK(!audio_pipeline_stretch_consume(stage, 2));
   while (accepted < 4)
   {
      CHECK(audio_pipeline_stretch_next(stage, 4, 1, &block));
      CHECK(block.frames == 1 && !block.input_used);
      CHECK(memcmp(block.data, (const char*)&data + accepted * frame, frame) == 0);
      CHECK(audio_pipeline_stretch_consume(stage, 1));
      accepted++;
   }
   CHECK(retro_atomic_load_relaxed_size(&ring.tail) - tail == 4 * frame);
   for (i = 0; i < sizeof(output); i++) CHECK(((uint8_t*)&output)[i] == 0x5a);
   CHECK(audio_pipeline_stretch_finish(stage, 0, &block, &complete) && !complete);
   CHECK(audio_pipeline_stretch_finish(stage, 4, &block, &complete) && complete);
   CHECK(retro_spsc_write_frames(&ring, &data, 1, frame) == 1);
   CHECK(!audio_pipeline_stretch_next(stage, 1, 1, &block));
   CHECK(audio_pipeline_stretch_discard(stage, 0));
   CHECK(audio_pipeline_stretch_next(stage, 1, 1, &block));
   CHECK(block.reset_serial == 1 && block.frames == 1);
   CHECK(audio_pipeline_stretch_discard(stage, 1));
   CHECK(!audio_pipeline_stretch_consume(stage, 1));
   CHECK(audio_pipeline_stretch_next(stage, 0, 1, &block));
   CHECK(block.reset_serial == 2 && !block.frames);
   guarded = 0;
   audio_pipeline_stretch_free(stage);
   retro_spsc_free(&ring);
}

static void reset_contracts(unsigned native)
{
   retro_spsc_t ring;
   audio_pipeline_layout_t metadata;
   audio_pipeline_stretch_t *stage;
   struct audio_pipeline_stretch_block block;
   union chain_buffer input, output;
   size_t frame = 2 * (native ? sizeof(float) : sizeof(int16_t));
   size_t tail;
   CHECK(retro_spsc_init(&ring, 4096));
   audio_pipeline_layout_init(&metadata, 3);
   stage = audio_pipeline_stretch_new(48000, 2, native, 1,
         &ring, &metadata, &output, 13);
   CHECK(stage != NULL);
   if (!stage) abort();
   memset(&input, 0x2a, sizeof(input));
   guarded = 1;
   CHECK(audio_pipeline_layout_publish_transport(&metadata, 0, 3, 131072, true, false));
   CHECK(retro_spsc_write_frames(&ring, &input, 32, frame) == 32);
   CHECK(audio_pipeline_stretch_next(stage, 32, 13, &block));
   CHECK(block.input_used == 32 && !block.frames && !block.reset_serial);
   CHECK(audio_pipeline_layout_publish_transport(&metadata, 32 * frame, 3, 65536, false, true));
   tail = retro_atomic_load_relaxed_size(&metadata.tail);
   CHECK(audio_pipeline_stretch_next(stage, 0, 0, &block));
   CHECK(!block.reset_serial && retro_atomic_load_relaxed_size(&metadata.tail) == tail);
   CHECK(audio_pipeline_stretch_next(stage, 0, 13, &block));
   CHECK(block.reset_serial == 1 && !block.frames && !block.input_used);
   memset(&input, 0x55, sizeof(input));
   CHECK(retro_spsc_write_frames(&ring, &input, 1, frame) == 1);
   CHECK(audio_pipeline_stretch_next(stage, 1, 13, &block));
   CHECK(block.frames == 1 && memcmp(block.data, &input, frame) == 0);
   CHECK(audio_pipeline_stretch_consume(stage, 1));
   CHECK(audio_pipeline_layout_publish_transport(&metadata, 33 * frame, 3, 131072, true, false));
   CHECK(retro_spsc_write_frames(&ring, &input, 512, frame) == 512);
   CHECK(audio_pipeline_stretch_next(stage, 512, 13, &block));
   CHECK(block.frames > 0 && !block.passthrough);
   CHECK(audio_pipeline_stretch_discard(stage, 0));
   CHECK(!audio_pipeline_stretch_consume(stage, 1));
   CHECK(audio_pipeline_stretch_next(stage, 0, 13, &block));
   CHECK(block.reset_serial == 2 && !block.frames && !block.input_used);
   guarded = 0;
   audio_pipeline_stretch_free(stage);
   retro_spsc_free(&ring);
}

static void factory_contracts(void)
{
   retro_spsc_t ring;
   audio_pipeline_layout_t metadata;
   union chain_buffer output;
   unsigned n;
   CHECK(retro_spsc_init(&ring, 256));
   audio_pipeline_layout_init(&metadata, 3);
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 1, NULL, &metadata, &output, 16));
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 1, &ring, NULL, &output, 16));
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 1, &ring, &metadata, NULL, 16));
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 1, &ring, &metadata, &output, 0));
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 1, &ring, &metadata, (char*)&output + 1, 16));
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 1, &ring, &metadata, &output, SIZE_MAX));
   CHECK(!audio_pipeline_stretch_new(48000, 12, true, 1, &ring, &metadata, &output, 16));
   CHECK(!audio_pipeline_stretch_new(7999, 2, true, 1, &ring, &metadata, &output, 16));
   CHECK(!audio_pipeline_stretch_new(48000, 2, true, 4, &ring, &metadata, &output, 16));
   for (n = 1; n <= 4; n++)
   {
      fail_after = n;
      CHECK(!audio_pipeline_stretch_new(48000, 2, false, 1, &ring, &metadata, &output, 16));
      fail_after = 0;
   }
   audio_pipeline_stretch_free(NULL);
   retro_spsc_free(&ring);
}

struct pipeline_stress
{
   retro_spsc_t ring;
   audio_pipeline_layout_t metadata;
   size_t frame;
};

static unsigned stress_layout(unsigned token)
{
   static const unsigned layouts[] = {3, 63, 255};
   return layouts[(token / 17) % 3];
}

static void pipeline_produce(void *arg)
{
   struct pipeline_stress *s = (struct pipeline_stress*)arg;
   uint8_t frame[44];
   unsigned token;
   unsigned spin = 0;
   size_t c;
   for (token = 0; token < 100000; token++)
   {
      for (c = 0; c < s->frame; c++) frame[c] = (uint8_t)(token * 13 + c);
      while (!audio_pipeline_layout_publish_transport(&s->metadata,
               retro_atomic_load_relaxed_size(&s->ring.head), stress_layout(token),
               65536, false, false))
         STRESS_YIELD(spin);
      while (!retro_spsc_write_frames(&s->ring, frame, 1, s->frame))
         STRESS_YIELD(spin);
   }
}

static void concurrent_direct(unsigned native)
{
   struct pipeline_stress s;
   audio_pipeline_stretch_t *stage;
   struct audio_pipeline_stretch_block block;
   union chain_buffer output;
   sthread_t *producer;
   unsigned token = 0;
   unsigned spin  = 0;
   size_t origin = SIZE_MAX - 127;
   s.frame = 11 * (native ? sizeof(float) : sizeof(int16_t));
   CHECK(retro_spsc_init(&s.ring, 4096));
   audio_pipeline_layout_init(&s.metadata, 3);
   retro_atomic_size_init(&s.ring.head, origin);
   retro_atomic_size_init(&s.ring.tail, origin);
   s.ring.cached_head = s.ring.cached_tail = origin;
   stage = audio_pipeline_stretch_new(48000, 11, native, 1,
         &s.ring, &s.metadata, &output, 13);
   CHECK(stage != NULL);
   producer = sthread_create(pipeline_produce, &s);
   CHECK(producer != NULL);
   if (!stage || !producer) abort();
   while (token < 100000)
   {
      size_t f, c, take;
      CHECK(audio_pipeline_stretch_next(stage, 71, 13, &block));
      CHECK(!block.input_used && (!block.frames || block.passthrough));
      take = block.frames > 3 ? 3 : block.frames;
      if (!take) STRESS_YIELD(spin);
      for (f = 0; f < take; f++, token++)
      {
         CHECK(block.layout == stress_layout(token));
         for (c = 0; c < s.frame; c++)
            CHECK(((const uint8_t*)block.data)[f * s.frame + c] == (uint8_t)(token * 13 + c));
      }
      CHECK(audio_pipeline_stretch_consume(stage, take));
   }
   sthread_join(producer);
   CHECK(!retro_spsc_read_avail(&s.ring));
   audio_pipeline_stretch_free(stage);
   retro_spsc_free(&s.ring);
}

int main(void)
{
   unsigned channels, native, hq, cap, discard, ratio, cases = 0;
   factory_contracts();
   reset_contracts(0);
   reset_contracts(1);
   concurrent_direct(0);
   concurrent_direct(1);
   for (channels = 2; channels <= 11; channels += 9)
      for (native = 0; native < 2; native++)
      {
         direct_contracts(channels, native, 0);
         direct_contracts(channels, native, 1);
         direct_contracts(channels, native, 2);
         for (hq = 0; hq < 2; hq++)
            for (cap = 1; cap <= 257; cap += 256)
               for (discard = 0; discard < 2; discard++)
                  for (ratio = 1; ratio <= 2; ratio++)
                  {
                     size_t a, b, frame = channels * (native ? sizeof(float) : sizeof(int16_t));
                     fill(channels);
                     a = epoch_run(channels, native, ratio, hq, cap, 0, discard);
                     b = pipeline_run(channels, native, ratio, hq, cap, discard);
                     if (a != b) printf("pipeline mismatch: channels=%u native=%u hq=%u cap=%u discard=%u ratio=%u reference=%lu pipeline=%lu\n",
                           channels, native, hq, cap, discard, ratio, (unsigned long)a, (unsigned long)b);
                     CHECK(a == b);
                     CHECK(memcmp(native ? (void*)sink_f[0] : (void*)sink_i[0],
                              native ? (void*)sink_f[1] : (void*)sink_i[1], a * frame) == 0);
                     cases++;
                     printf("  case %u/64 channels=%u native=%u hq=%u cap=%u discard=%u ratio=%u\n",
                           cases, channels, native, hq, cap, discard, ratio);
                     fflush(stdout);
                  }
      }
   CHECK(heap_calls == 0);
   printf("pipeline direct: 200000 concurrent frames pass\n");
   printf("pipeline stretch + sinc: %u cases, %u failures, %u guarded heap calls\n", cases, failures, heap_calls);
   return failures != 0;
}
