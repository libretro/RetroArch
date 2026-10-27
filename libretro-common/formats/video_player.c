/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_player.c).
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

#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <formats/image.h>
#include <formats/video_player.h>

/* The worker's queue is lock-free and needs the atomic pointer ops; a
 * build whose atomic backend has none decodes on the polling thread,
 * as a build without threads does. */
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)
#define VIDEO_PLAYER_WORKER 1
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/tpool.h>
#endif

/* A duration of zero or less is undefined by the container specs
 * (browsers show 100 ms). The floor only stops a hostile file from
 * asking for thousands of decodes a second: frames faster than the
 * poll are passed over by the deadline, whatever its rate. */
#define VP_DUR_DEFAULT 100
#define VP_DUR_MIN     1
/* System INT64_MAX can expand to a C99 long-long literal (Apple SDKs).
 * Keep this implementation usable by strict C89 builds too. */
#define VP_TIME_MAX ((int64_t)((~(uint64_t)0) >> 1))

/* Decode time a tick shared by every player decoding on the polling
 * thread for want of the worker; past it, animations run slower
 * rather than stall the caller. */
#define VP_BUDGET_US   8000

/* Internal flags, beside enum video_player_flags */
#define VP_STARTED     (1 << 8)
#define VP_THREADED    (1 << 9)
#define VP_BEHIND      (1 << 10)
#define VP_HW          (1 << 11)
#define VP_STOPPED     (1 << 12)

enum vp_job_status
{
   VP_JOB_QUEUED = 0,  /* posted, not started                        */
   VP_JOB_RUNNING,     /* the worker is decoding into its frame      */
   VP_JOB_READY,       /* decoded, waiting to be presented           */
   VP_JOB_FINISHED,    /* stream or sink failed                      */
   VP_JOB_COMPLETED,   /* finite movie reached clean EOF              */
   VP_JOB_HELD,        /* presented as QUEUED: the slot is the sink's */
   VP_JOB_IDLE,        /* neither the worker's nor holding a frame   */
   VP_JOB_WAITING,     /* hardware busy; the next poll retries       */
   VP_JOB_CANCELLED,   /* released while queued: the worker still
                          holds its place in the queue               */
   VP_JOB_DROPPED      /* the worker came to it and let it go        */
};

/* A player's two jobs come out of one block, at separate cache-line
 * offsets: the worker and the poll hand them back and forth. */
#define VP_JOB_STRIDE 64

typedef struct vp_job
{
   struct vp_job *next;    /* queue link, the queue's               */
   struct vp_job *retired; /* the first job only: the next block
                              waiting on a cancelled job            */
   video_player_t *owner;  /* read by the worker only while RUNNING */
   uint32_t *frame;        /* the slot this job decodes into        */
   int64_t time_us;        /* movie time, published with READY      */
   unsigned stride;        /* row stride in pixels, for this job    */
   int duration_ms;        /* of the READY frame                    */
   int32_t loops_left;     /* -1 forever                            */
   /* enum vp_job_status. The worker's READY / FINISHED store is a
    * release made after frame, duration_ms, time_us and loops_left; the poll's
    * acquire that sees it sees them whole. Every other field belongs
    * to the side the status names: the worker from RUNNING to that
    * store, the poll otherwise. */
   retro_atomic_int_t status;
   bool rgba;
   bool fp16;
   bool gpu;               /* frames come on the GPU (VP_HW)        */
   bool hw;                /* the READY frame is the player's hw[]   */
   bool seek;              /* a bounded transport step, no slot */
   bool fallback;          /* exclusive-owner software restart, no slot */
   bool startup;           /* cold hardware admission, no slot */
   bool behind;            /* polling-side catch-up request */
} vp_job_t;

struct video_player
{
   int64_t next_us;        /* when the next frame is due, 0 at once */
   int64_t pause_left_us;  /* remaining picture duration at pause */
   int64_t cadence_last;
   unsigned cadence_count, cadence_sum, cadence_min, cadence_max;
   unsigned cadence_span;
   bool paused;           /* polling thread only */
   bool presented, completed, ended, cadence_seen, cadence_unstable;
   bool inline_ready, inline_finished, inline_held;
   video_player_frame_t inline_frame;
   int64_t movie_us;       /* immutable full movie period, or zero */
   int64_t origin_us;      /* polling clock's movie origin */
   int64_t pause_us;
   /* The exclusive decode owner advances this only when rewinding.
    * The poll reads job->time_us, never this cursor while decoding. */
   int64_t decode_epoch_us;
   bool decode_behind;     /* exclusive decoder owner's last setting */
   video_player_seek_cb_t seek_cb;
   void *seek_user;
   video_player_hw_restart_t hw_restart;
   void *hw_restart_user;
   bool hw_disabled, hw_fallback_pending, hw_fallback_active, preserve_origin;
   bool hw_checked, hw_starting, hw_available;
   int64_t seek_target, seek_run_target, seek_position;
   int32_t initial_loops;
   retro_atomic_int_t repeat_request;
   bool seek_pending, seek_dirty, seek_active, seek_restart, seek_failed;
   bool seek_valid, seek_show, seek_gap;
#ifdef VIDEO_PLAYER_WORKER
   retro_atomic_int_t seek_interrupt;
#endif
   void *stream;
   vp_job_t *job[2];       /* one block; NULL until a threaded start */
   video_player_sink_t sink;
   unsigned width, height;
   unsigned flags;
   int32_t loops_left;     /* -1 forever                            */
   enum image_type_enum type;
   uint8_t upload;         /* the job whose frame is shown next     */
   /* GPU frames, one per job (or [0] decoding in line): written by
    * the side that decodes, as the job's frame is */
   video_hw_frame_t hw[2];
};

/* The fallback decode's shared budget; only ever the polling thread's. */
static int64_t vp_budget_start_us = 0;
static int64_t vp_budget_used_us  = 0;

/* One pass ended: count the loop down and rewind. The next frame, or
 * NULL when the loops are done or the stream fails. */
static const uint32_t *vp_next(video_player_t *p,
      int32_t *loops_left, int *duration_ms)
{
   void *stream = p->stream;
   enum image_type_enum type = p->type;
   int repeat;
   const uint32_t *frame = image_transfer_anim_stream_next(stream, type,
         duration_ms);
   if (frame)
      return frame;
   if (image_transfer_anim_stream_waiting(stream, type))
      return NULL;
   if ((type == IMAGE_TYPE_MP4 || type == IMAGE_TYPE_WEBM)
         && !image_transfer_anim_stream_completed(stream, type))
      return NULL;
   repeat = retro_atomic_load_acquire_int(&p->repeat_request);
   if (repeat) *loops_left = repeat;
   if (*loops_left > 0)
      (*loops_left)--;
   if (*loops_left == 0)
      return NULL;
   if (p->movie_us)
   {
      if (p->decode_epoch_us > VP_TIME_MAX - p->movie_us)
         return NULL;
      p->decode_epoch_us += p->movie_us;
   }
   image_transfer_anim_stream_rewind(stream, type);
   return image_transfer_anim_stream_next(stream, type, duration_ms);
}

/* Called only by the decoder owner. A finished finite loop holds its
 * last picture through the movie's audio tail. */
static int64_t vp_frame_time(video_player_t *p, bool finished)
{
   int64_t time;
   if (!p->movie_us) return -1;
   time = finished ? p->movie_us
      : image_transfer_anim_stream_frame_time_ns(p->stream, p->type);
   if (!finished && time >= 0) time /= 1000;
   if (time < 0 || time > VP_TIME_MAX - p->decode_epoch_us) return -1;
   return p->decode_epoch_us + time;
}

static int64_t vp_deadline(video_player_t *p, int64_t time)
{
   if (time < 0) return p->next_us;
   return p->origin_us > VP_TIME_MAX - time
      ? VP_TIME_MAX : p->origin_us + time;
}

