/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_thumbnail.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <memory/mem_stats.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <features/features_cpu.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <lists/file_list.h>
#include <streams/file_stream.h>
#include <formats/image.h>
#include <queues/task_queue.h>

#include "gfx_display.h"
#include "gfx_anim_preview.h"
#include "gfx_animation.h"
#include <formats/data_transfer.h>

#include "gfx_surface.h"
#include "gfx_instrument.h"
#include "gfx_thumbnail.h"
#include "../frontend/frontend_driver.h"

#if (defined(HAVE_RWEBM) || defined(HAVE_RMP4)) && \
      defined(HAVE_AUDIOMIXER) && \
      (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS) || defined(HAVE_RAAC))
#ifdef HAVE_RWEBM
#include <formats/rwebm_audio.h>
#endif
#ifdef HAVE_RMP4
#include <formats/rmp4_audio.h>
#endif
#include <audio/audio_mixer.h>
#include "../audio/audio_driver.h"
#endif

/* Playback - pacing, loops, the decode worker - is video_player's;
 * the surface side of it gfx_surface_player's. */
#include "gfx_surface_player.h"

#include "../configuration.h"
#include "../msg_hash.h"
#include "../paths.h"
#include "../file_path_special.h"


#ifdef HAVE_MENU
/* Only for the playlist/state-name helper far below, which reads
 * menu_state directly.  The animation pacing above takes the frame's
 * time as a parameter and needs nothing from the menu. */
#include "../menu/menu_driver.h"
#endif

#include "../tasks/tasks_internal.h"
#include <compat/strl.h>

#define DEFAULT_GFX_THUMBNAIL_STREAM_DELAY  (50.0f) /* ms */
#define DEFAULT_GFX_THUMBNAIL_FADE_DURATION 166.66667f

/* The thumbnail .status field is atomically-typed (see the
 * gfx_thumbnail_t comment in gfx_thumbnail.h).  Reads and writes
 * therefore must go through the retro_atomic API rather than
 * plain assignment / dereference; on the C11 stdatomic and C++11
 * std::atomic backends, plain access to such a field is illegal.
 *
 * The two cross-thread sites in this file are noted at the
 * call sites:
 *  - The release-stores in gfx_thumbnail_handle_upload publish
 *    prior texture / width / height writes.
 *  - The acquire-load in gfx_thumbnail_draw pairs with those.
 * All other accesses are single-threaded; they go through the
 * retro_atomic API only because the field's atomic-typed
 * storage requires it.  The cost on weak-memory ARM/PowerPC is
 * one extra ldar/stlr per cold-path access; on x86 TSO the
 * barriers compile out entirely.
 *
 * The wrappers are static-inline (rather than function-like
 * macros) so callers have a clear function boundary.  GCC 13+
 * at -O3 emits a spurious -Wstringop-overflow on the inlined
 * __atomic_* primitive when called from gfx_thumbnail_request,
 * because the optimiser cannot prove the @c thumbnail argument
 * non-NULL across every `goto end:` flow-graph path; the
 * suppression below is a targeted fix that does not affect
 * codegen.  No effect on non-GCC backends. */

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12
#  define GFX_THUMB_STATUS_DIAG_PUSH \
   _Pragma("GCC diagnostic push") \
   _Pragma("GCC diagnostic ignored \"-Wstringop-overflow\"")
#  define GFX_THUMB_STATUS_DIAG_POP \
   _Pragma("GCC diagnostic pop")
#else
#  define GFX_THUMB_STATUS_DIAG_PUSH
#  define GFX_THUMB_STATUS_DIAG_POP
#endif

GFX_THUMB_STATUS_DIAG_PUSH
static INLINE void gfx_thumb_status_store(
      retro_atomic_int_t *ptr, enum gfx_thumbnail_status val)
{
   retro_atomic_store_release_int(ptr, (int)val);
}

static INLINE enum gfx_thumbnail_status gfx_thumb_status_load(
      retro_atomic_int_t *ptr)
{
   return (enum gfx_thumbnail_status)retro_atomic_load_acquire_int(ptr);
}
GFX_THUMB_STATUS_DIAG_POP

#define GFX_THUMB_STATUS_STORE(ptr, val) gfx_thumb_status_store((ptr), (val))
#define GFX_THUMB_STATUS_LOAD(ptr)       gfx_thumb_status_load((ptr))

/* A still parked instead of uploaded (gfx_thumbnail_park_requests):
 * the decoded image, kept for as long as content is loaded and
 * uploaded from here, pixels borrowed, by every request for its path
 * meanwhile. dying: dropped while an upload still reads it, so the
 * completion frees it. */
typedef struct
{
   struct texture_image img;
   unsigned upscale_threshold;
   unsigned downscale_cap;
   char path[PATH_MAX_LENGTH];
   uint8_t inflight;             /* uploads reading the pixels */
   bool dying;
} gfx_thumbnail_parked_t;

/* Utility structure, sent as userdata when pushing
 * an image load */
typedef struct
{
   uint64_t list_id;
   gfx_thumbnail_t *thumbnail;
   gfx_thumbnail_parked_t *parked; /* landing from this slot */
   unsigned upscale_threshold;
   char path[PATH_MAX_LENGTH];
   bool park;                    /* land parked, not uploaded */
} gfx_thumbnail_tag_t;

/* Stills asked for during a frame wait here for the frame's end, when
 * they go to the task queue as sets of as many as there are cores to
 * decode them at once (gfx_thumbnail_flush_requests) */
#define GFX_THUMB_SET_MAX     8
#define GFX_THUMB_PENDING_MAX 64
static gfx_thumbnail_tag_t *gfx_thumb_pending[GFX_THUMB_PENDING_MAX];
static unsigned             gfx_thumb_pending_n;

static unsigned gfx_thumbnail_downscale_cap(void);

static gfx_thumbnail_state_t gfx_thumb_st = {0}; /* uint64_t alignment */

/* Stills requested while parking is on land here decoded instead of
 * uploaded, the image moved in whole. Every later request for the same
 * path uploads it from here, with no read or decode, until content
 * is closed or the menu torn down. */
#define GFX_THUMB_PARKED_MAX 4

static gfx_thumbnail_parked_t gfx_thumb_parked[GFX_THUMB_PARKED_MAX];
static unsigned gfx_thumb_parked_n;     /* slots holding a live image */
static unsigned gfx_thumb_parked_next;
static bool     gfx_thumb_parking;

#define GFX_THUMB_PARKED_LIVE(p) ((p)->img.pixels && !(p)->dying)

static void gfx_thumbnail_parked_drop(gfx_thumbnail_parked_t *p)
{
   if (!GFX_THUMB_PARKED_LIVE(p))
      return;
   gfx_thumb_parked_n--;
   if (p->inflight)
   {
      p->dying = true;
      return;
   }
   image_texture_free(&p->img);
   p->img.pixels = NULL;
}

/* An upload is done reading a parked image */
static void gfx_thumbnail_parked_done(void *payload)
{
   gfx_thumbnail_parked_t *p = (gfx_thumbnail_parked_t*)payload;
   if (--p->inflight || !p->dying)
      return;
   image_texture_free(&p->img);
   p->img.pixels = NULL;
   p->dying      = false;
}

/* Takes @img, the struct included. A path already parked is replaced,
 * else a free slot is used, else the oldest. False, the image left
 * with the caller, when the oldest is still being read. */
static bool gfx_thumbnail_park_image(const gfx_thumbnail_tag_t *tag,
      struct texture_image *img)
{
   unsigned i;
   gfx_thumbnail_parked_t *p = NULL;
   for (i = 0; i < GFX_THUMB_PARKED_MAX; i++)
      if (     GFX_THUMB_PARKED_LIVE(&gfx_thumb_parked[i])
            && string_is_equal(gfx_thumb_parked[i].path, tag->path))
         p = &gfx_thumb_parked[i];
   for (i = 0; !p && i < GFX_THUMB_PARKED_MAX; i++)
      if (!gfx_thumb_parked[i].img.pixels)
         p = &gfx_thumb_parked[i];
   if (!p)
      p = &gfx_thumb_parked[gfx_thumb_parked_next++ % GFX_THUMB_PARKED_MAX];
   gfx_thumbnail_parked_drop(p);
   if (p->img.pixels)
      return false;
   p->img               = *img;
   p->upscale_threshold = tag->upscale_threshold;
   p->downscale_cap     = gfx_thumbnail_downscale_cap();
   strlcpy(p->path, tag->path, sizeof(p->path));
   gfx_thumb_parked_n++;
   free(img);
   return true;
}

void gfx_thumbnail_park_requests(bool park)
{
   gfx_thumb_parking = park;
}

void gfx_thumbnail_parked_free(void)
{
   unsigned i;
   for (i = 0; gfx_thumb_parked_n && i < GFX_THUMB_PARKED_MAX; i++)
      gfx_thumbnail_parked_drop(&gfx_thumb_parked[i]);
}

gfx_thumbnail_state_t *gfx_thumb_get_ptr(void)
{
   return &gfx_thumb_st;
}

/* Setters */

/* When streaming thumbnails, sets time in ms that an
 * entry must be on screen before an image load is
 * requested
 * > if 'delay' is negative, default value is set */
void gfx_thumbnail_set_stream_delay(float delay)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->stream_delay = (delay >= 0.0f) ?
         delay : DEFAULT_GFX_THUMBNAIL_STREAM_DELAY;
}

/* Sets duration in ms of the thumbnail 'fade in'
 * animation
 * > If 'duration' is negative, default value is set */
void gfx_thumbnail_set_fade_duration(float duration)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->fade_duration = (duration >= 0.0f) ?
         duration : DEFAULT_GFX_THUMBNAIL_FADE_DURATION;
}

/* Specifies whether 'fade in' animation should be
 * triggered for missing thumbnails
 * > When 'true', allows menu driver to animate
 *   any 'thumbnail unavailable' notifications */
void gfx_thumbnail_set_fade_missing(bool fade_missing)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->fade_missing = fade_missing;
}

/* Callbacks */

/* Fade animation callback - simply resets thumbnail
 * 'fade_active' status */
static void gfx_thumbnail_fade_cb(void *userdata)
{
   gfx_thumbnail_t *thumbnail = (gfx_thumbnail_t*)userdata;
   if (thumbnail)
      thumbnail->flags |= GFX_THUMB_FLAG_FADE_ACTIVE;
}

/* Initialises thumbnail 'fade in' animation */
/* The channel order a decode should produce: what the driver that is
 * up wants, asked through the surface layer rather than read from the
 * display flag, so every producer has one place to ask. Main thread,
 * beside the decode it is asked for. */
static bool gfx_thumbnail_use_rgba(void)
{
   gfx_surface_requirements_t req;
   if (!gfx_surface_query_requirements(0, &req))
      return false;
   return req.rgba;
}

/* A thumbnail's still is uploaded through its surface, which takes
 * half floats where the driver offers them: an HDR video's still may
 * come as linear scRGB (TASK_IMAGE_LOAD_HDR). */
#define GFX_THUMBNAIL_LOAD_FLAGS ((gfx_thumbnail_use_rgba() \
      ? TASK_IMAGE_LOAD_RGBA : 0) | TASK_IMAGE_LOAD_HDR)

static void gfx_thumbnail_init_fade(
      gfx_thumbnail_state_t *p_gfx_thumb,
      gfx_thumbnail_t *thumbnail)
{
   /* Sanity check */
   if (!thumbnail)
      return;

   /* A 'fade in' animation is triggered if:
    * - Thumbnail is available
    * - Thumbnail is missing and 'fade_missing' is enabled */
   if (   (GFX_THUMB_STATUS_LOAD(&thumbnail->status) == GFX_THUMBNAIL_STATUS_AVAILABLE)
       || (p_gfx_thumb->fade_missing
       && (GFX_THUMB_STATUS_LOAD(&thumbnail->status) == GFX_THUMBNAIL_STATUS_MISSING)))
   {
      if (p_gfx_thumb->fade_duration > 0.0f && !runloop_is_content_switching())
      {
         gfx_animation_ctx_entry_t animation_entry;

         thumbnail->alpha                 = 0.0f;
         thumbnail->flags                |= GFX_THUMB_FLAG_FADE_ACTIVE;

         animation_entry.easing_enum      = EASING_OUT_QUAD;
         animation_entry.tag              = (uintptr_t)&thumbnail->alpha;
         animation_entry.duration         = p_gfx_thumb->fade_duration;
         animation_entry.target_value     = 1.0f;
         animation_entry.subject          = &thumbnail->alpha;
         animation_entry.cb               = gfx_thumbnail_fade_cb;
         animation_entry.userdata         = thumbnail;

         gfx_animation_push(&animation_entry);
      }
      else
         thumbnail->alpha = 1.0f;
   }
}

/* Used to process thumbnail data following completion
 * of image load task */
/* ---- Animated thumbnails ----
 * Control flow runs on the main thread only: the animation is opened
 * from the (main-thread) upload callback, advanced from the
 * (main-thread) stream request/process functions, and torn down from
 * gfx_thumbnail_reset. The video thread never touches these fields;
 * it only snapshots 'texture', which is replaced with the usual
 * load-new / unload-old sequence whose GPU side is serialised onto
 * the video thread by the texture command queue.
 *
 * The playback - the clock, loops, catch-up and the decode, on a
 * shared worker where the build has one - is a gfx_surface_player's,
 * which decodes into the thumbnail's surface. Closing the animation
 * frees the player first, which takes its decodes back from the
 * worker, so teardown never frees a stream under it. */

/* Refuse to animate anything larger than this many canvas pixels or
 * a file larger than this (the file buffer is held for the lifetime
 * of the animation). Decode COST does not need a tight cap here: the
 * player's per-vsync budget already makes large animations lower
 * their own frame rate instead of stalling the menu, so the pixel cap
 * only bounds MEMORY (two full canvases are kept while animating).
 * Admit anything up to a full 4K canvas - which also covers tall
 * portrait sources like 1920x2880 - and leave everything beyond that
 * as a static image. */
#define GFX_THUMB_ANIM_MAX_PIXELS   (3840 * 2160)
/* The file cap was originally sized for animated WebP, where 64 MiB is
 * already enormous; WebM videos (which also gate the file-browser
 * preview path through this constant) routinely exceed that, so admit
 * up to 256 MiB. The cost is transient: the buffer is only held while
 * the entry's animation is actually on screen, and only for files the
 * user deliberately highlighted. Anything larger is left with no
 * preview rather than risking an allocation spike on a multi-GiB
 * movie. */
#define GFX_THUMB_ANIM_MAX_FILE     (256 * 1024 * 1024)
/* When the platform can report free memory the caps above become the
 * fallback and admission scales with the heap instead: an animation may
 * pin at most a quarter of the reported free memory, bounded by an
 * absolute ceiling so a workstation with tens of gigabytes free does
 * not synchronously slurp a multi-gigabyte movie for a hover preview. */
#define GFX_THUMB_ANIM_ABS_MAX_FILE (1024 * 1024 * 1024)
/* Sliding-window sizing for the file-browser animation path.  KEEP is
 * the permanently resident head (container header plus index: an MP4
 * moov or a WebM cues block sits here and is revisited on every loop).
 * LOOKAHEAD is committed ahead of the decoder's byte frontier and
 * MARGIN behind it - the margin must exceed the largest single frame's
 * packet so a decommit can never overtake a read the decoder is still
 * inside.  Measured single-frame frontier steps on 4K H.264 stay in
 * the low tens of KiB, so 8 MiB of margin is three orders of headroom.
 */
