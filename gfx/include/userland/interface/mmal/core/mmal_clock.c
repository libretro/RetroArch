/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_list.h"
#include "interface/mmal/util/mmal_util_rational.h"
#include "interface/mmal/core/mmal_clock_private.h"

#ifdef __VIDEOCORE__
/* Use RTOS timer for improved accuracy */
# include "vcfw/rtos/rtos.h"
# define USE_RTOS_TIMER
#endif

/*****************************************************************************/
#ifdef USE_RTOS_TIMER
# define MIN_TIMER_DELAY  1     /* microseconds */
#else
# define MIN_TIMER_DELAY  10000 /* microseconds */
#endif

/* 1.0 in Q16 format */
#define Q16_ONE  (1 << 16)

/* Maximum number of pending requests */
#define CLOCK_REQUEST_SLOTS  32

/* Number of microseconds the clock tries to service requests early
 * to account for processing overhead */
#define CLOCK_TARGET_OFFSET  20

/* Default wait time (in microseconds) when the clock is paused. */
#define CLOCK_WAIT_TIME  200000LL

/* In order to prevent unnecessary clock jitter when updating the local media-time of the
 * clock, an upper and lower threshold is used. If the difference between the reference
 * media-time and local media-time is greater than the upper threshold, local media-time
 * is set to the reference time. Below this threshold, a weighted moving average is applied
 * to the difference. If this is greater than the lower threshold, the local media-time is
 * adjusted by the average. Anything below the lower threshold is ignored. */
#define CLOCK_UPDATE_THRESHOLD_LOWER  8000   /* microseconds */
#define CLOCK_UPDATE_THRESHOLD_UPPER  50000  /* microseconds */

/* Default threshold after which backward jumps in media time are treated as a discontinuity. */
#define CLOCK_DISCONT_THRESHOLD  1000000  /* microseconds */

/* Default duration for which a discontinuity applies. Used for wall time duration for which
 * a discontinuity continues to cause affected requests to fire immediately, and as the media
 * time span for detecting discontinuous requests. */
#define CLOCK_DISCONT_DURATION   1000000  /* microseconds */

/* Absolute value macro */
#define ABS_VALUE(v)  (((v) < 0) ? -(v) : (v))

/* Macros used to make clock access thread-safe */
#define LOCK(p)    vcos_mutex_lock(&(p)->lock);
#define UNLOCK(p)  vcos_mutex_unlock(&(p)->lock);

/*****************************************************************************/
#ifdef USE_RTOS_TIMER
typedef RTOS_TIMER_T MMAL_TIMER_T;
#else
typedef VCOS_TIMER_T MMAL_TIMER_T;
#endif

typedef struct MMAL_CLOCK_REQUEST_T
{
   MMAL_LIST_ELEMENT_T link; /**< must be first */
   MMAL_CLOCK_VOID_FP priv;  /**< client-supplied function pointer */
   MMAL_CLOCK_REQUEST_CB cb; /**< client-supplied callback to invoke */
   void *cb_data;            /**< client-supplied callback data */
   int64_t media_time;       /**< media-time requested by the client (microseconds) */
   int64_t media_time_adj;   /**< adjusted media-time at which the request will
                                  be serviced in microseconds (this takes
                                  CLOCK_TARGET_OFFSET into account) */
} MMAL_CLOCK_REQUEST_T;

