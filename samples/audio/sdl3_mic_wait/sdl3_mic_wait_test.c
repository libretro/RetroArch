/* The SDL3 driver's capture wait, against a real unbound SDL stream.
 * No recording device is opened: the put callback the driver installs
 * is driven by hand, which is what the device does to it anyway.
 *
 * What is asserted is the bound as much as the wake - the wait is what
 * a close waits on, and the amount it waits for is what decides
 * whether it can return at all, so both are the contract rather than
 * implementation detail. */
#include <SDL3/SDL.h>
#include <string.h>

#include "../../../audio/drivers/sdl3_audio.c"
/* The park the driver waits on, built into this one translation unit
 * like the driver itself. */
#include "../../../libretro-common/rthreads/rthreads.c"
#include "../../../libretro-common/rthreads/retro_eventcount.c"

/* Counted: the capture read is asked every frame and is to say
 * nothing, even failing. */
static unsigned log_lines;
void RARCH_ERR(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
settings_t *config_get_ptr(void) { static settings_t settings; return &settings; }

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", __LINE__, #x); failures++; } } while (0)

#define MIC_RATE     48000
/* A short Audio Latency setting: 5 ms of device period, and a backlog
 * cap below what the capture worker asks for in one slice. */
#define PERIOD_SHORT 240
#define BYTES_SHORT  (PERIOD_SHORT * 2)
/* A long one, to park a wait in for as long as a test needs. */
#define PERIOD_LONG  4800
#define BYTES_LONG   (PERIOD_LONG * 2)
/* What the capture worker asks for every time round its loop:
 * AUDIO_CHUNK_SIZE_NONBLOCKING frames of device audio. */
#define WORKER_SLICE (2048 * 2)

static int16_t tone[PERIOD_LONG];

static void mic_open(sdl3_audio_t *mic, int period_frames)
{
   memset(mic, 0, sizeof(*mic));
   mic->spec.format   = SDL_AUDIO_S16;
   mic->spec.channels = 1;
   mic->spec.freq     = MIC_RATE;
   mic->stream        = SDL_CreateAudioStream(&mic->spec, &mic->spec);
   mic->park_init     = retro_eventcount_init(&mic->park);
   mic->period_frames = period_frames;
   mic->buffer_size   = 1u << 20;
   SDL_SetAudioStreamPutCallback(mic->stream, sdl3_microphone_stream_cb, mic);
}

static void mic_close(sdl3_audio_t *mic)
{
   SDL_DestroyAudioStream(mic->stream);
   retro_eventcount_free(&mic->park);
}

/* Puts a period after a delay, the way the device would. */
static int SDLCALL producer_put(void *data)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)data;
   SDL_Delay(30);
   SDL_PutAudioStreamData(mic->stream, tone, BYTES_LONG);
   return 0;
}

/* Unplugs the device after a delay, the way the event watch would. */
static int SDLCALL producer_remove(void *data)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)data;
   SDL_Delay(30);
   SDL_SetAtomicInt(&mic->device_removed, 1);
   retro_eventcount_notify(&mic->park);
   return 0;
}