#define GFX_THUMB_ANIM_WINDOW_KEEP  (4 * 1024 * 1024)
#define GFX_THUMB_ANIM_WINDOW_AHEAD (8 * 1024 * 1024)
#define GFX_THUMB_ANIM_WINDOW_BACK  (8 * 1024 * 1024)
/* Preview audio's own window over the same file.  The head must hold
 * the container metadata for the stream's whole life (the AAC arm
 * borrows the AudioSpecificConfig out of the moov, and a loop rewinds
 * through it), so it is sized from the media floor plus slack rather
 * than fixed.  LOOKAHEAD is what the feeder keeps committed ahead of
 * the decoder's compressed frontier: at the 255 kbps of a typical
 * stereo AAC track that is ~66 seconds of audio, against a feeder
 * that ticks once per menu frame, so the decoder reaching the bound
 * is a theoretical case rather than a paced one.  MARGIN trails it. */
/* Ceiling on the bytes either feeder reads in ONE animate() tick.
 * Both feeders run on the main thread, and window_extend performs its
 * read synchronously, so before this cap the first tick after an open
 * - and the first tick after every loop of a looping video - read the
 * whole lookahead in one filestream_read: 8 MiB on the video window,
 * a 400 ms frame stall on 20 MB/s storage, repeated at every lap.
 * The budget spreads the same catch-up over consecutive vsyncs; the
 * bytes that end up resident are identical, only never all at once.
 *
 * The values are rates, not tastes: at 60 ticks/s these sustain
 * 30 MB/s of video and 15 MB/s of audio, an order of magnitude above
 * any stream a thumbnail previews (a 100 Mbps master needs 12.5 MB/s;
 * preview audio tops out three decimal orders below its budget), so
 * the feeder gains on the decoder every tick it is behind and the
 * steady state is unchanged.  The catch-up case the budget must not
 * touch - a frontier behind the consumer, where pacing means faulting
 * - is exempted inside feed_budget itself, not here. */
#define GFX_THUMB_ANIM_FEED_BUDGET  (512 * 1024)
#define GFX_THUMB_AUDIO_FEED_BUDGET (256 * 1024)

#define GFX_THUMB_AUDIO_WINDOW_KEEP (2 * 1024 * 1024)
#define GFX_THUMB_AUDIO_HEAD_SLACK  (1 * 1024 * 1024)
#define GFX_THUMB_AUDIO_LOOKAHEAD   (2 * 1024 * 1024)
#define GFX_THUMB_AUDIO_MARGIN      (1 * 1024 * 1024)
/* Ceiling on that head.  The floor is only a proxy for where the
 * metadata ends - it is the first sample's offset, which in a normal
 * file sits just past the moov but in one whose media starts far in
 * is nowhere near it.  Sizing the permanently resident head from an
 * unclamped floor would commit gigabytes for a preview.  Past this,
 * open with the clamp and let the decoder's bounded open decline:
 * a container whose metadata will not fit simply gets no audio. */
#define GFX_THUMB_AUDIO_HEAD_MAX    (32 * 1024 * 1024)
/* Preview audio: decode the animated thumbnail's audio track and loop
 * it through the audio mixer while the animation is shown. */
#if (defined(HAVE_RWEBM) || defined(HAVE_RMP4)) && \
      defined(HAVE_AUDIOMIXER) && \
      (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS) || defined(HAVE_RAAC))
#define GFX_THUMB_PREVIEW_AUDIO 1
/* The mixer streams the audio track and decodes on the flush, so no
 * PCM is buffered up front and the clip is not length-capped. */
#define GFX_THUMB_PREVIEW_AUDIO_NAME   "__gfx_thumb_preview"
#endif




/* The threads the preview may use now: the setting, unless a core is
 * running under the menu - content loaded and not paused - in which
 * case one, so that a 4K preview does not take the cores the game is
 * on. */
static unsigned gfx_thumbnail_anim_threads_wanted(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   if (     (runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)
         && !(runloop_st->flags & RUNLOOP_FLAG_PAUSED))
      return 1;
   return config_get_ptr()->uints.menu_thumbnail_preview_threads;
}

typedef struct gfx_thumbnail_anim_lifetime
{
   struct gfx_thumbnail_anim_lifetime *next;
   gfx_surface_player_t *player;
   void *surface;
   gfx_anim_preview_t *session;
   void *stream;
   void *buf;
   struct data_transfer *dt;
   enum image_type_enum type;
} gfx_thumbnail_anim_lifetime_t;

static gfx_thumbnail_anim_lifetime_t *gfx_thumb_retired;

static void gfx_thumbnail_anim_dispose(gfx_thumbnail_anim_lifetime_t *life)
{
   gfx_surface_player_free(life->player);
   gfx_surface_free((gfx_surface_t*)life->surface);
   gfx_anim_preview_release(life->session);
   image_transfer_anim_stream_free(life->stream, life->type);
   if (life->dt)
      data_transfer_free(life->dt);
   else
      free(life->buf);
   free(life);
}

static void gfx_thumbnail_anim_reap(void)
{
   gfx_thumbnail_anim_lifetime_t **link = &gfx_thumb_retired;
   while (*link)
   {
      gfx_thumbnail_anim_lifetime_t *life = *link;
      if (video_player_stop(life->player->vp))
      {
         *link = life->next;
         gfx_thumbnail_anim_dispose(life);
      }
      else
         link = &life->next;
   }
}

void gfx_thumbnail_anim_worker_deinit(void)
{
   video_player_worker_deinit();
   gfx_thumbnail_anim_reap();
}

static void gfx_thumbnail_anim_retired_release(void *user,
      gfx_surface_t *surface, unsigned slot)
{
   (void)user;
   (void)slot;
   if (surface->imported && surface->dropped && surface->import.abandon)
      surface->import.abandon(surface->import.owner, surface->import.done,
            surface->import.done_value);
}

static void gfx_thumbnail_anim_close(gfx_thumbnail_t *thumbnail)
{
   gfx_thumbnail_anim_lifetime_t *life =
      (gfx_thumbnail_anim_lifetime_t*)thumbnail->anim_lifetime;
   if (life)
   {
      life->player  = (gfx_surface_player_t*)thumbnail->anim_player;
      life->session = (gfx_anim_preview_t*)thumbnail->anim_sess;
      life->stream  = thumbnail->anim;
      life->buf     = thumbnail->anim_buf;
      life->dt      = thumbnail->anim_dt;
      life->type    = (enum image_type_enum)thumbnail->anim_type;
      gfx_anim_preview_audio_stop(life->session);
      if (!life->player || video_player_stop(life->player->vp))
         gfx_thumbnail_anim_dispose(life);
      else
      {
         gfx_surface_t *surface = (gfx_surface_t*)thumbnail->anim_surface;
         /* The decoder still borrows slots and its window feeder. */
         life->surface = surface;
         life->player->surface = &life->surface;
         if (surface)
         {
            surface->release = gfx_thumbnail_anim_retired_release;
            surface->user = NULL;
         }
         thumbnail->anim_surface = NULL;
         thumbnail->texture = 0;
         life->next = gfx_thumb_retired;
         gfx_thumb_retired = life;
      }
   }
   thumbnail->anim_player       = NULL;
   thumbnail->anim_lifetime     = NULL;
   thumbnail->anim_sess         = NULL;
   thumbnail->anim              = NULL;
   thumbnail->anim_buf          = NULL;
   thumbnail->anim_dt           = NULL;
   thumbnail->anim_buf_len      = 0;
   thumbnail->anim_type         = 0;
   thumbnail->anim_read_pending = 0;
   thumbnail->anim_windowed     = 0;
   thumbnail->flags            &= ~GFX_THUMB_FLAG_ANIM_ACTIVE;
}

static gfx_surface_t *gfx_thumbnail_anim_surface(void *user,
      unsigned dims, unsigned num_slots, uint32_t pixfmt);
static void gfx_thumbnail_anim_shown(void *user, gfx_surface_t *s);

/* The playback's window feed, on the thread that decodes. */
static bool gfx_thumbnail_anim_feed(void *user)
{
   return gfx_anim_preview_feed((gfx_anim_preview_t*)user);
}

/* Install an open animation stream on the thumbnail, applying the
 * frame-count and memory admission checks.  'buf' is the container
 * bytes the stream borrows: when 'xfer' is non-NULL it owns 'buf'
 * (released with data_transfer_free), otherwise 'buf' is a malloc'd
 * block this thumbnail takes over (released with free).  Ownership of
 * stream/buf/xfer transfers in every outcome; on rejection they
 * are released and the static thumbnail stays.  Returns true when the
 * animation was installed. */
static bool gfx_thumbnail_anim_install(gfx_thumbnail_t *thumbnail,
      void *stream, enum image_type_enum type,
      void *buf, size_t len, struct data_transfer *xfer, int windowed,
      const char *path)
{
   unsigned anim_w          = 0;
   unsigned anim_h          = 0;
   int num_frames           = 0;
   int loop_count           = 0;

   /* Correctness currently relies on every caller resetting the
    * thumbnail before install; make the invariant local so a future
    * second call site cannot leak or double-borrow a live decoder. */
   gfx_thumbnail_anim_close(thumbnail);

   image_transfer_anim_stream_get_info(stream, type,
         &anim_w, &anim_h, &num_frames, &loop_count);

   /* Admission charges what the playback actually pins.  A windowed
    * transfer only ever commits its head plus the sliding window, so
    * the file length is not the cost - substituting it here would
    * throw away the whole point of windowing and refuse a long video
    * that comfortably fits.  An unwindowed buffer really is resident
    * in full, so it is still charged at its length.  The pixel term is
    * unchanged either way: the decoder's reference frames and the
    * upload buffers scale with resolution, not with file size, and on
    * a large frame they dominate the footprint. */
   {
      uint64_t charge = (uint64_t)len;
      if (xfer && data_transfer_window_is_reserved(xfer))
         charge = (uint64_t)GFX_ANIM_PREVIEW_WINDOW_KEEP
                + GFX_ANIM_PREVIEW_WINDOW_AHEAD
                + GFX_ANIM_PREVIEW_WINDOW_BACK;
      if (   (num_frames < 2)
          || (anim_w < 1)
          || (anim_h < 1)
          || !gfx_anim_preview_admit(charge, (uint64_t)anim_w * anim_h))
         goto fail;
   }

   if (!(thumbnail->anim_lifetime = calloc(1,
               sizeof(gfx_thumbnail_anim_lifetime_t))))
      goto fail;

   /* The task hands the buffer over as the data_transfer that owns
    * it - possibly with its fill still in flight. */
   thumbnail->anim_dt = xfer;
   if (xfer && data_transfer_failed(xfer))
   {
      /* the read already ended short of the file: its unwritten tail
       * must never feed the decoders - keep the still, drop the
       * animation */
      thumbnail->anim_dt = NULL;
      free(thumbnail->anim_lifetime);
      thumbnail->anim_lifetime = NULL;
      goto fail;
   }

   thumbnail->anim            = stream;
   /* Owned here rather than patched in by the caller afterwards: the
    * pending decision below depends on it, and anim_close (first
    * thing this function does) had just zeroed it - so a caller-side
    * assignment always arrived one decision too late. */
   thumbnail->anim_windowed   = windowed ? 1 : 0;
   thumbnail->anim_buf        = buf;
   thumbnail->anim_buf_len    = len;
   thumbnail->anim_type       = (uint8_t)type;
   thumbnail->flags          |= GFX_THUMB_FLAG_ANIM_ACTIVE;

   /* The still's task can complete - and adoption run - while the
    * file's read is still in flight (the still needs only a prefix).
    * Until the read completes, hold the animation and the audio
    * preview at the static frame: the animation's demuxer captured a
    * byte wall at the walled open and must not treat it as EOF, and
    * the audio begin below hands the decoder the whole buffer.  (The
    * audio decoder can work from a prefix - see anim_audio_begin - but
    * feeding it progressively is a cross-thread change not made here.)
    * gfx_thumbnail_animate pumps the handle to completion; a fatter
    * chunk shortens the catch-up.
    *
    * ONLY for adopted prefix handles.  A windowed handle never
    * completes - done stays clear for its whole life, that is what a
    * window is - and the pump animate uses is iterate_while, which
    * declines windows by design.  Marking a window pending therefore
    * deadlocked the animation permanently: pending forced animate to
    * pump, the pump was a no-op, complete() stayed false, and animate
    * returned before ever advancing a frame - the stream sat
    * installed behind a static thumbnail forever.  That was every
    * path-based animation open (animated WEBP, APNG, and any WEBM/MP4
    * with no still stream to adopt), which is the whole file-browser
    * path.  A window needs no pending phase at all: its feeder in
    * animate keeps the committed range straddling the decoder, and
    * the head it decodes from is resident before install runs. */
   thumbnail->anim_read_pending =
         (thumbnail->anim_dt
          && !windowed
          && !data_transfer_complete(thumbnail->anim_dt)) ? 1 : 0;

   /* The shared session over the same stream / mapping: it feeds the
    * window and plays the preview audio, as the companions' panes do. */
   thumbnail->anim_sess = gfx_anim_preview_wrap(stream, type,
         thumbnail->anim_dt, (const uint8_t*)buf, len,
         windowed ? true : false, path);

   {
      gfx_surface_player_cbs_t cb;
      cb.user    = thumbnail;
      cb.surface = gfx_thumbnail_anim_surface;
      cb.shown   = gfx_thumbnail_anim_shown;
      cb.feed    = thumbnail->anim_sess ? gfx_thumbnail_anim_feed : NULL;
      cb.feed_user = thumbnail->anim_sess;
      /* frames decoded on the GPU where the stream and driver can */
      if (!(thumbnail->anim_player = gfx_surface_player_new(stream, type,
            loop_count, &thumbnail->anim_surface, &cb,
            GFX_SURFACE_PLAYER_GPU)))
      {
         gfx_thumbnail_anim_close(thumbnail);
         return false;
      }
   }
   if (thumbnail->anim_sess && !thumbnail->anim_read_pending)
      gfx_anim_preview_audio_begin((gfx_anim_preview_t*)thumbnail->anim_sess);
   return true;

fail:
   image_transfer_anim_stream_free(stream, type);
   if (xfer)
      /* cancels a fill still in flight before releasing the buffer */
      data_transfer_free(xfer);
   else
      free(buf);
   return false;
}

/* png_probe carries what the still-load task already learned about a
 * PNG from the buffer it read anyway: < 0 unknown (probe the file), 0
 * conclusively still (return without opening anything), 1 APNG (skip
 * the probe). Only consulted for IMAGE_TYPE_PNG. */
static void gfx_thumbnail_anim_open_probed(gfx_thumbnail_t *thumbnail,
      const char *path, int png_probe)
{
   gfx_anim_preview_t *p;

   gfx_thumbnail_anim_close(thumbnail);

   if (string_is_empty(path))
      return;

   /* The sliding-window progressive open, the APNG probe and the memory
    * admission live in gfx_anim_preview (shared with the desktop
    * companions' preview panes). NULL: a still, or not admitted - keep
    * the static thumbnail. */
   if (!(p = gfx_anim_preview_open_hw(path, png_probe)))
      return;

   /* Hand the stream and the mapping to the thumbnail (install borrows
    * them exactly as before) and rebuild the session there over the
    * same objects. */
   {
      void *stream              = p->stream;
      struct data_transfer *dt  = p->dt;
      const uint8_t *base       = p->base;
      size_t blen               = p->len;
      enum image_type_enum type = p->type;
      /* Carry the session's real windowed flag into install, not a
       * hardcoded 1: gfx_anim_preview_open() sets it to whether the
       * mapping is a reservation (data_transfer_window_is_reserved).
       * Where reservation is unavailable the open degrades to a
       * whole-file read and the flag is false - install must then
       * charge the whole file to memory admission and skip the sliding-
       * window feed, or a multi-GB video is admitted as if it cost one
       * small window and the feeder chases a frontier in an already
       * fully resident buffer. */
      int windowed              = gfx_anim_preview_windowed(p) ? 1 : 0;
      p->stream = NULL;
      p->dt     = NULL;
      gfx_anim_preview_release(p);
      gfx_thumbnail_anim_install(thumbnail, stream, type,
            (void*)base, blen, dt, windowed, path);
   }
}