typedef struct MMAL_CLOCK_PRIVATE_T
{
   MMAL_CLOCK_T clock;        /**< must be first */

   MMAL_BOOL_T is_active;     /**< TRUE -> media-time is advancing */

   MMAL_BOOL_T scheduling;    /**< TRUE -> client request scheduling is enabled */
   MMAL_BOOL_T stop_thread;
   VCOS_SEMAPHORE_T event;
   VCOS_THREAD_T thread;      /**< processing thread for client requests */
   MMAL_TIMER_T timer;        /**< used for scheduling client requests */

   VCOS_MUTEX_T lock;         /**< lock access to the request lists */

   int32_t scale;             /**< media-time scale factor (Q16 format) */
   int32_t scale_inv;         /**< 1/scale (Q16 format) */
   MMAL_RATIONAL_T scale_rational;
                              /**< clock scale as a rational number; keep a copy since
                                   converting from Q16 will result in precision errors */

   int64_t  average_ref_diff; /**< media-time moving average adjustment */
   int64_t  media_time;       /**< current local media-time in microseconds */
   uint32_t media_time_frac;  /**< media-time fraction in microseconds (Q24 format) */
   int64_t  wall_time;        /**< current local wall-time (microseconds) */
   uint32_t rtc_at_update;    /**< real-time clock value at local time update (microseconds) */
   int64_t  media_time_at_timer;
                              /**< media-time when the timer was last set */

   int64_t  discont_expiry;   /**< wall-time when discontinuity expires; 0 = no discontinuity
                                   in effect */
   int64_t  discont_start;    /**< media-time at start of discontinuity
                                   (n/a if discont_expiry = 0) */
   int64_t  discont_end;      /**< media-time at end of discontinuity
                                   (n/a if discont_expiry = 0) */
   int64_t  discont_threshold;/**< Threshold after which backward jumps in media time are treated
                                   as a discontinuity  (microseconds) */
   int64_t  discont_duration; /**< Duration (wall-time) for which a discontinuity applies */

   int64_t  request_threshold;/**< Threshold after which frames exceeding the media-time are
                                   dropped (microseconds) */
   MMAL_BOOL_T request_threshold_enable;/**< Enable the request threshold */
   int64_t  update_threshold_lower;
                              /**< Time differences below this threshold are ignored */
   int64_t  update_threshold_upper;
                              /**< Time differences above this threshold reset media time */

   /* Client requests */
   struct
   {
      MMAL_LIST_T* list_free;
      MMAL_LIST_T* list_pending;
      MMAL_CLOCK_REQUEST_T pool[CLOCK_REQUEST_SLOTS];
   } request;

} MMAL_CLOCK_PRIVATE_T;

/*****************************************************************************/
static void mmal_clock_wake_thread(MMAL_CLOCK_PRIVATE_T *private);

/*****************************************************************************
 * Timer-specific functions
 *****************************************************************************/
/* Callback invoked when timer expires */
#ifdef USE_RTOS_TIMER
static void mmal_clock_timer_cb(MMAL_TIMER_T *timer, void *ctx)
{
   MMAL_PARAM_UNUSED(timer);
   /* Notify the worker thread */
   mmal_clock_wake_thread((MMAL_CLOCK_PRIVATE_T*)ctx);
}
#else
static void mmal_clock_timer_cb(void *ctx)
{
   /* Notify the worker thread */
   mmal_clock_wake_thread((MMAL_CLOCK_PRIVATE_T*)ctx);
}
#endif

/* Create a timer */
static inline MMAL_BOOL_T mmal_clock_timer_create(MMAL_TIMER_T *timer, void *ctx)
{
#ifdef USE_RTOS_TIMER
   return (rtos_timer_init(timer, mmal_clock_timer_cb, ctx) == 0);
#else
   return (vcos_timer_create(timer, "mmal-clock timer", mmal_clock_timer_cb, ctx) == VCOS_SUCCESS);
#endif
}

/* Destroy a timer */
static inline void mmal_clock_timer_destroy(MMAL_TIMER_T *timer)
{
#ifdef USE_RTOS_TIMER
   /* Nothing to do */
#else
   vcos_timer_delete(timer);
#endif
}

/* Set the timer. Delay is in microseconds. */
static inline void mmal_clock_timer_set(MMAL_TIMER_T *timer, int64_t delay_us)
{
#ifdef USE_RTOS_TIMER
   rtos_timer_set(timer, (RTOS_TIMER_TIME_T)delay_us);
#else
   /* VCOS timer only provides millisecond accuracy */
   vcos_timer_set(timer, (VCOS_UNSIGNED)(delay_us / 1000));
#endif
}

