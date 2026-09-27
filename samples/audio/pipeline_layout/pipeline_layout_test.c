#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_spsc.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include "../../../audio/audio_pipeline_layout.h"

/* Yield inside the spin waits.
 *
 * The four waits below are pure busy loops.  On a machine with more
 * than one core that is what is wanted - the point is to hammer the
 * producer/consumer handoff, not to test a scheduler - but on a
 * uniprocessor it is a livelock: the spinning thread holds the CPU for
 * its whole quantum while the thread it waits on never runs, so
 * progress comes only from preemption and 600000 frames take
 * effectively forever.  Single-core CI runners and containers are
 * common enough to matter; this test used to hang on them, and hung
 * longer the narrower the ring got in frames - width 44 leaves five
 * frames in a 256 byte ring against eleven at width 22, so twice the
 * handoffs and twice the quanta to spend.
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

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int boundaries(void)
{
   audio_pipeline_layout_t q;
   unsigned i;
   size_t origin = SIZE_MAX - 127;
   audio_pipeline_layout_init(&q, 3);
   REQUIRE(audio_pipeline_layout_publish(&q, origin, 63));
   REQUIRE(audio_pipeline_layout_publish(&q, origin + 44, 255));
   REQUIRE(audio_pipeline_layout_publish(&q, origin + 132, 3));
   REQUIRE(audio_pipeline_layout_limit(&q, origin, 176, 256) == 44);
   REQUIRE(q.current_layout == 63);
   REQUIRE(audio_pipeline_layout_limit(&q, origin + 44, 132, 256) == 88);
   REQUIRE(q.current_layout == 255);
   REQUIRE(audio_pipeline_layout_limit(&q, origin + 132, 44, 256) == 44);
   REQUIRE(q.current_layout == 3);
   /* Consumer drops past two boundaries before taking its next span. */
   audio_pipeline_layout_init(&q, 3);
   REQUIRE(audio_pipeline_layout_publish(&q, origin, 63));
   REQUIRE(audio_pipeline_layout_publish(&q, origin + 44, 255));
   REQUIRE(audio_pipeline_layout_publish(&q, origin + 132, 3));
   REQUIRE(audio_pipeline_layout_limit(&q, origin + 88, 88, 256) == 44);
   REQUIRE(q.current_layout == 255);
   /* Full metadata, repeated controls, no audio between edges, index wrap. */
   audio_pipeline_layout_init(&q, 3);
   retro_atomic_size_init(&q.head, SIZE_MAX - 31);
   retro_atomic_size_init(&q.tail, SIZE_MAX - 31);
   for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY; i++)
      REQUIRE(audio_pipeline_layout_publish(&q, origin, i + 4));
   REQUIRE(!audio_pipeline_layout_publish(&q, origin, 999));
   REQUIRE(q.published_layout == AUDIO_PIPELINE_LAYOUT_CAPACITY + 3);
   REQUIRE(audio_pipeline_layout_publish(&q, origin, q.published_layout));
   REQUIRE(audio_pipeline_layout_limit(&q, origin, 0, 256) == 0);
   REQUIRE(q.current_layout == AUDIO_PIPELINE_LAYOUT_CAPACITY + 3);
   REQUIRE(audio_pipeline_layout_publish(&q, origin, 999));
   REQUIRE(audio_pipeline_layout_limit(&q, origin, 22, 256) == 22);
   REQUIRE(q.current_layout == 999);
   audio_pipeline_layout_init(&q, 3);
   REQUIRE(audio_pipeline_layout_limit(&q, 0, 22, 256) == 22);
   REQUIRE(q.current_layout == 3);
   return 0;
}

typedef struct
{
   retro_spsc_t audio;
   audio_pipeline_layout_t layout;
   size_t width;
   unsigned failures;
   bool transport;
} stress_t;

static unsigned token_layout(unsigned token)
{
   static const unsigned layouts[] = { 3, 63, 255 };
   return layouts[(token / 3) % 3];
}

static uint32_t token_control(unsigned token)
{
   return (token / 11) % 2
      ? AUDIO_PIPELINE_STRETCH | (16384 * (1 + (token / 5) % 128)) : 65536;
}

