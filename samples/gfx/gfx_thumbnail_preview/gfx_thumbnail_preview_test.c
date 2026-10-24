/* End-to-end regression test for the file-browser video preview.
 *
 * Drives the REAL gfx/gfx_thumbnail.c: gfx_thumbnail_request_file()
 * (which is gfx_thumbnail_request's video route, minus the playlist
 * path resolution the harness cannot stand up) followed by the
 * per-frame gfx_thumbnail_animate() pump, then asserts what the menu
 * driver would actually need in order to draw something:
 *
 *   R1  a texture is uploaded
 *   R2  status reaches AVAILABLE
 *   R3  alpha is non-zero, or a fade was pushed to raise it
 *   R4  no whole-file still load was queued for a video
 *   R5  peak RSS stays inside the window budget, not the file length
 *
 * R3 is the one that was broken: R1/R2/R4 all passed while every
 * frame drew at zero opacity.
 */
#include <features/features_cpu.h>
#include <audio/audio_mixer.h>
#include <retro_timers.h>
#include <stdio.h>
#include <formats/video_player.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "harness.h"
#include "../../../gfx/gfx_thumbnail.h"
#include "../../../gfx/gfx_anim_preview.h"
#include "../../../gfx/gfx_media_cpu.h"
#include "../../../gfx/video_defines.h"
#include "../../../libretro-common/include/formats/data_transfer.h"

static double rss_mib(void)
{
   long pages = 0, dummy = 0;
   FILE *f = fopen("/proc/self/statm", "r");
   if (!f)
      return -1.0;
   if (fscanf(f, "%ld %ld", &dummy, &pages) != 2)
      pages = 0;
   fclose(f);
   return (double)pages * 4096.0 / (1024.0 * 1024.0);
}

static int fails;

static size_t file_len(const char *p)
{
   struct stat st;
   return (stat(p, &st) == 0) ? (size_t)st.st_size : 0;
}