/* Stop the timer. */
static inline void mmal_clock_timer_cancel(MMAL_TIMER_T *timer)
{
#ifdef USE_RTOS_TIMER
   rtos_timer_cancel(timer);
#else
   vcos_timer_cancel(timer);
#endif
}

/*****************************************************************************
 * Clock module private functions
 *****************************************************************************/
/* Update the internal wall-time and media-time */
static void mmal_clock_update_local_time_locked(MMAL_CLOCK_PRIVATE_T *private)
{
   uint32_t time_now = vcos_getmicrosecs();
   uint32_t time_diff = (time_now > private->rtc_at_update) ? (time_now - private->rtc_at_update) : 0;

   private->wall_time += time_diff;

   /* For small clock scale values (i.e. slow motion), the media-time increment
    * could potentially be rounded down when doing lots of updates, so also keep
    * track of the fractional increment. */
   int64_t media_diff = ((int64_t)time_diff) * (int64_t)(private->scale << 8) + private->media_time_frac;

   private->media_time += media_diff >> 24;
   private->media_time_frac = media_diff & ((1<<24)-1);

   private->rtc_at_update = time_now;
}

/* Return the current local media-time */
static int64_t mmal_clock_media_time_get_locked(MMAL_CLOCK_PRIVATE_T *private)
{
   mmal_clock_update_local_time_locked(private);
   return private->media_time;
}

/* Comparison function used for inserting a request into
 * the list of pending requests when clock scale is positive. */
static int mmal_clock_request_compare_pos(MMAL_LIST_ELEMENT_T *lhs, MMAL_LIST_ELEMENT_T *rhs)
{
   return ((MMAL_CLOCK_REQUEST_T*)lhs)->media_time_adj < ((MMAL_CLOCK_REQUEST_T*)rhs)->media_time_adj;
}

/* Comparison function used for inserting a request into
 * the list of pending requests when clock scale is negative. */
static int mmal_clock_request_compare_neg(MMAL_LIST_ELEMENT_T *lhs, MMAL_LIST_ELEMENT_T *rhs)
{
   return ((MMAL_CLOCK_REQUEST_T*)lhs)->media_time_adj > ((MMAL_CLOCK_REQUEST_T*)rhs)->media_time_adj;
}

/* Insert a new request into the list of pending requests */
static MMAL_BOOL_T mmal_clock_request_insert(MMAL_CLOCK_PRIVATE_T *private, MMAL_CLOCK_REQUEST_T *request)
{
   MMAL_LIST_T *list = private->request.list_pending;
   MMAL_CLOCK_REQUEST_T *pending;

   if (private->stop_thread)
      return MMAL_FALSE; /* the clock is being destroyed */

   if (list->length == 0)
   {
      mmal_list_push_front(list, &request->link);
      return MMAL_TRUE;
   }

   /* It is more likely for requests to be received in sequence,
    * so try adding to the back of the list first before doing
    * a more expensive list insert. */
   pending = (MMAL_CLOCK_REQUEST_T*)list->last;
   if ((private->scale >= 0 && (request->media_time_adj >= pending->media_time_adj)) ||
       (private->scale <  0 && (request->media_time_adj <= pending->media_time_adj)))
   {
      mmal_list_push_back(list, &request->link);
   }
   else
   {
      mmal_list_insert(list, &request->link,
            (private->scale >= 0) ? mmal_clock_request_compare_pos : mmal_clock_request_compare_neg);
   }
   return MMAL_TRUE;
}

