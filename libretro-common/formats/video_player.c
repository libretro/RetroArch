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
   VP_JOB_FINISHED,    /* loops done, or the stream failed           */
   VP_JOB_HELD,        /* presented as QUEUED: the slot is the sink's */
   VP_JOB_IDLE,        /* neither the worker's nor holding a frame   */
   VP_JOB_WAITING,     /* hardware busy; the next poll retries       */
   VP_JOB_CANCELLED,   /* released while queued: the worker still
                          holds its place in the queue               */
   VP_JOB_DROPPED      /* the worker came to it and let it go        */
};

/* A player's two jobs come out of one block, the second a cache line
 * in: the worker and the poll hand them back and forth, so they must
 * not share a line. */
#define VP_JOB_STRIDE 64

typedef struct vp_job
{
   struct vp_job *next;    /* queue link, the queue's               */
   struct vp_job *retired; /* the first job only: the next block
                              waiting on a cancelled job            */
   video_player_t *owner;  /* read by the worker only while RUNNING */
   uint32_t *frame;        /* the slot this job decodes into        */
   unsigned stride;        /* row stride in pixels, for this job    */
   int duration_ms;        /* of the READY frame                    */
   int32_t loops_left;     /* -1 forever                            */
   /* enum vp_job_status. The worker's READY / FINISHED store is a
    * release made after frame, duration_ms and loops_left; the poll's
    * acquire that sees it sees them whole. Every other field belongs
    * to the side the status names: the worker from RUNNING to that
    * store, the poll otherwise. */
   retro_atomic_int_t status;
   bool rgba;
   bool fp16;
   bool gpu;               /* frames come on the GPU (VP_HW)        */
   bool hw;                /* the READY frame is the player's hw[]   */
} vp_job_t;