static void vp_movie_deadline(video_player_t *p, int64_t time, int64_t now)
{
   p->next_us = vp_deadline(p, time);
   if (p->seek_show && time >= 0)
      p->seek_gap = time > p->seek_position;
   if (p->paused)
   {
      uint64_t left = p->next_us > now
         ? (uint64_t)p->next_us - (uint64_t)now : 0;
      p->pause_left_us = left > (uint64_t)VP_TIME_MAX
         ? VP_TIME_MAX : (int64_t)left;
   }
}

static void vp_swizzle(uint32_t *dst, const uint32_t *src, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      uint32_t px = src[i];
      dst[i]      = (px & 0xFF00FF00u)
            | ((px & 0xFF) << 16) | ((px >> 16) & 0xFF);
   }
}

static unsigned vp_slot_stride(video_player_t *p, unsigned slot)
{
   unsigned stride = p->sink.slot_stride
      ? p->sink.slot_stride(p->sink.user, slot) : p->width;
   if (stride < p->width || (p->height && (size_t)stride
            > SIZE_MAX / ((p->flags & VIDEO_PLAYER_FP16) ? 8 : 4) / p->height))
      return 0;
   return stride;
}

static void vp_copy_frame(uint32_t *dst, const uint32_t *src,
      unsigned width, unsigned height, unsigned stride, unsigned src_stride,
      bool swizzle)
{
   unsigned y;
   if (stride == width && src_stride == width)
   {
      if (swizzle)
         vp_swizzle(dst, src, (size_t)width * height);
      else
         memcpy(dst, src, (size_t)width * height * sizeof(*dst));
      return;
   }
   for (y = 0; y < height; y++)
   {
      if (swizzle)
         vp_swizzle(dst, src, width);
      else
         memcpy(dst, src, (size_t)width * sizeof(*dst));
      dst += stride;
      src += src_stride;
   }
}

static void vp_cadence_reset(video_player_t *p)
{
   p->cadence_seen = p->cadence_unstable = false;
   p->cadence_count = p->cadence_sum = p->cadence_span = 0;
}

static void vp_cadence_present(video_player_t *p, int64_t time_us)
{
   uint64_t delta;
   unsigned interval;
   if (time_us < 0)
   {
      vp_cadence_reset(p);
      p->cadence_unstable = true;
      return;
   }
   if (!p->cadence_seen)
   {
      p->cadence_seen = true;
      p->cadence_last = time_us;
      return;
   }
   /* An audio-only loop tail does not describe picture cadence. */
   if (p->movie_us > 0
         && time_us / p->movie_us != p->cadence_last / p->movie_us)
   {
      p->cadence_last = time_us;
      p->cadence_count = p->cadence_sum = 0;
      return;
   }
   delta = time_us > p->cadence_last
      ? (uint64_t)time_us - (uint64_t)p->cadence_last : 0;
   p->cadence_last = time_us;
   if (delta < 4000 || delta > 200000)
   {
      p->cadence_count = p->cadence_sum = p->cadence_span = 0;
      p->cadence_unstable = true;
      return;
   }
   interval = (unsigned)delta;
   if (p->cadence_span
         && (interval * 32 + 32000 < p->cadence_span
            || interval * 32 > p->cadence_span + 32000))
   {
      p->cadence_span = 0;
      p->cadence_unstable = true;
   }
   if (!p->cadence_count)
      p->cadence_min = p->cadence_max = interval;
   else
   {
      if (interval < p->cadence_min)
         p->cadence_min = interval;
      if (interval > p->cadence_max)
         p->cadence_max = interval;
      if (p->cadence_max - p->cadence_min > 1000)
      {
         p->cadence_count = p->cadence_sum = p->cadence_span = 0;
         p->cadence_unstable = true;
         p->cadence_min = p->cadence_max = interval;
      }
   }
   p->cadence_sum += interval;
   if (++p->cadence_count == 32)
   {
      p->cadence_span = p->cadence_sum;
      p->cadence_unstable = false;
      p->cadence_count = p->cadence_sum = 0;
   }
}

/* The next deadline, from the last one so long-term pacing holds. A
 * frame that lands after its successor was due means the decode is
 * slower than the file: rather than show every frame late, the stream
 * passes over pictures nothing references until it is back on time. */
static void vp_schedule(video_player_t *p, int duration_ms, int64_t now)
{
   unsigned behind = 0;

   if (duration_ms <= 0)
      duration_ms = VP_DUR_DEFAULT;
   else if (duration_ms < VP_DUR_MIN)
      duration_ms = VP_DUR_MIN;

   if (p->next_us == 0)
      p->next_us = now + (int64_t)duration_ms * 1000;
   else
   {
      p->next_us += (int64_t)duration_ms * 1000;
      if (p->next_us < now)
      {
         behind     = VP_BEHIND;
         p->next_us = now + (int64_t)duration_ms * 1000;
      }
   }

   p->seek_show = false;
   p->presented = true;
   if (p->paused)
      p->pause_left_us = (int64_t)duration_ms * 1000;

   if (behind != (p->flags & VP_BEHIND))
   {
      p->flags = (p->flags & ~VP_BEHIND) | behind;
      /* A threaded decoder can be rewinding or reading its references
       * now. Its next job carries this request to the decode owner. */
      if (!(p->flags & VP_THREADED))
         image_transfer_anim_stream_set_catchup(p->stream, p->type,
               behind ? 1 : 0);
   }
}

/* Worker or polling thread: a GPU frame into hw[@idx]. */
static int vp_step_hw(video_player_t *p, unsigned idx, int32_t *loops_left,
      int *duration_ms)
{
   if (p->sink.feed && !p->sink.feed(p->sink.user))
      return false;
   if (!vp_next(p, loops_left, duration_ms))
      return image_transfer_anim_stream_waiting(p->stream, p->type) ? 2 : 0;
   /* a frame that came in software, which a backend that took the
    * stream once and not after a rewind gives, ends the playback */
   if (!image_transfer_anim_stream_hw_frame(p->stream, p->type,
            &p->hw[idx]))
      return false;
   if (p->sink.decoded)
      p->sink.decoded(p->sink.user, VIDEO_PLAYER_DIRECT);
   return true;
}

#ifdef VIDEO_PLAYER_WORKER
/* The queue takes no lock. The poll posts a job by pushing it on the
 * inbox; the worker takes the inbox whole and works through it oldest
 * first. A job released while it waits cannot be unlinked from under
 * the worker, so it is marked cancelled and its block kept until the
 * worker has let it go. */
static sthread_t          *vp_worker_thread = NULL;
static retro_atomic_ptr_t  vp_worker_inbox;          /* newest first */
static vp_job_t           *vp_worker_local  = NULL;  /* oldest first */
static retro_eventcount_t  vp_worker_wake;           /* worker       */
static retro_eventcount_t  vp_worker_done;           /* poll         */
static retro_atomic_int_t  vp_worker_die;
/* The poll's: blocks whose cancelled job the worker has yet to let go */
static vp_job_t           *vp_retired       = NULL;
/* The pool the decoders band their colour conversion, tiles and rows
 * over. The worker alone makes, uses and destroys it; set_threads only
 * publishes the count. */
#define VP_POOL_STACK (512 * 1024)
static tpool_t            *vp_pool          = NULL;
static unsigned            vp_pool_bands    = 1;
static retro_atomic_int_t  vp_pool_wanted;

/* Worker: the pool to the published count, bands - 1 threads beside
 * the worker's own band. */