/* Flush all pending requests */
static MMAL_STATUS_T mmal_clock_request_flush_locked(MMAL_CLOCK_PRIVATE_T *private,
                                                     int64_t media_time)
{
   MMAL_LIST_T *pending = private->request.list_pending;
   MMAL_LIST_T *list_free = private->request.list_free;
   MMAL_CLOCK_REQUEST_T *request;

   while ((request = (MMAL_CLOCK_REQUEST_T *)mmal_list_pop_front(pending)) != NULL)
   {
      /* Inform the client */
      request->cb(&private->clock, media_time, request->cb_data, request->priv);
      /* Recycle request slot */
      mmal_list_push_back(list_free, &request->link);
   }

   private->media_time_at_timer = 0;

   return MMAL_SUCCESS;
}

/* Process all pending requests */
static void mmal_clock_process_requests(MMAL_CLOCK_PRIVATE_T *private)
{
   int64_t media_time_now;
   MMAL_LIST_T* free = private->request.list_free;
   MMAL_LIST_T* pending = private->request.list_pending;
   MMAL_CLOCK_REQUEST_T *next;

   if (pending->length == 0 || !private->is_active)
      return;

   LOCK(private);

   /* Detect discontinuity */
   if (private->media_time_at_timer != 0)
   {
      media_time_now = mmal_clock_media_time_get_locked(private);
      /* Currently only applied to forward speeds */
      if (private->scale > 0 &&
          media_time_now + private->discont_threshold < private->media_time_at_timer)
      {
         LOG_INFO("discontinuity: was=%" PRIi64 " now=%" PRIi64 " pending=%d",
                  private->media_time_at_timer, media_time_now, pending->length);

         /* It's likely that packets from before the discontinuity will continue to arrive for
          * a short time. Ensure these are detected and the requests fired immediately. */
         private->discont_start = private->media_time_at_timer;
         private->discont_end = private->discont_start + private->discont_duration;
         private->discont_expiry = private->wall_time + private->discont_duration;

         /* Fire all pending requests */
         mmal_clock_request_flush_locked(private, media_time_now);
      }
   }

   /* Earliest request is always at the front */
   next = (MMAL_CLOCK_REQUEST_T*)mmal_list_pop_front(pending);
   while (next)
   {
      media_time_now = mmal_clock_media_time_get_locked(private);

      if (private->discont_expiry != 0 && private->wall_time > private->discont_expiry)
         private->discont_expiry = 0;

      /* Fire the request if it matches the pending discontinuity or if its requested media time
       * has been reached. */
      if ((private->discont_expiry != 0 &&
           next->media_time_adj >= private->discont_start &&
           next->media_time_adj < private->discont_end) ||
          (private->scale > 0 && ((media_time_now + MIN_TIMER_DELAY) >= next->media_time_adj)) ||
          (private->scale < 0 && ((media_time_now - MIN_TIMER_DELAY) <= next->media_time_adj)))
      {
         LOG_TRACE("servicing request: next %"PRIi64" now %"PRIi64, next->media_time_adj, media_time_now);
         /* Inform the client */
         next->cb(&private->clock, media_time_now, next->cb_data, next->priv);
         /* Recycle the request slot */
         mmal_list_push_back(free, &next->link);
         /* Move onto next pending request */
         next = (MMAL_CLOCK_REQUEST_T*)mmal_list_pop_front(pending);
      }
      else
      {
         /* The next request is in the future, so re-schedule the
          * timer based on the current clock scale and media-time diff */
         int64_t media_time_delay = ABS_VALUE(media_time_now - next->media_time_adj);
         int64_t wall_time_delay = ABS_VALUE(((int64_t)private->scale_inv * media_time_delay) >> 16);

         if (private->scale == 0)
            wall_time_delay = CLOCK_WAIT_TIME; /* Clock is paused */

         /* Put next request back into pending list */
         mmal_list_push_front(pending, &next->link);
         next = NULL;

         /* Set the timer */
         private->media_time_at_timer = media_time_now;
         mmal_clock_timer_set(&private->timer, wall_time_delay);

         LOG_TRACE("re-schedule timer: now %"PRIi64" delay %"PRIi64, media_time_now, wall_time_delay);
      }
   }

   UNLOCK(private);
}