static int transport_boundaries(void)
{
   audio_pipeline_layout_t q;
   unsigned i;
   size_t origin = SIZE_MAX - 127;
   audio_pipeline_layout_init(&q, 3);
   REQUIRE(q.current_control == 65536 && !q.reset_serial);
   REQUIRE(!audio_pipeline_layout_publish_transport(&q, origin, 63, 16383, true, true));
   REQUIRE(!audio_pipeline_layout_publish_transport(&q, origin, 63, 2097153, true, false));
   REQUIRE(!retro_atomic_load_relaxed_size(&q.head));
   REQUIRE(q.published_layout == 3 && q.published_control == 65536);
   REQUIRE(audio_pipeline_layout_publish_transport(&q, origin, 63, 16384, true, true));
   REQUIRE(audio_pipeline_layout_publish_transport(&q, origin + 44, 255, 2097152, true, false));
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin, 132, 256) == 44);
   REQUIRE(q.current_layout == 63 && q.current_control == (AUDIO_PIPELINE_STRETCH | 16384));
   REQUIRE(q.reset_serial == 1);
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin + 44, 88, 256) == 88);
   REQUIRE(q.current_layout == 255 && q.current_control == (AUDIO_PIPELINE_STRETCH | 2097152));
   /* Layout-only callers preserve the transport request. */
   REQUIRE(audio_pipeline_layout_publish(&q, origin + 132, 3));
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin + 132, 44, 256) == 44);
   REQUIRE(q.current_layout == 3 && q.current_control == (AUDIO_PIPELINE_STRETCH | 2097152));
   /* Even-numbered resets at one position cannot cancel each other. */
   q.reset_serial = UINT32_MAX - 1;
   REQUIRE(audio_pipeline_layout_publish_transport(&q, origin + 176, 3, 1, false, true));
   REQUIRE(audio_pipeline_layout_publish_transport(&q, origin + 176, 3, 1, false, true));
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin + 220, 0, 256) == 0);
   REQUIRE(q.reset_serial == 0 && q.current_control == 65536);
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin + 220, 22, 256) == 22);
   REQUIRE(q.reset_serial == 0);
   /* Inactive tempo changes do not consume metadata slots. */
   for (i = 0; i < 100; i++)
      REQUIRE(audio_pipeline_layout_publish_transport(&q, origin + 220, 3, i, false, false));
   REQUIRE(retro_atomic_load_relaxed_size(&q.head) == retro_atomic_load_relaxed_size(&q.tail));
   for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY; i++)
      REQUIRE(audio_pipeline_layout_publish_transport(&q, origin + 220, 3, 65536, false, true));
   REQUIRE(!audio_pipeline_layout_publish_transport(&q, origin + 220, 63, 131072, true, true));
   REQUIRE(q.published_layout == 3 && q.published_control == 65536);
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin + 220, 0, 256) == 0);
   REQUIRE(q.reset_serial == AUDIO_PIPELINE_LAYOUT_CAPACITY);
   REQUIRE(audio_pipeline_layout_publish_transport(&q, origin + 220, 63, 131072, true, true));
   REQUIRE(audio_pipeline_layout_limit_transport(&q, origin + 220, 22, 256) == 22);
   REQUIRE(q.reset_serial == AUDIO_PIPELINE_LAYOUT_CAPACITY + 1);
   REQUIRE(q.current_layout == 63 && q.current_control == (AUDIO_PIPELINE_STRETCH | 131072));
   return 0;
}

static void consume(void *arg)
{
   stress_t *s = (stress_t*)arg;
   unsigned token = 0;
   unsigned spin  = 0;
   uint8_t data[256];
   while (token < 100000)
   {
      size_t bytes = retro_spsc_read_avail(&s->audio);
      size_t f, i;
      if (!bytes) { STRESS_YIELD(spin); continue; }
      bytes = s->transport
         ? audio_pipeline_layout_limit_transport(&s->layout,
               retro_atomic_load_relaxed_size(&s->audio.tail), bytes, s->audio.capacity)
         : audio_pipeline_layout_limit(&s->layout,
               retro_atomic_load_relaxed_size(&s->audio.tail), bytes, s->audio.capacity);
      if (!bytes || bytes % s->width) { s->failures++; abort(); }
      if (retro_spsc_read(&s->audio, data, bytes) != bytes) abort();
      for (f = 0; f < bytes / s->width; f++, token++)
      {
         if (s->layout.current_layout != token_layout(token)) s->failures++;
         if (s->transport && (s->layout.current_control != token_control(token)
                  || s->layout.reset_serial != token / 17 + 1)) s->failures++;
         for (i = 0; i < s->width; i++)
            if (data[f * s->width + i] != (uint8_t)(token * 13 + i)) s->failures++;
      }
   }
}

static int stress(size_t width, bool transport)
{
   stress_t s;
   sthread_t *thread;
   unsigned token;
   unsigned spin = 0;
   size_t i, origin = SIZE_MAX - 127;
   uint8_t frame[44];
   REQUIRE(retro_spsc_init(&s.audio, 256));
   s.width = width;
   s.transport = transport;
   s.failures = 0;
   audio_pipeline_layout_init(&s.layout, 3);
   retro_atomic_size_init(&s.audio.head, origin);
   retro_atomic_size_init(&s.audio.tail, origin);
   s.audio.cached_head = s.audio.cached_tail = origin;
   thread = sthread_create(consume, &s);
   REQUIRE(thread != NULL);
   for (token = 0; token < 100000; token++)
   {
      for (i = 0; i < width; i++) frame[i] = (uint8_t)(token * 13 + i);
      if (transport)
      {
         uint32_t control = token_control(token);
         while (!audio_pipeline_layout_publish_transport(&s.layout,
                  retro_atomic_load_relaxed_size(&s.audio.head), token_layout(token),
                  control & AUDIO_PIPELINE_TEMPO_MASK,
                  (control & AUDIO_PIPELINE_STRETCH) != 0, token % 17 == 0))
            STRESS_YIELD(spin);
      }
      else while (!audio_pipeline_layout_publish(&s.layout,
               retro_atomic_load_relaxed_size(&s.audio.head), token_layout(token)))
         STRESS_YIELD(spin);
      while (!retro_spsc_write_frames(&s.audio, frame, 1, width))
         STRESS_YIELD(spin);
   }
   sthread_join(thread);
   REQUIRE(!s.failures);
   REQUIRE(!retro_spsc_read_avail(&s.audio));
   retro_spsc_free(&s.audio);
   return 0;
}

int main(void)
{
   if (boundaries() || transport_boundaries() || stress(22, false) || stress(44, false)
         || stress(4, true) || stress(8, true) || stress(22, true) || stress(44, true)) return 1;
   printf("layout epochs: boundaries, saturation, reset, cursor wrap; 600000 concurrent frames pass (%u bytes metadata)\n",
         (unsigned)sizeof(audio_pipeline_layout_t));
   return 0;
}
