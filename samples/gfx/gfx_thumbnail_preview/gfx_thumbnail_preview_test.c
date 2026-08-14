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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "harness.h"
#include "../../../gfx/gfx_thumbnail.h"

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

static void run(const char *path, const char *label, int expect_video)
{
   gfx_thumbnail_t th;
   int i;
   double rss0, peak;

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
   hp.force_preview_audio = getenv("NOAUDIO") ? 0 : 1;
   gfx_thumbnail_reset(&th);

   rss0 = rss_mib();
   printf("  %s\n", label);

   gfx_thumbnail_request_file(path, &th, 0);

   {
      const char *fe = getenv("FRAMES");
      int nf = fe ? atoi(fe) : 12;
      double after_open = rss_mib();
      for (i = 0; i < nf; i++)
         gfx_thumbnail_animate(&th);
      peak = rss_mib();
      printf("      RSS after open=%.1f MiB, after %d frames=%.1f MiB\n",
            after_open, nf, peak);
   }

   printf("      audio_streams=%d bytes=%llu\n", hp.audio_streams,
         (unsigned long long)hp.last_audio_bytes);
   printf("      status=%d alpha=%.3f tex=%lu uploads=%d fades=%d "
          "stills=%d %ux%u\n",
         (int)th.status, th.alpha, (unsigned long)th.texture,
         hp.texture_uploads, hp.fade_pushes, hp.still_loads,
         hp.last_tex_w, hp.last_tex_h);

   if (expect_video)
   {
      check(label, "R1 texture uploaded",  hp.texture_uploads > 0);
      check(label, "R2 status AVAILABLE",  th.status == GFX_THUMBNAIL_STATUS_AVAILABLE);
      check(label, "R3 drawable (alpha>0 or fade pushed)",
            (th.alpha > 0.0f) || (hp.fade_pushes > 0));
      check(label, "R4 no whole-file still load queued", hp.still_loads == 0);
      check(label, "R5 RSS growth < 600 MiB", (peak - rss0) < 600.0);
      /* Preview audio is bounded by gfx_thumb_anim_mem_ok, so a file
       * past GFX_THUMB_ANIM_ABS_MAX_FILE is meant to be silent - that
       * is policy, not breakage.  Under the cap it must start AND get
       * the whole container: handing the mixer a short buffer is the
       * failure 9650f04 papered over by skipping the hand-off. */
      if (1)
      {
         check(label, "A6 preview audio started", hp.audio_streams > 0);
         check(label, "A7 mixer got the whole container",
               hp.last_audio_bytes == (size_t)file_len(path));
      }
      else
      {
         /* No cap any more: the window costs its slide, not the
          * file, so even a 7 GB recording gets audio. */
         check(label, "A6 preview audio started", hp.audio_streams > 0);
         check(label, "A7 mixer sees the whole container",
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

int main(int argc, char **argv)
{
   int i;
   if (argc < 2)
   {
      printf("usage: %s <video> [more...]\n", argv[0]);
      return 2;
   }
   for (i = 1; i < argc; i++)
      run(argv[i], argv[i], 1);

   printf("\n%s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails,
         fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
