/* A libretro host for the real builtin player, without FFmpeg linkage. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <libretro.h>
#include "../../../cores/libretro-media/media_hw.h"

static unsigned failures;
static unsigned frames;
static unsigned buttons;
static unsigned shutdowns;
static uint64_t samples;
static uint64_t nonzero;
static uint32_t frame_hash;
static uint32_t audio_hash;
static const char *current;

static void check(int ok, const char *message)
{
   if (!ok)
   {
      fprintf(stderr, "FAIL [%s]: %s\n", current, message);
      failures++;
   }
}

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap;
   if (level < RETRO_LOG_ERROR)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static bool environment(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         ((struct retro_log_callback*)data)->log = log_cb;
         return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
      case RETRO_ENVIRONMENT_SHUTDOWN:
         shutdowns++;
         return true;
      default:
         return false;
   }
}

static void video(const void *data, unsigned width, unsigned height,
      size_t pitch)
{
   const uint8_t *pixels = (const uint8_t*)data;
   unsigned y;
   size_t x;
   uint32_t hash = 2166136261u;
   check(width == 64 && height == 48, "unexpected video geometry");
   check(pitch >= width * 4, "invalid frame pitch");
   if (!pixels)
      return;
   for (y = 0; y < height; y++)
      for (x = 0; x < width * 4; x++)
         hash = (hash ^ pixels[y * pitch + x]) * 16777619u;
   frame_hash = hash;
   frames++;
}

static size_t audio(const int16_t *data, size_t count)
{
   size_t i;
   check(data != NULL, "audio callback without samples");
   if (!data)
      return 0;
   for (i = 0; i < count * 2; i++)
   {
      audio_hash = (audio_hash ^ (uint16_t)data[i]) * 16777619u;
      if (data[i])
         nonzero++;
   }
   samples += count;
   return count;
}

static void audio_sample(int16_t left, int16_t right)
{
   (void)left;
   (void)right;
   check(0, "unexpected single-sample callback");
}

static void input_poll(void) { }

static int16_t input_state(unsigned port, unsigned device, unsigned index,
      unsigned id)
{
   (void)port;
   (void)index;
   return device == RETRO_DEVICE_JOYPAD && (buttons & (1u << id)) ? 1 : 0;
}

static void clear_capture(void)
{
   frames = 0;
   samples = 0;
   nonzero = 0;
   shutdowns = 0;
   frame_hash = 0;
   audio_hash = 2166136261u;
   buttons = 0;
}

static void run_clip(const char *dir, const char *name)
{
   char path[512];
   uint8_t state[40];
   uint8_t invalid[40];
   struct retro_game_info game;
   struct retro_system_av_info av;
   unsigned i;
   uint32_t first;
   uint32_t next;
   uint64_t next_samples;
   uint32_t next_audio;
   bool loaded;
   current = name;
   snprintf(path, sizeof(path), "%s/%s", dir, name);
   memset(&game, 0, sizeof(game));
   game.path = path;
   clear_capture();
   loaded = retro_load_game(&game);
   check(loaded, "content did not load");
   if (!loaded)
      return;
   retro_get_system_av_info(&av);
   check(av.geometry.base_width == 64 && av.geometry.base_height == 48,
         "AV info geometry disagrees");
   check(av.timing.fps > 11.9 && av.timing.fps < 12.1,
         "container frame rate lost");
   check(av.timing.sample_rate == 48000, "audio rate lost");
   retro_run();
   first = frame_hash;
   for (i = 0; i < 5; i++)
      retro_run();
   check(retro_serialize_size() == sizeof(state), "unexpected state size");
   check(retro_serialize(state, sizeof(state)), "position save failed");
   samples = 0;
   audio_hash = 2166136261u;
   retro_run();
   next = frame_hash;
   next_samples = samples;
   next_audio = audio_hash;
   check(retro_unserialize(state, sizeof(state)), "position restore failed");
   samples = 0;
   audio_hash = 2166136261u;
   retro_run();
   check(frame_hash == next, "restored position displayed a different frame");
   check(samples == next_samples, "restored position emitted different audio length");
   check(audio_hash == next_audio, "restored position emitted different PCM");
   check(!retro_serialize(NULL, sizeof(state)), "null save buffer accepted");
   check(!retro_serialize(state, 24), "short save buffer accepted");
   check(!retro_unserialize(state, 24), "truncated version 3 state accepted");
   check(!retro_unserialize(state, 8), "truncated state accepted");
   check(!retro_unserialize(NULL, sizeof(state)), "null state accepted");
   memcpy(invalid, state, sizeof(state));
   invalid[4] = 99;
   check(!retro_unserialize(invalid, sizeof(invalid)), "unknown state version accepted");
   memcpy(invalid, state, sizeof(state));
   invalid[31] = 0x80;
   check(!retro_unserialize(invalid, sizeof(invalid)), "invalid clock accepted");
   buttons = 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   retro_run();
   buttons = 0;
   retro_reset();
   clear_capture();
   retro_run();
   check(frame_hash == first, "reset did not display the initial frame");
   for (i = 0; i < 100 && !shutdowns; i++)
      retro_run();
   check(shutdowns != 0, "EOS did not request shutdown");
   printf("duration %s: frames=%u samples=%lu nonzero=%lu\n", name,
         frames, (unsigned long)samples, (unsigned long)nonzero);
   check(frames >= 24 && frames < 32, "movie duration/frame pacing is wrong");
#if defined(HAVE_RAAC) || defined(HAVE_ROPUS) || defined(HAVE_RVORBIS)
   check(nonzero > 1000, "supported soundtrack played silently");
   check(nonzero >= 180000 && nonzero <= 200000,
         "soundtrack duration is wrong");
#else
   check(nonzero == 0, "audio-disabled build emitted non-silence");
#endif
   check(retro_serialize(state, sizeof(state)), "EOS save failed");
   clear_capture();
   check(retro_unserialize(state, sizeof(state)), "EOS restore failed");
   for (i = 0; i < 3 && !shutdowns; i++)
      retro_run();
   check(shutdowns != 0, "EOS state restarted the movie");
   next = frame_hash;
   state[4] = 2;
   clear_capture();
   check(retro_unserialize(state, 24), "legacy version 2 state refused");
   for (i = 0; i < 100 && !shutdowns; i++)
      retro_run();
   check(shutdowns != 0, "legacy EOS state restarted the movie");
   check(frame_hash == next, "legacy EOS state replayed video");
   state[4] = 1;
   check(retro_unserialize(state, 16), "legacy version 1 state refused");
   retro_unload_game();
   printf("checked %s\n", name);
}

#ifdef HAVE_RMP4
/* The seek path skips pictures outside retro_run's normal counter.
 * A state saved there must still restore the exact video/audio pair. */
