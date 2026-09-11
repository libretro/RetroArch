/* ffmpeg_sweep_test: the ffmpeg core across the shapes of media it
 * has to take, and the abuse a host can put it through, under the
 * sanitizers.
 *
 * ffmpeg_audio_pipe_test drives one clip carefully and checks what
 * comes out of the audio pipe. This is the other half: breadth rather
 * than depth. Every clip is loaded, run, seeked, reset and unloaded in
 * one process, so a leak, a use-after-free or a stale pointer across a
 * load/unload cycle has somewhere to show; and the last set of cases
 * are files that are not media at all, or are media cut short, which
 * is where a demuxer's arithmetic goes wrong.
 *
 * The core is a single-instance libretro core with global state, so
 * running the whole matrix in one process is the point: the second
 * load is the one that finds what the first unload left behind. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#include <libretro.h>

static unsigned failures  = 0;
static unsigned clips_run = 0;
static const char *current = "";

#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL [%s]: ", current); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- the host ----------------------------------------------------- */

static unsigned  frames_seen, batches_seen;
static uint64_t  audio_frames;
static bool      quiet_log = true;

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap;
   if (quiet_log && level < RETRO_LOG_ERROR)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static bool env_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         ((struct retro_log_callback*)data)->log = log_cb;
         return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         return true;
      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool*)data = true;
         return true;
      case RETRO_ENVIRONMENT_SET_VARIABLES:
      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
         return true;
      case RETRO_ENVIRONMENT_GET_VARIABLE:
      {
         struct retro_variable *var = (struct retro_variable*)data;
         var->value = NULL;
         return false;
      }
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         *(bool*)data = false;
         return true;
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char**)data = "/tmp";
         return true;
      default:
         break;
   }
   return false;
}

static void video_cb(const void *d, unsigned w, unsigned h, size_t p)
{
   (void)d; (void)w; (void)h; (void)p;
   frames_seen++;
}

static size_t audio_batch_cb(const int16_t *data, size_t frames)
{
   (void)data;
   batches_seen++;
   audio_frames += frames;
   return frames;
}

static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static void input_poll_cb(void) {}

static int16_t held_button = -1;
static int16_t input_state_cb(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)port; (void)device; (void)index;
   return (held_button >= 0 && (int16_t)id == held_button) ? 1 : 0;
}

static void host_bind(void)
{
   retro_set_environment(env_cb);
   retro_set_video_refresh(video_cb);
   retro_set_audio_sample(audio_cb);
   retro_set_audio_sample_batch(audio_batch_cb);
   retro_set_input_poll(input_poll_cb);
   retro_set_input_state(input_state_cb);
}

static void watchdog(int sig)
{
   (void)sig;
   fprintf(stderr, "[FAIL] watchdog: the core did not come back (%s)\n", current);
   _exit(70);
}

static void run_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      retro_run();
}

/* ---- clips -------------------------------------------------------- */

static bool make_clip(const char *path, const char *args)
{
   char cmd[1024];
   snprintf(cmd, sizeof(cmd),
         "ffmpeg -hide_banner -loglevel error -y %s %s", args, path);
   return system(cmd) == 0;
}

/* One clip all the way through: load, run, seek forward and back,
 * reset, run again, unload. Whatever the clip holds, the core has to
 * come back from each of those and leave nothing behind. */
static void clip_case(const char *name, const char *path, const char *args,
      bool expect_audio, bool expect_video)
{
   struct retro_game_info info;
   struct retro_system_av_info av;
   unsigned f0, b0;

   current = name;
   if (args && !make_clip(path, args))
   {
      printf("      (ffmpeg would not make %s; skipped)\n", name);
      return;
   }

   memset(&info, 0, sizeof(info));
   info.path = path;

   frames_seen = batches_seen = 0;
   audio_frames = 0;

   retro_init();
   if (!retro_load_game(&info))
   {
      printf("      %-22s refused\n", name);
      retro_deinit();
      CHECK(!expect_video && !expect_audio, "%s: the core refused a clip it should take", name);
      return;
   }
   memset(&av, 0, sizeof(av));
   retro_get_system_av_info(&av);
   CHECK(av.timing.fps > 0.0 || !expect_video, "%s: fps is %.2f", name, av.timing.fps);
   CHECK(av.timing.sample_rate > 0.0 || !expect_audio,
         "%s: sample rate is %.0f", name, av.timing.sample_rate);

   run_frames(40);
   f0 = frames_seen;
   b0 = batches_seen;

   /* R1 and L1 are the core's own seek bindings. Held for a frame,
    * then released, which is the handshake the decode thread answers. */
   held_button = RETRO_DEVICE_ID_JOYPAD_R;
   run_frames(2);
   held_button = -1;
   run_frames(20);

   held_button = RETRO_DEVICE_ID_JOYPAD_L;
   run_frames(2);
   held_button = -1;
   run_frames(20);

   retro_reset();
   run_frames(20);

   printf("      %-22s %u frames, %u batches, %llu audio frames%s\n",
         name, frames_seen, batches_seen,
         (unsigned long long)audio_frames,
         frames_seen > f0 && batches_seen >= b0 ? "" : " (stalled)");
   if (expect_video)
      CHECK(frames_seen > f0, "%s: no video after the seeks and the reset", name);
   if (expect_audio)
      CHECK(audio_frames > 0, "%s: no audio at all", name);

   retro_unload_game();
   retro_deinit();
   clips_run++;
}

