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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <features/features_cpu.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <lists/file_list.h>
#include <streams/file_stream.h>
#include <formats/image.h>

#include "gfx_display.h"
#include "gfx_animation.h"
#include <formats/data_transfer.h>

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

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../configuration.h"
#include "../msg_hash.h"
#include "../paths.h"
#include "../file_path_special.h"

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "../tasks/tasks_internal.h"

#define DEFAULT_GFX_THUMBNAIL_STREAM_DELAY  16.66667f * 3
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

/* Utility structure, sent as userdata when pushing
 * an image load */
typedef struct
{
   uint64_t list_id;
   gfx_thumbnail_t *thumbnail;
   char path[PATH_MAX_LENGTH];
} gfx_thumbnail_tag_t;

static gfx_thumbnail_state_t gfx_thumb_st = {0}; /* uint64_t alignment */

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
      if (p_gfx_thumb->fade_duration > 0.0f)
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
 * In HAVE_THREADS builds the DECODE itself happens on a shared worker
 * thread (one per process, created lazily): each active animation owns
 * a job; the worker pops jobs from a FIFO, decodes one displayed frame
 * into the job's own upload-ready pixel buffer (including the ARGB
 * swizzle when the video driver needs it), and marks the job READY.
 * While a job is QUEUED or RUNNING, its stream belongs exclusively to
 * the worker; the main thread only inspects job state under the lock,
 * uploads READY frames, and re-enqueues. gfx_thumbnail_anim_close
 * unlinks QUEUED jobs and waits out RUNNING ones, so teardown can
 * never free a stream under the worker. Without HAVE_THREADS (or if
 * worker creation fails) the original synchronous, budget-limited
 * decode path is used instead. */

/* Total decoded-animation frame budget per vsync, shared across all
 * animated thumbnails; keeps e.g. a grid of animations from stalling
 * the menu (they degrade to a lower animation rate instead). */
#define GFX_THUMB_ANIM_BUDGET_US    8000
/* Refuse to animate anything larger than this many canvas pixels or
 * a file larger than this (the file buffer is held for the lifetime
 * of the animation). Decode COST does not need a tight cap here: the
 * shared per-vsync budget above already makes large animations lower
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
/* Frame-duration handling: <= 0 is undefined by the container spec
 * (browsers substitute 100 ms); very small durations are floored so
 * a hostile file cannot request thousands of decodes per second. */
#define GFX_THUMB_ANIM_DUR_DEFAULT  100
#define GFX_THUMB_ANIM_DUR_MIN      16

/* Preview audio: decode the animated thumbnail's audio track and loop
 * it through the audio mixer while the animation is shown. */
#if (defined(HAVE_RWEBM) || defined(HAVE_RMP4)) && \
      defined(HAVE_AUDIOMIXER) && \
      (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS) || defined(HAVE_RAAC))
#define GFX_THUMB_PREVIEW_AUDIO 1
/* Cap decoded PCM (memory bound: 90 s stereo 48 kHz s16 = ~17 MB). */
#define GFX_THUMB_PREVIEW_AUDIO_MAX_MS 90000
#define GFX_THUMB_PREVIEW_AUDIO_NAME   "__gfx_thumb_preview"
#endif

/* Memory an active animation pins: the file buffer for its lifetime,
 * two XRGB canvases, and decoder working state - for MP4/H.264 a
 * decoded-picture buffer of reference and reorder pictures, bounded
 * generously at 24 I420 frames. px may be 0 before the stream has
 * been opened (file buffer only). */
static uint64_t gfx_thumb_anim_mem_need(uint64_t file_len, uint64_t px)
{
   /* px * 4 * 3: the stream's decode canvas plus the two ping-pong
    * upload buffers of the threaded animation path. */
   return file_len + px * 4 * 3 + (px * 3 / 2) * 24 + (1 << 20);
}

/* Admission: scale with the heap when the platform reports free
 * memory, keep the static caps when it cannot (they return 0). */
static bool gfx_thumb_anim_mem_ok(uint64_t file_len, uint64_t px)
{
   uint64_t free_mem = frontend_driver_get_free_memory();
   if (free_mem)
      return (file_len <= GFX_THUMB_ANIM_ABS_MAX_FILE)
          && (gfx_thumb_anim_mem_need(file_len, px) <= free_mem / 4);
   return (file_len <= GFX_THUMB_ANIM_MAX_FILE)
       && (px == 0 || px <= GFX_THUMB_ANIM_MAX_PIXELS);
}

enum gfx_thumb_anim_job_status
{
   GFX_THUMB_JOB_QUEUED = 0,   /* linked in the FIFO, not started      */
   GFX_THUMB_JOB_RUNNING,      /* worker is decoding into job->frame   */
   GFX_THUMB_JOB_READY,        /* frame decoded, awaiting upload       */
   GFX_THUMB_JOB_FINISHED,     /* loops exhausted or stream error      */
   GFX_THUMB_JOB_IDLE          /* not owned by the worker, no pending
                                  frame (fresh, or consumed).  QUEUED
                                  stays 0 so the calloc'd preview-audio
                                  job, which is enqueued immediately,
                                  keeps its meaning. */
};

typedef struct gfx_thumb_anim_job
{
   struct gfx_thumb_anim_job *next;  /* FIFO link (owned by the queue) */
   void     *stream;                 /* borrowed from the thumbnail    */
   uint32_t *frame;                  /* job-owned upload-ready pixels  */
   unsigned  width, height;
   int       duration_ms;            /* of the READY frame             */
   int32_t   loops_left;             /* worker-maintained, -1 infinite */
   int       status;                 /* enum gfx_thumb_anim_job_status */
   uint8_t   type;                   /* enum image_type_enum           */
   bool      use_rgba;               /* output word format             */
   /* Preview-audio jobs (is_audio): decode src/src_len (the thumbnail's
    * file buffer, borrowed) into a job-owned in-memory WAV. */
   bool      is_audio;
   const void *src;
   size_t    src_len;
   void     *wav;
   size_t    wav_size;
} gfx_thumb_anim_job_t;

#ifdef HAVE_THREADS
/* ---- Animated-thumbnail decode worker ---- */

static slock_t               *gfx_thumb_worker_lock   = NULL;
static scond_t               *gfx_thumb_worker_wake   = NULL; /* worker */
static scond_t               *gfx_thumb_worker_done   = NULL; /* main   */
static sthread_t             *gfx_thumb_worker_thread = NULL;
static gfx_thumb_anim_job_t  *gfx_thumb_worker_head   = NULL;
static gfx_thumb_anim_job_t  *gfx_thumb_worker_tail   = NULL;
static bool                   gfx_thumb_worker_die    = false;

/* Decode one displayed frame (handling end-of-pass loop/rewind) and
 * convert it into job->frame in its final upload format. Returns false
 * when the animation is over. Runs on the worker thread; the job is
 * RUNNING, so it owns the stream exclusively. */
