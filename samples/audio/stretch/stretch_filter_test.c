/* Copyright (C) 2026 - The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_pipeline_stretch.h"
#include "../../../audio/audio_speed_lpf.h"

static unsigned failures, heap_calls;
static int guarded;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void*, size_t);
void __real_free(void*);
void *__wrap_malloc(size_t n) { if (guarded) heap_calls++; return __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { if (guarded) heap_calls++; return __real_calloc(n,z); }
void *__wrap_realloc(void *p, size_t n) { if (guarded) heap_calls++; return __real_realloc(p,n); }
void __wrap_free(void *p) { if (guarded) heap_calls++; __real_free(p); }
#define CHECK(x) do { if (!(x)) { if (failures < 20) printf("FAIL %u: %s\n", (unsigned)__LINE__, #x); failures++; } } while (0)
#define N 12288
#define CAP (N * 4)
static float source_f[N*11], sink_f[2][CAP*11], oracle_f[N*11];
static int16_t source_i[N*11], sink_i[2][CAP*11], oracle_i[N*11];
static const uint32_t cutoffs[] = {0, 1000, 200, 3000, 0, 0};

static size_t run(unsigned channels, bool floating, bool active,
      bool reset, unsigned fragmented, unsigned capacity)
{
   retro_spsc_t ring;
   audio_pipeline_layout_t metadata;
   audio_pipeline_stretch_t *stage;
   struct audio_pipeline_stretch_block block, retry;
   union { float f[257*11]; int16_t i[257*11]; } output;
   const void *source = floating ? (const void*)source_f : (const void*)source_i;
   void *sink = floating ? (void*)sink_f[fragmented] : (void*)sink_i[fragmented];
   size_t frame = channels * (floating ? sizeof(float) : sizeof(int16_t));
   size_t total = 0, used = 0, iterations = 0, origin = SIZE_MAX - 127;
   unsigned segment;
   bool complete = false;
   CHECK(retro_spsc_init(&ring, N * frame));
   retro_atomic_size_init(&ring.head, origin);
   retro_atomic_size_init(&ring.tail, origin);
   ring.cached_head = ring.cached_tail = origin;
   audio_pipeline_layout_init(&metadata, 3);
   stage = audio_pipeline_stretch_new(48000, channels, floating, 1,
         &ring, &metadata, &output, capacity);
   if (!stage) abort();
   for (segment = 0; segment < 6; segment++)
   {
      CHECK(audio_pipeline_layout_publish_processing(&metadata,
               origin + segment * 2048 * frame, reset && segment >= 3 ? 7 : 3,
               segment == 2 ? 32768 : 90112, active && segment < 4,
               reset && segment == 2, cutoffs[segment]));
      CHECK(retro_spsc_write_frames(&ring,
               (const char*)source + segment * 2048 * frame, 2048, frame) == 2048);
   }
   guarded = 1;
   while (!complete)
   {
      size_t before = retro_atomic_load_relaxed_size(&ring.tail), take;
      if (++iterations > 1000000) abort();
      if (used == N)
         CHECK(audio_pipeline_stretch_finish(stage, fragmented ? 17 : 257, &block, &complete));
      else
         CHECK(audio_pipeline_stretch_next(stage, fragmented ? 71 : 2048,
                  fragmented ? 17 : 257, &block));
      used += block.input_used;
      CHECK(retro_atomic_load_relaxed_size(&ring.tail) - before == block.input_used * frame);
      CHECK(total + block.frames < CAP);
      if (fragmented && block.frames)
      {
         size_t offered = block.frames;
         const void *ptr = block.data;
         memcpy((char*)sink + total * frame, ptr, offered * frame);
         CHECK(audio_pipeline_stretch_consume(stage, 0));
         CHECK(!audio_pipeline_stretch_consume(stage, offered + 1));
         CHECK(audio_pipeline_stretch_next(stage, 0, 0, &retry));
         CHECK(!retry.frames && !retry.input_used);
         CHECK(audio_pipeline_stretch_next(stage, 0, 257, &retry));
         CHECK(!retry.input_used && retry.data == ptr && retry.frames >= offered);
         CHECK(memcmp((char*)sink + total * frame, retry.data, offered * frame) == 0);
         block = retry;
      }
      take = block.frames;
      if (fragmented && take > 3) take = 3;
      if (take) memcpy((char*)sink + total * frame, block.data, take * frame);
      before = retro_atomic_load_relaxed_size(&ring.tail);
      CHECK(audio_pipeline_stretch_consume(stage, take));
      CHECK(retro_atomic_load_relaxed_size(&ring.tail) - before == (block.passthrough ? take * frame : 0));
      if (block.passthrough) used += take;
      total += take;
   }
   CHECK(used == N && !retro_spsc_read_avail(&ring));
   guarded = 0;
   audio_pipeline_stretch_free(stage); retro_spsc_free(&ring);
   return total;
}

static void oracle(unsigned channels, bool floating)
{
   audio_speed_lpf_t s;
   unsigned segment;
   size_t frame = channels * (floating ? sizeof(float) : sizeof(int16_t));
   void *output = floating ? (void*)oracle_f : (void*)oracle_i;
   const void *source = floating ? (const void*)source_f : (const void*)source_i;
   memcpy(output, source, N * frame);
   CHECK(audio_speed_lpf_init(&s, 48000, channels, floating));
   for (segment = 0; segment < 6; segment++)
   {
      if (segment < 5)
         CHECK(audio_speed_lpf_set(&s, cutoffs[segment] != 0,
                  cutoffs[segment] ? cutoffs[segment] : (segment ? cutoffs[segment-1] : 20)));
      CHECK(audio_speed_lpf_process(&s, (char*)output + segment * 2048 * frame, 2048));
   }
   CHECK(audio_speed_lpf_quiescent(&s));
   CHECK(memcmp(output, floating ? (void*)sink_f[0] : (void*)sink_i[0], N * frame) == 0);
}

static void metadata_contracts(void)
{
   audio_pipeline_layout_t q;
   unsigned j;
   audio_pipeline_layout_init(&q, 3);
   for (j = 1; j <= AUDIO_PIPELINE_LAYOUT_CAPACITY; j++)
      CHECK(audio_pipeline_layout_publish_cutoff(&q, j, j*100));
   CHECK(!audio_pipeline_layout_publish_cutoff(&q, 65, 123));
   CHECK(q.published_cutoff == 6400);
   CHECK(audio_pipeline_layout_publish_cutoff(&q, 65, 6400));
   audio_pipeline_layout_limit_transport(&q, 64, 0, 128);
   CHECK(q.current_cutoff == 6400);
   CHECK(audio_pipeline_layout_publish_transport(&q, 65, 7, 131072, true, true));
   audio_pipeline_layout_limit_transport(&q, 65, 0, 128);
   CHECK(q.current_cutoff == 6400 && q.reset_serial == 1 && q.current_layout == 7);
   CHECK(audio_pipeline_layout_publish_processing(&q, 66, 3, 32768, true, false, 1200));
   audio_pipeline_layout_limit_transport(&q, 66, 0, 128);
   CHECK(q.current_cutoff == 1200 && q.current_layout == 3
         && q.current_control == (32768 | AUDIO_PIPELINE_STRETCH));
}

static void discard_filter(bool floating)
{
   retro_spsc_t ring;
   audio_pipeline_layout_t q;
   audio_pipeline_stretch_t *s;
   struct audio_pipeline_stretch_block block, retry;
   union { float f[32]; int16_t i[32]; } data, output;
   size_t sample = floating ? sizeof(float) : sizeof(int16_t);
   unsigned j;
   for (j = 0; j < 32; j++)
      if (floating) data.f[j] = (j & 1) ? 0.5f : -0.5f;
      else data.i[j] = (j & 1) ? 16384 : -16384;
   CHECK(retro_spsc_init(&ring, 256));
   audio_pipeline_layout_init(&q, 3);
   CHECK(audio_pipeline_layout_publish_cutoff(&q, 0, 20));
   s = audio_pipeline_stretch_new(48000, 1, floating, 1, &ring, &q, &output, 16);
   if (!s) abort();
   CHECK(retro_spsc_write_frames(&ring, &data, 32, sample) == 32);
   guarded = 1;
   CHECK(audio_pipeline_stretch_next(s, 16, 16, &block));
   CHECK(block.frames == 16 && block.input_used == 16 && !block.passthrough);
   CHECK(audio_pipeline_stretch_consume(s, 3));
   CHECK(!audio_pipeline_stretch_discard(s, 17));
   CHECK(audio_pipeline_stretch_next(s, 0, 16, &retry));
   CHECK(retry.data == (const char*)block.data + 3*sample && retry.frames == 13);
   CHECK(audio_pipeline_stretch_discard(s, 0));
   CHECK(audio_pipeline_stretch_next(s, 16, 16, &retry));
   CHECK(retry.reset_serial != block.reset_serial && retry.frames == 16);
   CHECK(memcmp(retry.data, (const char*)&data + 16*sample, sample) == 0);
   CHECK(audio_pipeline_stretch_consume(s, 16));
   guarded = 0;
   audio_pipeline_stretch_free(s); retro_spsc_free(&ring);
}

static void readiness_controls(void)
{
   retro_spsc_t ring;
   audio_pipeline_layout_t q;
   audio_pipeline_stretch_t *s;
   struct audio_pipeline_stretch_block block;
   float output[32];
   unsigned event;
   CHECK(retro_spsc_init(&ring, 256));
   audio_pipeline_layout_init(&q, 3);
   s = audio_pipeline_stretch_new(48000, 2, true, 1, &ring, &q, output, 16);
   if (!s) abort();
   CHECK(audio_pipeline_stretch_needs_input(s));
   guarded = 1;
   for (event = 0; event < 4; event++)
   {
      /* Retired metadata can precede application to retained stream state. */
      CHECK(audio_pipeline_layout_publish_processing(&q, 0,
               event >= 2 ? 1 : 3, 131072, true, event == 3,
               event >= 1 ? 1000 : 0));
      audio_pipeline_layout_limit_transport(&q, 0, 0, ring.capacity);
      CHECK(!audio_pipeline_stretch_needs_input(s));
      CHECK(audio_pipeline_stretch_next(s, 0, 16, &block));
      CHECK(!block.frames);
      CHECK(audio_pipeline_stretch_needs_input(s));
   }
   guarded = 0;
   audio_pipeline_stretch_free(s);
   retro_spsc_free(&ring);
}