struct video_player
{
   int64_t next_us;        /* when the next frame is due, 0 at once */
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
static const uint32_t *vp_next(void *stream, enum image_type_enum type,
      int32_t *loops_left, int *duration_ms)
{
   const uint32_t *frame = image_transfer_anim_stream_next(stream, type,
         duration_ms);
   if (frame)
      return frame;
   if (image_transfer_anim_stream_waiting(stream, type))
      return NULL;
   if (*loops_left > 0)
      (*loops_left)--;
   if (*loops_left == 0)
      return NULL;
   image_transfer_anim_stream_rewind(stream, type);
   return image_transfer_anim_stream_next(stream, type, duration_ms);
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

   if (behind != (p->flags & VP_BEHIND))
   {
      p->flags = (p->flags & ~VP_BEHIND) | behind;
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
   if (!vp_next(p->stream, p->type, loops_left, duration_ms))
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

   if (job->gpu)
   {
      int r = vp_step_hw(p, (job == p->job[1]) ? 1 : 0,
            &job->loops_left, &duration_ms);
      job->hw = r == 1;
      job->duration_ms = duration_ms;
      return r;
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

   if (!(frame = vp_next(stream, type, &job->loops_left, &duration_ms)))
      return false;

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
         alive == 2 ? VP_JOB_WAITING : (alive ? VP_JOB_READY : VP_JOB_FINISHED));
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

      alive = vp_job_step(job);

      /* Publishes the frame to the poll, and wakes a release waiting
       * the decode out. */
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
   void *head;
   if (vp_retired)
      vp_retired_reap();
   retro_atomic_store_relaxed_int(&job->status, VP_JOB_QUEUED);
   do
   {
      head      = retro_atomic_load_acquire_ptr(&vp_worker_inbox);
      job->next = (vp_job_t*)head;
   } while (!retro_atomic_cas_ptr(&vp_worker_inbox, head, job));
   retro_eventcount_notify(&vp_worker_wake);
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

/* The first threaded poll: the two jobs, the first one posted. */
static bool vp_jobs_new(video_player_t *p)
{
   uint8_t *block;
   if (     sizeof(vp_job_t) > VP_JOB_STRIDE
         || !(block = (uint8_t*)calloc(1, 2 * VP_JOB_STRIDE)))
      return false;
   p->job[0]             = (vp_job_t*)block;
   p->job[1]             = (vp_job_t*)(block + VP_JOB_STRIDE);
   p->job[0]->owner      = p;
   p->job[1]->owner      = p;
   p->job[0]->loops_left = p->loops_left;
   p->job[0]->rgba       = p->job[1]->rgba = (p->flags & VIDEO_PLAYER_RGBA) ? true : false;
   p->job[0]->fp16       = p->job[1]->fp16 = (p->flags & VIDEO_PLAYER_FP16) ? true : false;
   p->job[0]->gpu        = p->job[1]->gpu  = (p->flags & VP_HW) ? true : false;
   retro_atomic_store_relaxed_int(&p->job[0]->status, VP_JOB_IDLE);
   retro_atomic_store_relaxed_int(&p->job[1]->status, VP_JOB_IDLE);
   p->upload             = 0;
   vp_job_start(p, p->job[0]);
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

   /* decode-ahead: the due job holds its frame, its sibling is free */
   if (su == VP_JOB_READY && so == VP_JOB_IDLE)
   {
      p->loops_left  = ju->loops_left;
      jo->loops_left = ju->loops_left;
      vp_job_start(p, jo);
   }

   if (p->next_us != 0 && now < p->next_us)
      return VIDEO_PLAYER_PLAYING;
   /* loops done or the stream failed: the sibling holds only a frame
    * already shown, so nothing is lost */
   if (su == VP_JOB_FINISHED)
      return VIDEO_PLAYER_ENDED;
   if (su != VP_JOB_READY)
      return VIDEO_PLAYER_PLAYING;

   /* READY and not queued: the frame is shown from its slot */
   fr.pixels      = NULL;
   fr.slot        = p->upload;
   fr.width       = p->width;
   fr.height      = p->height;
   fr.duration_ms = ju->duration_ms;
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
      return VIDEO_PLAYER_ENDED;
   }
   ju->hw         = false;   /* the sink's now */
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

   if (p->next_us != 0 && now < p->next_us)
      return VIDEO_PLAYER_PLAYING;

   if (budgeted)
   {
      /* the window resets after about one 60 Hz frame */
      if (now - vp_budget_start_us > 15000)
      {
         vp_budget_start_us = now;
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
         return VIDEO_PLAYER_ENDED;
      fr.pixels      = NULL;
      fr.slot        = 0;
      fr.width       = p->width;
      fr.height      = p->height;
      fr.duration_ms = duration_ms;
      fr.native      = true;
      fr.how         = VIDEO_PLAYER_DIRECT;
      fr.hw          = &p->hw[0];
      res            = p->sink.present(p->sink.user, &fr);
      /* decoded in line, a frame not taken now is gone */
      if (     res == VIDEO_PLAYER_PRESENT_BUSY
            || res == VIDEO_PLAYER_PRESENT_FAILED)
         video_hw_frame_abandon(fr.hw);
      if (res == VIDEO_PLAYER_PRESENT_BUSY)
         return VIDEO_PLAYER_PLAYING;
      if (res == VIDEO_PLAYER_PRESENT_FAILED)
         return VIDEO_PLAYER_ENDED;
      vp_schedule(p, duration_ms, now);
      if (budgeted)
         vp_budget_used_us += cpu_features_get_time_usec() - now;
      return VIDEO_PLAYER_PLAYING;
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

   if (!(frame = vp_next(p->stream, p->type, &p->loops_left, &duration_ms)))
      goto ended;

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
   res = p->sink.present(p->sink.user, &fr);
   if (res == VIDEO_PLAYER_PRESENT_BUSY)
      return VIDEO_PLAYER_PLAYING;
   if (res == VIDEO_PLAYER_PRESENT_FAILED)
      return VIDEO_PLAYER_ENDED;
   vp_schedule(p, duration_ms, now);
   if (budgeted)
      vp_budget_used_us += cpu_features_get_time_usec() - now;
   return VIDEO_PLAYER_PLAYING;

ended:
   if (buf && p->sink.slot_end)
      p->sink.slot_end(p->sink.user, 0);
   return VIDEO_PLAYER_ENDED;
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
   p->stream     = stream;
   p->type       = type;
   p->sink       = *sink;
   p->flags      = flags & (VIDEO_PLAYER_RGBA | VIDEO_PLAYER_FP16
         | VIDEO_PLAYER_INLINE);
   p->loops_left = (loop_count == 0) ? -1 : loop_count;
   image_transfer_anim_stream_get_info(stream, type, &p->width,
         &p->height, &num_frames, &loops);
   return p;
}

enum video_player_state video_player_poll(video_player_t *p, int64_t now_us)
{
   if (!p || (p->flags & VP_STOPPED))
      return VIDEO_PLAYER_ENDED;

   if (!(p->flags & VP_STARTED))
   {
      int r;
      bool threaded = false;
      /* frames on the GPU where the sink and the stream both can */
      bool hw       = p->sink.start_hw
         && image_transfer_anim_stream_set_hw(p->stream, p->type, true);
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
      if (r < 0)
         return VIDEO_PLAYER_PLAYING;
      p->flags = (p->flags & VIDEO_PLAYER_INLINE) | (hw ? VP_HW
         : ((unsigned)r & (VIDEO_PLAYER_RGBA | VIDEO_PLAYER_FP16)));
      /* told once, before any decode */
      image_transfer_anim_stream_set_want_fp16(p->stream, p->type,
            (p->flags & VIDEO_PLAYER_FP16) ? true : false);
#ifdef VIDEO_PLAYER_WORKER
      if (threaded)
      {
         /* all or nothing: asked again on a later poll */
         if (!vp_jobs_new(p))
            return VIDEO_PLAYER_PLAYING;
         p->flags |= VP_STARTED | VP_THREADED;
         return VIDEO_PLAYER_PLAYING;
      }
#endif
      p->flags |= VP_STARTED;
   }

#ifdef VIDEO_PLAYER_WORKER
   if (p->flags & VP_THREADED)
      return vp_poll_threaded(p, now_us);
#endif
   return vp_poll_inline(p, now_us);
}

int64_t video_player_due(const video_player_t *p)
{
   return p ? p->next_us : 0;
}

void video_player_slot_released(video_player_t *p, unsigned slot)
{
#ifdef VIDEO_PLAYER_WORKER
   vp_job_t *job;
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
      if (p->sink.slot_end && !(p->flags & VP_HW))
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