/* The historical entry point, and the symbol the anim oracle in
 * samples/gfx/gfx_thumbnail_anim globalizes and drives directly: no
 * task in sight there, so no verdict to pass - probe the file, as
 * this path always has. */
static void gfx_thumbnail_anim_open(gfx_thumbnail_t *thumbnail,
      const char *path)
{
   gfx_thumbnail_anim_open_probed(thumbnail, path, -1);
}

/* Video containers open over a sliding window before anything else:
 * the still task reads the whole file into memory, which a
 * multi-gigabyte recording fails outright (the allocation) or turns
 * into a minute of disk reads for one frame - while the windowed
 * animation path holds a fixed few-MiB footprint at any length, and
 * its first decoded frame doubles as the still.
 *
 * Returns true when the window took and the thumbnail is now PENDING
 * on it.  A false return (not a video, admission refused, no
 * reservation support, malformed container) leaves the thumbnail
 * untouched for the caller to fall through to the historical
 * whole-file still decode, which keeps small files on exotic
 * platforms working exactly as before. */
static bool gfx_thumbnail_try_video_open(gfx_thumbnail_t *thumbnail,
      const char *path)
{
   enum image_type_enum ptype;

   if (!thumbnail || string_is_empty(path))
      return false;

   ptype = image_texture_get_type(path);
   if (   (ptype != IMAGE_TYPE_WEBM)
       && (ptype != IMAGE_TYPE_MP4)
       && (ptype != IMAGE_TYPE_WEBP))
      return false;
   /* An animated WEBP takes the same anim-first route as a video: its
    * still decode read and parsed the whole file for the first frame,
    * which the animation then decoded again from its window.  A still
    * WEBP is told apart by its first chunk (32 bytes) and keeps the
    * still task.  PNG stays on the still task: a playlist's stills are
    * PNGs, and that task supplies the APNG verdict from the bytes it
    * reads anyway, where a probe here would be an extra open per
    * thumbnail. */
   if (ptype == IMAGE_TYPE_WEBP && gfx_anim_preview_probe(path) != 1)
      return false;

   gfx_thumbnail_anim_open(thumbnail, path);
   if (!thumbnail->anim)
      return false;

   thumbnail->list_id = gfx_thumb_st.list_id;
   GFX_THUMB_STATUS_STORE(&thumbnail->status,
         GFX_THUMBNAIL_STATUS_PENDING);
   return true;
}

/* --- Uploads -------------------------------------------------------
 * A still and an animation frame both reach the GPU through the one
 * surface the thumbnail owns. Under threaded video a submit hands the
 * pixels to the video thread and the handle arrives with the release
 * on a later frame - a round trip the main thread does not wait out,
 * which matters because thumbnails are the most frequent upload the
 * menu makes, one per row while scrolling. Without the wrapper the
 * same submit uploads on the spot. */


/* --- Animation frames: the streaming surface -----------------------
 * Every frame of a playing preview goes to one persistent GPU texture
 * owned by a gfx_surface, whose slots the decoder writes into. The
 * surface updates the texture in place where the driver can, and
 * routes the update through the video thread without copying under
 * threaded video; the thumbnail only learns, on the main thread, that
 * a frame is showing and that a slot is free again. */

/* The surface's texture is what the thumbnail draws: adopt it. On
 * the first frame of a preview opened without a still decode this is
 * also what makes the thumbnail drawable and starts its fade. */
static void gfx_thumbnail_anim_shown(void *user, gfx_surface_t *s)
{
   gfx_thumbnail_t *thumbnail         = (gfx_thumbnail_t*)user;
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   if (!s->handle)
      return;
   thumbnail->texture = s->handle;
   thumbnail->dims    = s->dims;
   /* Release-store pairs with the acquire-load in the draw path:
    * texture and dims are visible before AVAILABLE is. */
   if (GFX_THUMB_STATUS_LOAD(&thumbnail->status) ==
         GFX_THUMBNAIL_STATUS_PENDING)
   {
      GFX_THUMB_STATUS_STORE(&thumbnail->status,
            GFX_THUMBNAIL_STATUS_AVAILABLE);
      gfx_thumbnail_init_fade(p_gfx_thumb, thumbnail);
   }
}

/* A still's upload has landed on the video thread: the image the
 * load task decoded went to a still the thumbnail owns, which frees
 * it once taken - also when the thumbnail reset or replaced the still
 * meanwhile, which is when this never runs. */
static void gfx_thumbnail_still_release(void *user, gfx_surface_t *s,
      unsigned slot)
{
   gfx_thumbnail_t *thumbnail         = (gfx_thumbnail_t*)user;
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   (void)slot;
   if (!s->handle)
   {
      if (GFX_THUMB_STATUS_LOAD(&thumbnail->status)
            == GFX_THUMBNAIL_STATUS_PENDING)
      {
         GFX_THUMB_STATUS_STORE(&thumbnail->status,
               GFX_THUMBNAIL_STATUS_MISSING);
         gfx_thumbnail_init_fade(p_gfx_thumb, thumbnail);
      }
      return;
   }
   gfx_thumbnail_anim_shown(thumbnail, s);
}

/* Main thread, from a later frame's poll: the video thread has taken
 * the frame in @slot. The animation may have closed meanwhile, in
 * which case the surface simply keeps its last frame. */
static void gfx_thumbnail_anim_slot_release(void *user, gfx_surface_t *s,
      unsigned slot)
{
   gfx_thumbnail_t *thumbnail = (gfx_thumbnail_t*)user;
   if (gfx_surface_player_release(
         (gfx_surface_player_t*)thumbnail->anim_player, s, slot))
      return;
   gfx_thumbnail_anim_shown(thumbnail, s);
}

/* The thumbnail's surface for a @dims animation with @num_slots
 * producer slots in @pixfmt, or with none the still its images go to
 * - or, in NV12, the imported surface GPU frames go to - made on first
 * use. A surface of another shape - a different animation on a
 * thumbnail that was not reset - is replaced; its texture goes with
 * it, so it is only asked for when a frame is about to be shown. */
static gfx_surface_t *gfx_thumbnail_anim_surface(void *user,
      unsigned dims, unsigned num_slots, uint32_t pixfmt)
{
   gfx_thumbnail_t *thumbnail = (gfx_thumbnail_t*)user;
   gfx_surface_t *s           = (gfx_surface_t*)thumbnail->anim_surface;
   bool import                = !num_slots && pixfmt == IMAGE_PIXFMT_NV12;
   if (     s
         && (     (num_slots && s->dims != dims)
               || s->num_slots < num_slots
               || (!num_slots && s->num_slots)
               || (num_slots && s->pixfmt != pixfmt)
               || (s->imported ? 1 : 0) != (import ? 1 : 0)
               || (import && s->dims != dims)))
   {
      thumbnail->texture = 0;
      gfx_surface_free(s);
      s = NULL;
      thumbnail->anim_surface = NULL;
   }
   if (!s)
   {
      /* Animated thumbnails update every frame; always plain linear
       * filtering here, so no per-frame mip-map generation regardless
       * of the menu_texture_mipmapping setting. */
      if (import)
         s = gfx_surface_new_import(dims, TEXTURE_FILTER_LINEAR,
               gfx_thumbnail_anim_slot_release, thumbnail);
      else if (num_slots)
         s = gfx_surface_new(dims, num_slots, pixfmt,
               TEXTURE_FILTER_LINEAR,
               gfx_thumbnail_anim_slot_release, thumbnail);
      else if ((s = gfx_surface_new_still(gfx_display_texture_filter())))
      {
         s->release = gfx_thumbnail_still_release;
         s->user    = thumbnail;
      }
      thumbnail->anim_surface = s;
   }
   return s;
}

/* 2 ms of this vsync and no more: the frame guard for the adopted
 * read inside gfx_thumbnail_animate. */
static bool gfx_thumb_frame_budget(void *ud, size_t avail, size_t len)
{
   (void)avail;
   (void)len;
   return cpu_features_get_time_usec() - *(int64_t*)ud < 2000;
}

/* Advances an animated thumbnail: at most one frame shown a call, once
 * its duration has elapsed. The decode runs on video_player's worker
 * where there is one, a frame ahead of the clock, and this only shows
 * frames that are ready; without one it decodes here, under a budget
 * shared by every animation in the tick.
 *
 * MUST be called on the main thread, once per frame, for every visible
 * thumbnail (menu drivers do this from their main-thread iterate step).
 * For a still image the ANIM_ACTIVE flag is clear, so this returns
 * after a single flag test. */
void gfx_thumbnail_animate(gfx_thumbnail_t *thumbnail,
      retro_time_t current_time)
{
   int64_t now = (int64_t)current_time;
   enum image_type_enum type;

   gfx_thumbnail_anim_reap();
   if (   !thumbnail
       || !(thumbnail->flags & GFX_THUMB_FLAG_ANIM_ACTIVE)
       || !thumbnail->anim)
      return;
   {
      /* AVAILABLE is the normal running state.  PENDING with a stream
       * installed is the anim-first bootstrap for video files opened
       * over a window without a preceding still decode: the first
       * frame shown flips the status in anim_shown. */
      enum gfx_thumbnail_status st = (enum gfx_thumbnail_status)
            GFX_THUMB_STATUS_LOAD(&thumbnail->status);
      if (   st != GFX_THUMBNAIL_STATUS_AVAILABLE
          && st != GFX_THUMBNAIL_STATUS_PENDING)
         return;
   }

   /* No clock is read here.  The runloop samples the monotonic
    * counter once per iteration and hands it to the frame's consumers
    * (gfx_animation_update takes it the same way), so every thumbnail
    * advanced in a frame paces off one coherent "now" - and a harness
    * can drive this with synthetic time. */
   type = (enum image_type_enum)thumbnail->anim_type;

   /* Windowed playback: the shared session feeds the preview audio's
    * window from here (its own transfer, consumed by the mixer under
    * the mixer's lock).  The VIDEO window is fed by whichever thread
    * decodes, because the feed and the decoder share the demuxer's
    * cursor and bound. */
   if (thumbnail->anim_sess)
      gfx_anim_preview_audio_feed((gfx_anim_preview_t*)thumbnail->anim_sess);

   if (thumbnail->anim_read_pending)
   {
      /* The adopted file is still being read; finish it here, a small
       * time budget per vsync, holding the static frame meanwhile.
       * Playback and the audio decode only start on a complete
       * buffer, so the decoders never see the partial-read wall.  The
       * 2 ms cap is handed to the fill, so it lands between the fill's
       * own reads rather than after a whole chunk. */
      data_transfer_iterate_while(thumbnail->anim_dt, 0,
            gfx_thumb_frame_budget, &now);
      if (data_transfer_failed(thumbnail->anim_dt))
      {
         /* A read that ended short of the file (I/O error, the file
          * shrank) must not feed the decoders its unwritten tail:
          * keep the still, drop the animation. */
         gfx_thumbnail_anim_close(thumbnail);
         return;
      }
      if (!data_transfer_complete(thumbnail->anim_dt))
         return;
      thumbnail->anim_read_pending = 0;
      /* The adopted stream's demuxer captured its byte wall when the
       * still opened it; lift it to the full length, or the animation
       * would treat the wall as the end of the file and loop there. */
      image_transfer_anim_stream_set_avail(thumbnail->anim, type,
            thumbnail->anim_buf_len);
      /* ...and finish the WEBM timestamp pre-scan the walled open
       * truncated, so pacing matches a fully-read open exactly. */
      image_transfer_anim_stream_complete_scan(thumbnail->anim, type,
            thumbnail->anim_buf, thumbnail->anim_buf_len);
      if (thumbnail->anim_sess)
         gfx_anim_preview_audio_begin((gfx_anim_preview_t*)thumbnail->anim_sess);
   }

   video_player_set_threads(gfx_thumbnail_anim_threads_wanted());
   /* Loops done or the stream failed: the last frame's texture stays,
    * the decoder and the file buffer go. */
   if (video_player_poll(
            ((gfx_surface_player_t*)thumbnail->anim_player)->vp, now)
         == VIDEO_PLAYER_ENDED)
      gfx_thumbnail_anim_close(thumbnail);
}

/* A still's decode has come back, the image (NULL: nothing) the
 * thumbnail's to upload or free, the tag to free. @task is the image
 * task it came from, NULL for a set's, which holds no stream to adopt;
 * @png_probe is its verdict on an animated PNG (-1: unknown). */
static void gfx_thumbnail_still_landed(retro_task_t *task,
      struct texture_image *img, gfx_thumbnail_tag_t *thumbnail_tag,
      int png_probe)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   struct texture_image *src          = NULL;
   bool fade_enabled                  = false;

   /* Sanity check */
   if (!thumbnail_tag)
      goto end;

   /* Ensure that we are operating on the correct
    * thumbnail... */
   if (thumbnail_tag->list_id != p_gfx_thumb->list_id)
      goto end;

   /* Only process image if we are waiting for it */
   if (GFX_THUMB_STATUS_LOAD(&thumbnail_tag->thumbnail->status) != GFX_THUMBNAIL_STATUS_PENDING)
      goto end;

   /* Sanity check: if thumbnail already has a texture,
    * we're in some kind of weird error state - in this
    * case, the best course of action is to just reset
    * the thumbnail... */
   if (thumbnail_tag->thumbnail->texture)
      gfx_thumbnail_reset(thumbnail_tag->thumbnail);

   /* The image: the task's, or the parked one the request found */
   if (thumbnail_tag->parked)
      src = &thumbnail_tag->parked->img;
   else
      src = img;

   /* Check we have a valid image */
   if (!src || (src->width < 1) || (src->height < 1))
   {
      GFX_THUMB_STATUS_STORE(&thumbnail_tag->thumbnail->status,
            GFX_THUMBNAIL_STATUS_MISSING);
      fade_enabled = true;
      goto end;
   }

   /* Parked: kept decoded for the requests that show it, which
    * upload from it. The thumbnail asks again from UNKNOWN. */
   if (     thumbnail_tag->park && img && img->pixels && !img->compressed
         && gfx_thumbnail_park_image(thumbnail_tag, img))
   {
      img = NULL;
      GFX_THUMB_STATUS_STORE(&thumbnail_tag->thumbnail->status,
            GFX_THUMBNAIL_STATUS_UNKNOWN);
      goto end;
   }

   /* The still goes to a surface the thumbnail owns, which takes the
    * image: under threaded video the status stays PENDING until the
    * release installs the texture and starts the fade; without it the
    * submit uploads here and now. The animation below is opened either
    * way, and its first frame replaces the still if it arrives first.
    * A driver that stopped sampling what was decoded (a reinit since)
    * gets the image narrowed, or for half floats no still at all, and
    * the animation brings the picture. */
   {
      gfx_thumbnail_parked_t *p = thumbnail_tag->parked;
      gfx_surface_t *s = VIDEO_SCALE_FITS(src->width, src->height)
         ? gfx_thumbnail_anim_surface(thumbnail_tag->thumbnail, 0, 0, 0)
         : NULL;
      /* Dimensions now, so layout does not wait for the handle. */
      unsigned dims    = VIDEO_SCALE_PACK(src->width, src->height);
      bool up          = false;

      /* A parked image is lent to the upload and stays parked; one
       * the surface will not take is dropped, and the thumbnail has
       * nothing to show, as below. */
      if (s && p)
      {
         p->inflight++;
         if (!(up = gfx_surface_submit_image_borrowed(s, &p->img, p,
                     gfx_thumbnail_parked_done)))
         {
            p->inflight--;
            gfx_thumbnail_parked_drop(p);
         }
      }
      else if (s)
         up = gfx_surface_submit_image(s, img);

      if (up)
      {
         thumbnail_tag->thumbnail->dims = dims;
         if (!s->inflight)
         {
            fade_enabled = true;
            gfx_thumbnail_anim_shown(thumbnail_tag->thumbnail, s);
         }
         img = NULL;
         goto open_anim;
      }
      if (s)
         img = NULL;    /* refused: freed by the surface */
      /* No surface, or the driver refused the upload: the thumbnail
       * has nothing to show, and the fade below reports that. */
      GFX_THUMB_STATUS_STORE(&thumbnail_tag->thumbnail->status,
            GFX_THUMBNAIL_STATUS_MISSING);
      fade_enabled = true;
   }