static void check(const char *tag, const char *what, int ok)
{
   printf("      %-46s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
   (void)tag;
}

/* expect_video: 1 = must play; 2 = reservation is off, so a file too
 * large for a whole-file mapping may fall back to a still - what is
 * checked then is only that RSS stays bounded and the flag is honest. */
static void run(const char *path, const char *label, int expect_video)
{
   gfx_thumbnail_t th;
   int i;
   double rss0, peak;
   /* an animated WEBP takes the video's anim-first route - its first
    * frame is the still - and has no audio to check */
   int is_webp = strlen(path) > 5 && !strcmp(path + strlen(path) - 5, ".webp");

   /* An unreadable path is a broken invocation, not a result.  Left
    * unchecked the run reported MISSING with A7/A8 passing vacuously
    * (file_len is 0, so "mixer got the whole container" compares 0
    * against 0), which is how a CI step that had deleted its own
    * fixtures still went half-green. */
   if (!file_len(path))
   {
      printf("  %s\n", label);
      check(label, "F0 fixture readable", 0);
      return;
   }

   memset(&th, 0, sizeof(th));
   memset(&hp, 0, sizeof(hp));
   hp.hdr_output = getenv("HDR_OUTPUT") ? 1 : 0;
   hp.force_preview_audio = getenv("NOAUDIO") ? 0 : 1;
   gfx_thumbnail_reset(&th);

   rss0 = rss_mib();
   printf("  %s\n", label);

   gfx_thumbnail_request_file(path, &th, 0);

   {
      const char *fe = getenv("FRAMES");
      const char *pe = getenv("PACE_HZ");
      int nf = fe ? atoi(fe) : 12;
      int pace_hz = pe ? atoi(pe) : 0;
      double after_open = rss_mib();
      int64_t next_us = cpu_features_get_time_usec();
      int64_t last_change_us = next_us;
      int64_t worst_gap_us = 0;
      int last_frames = 0;
      for (i = 0; i < nf; i++)
      {
         int64_t now;
         /* PACE_HZ: run the poll at a display's cadence rather than
          * flat out, which is how the thumbnail is actually driven,
          * and record the longest run of polls in which the picture
          * did not change - the stall a viewer would see. */
         if (pace_hz > 0)
         {
            next_us += 1000000 / pace_hz;
            now = cpu_features_get_time_usec();
            if (next_us > now)
               retro_sleep((unsigned)((next_us - now) / 1000));
         }
         now = cpu_features_get_time_usec();
         gfx_thumbnail_animate(&th, now);
         if (hp.texture_uploads != last_frames)
         {
            int64_t gap = now - last_change_us;
            if (gap > worst_gap_us)
               worst_gap_us = gap;
            if (getenv("PACE_LOG") && gap > 150000)
               fprintf(stderr, "      stall %6.0f ms before picture %d\n",
                     (double)gap / 1000.0, hp.texture_uploads);
            last_change_us = now;
            last_frames = hp.texture_uploads;
         }
      }
      if (pace_hz > 0)
      {
         const char *ms = getenv("PACE_MAX_STALL_MS");
         printf("      paced at %d Hz: %d picture changes in %d polls, "
               "longest stall %.0f ms\n", pace_hz, last_frames, nf,
               (double)worst_gap_us / 1000.0);
         /* With a bound, the stall is a check: a small fixture decodes
          * in well under a frame, so a run of polls with the picture
          * unchanged is the scheduler's doing, not the decoder's. */
         if (ms)
            check(label, "P1 no stall past the bound",
                  worst_gap_us <= (int64_t)atoi(ms) * 1000);
      }
      /* The frames above are the measurement window; whether the
       * first picture has landed by then is a matter of how loaded
       * the worker is - under a sanitizer, with other fixtures' jobs
       * still draining, it may not have. What the checks below ask is
       * whether it lands at all, so keep animating, yielding to the
       * worker, until it does or a generous bound says it never will. */
      for (i = 0; i < 2000 && !th.texture
            && th.status != GFX_THUMBNAIL_STATUS_MISSING; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         retro_sleep(1);
      }
      peak = rss_mib();
      printf("      RSS after open=%.1f MiB, after %d frames=%.1f MiB\n",
            after_open, nf, peak);
      /* The session's own count: head plus moving window, nothing
       * else. This is the number the bitrate-sized feed changes. */
      if (th.anim_sess)
         printf("      session resident after %d frames=%.2f MiB "
                "(file %.2f MiB)\n", nf,
                gfx_anim_preview_resident_bytes(
                   (const gfx_anim_preview_t*)th.anim_sess) / (1024.0 * 1024.0),
                file_len(path) / (1024.0 * 1024.0));
   }

   printf("      audio_streams=%d bytes=%llu\n", hp.audio_streams,
         (unsigned long long)hp.last_audio_bytes);
   printf("      status=%d alpha=%.3f tex=%lu uploads=%d fades=%d "
          "stills=%d %ux%u\n",
         (int)th.status, th.alpha, (unsigned long)th.texture,
         hp.texture_uploads, hp.fade_pushes, hp.still_loads,
         hp.last_tex_w, hp.last_tex_h);

   /* HDR: with HDR_OUTPUT the stub driver offers half floats, and an
    * HDR source (HDR_EXPECT: the PQ-tagged flat-grey fixture) must have
    * its animation uploaded as them, on the PQ EOTF of its 10-bit grey
    * (luma 600, over 80 nits). Without both, not one frame may be. */
   if (hp.hdr_output && getenv("HDR_EXPECT"))
   {
      unsigned short h = hp.fp16_r;
      int    e   = (h >> 10) & 0x1f;
      double r   = (e ? ldexp(1.0 + (h & 0x3ff) / 1024.0, e - 15)
                      : ldexp((h & 0x3ff) / 1024.0, -14)) * ((h & 0x8000) ? -1 : 1);
      double ep  = pow(((298 * (600 - 64) + 128) >> 8) / 1023.0, 1.0 / 78.84375);
      double num = ep - 0.8359375 > 0.0 ? ep - 0.8359375 : 0.0;
      double ref = 10000.0 * pow(num / (18.8515625 - 18.6875 * ep),
            1.0 / 0.1593017578125) / 80.0;
      check(label, "H1 an HDR source animates in half floats",
            hp.fp16_uploads >= 1);
      check(label, "H2 its half floats lie on the PQ EOTF",
            fabs(r - ref) / ref < 0.04);
      printf("      [hdr] %d half-float frames, centre red %.4f "
             "(PQ grey %.4f)\n", hp.fp16_uploads, r, ref);
   }
   else
      check(label, "H0 no half floats without an HDR source and output",
            hp.fp16_uploads == 0);

   if (expect_video == 2)
   {
      /* No reservation: whatever the outcome, the file must not be
       * resident-loaded and the flag must match the mapping. A small
       * file still plays; a multi-GB one may become a still. */
      check(label, "N1 RSS growth < 600 MiB (no whole-file load)", (peak - rss0) < 600.0);
      check(label, "N2 windowed flag matches the mapping",
            (th.anim_windowed ? 1 : 0)
            == (th.anim_dt && data_transfer_window_is_reserved(
                  (data_transfer_t*)th.anim_dt) ? 1 : 0));
      check(label, "N3 not windowed without a reservation", !th.anim_windowed);
      printf("      [noreserve] RSS %.1f -> %.1f MiB uploads=%d stills=%d\n",
            rss0, peak, hp.texture_uploads, hp.still_loads);
   }
   else if (expect_video)
   {
      check(label, "R1 texture uploaded",  hp.texture_uploads > 0);
      check(label, "R2 status AVAILABLE",  th.status == GFX_THUMBNAIL_STATUS_AVAILABLE);
      check(label, "R3 drawable (alpha>0 or fade pushed)",
            (th.alpha > 0.0f) || (hp.fade_pushes > 0));
      /* the still comes from the session's first frame - for a video
       * since anim-first, for an animated WEBP since the head probe
       * routes it the same way (its still decode read and parsed the
       * whole file, then the animation decoded frame 0 again) */
      check(label, "R4 no whole-file still load queued", hp.still_loads == 0);
      check(label, "R5 RSS growth < 600 MiB", (peak - rss0) < 600.0);
      /* The install's windowed flag must match how the file was
       * actually mapped. With reservation off (pass "noreserve") the
       * open degrades to a whole-file mapping and the flag is false;
       * hardcoding it to 1 (the regression) both mis-admits a multi-GB
       * file and feeds the decoder against an already-resident buffer.
       * data_transfer_window_is_reserved is the ground truth. */
      check(label, "R6 windowed flag matches the mapping",
            (th.anim_windowed ? 1 : 0)
            == (th.anim_dt && data_transfer_window_is_reserved(
                  (data_transfer_t*)th.anim_dt) ? 1 : 0));
      /* Preview audio is bounded by gfx_thumb_anim_mem_ok, so a file
       * past GFX_THUMB_ANIM_ABS_MAX_FILE is meant to be silent - that
       * is policy, not breakage.  Under the cap it must start AND get
       * the whole container: handing the mixer a short buffer is the
       * failure 9650f04 papered over by skipping the hand-off. */
      if (is_webp)
      {
         /* no audio track to start */
         check(label, "A6 no preview audio for an animated WEBP",
               hp.audio_streams == 0);
      }
      else if (hp.force_preview_audio)
      {
         /* No cap: the window costs its slide, not the file, so even
          * a 7 GB recording gets audio. Not asked for under NOAUDIO,
          * which the paced runs use: they are about the pictures. */
         check(label, "A6 preview audio started", hp.audio_streams > 0);
         check(label, "A7 mixer got the whole container",
               hp.last_audio_bytes == (size_t)file_len(path));
      }
      check(label, "A8 feeder kept the decoder fed (no stalls)",
            hp.audio_stalls == 0);
      printf("      RSS %.1f -> %.1f MiB  audio=%d bytes=%llu "
             "raises=%d stalls=%d\n",
            rss0, peak, hp.audio_streams,
            (unsigned long long)hp.last_audio_bytes,
            hp.audio_avail_raises, hp.audio_stalls);
   }

   if (getenv("TRANSPORT_PAUSE"))
   {
      retro_time_t pause_now = cpu_features_get_time_usec();
      int uploads = hp.texture_uploads;
      int tick;
      check(label, "transport session exists", th.anim_player != NULL);
      check(label, "transport pause accepted",
            gfx_thumbnail_set_paused(&th, true, pause_now));
      for (tick = 0; tick < 10; tick++)
         gfx_thumbnail_animate(&th, pause_now + 1000000 + tick * 1000);
      check(label, "transport pause holds displayed texture",
            hp.texture_uploads == uploads);
      check(label, "transport resume accepted",
            gfx_thumbnail_set_paused(&th, false, pause_now + 2000000));
   }
   if (getenv("TRANSPORT_SEEK") && th.anim_sess)
   {
      retro_time_t seek_now = cpu_features_get_time_usec();
      int uploads = hp.texture_uploads;
      int tick;
      float pcm[128];
      size_t mixed = 0;
      double energy = 0.0;
      gfx_thumbnail_playback_state_t playback;
      gfx_anim_preview_t *session = (gfx_anim_preview_t*)th.anim_sess;
      audio_mixer_init(48000);
      check(label, "seek pauses current playback",
            gfx_thumbnail_set_paused(&th, true, seek_now));
      check(label, "first absolute seek accepted",
            gfx_thumbnail_seek(&th, 1250000, seek_now));
      gfx_thumbnail_animate(&th, seek_now);
      check(label, "replacement absolute seek accepted",
            gfx_thumbnail_seek(&th, 400000, seek_now + 1));
      check(label, "seeking snapshot shows latest target",
            gfx_thumbnail_get_playback_state(&th, seek_now + 1, &playback)
            && playback.seeking && playback.paused && playback.position_us == 400000);
      for (tick = 0; tick < 2000 && th.anim_sess
            && ((gfx_anim_preview_t*)th.anim_sess)->seeking; tick++)
      {
         gfx_thumbnail_animate(&th, seek_now + 1000 + tick * 1000);
         retro_sleep(1);
      }
      check(label, "latest seek presents and commits", th.anim_sess
            && !((gfx_anim_preview_t*)th.anim_sess)->seeking);
      if (th.anim_sess)
      {
         session = (gfx_anim_preview_t*)th.anim_sess;
         check(label, "seek preserves user pause", session->paused);
         check(label, "landed position belongs to replacement",
               session->media_origin_us > 100000 && session->media_origin_us <= 400000);
         check(label, "seek presents one replacement texture", hp.texture_uploads > uploads);
         uploads = hp.texture_uploads;
         for (tick = 0; tick < 10; tick++)
            gfx_thumbnail_animate(&th, seek_now + 3000000 + tick * 1000);
         check(label, "paused target texture is retained", hp.texture_uploads == uploads);
         if (hp.force_preview_audio)
         {
            check(label, "seek installs actual prepared audio", session->audio_voice != NULL);
            memset(pcm, 0, sizeof(pcm));
            audio_mixer_mix(pcm, 64, 1.0f, false);
            for (tick = 0; tick < 128; tick++) energy += fabs(pcm[tick]);
            check(label, "paused target emits no PCM", energy == 0.0);
            check(label, "paused target has zero mixed-frame clock",
                  audio_mixer_voice_clock(session->audio_voice, &mixed, NULL, NULL) && !mixed);
            check(label, "seek resume accepted",
                  gfx_thumbnail_set_paused(&th, false, seek_now + 3500000));
            memset(pcm, 0, sizeof(pcm));
            audio_mixer_mix(pcm, 64, 1.0f, false);
            for (tick = 0; tick < 128; tick++) energy += fabs(pcm[tick]);
            check(label, "resumed target emits actual PCM", energy > 0.0);
            check(label, "resumed target clock counts actual frames",
                  audio_mixer_voice_clock(session->audio_voice, &mixed, NULL, NULL) && mixed == 64);
            check(label, "timeline advances by PCM rather than wall gap",
                  gfx_thumbnail_get_playback_state(&th, seek_now + 3600000, &playback)
                  && playback.position_us > session->media_origin_us
                  && playback.position_us - session->media_origin_us <= 2000);
         }
         check(label, "close during seek accepted",
               gfx_thumbnail_seek(&th, 1000000, seek_now + 4000000));
         gfx_thumbnail_animate(&th, seek_now + 4000000);
      }
   }
   gfx_thumbnail_reset(&th);
   if (getenv("TRANSPORT_SEEK")) audio_mixer_done();
}


/* setenv/unsetenv are POSIX; the MSVCRT that MinGW builds against has
 * _putenv, where "NAME=" removes the variable. getenv sees both. */
static void env_set(const char *name, const char *value)
{
#ifdef _WIN32
   char buf[256];
   snprintf(buf, sizeof(buf), "%s=%s", name, value ? value : "");
   _putenv(buf);
#else
   if (value)
      setenv(name, value, 1);
   else
      unsetenv(name);
#endif
}

/* Stills asked for during a frame go out at its end, in sets as wide
 * as the cores, where the task queue is threaded; a cancel before then
 * sends nothing; an unthreaded queue takes each at once. Every tag
 * comes back through the thumbnail's landing, which ASan's leak check
 * holds to. */
/* The savestate thumbnail path for a playlist entry, with no core
 * running: built from the entry's name in the savestate folder, over
 * and over as the menu redraws it. Every call is to leave nothing
 * behind, which ASan's leak check holds to. */
extern int stub_playlist_entry_set(const char *savestate_dir,
      const char *entry_path);
static void lane_savestate_thumbnail_path(void)
{
   int  i;
   int  wrong = 0;
   char path[512];
   int  ok    = stub_playlist_entry_set("/states", "/roms/Some Game.zip");

   check("savestate path", "the playlist entry is laid out", ok);
   for (i = 0; ok && i < 64; i++)
   {
      gfx_savestate_thumbnail_get_path(path, sizeof(path),
            "ignored", (i & 1) ? 3 : 0);
      if (strcmp(path, (i & 1)
               ? "/states/Some Game.state3.png"
               : "/states/Some Game.state.png"))
      {
         if (!wrong++)
            printf("      savestate thumbnail path: %s\n", path);
      }
   }
   check("savestate path", "built from the entry in the state folder",
         ok && !wrong);
   stub_playlist_entry_set(NULL, NULL);
}

#define SET_STILLS 5
static void lane_still_sets(void)
{
   gfx_thumbnail_path_data_t *pd = gfx_thumbnail_path_init();
   gfx_thumbnail_t th[SET_STILLS];
   char names[SET_STILLS][32];
   unsigned i, width = cpu_features_get_core_amount();
   int ok;

   if (width > 8)
      width = 8;
   printf("still sets (%u cores)\n", width);
   for (i = 0; i < SET_STILLS; i++)
   {
      FILE *f;
      char full[64];
      snprintf(names[i], sizeof(names[i]), "gfx_thumb_set_%u.png", i);
      snprintf(full, sizeof(full), "./%s", names[i]);
      if ((f = fopen(full, "wb")))
      {
         fputs("png", f);
         fclose(f);
      }
      gfx_thumbnail_init_blank(&th[i]);
   }
   hp.thumb_cfg      = 1;
   hp.threaded_queue = 1;
   hp.still_loads    = hp.still_sets = hp.set_items = hp.set_max = 0;

   for (i = 0; i < SET_STILLS; i++)
   {
      gfx_thumbnail_set_content_image(pd, ".", names[i]);
      gfx_thumbnail_request(pd, GFX_THUMBNAIL_RIGHT, NULL, 0, &th[i],
            0, false);
   }
   ok = 1;
   for (i = 0; i < SET_STILLS; i++)
      if (th[i].status != GFX_THUMBNAIL_STATUS_PENDING)
         ok = 0;
   check("sets", "requests wait for the frame's end", ok
         && hp.still_loads == 0 && hp.still_sets == 0);
   gfx_thumbnail_flush_requests();
   if (width > 1)
      check("sets", "the frame's stills go out in sets of the cores",
            hp.set_items + hp.still_loads == SET_STILLS
            && hp.set_max == (int)(width < SET_STILLS ? width : SET_STILLS)
            && hp.still_sets == (int)(SET_STILLS / width
               + (SET_STILLS % width > 1))
            && hp.still_loads == (int)(SET_STILLS % width == 1));
   else
      check("sets", "one core: each still alone",
            hp.still_loads == SET_STILLS && hp.still_sets == 0);
   /* The sets come first, in request order; a single's task here
    * never answers */
   ok = 1;
   for (i = 0; i < (unsigned)hp.set_items; i++)
      if (th[i].status == GFX_THUMBNAIL_STATUS_PENDING)
         ok = 0;
   check("sets", "a set's stills land", ok);
   gfx_thumbnail_flush_requests();
   check("sets", "a second flush sends nothing",
         hp.set_items + hp.still_loads == SET_STILLS);

   /* cancelled before the frame's end */
   hp.still_loads = hp.still_sets = hp.set_items = 0;
   for (i = 0; i < 2; i++)
   {
      gfx_thumbnail_set_content_image(pd, ".", names[i]);
      gfx_thumbnail_request(pd, GFX_THUMBNAIL_RIGHT, NULL, 0, &th[i],
            0, false);
   }
   gfx_thumbnail_cancel_pending_requests();
   gfx_thumbnail_flush_requests();
   check("sets", "a cancel before the frame's end sends nothing",
         hp.still_loads == 0 && hp.still_sets == 0);
   check("sets", "and leaves them orphaned for a re-request",
         gfx_thumbnail_reset_if_orphaned(&th[0])
         && gfx_thumbnail_reset_if_orphaned(&th[1]));

   /* an unthreaded queue */
   hp.threaded_queue = 0;
   gfx_thumbnail_set_content_image(pd, ".", names[0]);
   gfx_thumbnail_request(pd, GFX_THUMBNAIL_RIGHT, NULL, 0, &th[0], 0, false);
   check("sets", "an unthreaded queue takes each still at once",
         hp.still_loads == 1 && hp.still_sets == 0);

   for (i = 0; i < SET_STILLS; i++)
   {
      char full[64];
      gfx_thumbnail_reset(&th[i]);
      snprintf(full, sizeof(full), "./%s", names[i]);
      remove(full);
   }
   free(pd);
   hp.thumb_cfg      = 0;
   hp.still_loads    = 0;
}

#ifdef PREVIEW_THREADED
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include "../../../gfx/gfx_surface_player.h"

static retro_eventcount_t retire_wake;
static retro_atomic_int_t retire_entered;
static retro_atomic_int_t retire_released;

static bool retire_feed(void *user)
{
   retro_atomic_store_release_int(&retire_entered, 1);
   while (!retro_atomic_load_acquire_int(&retire_released))
   {
      int key = retro_eventcount_prepare_wait(&retire_wake);
      if (!retro_atomic_load_acquire_int(&retire_released))
         retro_eventcount_commit_wait(&retire_wake, key);
      else
         retro_eventcount_cancel_wait(&retire_wake);
   }
   return gfx_anim_preview_feed((gfx_anim_preview_t*)user);
}

static void retire_watchdog(void *unused)
{
   (void)unused;
   retro_sleep(500);
   retro_atomic_store_release_int(&retire_released, 1);
   retro_eventcount_notify(&retire_wake);
}

static void lane_retire(const char *path)
{
   gfx_thumbnail_t *th = (gfx_thumbnail_t*)calloc(1, sizeof(*th));
   gfx_surface_player_t *player;
   sthread_t *watchdog;
   int64_t start, deadline;
   int unloads;
   if (!th)
   {
      check("retire", "thumbnail allocated", 0);
      return;
   }
   gfx_thumbnail_init_blank(th);
   gfx_thumbnail_request_file(path, th, 0);
   player = (gfx_surface_player_t*)th->anim_player;
   check("retire", "playback opened for retirement test", player != NULL);
   if (!player)
   {
      gfx_thumbnail_reset(th);
      free(th);
      return;
   }
   retro_eventcount_init(&retire_wake);
   retro_atomic_store_relaxed_int(&retire_entered, 0);
   retro_atomic_store_relaxed_int(&retire_released, 0);
   player->cb.feed = retire_feed;
   player->cb.feed_user = th->anim_sess;
   gfx_thumbnail_animate(th, cpu_features_get_time_usec());
   deadline = cpu_features_get_time_usec() + 5000000;
   while (!retro_atomic_load_acquire_int(&retire_entered)
         && cpu_features_get_time_usec() < deadline)
   {
      gfx_thumbnail_animate(th, cpu_features_get_time_usec());
      retro_sleep(1);
   }
   check("retire", "worker entered blocked feeder",
         retro_atomic_load_acquire_int(&retire_entered));
   if (!retro_atomic_load_acquire_int(&retire_entered))
   {
      retro_atomic_store_release_int(&retire_released, 1);
      retro_eventcount_notify(&retire_wake);
      gfx_thumbnail_reset(th);
      free(th);
      gfx_thumbnail_anim_worker_deinit();
      retro_eventcount_free(&retire_wake);
      return;
   }
   {
      gfx_surface_t *surface = (gfx_surface_t*)th->anim_surface;
      check("retire", "producer surface remains available", surface != NULL);
      if (surface && surface->num_slots > 1)
      {
         memset(surface->slots[1], 0, (size_t)VIDEO_SCALE_W(surface->dims)
               * VIDEO_SCALE_H(surface->dims) * sizeof(uint32_t));
         check("retire", "texture created beside the held producer",
               gfx_surface_submit(surface, 1, player->rgba)
                  == GFX_SURFACE_SUBMIT_DONE);
      }
   }
   unloads = hp.texture_unloads;
   watchdog = sthread_create(retire_watchdog, NULL);
   check("retire", "watchdog created", watchdog != NULL);
   if (!watchdog)
      retro_atomic_store_release_int(&retire_released, 1);
   start = cpu_features_get_time_usec();
   gfx_thumbnail_reset(th);
   check("retire", "reset returns before the feeder is released",
         cpu_features_get_time_usec() - start < 100000
         && !retro_atomic_load_acquire_int(&retire_released));
   check("retire", "reset detaches playback and producer slots",
         !th->anim_player && !th->anim_surface && !th->anim_lifetime);
   check("retire", "reset retains the producer texture",
         hp.texture_unloads == unloads);
   free(th);
   retro_atomic_store_release_int(&retire_released, 1);
   retro_eventcount_notify(&retire_wake);
   if (watchdog)
      sthread_join(watchdog);
   deadline = cpu_features_get_time_usec() + 5000000;
   do
   {
      gfx_thumbnail_flush_requests();
      if (hp.texture_unloads != unloads)
         break;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < deadline);
   check("retire", "completed worker and polling reclaim the retired texture",
         hp.texture_unloads == unloads + 1);
   gfx_thumbnail_anim_worker_deinit();
   retro_eventcount_free(&retire_wake);
}
#endif

typedef struct cpu_media_capture
{
   unsigned frames;
   uint32_t first_hash;
} cpu_media_capture_t;

static uint32_t cpu_media_hash(const uint32_t *pixels, unsigned width,
      unsigned height, bool native)
{
   size_t i, count = (size_t)width * height;
   uint32_t hash = 2166136261U;
   for (i = 0; i < count; i++)
   {
      uint32_t v = pixels[i];
      if (!native) v = (v & 0xff00ff00U)
         | ((v & 0xffU) << 16) | ((v >> 16) & 0xffU);
      hash = (hash ^ (v & 0xffffffU)) * 16777619U;
   }
   return hash;
}

static bool cpu_media_capture(void *user, const video_player_frame_t *frame)
{
   cpu_media_capture_t *c = (cpu_media_capture_t*)user;
   if (!frame->pixels || frame->hw || !frame->width || !frame->height) return false;
   if (!c->frames) c->first_hash = cpu_media_hash(frame->pixels,
         frame->width, frame->height, frame->native);
   c->frames++;
   return true;
}

static void lane_group_feed_budget(const char *path)
{
   gfx_anim_preview_t *p = gfx_anim_preview_open(path, -1);
   unsigned frames = 0, bounded = 0;
   int i;
   size_t peak_step = 0;
   check("group feed", "owner session opened", p != NULL);
   if (!p) return;
   for (i = 0; i < 4000 && frames < 48; i++)
   {
      size_t lo, hi, old = p->feed_res_hi;
      size_t anchor = image_transfer_anim_stream_feed_frontier(p->stream, p->type);
      image_transfer_anim_stream_next_span(p->stream, p->type, &lo, &hi);
      if (!gfx_anim_preview_feed(p)) break;
      if (hi > lo && hi > anchor && hi - anchor > p->feed_ahead
            && old >= anchor && p->feed_res_hi > old)
      {
         size_t step = p->feed_res_hi - old;
         if (step > peak_step) peak_step = step;
         bounded++;
      }
      if (gfx_anim_preview_next(p, NULL, NULL)) frames++;
   }
   printf("group feed: frames=%u paced_steps=%u largest_step=%lu\n",
         frames, bounded, (unsigned long)peak_step);
   check("group feed", "large group admitted over multiple polls", frames >= 48 && bounded > 1);
   check("group feed", "each blocked-element refill respects I/O budget",
         peak_step && peak_step <= GFX_ANIM_PREVIEW_FEED_BUDGET);
   gfx_anim_preview_close(p);
}

static bool group_reference(const char *path, unsigned index, uint32_t *hash)
{
   gfx_anim_preview_t *p = gfx_anim_preview_open(path, -1);
   unsigned frame = 0;
   int tick;
   bool ok = false;
   if (!p) return false;
   for (tick = 0; tick < 5000; tick++)
   {
      bool native = false;
      const uint32_t *pixels;
      if (!gfx_anim_preview_feed(p)) break;
      pixels = gfx_anim_preview_next(p, NULL, &native);
      if (pixels && frame++ == index)
      {
         *hash = cpu_media_hash(pixels, VIDEO_SCALE_W(p->dims), VIDEO_SCALE_H(p->dims), native);
         ok = true; break;
      }
   }
   gfx_anim_preview_close(p);
   return ok;
}

static void lane_group_seek(const char *path)
{
   unsigned pass;
   uint32_t expected[2] = { 0, 0 };
   if (!group_reference(path, 36, &expected[0]) || !group_reference(path, 12, &expected[1]))
   { check("group seek", "sequential reference pictures available", false); return; }
   for (pass = 0; pass < 3; pass++)
   {
      gfx_anim_preview_t *p = gfx_anim_preview_open(path, -1);
      int result = 0, tick;
      int64_t position = -1;
      int64_t target = pass ? 400000 : 1200000;
      bool blocked = false, native = false;
      const uint32_t *pixels = NULL;
      size_t lo = 0, hi = 0, old_hi = 0;
      check("group seek", "windowed owner opened", p != NULL);
      if (!p) continue;
      gfx_anim_preview_set_paused(p, true);
      check("group seek", "seek request accepted", gfx_anim_preview_seek_request(p, 0, false));
      for (tick = 0; tick < 512 && (result == 0 || result == VIDEO_PLAYER_SEEK_PROGRESS); tick++)
      {
         result = gfx_anim_preview_seek_step(p, 1200000, tick == 0, &position);
         image_transfer_anim_stream_next_span(p->stream, p->type, &lo, &hi);
         if (hi > lo && hi > p->feed_res_hi && hi - lo > p->feed_ahead)
         {
            blocked = true; old_hi = hi;
            if (pass) break;
         }
      }
      check("group seek", "large admission actually encountered", blocked);
      if (pass == 2)
      {
         gfx_anim_preview_close(p);
         check("group seek", "owner closes during incomplete admission", blocked);
         continue;
      }
      if (pass)
      {
         result = gfx_anim_preview_seek_step(p, target, true, &position);
         image_transfer_anim_stream_next_span(p->stream, p->type, &lo, &hi);
         check("group seek", "replacement clears old element request", hi != old_hi);
         for (tick = 0; tick < 512 && (result == 0 || result == VIDEO_PLAYER_SEEK_PROGRESS); tick++)
            result = gfx_anim_preview_seek_step(p, target, false, &position);
      }
      printf("group seek: replacement=%u result=%d polls=%d position=%ld\n",
            pass, result, tick, (long)position);
      check("group seek", "seek completes at selected target", result == 1 && position == target);
      if (result == 1)
      {
         gfx_anim_preview_seek_commit(p, position);
         for (tick = 0; tick < 512 && !pixels; tick++)
         {
            gfx_anim_preview_feed(p);
            pixels = gfx_anim_preview_next(p, NULL, &native);
         }
         check("group seek", "accepted target matches sequential picture",
               pixels && cpu_media_hash(pixels, VIDEO_SCALE_W(p->dims),
                  VIDEO_SCALE_H(p->dims), native) == expected[pass]);
      }
      gfx_anim_preview_close(p);
   }
}

static void lane_cpu_gap(const char *path)
{
   gfx_anim_preview_t *session = gfx_anim_preview_open(path, -1);
   gfx_media_cpu_t *player;
   cpu_media_capture_t capture;
   int tick;
   size_t peak = 0, bound;
   unsigned target = getenv("CPU_GAP_FRAMES") ? (unsigned)strtoul(getenv("CPU_GAP_FRAMES"), NULL, 10) : 8;
   int max_ticks = getenv("CPU_GAP_TICKS") ? atoi(getenv("CPU_GAP_TICKS")) : 4000;
   if (max_ticks < 4000 || max_ticks > 60000) max_ticks = 4000;
   if (getenv("CPU_GROUP")) lane_group_feed_budget(path);
   if (target < 8 || target > 1000) target = 8;
   memset(&capture, 0, sizeof(capture));
   check("cpu gap", "windowed session opened", session != NULL);
   if (!session) return;
   player = gfx_media_cpu_new(session, cpu_media_capture, &capture, 0);
   check("cpu gap", "CPU player created", player != NULL);
   if (!player) { gfx_anim_preview_close(session); return; }
   for (tick = 0; tick < max_ticks && capture.frames < target; tick++)
   {
      size_t resident;
      gfx_media_cpu_poll(player, 1000000 + tick * 2000);
      resident = gfx_anim_preview_resident_bytes(session);
      if (resident > peak) peak = resident;
      retro_sleep(1);
   }
   printf("cpu gap: pictures=%u ticks=%d resident=%lu size=%lu observed_peak=%lu\n", capture.frames,
         tick, (unsigned long)gfx_anim_preview_resident_bytes(session), (unsigned long)session->len, (unsigned long)peak);
   check("cpu gap", "playback crosses metadata gap", capture.frames >= target);
   check("cpu gap", "reservation used for gap traversal", data_transfer_window_is_reserved(session->dt));
   /* A noisy file's ordinary lookahead/back margin can exceed half its
    * encoded length. Bound that case by the configured window plus one
    * coded picture (the fixture generator checks its raw-picture bound),
    * refill quantum and page rounding. Reading the 12 MiB hole would
    * exceed this allowance. The small-header case has a tighter bound. */
   bound = target == 8 ? session->len / 2
      : GFX_ANIM_PREVIEW_WINDOW_KEEP + session->feed_ahead + session->feed_back
        + (size_t)VIDEO_SCALE_W(session->dims) * VIDEO_SCALE_H(session->dims) * 4
        + GFX_ANIM_PREVIEW_FEED_BUDGET + 128 * 1024;
   printf("cpu gap: observed footprint bound=%lu\n", (unsigned long)bound);
   if (getenv("CPU_GROUP"))
   {
      /* One checked-size key picture and 12 MiB of legal group padding.
       * Account for this complete element instead of ordinary lookahead. */
      size_t group = 12 * 1024 * 1024
         + (size_t)VIDEO_SCALE_W(session->dims) * VIDEO_SCALE_H(session->dims) * 4 + 32;
      if (group > session->feed_ahead) bound += group - session->feed_ahead;
      printf("cpu group: declared-element footprint bound=%lu\n", (unsigned long)bound);
   }
   check("cpu gap", "observed footprint stays within retained-window budget", peak < bound);
   gfx_media_cpu_retire(player);
   gfx_thumbnail_anim_worker_deinit();
   gfx_media_cpu_reap();
}

typedef struct frontend_timeline_capture
{
   unsigned frames;
   int64_t now, first, second;
} frontend_timeline_capture_t;

static bool frontend_timeline_present(void *user, const video_player_frame_t *frame)
{
   frontend_timeline_capture_t *c = (frontend_timeline_capture_t*)user;
   if (!frame->pixels || frame->hw) return false;
   if (!c->frames) c->first = c->now;
   if (c->frames == 24) c->second = c->now;
   c->frames++;
   return true;
}

/* Video occupies movie seconds 1..3; audio occupies 0..4. The next
 * video loop must begin at second5, including the movie tail and lead. */
static bool frontend_timing_read(const char *path, int64_t *period, int64_t times[24])
{
   size_t n = strlen(path);
   char *name = (char*)malloc(n + 8);
   FILE *f;
   unsigned i, sec, ns;
   if (!name) return false;
   memcpy(name,path,n);memcpy(name+n,".timing",8);
   f=fopen(name,"r");free(name);
   if(!f)
   {
      if(strstr(path,".webm"))return false;
      *period=(int64_t)4000000*1000;
      for(i=0;i<24;i++)times[i]=1000000000+(int64_t)i*1000000000/12;
      return true;
   }
   for(i=0;i<25;i++)
   {
      int64_t value;
      if(fscanf(f,"%u %u",&sec,&ns)!=2||ns>=1000000000){fclose(f);return false;}
      value=(int64_t)sec*1000000000+ns;
      if(!i)*period=value;else times[i-1]=value;
   }
   fclose(f);return true;
}

static void lane_frontend_timeline(const char *path)
{
   gfx_anim_preview_t *session;
   gfx_media_cpu_t *player;
   frontend_timeline_capture_t capture;
   float *pcm;
   int tick, before_video_audio = 0, startup_voice = 0;
   int old_audio = hp.force_preview_audio;
   int64_t first_audio = -1;
   int64_t period, expected_times[24], seek_time;
   const int64_t origin = 1000000;
   bool timing=frontend_timing_read(path,&period,expected_times);
   check("frontend timeline","independent timestamp expectations available",timing);
   if(!timing)return;
   seek_time=expected_times[0];
   for(tick=0;tick<24;tick++)if(expected_times[tick]>=1500000000){seek_time=expected_times[tick];break;}
   memset(&capture,0,sizeof(capture));capture.first=capture.second=-1;
   hp.force_preview_audio=1;audio_mixer_init(48000);
   {
      gfx_anim_preview_t *reference=gfx_anim_preview_open(path,-1);
      check("frontend timeline","still-reference session opened",reference!=NULL);
      if(reference)
      {
         unsigned i;
         check("frontend timeline","timestamp unavailable before first decode",
               image_transfer_anim_stream_frame_time_ns(reference->stream,reference->type)==-1);
         check("frontend timeline","movie duration includes continuous audio",
               image_transfer_anim_stream_duration_ns(reference->stream,reference->type)==period);
         for(i=0;i<24;i++)
         {
            bool native=false;
            const uint32_t *pixels;
            int tries;
            for(tries=0;tries<100;tries++)
            {
               gfx_anim_preview_feed(reference);
               pixels=gfx_anim_preview_next(reference,NULL,&native);
               if(pixels)break;
            }
            check("frontend timeline","still/reference decode remains immediately available",tries<100);
            check("frontend timeline","decoded frame exposes exact movie timestamp",
                  image_transfer_anim_stream_frame_time_ns(reference->stream,reference->type)
                     ==expected_times[i]);
         }
         image_transfer_anim_stream_rewind(reference->stream,reference->type);
         check("frontend timeline","rewind clears current-frame timestamp",
               image_transfer_anim_stream_frame_time_ns(reference->stream,reference->type)==-1);
         check("frontend timeline","movie-coordinate reference seek accepted",
               image_transfer_anim_stream_seek_begin(reference->stream,reference->type,1500000));
         for(i=0;i<100;i++)
         {
            int64_t position=0;
            gfx_anim_preview_feed(reference);
            if(image_transfer_anim_stream_seek_step(reference->stream,reference->type,&position)==1)
            {
               check("frontend timeline","seek translates movie to decoder coordinates",position==seek_time/1000);
               break;
            }
         }
         check("frontend timeline","bounded reference seek finishes",i<100);
         for(i=0;i<100;i++)
         {
            bool native=false;
            gfx_anim_preview_feed(reference);
            if(gfx_anim_preview_next(reference,NULL,&native))break;
         }
         check("frontend timeline","next reference picture retains movie timestamp",
               i<100&&image_transfer_anim_stream_frame_time_ns(reference->stream,reference->type)==seek_time);
         gfx_anim_preview_close(reference);
      }
   }
   session=gfx_anim_preview_open(path,-1);
   check("frontend timeline","real session opened",session!=NULL);
   if(!session){audio_mixer_done();hp.force_preview_audio=old_audio;return;}
   pcm=(float*)calloc(960,sizeof(*pcm));
   player=gfx_media_cpu_new(session,frontend_timeline_present,&capture,0);
   check("frontend timeline","actual CPU owner and mixer buffer",player&&pcm);
   if(!player||!pcm)
   {
      if(player)gfx_media_cpu_retire(player);else gfx_anim_preview_close(session);
      free(pcm);audio_mixer_done();hp.force_preview_audio=old_audio;return;
   }
   for(tick=0;tick<=600;tick++)
   {
      unsigned i;
      capture.now=(int64_t)tick*10000;
      gfx_media_cpu_poll(player,origin+capture.now);
      if(tick==10)startup_voice=session->audio_voice!=NULL;
      memset(pcm,0,960*sizeof(*pcm));
      audio_mixer_mix(pcm,480,1.0f,false);
      for(i=0;i<960;i++)if(pcm[i]>0.00001f||pcm[i]<-0.00001f)
      {
         if(first_audio<0)first_audio=capture.now;
         if(!capture.frames)before_video_audio=1;
         break;
      }
      retro_sleep(1);
   }
   check("frontend timeline","audio voice exists by 100ms",startup_voice);
   check("frontend timeline","audible PCM precedes first video",before_video_audio);
   check("frontend timeline","audio starts near movie origin",first_audio>=0&&first_audio<=100000);
   check("frontend timeline","video first interval starts at one second",
         capture.first>=950000&&capture.first<=1100000);
   check("frontend timeline","second video loop keeps full movie period",
         capture.second>=4950000&&capture.second<=5100000);
   {
      gfx_media_playback_state_t state;
      check("frontend timeline","looping controls expose current movie position",
            gfx_media_cpu_state(player,origin+6000000,&state)
            &&state.position_us>=1990000&&state.position_us<=2030000);
   }
   printf("frontend timeline: first_video=%ld second_video=%ld first_audio=%ld pictures=%u voice_at100ms=%d audio_before_video=%d\n",
         (long)capture.first,(long)capture.second,(long)first_audio,capture.frames,startup_voice,before_video_audio);
   {
      gfx_media_playback_state_t state;
      unsigned held = capture.frames;
      int64_t resumed_first = -1;
      const int64_t wall = origin + 6000000;
      check("frontend timeline","pause before leading-gap seek",gfx_media_cpu_pause(player,true,wall));
      check("frontend timeline","seek movie 500ms",gfx_media_cpu_seek(player,500000,48000,false,wall));
      for(tick=0;tick<2000;tick++)
      {
         gfx_media_cpu_poll(player,wall);
         if(!session->seeking)break;
         retro_sleep(1);
      }
      check("frontend timeline","leading-gap seek completes while paused",tick<2000);
      check("frontend timeline","leading-gap seek preserves requested movie/audio position",
            gfx_media_cpu_state(player,wall,&state)&&state.position_us==500000&&state.paused);
      for(tick=0;tick<10;tick++)gfx_media_cpu_poll(player,wall+1000000);
      check("frontend timeline","paused leading gap does not expose future video",capture.frames==held);
      gfx_media_cpu_pause(player,false,wall+2000000);
      for(tick=0;tick<=70;tick++)
      {
         capture.now=(int64_t)tick*10000;
         gfx_media_cpu_poll(player,wall+2000000+capture.now);
         if(capture.frames>held&&resumed_first<0)resumed_first=capture.now;
         memset(pcm,0,960*sizeof(*pcm));
         audio_mixer_mix(pcm,480,1.0f,false);
         retro_sleep(1);
      }
      check("frontend timeline","resume preserves remaining 500ms video gap",
            resumed_first>=490000&&resumed_first<=550000);
      printf("frontend leading seek: position=%ld resumed_first=%ld\n",
            (long)state.position_us,(long)resumed_first);
      gfx_media_cpu_pause(player,true,wall+2700000);
      held=capture.frames;
      check("frontend timeline","seek into video tail accepted",
            gfx_media_cpu_seek(player,3500000,48000,false,wall+2700000));
      for(tick=0;tick<2000;tick++)
      {
         gfx_media_cpu_poll(player,wall+2700000);
         if(!session->seeking)break;
         retro_sleep(1);
      }
      check("frontend timeline","tail seek holds last picture at requested audio position",
            tick<2000&&capture.frames==held+1
            &&gfx_media_cpu_state(player,wall+2700000,&state)&&state.position_us==3500000&&state.paused);
      held=capture.frames;resumed_first=-1;
      gfx_media_cpu_pause(player,false,wall+3000000);
      for(tick=0;tick<=170;tick++)
      {
         capture.now=(int64_t)tick*10000;
         gfx_media_cpu_poll(player,wall+3000000+capture.now);
         if(capture.frames>held&&resumed_first<0)resumed_first=capture.now;
         memset(pcm,0,960*sizeof(*pcm));audio_mixer_mix(pcm,480,1.0f,false);
         retro_sleep(1);
      }
      check("frontend timeline","tail seek retains tail and next-loop lead",
            resumed_first>=1490000&&resumed_first<=1550000);
      printf("frontend tail seek: position=%ld resumed_first=%ld\n",
            (long)state.position_us,(long)resumed_first);
   }
   gfx_media_cpu_retire(player);gfx_thumbnail_anim_worker_deinit();gfx_media_cpu_reap();
   audio_mixer_done();free(pcm);hp.force_preview_audio=old_audio;
}

/* Exercise the actual thumbnail/surface owner as used by texture menus,
 * independently of the CPU owner used by RGUI. No GPU readback. */
static void lane_frontend_texture_timeline(const char *path)
{
   gfx_thumbnail_t th;
   float *pcm = (float*)calloc(960,sizeof(*pcm));
   int tick, first = -1, second = -1, audio_before = 0;
   memset(&th,0,sizeof(th));memset(&hp,0,sizeof(hp));
   hp.force_preview_audio=1;audio_mixer_init(48000);
   gfx_thumbnail_init_blank(&th);
   gfx_thumbnail_request_file(path,&th,0);
   check("texture timeline","actual texture owner opened",th.anim_player!=NULL&&pcm!=NULL);
   if(th.anim_player&&pcm)
      for(tick=0;tick<=600;tick++)
      {
         unsigned i;
         gfx_thumbnail_animate(&th,1000000+(int64_t)tick*10000);
         if(hp.texture_uploads&&first<0)first=tick*10000;
         if(hp.texture_uploads>=25&&second<0)second=tick*10000;
         memset(pcm,0,960*sizeof(*pcm));audio_mixer_mix(pcm,480,1.0f,false);
         if(!hp.texture_uploads)
            for(i=0;i<960;i++)if(pcm[i]>0.00001f||pcm[i]<-0.00001f){audio_before=1;break;}
         retro_sleep(1);
      }
   check("texture timeline","audio precedes first texture",audio_before);
   check("texture timeline","first texture at one second",first>=950000&&first<=1100000);
   check("texture timeline","second texture loop at five seconds",second>=4950000&&second<=5100000);
   check("texture timeline","first due picture becomes drawable",
         th.texture&&th.status==GFX_THUMBNAIL_STATUS_AVAILABLE&&(th.alpha>0.0f||hp.fade_pushes));
   printf("texture timeline: first=%d second=%d uploads=%d audio_before=%d\n",
         first,second,hp.texture_uploads,audio_before);
   gfx_thumbnail_reset(&th);gfx_thumbnail_anim_worker_deinit();
   audio_mixer_done();free(pcm);
}

static void lane_cpu_media(const char *path)
{
   gfx_anim_preview_t *reference = gfx_anim_preview_open(path, -1);
   gfx_anim_preview_t *session;
   gfx_media_cpu_t *player;
   gfx_media_playback_state_t state;
   cpu_media_capture_t capture;
   const uint32_t *pixels = NULL;
   uint32_t expected = 0;
   int duration = 0, tick;
   bool native = false, closed;
   /* Frozen logical time must not freeze the real work budget. */
   int64_t now = 1000000;
   unsigned held;
   int uploads = hp.texture_uploads;
   memset(&capture, 0, sizeof(capture));
   check("cpu media", "software reference opened", reference != NULL);
   if (!reference) return;
   for (tick = 0; tick < 2000 && !pixels; tick++)
   {
      gfx_anim_preview_feed(reference);
      pixels = gfx_anim_preview_next(reference, &duration, &native);
   }
   check("cpu media", "reference first picture decoded", pixels != NULL);
   if (pixels) expected = cpu_media_hash(pixels, VIDEO_SCALE_W(reference->dims),
         VIDEO_SCALE_H(reference->dims), native);
   gfx_anim_preview_close(reference);
   session = gfx_anim_preview_open(path, -1);
   check("cpu media", "framebuffer session opened", session != NULL);
   if (!session) return;
#ifdef PREVIEW_THREADED
   player = gfx_media_cpu_new(session, cpu_media_capture, &capture, 0);
#else
   player = gfx_media_cpu_new(session, cpu_media_capture, &capture, 0);
#endif
   check("cpu media", "framebuffer sink created", player != NULL);
   if (!player) { gfx_anim_preview_close(session); return; }
   for (tick = 0; tick < 2000 && !capture.frames; tick++)
   {
      gfx_media_cpu_poll(player, now + tick * 1000);
      retro_sleep(1);
   }
   check("cpu media", "CPU picture matches reference pixels",
         capture.frames && capture.first_hash == expected);
   check("cpu media", "CPU transport pauses", gfx_media_cpu_pause(player, true, now + 1000));
   held = capture.frames;
   for (tick = 0; tick < 10; tick++) gfx_media_cpu_poll(player, now + 3000000 + tick * 1000);
   check("cpu media", "paused CPU picture retained", capture.frames == held);
   check("cpu media", "CPU seek accepts first target",
         gfx_media_cpu_seek(player, 1250000, 0, false, now + 4000000));
   gfx_media_cpu_poll(player, now + 4000000);
   check("cpu media", "CPU seek replaces pending target",
         gfx_media_cpu_seek(player, 400000, 0, false, now + 4000001));
   check("cpu media", "CPU snapshot follows replacement",
         gfx_media_cpu_state(player, now + 4000001, &state)
         && state.paused && state.seeking && state.position_us == 400000);
   for (tick = 0; tick < 2000; tick++)
   {
      gfx_media_cpu_poll(player, now + 4001000 + tick * 1000);
      if (gfx_media_cpu_state(player, now + 4001000 + tick * 1000, &state)
            && !state.seeking) break;
      retro_sleep(1);
   }
   check("cpu media", "CPU seek commits target while paused",
         tick < 2000 && state.paused && state.position_us > 100000
         && state.position_us <= 400000 && capture.frames > held);
   check("cpu media", "CPU transport used no texture upload", hp.texture_uploads == uploads);
   check("cpu media", "CPU closing during seek accepted",
         gfx_media_cpu_seek(player, 1000000, 0, false, now + 7000000));
   gfx_media_cpu_poll(player, now + 7000000);
   closed = gfx_media_cpu_free(player);
   for (tick = 0; tick < 2000 && !closed; tick++)
   {
      retro_sleep(1);
      closed = gfx_media_cpu_free(player);
   }
   check("cpu media", "CPU retire completes without blocking free", closed);
}

static void lane_cpu_retirement(const char *path)
{
   unsigned i;
   int64_t now = cpu_features_get_time_usec();
   for (i = 0; i < 32; i++)
   {
      gfx_anim_preview_t *session = gfx_anim_preview_open(path, -1);
      cpu_media_capture_t *capture = (cpu_media_capture_t*)calloc(1, sizeof(*capture));
      gfx_media_cpu_t *player;
      check("cpu retire", "retirement fixture and callback allocated", session && capture);
      if (!session || !capture)
      {
         if (session) gfx_anim_preview_close(session);
         free(capture); break;
      }
#ifdef PREVIEW_THREADED
      player = gfx_media_cpu_new(session, cpu_media_capture, capture, 0);
#else
      player = gfx_media_cpu_new(session, cpu_media_capture, capture, VIDEO_PLAYER_INLINE);
#endif
      check("cpu retire", "retirement player created", player != NULL);
      if (!player) { gfx_anim_preview_close(session); free(capture); break; }
      gfx_media_cpu_poll(player, now);
      gfx_media_cpu_seek(player, 400000, 0, false, now + 1);
      gfx_media_cpu_poll(player, now + 1);
      gfx_media_cpu_retire(player);
      free(capture);
      gfx_media_cpu_reap();
   }
   gfx_thumbnail_anim_worker_deinit();
   gfx_media_cpu_reap();
}

static void lane_mp4_packet_span(const char *path)
{
   gfx_anim_preview_t *p = gfx_anim_preview_open(path, -1);
   size_t lo = 0, hi = 0;
   unsigned polls = 0;
   bool native = false;
   const uint32_t *pixels;
   check("MP4 packet span", "windowed MP4 session opens",
         p && p->windowed && p->type == IMAGE_TYPE_MP4);
   if (!p) return;
   image_transfer_anim_stream_next_span(p->stream, p->type, &lo, &hi);
   check("MP4 packet span", "sample metadata names a large distant packet",
         lo > GFX_ANIM_PREVIEW_WINDOW_KEEP
         && hi > lo && hi - lo > GFX_ANIM_PREVIEW_FEED_BUDGET);
   if (!(lo > GFX_ANIM_PREVIEW_WINDOW_KEEP && hi > lo
            && hi - lo > GFX_ANIM_PREVIEW_FEED_BUDGET))
   {
      gfx_anim_preview_close(p);
      return;
   }
   data_transfer_window_rewind(p->dt);
   data_transfer_window_skip(p->dt, lo);
   p->feed_tell = p->feed_res_hi = lo;
   p->feed_ahead = 1;
   p->feed_back = 0;
   while (p->feed_res_hi < hi && polls < 64)
   {
      size_t before = p->feed_res_hi;
      bool fed = gfx_anim_preview_feed(p);
      check("MP4 packet span", "packet admission advances within the I/O budget",
            fed && p->feed_res_hi > before
            && p->feed_res_hi - before <= GFX_ANIM_PREVIEW_FEED_BUDGET
            && p->feed_res_hi <= hi);
      polls++;
      if (!fed || p->feed_res_hi <= before) break;
   }
   printf("MP4 packet span: offset=%lu bytes=%lu polls=%u admitted=%lu\n",
         (unsigned long)lo, (unsigned long)(hi - lo), polls,
         (unsigned long)(p->feed_res_hi - lo));
   check("MP4 packet span", "complete packet admitted over multiple polls",
         polls > 1 && p->feed_res_hi == hi);
   pixels = gfx_anim_preview_next(p, NULL, &native);
   check("MP4 packet span", "the admitted packet decodes its picture", pixels != NULL);
   gfx_anim_preview_close(p);
}

int main(int argc, char **argv)
{
   int i;
   if (argc < 2)
   {
      printf("usage: %s <video> [more...]\n", argv[0]);
      return 2;
   }
   if (getenv("MP4_PACKET_SPAN"))
   {
      lane_mp4_packet_span(argv[1]);
      return fails ? 1 : 0;
   }
   if (getenv("FRONTEND_TIMELINE"))
   {
      lane_frontend_timeline(argv[1]);
      lane_frontend_texture_timeline(argv[1]);
      return fails ? 1 : 0;
   }
   if (getenv("GROUP_SEEK"))
   {
      lane_group_seek(argv[1]);
      return fails ? 1 : 0;
   }
   if (getenv("CPU_GAP"))
   {
      lane_cpu_gap(argv[1]);
      return fails ? 1 : 0;
   }
#ifdef PREVIEW_THREADED
   lane_retire(argv[1]);
#endif
   if (getenv("CPU_MEDIA"))
   {
      lane_cpu_media(argv[1]);
      lane_cpu_retirement(argv[1]);
   }
   lane_savestate_thumbnail_path();
   lane_still_sets();
   for (i = 1; i < argc; i++)
      run(argv[i], argv[i], 1);

   /* Second pass over the same files with address-space reservation
    * refused (MEMMAP_TEST_NO_RESERVE build + this env), so the whole-
    * file mapping path is exercised: the windowed flag must be false
    * and RSS must not balloon to the file size on the huge sparse
    * fixtures. This is the path the companion-UI merge regressed. */
   env_set("MEMMAP_NO_RESERVE", "1");
   for (i = 1; i < argc; i++)
   {
      char lbl[512];
      snprintf(lbl, sizeof(lbl), "%s [noreserve]", argv[i]);
      run(argv[i], lbl, 2);
   }
   env_set("MEMMAP_NO_RESERVE", NULL);

   printf("\n%s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails,
         fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