int main(void)
{
   unsigned channels, mode, active, reset, capacity, j, cases = 0;
   metadata_contracts();
   readiness_controls();
   discard_filter(false); discard_filter(true);
   for (channels = 2; channels <= 11; channels += 9)
   {
      for (j = 0; j < N*channels; j++)
      {
         source_i[j] = (int16_t)((int)((j * 7919u) % 60000) - 30000);
         source_f[j] = source_i[j] / 32768.0f;
      }
      for (mode = 0; mode < 2; mode++)
         for (active = 0; active < 2; active++)
            for (reset = 0; reset < 2; reset++)
               for (capacity = 1; capacity <= 257; capacity += 256)
               {
                  size_t a = run(channels, mode != 0, active != 0, reset != 0, 0, capacity);
                  size_t b = run(channels, mode != 0, active != 0, reset != 0, 1, capacity);
                  size_t frame = channels * (mode ? sizeof(float) : sizeof(int16_t));
                  CHECK(a == b);
                  CHECK(memcmp(mode ? (void*)sink_f[0] : (void*)sink_i[0],
                           mode ? (void*)sink_f[1] : (void*)sink_i[1], a * frame) == 0);
                  if (!active && !reset) { CHECK(a == N); oracle(channels, mode != 0); }
                  cases++;
               }
   }
   CHECK(heap_calls == 0);
   printf("pipeline native LPF: %u cases, %u failures, %u guarded heap calls\n", cases, failures, heap_calls);
   return failures != 0;
}
