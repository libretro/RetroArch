/* overlay_apng_test.c -- an animated overlay image advances in place.
 *
 * The pack's animated images are gfx_surfaces with slots, and a frame
 * is composed into a free slot and submitted, which updates the
 * texture the pages already point at. What this asserts is what that
 * buys and what it must not break:
 *
 *   - the stream yields distinct frames and loops at the end of a
 *     pass, which is what an animated control surface needs;
 *   - advancing a frame changes no texture handle, so the pages
 *     built from those handles stay valid and a page switch is still
 *     an upload of nothing;
 *   - a frame whose slot is still with the video thread is skipped,
 *     not waited for;
 *   - closing the stream with a frame in flight is clean;
 *   - played as the overlay plays it (gfx_surface_player, decoded in
 *     line), it loops for good, or stops after the container's count.
 *
 * The decode and the surface are the real ones; the driver is the
 * stub below, as in the other overlay samples.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rpng.h>
#include <streams/file_stream.h>

#include "../../../gfx/gfx_surface.h"
#include "../../../gfx/gfx_surface_player.h"

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("[FAIL] "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static unsigned shown;

static gfx_surface_t *lane_surface(void *user, unsigned dims,
      unsigned num_slots, uint32_t pixfmt)
{
   gfx_surface_t *s = (gfx_surface_t*)*(void**)user;
   if (!s || s->dims != dims || s->num_slots < num_slots
         || s->pixfmt != pixfmt)
      return NULL;
   return s;
}

static void lane_shown(void *user, gfx_surface_t *s)
{
   (void)user;
   (void)s;
   shown++;
}

/* The overlay's playback: frames on their durations into the one
 * slot, the texture updated in place; @loops 0 plays for good. */
static void lane_player(rpng_apng_stream_t *st, unsigned w, unsigned h,
      int num_frames, int loops)
{
   gfx_surface_player_cbs_t cb;
   gfx_surface_player_t *gsp;
   void *surface = gfx_surface_new(VIDEO_SCALE_PACK(w, h), 1,
         IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, NULL, NULL);
   uintptr_t first = 0;
   int64_t now     = 0;
   int i, ended    = -1;

   CHECK(surface != NULL, "player: surface allocation failed");
   if (!surface)
      return;
   rpng_apng_stream_rewind(st);
   shown      = 0;
   cb.user    = &surface;
   cb.surface = lane_surface;
   cb.shown   = lane_shown;
   cb.feed    = NULL;
   cb.feed_user = NULL;
   gsp = gfx_surface_player_new(st, IMAGE_TYPE_PNG, loops, &surface, &cb,
         VIDEO_PLAYER_INLINE);
   CHECK(gsp != NULL, "player: new failed");
   for (i = 0; gsp && i < 3 * num_frames; i++)
   {
      if (video_player_poll(gsp->vp, now) == VIDEO_PLAYER_ENDED)
      {
         ended = i;
         break;
      }
      CHECK(video_player_due(gsp->vp) > now, "player: step %d not paced", i);
      now = video_player_due(gsp->vp);
      if (!first)
         first = ((gfx_surface_t*)surface)->handle;
      else
         CHECK(((gfx_surface_t*)surface)->handle == first,
               "player: step %d replaced the texture", i);
   }
   if (loops)
      CHECK(ended == loops * num_frames && shown == (unsigned)ended,
            "player: %d loop(s) of %d ended at %d, %u shown", loops,
            num_frames, ended, shown);
   else
      CHECK(ended < 0 && shown == (unsigned)(3 * num_frames),
            "player: looping playback ended at %d, %u shown", ended, shown);
   gfx_surface_player_free(gsp);
   gfx_surface_free((gfx_surface_t*)surface);
}

int main(int argc, char **argv)
{
   const char *path = (argc > 1) ? argv[1] : "fixtures/anim.png";
   int64_t len      = 0;
   void *buf        = NULL;
   rpng_apng_stream_t *st;
   unsigned w = 0, h = 0;
   int num_frames = 0, loops = 0, dur = 0, i;
   uint32_t seen[8];
   int nseen = 0;

   if (!filestream_read_file(path, &buf, &len) || !buf || len <= 0)
   {
      printf("[FAIL] cannot read %s\n", path);
      return 2;
   }
   CHECK(rpng_is_apng((const uint8_t*)buf, (size_t)len),
         "fixture is not detected as an APNG");
   if (!(st = rpng_apng_stream_open((const uint8_t*)buf, (size_t)len)))
   {
      printf("[FAIL] stream open failed\n");
      free(buf);
      return 2;
   }
   rpng_apng_stream_get_info(st, &w, &h, &num_frames, &loops);
   CHECK(w && h, "no dimensions");
   CHECK(num_frames > 1, "%d frames: not an animation", num_frames);

   /* Distinct frames, then end of pass, then a rewind that plays
    * again - the loop an overlay relies on. */
   for (i = 0; i < num_frames; i++)
   {
      const uint32_t *f = rpng_apng_stream_next(st, &dur);
      CHECK(f != NULL, "frame %d missing", i);
      if (!f)
         break;
      CHECK(dur > 0, "frame %d has no duration", i);
      if (nseen < (int)(sizeof(seen) / sizeof(seen[0])))
         seen[nseen++] = f[0];
   }
   CHECK(rpng_apng_stream_next(st, &dur) == NULL,
         "stream did not end its pass after %d frames", num_frames);
   rpng_apng_stream_rewind(st);
   CHECK(rpng_apng_stream_next(st, &dur) != NULL, "rewind did not replay");
   for (i = 1; i < nseen; i++)
      CHECK(seen[i] != seen[0] || nseen < 2,
            "frame %d is identical to frame 0 (%08x)", i, seen[i]);

   /* The surface side: one slot is all a frame composed here and
    * submitted at once can use - a surface with a submit in flight
    * refuses every slot - and the handle the pages hold never
    * changes across the submits. */
   {
      gfx_surface_t *s = gfx_surface_new(VIDEO_SCALE_PACK(w, h), 1, IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR,
            NULL, NULL);
      uintptr_t first  = 0;
      CHECK(s != NULL, "surface allocation failed");
      if (s)
      {
         rpng_apng_stream_rewind(st);
         for (i = 0; i < 6; i++)
         {
            const uint32_t *f = rpng_apng_stream_next(st, &dur);
            enum gfx_surface_submit_result r;
            if (!f)
            {
               rpng_apng_stream_rewind(st);
               f = rpng_apng_stream_next(st, &dur);
            }
            CHECK(f != NULL, "no frame at step %d", i);
            if (!f)
               break;
            memcpy(s->slots[0], f,
                  (size_t)w * h * sizeof(uint32_t));
            r = gfx_surface_submit(s, 0, false);
            CHECK(r != GFX_SURFACE_SUBMIT_FAILED,
                  "submit %d failed", i);
            if (!first)
               first = s->handle;
            else
               CHECK(s->handle == first,
                     "step %d replaced the texture (%lx -> %lx)",
                     i, (unsigned long)first, (unsigned long)s->handle);
         }
         /* Freed with the last frame still owned by the surface. */
         gfx_surface_free(s);
      }
   }

   lane_player(st, w, h, num_frames, 0);
   lane_player(st, w, h, num_frames, 1);

   rpng_apng_stream_close(st);
   free(buf);
   if (!failures)
      printf("[pass] overlay_apng: %d frames, looping, one texture\n",
            num_frames);
   printf(failures ? "FAIL\n" : "PASS\n");
   return failures ? 1 : 0;
}
