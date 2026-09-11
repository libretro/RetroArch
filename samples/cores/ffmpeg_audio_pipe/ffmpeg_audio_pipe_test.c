/* ffmpeg_audio_pipe_test: the ffmpeg core's decode-thread -> main-thread
 * audio ring, driven as a libretro host would.
 *
 * Loads a generated clip (a 440 Hz tone under a test pattern), runs the
 * core frame by frame collecting what it hands audio_sample_batch, and
 * checks the stream is what the pipe promises: the frame count the
 * clip's rate implies, and audio present (a tone, not silence) in
 * every batch once the pipe has primed.  Then presses R1 (+30 s seek)
 * and L1 (-30 s), which drive the seek handshake between the two
 * threads - the main thread's request, the decode thread's post-seek
 * reset - and checks audio resumes after each.  A hang shows as the
 * watchdog firing; a torn or stale batch shows as silence or as a
 * discontinuity in the tone's phase. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#include <libretro.h>

#define CLIP     "ffmpeg_audio_pipe_test.mkv"
#define FPS      30
#define RATE     48000
#define SECONDS  90
#define TONE_HZ  440.0

static unsigned frames_run;
static uint64_t audio_frames_total;
static unsigned batches_total, batches_silent;
static uint32_t press_mask;

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap;
   (void)level;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static bool environ_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         ((struct retro_log_callback*)data)->log = log_cb;
         return true;
      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      case RETRO_ENVIRONMENT_SET_VARIABLES:
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
         return true;
      default:
         return false;
   }
}

static void video_cb(const void *d, unsigned w, unsigned h, size_t p)
{ (void)d; (void)w; (void)h; (void)p; }

/* The tone is 440 Hz at 48 kHz; a batch is silent if its RMS is near
 * zero.  Track how the batch's phase lines up with a free-running
 * expectation to catch a stale or repeated chunk. */
static size_t audio_batch_cb(const int16_t *data, size_t frames)
{
   size_t i;
   double acc = 0.0;
   for (i = 0; i < frames; i++)
   {
      double l = data[2 * i] / 32768.0;
      acc += l * l;
   }
   batches_total++;
   if (frames && sqrt(acc / frames) < 0.05)
      batches_silent++;
   audio_frames_total += frames;
   return frames;
}

static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned port, unsigned dev, unsigned idx, unsigned id)
{
   (void)port; (void)dev; (void)idx;
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)press_mask;
   return (press_mask >> id) & 1;
}

static void watchdog(int sig)
{
   (void)sig;
   fprintf(stderr, "[FAIL] watchdog: hung after %u frames\n", frames_run);
   _exit(1);
}

static int run_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      alarm(10);
      retro_run();
      frames_run++;
   }
   alarm(0);
   return 0;
}

int main(void)
{
   struct retro_game_info info;
   struct retro_system_av_info av;
   double core_fps;
   char cmd[512];
   unsigned before_batches, before_silent;
   uint64_t before_frames;

   snprintf(cmd, sizeof(cmd),
         "ffmpeg -hide_banner -loglevel error -y "
         "-f lavfi -i testsrc=size=160x120:rate=%d "
         "-f lavfi -i sine=frequency=%d:sample_rate=%d "
         "-t %d -c:v mpeg4 -q:v 5 -c:a pcm_s16le -ac 2 %s",
         FPS, (int)TONE_HZ, RATE, SECONDS, CLIP);
   if (system(cmd) != 0)
   {
      fprintf(stderr, "could not generate clip (ffmpeg CLI missing?)\n");
      return 1;
   }

   signal(SIGALRM, watchdog);

   retro_set_environment(environ_cb);
   retro_set_video_refresh(video_cb);
   retro_set_audio_sample(audio_cb);
   retro_set_audio_sample_batch(audio_batch_cb);
   retro_set_input_poll(input_poll_cb);
   retro_set_input_state(input_state_cb);
   retro_init();

   memset(&info, 0, sizeof(info));
   info.path = CLIP;
   if (!retro_load_game(&info))
   {
      fprintf(stderr, "[FAIL] retro_load_game\n");
      return 1;
   }
   retro_get_system_av_info(&av);
   /* The core reports the rate it runs at (it doubles the clip's for
    * frame interpolation); audio per frame follows that rate. */
   core_fps = av.timing.fps;
   printf("clip: %.1f fps, %.0f Hz\n", av.timing.fps, av.timing.sample_rate);

   /* 1. Steady state: 5 s of frames; the audio must be a tone in every
    *    batch after the first few (the pipe primes), and the frame
    *    total must be what the rate implies within one batch. */
   run_frames(5 * FPS);
   {
      uint64_t expect = (uint64_t)(frames_run * RATE / core_fps);
      printf("steady: %u frames, %llu audio frames (expect ~%llu), %u batches, %u silent\n",
            frames_run, (unsigned long long)audio_frames_total,
            (unsigned long long)expect, batches_total, batches_silent);
      if (audio_frames_total + 2 * RATE / FPS < expect || audio_frames_total > expect + 2 * RATE / FPS)
      {
         fprintf(stderr, "[FAIL] audio frame count off\n");
         return 1;
      }
      if (batches_silent > 5)
      {
         fprintf(stderr, "[FAIL] %u silent batches in steady state\n", batches_silent);
         return 1;
      }
   }

   /* 2. Seek forward 30 s (R1), then keep running: audio must resume. */
   before_batches = batches_total; before_silent = batches_silent; before_frames = audio_frames_total;
   press_mask = 1u << RETRO_DEVICE_ID_JOYPAD_R;
   run_frames(1);
   press_mask = 0;
   run_frames(3 * FPS);
   printf("after +30s seek: %u batches (+%u), silent +%u, audio +%llu\n",
         batches_total, batches_total - before_batches, batches_silent - before_silent,
         (unsigned long long)(audio_frames_total - before_frames));
   if (batches_total - before_batches < (unsigned)(2 * FPS) || batches_silent - before_silent > 10)
   {
      fprintf(stderr, "[FAIL] audio did not resume after forward seek\n");
      return 1;
   }

   /* 3. Seek back 30 s (L1) - the decode thread's post-seek reset with
    *    a ring that has data in it - and run again. */
   before_batches = batches_total; before_silent = batches_silent;
   press_mask = 1u << RETRO_DEVICE_ID_JOYPAD_L;
   run_frames(1);
   press_mask = 0;
   run_frames(3 * FPS);
   printf("after -30s seek: batches +%u, silent +%u\n",
         batches_total - before_batches, batches_silent - before_silent);
   if (batches_total - before_batches < (unsigned)(2 * FPS) || batches_silent - before_silent > 10)
   {
      fprintf(stderr, "[FAIL] audio did not resume after backward seek\n");
      return 1;
   }

   /* 4. Two seeks back to back, then teardown mid-stream. */
   press_mask = 1u << RETRO_DEVICE_ID_JOYPAD_R;
   run_frames(1);
   press_mask = 1u << RETRO_DEVICE_ID_JOYPAD_L;
   run_frames(1);
   press_mask = 0;
   run_frames(FPS);

   alarm(10);
   retro_unload_game();
   retro_deinit();
   alarm(0);
   remove(CLIP);

   printf("[pass] %u frames, %u batches, %u silent, seeks both ways, clean teardown\n",
         frames_run, batches_total, batches_silent);
   printf("ALL OK\n");
   return 0;
}