static void run_backend_restart(const char *dir, const char *name, int seek)
{
   char path[512];
   struct retro_game_info game;
   uint32_t picture[12], pcm[12];
   uint64_t lengths[12];
   unsigned pass, i;
   current = name;
   snprintf(path, sizeof(path), "%s/%s", dir, name);
   memset(&game, 0, sizeof(game));
   game.path = path;
   for (pass = 0; pass < 2; pass++)
   {
      bool loaded;
      clear_capture();
      loaded = retro_load_game(&game);
      check(loaded, "backend restart fixture did not load");
      if (!loaded)
         return;
      for (i = 0; i < 12; i++)
      {
         if (seek && i == 3)
            buttons = 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
         if (pass && (i == 5 || i == 9))
            media_suspend_video_hw();
         samples = 0;
         audio_hash = 2166136261u;
         retro_run();
         buttons = 0;
         if (!pass)
         {
            picture[i] = frame_hash;
            pcm[i] = audio_hash;
            lengths[i] = samples;
         }
         else
         {
            check(frame_hash == picture[i], "backend restart changed the next picture");
            check(samples == lengths[i], "backend restart changed audio length");
            check(audio_hash == pcm[i], "backend restart changed PCM");
         }
      }
      check(!shutdowns, "backend restart ended playback");
      retro_unload_game();
   }
   printf("checked backend restart %s seek=%d\n", name, seek);
}

static void run_seek_restore(const char *dir, const char *name)
{
   char path[512];
   uint8_t state[40];
   struct retro_game_info game;
   uint32_t next_video, next_audio;
   uint64_t next_samples;
   unsigned i;
   bool loaded;
   current = name;
   snprintf(path, sizeof(path), "%s/%s", dir, name);
   memset(&game, 0, sizeof(game));
   game.path = path;
   clear_capture();
   loaded = retro_load_game(&game);
   check(loaded, "seek fixture did not load");
   if (!loaded)
      return;
   for (i = 0; i < 6; i++)
      retro_run();
   buttons = 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   retro_run();
   buttons = 0;
   check(!shutdowns, "ten-second seek ended a sixteen-second movie");
   check(retro_serialize(state, sizeof(state)), "post-seek save failed");
   samples = 0;
   audio_hash = 2166136261u;
   retro_run();
   next_video = frame_hash;
   next_audio = audio_hash;
   next_samples = samples;
   check(retro_unserialize(state, sizeof(state)), "post-seek restore failed");
   samples = 0;
   audio_hash = 2166136261u;
   retro_run();
   check(frame_hash == next_video, "post-seek restore displayed a different frame");
   check(samples == next_samples, "post-seek restore changed audio length");
   check(audio_hash == next_audio, "post-seek restore changed PCM");
   retro_unload_game();
   printf("checked post-seek restore %s\n", name);
}
#endif

int main(int argc, char **argv)
{
   struct retro_game_info game;
   char path[512];
   const char *dir = argc > 1 ? argv[1] : "fixtures";
   unsigned i;
   const char *bad[] = { "empty.webm", "invalid.webm", "missing.webm" };
   current = "host";
   retro_set_environment(environment);
   retro_set_video_refresh(video);
   retro_set_audio_sample(audio_sample);
   retro_set_audio_sample_batch(audio);
   retro_set_input_poll(input_poll);
   retro_set_input_state(input_state);
   retro_init();
   check(!retro_load_game(NULL), "null game accepted");
   for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
   {
      current = bad[i];
      memset(&game, 0, sizeof(game));
      snprintf(path, sizeof(path), "%s/%s", dir, bad[i]);
      game.path = path;
      check(!retro_load_game(&game), "invalid content accepted");
      retro_unload_game();
   }
   run_clip(dir, "vp8-opus.webm");
   run_clip(dir, "vp9-vorbis.webm");
#ifdef HAVE_RMP4
   run_clip(dir, "h264-aac.mp4");
   run_clip(dir, "h264-aac.mkv");
   run_clip(dir, "hevc-aac.mp4");
   run_backend_restart(dir, "hevc-seek-aac.mp4", 0);
   run_backend_restart(dir, "h264-seek-aac.mp4", 0);
   run_backend_restart(dir, "hevc-seek-aac.mp4", 1);
   run_backend_restart(dir, "h264-seek-aac.mp4", 1);
   run_seek_restore(dir, "hevc-seek-aac.mp4");
   run_seek_restore(dir, "h264-seek-aac.mp4");
#endif
   run_clip(dir, "vp8-opus.webm");
   retro_deinit();
   printf("%u failures\n", failures);
   return failures ? 1 : 0;
}