#if defined(GFX_THUMB_PREVIEW_AUDIO)
/* Decode the preview audio of the container type the job carries. */
static bool gfx_thumb_preview_audio_decode(uint8_t type,
      const void *src, size_t src_len, void **wav, size_t *wav_size)
{
#ifdef HAVE_RWEBM
   if (type == IMAGE_TYPE_WEBM)
      return rwebm_audio_decode_wav(src, src_len,
            GFX_THUMB_PREVIEW_AUDIO_MAX_MS, wav, wav_size) ? true : false;
#endif
#ifdef HAVE_RMP4
   if (type == IMAGE_TYPE_MP4)
      return rmp4_audio_decode_wav(src, src_len,
            GFX_THUMB_PREVIEW_AUDIO_MAX_MS, wav, wav_size) ? true : false;
#endif
   return false;
}
#endif

static bool gfx_thumbnail_anim_job_step(gfx_thumb_anim_job_t *job)
{
   const uint32_t *frame;
   enum image_type_enum type = (enum image_type_enum)job->type;
   int duration_ms           = 0;
   size_t i, n;
   /* Ask the stream to emit the upload order directly: the video
    * streams bake it in their blit, which removes the per-pixel R/B
    * swizzle pass below; WEBP always emits R,G,B,A and keeps the
    * fallback conversion. */
   bool native_order         = image_transfer_anim_stream_set_argb(
         job->stream, type, job->use_rgba ? 0 : 1);

   if (!(frame = image_transfer_anim_stream_next(job->stream, type,
         &duration_ms)))
   {
      /* End of one pass: honour the container loop count */
      if (job->loops_left > 0)
         job->loops_left--;
      if (job->loops_left == 0)
         return false;
      image_transfer_anim_stream_rewind(job->stream, type);
      if (!(frame = image_transfer_anim_stream_next(job->stream, type,
            &duration_ms)))
         return false;
   }

   n = (size_t)job->width * job->height;
   if (job->use_rgba || native_order)
      /* Frame is already in the upload order (RGBA requested, or the
       * stream honoured the ARGB request); the copy just decouples the
       * upload buffer from the decoder's canvas. */
      memcpy(job->frame, frame, n * sizeof(uint32_t));
   else
   {
      /* The stream emits memory-order R,G,B,A; swizzle to ARGB words
       * here so the main thread only has to upload. */
      for (i = 0; i < n; i++)
      {
         uint32_t px   = frame[i];
         job->frame[i] = (px & 0xFF00FF00u)
               | ((px & 0xFF) << 16) | ((px >> 16) & 0xFF);
      }
   }
   job->duration_ms = duration_ms;
   return true;
}

static void gfx_thumbnail_anim_worker(void *unused)
{
   (void)unused;
   slock_lock(gfx_thumb_worker_lock);
   for (;;)
   {
      gfx_thumb_anim_job_t *job;
      bool alive;

      while (!gfx_thumb_worker_die && !gfx_thumb_worker_head)
         scond_wait(gfx_thumb_worker_wake, gfx_thumb_worker_lock);
      if (gfx_thumb_worker_die)
         break;

      job                   = gfx_thumb_worker_head;
      gfx_thumb_worker_head = job->next;
      if (!gfx_thumb_worker_head)
         gfx_thumb_worker_tail = NULL;
      job->next             = NULL;
      job->status           = GFX_THUMB_JOB_RUNNING;

      slock_unlock(gfx_thumb_worker_lock);
#if defined(GFX_THUMB_PREVIEW_AUDIO)
      if (job->is_audio)
         alive = gfx_thumb_preview_audio_decode(job->type,
               job->src, job->src_len, &job->wav, &job->wav_size);
      else
#endif
         alive = gfx_thumbnail_anim_job_step(job);
      slock_lock(gfx_thumb_worker_lock);

      job->status = alive ? GFX_THUMB_JOB_READY : GFX_THUMB_JOB_FINISHED;
      scond_broadcast(gfx_thumb_worker_done);
   }
   slock_unlock(gfx_thumb_worker_lock);
}

/* Lazily creates the worker. Returns false if thread primitives could
 * not be allocated; callers then use the synchronous path. */
static bool gfx_thumbnail_anim_worker_init(void)
{
   if (gfx_thumb_worker_thread)
      return true;
   if (!gfx_thumb_worker_lock && !(gfx_thumb_worker_lock = slock_new()))
      goto fail;
   if (!gfx_thumb_worker_wake && !(gfx_thumb_worker_wake = scond_new()))
      goto fail;
   if (!gfx_thumb_worker_done && !(gfx_thumb_worker_done = scond_new()))
      goto fail;
   gfx_thumb_worker_die = false;
   if (!(gfx_thumb_worker_thread = sthread_create(
         gfx_thumbnail_anim_worker, NULL)))
      goto fail;
   return true;
fail:
   if (gfx_thumb_worker_done)
      scond_free(gfx_thumb_worker_done);
   if (gfx_thumb_worker_wake)
      scond_free(gfx_thumb_worker_wake);
   if (gfx_thumb_worker_lock)
      slock_free(gfx_thumb_worker_lock);
   gfx_thumb_worker_done = NULL;
   gfx_thumb_worker_wake = NULL;
   gfx_thumb_worker_lock = NULL;
   return false;
}

static void gfx_thumbnail_anim_job_enqueue(gfx_thumb_anim_job_t *job)
{
   slock_lock(gfx_thumb_worker_lock);
   job->status = GFX_THUMB_JOB_QUEUED;
   job->next   = NULL;
   if (gfx_thumb_worker_tail)
      gfx_thumb_worker_tail->next = job;
   else
      gfx_thumb_worker_head       = job;
   gfx_thumb_worker_tail          = job;
   scond_signal(gfx_thumb_worker_wake);
   slock_unlock(gfx_thumb_worker_lock);
}

/* Detach a job from the worker: unlink it if still queued, wait out the
 * decode if running. On return the worker holds no reference to it. */
static void gfx_thumbnail_anim_job_release(gfx_thumb_anim_job_t *job)
{
   if (!gfx_thumb_worker_lock)
      return;
   slock_lock(gfx_thumb_worker_lock);
   if (job->status == GFX_THUMB_JOB_QUEUED)
   {
      gfx_thumb_anim_job_t **pp = &gfx_thumb_worker_head;
      while (*pp && *pp != job)
         pp = &(*pp)->next;
      if (*pp)
      {
         *pp = job->next;
         if (gfx_thumb_worker_tail == job)
         {
            gfx_thumb_anim_job_t *t = gfx_thumb_worker_head;
            while (t && t->next)
               t = t->next;
            gfx_thumb_worker_tail = t;
         }
      }
   }
   while (job->status == GFX_THUMB_JOB_RUNNING)
      scond_wait(gfx_thumb_worker_done, gfx_thumb_worker_lock);
   slock_unlock(gfx_thumb_worker_lock);
}