open_anim:

   /* If the file is an animation, open a streaming decoder for it;
    * frames are advanced by gfx_thumbnail_animate() while the
    * entry is on-screen. On failure the static image just uploaded
    * remains as-is.
    *
    * For WEBM/MP4 the load task's still-frame decode already opened,
    * pre-scanned, and advanced a stream past the first displayed
    * frame; adopt it - together with the nbio handle whose buffer it
    * borrows - instead of re-reading the file from disk and repeating
    * the open on this (the main) thread.  The animation then resumes
    * at the second displayed frame, which the static texture just
    * uploaded precedes.  The path-based open remains for animated
    * WEBP and any load where no stream was held. */
   {
      void *vstream               = NULL;
      struct data_transfer *vxfer = NULL;
      void *vbuf                  = NULL;
      size_t vlen                 = 0;
      enum image_type_enum vtype  = IMAGE_TYPE_NONE;

      if (task_image_detach_video_stream(task, &vstream, &vtype,
            &vxfer, &vbuf, &vlen))
         gfx_thumbnail_anim_install(thumbnail_tag->thumbnail,
               vstream, vtype, vbuf, vlen, vxfer, 0,
               thumbnail_tag->path);
      else
         /* Everything without a stream to hand over opens by path.
          * For the video types adoption is the fast route because the
          * still decode had to build the demuxer stream anyway, so
          * handing it over saves a second open and a pre-scan.
          *
          * APNG deliberately stays on this branch: decoding a still
          * PNG needs no animation stream at all (rpng ignores
          * acTL/fcTL/fdAT as unknown ancillary chunks and decodes the
          * default image), so making PNG adoptable would mean opening
          * an APNG stream during every still decode purely so the rare
          * animated one could inherit it - a cost on the hot path for
          * a benefit that is one file open and a chunk walk.  The
          * open-by-path route is windowed, so an animated PNG already
          * streams over a sliding window rather than a whole-file
          * read; adoption would not change its footprint.
          *
          * The chunk walk itself, though, IS worth inheriting: the
          * task read the whole file, so ask it whether the PNG was
          * animated instead of re-opening the file here (on the main
          * thread) to read a 4 KiB prefix that answers the same
          * question.  Inconclusive (< 0: not a PNG task, adopted
          * transfer, decode path without the buffer) falls back to
          * the probe inside the open, exactly as before. */
         gfx_thumbnail_anim_open_probed(thumbnail_tag->thumbnail,
               thumbnail_tag->path, png_probe);
   }

end:
   /* Clean up */
   if (img)
   {
      image_texture_free(img);
      free(img);
   }

   if (thumbnail_tag)
   {
      /* Trigger 'fade in' animation, if required */
      if (fade_enabled)
         gfx_thumbnail_init_fade(p_gfx_thumb,
               thumbnail_tag->thumbnail);

      free(thumbnail_tag);
   }
}

static void gfx_thumbnail_handle_upload(
      retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   (void)err;
   gfx_thumbnail_still_landed(task, (struct texture_image*)task_data,
         (gfx_thumbnail_tag_t*)user_data, task_image_png_probe(task));
}

static void gfx_thumbnail_set_landed(struct texture_image *img,
      int png_probe, void *item_ud)
{
   gfx_thumbnail_still_landed(NULL, img,
         (gfx_thumbnail_tag_t*)item_ud, png_probe);
}

/* How many stills a set takes: the cores the decode can use, where
 * the task queue runs on a thread of its own. One is no set at all. */
static unsigned gfx_thumbnail_set_width(void)
{
   static unsigned cores;
   if (!task_queue_is_threaded())
      return 1;
   if (!cores)
   {
      cores = cpu_features_get_core_amount();
      if (cores > GFX_THUMB_SET_MAX)
         cores = GFX_THUMB_SET_MAX;
      if (!cores)
         cores = 1;
   }
   return cores;
}

/* A request for a parked still lands it from the slot, uploaded now
 * as a decode coming back would be, the image staying parked for the
 * next request. One parked under other decode settings, or for a
 * driver wanting the other channel order, is dropped and the still
 * decoded again. False when nothing matched. */
static bool gfx_thumbnail_unpark(gfx_thumbnail_tag_t *tag)
{
   unsigned i;
   if (!gfx_thumb_parked_n || tag->park)
      return false;
   for (i = 0; i < GFX_THUMB_PARKED_MAX; i++)
   {
      gfx_thumbnail_parked_t *p = &gfx_thumb_parked[i];
      if (!GFX_THUMB_PARKED_LIVE(p) || !string_is_equal(p->path, tag->path))
         continue;
      if (     p->img.supports_rgba    != gfx_thumbnail_use_rgba()
            || p->upscale_threshold    != tag->upscale_threshold
            || p->downscale_cap        != gfx_thumbnail_downscale_cap())
      {
         gfx_thumbnail_parked_drop(p);
         return false;
      }
      tag->parked             = p;
      tag->thumbnail->list_id = tag->list_id;
      GFX_THUMB_STATUS_STORE(&tag->thumbnail->status,
            GFX_THUMBNAIL_STATUS_PENDING);
      gfx_thumbnail_still_landed(NULL, NULL, tag, -1);
      return true;
   }
   return false;
}

/* The still at @tag's path, decoded on the task queue: held for the
 * frame's set where one can be made, else a task of its own. A video
 * always goes alone - only its task can hand the thumbnail the decoder
 * stream the still came from. False when nothing was queued. */
static bool gfx_thumbnail_push_still(gfx_thumbnail_tag_t *tag,
      unsigned upscale_threshold)
{
   enum image_type_enum type = image_texture_get_type(tag->path);
   tag->upscale_threshold    = upscale_threshold;
   if (gfx_thumbnail_unpark(tag))
      return false;
   if (     type != IMAGE_TYPE_WEBM && type != IMAGE_TYPE_MP4
         && gfx_thumbnail_set_width() > 1)
   {
      if (gfx_thumb_pending_n == GFX_THUMB_PENDING_MAX)
         gfx_thumbnail_flush_requests();
      gfx_thumb_pending[gfx_thumb_pending_n++] = tag;
      return true;
   }
   return task_push_image_load_ex(tag->path, GFX_THUMBNAIL_LOAD_FLAGS,
         upscale_threshold, gfx_thumbnail_downscale_cap(),
         gfx_thumbnail_handle_upload, tag);
}

void gfx_thumbnail_flush_requests(void)
{
   unsigned width, cap, i = 0, n = gfx_thumb_pending_n;
   gfx_thumbnail_anim_reap();
   /* Parked stills are for the content's close; once it is closed
    * whatever was not taken back goes. */
   if (     gfx_thumb_parked_n
         && !runloop_is_content_switching()
         && !runloop_content_loaded())
      gfx_thumbnail_parked_free();
   if (!n)
      return;
   gfx_thumb_pending_n = 0;
   width               = gfx_thumbnail_set_width();
   cap                 = gfx_thumbnail_downscale_cap();
   while (i < n)
   {
      const char *paths[GFX_THUMB_SET_MAX];
      void *uds[GFX_THUMB_SET_MAX];
      gfx_thumbnail_tag_t *first = gfx_thumb_pending[i];
      unsigned j, count          = n - i;
      bool queued;
      if (count > width)
         count = width;
      /* One threshold to a set */
      for (j = 1; j < count; j++)
         if (gfx_thumb_pending[i + j]->upscale_threshold
               != first->upscale_threshold)
            break;
      count = j;
      if (count == 1)
         queued = task_push_image_load_ex(first->path,
               GFX_THUMBNAIL_LOAD_FLAGS, first->upscale_threshold, cap,
               gfx_thumbnail_handle_upload, first);
      else
      {
         for (j = 0; j < count; j++)
         {
            paths[j] = gfx_thumb_pending[i + j]->path;
            uds[j]   = gfx_thumb_pending[i + j];
         }
         queued = task_push_image_load_set(paths, uds, count,
               GFX_THUMBNAIL_LOAD_FLAGS, first->upscale_threshold, cap,
               gfx_thumbnail_set_landed, first->list_id);
      }
      /* Nothing will come back for these: they land empty now */
      if (!queued)
         for (j = 0; j < count; j++)
            gfx_thumbnail_still_landed(NULL, NULL,
                  gfx_thumb_pending[i + j], -1);
      i += count;
   }
}

/* Core interface */

/* When called, prevents the handling of any pending
 * thumbnail load requests
 * >> **MUST** be called before deleting any gfx_thumbnail_t
 *    objects passed to gfx_thumbnail_request() or
 *    gfx_thumbnail_process_stream(), otherwise
 *    heap-use-after-free errors *will* occur */
/* Predicate for the sweep below.  Always returns false so that
 * task_queue_find visits every task rather than stopping at the first
 * match - the cancellation is the side effect.  Only sets a flag, so it
 * is safe to call while the queue holds its own lock: task_set_flags
 * takes the property lock and nothing else. */
static bool gfx_thumbnail_cancel_finder(retro_task_t *task, void *userdata)
{
   uint64_t             *current_id = (uint64_t*)userdata;
   gfx_thumbnail_tag_t  *tag;

   uint64_t              set_tag;

   /* A set of thumbnails carries the generation it was asked under */
   if (task_image_set_tag(task, &set_tag))
   {
      if (set_tag != *current_id)
         task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
      return false;
   }
   /* Thumbnail loads are the ones that come back through our own
    * upload handler; other image loads (menu icons, savestate shots
    * pushed elsewhere) are not ours to cancel. */
   if (!task || task->callback != gfx_thumbnail_handle_upload)
      return false;
   if (!(tag = (gfx_thumbnail_tag_t*)task->user_data))
      return false;
   /* Belongs to the selection we just moved to - leave it alone. */
   if (tag->list_id == *current_id)
      return false;

   task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
   return false;
}

/* When called, prevents the handling of any pending
 * thumbnail load requests */
void gfx_thumbnail_cancel_pending_requests(void)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   task_finder_data_t     find_data;

   p_gfx_thumb->list_id++;

   /* Stills not yet sent are simply not sent; their thumbnails are
    * PENDING under the old generation, which is what
    * gfx_thumbnail_reset_if_orphaned looks for. */
   while (gfx_thumb_pending_n)
      free(gfx_thumb_pending[--gfx_thumb_pending_n]);

   /* Bumping the generation only makes the *results* unwanted; the
    * decodes themselves kept running to completion and were then thrown
    * away.  That is affordable for menu icons and ruinous for a
    * directory of large images: scrolling through one issues a load per
    * entry, none of them stop, and they pile up holding both a file
    * buffer and a full-resolution surface each while sharing the CPU,
    * so the one thumbnail the user is actually looking at arrives tens
    * of seconds late or not at all.
    *
    * Cancel them here instead.  task_file_load_handler already checks
    * RETRO_TASK_FLG_CANCELLED every tick and finishes the task when it
    * is set, so the queue runs cleanup and releases both buffers on the
    * next pass; nothing else is needed to make this take effect. */
   find_data.func     = gfx_thumbnail_cancel_finder;
   find_data.userdata = &p_gfx_thumb->list_id;
   task_queue_find(&find_data);
}

/* Fetches the current thumbnail file path of the
 * specified thumbnail 'type'.
 * Returns true if path is valid. */
static bool gfx_thumbnail_get_path(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      const char **path)
{
   if (path_data && path)
   {
      switch (thumbnail_id)
      {
         case GFX_THUMBNAIL_RIGHT:
            if (*path_data->right_path)
            {
               *path          = path_data->right_path;
               return true;
            }
            break;
         case GFX_THUMBNAIL_LEFT:
            if (*path_data->left_path)
            {
               *path          = path_data->left_path;
               return true;
            }
            break;
         case GFX_THUMBNAIL_ICON:
            if (*path_data->icon_path)
            {
               *path          = path_data->icon_path;
               return true;
            }
            break;
         default:
            break;
      }
   }

   return false;
}


/* Requests loading of the specified thumbnail
 * - If operation fails, 'thumbnail->status' will be set to
 *   GFX_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to GFX_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...) */
/* The cap to hand task_push_image_load for a thumbnail: the display's
 * longest side.
 *
 * The sidebar thumbnail and the fullscreen view share one texture -
 * pressing the fullscreen key only raises a flag and fades alpha, it
 * never re-requests the image - so a single size has to serve both.
 * The display is that size: the fullscreen view can never be drawn
 * larger than the panel, so it is unaffected, and the sidebar
 * downsamples from it on the GPU for free.
 *
 * Falls back to 0 (no cap, previous behaviour) when the viewport is
 * not known yet, rather than guessing a size the display might
 * exceed and softening the fullscreen view. */
static unsigned gfx_thumbnail_downscale_cap(void)
{
   struct video_viewport vp;
   unsigned cap  = 0;
   unsigned dims = 0;
   char desc[64];

   desc[0] = '\0';

   /* Prefer the display's own size over the current window: a
    * thumbnail is cached for as long as the entry stays selected, so
    * a cap taken from a small window would leave the texture
    * undersized after the window grows - and nothing re-requests it,
    * since going fullscreen only raises a flag.  Sizing to the
    * display costs nothing while windowed (the extra texels are
    * simply downsampled) and keeps the fullscreen view sharp at any
    * window size the display can reach. */
   if (     video_driver_get_video_output_size(&dims, desc, sizeof(desc))
         && (VIDEO_SCALE_W(dims) > 0) && (VIDEO_SCALE_H(dims) > 0))
      cap = (VIDEO_SCALE_W(dims) > VIDEO_SCALE_H(dims))
         ? VIDEO_SCALE_W(dims) : VIDEO_SCALE_H(dims);

   /* The display size is not always available - it depends on the
    * driver and display server - so fall back to the viewport, and
    * take the larger of the two when both are known rather than
    * assuming either bounds the other. */
   vp.dims   = 0;

   if (     video_driver_get_viewport_info(&vp)
         && (VIDEO_SCALE_W(vp.dims) > 0) && (VIDEO_SCALE_H(vp.dims) > 0))
   {
      unsigned vp_w = VIDEO_SCALE_W(vp.dims);
      unsigned vp_h = VIDEO_SCALE_H(vp.dims);
      unsigned v    = (vp_w > vp_h) ? vp_w : vp_h;

      if (v > cap)
         cap = v;
   }

   return cap;
}