static void vp_pool_sync(void)
{
   int wanted = retro_atomic_load_relaxed_int(&vp_pool_wanted);
   if (wanted < 1)
      wanted = 1;
   if ((unsigned)wanted == vp_pool_bands)
      return;
   if (vp_pool)
   {
      tpool_destroy(vp_pool);
      vp_pool = NULL;
   }
   vp_pool_bands = 1;
   if (wanted > 1)
   {
      /* A stack that holds the VP9 and HEVC decoders' recursion on
       * every platform, not the platform's default. */
      vp_pool = tpool_create_with_stack_size((size_t)(wanted - 1),
            VP_POOL_STACK);
      if (vp_pool)
         vp_pool_bands = (unsigned)wanted;
   }
}

/* Worker: one shown frame into job->frame in its final order. false
 * when the playback is over. The job is RUNNING: the stream is the
 * worker's alone. */
static int vp_job_step(vp_job_t *job)
{
   video_player_t *p    = job->owner;
   void *stream         = p->stream;
   enum image_type_enum type = p->type;
   const uint32_t *frame;
   int duration_ms      = 0;
   bool native, direct;
   enum video_player_how how;

   if (job->startup)
   {
      job->gpu = image_transfer_anim_stream_set_hw(stream, type, true);
      return 1;
   }
   if (job->fallback)
   {
      int r = p->hw_restart(p->hw_restart_user);
      return r < 0 ? 0 : (r ? 1 : 2);
   }
   if (p->decode_behind != job->behind)
   {
      image_transfer_anim_stream_set_catchup(stream, type, job->behind ? 1 : 0);
      p->decode_behind = job->behind;
   }
   if (job->seek)
   {
      int r = p->seek_cb(p->seek_user, p->seek_run_target,
            p->seek_restart, &p->seek_position);
      p->seek_restart = false;
      return r == VIDEO_PLAYER_SEEK_PROGRESS ? 3 : (r < 0 ? 0 : (r ? 1 : 2));
   }
   if (job->gpu)
   {
      int r = vp_step_hw(p, (job == p->job[1]) ? 1 : 0,
            &job->loops_left, &duration_ms);
      job->hw = r == 1;
      job->duration_ms = duration_ms;
      job->time_us = vp_frame_time(p, !r);
      return !r && job->loops_left == 0
            && image_transfer_anim_stream_completed(stream, type) ? 3 : r;
   }
   /* Every stream emits the order asked for; one that cannot is
    * swizzled below. Asked a frame at a time, and a repeat ask of the
    * order already emitted answers yes. */
   native = image_transfer_anim_stream_set_argb(stream, type,
         job->rgba ? 0 : 1);
   /* The video streams decode into the slot; APNG and WEBP hand out
    * their canvas, copied below. */
   direct = image_transfer_anim_stream_set_output_stride(stream, type,
         job->frame, job->stride);

   vp_pool_sync();
   image_transfer_anim_stream_set_blit_pool(stream, type, vp_pool,
         vp_pool_bands);

   /* The feed reads the demuxer's cursor and sets its bound, which the
    * decoder writes and reads every packet: it runs here, on the
    * thread that owns the stream. */
   if (p->sink.feed && !p->sink.feed(p->sink.user))
      return false;

   if (!(frame = vp_next(p, &job->loops_left, &duration_ms)))
   {
      job->time_us = vp_frame_time(p, true);
      if (image_transfer_anim_stream_waiting(stream, type)) return 2;
      return job->loops_left == 0
            && image_transfer_anim_stream_completed(stream, type) ? 3 : 0;
   }
   job->time_us = vp_frame_time(p, false);

   if (job->fp16)
   {
      /* half floats only come from a decode into the slot itself */
      if (     !direct || frame != job->frame
            || !image_transfer_anim_stream_is_fp16(stream, type))
         return false;
      how = VIDEO_PLAYER_DIRECT;
   }
   else if (direct && frame == job->frame)
   {
      how = VIDEO_PLAYER_DIRECT;
      if (!job->rgba && !native)
      {
         vp_copy_frame(job->frame, frame, p->width, p->height,
               job->stride, job->stride, true);
         how = VIDEO_PLAYER_SWIZZLE;
      }
   }
   else if (job->rgba || native)
   {
      /* decouples the slot from the canvas the next decode rewrites */
      vp_copy_frame(job->frame, frame, p->width, p->height,
            job->stride, p->width, false);
      how = VIDEO_PLAYER_COPY;
   }
   else
   {
      vp_copy_frame(job->frame, frame, p->width, p->height,
            job->stride, p->width, true);
      how = VIDEO_PLAYER_SWIZZLE;
   }
   if (p->sink.decoded)
      p->sink.decoded(p->sink.user, how);
   job->duration_ms = duration_ms;
   return true;
}

/* Whether a job has let go of its slot: neither queued, decoding nor
 * still holding its place as cancelled. */
static bool vp_job_settled(vp_job_t *job)
{
   switch (retro_atomic_load_acquire_int(&job->status))
   {
      case VP_JOB_QUEUED:
      case VP_JOB_RUNNING:
      case VP_JOB_CANCELLED:
         return false;
      default:
         break;
   }
   return true;
}

static void vp_job_publish(vp_job_t *job, int alive)
{
   /* Failed decodes may also have written a write-combined slot. */
   if (!job->gpu)
      retro_atomic_thread_fence_seq_cst();
   retro_atomic_store_release_int(&job->status,
         alive == 3 ? VP_JOB_COMPLETED : alive == 2 ? VP_JOB_WAITING
               : (alive ? VP_JOB_READY : VP_JOB_FINISHED));
}

static void vp_job_post(vp_job_t *job)
{
   void *head;
   retro_atomic_store_relaxed_int(&job->status, VP_JOB_QUEUED);
   do
   {
      head      = retro_atomic_load_acquire_ptr(&vp_worker_inbox);
      job->next = (vp_job_t*)head;
   } while (!retro_atomic_cas_ptr(&vp_worker_inbox, head, job));
   retro_eventcount_notify(&vp_worker_wake);
}

static void vp_worker(void *unused)
{
   (void)unused;
   for (;;)
   {
      int alive;
      vp_job_t *job = vp_worker_local;

      if (!job)
      {
         int key;
         job = (vp_job_t*)retro_atomic_exchange_ptr(&vp_worker_inbox, NULL);
         /* posted newest first: turned round, oldest first */
         while (job)
         {
            vp_job_t *next  = job->next;
            job->next       = vp_worker_local;
            vp_worker_local = job;
            job             = next;
         }
         if (vp_worker_local)
            continue;
         if (retro_atomic_load_acquire_int(&vp_worker_die))
            break;
         key = retro_eventcount_prepare_wait(&vp_worker_wake);
         if (     retro_atomic_load_acquire_ptr(&vp_worker_inbox)
               || retro_atomic_load_acquire_int(&vp_worker_die))
            retro_eventcount_cancel_wait(&vp_worker_wake);
         else
            retro_eventcount_commit_wait(&vp_worker_wake, key);
         continue;
      }
      if (retro_atomic_load_acquire_int(&vp_worker_die))
         break;

      vp_worker_local = job->next;

      /* Released while it waited: let go unrun. After either store
       * below the job may be freed and is not touched again. */
      if (!retro_atomic_cas_int(&job->status,
               VP_JOB_QUEUED, VP_JOB_RUNNING))
      {
         retro_atomic_store_release_int(&job->status, VP_JOB_DROPPED);
         continue;
      }

      if (job->seek && retro_atomic_load_acquire_int(&job->owner->seek_interrupt))
         alive = 2;
      else
         alive = vp_job_step(job);
      if (job->seek && alive == 3)
      {
         if (retro_atomic_load_acquire_int(&job->owner->seek_interrupt))
            vp_job_publish(job, 2);
         else
            vp_job_post(job);
      }
      else
         vp_job_publish(job, alive);
      retro_eventcount_notify(&vp_worker_done);
   }
}