void gfx_thumbnail_anim_worker_deinit(void)
{
   if (!gfx_thumb_worker_thread)
      return;
   slock_lock(gfx_thumb_worker_lock);
   gfx_thumb_worker_die  = true;
   /* Orphan anything still queued: the jobs stay owned by their
    * thumbnails (freed by gfx_thumbnail_reset); they simply never
    * advance. Normal shutdown order resets thumbnails first, so the
    * queue is expected to be empty here. */
   gfx_thumb_worker_head = NULL;
   gfx_thumb_worker_tail = NULL;
   scond_signal(gfx_thumb_worker_wake);
   slock_unlock(gfx_thumb_worker_lock);
   sthread_join(gfx_thumb_worker_thread);
   gfx_thumb_worker_thread = NULL;
   scond_free(gfx_thumb_worker_done);
   scond_free(gfx_thumb_worker_wake);
   slock_free(gfx_thumb_worker_lock);
   gfx_thumb_worker_done = NULL;
   gfx_thumb_worker_wake = NULL;
   gfx_thumb_worker_lock = NULL;
}
#else
void gfx_thumbnail_anim_worker_deinit(void) { }
#endif

#if defined(GFX_THUMB_PREVIEW_AUDIO)
/* The thumbnail currently holding the (single) preview-audio mixer
 * stream, and the granted slot. Main thread only. */
static const gfx_thumbnail_t *gfx_thumb_audio_owner = NULL;
static int                    gfx_thumb_audio_slot  = -1;

static void gfx_thumbnail_preview_audio_stop(const gfx_thumbnail_t *owner)
{
   if (gfx_thumb_audio_owner != owner)
      return;
   if (gfx_thumb_audio_slot >= 0)
   {
      /* Only touch the slot if it still holds our stream (another
       * subsystem may have replaced it). */
      const char *name = audio_driver_mixer_get_stream_name(
            (unsigned)gfx_thumb_audio_slot);
      if (name && string_is_equal(name, GFX_THUMB_PREVIEW_AUDIO_NAME))
         audio_driver_mixer_remove_stream((unsigned)gfx_thumb_audio_slot);
   }
   gfx_thumb_audio_owner = NULL;
   gfx_thumb_audio_slot  = -1;
}

static void gfx_thumbnail_preview_audio_start(gfx_thumbnail_t *thumbnail,
      void *wav, size_t wav_size)
{
   audio_mixer_stream_params_t params;
   int out_slot = -1;

   /* One preview stream at a time */
   gfx_thumbnail_preview_audio_stop(gfx_thumb_audio_owner);

   params.buf                 = wav;
   params.bufsize             = wav_size;
   params.basename            = strdup(GFX_THUMB_PREVIEW_AUDIO_NAME);
   params.cb                  = NULL;
   /* Donate the WAV blob: add_stream's WAV arm reads the borrowed
    * bytes during the PCM conversion and releases the owner right
    * after (or on any failure path) - the defensive input copy,
    * which briefly tripled this allocation next to both PCM
    * buffers, is gone.  Ownership transfers on the call in every
    * outcome. */
   params.buf_owner           = wav;
   params.buf_owner_free      = free;
   params.out_slot            = &out_slot;
   params.slot_selection_idx  = 0;
   params.volume              = 1.0f;
   params.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
   params.stream_type         = AUDIO_STREAM_TYPE_SYSTEM;
   params.type                = AUDIO_MIXER_TYPE_WAV;
   params.state               = AUDIO_STREAM_STATE_PLAYING_LOOPED;

   if (!audio_driver_mixer_add_stream(&params))
   {
      free(params.basename);
      return;
   }
   /* add_stream reports the granted slot directly; the sentinel name
    * remains only as the staleness guard at stop time, where the slot
    * may since have been handed to another subsystem. */
   gfx_thumb_audio_slot  = out_slot;
   gfx_thumb_audio_owner = thumbnail;
}
#endif /* GFX_THUMB_PREVIEW_AUDIO */

static void gfx_thumbnail_anim_close(gfx_thumbnail_t *thumbnail)
{
#ifdef HAVE_THREADS
   if (thumbnail->anim_job)
   {
      gfx_thumb_anim_job_t *job = (gfx_thumb_anim_job_t*)thumbnail->anim_job;
      gfx_thumbnail_anim_job_release(job);
      free(job->frame);
      free(job);
      thumbnail->anim_job = NULL;
   }
   if (thumbnail->anim_job2)
   {
      gfx_thumb_anim_job_t *job = (gfx_thumb_anim_job_t*)thumbnail->anim_job2;
      gfx_thumbnail_anim_job_release(job);
      free(job->frame);
      free(job);
      thumbnail->anim_job2 = NULL;
   }
   thumbnail->anim_job_upload = 0;
#endif
#if defined(GFX_THUMB_PREVIEW_AUDIO)
   if (thumbnail->anim_audio_job)
   {
      gfx_thumb_anim_job_t *job =
            (gfx_thumb_anim_job_t*)thumbnail->anim_audio_job;
#ifdef HAVE_THREADS
      gfx_thumbnail_anim_job_release(job);
#endif
      free(job->wav);
      free(job);
      thumbnail->anim_audio_job = NULL;
   }
   gfx_thumbnail_preview_audio_stop(thumbnail);
#endif
   if (thumbnail->anim)
      image_transfer_anim_stream_free(thumbnail->anim,
            (enum image_type_enum)thumbnail->anim_type);
   /* Stream first, buffer second: the stream borrows the buffer.
    * An adopted animation's buffer lives inside the nbio handle
    * (possibly as a file mapping) and is released with it; only the
    * open-by-path fallback malloc's anim_buf. */
   if (thumbnail->anim_dt)
      /* Deselected before the adopted read finished: the transfer
       * cancels the in-flight read before releasing the handle - the
       * rest of the file is never read. */
      data_transfer_free(thumbnail->anim_dt);
   else if (thumbnail->anim_buf)
      free(thumbnail->anim_buf);
   thumbnail->anim            = NULL;
   thumbnail->anim_buf        = NULL;
   thumbnail->anim_dt         = NULL;
   thumbnail->anim_buf_len    = 0;
   thumbnail->anim_next_us    = 0;
   thumbnail->anim_loops_left = 0;
   thumbnail->anim_type       = 0;
   thumbnail->anim_read_pending = 0;
   thumbnail->flags          &= ~GFX_THUMB_FLAG_ANIM_ACTIVE;
}

/* Install an open animation stream on the thumbnail, applying the
 * frame-count and memory admission checks.  'buf' is the container
 * bytes the stream borrows: when 'xfer' is non-NULL it owns 'buf'
 * (released with data_transfer_free), otherwise 'buf' is a malloc'd
 * block this thumbnail takes over (released with free).  Ownership of
 * stream/buf/xfer transfers in every outcome; on rejection they
 * are released and the static thumbnail stays.  Returns true when the
 * animation was installed. */