/* Trigger the worker thread (if present) */
static void mmal_clock_wake_thread(MMAL_CLOCK_PRIVATE_T *private)
{
   if (private->scheduling)
      vcos_semaphore_post(&private->event);
}

/* Stop the worker thread */
static void mmal_clock_stop_thread(MMAL_CLOCK_PRIVATE_T *private)
{
   private->stop_thread = MMAL_TRUE;
   mmal_clock_wake_thread(private);
   vcos_thread_join(&private->thread, NULL);
}

/* Main processing thread */
static void* mmal_clock_worker_thread(void *ctx)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)ctx;

   while (1)
   {
      vcos_semaphore_wait(&private->event);

      /* Either the timer has expired or a new request is pending */
      mmal_clock_timer_cancel(&private->timer);

      if (private->stop_thread)
         break;

      mmal_clock_process_requests(private);
   }
   return NULL;
}

/* Create scheduling resources */
static MMAL_STATUS_T mmal_clock_create_scheduling(MMAL_CLOCK_PRIVATE_T *private)
{
   unsigned int i;
   MMAL_BOOL_T timer_status = MMAL_FALSE;
   VCOS_STATUS_T event_status = VCOS_EINVAL;
   VCOS_UNSIGNED priority;

   timer_status = mmal_clock_timer_create(&private->timer, private);
   if (!timer_status)
   {
      LOG_ERROR("failed to create timer %p", private);
      goto error;
   }

   event_status = vcos_semaphore_create(&private->event, "mmal-clock sema", 0);
   if (event_status != VCOS_SUCCESS)
   {
      LOG_ERROR("failed to create event semaphore %d", event_status);
      goto error;
   }

   private->request.list_free = mmal_list_create();
   private->request.list_pending = mmal_list_create();
   if (!private->request.list_free || !private->request.list_pending)
   {
      LOG_ERROR("failed to create list %p %p", private->request.list_free, private->request.list_pending);
      goto error;
   }

   /* Populate the list of available request slots */
   for (i = 0; i < CLOCK_REQUEST_SLOTS; ++i)
      mmal_list_push_back(private->request.list_free, &private->request.pool[i].link);

   if (vcos_thread_create(&private->thread, "mmal-clock thread", NULL,
                          mmal_clock_worker_thread, private) != VCOS_SUCCESS)
   {
      LOG_ERROR("failed to create worker thread");
      goto error;
   }
   priority = vcos_thread_get_priority(&private->thread);
   vcos_thread_set_priority(&private->thread, 1 | (priority & VCOS_AFFINITY_MASK));

   private->scheduling = MMAL_TRUE;

   return MMAL_SUCCESS;

error:
   if (event_status == VCOS_SUCCESS) vcos_semaphore_delete(&private->event);
   if (timer_status) mmal_clock_timer_destroy(&private->timer);
   if (private->request.list_free) mmal_list_destroy(private->request.list_free);
   if (private->request.list_pending) mmal_list_destroy(private->request.list_pending);
   return MMAL_ENOSPC;
}

/* Destroy all scheduling resources */
static void mmal_clock_destroy_scheduling(MMAL_CLOCK_PRIVATE_T *private)
{
   mmal_clock_stop_thread(private);

   mmal_clock_request_flush(&private->clock);

   mmal_list_destroy(private->request.list_free);
   mmal_list_destroy(private->request.list_pending);

   vcos_semaphore_delete(&private->event);

   mmal_clock_timer_destroy(&private->timer);
}

/* Start the media-time */
static void mmal_clock_start(MMAL_CLOCK_T *clock)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;

   private->is_active = MMAL_TRUE;

   mmal_clock_wake_thread(private);
}

/* Stop the media-time */
static void mmal_clock_stop(MMAL_CLOCK_T *clock)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;

   private->is_active = MMAL_FALSE;

   mmal_clock_wake_thread(private);
}