int main(void)
{
   sdl3_audio_t mic;
   sdl3_audio_t slow;
   SDL_Thread *thread;
   Uint64 t0;
   Uint64 elapsed;
   size_t got;

   /* The bound is two device periods, clamped at both ends. */
   memset(&mic, 0, sizeof(mic));
   mic.spec.freq     = MIC_RATE;
   CHECK(sdl3_microphone_wait_ms(&mic) == 20);   /* no period yet */
   mic.period_frames = PERIOD_SHORT;             /* 10 ms, floored */
   CHECK(sdl3_microphone_wait_ms(&mic) == 20);
   mic.period_frames = 1200;                     /* 50 ms */
   CHECK(sdl3_microphone_wait_ms(&mic) == 50);
   mic.period_frames = PERIOD_LONG;              /* 200 ms */
   CHECK(sdl3_microphone_wait_ms(&mic) == 200);
   mic.period_frames = 48000;                    /* 2 s, capped */
   CHECK(sdl3_microphone_wait_ms(&mic) == 200);
   mic.spec.freq     = 0;                        /* absurd rate */
   CHECK(sdl3_microphone_wait_ms(&mic) == 200);

   CHECK(sdl3_microphone_wait_readable(NULL, NULL, WORKER_SLICE) == 0);

   mic_open(&mic, PERIOD_SHORT);
   mic_open(&slow, PERIOD_LONG);
   if (!mic.stream || !mic.park_init || !slow.stream || !slow.park_init)
   {
      fprintf(stderr, "SDL stream: %s\n", SDL_GetError());
      return 1;
   }

   /* A device delivering nothing hands the caller back within its own
    * period rather than the playback stall timeout: the capture worker
    * only re-reads its exit flag between these waits, so a close is
    * held for exactly as long as one of them. */
   t0      = SDL_GetTicks();
   got     = sdl3_microphone_wait_readable(NULL, &mic, WORKER_SLICE);
   elapsed = SDL_GetTicks() - t0;
   CHECK(got == 0);
   CHECK(elapsed < 100);

   /* A period is all a wait can promise, whatever the caller asked
    * for: the callback drops the backlog past buffer_size, so waiting
    * out every lap for a whole slice only comes back short anyway. */
   CHECK(SDL_PutAudioStreamData(mic.stream, tone, BYTES_SHORT));
   t0      = SDL_GetTicks();
   got     = sdl3_microphone_wait_readable(NULL, &mic, WORKER_SLICE);
   elapsed = SDL_GetTicks() - t0;
   CHECK(got == BYTES_SHORT);
   CHECK(elapsed < 100);

   /* It parks on the put callback rather than polling: the data lands
    * inside one wait, and the wait ends on it. */
   thread  = SDL_CreateThread(producer_put, "put", &slow);
   t0      = SDL_GetTicks();
   got     = sdl3_microphone_wait_readable(NULL, &slow, WORKER_SLICE);
   elapsed = SDL_GetTicks() - t0;
   SDL_WaitThread(thread, NULL);
   CHECK(got == BYTES_LONG);
   CHECK(elapsed >= 15 && elapsed < 150);

   /* An unplug ends a parked wait, and is answered with nothing rather
    * than with what is left in the stream. */
   SDL_ClearAudioStream(slow.stream);
   thread  = SDL_CreateThread(producer_remove, "remove", &slow);
   t0      = SDL_GetTicks();
   got     = sdl3_microphone_wait_readable(NULL, &slow, WORKER_SLICE);
   elapsed = SDL_GetTicks() - t0;
   SDL_WaitThread(thread, NULL);
   CHECK(got == 0);
   CHECK(elapsed < 150);

   /* And a device already gone is answered without waiting at all. */
   t0 = SDL_GetTicks();
   CHECK(sdl3_microphone_wait_readable(NULL, &slow, WORKER_SLICE) == 0);
   CHECK(SDL_GetTicks() - t0 < 50);

   /* A stream that fails: every read comes back -1, and a second of
    * frames asking says nothing. */
   {
      sdl3_audio_t broken;
      int16_t      frame[PERIOD_SHORT];
      int          dummy_driver, i, rc = 0;
      memset(&broken, 0, sizeof(broken));
      broken.stream = NULL;   /* SDL_GetAudioStreamData fails on it */
      log_lines     = 0;
      for (i = 0; i < 60; i++)
         rc |= sdl3_microphone_read(&dummy_driver, &broken, frame, sizeof(frame)) != -1;
      CHECK(rc == 0);
      CHECK(log_lines == 0);
   }

   mic_close(&slow);
   mic_close(&mic);
   SDL_Quit();
   printf("SDL3 capture wait contract: %u failures\n", failures);
   return failures ? 1 : 0;
}