#if defined(GFX_THUMB_PREVIEW_AUDIO)
/* Preview audio (opt-in): decode the file's audio track to PCM and
 * loop it through the mixer while the animation is shown. WebM and
 * MP4 only (animated WebP has no audio). With threads the decode
 * runs on the shared worker; without, it runs here once (a one-shot
 * cost when the preview opens).  Called from anim_install for a
 * fully-resident buffer, or deferred to the moment the adopted read
 * completes - the decode consumes the whole buffer, so it must never
 * see a partially-read one. */
static void gfx_thumbnail_anim_audio_begin(gfx_thumbnail_t *thumbnail)
{
   enum image_type_enum type = (enum image_type_enum)thumbnail->anim_type;
   if (     (   (type == IMAGE_TYPE_WEBM)
             || (type == IMAGE_TYPE_MP4))
         && config_get_ptr()->bools.menu_thumbnail_preview_audio)
   {
      gfx_thumb_anim_job_t *job =
            (gfx_thumb_anim_job_t*)calloc(1, sizeof(*job));
      if (job)
      {
         job->is_audio = true;
         job->type     = (uint8_t)type;
         job->src      = thumbnail->anim_buf;
         job->src_len  = thumbnail->anim_buf_len;
#ifdef HAVE_THREADS
         if (gfx_thumbnail_anim_worker_init())
         {
            thumbnail->anim_audio_job = job;
            gfx_thumbnail_anim_job_enqueue(job);
         }
         else
#endif
         {
            if (gfx_thumb_preview_audio_decode(job->type,
                  job->src, job->src_len, &job->wav, &job->wav_size))
               job->status = GFX_THUMB_JOB_READY;
            else
               job->status = GFX_THUMB_JOB_FINISHED;
            thumbnail->anim_audio_job = job;
         }
      }
   }
}
#endif

static bool gfx_thumbnail_anim_install(gfx_thumbnail_t *thumbnail,
      void *stream, enum image_type_enum type,
      void *buf, size_t len, struct data_transfer *xfer)
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

   if (   (num_frames < 2)
       || (anim_w < 1)
       || (anim_h < 1)
       || !gfx_thumb_anim_mem_ok((uint64_t)len,
             (uint64_t)anim_w * anim_h))
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
      goto fail;
   }

   thumbnail->anim            = stream;
   thumbnail->anim_buf        = buf;
   thumbnail->anim_buf_len    = len;
   thumbnail->anim_type       = (uint8_t)type;
   thumbnail->anim_loops_left = (loop_count == 0) ? -1 : loop_count;
   thumbnail->anim_next_us    = 0;   /* first advance establishes timing */
   thumbnail->flags          |= GFX_THUMB_FLAG_ANIM_ACTIVE;

   /* The still's task can complete - and adoption run - while the
    * file's read is still in flight (the still needs only a prefix).
    * Until the read completes, hold the animation and the audio
    * preview at the static frame: the decode stream and the audio
    * decoder both need the full buffer.  gfx_thumbnail_animate pumps
    * the handle to completion; a fatter chunk shortens the catch-up. */
   thumbnail->anim_read_pending =
         (thumbnail->anim_dt
          && !data_transfer_complete(thumbnail->anim_dt)) ? 1 : 0;

#if defined(GFX_THUMB_PREVIEW_AUDIO)
   if (!thumbnail->anim_read_pending)
      gfx_thumbnail_anim_audio_begin(thumbnail);
#endif
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

static void gfx_thumbnail_anim_open(gfx_thumbnail_t *thumbnail,
      const char *path)
{
   enum image_type_enum type;
   int64_t len              = 0;
   void *buf                = NULL;
   void *stream             = NULL;

   gfx_thumbnail_anim_close(thumbnail);

   if (string_is_empty(path))
      return;

   /* Cheap gate: only container types with an animation decoder */
   type = image_texture_get_type(path);
   if (   (type != IMAGE_TYPE_WEBP)
       && (type != IMAGE_TYPE_WEBM)
       && (type != IMAGE_TYPE_MP4))
      return;

   /* Gate on the file's size before reading it: rejecting after the
    * read would itself be the allocation spike the cap exists to
    * prevent. */
   {
      int64_t fsz = path_get_size(path);
      if ((fsz <= 0) || !gfx_thumb_anim_mem_ok((uint64_t)fsz, 0))
         return;
   }
   if (!filestream_read_file(path, &buf, &len))
      return;
   if (len <= 0)
   {
      free(buf);
      return;
   }

   if (!(stream = image_transfer_anim_stream_new(buf, (size_t)len, type)))
   {
      /* still image or malformed: keep static thumbnail */
      free(buf);
      return;
   }

   gfx_thumbnail_anim_install(thumbnail, stream, type,
         buf, (size_t)len, NULL);
}

/* Uploads one final-format animation frame as the thumbnail's texture.
 * 'pixels' must already be in the format the video driver expects
 * ('use_rgba' describes it). Runs on the main thread. */
static void gfx_thumbnail_anim_upload(gfx_thumbnail_t *thumbnail,
      const uint32_t *pixels, unsigned width, unsigned height,
      bool use_rgba)
{
   struct texture_image img;
   uintptr_t new_texture = 0;

   img.width         = width;
   img.height        = height;
   img.supports_rgba = use_rgba;
   img.pixels        = (uint32_t*)pixels;
   img.compressed    = NULL; /* raw frame, not a loaded compressed texture */
   img.pix10         = false;

   /* Animated thumbnails re-upload every frame; always use
    * plain linear filtering here to avoid per-frame mip-map
    * generation regardless of the menu_texture_mipmapping
    * setting. */
   if (video_driver_texture_load(&img,
         TEXTURE_FILTER_LINEAR, &new_texture) && new_texture)
   {
      if (thumbnail->texture)
         video_driver_texture_unload(&thumbnail->texture);
      thumbnail->texture = new_texture;
      thumbnail->width   = width;
      thumbnail->height  = height;
   }
}

/* Schedules the next animation frame. Accumulates from the previous
 * due time to keep long-term pacing, but never falls so far behind
 * that frames are decoded continuously to catch up. */
static void gfx_thumbnail_anim_schedule(gfx_thumbnail_t *thumbnail,
      int duration_ms, int64_t now)
{
   if (duration_ms <= 0)
      duration_ms = GFX_THUMB_ANIM_DUR_DEFAULT;
   else if (duration_ms < GFX_THUMB_ANIM_DUR_MIN)
      duration_ms = GFX_THUMB_ANIM_DUR_MIN;

   if (thumbnail->anim_next_us == 0)
      thumbnail->anim_next_us = now + (int64_t)duration_ms * 1000;
   else
   {
      thumbnail->anim_next_us += (int64_t)duration_ms * 1000;
      if (thumbnail->anim_next_us < now)
         thumbnail->anim_next_us = now + (int64_t)duration_ms * 1000;
   }
}