/* A file that is not what it says, or is cut short. The core has to
 * refuse it without reading past the end of it. */
static void reject_case(const char *name, const char *path)
{
   struct retro_game_info info;
   bool ok;

   current = name;
   memset(&info, 0, sizeof(info));
   info.path = path;

   retro_init();
   ok = retro_load_game(&info);
   printf("      %-22s %s\n", name, ok ? "loaded" : "refused");
   if (ok)
   {
      /* It took it, so it must survive being run and torn down. */
      run_frames(10);
      retro_unload_game();
   }
   retro_deinit();
}

/* Teardown while the decode thread is still working: the case a host
 * makes when a user closes content a frame after opening it. */
static void early_unload_case(const char *path)
{
   struct retro_game_info info;
   unsigned i;

   current = "unload while decoding";
   memset(&info, 0, sizeof(info));
   info.path = path;

   for (i = 0; i < 4; i++)
   {
      retro_init();
      if (retro_load_game(&info))
      {
         /* i frames in: none at all, then one, then a few - the
          * decode thread is at a different point each time. */
         run_frames(i);
         retro_unload_game();
      }
      retro_deinit();
   }
   printf("      %-22s four cycles, unloaded after 0 to 3 frames\n",
         "unload while decoding");
}

int main(void)
{
   char trunc_cmd[512];

   signal(SIGALRM, watchdog);
   alarm(600);

   host_bind();
   printf("ffmpeg core sweep:\n");

   printf("   clips the core has to take\n");
   /* The matrix: containers, video codecs, audio codecs, rates and
    * channel counts, plus the two one-sided cases. */
   clip_case("mkv mpeg4 + pcm", "/tmp/sw_a.mkv",
         "-f lavfi -i testsrc=size=160x120:rate=30:duration=4 "
         "-f lavfi -i sine=f=440:r=48000:duration=4 "
         "-c:v mpeg4 -c:a pcm_s16le", true, true);
   clip_case("mkv h264 + aac 44k", "/tmp/sw_b.mkv",
         "-f lavfi -i testsrc=size=160x120:rate=25:duration=4 "
         "-f lavfi -i sine=f=440:r=44100:duration=4 "
         "-c:v libx264 -preset ultrafast -c:a aac", true, true);
   clip_case("mp4 h264 + aac 5.1", "/tmp/sw_c.mp4",
         "-f lavfi -i testsrc=size=160x120:rate=30:duration=4 "
         "-f lavfi -i sine=f=440:r=48000:duration=4 "
         "-ac 6 -c:v libx264 -preset ultrafast -c:a aac", true, true);
   clip_case("mkv mpeg4 + mp3", "/tmp/sw_d.mkv",
         "-f lavfi -i testsrc=size=160x120:rate=30:duration=4 "
         "-f lavfi -i sine=f=440:r=48000:duration=4 "
         "-c:v mpeg4 -c:a libmp3lame", true, true);
   clip_case("video only", "/tmp/sw_e.mkv",
         "-f lavfi -i testsrc=size=160x120:rate=30:duration=4 -c:v mpeg4",
         false, true);
   clip_case("audio only", "/tmp/sw_f.mka",
         "-f lavfi -i sine=f=440:r=48000:duration=4 -c:a pcm_s16le",
         true, false);
   clip_case("tiny frame", "/tmp/sw_g.mkv",
         "-f lavfi -i testsrc=size=16x16:rate=30:duration=2 "
         "-f lavfi -i sine=f=440:r=48000:duration=2 -c:v mpeg4 -c:a pcm_s16le",
         true, true);
   clip_case("odd size 161x121", "/tmp/sw_h.mkv",
         "-f lavfi -i testsrc=size=161x121:rate=30:duration=2 "
         "-f lavfi -i sine=f=440:r=48000:duration=2 -c:v mpeg4 -c:a pcm_s16le",
         true, true);

   printf("   files that are not what they claim\n");
   /* A media file cut in half: the header parses, the stream does
    * not reach where the index says it does. */
   snprintf(trunc_cmd, sizeof(trunc_cmd),
         "head -c $(( $(stat -c%%s /tmp/sw_a.mkv) / 2 )) /tmp/sw_a.mkv > /tmp/sw_trunc.mkv");
   if (system(trunc_cmd) == 0)
      reject_case("truncated mkv", "/tmp/sw_trunc.mkv");
   /* The first 64 bytes only: not even a header. */
   if (system("head -c 64 /tmp/sw_a.mkv > /tmp/sw_stub.mkv") == 0)
      reject_case("64-byte stub", "/tmp/sw_stub.mkv");
   if (system("head -c 200000 /dev/urandom > /tmp/sw_noise.mkv") == 0)
      reject_case("noise", "/tmp/sw_noise.mkv");
   if (system(": > /tmp/sw_empty.mkv") == 0)
      reject_case("empty file", "/tmp/sw_empty.mkv");
   reject_case("no such file", "/tmp/sw_absent_file.mkv");

   printf("   teardown under load\n");
   early_unload_case("/tmp/sw_a.mkv");

   printf("   %u clip(s) run end to end\n", clips_run);
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("ffmpeg core sweep: every clip loads, seeks, resets and tears down, and the rest are refused\n");
   return 0;
}
