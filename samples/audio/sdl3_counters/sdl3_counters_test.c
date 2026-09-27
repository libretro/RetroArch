/* The SDL3 audio driver's underruns() accounting, against SDL's own
 * dummy device.
 *
 * underruns() counts the periods the device zero-filled for want of
 * audio, since init. Three things make that true, and each is under
 * test here:
 *
 * - it is the output stream's get callback that counts. The capture
 *   stream shares the wake, and a put callback carries the audio the
 *   microphone just delivered in the same argument, so counting in
 *   the shared function turns a working microphone into a stream of
 *   underruns on its own context.
 * - a period the queue covered is not a shortfall. Feeding at the
 *   frame rate should leave the count alone; starving the driver
 *   should move it.
 * - the count is per driver instance, not per stream. The frontend
 *   compares it against what it last saw and reads any change as
 *   fresh silence, so a reopen that reset it would cost the pipeline
 *   a pass every time SDL migrated the device.
 *
 * Builds audio/drivers/sdl3_audio.c itself: the subject is the
 * driver's own accounting, and the callbacks that do it are static.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <boolean.h>

#include "../../../audio/drivers/sdl3_audio.c"

/* Only the microphone open path asks, and nothing here opens one. */
bool verbosity_is_enabled(void) { return false; }

#define OUT_RATE    48000
#define CHANNELS    2
#define FPS         60
#define LATENCY_MS  64

static unsigned failures;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

static void nap_ns(long ns)
{
   struct timespec ts;
   ts.tv_sec  = ns / 1000000000L;
   ts.tv_nsec = ns % 1000000000L;
   nanosleep(&ts, NULL);
}

static size_t count_of(const sdl3_audio_t *ctx)
{
   return retro_atomic_load_acquire_size(&ctx->underruns);
}

/* A context with the parts the callbacks touch, and no device. */
static bool ctx_open(sdl3_audio_t *ctx, int channels)
{
   memset(ctx, 0, sizeof(*ctx));
   ctx->spec.format   = SDL_AUDIO_S16;
   ctx->spec.channels = channels;
   ctx->spec.freq     = OUT_RATE;
   ctx->stream        = SDL_CreateAudioStream(&ctx->spec, &ctx->spec);
   ctx->lock          = SDL_CreateMutex();
   ctx->cond          = SDL_CreateCondition();
   ctx->buffer_size   = 1u << 20;
   return ctx->stream && ctx->lock && ctx->cond;
}

static void ctx_close(sdl3_audio_t *ctx)
{
   SDL_DestroyAudioStream(ctx->stream);
   SDL_DestroyCondition(ctx->cond);
   SDL_DestroyMutex(ctx->lock);
}

/* What each callback does to the count, driven directly. */
static void test_callbacks(void)
{
   sdl3_audio_t out;
   sdl3_audio_t mic;
   int16_t      block[256];

   memset(block, 0, sizeof(block));

   if (!ctx_open(&out, CHANNELS) || !ctx_open(&mic, 1))
   {
      printf("FAIL: no SDL audio stream: %s\n", SDL_GetError());
      failures++;
      return;
   }

   /* A callback the queue covered is not a shortfall. */
   sdl3_audio_out_stream_cb(&out, out.stream, 0, 4096);
   CHECK(count_of(&out) == 0, "%u underruns for a callback with nothing outstanding",
         (unsigned)count_of(&out));

   sdl3_audio_out_stream_cb(&out, out.stream, 512, 4096);
   sdl3_audio_out_stream_cb(&out, out.stream, 512, 4096);
   CHECK(count_of(&out) == 2, "%u underruns for two short callbacks",
         (unsigned)count_of(&out));

   /* The capture stream shares the wake and must not reach the
    * count: every put here is the microphone delivering audio. */
   SDL_SetAudioStreamPutCallback(mic.stream, sdl3_microphone_stream_cb, &mic);
   SDL_PutAudioStreamData(mic.stream, block, sizeof(block));
   SDL_PutAudioStreamData(mic.stream, block, sizeof(block));
   SDL_PutAudioStreamData(mic.stream, block, sizeof(block));
   CHECK(count_of(&mic) == 0, "%u underruns counted from microphone puts",
         (unsigned)count_of(&mic));

   ctx_close(&mic);
   ctx_close(&out);
}