void gfx_thumbnail_request(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist,
      size_t idx,
      gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   if (!path_data || !thumbnail)
      return;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   gfx_thumbnail_reset(thumbnail);
   GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);

   /* Update/extract thumbnail path */
   if (gfx_thumbnail_is_enabled(path_data, thumbnail_id))
   {
      if (gfx_thumbnail_update_path(path_data, thumbnail_id))
      {
         const char *thumbnail_path = NULL;
         if (gfx_thumbnail_get_path(path_data, thumbnail_id, &thumbnail_path))
         {
            /* Load thumbnail, if required */
            if (path_is_valid(thumbnail_path))
            {
               gfx_thumbnail_tag_t *thumbnail_tag;

               /* A WebM or MP4 that serves as its own thumbnail takes
                * the sliding window rather than the whole-file still
                * decode; see gfx_thumbnail_try_video_open. */
               if (gfx_thumbnail_try_video_open(thumbnail, thumbnail_path))
                  goto end;

               if (!(thumbnail_tag = (gfx_thumbnail_tag_t*)
                        malloc(sizeof(gfx_thumbnail_tag_t))))
                  goto end;

               /* Configure user data */
               thumbnail_tag->thumbnail = thumbnail;
               thumbnail_tag->list_id   = p_gfx_thumb->list_id;
               thumbnail_tag->park      = gfx_thumb_parking;
               thumbnail_tag->parked    = NULL;
               strlcpy(thumbnail_tag->path, thumbnail_path,
                     sizeof(thumbnail_tag->path));

               /* Would like to cancel any existing image load tasks
                * here, but can't see how to do it... */
               if (gfx_thumbnail_push_still(thumbnail_tag,
                        gfx_thumbnail_upscale_threshold))
               {
                  /* Not thumbnail_tag->list_id: the tag belongs to the
                   * task from the push on, and its callback frees it
                   * when the load completes - on another thread, so a
                   * fast load races this line (heap-use-after-free
                   * under the harness's synchronous stub, where the
                   * callback has already run). */
                  thumbnail->list_id = p_gfx_thumb->list_id;
                  GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_PENDING);
               }
            }
#ifdef HAVE_NETWORKING
            /* Handle on demand thumbnail downloads */
            else if (network_on_demand_thumbnails)
            {
               enum playlist_thumbnail_name_flags curr_flag;
               static char last_img_name[PATH_MAX_LENGTH] = {0};
               bool playlist_use_filename                 = config_get_ptr()->bools.playlist_use_filename;
               if (!playlist)
                  goto end;
               /* Only trigger a thumbnail download if image
                * name has changed since the last call of
                * gfx_thumbnail_request()
                * > Allows gfx_thumbnail_request() to be used
                *   for successive right/left thumbnail requests
                *   with minimal duplication of effort
                *   (i.e. task_push_pl_entry_thumbnail_download()
                *   will automatically cancel if a download for the
                *   existing playlist entry is pending, but the
                *   checks required for this involve significant
                *   overheads. We can avoid this entirely with
                *   a simple string comparison) */
               if (*path_data->content_img)
                  if (string_is_equal(path_data->content_img, last_img_name))
                     goto end;

               strlcpy(last_img_name, path_data->content_img, sizeof(last_img_name));

               /* Get system name */
               if (!*path_data->system)
                  goto end;

               /* Since task_push_pl_entry_download will shift the flag, do not attempt if it is already
                * at second to last option. */
               curr_flag = playlist_get_curr_thumbnail_name_flag(playlist,idx);
               if (   curr_flag & PLAYLIST_THUMBNAIL_FLAG_NONE
                   || curr_flag & PLAYLIST_THUMBNAIL_FLAG_SHORT_NAME)
                  goto end;
               /* Do not try to fetch full names here, if it is not explicitly wanted */
               if (   !playlist_use_filename
                   && !playlist_thumbnail_match_with_filename(playlist)
                   && curr_flag == PLAYLIST_THUMBNAIL_FLAG_INVALID)
                    playlist_update_thumbnail_name_flag(playlist, idx, PLAYLIST_THUMBNAIL_FLAG_FULL_NAME);

               /* Trigger thumbnail download *
                * Note: download will grab all 3 possible thumbnails, no matter
                * what left/right thumbnails are set at the moment */
               task_push_pl_entry_thumbnail_download(path_data->system, playlist,
                     (unsigned)idx, false, true);
            }
#endif
         }
      }
   }

end:
   /* Trigger 'fade in' animation, if required */
   if (GFX_THUMB_STATUS_LOAD(&thumbnail->status) != GFX_THUMBNAIL_STATUS_PENDING)
      gfx_thumbnail_init_fade(p_gfx_thumb,
            thumbnail);
}

/* Requests loading of a specific thumbnail image file
 * (may be used, for example, to load savestate images)
 * - If operation fails, 'thumbnail->status' will be set to
 *   MUI_THUMBNAIL_STATUS_MISSING
 * - If operation is successful, 'thumbnail->status' will be
 *   set to MUI_THUMBNAIL_STATUS_PENDING
 * 'thumbnail' will be populated with texture info/metadata
 * once the image load is complete */
void gfx_thumbnail_request_file(
      const char *file_path, gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   gfx_thumbnail_tag_t *thumbnail_tag = NULL;

   if (!thumbnail)
      return;

   /* Reset thumbnail, then set 'missing' status by default
    * (saves a number of checks later) */
   gfx_thumbnail_reset(thumbnail);
   GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);

   /* Check if file path is valid */
   if (   (!file_path || !*file_path)
       || !path_is_valid(file_path))
      return;

   /* Video containers take the sliding window before the still task
    * is ever queued; see gfx_thumbnail_try_video_open. */
   if (gfx_thumbnail_try_video_open(thumbnail, file_path))
      return;

   /* Load thumbnail */
   if (!(thumbnail_tag = (gfx_thumbnail_tag_t*)malloc(sizeof(gfx_thumbnail_tag_t))))
      return;

   /* Configure user data.
    *
    * The path matters beyond identifying the load: it is what
    * gfx_thumbnail_handle_upload hands to gfx_thumbnail_anim_open for
    * anything that has no video stream to adopt.  Leaving it unset
    * left that call reading uninitialised malloc bytes, so the open
    * saw either an empty string or garbage and returned without ever
    * looking at the file - the static texture uploaded and nothing
    * ever animated.
    *
    * Only this entry point was affected; gfx_thumbnail_request fills
    * the same field for playlist entries, which is why animation
    * worked there and not in the file browser.  Animated WEBP and
    * APNG never adopt a stream, so they never animated here at all;
    * WEBM and MP4 animated only when the still decode happened to
    * leave an adoptable stream behind, which is why the larger ones
    * stopped. */
   thumbnail_tag->thumbnail = thumbnail;
   thumbnail_tag->list_id   = p_gfx_thumb->list_id;
   thumbnail_tag->upscale_threshold = gfx_thumbnail_upscale_threshold;
   thumbnail_tag->park      = gfx_thumb_parking;
   thumbnail_tag->parked    = NULL;
   strlcpy(thumbnail_tag->path, file_path,
         sizeof(thumbnail_tag->path));

   if (gfx_thumbnail_unpark(thumbnail_tag))
      return;

   /* Would like to cancel any existing image load tasks
    * here, but can't see how to do it... */
   if (task_push_image_load_ex(
         file_path, GFX_THUMBNAIL_LOAD_FLAGS,
         gfx_thumbnail_upscale_threshold,
         gfx_thumbnail_downscale_cap(),
         gfx_thumbnail_handle_upload, thumbnail_tag))
   {
      /* Same as gfx_thumbnail_request: the tag is the task's from the
       * push on and may already be freed by its callback. */
      thumbnail->list_id = p_gfx_thumb->list_id;
      GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_PENDING);
   }
}

bool gfx_thumbnail_reset_if_orphaned(gfx_thumbnail_t *thumbnail)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   if (!thumbnail)
      return false;
   if (GFX_THUMB_STATUS_LOAD(&thumbnail->status)
         != GFX_THUMBNAIL_STATUS_PENDING)
      return false;
   if (thumbnail->list_id == p_gfx_thumb->list_id)
      return false;

   /* PENDING with nothing behind it: the load was superseded, and
    * gfx_thumbnail_handle_upload deliberately refuses to touch a
    * thumbnail through a stale tag (the pointer may be dead by then),
    * so nothing else will ever clear this.  Left alone the entry stays
    * blank for as long as it remains on screen. */
   gfx_thumbnail_reset(thumbnail);
   return true;
}

/* Resets (and free()s the current texture of) the
 * specified thumbnail */
void gfx_thumbnail_reset(gfx_thumbnail_t *thumbnail)
{
   if (!thumbnail)
      return;

   /* Release any animation state (decoder + file buffer) */
   gfx_thumbnail_anim_close(thumbnail);

   /* The texture goes with the surface, which also outlives any
    * image of it still on its way to the video thread */
   if (thumbnail->anim_surface)
   {
      gfx_surface_free((gfx_surface_t*)thumbnail->anim_surface);
      thumbnail->anim_surface = NULL;
   }

   /* Ensure any 'fade in' animation is killed */
   if (thumbnail->flags & GFX_THUMB_FLAG_FADE_ACTIVE)
   {
      uintptr_t tag = (uintptr_t)&thumbnail->alpha;
      gfx_animation_kill_by_tag(&tag);
   }

   /* Reset all parameters. Uploads still on their way to the video
    * thread learn on delivery that this reset happened. */
   GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_UNKNOWN);
   thumbnail->texture     = 0;
   thumbnail->dims        = 0;
   thumbnail->alpha       = 0.0f;
   thumbnail->delay_timer = 0.0f;
   thumbnail->flags       = 0;
}

/* Stream processing */

/* Requests loading of the specified thumbnail via
 * the stream interface
 * - Must be called on each frame for the duration
 *   that specified thumbnail is on-screen
 * - Actual load request is deferred by currently
 *   set stream delay
 * - Function becomes a no-op once load request is
 *   made
 * - Thumbnails loaded via this function must be
 *   deleted manually via gfx_thumbnail_reset()
 *   when they move off-screen
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...)
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has a *single* thumbnail.
 *         If each entry has two thumbnails, use
 *         gfx_thumbnail_request_streams() for improved
 *         performance */
void gfx_thumbnail_request_stream(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   if (!thumbnail)
      return;

   /* A request superseded while in flight leaves the slot PENDING with
    * nothing behind it; recover it before the status test below, or the
    * entry stays blank for as long as it is on screen. */
   gfx_thumbnail_reset_if_orphaned(thumbnail);

   /* Only process request if current status
    * is GFX_THUMBNAIL_STATUS_UNKNOWN */
   if (GFX_THUMB_STATUS_LOAD(&thumbnail->status) != GFX_THUMBNAIL_STATUS_UNKNOWN)
      return;

   /* Check if stream delay timer has elapsed */
   thumbnail->delay_timer += p_anim->delta_time;

   if (thumbnail->delay_timer > p_gfx_thumb->stream_delay)
   {
      /* Sanity check */
      if (!path_data)
      {
         /* No path information
          * > Reset thumbnail and set missing status
          *   to prevent repeated load attempts */
         gfx_thumbnail_reset(thumbnail);
         GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);
         thumbnail->alpha  = 1.0f;
         return;
      }

      /* Request image load */
      gfx_thumbnail_request(
            path_data, thumbnail_id, playlist, idx, thumbnail,
            gfx_thumbnail_upscale_threshold,
            network_on_demand_thumbnails);
   }
}

/* Requests loading of the specified thumbnails via
 * the stream interface
 * - Must be called on each frame for the duration
 *   that specified thumbnails are on-screen
 * - Actual load request is deferred by currently
 *   set stream delay
 * - Function becomes a no-op once load request is
 *   made
 * - Thumbnails loaded via this function must be
 *   deleted manually via gfx_thumbnail_reset()
 *   when they move off-screen
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 *         and gfx_thumbnail_set_content*()
 * NOTE 2: 'playlist' and 'idx' are only required here for
 *         on-demand thumbnail download support
 *         (an annoyance...)
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has *two* thumbnails.
 *         If each entry only has a single thumbnail, use
 *         gfx_thumbnail_request_stream() for improved
 *         performance */