static int mmal_clock_is_paused(MMAL_CLOCK_T *clock)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;
   return private->scale == 0;
}

/*****************************************************************************
 * Clock module public functions
 *****************************************************************************/
/* Create new clock instance */
MMAL_STATUS_T mmal_clock_create(MMAL_CLOCK_T **clock)
{
   unsigned int size = sizeof(MMAL_CLOCK_PRIVATE_T);
   MMAL_RATIONAL_T scale = { 1, 1 };
   MMAL_CLOCK_PRIVATE_T *private;

   /* Sanity checking */
   if (clock == NULL)
      return MMAL_EINVAL;

   private = vcos_calloc(1, size, "mmal-clock");
   if (!private)
   {
      LOG_ERROR("failed to allocate memory");
      return MMAL_ENOMEM;
   }

   if (vcos_mutex_create(&private->lock, "mmal-clock lock") != VCOS_SUCCESS)
   {
      LOG_ERROR("failed to create lock mutex");
      vcos_free(private);
      return MMAL_ENOSPC;
   }

   /* Set the default threshold values */
   private->update_threshold_lower = CLOCK_UPDATE_THRESHOLD_LOWER;
   private->update_threshold_upper = CLOCK_UPDATE_THRESHOLD_UPPER;
   private->discont_threshold      = CLOCK_DISCONT_THRESHOLD;
   private->discont_duration       = CLOCK_DISCONT_DURATION;
   private->request_threshold      = 0;
   private->request_threshold_enable = MMAL_FALSE;

   /* Default scale = 1.0, i.e. normal playback speed */
   mmal_clock_scale_set(&private->clock, scale);

   *clock = &private->clock;
   return MMAL_SUCCESS;
}

/* Destroy a clock instance */
MMAL_STATUS_T mmal_clock_destroy(MMAL_CLOCK_T *clock)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;

   if (private->scheduling)
      mmal_clock_destroy_scheduling(private);

   vcos_mutex_delete(&private->lock);

   vcos_free(private);

   return MMAL_SUCCESS;
}

/* Add new client request to list of pending requests */
MMAL_STATUS_T mmal_clock_request_add(MMAL_CLOCK_T *clock, int64_t media_time,
      MMAL_CLOCK_REQUEST_CB cb, void *cb_data, MMAL_CLOCK_VOID_FP priv)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;
   MMAL_CLOCK_REQUEST_T *request;
   MMAL_BOOL_T wake_thread = MMAL_FALSE;
   int64_t media_time_now;

   LOG_TRACE("media time %"PRIi64, media_time);

   LOCK(private);

   media_time_now = mmal_clock_media_time_get_locked(private);

   /* Drop the request if request_threshold_enable and the frame exceeds the request threshold */
   if (private->request_threshold_enable && (media_time > (media_time_now + private->request_threshold)))
   {
      LOG_TRACE("dropping request: media time %"PRIi64" now %"PRIi64, media_time, media_time_now);
      UNLOCK(private);
      return MMAL_ECORRUPT;
   }

   /* The clock module is usually only used for time-keeping, so all the
    * objects needed to process client requests are not allocated by default
    * and need to be created on the first client request received */
   if (!private->scheduling)
   {
      if (mmal_clock_create_scheduling(private) != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to create scheduling objects");
         UNLOCK(private);
         return MMAL_ENOSPC;
      }
   }

   request = (MMAL_CLOCK_REQUEST_T*)mmal_list_pop_front(private->request.list_free);
   if (request == NULL)
   {
      LOG_ERROR("no more free clock request slots");
      UNLOCK(private);
      return MMAL_ENOSPC;
   }

   request->cb = cb;
   request->cb_data = cb_data;
   request->priv = priv;
   request->media_time = media_time;
   request->media_time_adj = media_time - (int64_t)(private->scale * CLOCK_TARGET_OFFSET >> 16);

   if (mmal_clock_request_insert(private, request))
      wake_thread = private->is_active;

   UNLOCK(private);

   /* Notify the worker thread */
   if (wake_thread)
      mmal_clock_wake_thread(private);

   return MMAL_SUCCESS;
}