/* Makes the worker on first use; false when the thread primitives
 * cannot be had, and the players decode on the polling thread. */
static bool vp_worker_init(void)
{
   if (vp_worker_thread)
      return true;
   if (!retro_eventcount_init(&vp_worker_wake))
   {
      retro_eventcount_free(&vp_worker_wake);
      return false;
   }
   if (!retro_eventcount_init(&vp_worker_done))
   {
      retro_eventcount_free(&vp_worker_done);
      retro_eventcount_free(&vp_worker_wake);
      return false;
   }
   retro_atomic_store_release_int(&vp_worker_die, 0);
   if (!(vp_worker_thread = sthread_create(vp_worker, NULL)))
   {
      retro_eventcount_free(&vp_worker_done);
      retro_eventcount_free(&vp_worker_wake);
      return false;
   }
   return true;
}

/* Frees the retired blocks the worker has let go of. */
static void vp_retired_reap(void)
{
   vp_job_t **pp = &vp_retired;
   while (*pp)
   {
      vp_job_t *j0 = *pp;
      vp_job_t *j1 = (vp_job_t*)((uint8_t*)j0 + VP_JOB_STRIDE);
      if (vp_job_settled(j0) && vp_job_settled(j1))
      {
         *pp = j0->retired;
         free(j0);
      }
      else
         pp  = &j0->retired;
   }
}

static void vp_job_enqueue(vp_job_t *job)
{
   if (vp_retired)
      vp_retired_reap();
   vp_job_post(job);
}

/* Takes a job back from the worker: cancels it while queued, waits the
 * decode out while running. false while the worker still holds its
 * place in the queue: the block is not to be freed until it lets go. */
static bool vp_job_release(vp_job_t *job)
{
   for (;;)
   {
      int key;
      int status = retro_atomic_load_acquire_int(&job->status);

      if (status == VP_JOB_QUEUED)
      {
         /* fails when the worker has just started on it */
         if (retro_atomic_cas_int(&job->status,
                  VP_JOB_QUEUED, VP_JOB_CANCELLED))
            return false;
         continue;
      }
      if (status == VP_JOB_CANCELLED)
         return false;
      if (status != VP_JOB_RUNNING)
         return true;

      key = retro_eventcount_prepare_wait(&vp_worker_done);
      if (retro_atomic_load_acquire_int(&job->status) == VP_JOB_RUNNING)
         retro_eventcount_commit_wait(&vp_worker_done, key);
      else
         retro_eventcount_cancel_wait(&vp_worker_done);
   }
}

/* Hands a job its slot - the first job slot 0, the second slot 1 - and
 * posts it. A slot the sink cannot give yet leaves the job IDLE for a
 * later poll to start. */
static bool vp_job_start(video_player_t *p, vp_job_t *job)
{
   unsigned slot = (job == p->job[1]) ? 1 : 0;
   uint32_t *buf = NULL;
   job->hw       = false;
   job->seek     = false;
   job->startup  = false;
   job->behind   = (p->flags & VP_BEHIND) != 0;
   if (     !(p->flags & VP_HW)
         && !(buf = p->sink.slot_begin(p->sink.user, slot)))
      return false;
   if (!(p->flags & VP_HW) && !(job->stride = vp_slot_stride(p, slot)))
   {
      if (p->sink.slot_end)
         p->sink.slot_end(p->sink.user, slot);
      retro_atomic_store_relaxed_int(&job->status, VP_JOB_FINISHED);
      return false;
   }
   job->frame = buf;
   vp_job_enqueue(job);
   return true;
}

/* Allocate or reuse the jobs; cold setup takes no sink slot. */
static bool vp_jobs_new(video_player_t *p, bool post)
{
   uint8_t *block;
   if (!p->job[0])
   {
      if (     sizeof(vp_job_t) > VP_JOB_STRIDE
            || !(block = (uint8_t*)calloc(1, 2 * VP_JOB_STRIDE)))
         return false;
      p->job[0] = (vp_job_t*)block;
      p->job[1] = (vp_job_t*)(block + VP_JOB_STRIDE);
   }
   p->job[0]->owner      = p;
   p->job[1]->owner      = p;
   p->job[0]->loops_left = p->loops_left;
   p->job[0]->rgba       = p->job[1]->rgba = (p->flags & VIDEO_PLAYER_RGBA) ? true : false;
   p->job[0]->fp16       = p->job[1]->fp16 = (p->flags & VIDEO_PLAYER_FP16) ? true : false;
   p->job[0]->gpu        = p->job[1]->gpu  = (p->flags & VP_HW) ? true : false;
   retro_atomic_store_relaxed_int(&p->job[0]->status, VP_JOB_IDLE);
   retro_atomic_store_relaxed_int(&p->job[1]->status, VP_JOB_IDLE);
   p->upload             = 0;
   if (post && !p->seek_pending)
      vp_job_start(p, p->job[0]);
   return true;
}

/* No sink slot is lent until cold codec admission settles. */
static bool vp_poll_hw_start(video_player_t *p, bool *hw)
{
   vp_job_t *job;
   if (p->hw_checked)
   {
      *hw = p->hw_available;
      return true;
   }
   if (!p->hw_starting)
   {
      if (!vp_worker_init())
      {
         p->hw_checked = true;
         p->hw_available =
               image_transfer_anim_stream_set_hw(p->stream, p->type, false);
         *hw = p->hw_available;
         return true;
      }
      if (!vp_jobs_new(p, false))
         return false;
      job = p->job[0];
      job->startup = true;
      p->hw_starting = true;
      vp_job_enqueue(job);
      return false;
   }
   job = p->job[0];
   if (!vp_job_settled(job))
      return false;
   p->hw_available =
         retro_atomic_load_acquire_int(&job->status) == VP_JOB_READY &&
         job->gpu;
   p->hw_checked = true;
   p->hw_starting = false;
   job->startup = job->gpu = false;
   retro_atomic_store_relaxed_int(&job->status, VP_JOB_IDLE);
   *hw = p->hw_available;
   return true;
}

/* Two jobs ping-pong over the stream, so the decode runs a shown frame
 * ahead of the clock: while the due frame waits in one, the other
 * decodes its successor, and a shown frame behind a run of hidden ones
 * (a VP9 alt-ref, an H.264 reorder) has a whole interval more before
 * it is late. A job is posted only while its sibling is neither queued
 * nor running, so the stream is decoded strictly in order and the loop
 * count threads from the job that just finished into the next one. */
