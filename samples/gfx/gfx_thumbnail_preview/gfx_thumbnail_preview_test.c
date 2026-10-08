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
#include <retro_timers.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "harness.h"
#include "../../../gfx/gfx_thumbnail.h"
#include "../../../gfx/gfx_anim_preview.h"
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

   gfx_thumbnail_reset(&th);
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
      snprintf(full, sizeof(full), "/tmp/%s", names[i]);
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
      gfx_thumbnail_set_content_image(pd, "/tmp", names[i]);
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
      gfx_thumbnail_set_content_image(pd, "/tmp", names[i]);
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
   gfx_thumbnail_set_content_image(pd, "/tmp", names[0]);
   gfx_thumbnail_request(pd, GFX_THUMBNAIL_RIGHT, NULL, 0, &th[0], 0, false);
   check("sets", "an unthreaded queue takes each still at once",
         hp.still_loads == 1 && hp.still_sets == 0);

   for (i = 0; i < SET_STILLS; i++)
   {
      char full[64];
      gfx_thumbnail_reset(&th[i]);
      snprintf(full, sizeof(full), "/tmp/%s", names[i]);
      remove(full);
   }
   free(pd);
   hp.thumb_cfg      = 0;
   hp.still_loads    = 0;
}

int main(int argc, char **argv)
{
   int i;
   if (argc < 2)
   {
      printf("usage: %s <video> [more...]\n", argv[0]);
      return 2;
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