/* Flush all pending requests */
MMAL_STATUS_T mmal_clock_request_flush(MMAL_CLOCK_T *clock)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;

   LOCK(private);
   if (private->scheduling)
      mmal_clock_request_flush_locked(private, MMAL_TIME_UNKNOWN);
   UNLOCK(private);

   return MMAL_SUCCESS;
}

/* Update the local media-time with the given reference */
MMAL_STATUS_T mmal_clock_media_time_set(MMAL_CLOCK_T *clock, int64_t media_time)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;
   MMAL_BOOL_T wake_thread = MMAL_TRUE;
   int64_t time_diff;

   LOCK(private);

   if (!private->is_active)
   {
      uint32_t time_now = vcos_getmicrosecs();
      private->wall_time = time_now;
      private->media_time = media_time;
      private->media_time_frac = 0;
      private->rtc_at_update = time_now;

      UNLOCK(private);
      return MMAL_SUCCESS;
   }

   if (mmal_clock_is_paused(clock))
   {
      LOG_TRACE("clock is paused; ignoring update");
      UNLOCK(private);
      return MMAL_SUCCESS;
   }

   /* Reset the local media-time with the given time reference */
   mmal_clock_update_local_time_locked(private);

   time_diff = private->media_time - media_time;
   if (time_diff >  private->update_threshold_upper ||
       time_diff < -private->update_threshold_upper)
   {
      LOG_TRACE("cur:%"PRIi64" new:%"PRIi64" diff:%"PRIi64, private->media_time, media_time, time_diff);
      private->media_time = media_time;
      private->average_ref_diff = 0;
   }
   else
   {
      private->average_ref_diff = ((private->average_ref_diff << 6) - private->average_ref_diff + time_diff) >> 6;
      if(private->average_ref_diff >  private->update_threshold_lower ||
         private->average_ref_diff < -private->update_threshold_lower)
      {
         LOG_TRACE("cur:%"PRIi64" new:%"PRIi64" ave:%"PRIi64, private->media_time,
               private->media_time - private->average_ref_diff, private->average_ref_diff);
         private->media_time -= private->average_ref_diff;
         private->average_ref_diff = 0;
      }
      else
      {
         /* Don't update the media-time */
         wake_thread = MMAL_FALSE;
         LOG_TRACE("cur:%"PRIi64" new:%"PRIi64" diff:%"PRIi64" ave:%"PRIi64" ignored", private->media_time,
               media_time, private->media_time - media_time, private->average_ref_diff);
      }
   }

   UNLOCK(private);

   if (wake_thread)
      mmal_clock_wake_thread(private);

   return MMAL_SUCCESS;
}

/* Change the clock scale */
MMAL_STATUS_T mmal_clock_scale_set(MMAL_CLOCK_T *clock, MMAL_RATIONAL_T scale)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;

   LOG_TRACE("new scale %d/%d", scale.num, scale.den);

   LOCK(private);

   mmal_clock_update_local_time_locked(private);

   private->scale_rational = scale;
   private->scale = mmal_rational_to_fixed_16_16(scale);

   if (private->scale)
      private->scale_inv = (int32_t)((1LL << 32) / (int64_t)private->scale);
   else
      private->scale_inv = Q16_ONE; /* clock is paused */

   UNLOCK(private);

   mmal_clock_wake_thread(private);

   return MMAL_SUCCESS;
}

/* Set the clock state */
MMAL_STATUS_T mmal_clock_active_set(MMAL_CLOCK_T *clock, MMAL_BOOL_T active)
{
   if (active)
      mmal_clock_start(clock);
   else
      mmal_clock_stop(clock);

   return MMAL_SUCCESS;
}