static enum video_player_state vp_poll_threaded(video_player_t *p,
      int64_t now)
{
   video_player_frame_t fr;
   enum video_player_present res;
   vp_job_t *ju = p->job[p->upload];
   vp_job_t *jo = p->job[p->upload ^ 1];
   int su, so;

   /* recreated if it was torn down under a playing animation */
   if (!vp_worker_init())
      return VIDEO_PLAYER_PLAYING;

   /* two independent acquires: an IDLE job is not the worker's, a
    * READY one is done; the pair needs no joint snapshot */
   su = retro_atomic_load_acquire_int(&ju->status);
   so = retro_atomic_load_acquire_int(&jo->status);
   if (su == VP_JOB_WAITING)
   {
      vp_job_enqueue(ju);
      return VIDEO_PLAYER_PLAYING;
   }
   /* a job that writes its slot no more lets the sink keep it no more */
   if (p->sink.slot_end && !(p->flags & VP_HW))
   {
      if (vp_job_settled(ju))
         p->sink.slot_end(p->sink.user, p->upload);
      if (vp_job_settled(jo))
         p->sink.slot_end(p->sink.user, p->upload ^ 1);
   }

   /* Both presentations may retain their slots until a later poll.
    * Once both releases arrive there is no READY sibling to trigger
    * decode-ahead, so restart the next upload job explicitly. */
   if (su == VP_JOB_IDLE && so == VP_JOB_IDLE)
   {
      ju->loops_left = p->loops_left;
      vp_job_start(p, ju);
      return VIDEO_PLAYER_PLAYING;
   }

   /* decode-ahead: the due job holds its frame, its sibling is free */
   if (su == VP_JOB_READY && so == VP_JOB_IDLE)
   {
      p->loops_left  = ju->loops_left;
      jo->loops_left = ju->loops_left;
      vp_job_start(p, jo);
   }

   if ((su == VP_JOB_FINISHED || su == VP_JOB_COMPLETED)
         && !p->presented)
      return (p->flags & VP_HW) && video_player_request_software(p)
         ? VIDEO_PLAYER_PLAYING : VIDEO_PLAYER_ENDED;
   if (p->movie_us && (su == VP_JOB_READY || su == VP_JOB_FINISHED
            || su == VP_JOB_COMPLETED))
      vp_movie_deadline(p, ju->time_us, now);
   if (p->next_us != 0 && now < p->next_us)
      return VIDEO_PLAYER_PLAYING;
   /* loops done or the stream failed: the sibling holds only a frame
    * already shown, so nothing is lost */
   if (su == VP_JOB_FINISHED || su == VP_JOB_COMPLETED)
   {
      p->completed = p->presented && su == VP_JOB_COMPLETED;
      return VIDEO_PLAYER_ENDED;
   }
   if (su != VP_JOB_READY)
      return VIDEO_PLAYER_PLAYING;

   /* READY and not queued: the frame is shown from its slot */
   fr.pixels      = NULL;
   fr.slot        = p->upload;
   fr.width       = p->width;
   fr.height      = p->height;
   fr.duration_ms = ju->duration_ms;
   fr.time_us     = ju->time_us;
   fr.how         = VIDEO_PLAYER_DIRECT;
   fr.native      = true;
   fr.hw          = ju->hw ? &p->hw[p->upload] : NULL;
   res            = p->sink.present(p->sink.user, &fr);
   if (res == VIDEO_PLAYER_PRESENT_BUSY)
      return VIDEO_PLAYER_PLAYING;
   if (res == VIDEO_PLAYER_PRESENT_FAILED)
   {
      if (fr.hw)
         video_hw_frame_abandon(fr.hw);
      ju->hw = false;
      if (fr.hw && !p->presented && video_player_request_software(p))
         return VIDEO_PLAYER_PLAYING;
      return VIDEO_PLAYER_ENDED;
   }
   ju->hw         = false;   /* the sink's now */
   vp_cadence_present(p, fr.time_us);
   vp_schedule(p, ju->duration_ms, now);
   /* a QUEUED frame keeps its slot until the sink lets it go */
   retro_atomic_store_relaxed_int(&ju->status,
         res == VIDEO_PLAYER_PRESENT_QUEUED ? VP_JOB_HELD : VP_JOB_IDLE);
   so         = retro_atomic_load_acquire_int(&jo->status);
   p->upload ^= 1;

   /* keep the worker fed: the sibling has the next frame banked, so
    * this job can start on the one after it now */
   if (     so == VP_JOB_READY
         && retro_atomic_load_relaxed_int(&ju->status) == VP_JOB_IDLE)
   {
      p->loops_left  = jo->loops_left;
      ju->loops_left = jo->loops_left;
      vp_job_start(p, ju);
   }
   return VIDEO_PLAYER_PLAYING;
}
#endif

/* The decode on the polling thread: at the frame's due time, into the
 * slot when the sink has one, and - unless the player is INLINE by
 * choice - within the shared budget. */
static enum video_player_state vp_poll_inline(video_player_t *p,
      int64_t now)
{
   video_player_frame_t fr;
   enum video_player_present res;
   const uint32_t *frame;
   uint32_t *buf        = NULL;
   unsigned stride     = p->width;
   bool direct          = false;
   bool native;
   bool rgba            = (p->flags & VIDEO_PLAYER_RGBA) ? true : false;
   bool budgeted        = !(p->flags & VIDEO_PLAYER_INLINE);
   int duration_ms      = 0;
   int64_t budget_now   = 0;

   if (p->inline_held)
      return VIDEO_PLAYER_PLAYING;
   if (p->inline_ready)
   {
      if (now < p->next_us) return VIDEO_PLAYER_PLAYING;
      fr = p->inline_frame;
      budgeted = false;
      goto present_frame;
   }
   if (p->inline_finished)
      return now < p->next_us ? VIDEO_PLAYER_PLAYING : VIDEO_PLAYER_ENDED;
   if (!p->movie_us && p->next_us != 0 && now < p->next_us)
      return VIDEO_PLAYER_PLAYING;

   if (budgeted)
   {
      /* The playback clock may be paused or follow mixed PCM. */
      budget_now = cpu_features_get_time_usec();
      if (budget_now - vp_budget_start_us > 15000)
      {
         vp_budget_start_us = budget_now;
         vp_budget_used_us  = 0;
      }
      if (vp_budget_used_us > VP_BUDGET_US)
         return VIDEO_PLAYER_PLAYING;
   }

   fr.hw = NULL;
   if (p->flags & VP_HW)
   {
      int r = vp_step_hw(p, 0, &p->loops_left, &duration_ms);
      if (r == 2)
         return VIDEO_PLAYER_PLAYING;
      if (!r)
         goto finished;
      fr.pixels      = NULL;
      fr.slot        = 0;
      fr.width       = p->width;
      fr.height      = p->height;
      fr.duration_ms = duration_ms;
      fr.native      = true;
      fr.how         = VIDEO_PLAYER_DIRECT;
      fr.hw          = &p->hw[0];
      goto decoded_frame;
   }

   native = image_transfer_anim_stream_set_argb(p->stream, p->type,
         rgba ? 0 : 1);
   if (p->sink.slot_begin)
   {
      if (!(buf = p->sink.slot_begin(p->sink.user, 0)))
         return VIDEO_PLAYER_PLAYING;
      if (!(stride = vp_slot_stride(p, 0)))
      {
         if (p->sink.slot_end)
            p->sink.slot_end(p->sink.user, 0);
         return VIDEO_PLAYER_ENDED;
      }
      direct = image_transfer_anim_stream_set_output_stride(p->stream, p->type,
            buf, stride);
   }

   /* the feed sits beside the decode, as on the worker */
   if (p->sink.feed && !p->sink.feed(p->sink.user))
      goto ended;

   if (!(frame = vp_next(p, &p->loops_left, &duration_ms)))
   {
      if (buf && p->sink.slot_end)
         p->sink.slot_end(p->sink.user, 0);
      if (image_transfer_anim_stream_waiting(p->stream, p->type))
         return VIDEO_PLAYER_PLAYING;
      goto finished;
   }

   fr.pixels      = NULL;
   fr.slot        = 0;
   fr.width       = p->width;
   fr.height      = p->height;
   fr.duration_ms = duration_ms;
   fr.native      = true;
   if (p->flags & VIDEO_PLAYER_FP16)
   {
      if (     !direct || frame != buf
            || !image_transfer_anim_stream_is_fp16(p->stream, p->type))
         goto ended;
      fr.how = VIDEO_PLAYER_DIRECT;
   }
   else if (direct && frame == buf)
   {
      fr.how = VIDEO_PLAYER_DIRECT;
      if (!rgba && !native)
      {
         vp_copy_frame(buf, frame, p->width, p->height, stride, stride, true);
         fr.how = VIDEO_PLAYER_SWIZZLE;
      }
   }
   else if (buf && !rgba && !native)
   {
      vp_copy_frame(buf, frame, p->width, p->height, stride, p->width, true);
      fr.how = VIDEO_PLAYER_SWIZZLE;
   }
   else
   {
      /* shown from the canvas, which no further decode touches before
       * the present returns */
      fr.pixels = frame;
      fr.how    = VIDEO_PLAYER_CANVAS;
      fr.native = rgba || native;
   }
   if (p->sink.decoded)
      p->sink.decoded(p->sink.user, fr.how);

   if (buf && p->sink.slot_end)
      p->sink.slot_end(p->sink.user, 0);
decoded_frame:
   fr.time_us = vp_frame_time(p, false);
   if (budgeted)
      vp_budget_used_us += cpu_features_get_time_usec() - budget_now;
   if (p->movie_us)
   {
      p->inline_frame = fr;
      p->inline_ready = true;
      vp_movie_deadline(p, fr.time_us, now);
      if (now < p->next_us) return VIDEO_PLAYER_PLAYING;
   }
present_frame:
   res = p->sink.present(p->sink.user, &fr);
   if (res == VIDEO_PLAYER_PRESENT_BUSY)
   {
      if (fr.hw && !p->inline_ready) video_hw_frame_abandon(fr.hw);
      return VIDEO_PLAYER_PLAYING;
   }
   p->inline_ready = false;
   if (res == VIDEO_PLAYER_PRESENT_FAILED)
   {
      if (fr.hw) video_hw_frame_abandon(fr.hw);
      if (fr.hw && !p->presented && video_player_request_software(p))
         return VIDEO_PLAYER_PLAYING;
      return VIDEO_PLAYER_ENDED;
   }
   p->inline_held = fr.hw && res == VIDEO_PLAYER_PRESENT_QUEUED;
   vp_cadence_present(p, fr.time_us);
   vp_schedule(p, fr.duration_ms, now);
   return VIDEO_PLAYER_PLAYING;

finished:
   p->completed = p->presented && p->loops_left == 0
         && image_transfer_anim_stream_completed(p->stream, p->type);
   if ((p->flags & VP_HW) && !p->presented && video_player_request_software(p))
      return VIDEO_PLAYER_PLAYING;
   if (!p->presented)
      return VIDEO_PLAYER_ENDED;
   if (p->movie_us)
   {
      p->inline_finished = true;
      vp_movie_deadline(p, vp_frame_time(p, true), now);
      return now < p->next_us ? VIDEO_PLAYER_PLAYING : VIDEO_PLAYER_ENDED;
   }
   return VIDEO_PLAYER_ENDED;
ended:
   if (buf && p->sink.slot_end)
      p->sink.slot_end(p->sink.user, 0);
   return VIDEO_PLAYER_ENDED;
}