/* Advances an animated thumbnail by (at most) one frame once its
 * display duration has elapsed. With HAVE_THREADS the decode runs on
 * the shared worker and this function only uploads finished frames
 * (late frames simply appear a vsync or two later); without it the
 * frame is decoded inline under the shared per-vsync budget.
 * Runs on the main thread; called from the per-frame stream
 * request/process functions for on-screen entries. */
/* Advances an animated thumbnail by at most one frame if its display
 * duration has elapsed and the shared per-vsync decode budget allows.
 *
 * MUST be called on the main thread, once per frame, for every visible
 * thumbnail (menu drivers do this from their main-thread iterate step).
 * For a still image the ANIM_ACTIVE flag is clear, so this returns
 * immediately after a single flag test - non-animated thumbnails, and
 * every image type without an animation decoder, pay nothing beyond
 * that. */
void gfx_thumbnail_animate(gfx_thumbnail_t *thumbnail)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   const uint32_t *frame              = NULL;
   int64_t now;
   int64_t decode_start;
   int duration_ms                    = 0;
   bool sync_use_rgba                 = false;
   bool sync_native_order             = false;
   enum image_type_enum type;

   if (   !thumbnail
       || !(thumbnail->flags & GFX_THUMB_FLAG_ANIM_ACTIVE)
       || !thumbnail->anim
       || (GFX_THUMB_STATUS_LOAD(&thumbnail->status) !=
             GFX_THUMBNAIL_STATUS_AVAILABLE))
      return;

   now  = cpu_features_get_time_usec();
   type = (enum image_type_enum)thumbnail->anim_type;

   if (thumbnail->anim_read_pending)
   {
      /* The adopted file is still being read; finish it here, a small
       * time budget per vsync, holding the static frame meanwhile.
       * Animation jobs and the audio decode only start on a complete
       * buffer, so the decoders never see the partial-read wall.
       *
       * The 2 ms wall-clock cap is the frame-time guard; the chunk is
       * only the granularity at which that cap is checked.  Filling
       * the buffer is disk-read-bound (a warm 66 MB file iterates in
       * ~44 ms of pure I/O), so a 256 KB granularity spread the read
       * over dozens of vsyncs even though the cap was rarely reached
       * - the prefix reader delivered the still frame in a few MB,
       * then the animation waited seconds for this dribble to finish.
       * A 2 MB chunk lets each vsync's 2 ms window move far more than
       * the old 256 KB, so the read completes in a handful of frames
       * instead of dozens and the animation starts promptly after the
       * still.  The cap is only re-checked between iterates, so the
       * chunk is also bounded by frame safety: a single 2 MB iterate
       * measures ~1.2 ms on a warm file, comfortably inside one 60 Hz
       * frame, whereas 4 MB approached a whole 2 ms in one go. */
      do
      {
         data_transfer_iterate(thumbnail->anim_dt, 2 * 1024 * 1024);
      } while (!data_transfer_complete(thumbnail->anim_dt)
            && !data_transfer_failed(thumbnail->anim_dt)
            && cpu_features_get_time_usec() - now < 2000);
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
       * still opened it (the still's task completed - and its
       * completion callback died - before the read did); lift it to
       * the full length, or the animation would treat the wall as the
       * end of the file and loop there forever. */
      image_transfer_anim_stream_set_avail(thumbnail->anim, type,
            thumbnail->anim_buf_len);
      /* ...and finish the WEBM timestamp pre-scan the walled open
       * truncated, so pacing matches a fully-read open exactly. */
      image_transfer_anim_stream_complete_scan(thumbnail->anim, type,
            thumbnail->anim_buf, thumbnail->anim_buf_len);
#if defined(GFX_THUMB_PREVIEW_AUDIO)
      gfx_thumbnail_anim_audio_begin(thumbnail);
#endif
   }

#if defined(GFX_THUMB_PREVIEW_AUDIO)
   /* Hand a finished preview-audio decode to the mixer (independent of
    * the video frame cadence). */
   if (thumbnail->anim_audio_job)
   {
      gfx_thumb_anim_job_t *ajob =
            (gfx_thumb_anim_job_t*)thumbnail->anim_audio_job;
      int astatus;
#ifdef HAVE_THREADS
      if (gfx_thumb_worker_lock)
      {
         slock_lock(gfx_thumb_worker_lock);
         astatus = ajob->status;
         slock_unlock(gfx_thumb_worker_lock);
      }
      else
#endif
         astatus = ajob->status;
      if (astatus == GFX_THUMB_JOB_READY)
      {
         gfx_thumbnail_preview_audio_start(thumbnail,
               ajob->wav, ajob->wav_size);
         ajob->wav = NULL;         /* donated: the mixer releases it
                                      in every outcome */
         free(ajob);
         thumbnail->anim_audio_job = NULL;
      }
      else if (astatus == GFX_THUMB_JOB_FINISHED)
      {
         free(ajob->wav);
         free(ajob);
         thumbnail->anim_audio_job = NULL;
      }
   }
#endif

