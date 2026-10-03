/* Mixer streams arrive only inside the mixer's init..done window.
 *
 * Links the shipping RetroArch objects with only main() replaced
 * (the playlist_nav pattern) and drives the seam the whole-tree
 * audit pointed at next: task retirement against subsystem
 * teardown. audio_driver_deinit() runs audio_mixer_done(), which
 * releases every voice and frees its lock; a mixer-load task that
 * retires after that - RetroArch's shutdown tears the drivers down
 * before the task queue - lands its finish callback in
 * audio_driver_mixer_add_stream(), which would claim a voice inside
 * the torn subsystem. The same door is open for every menu sound in
 * a session whose audio driver failed to initialize, since all of
 * audio_driver_init_internal()'s failure exits precede
 * audio_mixer_init().
 *
 * The claims: with audio up, a stream adds; after
 * audio_driver_deinit(), an add is refused and the ownership
 * contract still holds (buf_owner released exactly once, out_slot
 * -1); after CMD_EVENT_AUDIO_REINIT, adds work again.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/audio/mixer_retirement/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../command.h"
#include "../../../audio/audio_driver.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* A minimal 16-bit stereo PCM WAV at the configured output rate:
 * 44-byte canonical header plus a short run of silence. */
#define WAV_FRAMES 256
#define WAV_RATE   48000
static unsigned char wav_buf[44 + WAV_FRAMES * 4];

static void wav_fill(void)
{
   unsigned char *p    = wav_buf;
   unsigned data_bytes = WAV_FRAMES * 4;
   unsigned riff_bytes = 36 + data_bytes;

#define PUT4(s)  do { memcpy(p, s, 4); p += 4; } while (0)
#define PUT32(v) do { p[0] = (unsigned char)((v) & 0xff); \
                      p[1] = (unsigned char)(((v) >> 8) & 0xff); \
                      p[2] = (unsigned char)(((v) >> 16) & 0xff); \
                      p[3] = (unsigned char)(((v) >> 24) & 0xff); \
                      p += 4; } while (0)
#define PUT16(v) do { p[0] = (unsigned char)((v) & 0xff); \
                      p[1] = (unsigned char)(((v) >> 8) & 0xff); \
                      p += 2; } while (0)

   PUT4("RIFF"); PUT32(riff_bytes); PUT4("WAVE");
   PUT4("fmt "); PUT32(16);
   PUT16(1);                 /* PCM        */
   PUT16(2);                 /* stereo     */
   PUT32(WAV_RATE);
   PUT32(WAV_RATE * 4);      /* byte rate  */
   PUT16(4);                 /* block align*/
   PUT16(16);                /* bits       */
   PUT4("data"); PUT32(data_bytes);
   memset(p, 0, data_bytes);

#undef PUT4
#undef PUT32
#undef PUT16
}

/* Counting owner: the ownership contract says buf_owner is released
 * in every outcome - on failure immediately, on success when the
 * sound dies (or straight after conversion for WAV). */
static unsigned owner_frees = 0;
static void counting_owner_free(void *owner)
{
   (void)owner;
   owner_frees++;
}

static bool add_one(void *owner, int *slot_out)
{
   audio_mixer_stream_params_t params;

   memset(&params, 0, sizeof(params));
   params.buf                 = wav_buf;
   params.bufsize             = sizeof(wav_buf);
   params.basename            = NULL;
   params.cb                  = NULL;
   params.volume              = 1.0f;
   params.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
   params.stream_type         = AUDIO_STREAM_TYPE_SYSTEM;
   params.type                = AUDIO_MIXER_TYPE_WAV;
   params.state               = AUDIO_STREAM_STATE_PLAYING;
   params.buf_owner           = owner;
   params.buf_owner_free      = owner ? counting_owner_free : NULL;
   params.out_slot            = slot_out;

   return audio_driver_mixer_add_stream(&params);
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   int slot       = -2;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/mixer_retire_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg",
            fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         /* The "null" audio driver is a stub vtable that cannot
          * initialize; ALSA against its null PCM plugin gives a
          * real, headless driver for the audio-up lanes. */
         fprintf(cfg, "audio_driver = \"alsa\"\n");
         fprintf(cfg, "audio_device = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"rgui\"\n");
         fprintf(cfg, "audio_enable = \"true\"\n");
         fprintf(cfg, "log_verbosity = \"true\"\n");
         fprintf(cfg, "audio_out_rate = \"%d\"\n", WAV_RATE);
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   wav_fill();

   /* A menu-only boot defers audio bring-up; the reinit command runs
    * the full deinit+init pair and leaves audio initialized, which
    * is the state the lanes below need. */
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   CHECK((AUDIO_FLAGS_GET(audio_state_get_ptr())
            & AUDIO_FLAG_MIXER_INITED) != 0,
         "fixture: mixer not inited after AUDIO_REINIT (flags 0x%x)",
         (unsigned)AUDIO_FLAGS_GET(audio_state_get_ptr()));

   /* Lane 1: with audio up, the stream adds and lands in a slot.
    * The WAV path releases the owner right after conversion, so one
    * release is the correct count here, not zero. */
   owner_frees = 0;
   CHECK(add_one(wav_buf, &slot),
         "baseline add_stream failed with audio initialized");
   CHECK(slot >= 0, "baseline add reported no slot (%d)", slot);
   CHECK(owner_frees == 1,
         "WAV conversion owner release: %u frees, want 1",
         owner_frees);

   /* Lane 2: after teardown, the add is refused and the ownership
    * contract still holds - this is the retiring-task shape, the
    * callback arriving after audio_driver_deinit(). */
   audio_driver_deinit();

   owner_frees = 0;
   slot        = -2;
   CHECK(!add_one(wav_buf, &slot),
         "add_stream accepted a stream into the torn-down mixer");
   CHECK(owner_frees == 1,
         "refused add released the owner %u times, want exactly 1",
         owner_frees);
   CHECK(slot == -1,
         "refused add left out_slot at %d, want -1", slot);

   /* Lane 3: after a reinit, the window is open again. */
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);

   owner_frees = 0;
   slot        = -2;
   CHECK(add_one(wav_buf, &slot),
         "add_stream still refused after CMD_EVENT_AUDIO_REINIT");
   CHECK(slot >= 0, "post-reinit add reported no slot (%d)", slot);

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("mixer_retirement: all lanes passed\n");
   return 0;
}