/* Quiesce the old decode without waiting, then reuse its worker slot
 * for bounded seek steps. The polling thread alone changes requests. */
static int vp_poll_seek(video_player_t *p, int64_t now)
{
   int result;
#ifdef VIDEO_PLAYER_WORKER
   if (p->flags & VP_THREADED)
   {
      vp_job_t *job = p->job[0];
      if (!p->seek_active)
      {
         unsigned i;
         for (i = 0; i < 2; i++)
         {
            vp_job_t *j = p->job[i];
            int status = retro_atomic_load_acquire_int(&j->status);
            if (status == VP_JOB_QUEUED)
            {
               retro_atomic_cas_int(&j->status,
                     VP_JOB_QUEUED, VP_JOB_CANCELLED);
               return 0;
            }
            if (status == VP_JOB_RUNNING || status == VP_JOB_CANCELLED
                  || status == VP_JOB_HELD)
               return 0;
            if (j->hw)
               video_hw_frame_abandon(&p->hw[i]);
            j->hw = false;
            if (p->sink.slot_end && !(p->flags & VP_HW))
               p->sink.slot_end(p->sink.user, i);
            j->frame = NULL;
            retro_atomic_store_relaxed_int(&j->status, VP_JOB_IDLE);
         }
      }
      else
      {
         int status = retro_atomic_load_acquire_int(&job->status);
         if (!vp_job_settled(job))
            return 0;
         if (!p->seek_dirty && status != VP_JOB_WAITING)
         {
            result = status == VP_JOB_READY ? 1 : -1;
            goto finished;
         }
      }
      if (p->seek_dirty)
      {
         p->seek_run_target = p->seek_target;
         p->seek_position = -1;
      }
      p->seek_restart = p->seek_dirty;
      p->seek_dirty = false;
      p->seek_active = true;
      job->seek = true;
      retro_atomic_store_release_int(&p->seek_interrupt, 0);
      vp_job_enqueue(job);
      return 0;
   }
#endif
   if (p->seek_dirty)
   {
      p->seek_run_target = p->seek_target;
      p->seek_position = -1;
   }
   p->seek_restart = p->seek_dirty;
   p->seek_dirty = false;
   p->seek_active = true;
   result = p->seek_cb(p->seek_user, p->seek_run_target,
         p->seek_restart, &p->seek_position);
   if (!result || result == VIDEO_PLAYER_SEEK_PROGRESS)
      return 0;
#ifdef VIDEO_PLAYER_WORKER
finished:
#endif
   p->seek_pending = p->seek_active = false;
   if (result < 0 || p->seek_position < 0)
   {
      p->seek_failed = true;
      return -1;
   }
   p->seek_valid = p->seek_show = true;
   p->next_us = p->pause_left_us = 0;
   p->decode_epoch_us = 0;
   p->inline_finished = false;
   if (p->paused) p->pause_us = now;
   p->origin_us = now < -VP_TIME_MAX + p->seek_position
      ? -VP_TIME_MAX : now - p->seek_position;
   p->loops_left = p->initial_loops;
#ifdef VIDEO_PLAYER_WORKER
   if (p->flags & VP_THREADED)
   {
      unsigned i;
      p->upload = 0;
      for (i = 0; i < 2; i++)
      {
         p->job[i]->seek = false;
         p->job[i]->loops_left = p->initial_loops;
         retro_atomic_store_relaxed_int(&p->job[i]->status, VP_JOB_IDLE);
      }
      vp_job_start(p, p->job[0]);
   }
#endif
   return 1;
}

/* Polling side quiesces without waiting; the existing decoder worker owns
 * stream replacement. No frame/slot is lent to this recovery job. */
static int vp_poll_hw_fallback(video_player_t *p)
{
   int result;
   if (p->inline_held)
      return 0;
#ifdef VIDEO_PLAYER_WORKER
   if (p->flags & VP_THREADED)
   {
      vp_job_t *job = p->job[0];
      if (!p->hw_fallback_active)
      {
         unsigned i;
         for (i = 0; i < 2; i++)
         {
            vp_job_t *j = p->job[i];
            int status = retro_atomic_load_acquire_int(&j->status);
            if (status == VP_JOB_QUEUED)
            {
               retro_atomic_cas_int(&j->status, VP_JOB_QUEUED,
                                    VP_JOB_CANCELLED);
               return 0;
            }
            if (!vp_job_settled(j) || status == VP_JOB_HELD)
               return 0;
            if (j->hw)
               video_hw_frame_abandon(&p->hw[i]);
            j->hw = false;
            retro_atomic_store_relaxed_int(&j->status, VP_JOB_IDLE);
         }
         job->fallback = true;
         p->hw_fallback_active = true;
         vp_job_enqueue(job);
         return 0;
      }
      if (!vp_job_settled(job))
         return 0;
      if (retro_atomic_load_acquire_int(&job->status) == VP_JOB_WAITING)
      {
         vp_job_enqueue(job);
         return 0;
      }
      result = retro_atomic_load_acquire_int(&job->status)
            == VP_JOB_READY ? 1 : -1;
      free(p->job[0]);
      p->job[0] = p->job[1] = NULL;
   }
   else
#endif
      result = p->hw_restart(p->hw_restart_user);
   if (!result)
      return 0;
   p->hw_fallback_pending = p->hw_fallback_active = false;
   if (result < 0)
   {
      p->flags |= VP_STOPPED;
      return -1;
   }
   p->flags &= VIDEO_PLAYER_INLINE;
   p->presented = false;
   p->loops_left = p->initial_loops;
   p->next_us = p->decode_epoch_us = 0;
   p->decode_behind = false;
   p->inline_ready = p->inline_finished = false;
   p->seek_active = false;
   if (p->seek_cb && p->seek_valid)
      p->seek_pending = true;
   p->seek_dirty = p->seek_pending;
   if (p->paused)
      p->seek_show = true;
   return 1;
}