/* Get the clock's scale */
MMAL_RATIONAL_T mmal_clock_scale_get(MMAL_CLOCK_T *clock)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;
   MMAL_RATIONAL_T scale;

   LOCK(private);
   scale = private->scale_rational;
   UNLOCK(private);

   return scale;
}

/* Return the current local media-time */
int64_t mmal_clock_media_time_get(MMAL_CLOCK_T *clock)
{
   int64_t media_time;
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T*)clock;

   LOCK(private);
   media_time = mmal_clock_media_time_get_locked(private);
   UNLOCK(private);

   return media_time;
}

/* Get the clock's state */
MMAL_BOOL_T mmal_clock_is_active(MMAL_CLOCK_T *clock)
{
   return ((MMAL_CLOCK_PRIVATE_T*)clock)->is_active;
}

/* Get the clock's media-time update threshold values */
MMAL_STATUS_T mmal_clock_update_threshold_get(MMAL_CLOCK_T *clock, MMAL_CLOCK_UPDATE_THRESHOLD_T *update_threshold)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T *)clock;

   LOCK(private);
   update_threshold->threshold_lower = private->update_threshold_lower;
   update_threshold->threshold_upper = private->update_threshold_upper;
   UNLOCK(private);

   return MMAL_SUCCESS;
}

/* Set the clock's media-time update threshold values */
MMAL_STATUS_T mmal_clock_update_threshold_set(MMAL_CLOCK_T *clock, const MMAL_CLOCK_UPDATE_THRESHOLD_T *update_threshold)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T *)clock;

   LOG_TRACE("new clock update thresholds: upper %"PRIi64", lower %"PRIi64,
         update_threshold->threshold_lower, update_threshold->threshold_upper);

   LOCK(private);
   private->update_threshold_lower = update_threshold->threshold_lower;
   private->update_threshold_upper = update_threshold->threshold_upper;
   UNLOCK(private);

   return MMAL_SUCCESS;
}

/* Get the clock's discontinuity threshold values */
MMAL_STATUS_T mmal_clock_discont_threshold_get(MMAL_CLOCK_T *clock, MMAL_CLOCK_DISCONT_THRESHOLD_T *discont)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T *)clock;

   LOCK(private);
   discont->threshold = private->discont_threshold;
   discont->duration  = private->discont_duration;
   UNLOCK(private);

   return MMAL_SUCCESS;
}

/* Set the clock's discontinuity threshold values */
MMAL_STATUS_T mmal_clock_discont_threshold_set(MMAL_CLOCK_T *clock, const MMAL_CLOCK_DISCONT_THRESHOLD_T *discont)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T *)clock;

   LOG_TRACE("new clock discontinuity values: threshold %"PRIi64", duration %"PRIi64,
         discont->threshold, discont->duration);

   LOCK(private);
   private->discont_threshold = discont->threshold;
   private->discont_duration  = discont->duration;
   UNLOCK(private);

   return MMAL_SUCCESS;
}

/* Get the clock's request threshold values */
MMAL_STATUS_T mmal_clock_request_threshold_get(MMAL_CLOCK_T *clock, MMAL_CLOCK_REQUEST_THRESHOLD_T *req)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T *)clock;

   LOCK(private);
   req->threshold = private->request_threshold;
   req->threshold_enable = private->request_threshold_enable;
   UNLOCK(private);

   return MMAL_SUCCESS;
}

/* Set the clock's request threshold values */
MMAL_STATUS_T mmal_clock_request_threshold_set(MMAL_CLOCK_T *clock, const MMAL_CLOCK_REQUEST_THRESHOLD_T *req)
{
   MMAL_CLOCK_PRIVATE_T *private = (MMAL_CLOCK_PRIVATE_T *)clock;

   LOG_TRACE("new clock request values: threshold %"PRIi64,
         req->threshold);

   LOCK(private);
   private->request_threshold = req->threshold;
   private->request_threshold_enable = req->threshold_enable;
   UNLOCK(private);

   return MMAL_SUCCESS;
}