/* What the device does to the count, at the device's own pace.
 * False when no device would open and nothing was measured. */
static bool test_device(void)
{
   const audio_driver_t *d = &audio_sdl3;
   unsigned  out_rate      = OUT_RATE;
   size_t    per_frame     = OUT_RATE / FPS;
   size_t    block         = per_frame * CHANNELS * sizeof(int16_t);
   size_t    i, under_open, under_fed, under_starved, under_reopen;
   int16_t  *buf;
   void     *ctx;

   if (!(ctx = d->init(NULL, OUT_RATE, LATENCY_MS, &out_rate)))
   {
      printf("SKIP: no SDL3 audio device would open\n");
      return false;
   }

   if (!d->underruns)
   {
      printf("FAIL: the driver reports no underruns\n");
      failures++;
      d->free(ctx);
      return true;
   }

   d->start(ctx, false);
   d->set_nonblock_state(ctx, false);

   if (!(buf = (int16_t*)calloc(per_frame * CHANNELS, sizeof(int16_t))))
   {
      d->free(ctx);
      return true;
   }

   under_open = d->underruns(ctx);
   printf("at open:       %u underruns\n", (unsigned)under_open);
   /* init primes the stream before it resumes the device, so nothing
    * has gone short yet. */
   CHECK(under_open == 0, "%u underruns before the device asked for anything",
         (unsigned)under_open);

   /* A second of audio, handed over one frame at a time. */
   for (i = 0; i < FPS; i++)
   {
      d->write(ctx, buf, block);
      nap_ns(1000000000L / FPS);
   }

   under_fed = d->underruns(ctx) - under_open;
   printf("fed for 1 s:   %u underruns\n", (unsigned)under_fed);
   /* A wide band: the harness's own pacing is a sleep loop, not a
    * clock, and a slow runner can starve the device on its own. What
    * it rules out is a count that runs at the device's period. */
   CHECK(under_fed <= FPS / 2, "%u underruns while the driver was being fed",
         (unsigned)under_fed);

   /* Now starve it. The queue drains, and every period after that is
    * one the device asked for and did not get. */
   under_fed += under_open;
   nap_ns(500000000L);

   under_starved = d->underruns(ctx) - under_fed;
   printf("starved 0.5 s: %u underruns\n", (unsigned)under_starved);
   CHECK(under_starved > 0, "no underruns counted while the driver was starved");

   /* A device migration opens a new stream under the same driver;
    * what the frontend has already seen must still hold. */
   under_starved += under_fed;
   if (sdl3_audio_reopen_default((sdl3_audio_t*)ctx))
   {
      under_reopen = d->underruns(ctx);
      printf("after reopen:  %u underruns\n", (unsigned)under_reopen);
      CHECK(under_reopen >= under_starved,
            "the count fell from %u to %u across a reopen",
            (unsigned)under_starved, (unsigned)under_reopen);
   }
   else
      printf("after reopen:  skipped, no default device to reopen on\n");

   d->stop(ctx);
   free(buf);
   d->free(ctx);
   return true;
}

int main(void)
{
   bool measured;

   /* No hardware in a harness; SDL's dummy device paces on the host
    * clock, which is the clock this measures against. Set before any
    * call into SDL, which takes the driver name once. */
   SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");

   test_callbacks();
   measured = test_device();

   SDL_Quit();

   if (failures)
   {
      printf("sdl3 counters: %u failure%s\n", failures, failures == 1 ? "" : "s");
      return 1;
   }
   if (!measured)
   {
      printf("sdl3 counters: callbacks only, no device to pace against\n");
      return 0;
   }
   printf("sdl3 counters: silence is counted where it happens, and only there\n");
   return 0;
}