bool video_player_set_hw_fallback(video_player_t *p,
                                  video_player_hw_restart_t restart, void *user)
{
   if (!p || (p->flags & (VP_STARTED | VP_STOPPED)) || p->hw_fallback_pending ||
       p->hw_disabled)
      return false;
   p->hw_restart = restart;
   p->hw_restart_user = user;
   return true;
}
bool video_player_request_software(video_player_t *p)
{
   if (!p || (p->flags & VP_STOPPED) || !p->hw_restart)
      return false;
   if (p->hw_fallback_pending)
      return true;
   if (!(p->flags & VP_HW) || p->hw_disabled)
      return false;
   if (p->inline_ready && p->inline_frame.hw)
      video_hw_frame_abandon(p->inline_frame.hw);
   p->inline_ready = false;
   p->hw_disabled = p->hw_fallback_pending = true;
   p->preserve_origin = (p->flags & VP_STARTED) != 0;
   return true;
}

video_player_t *video_player_new(void *stream, enum image_type_enum type,
      int loop_count, const video_player_sink_t *sink, unsigned flags)
{
   video_player_t *p;
   int num_frames = 0, loops = 0;

   if (!stream || !sink || !sink->start || !sink->present)
      return NULL;
   if (!(p = (video_player_t*)calloc(1, sizeof(*p))))
      return NULL;
#ifdef VIDEO_PLAYER_WORKER
   retro_atomic_int_init(&p->seek_interrupt, 0);
#endif
   p->stream     = stream;
   p->type       = type;
   p->sink       = *sink;
   p->flags      = flags & (VIDEO_PLAYER_RGBA | VIDEO_PLAYER_FP16
         | VIDEO_PLAYER_INLINE);
   p->loops_left = (loop_count == 0) ? -1 : loop_count;
   p->initial_loops = p->loops_left;
   retro_atomic_int_init(&p->repeat_request, 0);
   if (type == IMAGE_TYPE_MP4 || type == IMAGE_TYPE_WEBM)
   {
      int64_t duration = image_transfer_anim_stream_duration_ns(stream, type);
      if (duration > 0) p->movie_us = duration / 1000;
   }
   image_transfer_anim_stream_get_info(stream, type, &p->width,
         &p->height, &num_frames, &loops);
   return p;
}

bool video_player_set_repeat(video_player_t *p, bool repeat)
{
   if (!p || p->ended || (p->flags & VP_STOPPED)
         || (p->type != IMAGE_TYPE_MP4 && p->type != IMAGE_TYPE_WEBM))
      return false;
   p->initial_loops = repeat ? -1 : 1;
   retro_atomic_store_release_int(&p->repeat_request, p->initial_loops);
   return true;
}

bool video_player_cadence_unstable(const video_player_t *p)
{
   return p && p->movie_us && p->cadence_unstable;
}

bool video_player_cadence(const video_player_t *p,
      unsigned *numerator, unsigned *denominator)
{
   if (!p || !numerator || !denominator || !p->cadence_span
         || (p->flags & VP_STOPPED) || p->seek_pending)
      return false;
   *numerator = 32000000;
   *denominator = p->cadence_span;
   return true;
}

bool video_player_completed(const video_player_t *p)
{
   return p && p->ended && p->completed && !(p->flags & VP_STOPPED)
         && !p->inline_ready && !p->seek_pending && !p->seek_failed;
}

enum video_player_state video_player_poll(video_player_t *p, int64_t now_us)
{
   enum video_player_state result;
   if (!p || (p->flags & VP_STOPPED))
      return VIDEO_PLAYER_ENDED;
   if (p->hw_fallback_pending)
   {
      int recovered = vp_poll_hw_fallback(p);
      if (recovered <= 0)
         return recovered < 0 ? VIDEO_PLAYER_ENDED : VIDEO_PLAYER_PLAYING;
   }
   if (p->seek_failed)
      return VIDEO_PLAYER_SEEK_FAILED;
   if (p->paused && !p->seek_pending && !p->seek_show)
      return VIDEO_PLAYER_PLAYING;

   if (!(p->flags & VP_STARTED))
   {
      int r;
      bool threaded = false;
      /* frames on the GPU where the sink and the stream both can */
      bool hw = false;
      if (!p->hw_disabled && p->sink.start_hw
            && (p->type == IMAGE_TYPE_MP4 || p->type == IMAGE_TYPE_WEBM))
      {
#ifdef VIDEO_PLAYER_WORKER
         if (!vp_poll_hw_start(p, &hw))
            return VIDEO_PLAYER_PLAYING;
#else
         hw = image_transfer_anim_stream_set_hw(p->stream, p->type, true);
#endif
      }
#ifdef VIDEO_PLAYER_WORKER
      threaded = !(p->flags & VIDEO_PLAYER_INLINE)
         && (p->sink.slot_begin || hw) && vp_worker_init();
#endif
      if (hw)
         r = p->sink.start_hw(p->sink.user, p->width, p->height,
               threaded ? 2 : 1);
      else
         r = p->sink.start(p->sink.user, p->width, p->height,
               threaded ? 2 : (p->sink.slot_begin ? 1 : 0));
      if (hw && r == VIDEO_PLAYER_HW_REFUSED)
      {
         if (!p->hw_restart)
         {
            p->flags |= VP_STOPPED;
            return VIDEO_PLAYER_ENDED;
         }
         p->flags |= VP_HW;
#ifdef VIDEO_PLAYER_WORKER
         if (p->job[0])
            p->flags |= VP_THREADED;
#endif
         video_player_request_software(p);
         return VIDEO_PLAYER_PLAYING;
      }
      if (r < 0)
         return VIDEO_PLAYER_PLAYING;
      p->flags = (p->flags & VIDEO_PLAYER_INLINE) | (hw ? VP_HW
         : ((unsigned)r & (VIDEO_PLAYER_RGBA | VIDEO_PLAYER_FP16)));
      /* told once, before any decode */
      image_transfer_anim_stream_set_want_fp16(p->stream, p->type,
            (p->flags & VIDEO_PLAYER_FP16) ? true : false);
      if (!p->preserve_origin) p->origin_us = now_us;
      p->preserve_origin = false;
#ifdef VIDEO_PLAYER_WORKER
      if (threaded)
      {
         /* all or nothing: asked again on a later poll */
         if (!vp_jobs_new(p, true))
            return VIDEO_PLAYER_PLAYING;
         p->flags |= VP_STARTED | VP_THREADED;
         return VIDEO_PLAYER_PLAYING;
      }
      if (p->job[0])
      {
         free(p->job[0]);
         p->job[0] = p->job[1] = NULL;
      }
#endif
      p->flags |= VP_STARTED;
   }

   if (p->seek_pending)
   {
      int r = vp_poll_seek(p, now_us);
      if (r <= 0)
         return r < 0 ? VIDEO_PLAYER_SEEK_FAILED : VIDEO_PLAYER_PLAYING;
   }
   if (p->paused && p->movie_us) now_us = p->pause_us;

#ifdef VIDEO_PLAYER_WORKER
   if (p->flags & VP_THREADED)
      result = vp_poll_threaded(p, now_us);
   else
#endif
      result = vp_poll_inline(p, now_us);
   p->ended = result == VIDEO_PLAYER_ENDED;
   return result;
}