#ifdef HAVE_THREADS
   /* Threaded path: decode happens on the shared worker; this thread
    * only inspects job state, uploads READY frames, and re-enqueues.
    * A frame that is not ready when due is simply uploaded on a later
    * vsync - the menu never blocks on the decoder.
    *
    * Two jobs ping-pong over the stream so decoding runs one displayed
    * frame ahead of the display clock: while the due frame waits in
    * one job, the other is already decoding its successor.  A shown
    * frame preceded by a burst of hidden frames (a VP9 alt-ref chain,
    * an H.264 reorder run) then has a whole extra display interval to
    * decode before it is late, where the single job started it only
    * after the previous upload.  A job is enqueued only while its
    * sibling is not QUEUED or RUNNING, so stream access stays strictly
    * serialised in enqueue order - the decoded frame sequence is
    * identical to the single-job scheme by construction - and the
    * loop counter threads soundly from the job that just finished
    * decoding (via thumbnail->anim_loops_left) into the next enqueue. */
   if (gfx_thumbnail_anim_worker_init())
   {
      gfx_thumb_anim_job_t *ju = (gfx_thumb_anim_job_t*)
            (thumbnail->anim_job_upload ? thumbnail->anim_job2
                                        : thumbnail->anim_job);
      gfx_thumb_anim_job_t *jo = (gfx_thumb_anim_job_t*)
            (thumbnail->anim_job_upload ? thumbnail->anim_job
                                        : thumbnail->anim_job2);
      int su, so;

      if (!ju || !jo)
      {
         unsigned anim_w = 0, anim_h = 0;
         int num_frames = 0, loop_count = 0;
         gfx_thumb_anim_job_t *j0 = NULL;
         gfx_thumb_anim_job_t *j1 = NULL;

         image_transfer_anim_stream_get_info(thumbnail->anim, type,
               &anim_w, &anim_h, &num_frames, &loop_count);
         j0 = (gfx_thumb_anim_job_t*)calloc(1, sizeof(*j0));
         j1 = (gfx_thumb_anim_job_t*)calloc(1, sizeof(*j1));
         if (j0)
            j0->frame = (uint32_t*)malloc(
                  (size_t)anim_w * anim_h * sizeof(uint32_t));
         if (j1)
            j1->frame = (uint32_t*)malloc(
                  (size_t)anim_w * anim_h * sizeof(uint32_t));
         if (!j0 || !j1 || !j0->frame || !j1->frame)
         {
            /* Retry on a later vsync; the pair is all or nothing. */
            if (j0)
               free(j0->frame);
            if (j1)
               free(j1->frame);
            free(j0);
            free(j1);
            return;
         }
         j0->stream     = thumbnail->anim;
         j1->stream     = thumbnail->anim;
         j0->type       = thumbnail->anim_type;
         j1->type       = thumbnail->anim_type;
         j0->width      = anim_w;
         j1->width      = anim_w;
         j0->height     = anim_h;
         j1->height     = anim_h;
         j0->loops_left = thumbnail->anim_loops_left;
         j0->use_rgba   =
               (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
                     ? true : false;
         j1->status     = GFX_THUMB_JOB_IDLE;
         thumbnail->anim_job        = j0;
         thumbnail->anim_job2       = j1;
         thumbnail->anim_job_upload = 0;
         gfx_thumbnail_anim_job_enqueue(j0);
         return;
      }

      slock_lock(gfx_thumb_worker_lock);
      su = ju->status;
      so = jo->status;
      slock_unlock(gfx_thumb_worker_lock);

      /* Decode-ahead: the due-side job holds its frame, its sibling is
       * consumed - start the sibling on the following frame now, ahead
       * of the display clock.  ju's decode is complete, so its
       * loops_left is the current decode-side value to thread on. */
      if (su == GFX_THUMB_JOB_READY && so == GFX_THUMB_JOB_IDLE)
      {
         thumbnail->anim_loops_left = ju->loops_left;
         jo->loops_left             = ju->loops_left;
         jo->use_rgba               =
               (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
                     ? true : false;
         gfx_thumbnail_anim_job_enqueue(jo);
      }

      if ((thumbnail->anim_next_us != 0) && (now < thumbnail->anim_next_us))
         return;

      if (su == GFX_THUMB_JOB_FINISHED)
      {
         /* Decode exhausted the final loop (or errored): the last
          * uploaded frame's texture stays, release the decoder and
          * file buffer.  The sibling holds only an already-shown
          * frame at this point, so nothing is dropped. */
         gfx_thumbnail_anim_close(thumbnail);
         return;
      }
      if (su != GFX_THUMB_JOB_READY)
         return;   /* still decoding; try again next vsync */

      /* READY and not queued: the worker holds no reference, so the
       * frame buffer can be read without the lock. */
      gfx_thumbnail_anim_upload(thumbnail, ju->frame,
            ju->width, ju->height, ju->use_rgba);
      gfx_thumbnail_anim_schedule(thumbnail, ju->duration_ms, now);

      slock_lock(gfx_thumb_worker_lock);
      ju->status = GFX_THUMB_JOB_IDLE;   /* consumed */
      so         = jo->status;
      slock_unlock(gfx_thumb_worker_lock);
      thumbnail->anim_job_upload ^= 1;

      /* Keep the worker fed: if the sibling already banked the next
       * frame, the just-consumed job can start on the one after it
       * immediately (sibling's decode is complete, so its loops_left
       * is current).  If the sibling is still QUEUED/RUNNING, the
       * READY branch above banks this job on a later poll. */
      if (so == GFX_THUMB_JOB_READY)
      {
         thumbnail->anim_loops_left = jo->loops_left;
         ju->loops_left             = jo->loops_left;
         ju->use_rgba               =
               (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
                     ? true : false;
         gfx_thumbnail_anim_job_enqueue(ju);
      }
      return;
   }
#endif

   /* Synchronous path (no worker thread): nothing to do until the
    * next frame is due.  The threaded path above keeps polling before
    * the due time so it can bank the following frame; here the decode
    * happens in-line at upload time, so an early poll has no work. */
   if ((thumbnail->anim_next_us != 0) && (now < thumbnail->anim_next_us))
      return;

   /* Per-vsync decode budget (window resets after ~one 60 Hz frame) */
   if (now - p_gfx_thumb->anim_budget_start_us > 15000)
   {
      p_gfx_thumb->anim_budget_start_us = now;
      p_gfx_thumb->anim_budget_used_us  = 0;
   }
   if (p_gfx_thumb->anim_budget_used_us > GFX_THUMB_ANIM_BUDGET_US)
      return;   /* try again next frame; animation just runs slower */

   decode_start = now;

   /* Sample the upload format once and ask the stream to emit it
    * directly (video streams bake the order in their blit); WEBP is
    * not honoured and takes the swizzle fallback below. */
   sync_use_rgba     = (video_driver_get_disp_flags()
         & VIDEO_FLAG_USE_RGBA) ? true : false;
   sync_native_order = image_transfer_anim_stream_set_argb(
         thumbnail->anim, type, sync_use_rgba ? 0 : 1);

   if (!(frame = image_transfer_anim_stream_next(thumbnail->anim, type,
         &duration_ms)))
   {
      /* End of one pass: honour the container loop count */
      if (thumbnail->anim_loops_left > 0)
         thumbnail->anim_loops_left--;
      if (thumbnail->anim_loops_left == 0)
      {
         /* Finished: keep the last frame's texture, release the
          * decoder and file buffer */
         gfx_thumbnail_anim_close(thumbnail);
         return;
      }
      image_transfer_anim_stream_rewind(thumbnail->anim, type);
      frame = image_transfer_anim_stream_next(thumbnail->anim, type,
            &duration_ms);
      if (!frame)
      {
         gfx_thumbnail_anim_close(thumbnail);
         return;
      }
   }

   /* Upload the frame.  The stream already emitted the upload order
    * when the request above was honoured; otherwise (WEBP) it emits
    * memory-order R,G,B,A and an ARGB pipeline needs the swap into
    * the shared scratch buffer. */
   {
      static uint32_t *swap_scratch = NULL;
      static size_t swap_scratch_px = 0;
      unsigned anim_w               = 0;
      unsigned anim_h               = 0;
      int num_frames                = 0;
      int loop_count                = 0;
      const uint32_t *pixels        = frame;
      bool use_rgba                 = sync_use_rgba;

      image_transfer_anim_stream_get_info(thumbnail->anim, type,
            &anim_w, &anim_h, &num_frames, &loop_count);

      if (!use_rgba && !sync_native_order)
      {
         size_t i, n = (size_t)anim_w * anim_h;
         if (swap_scratch_px < n)
         {
            uint32_t *tmp = (uint32_t*)realloc(swap_scratch,
                  n * sizeof(uint32_t));
            if (!tmp)
               return;
            swap_scratch    = tmp;
            swap_scratch_px = n;
         }
         for (i = 0; i < n; i++)
         {
            uint32_t px      = frame[i];
            swap_scratch[i]  = (px & 0xFF00FF00u)
                  | ((px & 0xFF) << 16) | ((px >> 16) & 0xFF);
         }
         pixels = swap_scratch;
      }

      gfx_thumbnail_anim_upload(thumbnail, pixels, anim_w, anim_h,
            use_rgba);
   }

   gfx_thumbnail_anim_schedule(thumbnail, duration_ms, now);

   p_gfx_thumb->anim_budget_used_us +=
         cpu_features_get_time_usec() - decode_start;
}

static void gfx_thumbnail_handle_upload(
      retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;
   struct texture_image *img          = (struct texture_image*)task_data;
   gfx_thumbnail_tag_t *thumbnail_tag = (gfx_thumbnail_tag_t*)user_data;
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

   /* Set thumbnail 'missing' status by default
    * (saves a number of checks later)
    * > Release-store ensures prior texture reset is
    *   visible before status change */
   GFX_THUMB_STATUS_STORE(&thumbnail_tag->thumbnail->status,
         GFX_THUMBNAIL_STATUS_MISSING);

   /* If we reach this stage, thumbnail 'fade in'
    * animations should be applied (based on current
    * thumbnail status and global configuration) */
   fade_enabled = true;

   /* Check we have a valid image */
   if (!img || (img->width < 1) || (img->height < 1))
      goto end;

   /* Upload texture to GPU */
   if (!video_driver_texture_load(
            img, gfx_display_texture_filter(),
            &thumbnail_tag->thumbnail->texture))
      goto end;

   /* Cache dimensions */
   thumbnail_tag->thumbnail->width  = img->width;
   thumbnail_tag->thumbnail->height = img->height;

   /* Update thumbnail status
    * > Release-store ensures texture/width/height writes
    *   are visible to the video thread before it sees
    *   AVAILABLE via acquire-load in gfx_thumbnail_draw() */
   GFX_THUMB_STATUS_STORE(&thumbnail_tag->thumbnail->status,
         GFX_THUMBNAIL_STATUS_AVAILABLE);

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
               vstream, vtype, vbuf, vlen, vxfer);
      else
         gfx_thumbnail_anim_open(thumbnail_tag->thumbnail,
               thumbnail_tag->path);
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

/* Core interface */

/* When called, prevents the handling of any pending
 * thumbnail load requests
 * >> **MUST** be called before deleting any gfx_thumbnail_t
 *    objects passed to gfx_thumbnail_request() or
 *    gfx_thumbnail_process_stream(), otherwise
 *    heap-use-after-free errors *will* occur */
void gfx_thumbnail_cancel_pending_requests(void)
{
   gfx_thumbnail_state_t *p_gfx_thumb = &gfx_thumb_st;

   p_gfx_thumb->list_id++;
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
               gfx_thumbnail_tag_t *thumbnail_tag =
                  (gfx_thumbnail_tag_t*)malloc(sizeof(gfx_thumbnail_tag_t));

               if (!thumbnail_tag)
                  goto end;

               /* Configure user data */
               thumbnail_tag->thumbnail = thumbnail;
               thumbnail_tag->list_id   = p_gfx_thumb->list_id;
               strlcpy(thumbnail_tag->path, thumbnail_path,
                     sizeof(thumbnail_tag->path));

               /* Would like to cancel any existing image load tasks
                * here, but can't see how to do it... */
               if (task_push_image_load(
                        thumbnail_path, (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA),
                        gfx_thumbnail_upscale_threshold,
                        gfx_thumbnail_handle_upload, thumbnail_tag))
                  GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_PENDING);
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

   /* Load thumbnail */
   if (!(thumbnail_tag = (gfx_thumbnail_tag_t*)malloc(sizeof(gfx_thumbnail_tag_t))))
      return;

   /* Configure user data */
   thumbnail_tag->thumbnail = thumbnail;
   thumbnail_tag->list_id   = p_gfx_thumb->list_id;

   /* Would like to cancel any existing image load tasks
    * here, but can't see how to do it... */
   if (task_push_image_load(
         file_path, (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA),
         gfx_thumbnail_upscale_threshold,
         gfx_thumbnail_handle_upload, thumbnail_tag))
      GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_PENDING);
}