void gfx_thumbnail_request_streams(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *right_thumbnail,
      gfx_thumbnail_t *left_thumbnail,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   bool process_r = false;
   bool process_l = false;

   if (!right_thumbnail || !left_thumbnail)
      return;

   /* See gfx_thumbnail_request_stream: recover slots whose in-flight
    * request was superseded, else they never leave PENDING. */
   gfx_thumbnail_reset_if_orphaned(right_thumbnail);
   gfx_thumbnail_reset_if_orphaned(left_thumbnail);

   /* Only process request if current status
    * is GFX_THUMBNAIL_STATUS_UNKNOWN */
   process_r = (GFX_THUMB_STATUS_LOAD(&right_thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN);
   process_l = (GFX_THUMB_STATUS_LOAD(&left_thumbnail->status)  == GFX_THUMBNAIL_STATUS_UNKNOWN);

   if (process_r || process_l)
   {
      /* Check if stream delay timer has elapsed */
      gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
      float delta_time                   = p_anim->delta_time;
      bool request_r                     = false;
      bool request_l                     = false;

      if (process_r)
      {
         right_thumbnail->delay_timer += delta_time;
         request_r                     =
               (right_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
      }

      if (process_l)
      {
         left_thumbnail->delay_timer  += delta_time;
         request_l                     =
               (left_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
      }

      /* Check if one or more thumbnails should be requested */
      if (request_r || request_l)
      {
         /* Sanity check */
         if (!path_data)
         {
            /* No path information
             * > Reset thumbnail and set missing status
             *   to prevent repeated load attempts */
            if (request_r)
            {
               gfx_thumbnail_reset(right_thumbnail);
               GFX_THUMB_STATUS_STORE(&right_thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);
               right_thumbnail->alpha  = 1.0f;
            }

            if (request_l)
            {
               gfx_thumbnail_reset(left_thumbnail);
               GFX_THUMB_STATUS_STORE(&left_thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);
               left_thumbnail->alpha   = 1.0f;
            }

            return;
         }

         /* Request image load */
         if (request_r)
            gfx_thumbnail_request(
                  path_data, GFX_THUMBNAIL_RIGHT, playlist, idx, right_thumbnail,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);

         if (request_l)
            gfx_thumbnail_request(
                  path_data, GFX_THUMBNAIL_LEFT, playlist, idx, left_thumbnail,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
      }
   }
}

/* Handles streaming of the specified thumbnail as it moves
 * on/off screen
 * - Must be called each frame for every on-screen entry
 * - Must be called once for each entry as it moves off-screen
 *   (or can be called each frame - overheads are small)
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 * NOTE 2: This function calls gfx_thumbnail_set_content*()
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has a *single* thumbnail.
 *         If each entry has two thumbnails, use
 *         gfx_thumbnail_process_streams() for improved
 *         performance */
void gfx_thumbnail_process_stream(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      enum gfx_thumbnail_id thumbnail_id,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *thumbnail,
      bool on_screen,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   if (!thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   GFX_THUMBNAIL_STATUS_UNKNOWN */
      gfx_thumbnail_reset_if_orphaned(thumbnail);

      if (GFX_THUMB_STATUS_LOAD(&thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN)
      {
         gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

         /* Check if stream delay timer has elapsed */
         thumbnail->delay_timer += p_anim->delta_time;

         if (thumbnail->delay_timer > p_gfx_thumb->stream_delay)
         {
            /* Update thumbnail content */
            if (   !path_data
                || !playlist
                || !gfx_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               gfx_thumbnail_reset(thumbnail);
               GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);
               thumbnail->alpha  = 1.0f;
               return;
            }

            /* Request image load */
            gfx_thumbnail_request(
                  path_data, thumbnail_id, playlist, idx, thumbnail,
                  gfx_thumbnail_upscale_threshold,
                  network_on_demand_thumbnails);
         }
      }
   }
   else
   {
      /* Entry is off-screen
       * > If status is GFX_THUMBNAIL_STATUS_UNKNOWN,
       *   thumbnail is already in a blank state - but we
       *   must ensure that delay timer is set to zero */
      if (GFX_THUMB_STATUS_LOAD(&thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN)
         thumbnail->delay_timer = 0.0f;
      /* In all other cases, reset thumbnail */
      else
         gfx_thumbnail_reset(thumbnail);
   }
}

/* Handles streaming of the specified thumbnails as they move
 * on/off screen
 * - Must be called each frame for every on-screen entry
 * - Must be called once for each entry as it moves off-screen
 *   (or can be called each frame - overheads are small)
 * NOTE 1: Must be called *after* gfx_thumbnail_set_system()
 * NOTE 2: This function calls gfx_thumbnail_set_content*()
 * NOTE 3: This function is intended for use in situations
 *         where each menu entry has *two* thumbnails.
 *         If each entry only has a single thumbnail, use
 *         gfx_thumbnail_process_stream() for improved
 *         performance */
void gfx_thumbnail_process_streams(
      gfx_thumbnail_path_data_t *path_data,
      gfx_animation_t *p_anim,
      playlist_t *playlist, size_t idx,
      gfx_thumbnail_t *right_thumbnail,
      gfx_thumbnail_t *left_thumbnail,
      bool on_screen,
      unsigned gfx_thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   if (!right_thumbnail || !left_thumbnail)
      return;

   if (on_screen)
   {
      /* Entry is on-screen
       * > Only process if current status is
       *   GFX_THUMBNAIL_STATUS_UNKNOWN */
      bool process_r;
      bool process_l;

      gfx_thumbnail_reset_if_orphaned(right_thumbnail);
      gfx_thumbnail_reset_if_orphaned(left_thumbnail);

      process_r = (GFX_THUMB_STATUS_LOAD(&right_thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN);
      process_l = (GFX_THUMB_STATUS_LOAD(&left_thumbnail->status)  == GFX_THUMBNAIL_STATUS_UNKNOWN);

      if (process_r || process_l)
      {
         /* Check if stream delay timer has elapsed */
         gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
         float delta_time                   = p_anim->delta_time;
         bool request_r                     = false;
         bool request_l                     = false;

         if (process_r)
         {
            right_thumbnail->delay_timer += delta_time;
            request_r                     =
                  (right_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
         }

         if (process_l)
         {
            left_thumbnail->delay_timer  += delta_time;
            request_l                     =
                  (left_thumbnail->delay_timer > p_gfx_thumb->stream_delay);
         }

         /* Check if one or more thumbnails should be requested */
         if (request_r || request_l)
         {
            /* Update thumbnail content */
            if (   !path_data
                || !playlist
                || !gfx_thumbnail_set_content_playlist(path_data, playlist, idx))
            {
               /* Content is invalid
                * > Reset thumbnail and set missing status */
               if (request_r)
               {
                  gfx_thumbnail_reset(right_thumbnail);
                  GFX_THUMB_STATUS_STORE(&right_thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);
                  right_thumbnail->alpha  = 1.0f;
               }

               if (request_l)
               {
                  gfx_thumbnail_reset(left_thumbnail);
                  GFX_THUMB_STATUS_STORE(&left_thumbnail->status, GFX_THUMBNAIL_STATUS_MISSING);
                  left_thumbnail->alpha   = 1.0f;
               }

               return;
            }

            /* Request image load */
            if (request_r)
               gfx_thumbnail_request(
                     path_data, GFX_THUMBNAIL_RIGHT, playlist, idx, right_thumbnail,
                     gfx_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);

            if (request_l)
               gfx_thumbnail_request(
                     path_data, GFX_THUMBNAIL_LEFT, playlist, idx, left_thumbnail,
                     gfx_thumbnail_upscale_threshold,
                     network_on_demand_thumbnails);
         }
      }
   }
   else
   {
      /* Entry is off-screen
       * > If status is GFX_THUMBNAIL_STATUS_UNKNOWN,
       *   thumbnail is already in a blank state - but we
       *   must ensure that delay timer is set to zero
       * > In all other cases, reset thumbnail */
      if (GFX_THUMB_STATUS_LOAD(&right_thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN)
         right_thumbnail->delay_timer = 0.0f;
      else
         gfx_thumbnail_reset(right_thumbnail);

      if (GFX_THUMB_STATUS_LOAD(&left_thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN)
         left_thumbnail->delay_timer = 0.0f;
      else
         gfx_thumbnail_reset(left_thumbnail);
   }
}

/* Thumbnail rendering */

/* Determines the actual screen dimensions of a
 * thumbnail when centred with aspect correct
 * scaling within a rectangle of (width x height) */
void gfx_thumbnail_get_draw_dimensions(
      gfx_thumbnail_t *thumbnail,
      unsigned dims, float scale_factor,
      float *draw_width, float *draw_height)
{
   float width                    = (float)VIDEO_SCALE_W(dims);
   float height                   = (float)VIDEO_SCALE_H(dims);
   float thumb_w, thumb_h, dw, dh;
   float thumbnail_aspect;
   /* thumbnail_aspect / core_aspect: 1 when the core's aspect is not
    * applied, which leaves the sizes below as they are. */
   float core_ratio               = 1.0f;
   video_driver_state_t *video_st = video_state_get_ptr();

   /* Sanity check */
   if (   !thumbnail
       || (width  < 1.0f)
       || (height < 1.0f)
       || (VIDEO_SCALE_W(thumbnail->dims) < 1)
       || (VIDEO_SCALE_H(thumbnail->dims) < 1))
   {
      *draw_width  = 0.0f;
      *draw_height = 0.0f;
      return;
   }

   /* Account for display/thumbnail/core aspect ratio
    * differences */
   thumb_w          = (float)VIDEO_SCALE_W(thumbnail->dims);
   thumb_h          = (float)VIDEO_SCALE_H(thumbnail->dims);
   thumbnail_aspect = thumb_w / thumb_h;
   if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
      core_ratio    = (video_st && video_st->av_info.geometry.aspect_ratio > 0)
         ? thumbnail_aspect / video_st->av_info.geometry.aspect_ratio
         : 1.0f;

   if (thumbnail_aspect > width / height)
   {
      dw = width;
      dh = thumb_h * (dw / thumb_w) * core_ratio;
      if ((thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT) && dh > height)
      {
         dh = height;
         dw = thumb_w * (dh / thumb_h) / core_ratio;
      }
   }
   else
   {
      dh = height;
      dw = thumb_w * (dh / thumb_h) / core_ratio;
   }

   /* Final overwidth check */
   if (dw > width)
   {
      dw = width;
      dh = thumb_h * (dw / thumb_w) * core_ratio;
   }

   /* Account for scale factor
    * > Side note: We cannot use the gfx_display_ctx_draw_t
    *   'scale_factor' parameter for scaling thumbnails,
    *   since this clips off any part of the expanded image
    *   that extends beyond the bounding box. But even if
    *   it didn't, we can't get real screen dimensions
    *   without scaling manually... */
   *draw_width  = dw * scale_factor;
   *draw_height = dh * scale_factor;
}

/* Draws specified thumbnail with specified alignment
 * (and aspect correct scaling) within a rectangle of
 * (width x height).
 * 'shadow' defines an optional shadow effect (may be
 * set to NULL if a shadow effect is not required).
 * NOTE: Setting scale_factor > 1.0f will increase the
 *       size of the thumbnail beyond the limits of the
 *       (width x height) rectangle (alignment + aspect
 *       correct scaling is preserved). Use with caution */

void gfx_thumbnail_draw(
      void *userdata,
      unsigned video_dims,
      gfx_thumbnail_t *thumbnail,
      float x, float y, unsigned dims,
      enum gfx_thumbnail_alignment alignment,
      float alpha, float scale_factor,
      gfx_thumbnail_shadow_t *shadow)
{
   unsigned video_width              = VIDEO_SCALE_W(video_dims);
   unsigned video_height             = VIDEO_SCALE_H(video_dims);
   unsigned width                    = VIDEO_SCALE_W(dims);
   unsigned height                   = VIDEO_SCALE_H(dims);
   gfx_display_t            *p_disp  = disp_get_ptr();
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;
   /* Sanity check */
   if (
            !thumbnail
         || !dispctx
         || (width         < 1)
         || (height        < 1)
         || (alpha        <= 0.0f)
         || (scale_factor <= 0.0f)
      )
      return;

   /* Only draw thumbnail if it is available...
    * > Acquire-load pairs with release-store in
    *   gfx_thumbnail_handle_upload() to ensure
    *   texture/width/height are visible when we
    *   read AVAILABLE */
   if (GFX_THUMB_STATUS_LOAD(&thumbnail->status) == GFX_THUMBNAIL_STATUS_AVAILABLE)
   {
      gfx_display_ctx_draw_t draw;
      struct video_coords coords;
      math_matrix_4x4 mymat;
      float draw_width;
      float draw_height;
      float draw_x;
      float draw_y;

      /* Snapshot fields — read once, use local copies for the
       * remainder of this draw call.  The main thread may update
       * the live struct concurrently (upload callback, reset, or
       * animation tick on alpha), but these locals are stable. */
      uintptr_t thumb_texture = thumbnail->texture;
      unsigned  thumb_width   = VIDEO_SCALE_W(thumbnail->dims);
      unsigned  thumb_height  = VIDEO_SCALE_H(thumbnail->dims);
      float     thumb_alpha   = thumbnail->alpha;
      uint8_t   thumb_flags   = thumbnail->flags;

      float thumbnail_alpha     = thumb_alpha * alpha;
      float thumbnail_color[16] = {
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f
      };

      /* Set thumbnail opacity */
      if (thumbnail_alpha <= 0.0f)
         return;
      if (thumbnail_alpha < 1.0f)
         gfx_display_set_alpha(thumbnail_color, thumbnail_alpha);

      /* Get thumbnail dimensions using snapshot values
       * > Build a temporary on the stack so
       *   gfx_thumbnail_get_draw_dimensions reads
       *   consistent data without touching the live struct */
      {
         gfx_thumbnail_t thumb_snapshot;
         thumb_snapshot.texture = thumb_texture;
         thumb_snapshot.dims    = VIDEO_SCALE_PACK(thumb_width, thumb_height);
         thumb_snapshot.alpha   = thumb_alpha;
         thumb_snapshot.flags   = thumb_flags;
         /* Local snapshot: initialise the atomic-typed status
          * field via _init since this is its first write --
          * direct assignment to the C11/C++11 atomic forms is
          * illegal.  This local is never observed by another
          * thread, so no ordering is required. */
         retro_atomic_int_init(&thumb_snapshot.status, GFX_THUMBNAIL_STATUS_AVAILABLE);
         thumb_snapshot.delay_timer = 0.0f;
         gfx_thumbnail_get_draw_dimensions(
               &thumb_snapshot, dims, scale_factor,
               &draw_width, &draw_height);
      }

      gfx_display_blend_begin(dispctx, userdata);

      if (!dispctx->handles_transform)
      {
         /* Perform 'rotation' step
          * > Note that rotation does not actually work...
          * > It rotates the image all right, but distorts it
          *   to fit the aspect of the bounding box while clipping
          *   off any 'corners' that extend beyond the bounding box
          * > Since the result is visual garbage, we disable
          *   rotation entirely
          * > But we still have to call gfx_display_rotate_z(),
          *   or nothing will be drawn...
          */
         float cosine             = 1.0f; /* cos(rad)  = cos(0)  = 1.0f */
         float sine               = 0.0f; /* sine(rad) = sine(0) = 0.0f */
         gfx_display_rotate_z(p_disp, &mymat, cosine, sine, userdata);
      }

      /* Configure draw object
       * > Note: Colour, width/height and position must
       *   be set *after* drawing any shadow effects */
      coords.vertices      = 4;
      coords.vertex        = NULL;
      coords.tex_coord     = NULL;
      coords.lut_tex_coord = NULL;

      draw.scale_factor    = 1.0f;
      draw.rotation        = 0.0f;
      draw.coords          = &coords;
      draw.matrix_data     = &mymat;
      draw.texture         = thumb_texture;
      draw.pipeline_id     = 0;

      /* Set thumbnail alignment within bounding box */
      switch (alignment)
      {
         case GFX_THUMBNAIL_ALIGN_TOP:
            /* Centred horizontally */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            /* Drawn at top of bounding box */
            draw_y = (float)video_height - y - draw_height;
            break;
         case GFX_THUMBNAIL_ALIGN_BOTTOM:
            /* Centred horizontally */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            /* Drawn at bottom of bounding box */
            draw_y = (float)video_height - y - (float)height;
            break;
         case GFX_THUMBNAIL_ALIGN_LEFT:
            /* Drawn at left side of bounding box */
            draw_x = x;
            /* Centred vertically */
            draw_y = (float)video_height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
         case GFX_THUMBNAIL_ALIGN_RIGHT:
            /* Drawn at right side of bounding box */
            draw_x = x + (float)width - draw_width;
            /* Centred vertically */
            draw_y = (float)video_height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
         case GFX_THUMBNAIL_ALIGN_CENTRE:
         default:
            /* Centred both horizontally and vertically */
            draw_x = x + ((float)width - draw_width) / 2.0f;
            draw_y = (float)video_height - y - draw_height - ((float)height - draw_height) / 2.0f;
            break;
      }

      /* Draw shadow effect, if required */
      if (shadow)
      {
         /* Sanity check */
         if (     (shadow->type != GFX_THUMBNAIL_SHADOW_NONE)
               && (shadow->alpha > 0.0f))
         {
            float shadow_width;
            float shadow_height;
            float shadow_x;
            float shadow_y;
            float shadow_color[16] = {
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 0.0f, 0.0f, 1.0f
            };
            float shadow_alpha     = thumbnail_alpha;

            /* Set shadow opacity */
            if (shadow->alpha < 1.0f)
               shadow_alpha *= shadow->alpha;

            gfx_display_set_alpha(shadow_color, shadow_alpha);

            /* Configure shadow based on effect type
             * > Not using a switch() here, since we've
             *   already eliminated GFX_THUMBNAIL_SHADOW_NONE */
            if (shadow->type == GFX_THUMBNAIL_SHADOW_OUTLINE)
            {
               shadow_width  = draw_width  + (float)(shadow->outline.width * 2);
               shadow_height = draw_height + (float)(shadow->outline.width * 2);
               shadow_x      = draw_x - (float)shadow->outline.width;
               shadow_y      = draw_y - (float)shadow->outline.width;
            }
            /* Default: GFX_THUMBNAIL_SHADOW_DROP */
            else
            {
               shadow_width  = draw_width;
               shadow_height = draw_height;
               shadow_x      = draw_x + shadow->drop.x_offset;
               shadow_y      = draw_y - shadow->drop.y_offset;
            }

            /* Apply shadow draw object configuration */
            coords.color = (const float*)shadow_color;
            draw.dims    = VIDEO_SCALE_PACK((unsigned)shadow_width, (unsigned)shadow_height);
            draw.pos     = VIDEO_POS_PACK(VIDEO_PX(shadow_x),
                  VIDEO_PX(shadow_y));

            /* Draw shadow */
            if (VIDEO_SCALE_H(draw.dims) > 0 && VIDEO_SCALE_W(draw.dims) > 0)
               gfx_display_draw(dispctx, &draw, userdata,
                        VIDEO_SCALE_PACK(video_width, video_height));
         }
      }

      /* Final thumbnail draw object configuration */
      coords.color = (const float*)thumbnail_color;
      draw.dims    = VIDEO_SCALE_PACK((unsigned)draw_width, (unsigned)draw_height);
      draw.pos     = VIDEO_POS_PACK(VIDEO_PX(draw_x), VIDEO_PX(draw_y));

      /* Draw thumbnail */
      if (VIDEO_SCALE_H(draw.dims) > 0 && VIDEO_SCALE_W(draw.dims) > 0)
         gfx_display_draw(dispctx, &draw, userdata,
               VIDEO_SCALE_PACK(video_width, video_height));

      gfx_display_blend_end(dispctx, userdata);
   }
}

/* Returns currently set thumbnail 'type' (Named_Snaps,
 * Named_Titles, Named_Boxarts, Named_Logos) for specified thumbnail
 * identifier (right, left) */
static const char *gfx_thumbnail_get_type(
      unsigned gfx_thumbnails,
      unsigned left_thumbnails,
      unsigned icon_thumbnails,
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   if (path_data)
   {
      unsigned type                 = 0;
      switch (thumbnail_id)
      {
         case GFX_THUMBNAIL_RIGHT:
            if (   path_data->playlist_right_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               type = (unsigned)path_data->playlist_right_mode - 1;
            else
               type = gfx_thumbnails;
            break;
         case GFX_THUMBNAIL_LEFT:
            if (   path_data->playlist_left_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               type = (unsigned)path_data->playlist_left_mode - 1;
            else
               type = left_thumbnails;
            break;
         case GFX_THUMBNAIL_ICON:
            if (   path_data->playlist_icon_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               type = (unsigned)path_data->playlist_icon_mode - 1;
            else
               type = icon_thumbnails;
            break;
         default:
            goto end;
      }

      switch (type)
      {
         case 1:
            return "Named_Snaps";
         case 2:
            return "Named_Titles";
         case 3:
            return "Named_Boxarts";
         case 4:
            return "Named_Logos";
         case 0:
         default:
            break;
      }
   }

end:
   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
}

/* Fills content_img field of path_data using existing
 * content_label field (for internal use only) */
void gfx_thumbnail_fill_content_img(char *s,
   size_t len, const char *src, bool shorten)
{
   char *scrub_char_ptr = NULL;
   /* Copy source label string */
   size_t _len          = strlcpy(s, src, len);

   /* Shortening logic: up to first space + bracket */
   if (shorten)
   {
      int bracketpos    = -1;
      if ((bracketpos = string_find_index_substring_string(src, " (")) > 0)
         _len = bracketpos;
      /* Explicit zero if short name is same as standard name - saves some queries later. */
      else
      {
         s[0] = '\0';
         return;
      }
   }
   /* Scrub characters that are not cross-platform and/or violate the
    * No-Intro filename standard:
    * https://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).pdf
    * Replace these characters in the entry name with underscores */
   while ((scrub_char_ptr = strpbrk(s, "&*/:`\"<>?\\|")))
      *scrub_char_ptr = '_';
   /* Add PNG extension */
   strlcpy_lit(s + _len, ".png", len - _len);
}

/* Resets thumbnail path data
 * (blanks all internal string containers) */
void gfx_thumbnail_path_reset(gfx_thumbnail_path_data_t *path_data)
{
   if (!path_data)
      return;

   path_data->system_len           = 0;
   path_data->system[0]            = '\0';
   path_data->content_path[0]      = '\0';
   path_data->content_label[0]     = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0]   = '\0';
   path_data->content_img[0]       = '\0';
   path_data->content_img_full[0]  = '\0';
   path_data->content_img_short[0] = '\0';
   path_data->right_path[0]        = '\0';
   path_data->left_path[0]         = '\0';
   path_data->icon_path[0]         = '\0';

   path_data->playlist_right_mode = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_icon_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
}

/* Initialisation */

/* Creates new thumbnail path data container.
 * Returns handle to new gfx_thumbnail_path_data_t object.
 * on success, otherwise NULL.
 * Note: Returned object must be free()d */
gfx_thumbnail_path_data_t *gfx_thumbnail_path_init(void)
{
   /* Use calloc rather than malloc.  gfx_thumbnail_path_reset()
    * resets the string buffers and the playlist_*_mode fields
    * but does NOT touch playlist_index, leaving it as garbage
    * from malloc until the first set_content_*() call.  Read
    * sites in xmb (xmb_set_title) sample
    * thumbnail_path_data->playlist_index before any setter has
    * necessarily run, then pass it to playlist_get_index() --
    * which is bounds-checked, so a garbage index just returns
    * a stale pl_entry rather than crashing, but the code is
    * latently UB and a future read site without the same defence
    * would inherit a real bug.  Zero-init via calloc closes
    * the window without churning path_reset's API. */
   gfx_thumbnail_path_data_t *path_data = (gfx_thumbnail_path_data_t*)
      calloc(1, sizeof(*path_data));
   if (!path_data)
      return NULL;

   gfx_thumbnail_path_reset(path_data);

   return path_data;
}

/* Utility Functions */

/* Returns true if specified thumbnail is enabled
 * (i.e. if 'type' is not equal to MENU_ENUM_LABEL_VALUE_OFF) */
/* The decision with the three mode settings as values: the live
 * wrapper reads them, the _cfg path passes its capture, and this
 * reads nothing. */
static bool gfx_thumbnail_is_enabled_values(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      unsigned gfx_thumbnails,
      unsigned menu_left_thumbnails,
      unsigned menu_icon_thumbnails)
{
   if (path_data)
   {
      switch (thumbnail_id)
      {
         case GFX_THUMBNAIL_RIGHT:
            if (   path_data->playlist_right_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               return path_data->playlist_right_mode != PLAYLIST_THUMBNAIL_MODE_OFF;
            return gfx_thumbnails != 0;
         case GFX_THUMBNAIL_LEFT:
            if (   path_data->playlist_left_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
               return path_data->playlist_left_mode != PLAYLIST_THUMBNAIL_MODE_OFF;
            return menu_left_thumbnails != 0;
         case GFX_THUMBNAIL_ICON:
            if (   path_data->playlist_icon_mode 
                != PLAYLIST_THUMBNAIL_MODE_DEFAULT)
                return path_data->playlist_icon_mode != PLAYLIST_THUMBNAIL_MODE_OFF;
            return menu_icon_thumbnails != 0;
         default:
            break;
      }
   }

   return false;
}

bool gfx_thumbnail_is_enabled(gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   settings_t *settings = config_get_ptr();
   return gfx_thumbnail_is_enabled_values(path_data, thumbnail_id,
         settings->uints.gfx_thumbnails,
         settings->uints.menu_left_thumbnails,
         settings->uints.menu_icon_thumbnails);
}

/* Setters */

/* Sets current 'system' (default database name).
 * Returns true if 'system' is valid.
 * If playlist is provided, extracts system-specific
 * thumbnail assignment metadata (required for accurate
 * usage of gfx_thumbnail_is_enabled())
 * > Used as a fallback when individual content lacks an
 *   associated database name */
bool gfx_thumbnail_set_system(gfx_thumbnail_path_data_t *path_data,
      const char *system, playlist_t *playlist)
{
   if (!path_data)
      return false;

   /* When system is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]       = '\0';
   path_data->left_path[0]        = '\0';

   /* 'Reset' path_data system string */
   path_data->system_len          = 0;
   path_data->system[0]           = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;

   if (!system || !*system)
      return false;

   /* Hack: There is only one MAME thumbnail repo,
    * so filter any input starting with 'MAME...' */
   if (strncmp(system, "MAME", 4) == 0)
      path_data->system_len = strlcpy_lit(path_data->system, "MAME", sizeof(path_data->system));
   else
      path_data->system_len = strlcpy(path_data->system, system, sizeof(path_data->system));

   /* Addendum: Now that we have per-playlist thumbnail display
    * modes, we must extract them here - otherwise
    * gfx_thumbnail_is_enabled() will go out of sync */
   if (playlist)
   {
      const char *playlist_path    = playlist_get_conf_path(playlist);

      /* Note: This is not considered an error
       * (just means that input playlist is ignored) */
      if (playlist_path && *playlist_path)
      {
         const char *playlist_file = path_basename_nocompression(playlist_path);
         /* Note: This is not considered an error
          * (just means that input playlist is ignored) */
         if (playlist_file && *playlist_file)
         {
            /* Check for history/favourites playlists */
            bool playlist_valid =
               (   memcmp(system, "history", 8) == 0
                && memcmp(playlist_file,
                   FILE_PATH_CONTENT_HISTORY,
                   sizeof(FILE_PATH_CONTENT_HISTORY)) == 0)
               || (     memcmp(system, "favorites", 10) == 0
                     && memcmp(playlist_file,
                        FILE_PATH_CONTENT_FAVORITES,
                        sizeof(FILE_PATH_CONTENT_FAVORITES)) == 0);

            /* This means we have to work a little harder
             * i.e. check whether the cached playlist file
             * matches the database name */
            if (!playlist_valid)
            {
               char playlist_name[NAME_MAX_LENGTH];
               fill_pathname(playlist_name, playlist_file, "", sizeof(playlist_name));
               playlist_valid = string_is_equal(playlist_name, system);
            }

            /* If we have a valid playlist, extract thumbnail modes */
            if (playlist_valid)
            {
               path_data->playlist_right_mode =
                  playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_RIGHT);
               path_data->playlist_left_mode =
                  playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_LEFT);
            }
         }
      }
   }

   return true;
}

/* Sets current thumbnail content according to the specified label.
 * Returns true if content is valid */
bool gfx_thumbnail_set_content(gfx_thumbnail_path_data_t *path_data, const char *label)
{
   if (!path_data)
      return false;

   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]        = '\0';
   path_data->left_path[0]         = '\0';

   /* 'Reset' path_data content strings */
   path_data->content_path[0]      = '\0';
   path_data->content_label[0]     = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0]   = '\0';
   path_data->content_img[0]       = '\0';
   path_data->content_img_full[0]  = '\0';
   path_data->content_img_short[0] = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode   = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_index       = 0;

   if (!label || !*label)
      return false;

   /* Cache content label */
   strlcpy(path_data->content_label,
         label, sizeof(path_data->content_label));

   /* Determine content image name */
   gfx_thumbnail_fill_content_img(path_data->content_img,
         sizeof(path_data->content_img),
         path_data->content_label, false);
   gfx_thumbnail_fill_content_img(path_data->content_img_short,
         sizeof(path_data->content_img_short),
         path_data->content_label, true);

   /* Have to set content path to *something*...
    * Just use label value (it doesn't matter) */
   strlcpy(path_data->content_path, label, sizeof(path_data->content_path));

   /* Redundant error check... */
   return *path_data->content_img;
}

/* Sets current thumbnail content to the specified image.
 * Returns true if content is valid */
bool gfx_thumbnail_set_content_image(
      gfx_thumbnail_path_data_t *path_data,
      const char *img_dir, const char *img_name)
{
   if (!path_data)
      return false;

   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]        = '\0';
   path_data->left_path[0]         = '\0';

   /* 'Reset' path_data content strings */
   path_data->content_path[0]      = '\0';
   path_data->content_label[0]     = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0]   = '\0';
   path_data->content_img[0]       = '\0';
   path_data->content_img_full[0]  = '\0';
   path_data->content_img_short[0] = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode  = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode   = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_index       = 0;

   if ((!img_dir || !*img_dir) || (!img_name || !*img_name))
      return false;

   /* Images, and WebM/MP4 files (whose video track the thumbnail
    * pipeline decodes like an animated WebP), can serve as their own
    * thumbnail */
   if (   (path_is_media_type(img_name) != RARCH_CONTENT_IMAGE)
       && (image_texture_get_type(img_name) != IMAGE_TYPE_WEBM)
       && (image_texture_get_type(img_name) != IMAGE_TYPE_MP4))
      return false;

   /* Cache content image name */
   strlcpy(path_data->content_img,
            img_name, sizeof(path_data->content_img));

   fill_pathname(
         path_data->content_label,
         path_data->content_img, "",
         sizeof(path_data->content_label));

   /* Set file path */
   fill_pathname_join_special(path_data->content_path,
      img_dir, img_name, sizeof(path_data->content_path));

   /* Set core name to "imageviewer" */
   strlcpy_lit(path_data->content_core_name,
         "imageviewer",
         sizeof(path_data->content_core_name));

   /* Set database name (arbitrarily) to "_images_"
    * (required for compatibility with gfx_thumbnail_update_path(),
    * but not actually used...) */
   strlcpy_lit(path_data->content_db_name,
         "_images_", sizeof(path_data->content_db_name));

   /* Redundant error check */
   return *path_data->content_path;
}

/* Sets current thumbnail content to the specified playlist entry.
 * Returns true if content is valid.
 * > Note: It is always best to use playlists when setting
 *   thumbnail content, since there is no guarantee that the
 *   corresponding menu entry label will contain a useful
 *   identifier (it may be 'tainted', e.g. with the current
 *   core name). 'Real' labels should be extracted from source */
bool gfx_thumbnail_set_content_playlist(
      gfx_thumbnail_path_data_t *path_data, playlist_t *playlist, size_t idx)
{
   const char *content_path           = NULL;
   const char *content_label          = NULL;
   const char *core_name              = NULL;
   const char *db_name                = NULL;
   const struct playlist_entry *entry = NULL;

   if (!path_data)
      return false;

   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0]           = '\0';
   path_data->left_path[0]            = '\0';

   /* 'Reset' path_data content strings */
   path_data->content_path[0]         = '\0';
   path_data->content_label[0]        = '\0';
   path_data->content_core_name[0]    = '\0';
   path_data->content_db_name[0]      = '\0';
   path_data->content_img[0]          = '\0';
   path_data->content_img_full[0]     = '\0';
   path_data->content_img_short[0]    = '\0';

   /* Must also reset playlist thumbnail display modes */
   path_data->playlist_right_mode     = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_left_mode      = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   path_data->playlist_index          = 0;

   if (!playlist)
      return false;

   if (idx >= playlist_get_size(playlist))
      return false;

   /* Read playlist values */
   playlist_get_index(playlist, idx, &entry);

   if (!entry)
      return false;

   content_path  = entry->path;
   content_label = entry->label;
   core_name     = entry->core_name;
   db_name       = entry->db_name;

   /* Content without a path is invalid by definition */
   if (!content_path || !*content_path)
      return false;

   /* Cache content path
    * (This is required for imageviewer, history and favourites content) */
   strlcpy(path_data->content_path,
            content_path, sizeof(path_data->content_path));

   /* Cache core name
    * (This is required for imageviewer content) */
   if (core_name && *core_name)
      strlcpy(path_data->content_core_name,
            core_name, sizeof(path_data->content_core_name));

   /* Get content label */
   if (content_label && *content_label)
      strlcpy(path_data->content_label,
            content_label, sizeof(path_data->content_label));
   else
      fill_pathname(path_data->content_label,
            path_basename(content_path),
            "", sizeof(path_data->content_label));

   /* Determine content image name */
   {
      char tmp_buf[NAME_MAX_LENGTH];
      fill_pathname(tmp_buf, path_basename(path_data->content_path),
            "", sizeof(tmp_buf));

      gfx_thumbnail_fill_content_img(path_data->content_img_full,
         sizeof(path_data->content_img_full), tmp_buf, false);
      gfx_thumbnail_fill_content_img(path_data->content_img,
         sizeof(path_data->content_img), path_data->content_label, false);

      /* Explicit zero if full name is same as standard name 
         - saves some queries later. */
      if (string_is_equal(path_data->content_img,
          path_data->content_img_full))
         path_data->content_img_full[0] = '\0';

      gfx_thumbnail_fill_content_img(path_data->content_img_short,
         sizeof(path_data->content_img_short), path_data->content_label, true);
   }

   /* Store playlist index */
   path_data->playlist_index = idx;

   /* Redundant error check... */
   if (!*path_data->content_img)
      return false;

   /* Thumbnail image name is done -> now check if
    * per-content database name is defined */
   if (!db_name || !*db_name)
      playlist_get_db_name(playlist, idx, &db_name);
   if (db_name && *db_name)
   {
      /* Hack: There is only one MAME thumbnail repo,
       * so filter any input starting with 'MAME...' */
      if (strncmp(db_name, "MAME", 4) == 0)
      {
         path_data->content_db_name[0] = path_data->content_db_name[2] = 'M';
         path_data->content_db_name[1] = 'A';
         path_data->content_db_name[3] = 'E';
         path_data->content_db_name[4] = '\0';
      }
      else
      {
         char tmp_buf[NAME_MAX_LENGTH];
         const char *pos      = strchr(db_name, '|');
         /* If db_name comes from core info, and there are multiple
          * databases mentioned separated by |, use only first one */
         if (pos && (size_t) (pos - db_name) + 1 < sizeof(tmp_buf))
            strlcpy(tmp_buf, db_name, (size_t)(pos - db_name) + 1);
         else
            strlcpy(tmp_buf, db_name, sizeof(tmp_buf));

         fill_pathname(path_data->content_db_name,
               tmp_buf, "",
               sizeof(path_data->content_db_name));
      }
   }

   /* Playlist entry is valid -> it is now 'safe' to
    * extract any remaining playlist metadata
    * (i.e. thumbnail display modes) */
   path_data->playlist_right_mode =
         playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_RIGHT);
   path_data->playlist_left_mode =
         playlist_get_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_LEFT);

   return true;
}

/* Updaters */

/* Updates path for specified thumbnail identifier (right, left).
 * Must be called after:
 * - gfx_thumbnail_set_system()
 * - gfx_thumbnail_set_content*()
 * ...and before:
 * - gfx_thumbnail_get_path()
 * Returns true if generated path is valid */
void gfx_thumbnail_dir_config_capture(gfx_thumbnail_dir_config_t *cfg)
{
   settings_t *settings        = config_get_ptr();
   strlcpy(cfg->dir_thumbnails, settings->paths.directory_thumbnails,
         sizeof(cfg->dir_thumbnails));
   cfg->playlist_allow_non_png = settings->bools.playlist_allow_non_png;
   cfg->gfx_thumbnails         = settings->uints.gfx_thumbnails;
   cfg->menu_left_thumbnails   = settings->uints.menu_left_thumbnails;
   cfg->menu_icon_thumbnails   = settings->uints.menu_icon_thumbnails;
}

bool gfx_thumbnail_update_path(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   gfx_thumbnail_dir_config_t cfg;
   gfx_thumbnail_dir_config_capture(&cfg);
   return gfx_thumbnail_update_path_cfg(path_data, thumbnail_id, &cfg);
}

bool gfx_thumbnail_update_path_cfg(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      const gfx_thumbnail_dir_config_t *cfg)
{
   char content_dir[DIR_MAX_LENGTH];
   const char *system_name       = NULL;
   char *thumbnail_path          = NULL;
   const char *dir_thumbnails    = cfg->dir_thumbnails;
   bool playlist_allow_non_png   = cfg->playlist_allow_non_png;
   unsigned gfx_thumbnails       = cfg->gfx_thumbnails;
   unsigned menu_left_thumbnails = cfg->menu_left_thumbnails;
   unsigned menu_icon_thumbnails = cfg->menu_icon_thumbnails;
   /* Thumbnail extension order. The default (i.e. png) is always the first. */
   const char* const SUPPORTED_THUMBNAIL_EXTENSIONS[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga",
#ifdef HAVE_RWEBP
         ".webp",
#endif
#ifdef HAVE_RWEBM
         ".webm",
#endif
#ifdef HAVE_RMP4
         ".mp4",
#endif
         0 };
   /* Entries before the terminating null */
#define MAX_SUPPORTED_THUMBNAIL_EXTENSIONS \
   ((int)(sizeof(SUPPORTED_THUMBNAIL_EXTENSIONS) / \
          sizeof(SUPPORTED_THUMBNAIL_EXTENSIONS[0])) - 1)

   if (!path_data)
      return false;

   /* Determine which path we are updating... */
   switch (thumbnail_id)
   {
      case GFX_THUMBNAIL_RIGHT:
         thumbnail_path = path_data->right_path;
         break;
      case GFX_THUMBNAIL_LEFT:
         thumbnail_path = path_data->left_path;
         break;
      case GFX_THUMBNAIL_ICON:
         thumbnail_path = path_data->icon_path;
         break;
      default:
         return false;
   }

   content_dir[0]    = '\0';

   /* Sundry error checking */
   if (!dir_thumbnails || !*dir_thumbnails)
      return false;

   if (!gfx_thumbnail_is_enabled_values(path_data, thumbnail_id,
         cfg->gfx_thumbnails, cfg->menu_left_thumbnails,
         cfg->menu_icon_thumbnails))
      return false;

   /* Generate new path */

   /* > Check path_data for empty strings */
   if (       (!*path_data->content_path)
       ||     (!*path_data->content_img)
       || (   (!*path_data->system)
           && (!*path_data->content_db_name)))
      return false;

   /* > Get current system */
   if (!*path_data->content_db_name)
   {
      /* If this is a content history or favorites playlist
       * then the current 'path_data->system' string is
       * meaningless. In this case, we fall back to the
       * content directory name */
      if (     memcmp(path_data->system, "history", 8) == 0
            || memcmp(path_data->system, "favorites", 10) == 0)
      {
         if (gfx_thumbnail_get_content_dir(
               path_data, content_dir, sizeof(content_dir)) == 0)
            return false;

         system_name = content_dir;
      }
      else
         system_name = path_data->system;
   }
   else
      system_name = path_data->content_db_name;

   /* > Special case: thumbnail for imageviewer content
    *   is the image file itself */
   if (   memcmp(system_name, "images_history", sizeof("images_history")) == 0
       || memcmp(path_data->content_core_name, "imageviewer", sizeof("imageviewer")) == 0)
   {
      /* imageviewer content is identical for left and right thumbnails */
      if (path_is_media_type(path_data->content_path) == RARCH_CONTENT_IMAGE)
         strlcpy(thumbnail_path,
               path_data->content_path, PATH_MAX_LENGTH * sizeof(char));
      /* A WebM or MP4 file is likewise its own (animated) thumbnail.
       *
       * Where the platform can reserve address space the load opens
       * over a sliding window and commits only its head plus that
       * window, so the file's length costs nothing and only the
       * window has to be admitted - a multi-hour 4K recording
       * previews from the same budget as a ten-second clip.  The
       * whole-file cap applied here unconditionally, which refused
       * every video past GFX_THUMB_ANIM_ABS_MAX_FILE outright: the
       * path resolved empty, the thumbnail went MISSING, and no
       * decode was ever attempted.  That cap predates the windowed
       * open and had simply never been revisited.
       *
       * Without reservation support the open degrades to a whole-file
       * read, so the length-based test is still the right one there.
       * This mirrors the admission in gfx_thumbnail_anim_open. */
      else if (   (   (image_texture_get_type(path_data->content_path)
                        == IMAGE_TYPE_WEBM)
                   || (image_texture_get_type(path_data->content_path)
                        == IMAGE_TYPE_MP4))
               && (data_transfer_reserve_supported()
                     ? gfx_anim_preview_window_admit(0)
                     : gfx_anim_preview_admit(
                        (uint64_t)path_get_size(
                           path_data->content_path), 0)))
         strlcpy(thumbnail_path,
               path_data->content_path, PATH_MAX_LENGTH * sizeof(char));
   }
   else
   {
      int  i;
      char tmp_buf[DIR_MAX_LENGTH];
      const char *type = gfx_thumbnail_get_type(gfx_thumbnails,
            menu_left_thumbnails, menu_icon_thumbnails, path_data, thumbnail_id);
      bool thumbnail_found = false;
      /* > Normal content: assemble path */

      /* >> Base + system name */
      fill_pathname_join_special(thumbnail_path, dir_thumbnails,
            system_name, PATH_MAX_LENGTH * sizeof(char));

      /* >> Add type */
      fill_pathname_join_special(tmp_buf, thumbnail_path, type, sizeof(tmp_buf));

      thumbnail_path[0] = '\0';

      /* >> Add content image - first try with label (database name) */
      if (*path_data->content_img)
      {
         fill_pathname_join_special(thumbnail_path, tmp_buf,
               path_data->content_img, PATH_MAX_LENGTH * sizeof(char));
         thumbnail_found = path_is_valid(thumbnail_path);

         if (playlist_allow_non_png && thumbnail_path && *thumbnail_path)
         {
            for (i = 1; i < MAX_SUPPORTED_THUMBNAIL_EXTENSIONS && !thumbnail_found; i++)
            {
               if (!path_get_extension_mutable(thumbnail_path))
                  continue;

               strlcpy(path_get_extension_mutable(thumbnail_path),
                     SUPPORTED_THUMBNAIL_EXTENSIONS[i], 6);
               thumbnail_found = path_is_valid(thumbnail_path);
            }
         }
      }

      /* >> Add content image - second try with full file name */
      if (   !thumbnail_found 
          && *path_data->content_img_full)
      {
         thumbnail_path[0] = '\0';
         fill_pathname_join_special(thumbnail_path, tmp_buf,
               path_data->content_img_full, PATH_MAX_LENGTH * sizeof(char));
         thumbnail_found = path_is_valid(thumbnail_path);

         if (playlist_allow_non_png && thumbnail_path && *thumbnail_path)
         {
            for (i = 1; i < MAX_SUPPORTED_THUMBNAIL_EXTENSIONS && !thumbnail_found; i++)
            {
               if (!path_get_extension_mutable(thumbnail_path))
                  continue;

               strlcpy(path_get_extension_mutable(thumbnail_path),
                     SUPPORTED_THUMBNAIL_EXTENSIONS[i], 6);
               thumbnail_found = path_is_valid(thumbnail_path);
            }
         }
      }

      /* >> Add content image - third try with shortened name (title only) */
      if (!thumbnail_found && *path_data->content_img_short)
      {
         thumbnail_path[0] = '\0';
         fill_pathname_join_special(thumbnail_path, tmp_buf,
               path_data->content_img_short, PATH_MAX_LENGTH * sizeof(char));
         thumbnail_found = path_is_valid(thumbnail_path);

         if (playlist_allow_non_png && thumbnail_path && *thumbnail_path)
         {
            for (i = 1; i < MAX_SUPPORTED_THUMBNAIL_EXTENSIONS && !thumbnail_found; i++)
            {
               if (!path_get_extension_mutable(thumbnail_path))
                  continue;

               strlcpy(path_get_extension_mutable(thumbnail_path),
                     SUPPORTED_THUMBNAIL_EXTENSIONS[i], 6);
               thumbnail_found = path_is_valid(thumbnail_path);
            }
         }
      }
      /* This logic is valid for locally stored thumbnails. For optional downloads,
       * gfx_thumbnail_get_img_name() is used */
   }

   /* Final error check - is cached path empty? */
   return thumbnail_path && *thumbnail_path;
}

/* Getters */

/* Fetches current content directory.
 * Returns true if content directory is valid. */
size_t gfx_thumbnail_get_content_dir(gfx_thumbnail_path_data_t *path_data,
      char *s, size_t len)
{
   size_t _len;
   char *last_slash;
   char tmp_buf[NAME_MAX_LENGTH];
   const char *dir_start;
   if (!path_data || !*path_data->content_path)
      return 0;
   if (!(last_slash = find_last_slash(path_data->content_path)))
      return 0;
   _len = last_slash + 1 - path_data->content_path;
   if (!((_len > 1) && (_len < PATH_MAX_LENGTH)))
      return 0;
   /* The historical implementation copied the whole directory
    * portion of content_path into tmp_buf and then took its
    * basename.  But content_path is sized PATH_MAX_LENGTH (2048)
    * and tmp_buf only NAME_MAX_LENGTH (256), so a directory
    * portion longer than 255 chars (reachable on systems with
    * deep folder hierarchies) caused strlcpy to write up to
    * _len-1 bytes into the 256-byte tmp_buf -- a stack overflow.
    *
    * Since the goal is the *basename* of the directory (i.e.
    * the segment between the second-to-last and last slashes),
    * skip the prefix copy: we only need the tail.  Anchor the
    * copy at the latest position that still lets the segment
    * fit in tmp_buf, then take its basename. */
   dir_start = path_data->content_path;
   if (_len > sizeof(tmp_buf))
      dir_start = last_slash + 1 - sizeof(tmp_buf);
   strlcpy(tmp_buf, dir_start,
         (last_slash - dir_start + 1) * sizeof(char));
   return strlcpy(s, path_basename_nocompression(tmp_buf), len);
}

/* Gets the common savestate thumbnail path. */
void gfx_savestate_thumbnail_get_path(
      char *s, size_t len,
      const char *state_name,
      int state_slot)
{
   size_t _len;
   playlist_t *playlist = playlist_get_cached();
   /* Holds the path state_name points into, when it is built here */
   void *owned          = NULL;

   if (!s || !len)
      return;

   s[0] = '\0';

   if (!state_name || !*state_name)
      return;

#ifdef HAVE_MENU
   if (playlist)
   {
      struct menu_state *menu_st         = menu_state_get_ptr();
      runloop_state_t *runloop_st        = runloop_state_get_ptr();
      const struct playlist_entry *entry = NULL;

      if (menu_st && menu_st->driver_data && !(runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING))
         playlist_get_index(playlist, menu_st->driver_data->rpl_entry_selection_ptr, &entry);

      if (entry && *entry->path)
      {
         size_t _len;
         /* Four path buffers in one heap block: even at the console
          * PATH_MAX_LENGTH of 512 the set is a whole 2 KiB stack
          * budget, and this runs from the menu task path.  On
          * allocation failure the thumbnail path is simply not
          * resolved -- the caller already treats an empty result as
          * "no savestate thumbnail". */
         struct gfx_ss_thumb_paths
         {
            char new_path[PATH_MAX_LENGTH];
            char entry_basename[PATH_MAX_LENGTH];
            char old_savefile_dir[PATH_MAX_LENGTH];
            char old_savestate_dir[PATH_MAX_LENGTH];
         } *pb = (struct gfx_ss_thumb_paths*)calloc(1, sizeof(*pb));
         char *new_path          = pb ? pb->new_path : NULL;
         char *entry_basename    = pb ? pb->entry_basename : NULL;
         char *old_savefile_dir  = pb ? pb->old_savefile_dir : NULL;
         char *old_savestate_dir = pb ? pb->old_savestate_dir : NULL;

         if (!pb)
            return;

         strlcpy(old_savefile_dir, runloop_st->savefile_dir, PATH_MAX_LENGTH);
         strlcpy(old_savestate_dir, runloop_st->savestate_dir, PATH_MAX_LENGTH);

         strlcpy(new_path, entry->path, PATH_MAX_LENGTH);
         path_remove_extension(new_path);

         _len = strlcpy(entry_basename, path_basename(new_path), PATH_MAX_LENGTH);
         _len = strlcpy_lit(entry_basename + _len, ".state", PATH_MAX_LENGTH - _len);

         /* Set temporary save redirection paths */
         runloop_path_set_redirect(config_get_ptr(), old_savefile_dir, old_savestate_dir);

         fill_pathname_join_special(new_path,
               runloop_st->savestate_dir, entry_basename, PATH_MAX_LENGTH);

         /* Restore current save redirection paths */
         dir_set(RARCH_DIR_CURRENT_SAVEFILE, old_savefile_dir);
         dir_set(RARCH_DIR_CURRENT_SAVESTATE, old_savestate_dir);

         state_name = new_path;
         owned      = pb;
      }
   }
#endif /* HAVE_MENU */

   _len = strlcpy(s, state_name, len);

   /* The historical implementation accumulated _len from
    * strlcpy / snprintf returns and used `len - _len` as the
    * size for subsequent calls.  That pattern is NOT
    * self-bounding: strlcpy returns strlen(@src), and snprintf
    * returns the would-be length on truncation, so on any
    * truncating call _len overshoots @len, the next len-_len
    * subtraction underflows size_t to ~SIZE_MAX, and the
    * subsequent strlcpy treats the destination as essentially
    * infinite.  Reachable when state_name approaches PATH_MAX_LENGTH
    * (e.g. content loaded from a deep directory tree, since
    * runloop_st->name.savestate is sized PATH_MAX_LENGTH) and
    * the slot suffix or extension push the total past @len.
    *
    * Clamp _len after each truncation so the chain stays inside
    * the buffer instead of running off the end. */
   if (_len >= len)
      _len = len - 1;

   if (state_slot > 0)
   {
      int n = snprintf(s + _len, len - _len, "%d", state_slot);
      if (n < 0)
      {
         free(owned);
         return;
      }
      _len += (size_t)n;
      if (_len >= len)
         _len = len - 1;
   }
   else if (state_slot < 0)
   {
      _len = fill_pathname_join_delim(s, state_name, "auto", '.', len);
      if (_len >= len)
         _len = len - 1;
   }

   strlcpy(s + _len, FILE_PATH_PNG_EXTENSION, len - _len);
   free(owned);
}