bool video_player_set_seek_handler(video_player_t *p,
      video_player_seek_cb_t handler, void *user)
{
   if (!p || (p->flags & (VP_STARTED | VP_STOPPED)) || p->seek_pending)
      return false;
   p->seek_cb = handler;
   p->seek_user = user;
   return true;
}

bool video_player_seek(video_player_t *p, int64_t target_us)
{
   if (!p || !p->seek_cb || (p->flags & VP_STOPPED))
      return false;
   p->ended = p->completed = false;
   vp_cadence_reset(p);
   if (p->inline_ready && p->inline_frame.hw)
      video_hw_frame_abandon(p->inline_frame.hw);
   p->inline_ready = false;
   p->seek_target = target_us < 0 ? 0 : target_us;
   p->seek_failed = false;
   p->seek_pending = p->seek_dirty = true;
#ifdef VIDEO_PLAYER_WORKER
   retro_atomic_store_release_int(&p->seek_interrupt, 1);
#endif
   p->seek_valid = p->seek_show = false;
   p->seek_gap = false;
   return true;
}

bool video_player_seeking(const video_player_t *p)
{
   return p && p->seek_pending && !(p->flags & VP_STOPPED);
}

bool video_player_seek_position(const video_player_t *p, int64_t *position_us)
{
   if (position_us) *position_us = 0;
   if (!p || p->seek_pending || !p->seek_valid
         || (p->seek_show && !p->seek_gap) || (p->flags & VP_STOPPED))
      return false;
   if (position_us) *position_us = p->seek_position;
   return true;
}

void video_player_set_paused(video_player_t *p, bool paused, int64_t now_us)
{
   if (!p || (p->flags & VP_STOPPED) || p->paused == paused)
      return;
   if (paused)
   {
      uint64_t left = p->next_us && p->next_us > now_us
         ? (uint64_t)p->next_us - (uint64_t)now_us : 0;
      p->pause_left_us = left > (uint64_t)VP_TIME_MAX
         ? VP_TIME_MAX : (int64_t)left;
      p->pause_us = now_us;
   }
   else
   {
      if (p->movie_us && now_us > p->pause_us)
      {
         uint64_t elapsed = (uint64_t)now_us - (uint64_t)p->pause_us;
         p->origin_us = elapsed > (uint64_t)VP_TIME_MAX
            || p->origin_us > VP_TIME_MAX - (int64_t)elapsed
            ? VP_TIME_MAX : p->origin_us + (int64_t)elapsed;
      }
      if (p->next_us)
         p->next_us = now_us > VP_TIME_MAX - p->pause_left_us
            ? VP_TIME_MAX : now_us + p->pause_left_us;
   }
   p->paused = paused;
}

bool video_player_paused(const video_player_t *p)
{
   return p && p->paused && !(p->flags & VP_STOPPED);
}

bool video_player_has_presented(const video_player_t *p)
{
   return p && p->presented;
}

bool video_player_started(const video_player_t *p)
{
   return p && (p->flags & VP_STARTED) && !(p->flags & VP_STOPPED);
}

int64_t video_player_due(const video_player_t *p)
{
   return p ? (video_player_paused(p) ? VP_TIME_MAX : p->next_us) : 0;
}

void video_player_slot_released(video_player_t *p, unsigned slot)
{
#ifdef VIDEO_PLAYER_WORKER
   vp_job_t *job;
#endif
   if (p && slot == 0) p->inline_held = false;
#ifdef VIDEO_PLAYER_WORKER
   if (!p || slot > 1 || !(job = p->job[slot]))
      return;
   if (retro_atomic_load_relaxed_int(&job->status) == VP_JOB_HELD)
      retro_atomic_store_relaxed_int(&job->status, VP_JOB_IDLE);
#else
   (void)p;
   (void)slot;
#endif
}

bool video_player_stop(video_player_t *p)
{
   bool stopped = true;
   if (!p)
      return true;
   p->flags |= VP_STOPPED;
#ifdef VIDEO_PLAYER_WORKER
   retro_atomic_store_release_int(&p->seek_interrupt, 1);
   if (p->job[0])
   {
      unsigned i;
      for (i = 0; i < 2; i++)
      {
         vp_job_t *job = p->job[i];
         int status = retro_atomic_load_acquire_int(&job->status);
         if (status == VP_JOB_QUEUED)
         {
            if (retro_atomic_cas_int(&job->status,
                     VP_JOB_QUEUED, VP_JOB_CANCELLED))
               continue;
            status = retro_atomic_load_acquire_int(&job->status);
         }
         if (status == VP_JOB_RUNNING)
            stopped = false;
      }
   }
#endif
   return stopped;
}

void video_player_free(video_player_t *p)
{
   if (!p)
      return;
#ifdef VIDEO_PLAYER_WORKER
   retro_atomic_store_release_int(&p->seek_interrupt, 1);
#endif
   if (p->inline_ready && p->inline_frame.hw)
      video_hw_frame_abandon(p->inline_frame.hw);
#ifdef VIDEO_PLAYER_WORKER
   if (p->job[0])
   {
      unsigned i;
      vp_job_t *j0  = p->job[0];
      bool free_now = vp_job_release(p->job[1]);
      if (!vp_job_release(j0))
         free_now = false;
      /* a GPU frame decoded and never shown goes back */
      for (i = 0; i < 2; i++)
         if (     p->job[i]->hw
               && retro_atomic_load_acquire_int(&p->job[i]->status)
                  == VP_JOB_READY)
            video_hw_frame_abandon(&p->hw[i]);
      if (free_now)
         free(j0);
      else
      {
         j0->retired = vp_retired;
         vp_retired  = j0;
      }
      vp_retired_reap();
      /* a running decode was waited out and a queued one will not run:
       * neither slot is written any more */
      if (p->sink.slot_end && (p->flags & VP_STARTED) && !(p->flags & VP_HW))
      {
         p->sink.slot_end(p->sink.user, 0);
         p->sink.slot_end(p->sink.user, 1);
      }
   }
#endif
   free(p);
}

void video_player_set_threads(unsigned threads)
{
#ifdef VIDEO_PLAYER_WORKER
   retro_atomic_store_relaxed_int(&vp_pool_wanted, (int)threads);
#else
   (void)threads;
#endif
}

void video_player_worker_deinit(void)
{
#ifdef VIDEO_PLAYER_WORKER
   vp_job_t *job;
   if (!vp_worker_thread)
      return;
   retro_atomic_store_release_int(&vp_worker_die, 1);
   retro_eventcount_notify(&vp_worker_wake);
   sthread_join(vp_worker_thread);
   vp_worker_thread = NULL;

   /* Joined: the queue is the caller's now. What is still in it goes
    * back to its player and never advances, and a cancelled job is let
    * go as the worker would have. */
   job = (vp_job_t*)retro_atomic_exchange_ptr(&vp_worker_inbox, NULL);
   for (;;)
   {
      vp_job_t *next;
      if (!job)
      {
         if (!(job = vp_worker_local))
            break;
         vp_worker_local = NULL;
      }
      next = job->next;
      retro_atomic_store_release_int(&job->status,
            retro_atomic_load_acquire_int(&job->status) == VP_JOB_CANCELLED
            ? VP_JOB_DROPPED : VP_JOB_IDLE);
      job  = next;
   }
   vp_retired_reap();

   if (vp_pool)
   {
      tpool_destroy(vp_pool);
      vp_pool = NULL;
   }
   vp_pool_bands = 1;
   retro_eventcount_free(&vp_worker_done);
   retro_eventcount_free(&vp_worker_wake);
#endif
}