/* Resets (and free()s the current texture of) the
 * specified thumbnail */
void gfx_thumbnail_reset(gfx_thumbnail_t *thumbnail)
{
   if (!thumbnail)
      return;

   /* Release any animation state (decoder + file buffer) */
   gfx_thumbnail_anim_close(thumbnail);

   /* Unload texture */
   if (thumbnail->texture)
      video_driver_texture_unload(&thumbnail->texture);

   /* Ensure any 'fade in' animation is killed */
   if (thumbnail->flags & GFX_THUMB_FLAG_FADE_ACTIVE)
   {
      uintptr_t tag = (uintptr_t)&thumbnail->alpha;
      gfx_animation_kill_by_tag(&tag);
   }

   /* Reset all parameters */
   GFX_THUMB_STATUS_STORE(&thumbnail->status, GFX_THUMBNAIL_STATUS_UNKNOWN);
   thumbnail->texture     = 0;
   thumbnail->width       = 0;
   thumbnail->height      = 0;
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
      bool process_r = (GFX_THUMB_STATUS_LOAD(&right_thumbnail->status) == GFX_THUMBNAIL_STATUS_UNKNOWN);
      bool process_l = (GFX_THUMB_STATUS_LOAD(&left_thumbnail->status)  == GFX_THUMBNAIL_STATUS_UNKNOWN);

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
      unsigned width, unsigned height, float scale_factor,
      float *draw_width, float *draw_height)
{
   float core_aspect;
   float display_aspect;
   float thumbnail_aspect;
   video_driver_state_t *video_st = video_state_get_ptr();

   /* Sanity check */
   if (   !thumbnail
       || (width             < 1)
       || (height            < 1)
       || (thumbnail->width  < 1)
       || (thumbnail->height < 1))
   {
      *draw_width  = 0.0f;
      *draw_height = 0.0f;
      return;
   }

   /* Account for display/thumbnail/core aspect ratio
    * differences */
   display_aspect   = (float)width            / (float)height;
   thumbnail_aspect = (float)thumbnail->width / (float)thumbnail->height;
   core_aspect      = ((thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
         && video_st && video_st->av_info.geometry.aspect_ratio > 0)
               ? video_st->av_info.geometry.aspect_ratio
               : thumbnail_aspect;

   if (thumbnail_aspect > display_aspect)
   {
      *draw_width  = (float)width;
      *draw_height = (float)thumbnail->height * (*draw_width / (float)thumbnail->width);

      if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
      {
         *draw_height = *draw_height * (thumbnail_aspect / core_aspect);

         if (*draw_height > height)
         {
            *draw_height = (float)height;
            *draw_width  = (float)thumbnail->width * (*draw_height / (float)thumbnail->height);
            *draw_width  = *draw_width / (thumbnail_aspect / core_aspect);
         }
      }
   }
   else
   {
      *draw_height = (float)height;
      *draw_width  = (float)thumbnail->width * (*draw_height / (float)thumbnail->height);

      if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
         *draw_width  = *draw_width / (thumbnail_aspect / core_aspect);
   }

   /* Final overwidth check */
   if (*draw_width > width)
   {
      *draw_width  = (float)width;
      *draw_height = (float)thumbnail->height * (*draw_width / (float)thumbnail->width);

      if (thumbnail->flags & GFX_THUMB_FLAG_CORE_ASPECT)
         *draw_height = *draw_height * (thumbnail_aspect / core_aspect);
   }

   /* Account for scale factor
    * > Side note: We cannot use the gfx_display_ctx_draw_t
    *   'scale_factor' parameter for scaling thumbnails,
    *   since this clips off any part of the expanded image
    *   that extends beyond the bounding box. But even if
    *   it didn't, we can't get real screen dimensions
    *   without scaling manually... */
   *draw_width  *= scale_factor;
   *draw_height *= scale_factor;
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
      unsigned video_width,
      unsigned video_height,
      gfx_thumbnail_t *thumbnail,
      float x, float y, unsigned width, unsigned height,
      enum gfx_thumbnail_alignment alignment,
      float alpha, float scale_factor,
      gfx_thumbnail_shadow_t *shadow)
{
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
      unsigned  thumb_width   = thumbnail->width;
      unsigned  thumb_height  = thumbnail->height;
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
         thumb_snapshot.width   = thumb_width;
         thumb_snapshot.height  = thumb_height;
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
               &thumb_snapshot, width, height, scale_factor,
               &draw_width, &draw_height);
      }

      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);

      if (!p_disp->dispctx->handles_transform)
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
            draw.width   = (unsigned)shadow_width;
            draw.height  = (unsigned)shadow_height;
            draw.x       = shadow_x;
            draw.y       = shadow_y;

            /* Draw shadow */
            if (draw.height > 0 && draw.width > 0)
               if (dispctx->draw)
                  dispctx->draw(&draw, userdata, video_width, video_height);
         }
      }

      /* Final thumbnail draw object configuration */
      coords.color = (const float*)thumbnail_color;
      draw.width   = (unsigned)draw_width;
      draw.height  = (unsigned)draw_height;
      draw.x       = draw_x;
      draw.y       = draw_y;

      /* Draw thumbnail */
      if (draw.height > 0 && draw.width > 0)
         if (dispctx->draw)
            dispctx->draw(&draw, userdata, video_width, video_height);

      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
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
   strlcpy(s + _len, ".png", len - _len);
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
   path_data->content_label_len    = 0;
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
bool gfx_thumbnail_is_enabled(gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   if (path_data)
   {
      settings_t          *settings = config_get_ptr();
      unsigned gfx_thumbnails       = settings->uints.gfx_thumbnails;
      unsigned menu_left_thumbnails = settings->uints.menu_left_thumbnails;
      unsigned menu_icon_thumbnails = settings->uints.menu_icon_thumbnails;

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
      path_data->system_len = strlcpy(path_data->system, "MAME", sizeof(path_data->system));
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
   path_data->content_label_len    = 0;
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
   path_data->content_label_len = strlcpy(path_data->content_label,
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
   path_data->content_label_len    = 0;
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

   path_data->content_label_len = fill_pathname(
         path_data->content_label,
         path_data->content_img, "",
         sizeof(path_data->content_label));

   /* Set file path */
   fill_pathname_join_special(path_data->content_path,
      img_dir, img_name, sizeof(path_data->content_path));

   /* Set core name to "imageviewer" */
   strlcpy(path_data->content_core_name,
         "imageviewer",
         sizeof(path_data->content_core_name));

   /* Set database name (arbitrarily) to "_images_"
    * (required for compatibility with gfx_thumbnail_update_path(),
    * but not actually used...) */
   strlcpy(path_data->content_db_name,
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
   path_data->content_label_len       = 0;
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
      path_data->content_label_len = strlcpy(path_data->content_label,
            content_label, sizeof(path_data->content_label));
   else
      path_data->content_label_len = fill_pathname(path_data->content_label,
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
bool gfx_thumbnail_update_path(
      gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id)
{
   char content_dir[DIR_MAX_LENGTH];
   settings_t *settings          = config_get_ptr();
   const char *system_name       = NULL;
   char *thumbnail_path          = NULL;
   const char *dir_thumbnails    = settings->paths.directory_thumbnails;
   bool playlist_allow_non_png   = settings->bools.playlist_allow_non_png;
   unsigned gfx_thumbnails       = settings->uints.gfx_thumbnails;
   unsigned menu_left_thumbnails = settings->uints.menu_left_thumbnails;
   unsigned menu_icon_thumbnails = settings->uints.menu_icon_thumbnails;
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

   if (!gfx_thumbnail_is_enabled(path_data, thumbnail_id))
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
      /* A WebM or MP4 file is likewise its own (animated) thumbnail,
       * but unlike an image the entire file must be read to decode it,
       * so refuse anything past the animation decoder's file-size cap */
      else if (   (   (image_texture_get_type(path_data->content_path)
                        == IMAGE_TYPE_WEBM)
                   || (image_texture_get_type(path_data->content_path)
                        == IMAGE_TYPE_MP4))
               && gfx_thumb_anim_mem_ok(
                     (uint64_t)path_get_size(path_data->content_path), 0))
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
         char new_path[PATH_MAX_LENGTH];
         char entry_basename[PATH_MAX_LENGTH];
         char old_savefile_dir[PATH_MAX_LENGTH];
         char old_savestate_dir[PATH_MAX_LENGTH];

         strlcpy(old_savefile_dir, runloop_st->savefile_dir, sizeof(old_savefile_dir));
         strlcpy(old_savestate_dir, runloop_st->savestate_dir, sizeof(old_savestate_dir));

         strlcpy(new_path, entry->path, sizeof(new_path));
         path_remove_extension(new_path);

         _len = strlcpy(entry_basename, path_basename(new_path), sizeof(entry_basename));
         _len = strlcpy(entry_basename + _len, ".state", sizeof(entry_basename) - _len);

         /* Set temporary save redirection paths */
         runloop_path_set_redirect(config_get_ptr(), old_savefile_dir, old_savestate_dir);

         fill_pathname_join_special(new_path,
               runloop_st->savestate_dir, entry_basename, sizeof(new_path));

         /* Restore current save redirection paths */
         dir_set(RARCH_DIR_CURRENT_SAVEFILE, old_savefile_dir);
         dir_set(RARCH_DIR_CURRENT_SAVESTATE, old_savestate_dir);

         state_name = strdup(new_path);
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
         return;
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
}
