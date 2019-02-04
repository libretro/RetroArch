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

#include <stddef.h>
#include "applog.h"
#include "interface/vcos/vcos_stdbool.h"
#include "interface/vcos/vcos_inttypes.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "svp.h"

#define CHECK_STATUS(s, m) \
         if ((s) != MMAL_SUCCESS) { \
            LOG_ERROR("%s: %s", (m), mmal_status_to_string((s))); \
            goto error; \
         }

/* Flags specifying fields of SVP_T struct */
#define SVP_CREATED_SEM       (1 << 0)
#define SVP_CREATED_THREAD    (1 << 1)
#define SVP_CREATED_MUTEX     (1 << 2)
#define SVP_CREATED_TIMER     (1 << 3)
#define SVP_CREATED_WD_TIMER  (1 << 4)

/* Hard-coded camera parameters */
#if 0
#define SVP_CAMERA_WIDTH         1920
#define SVP_CAMERA_HEIGHT        1080
#else
#define SVP_CAMERA_WIDTH         1280
#define SVP_CAMERA_HEIGHT         720
#endif
#define SVP_CAMERA_FRAMERATE       30
#define SVP_CAMERA_DURATION_MS  10000

/* Watchdog timeout - elapsed time to allow for no video frames received */
#define SVP_WATCHDOG_TIMEOUT_MS  5000

/** Simple video player instance */
struct SVP_T
{
   /* Player options */
   SVP_OPTS_T opts;

   /* Bitmask of SVP_CREATED_XXX values indicating which fields have been created.
    * Only used for those fields which can't be (portably) determined from their
    * own value.
    */
   uint32_t created;

   /* Semaphore used for synchronising buffer handling for decoded frames */
   VCOS_SEMAPHORE_T sema;

   /* User supplied callbacks */
   SVP_CALLBACKS_T callbacks;

   /* Container reader component */
   MMAL_COMPONENT_T *reader;

   /* Video decoder component */
   MMAL_COMPONENT_T *video_decode;

   /* Camera component */
   MMAL_COMPONENT_T *camera;

   /* Connection: container reader -> video decoder */
   MMAL_CONNECTION_T *connection;

   /* Output port from video decoder or camera */
   MMAL_PORT_T *video_output;

   /* Pool of buffers for video decoder output */
   MMAL_POOL_T *out_pool;

   /* Queue to hold decoded video frames */
   MMAL_QUEUE_T *queue;

   /* Worker thread */
   VCOS_THREAD_T thread;

   /* Timer to trigger stop in camera preview case */
   VCOS_TIMER_T timer;

   /* Watchdog timer */
   VCOS_TIMER_T wd_timer;

   /* Mutex to synchronise access to all following fields */
   VCOS_MUTEX_T mutex;

   /* Stop control: 0 to process stream; bitmask of SVP_STOP_XXX values to stop */
   uint32_t stop;

   /* Player stats */
   SVP_STATS_T stats;
};

/* Local function prototypes */
static MMAL_STATUS_T svp_setup_reader(MMAL_COMPONENT_T *reader, const char *uri,
                                      MMAL_PORT_T **video_port);
static void svp_timer_cb(void *ctx);
static void svp_watchdog_cb(void *ctx);
static MMAL_STATUS_T svp_port_enable(SVP_T *svp, MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb);
static void *svp_worker(void *arg);
static void svp_bh_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void svp_bh_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void svp_reset_stop(SVP_T *svp);
static void svp_set_stop(SVP_T *svp, uint32_t stop_flags);
static uint32_t svp_get_stop(SVP_T *svp);

/* Create Simple Video Player instance. */
SVP_T *svp_create(const char *uri, SVP_CALLBACKS_T *callbacks, const SVP_OPTS_T *opts)
{
   SVP_T *svp;
   MMAL_STATUS_T st;
   VCOS_STATUS_T vst;
   MMAL_PORT_T *reader_output = NULL;
   MMAL_COMPONENT_T *video_decode = NULL;
   MMAL_PORT_T *video_output = NULL;

   LOG_TRACE("Creating player for %s", (uri ? uri : "camera preview"));

   vcos_assert(callbacks->video_frame_cb);
   vcos_assert(callbacks->stop_cb);

   svp = vcos_calloc(1, sizeof(*svp), "svp");
   CHECK_STATUS((svp ? MMAL_SUCCESS : MMAL_ENOMEM), "Failed to allocate context");

   svp->opts = *opts;
   svp->callbacks = *callbacks;

   /* Semaphore used for synchronising buffer handling for decoded frames */
   vst = vcos_semaphore_create(&svp->sema, "svp-sem", 0);
   CHECK_STATUS((vst == VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOMEM), "Failed to create semaphore");
   svp->created |= SVP_CREATED_SEM;

   vst = vcos_mutex_create(&svp->mutex, "svp-mutex");
   CHECK_STATUS((vst == VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOMEM), "Failed to create mutex");
   svp->created |= SVP_CREATED_MUTEX;

   vst = vcos_timer_create(&svp->timer, "svp-timer", svp_timer_cb, svp);
   CHECK_STATUS((vst == VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOMEM), "Failed to create timer");
   svp->created |= SVP_CREATED_TIMER;

   vst = vcos_timer_create(&svp->wd_timer, "svp-wd-timer", svp_watchdog_cb, svp);
   CHECK_STATUS((vst == VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOMEM), "Failed to create timer");
   svp->created |= SVP_CREATED_WD_TIMER;

   /* Create components */
   svp->reader = NULL;
   svp->video_decode = NULL;
   svp->camera = NULL;
   svp->connection = NULL;

   if (uri)
   {
      /* Video from URI: setup container_reader -> video_decode */

      /* Create and set up container reader */
      st = mmal_component_create(MMAL_COMPONENT_DEFAULT_CONTAINER_READER, &svp->reader);
      CHECK_STATUS(st, "Failed to create container reader");

      st = svp_setup_reader(svp->reader, uri, &reader_output);
      if (st != MMAL_SUCCESS)
         goto error;

      st = mmal_component_enable(svp->reader);
      CHECK_STATUS(st, "Failed to enable container reader");

      st = svp_port_enable(svp, svp->reader->control, svp_bh_control_cb);
      CHECK_STATUS(st, "Failed to enable container reader control port");

      /* Create and set up video decoder */
      st = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &svp->video_decode);
      CHECK_STATUS(st, "Failed to create video decoder");

      video_decode = svp->video_decode;
      video_output = video_decode->output[0];

      st = mmal_component_enable(video_decode);
      CHECK_STATUS(st, "Failed to enable video decoder");

      st = svp_port_enable(svp, video_decode->control, svp_bh_control_cb);
      CHECK_STATUS(st, "Failed to enable video decoder control port");
   }
   else
   {
      /* Camera preview */
      st = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &svp->camera);
      CHECK_STATUS(st, "Failed to create camera");

      st = mmal_component_enable(svp->camera);
      CHECK_STATUS(st, "Failed to enable camera");

      st = svp_port_enable(svp, svp->camera->control, svp_bh_control_cb);
      CHECK_STATUS(st, "Failed to enable camera control port");

      video_output = svp->camera->output[0]; /* Preview port */
   }

   st = mmal_port_parameter_set_boolean(video_output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
   CHECK_STATUS((st == MMAL_ENOSYS ? MMAL_SUCCESS : st), "Failed to enable zero copy");

   if (uri)
   {
      /* Create connection: container_reader -> video_decoder */
      st = mmal_connection_create(&svp->connection, reader_output, video_decode->input[0],
                                  MMAL_CONNECTION_FLAG_TUNNELLING);
      CHECK_STATUS(st, "Failed to create connection");
   }

   /* Set video output port format.
    * Opaque encoding ensures we get buffer data as handles to relocatable heap. */
   video_output->format->encoding = MMAL_ENCODING_OPAQUE;

   if (!uri)
   {
      /* Set video format for camera preview */
      MMAL_VIDEO_FORMAT_T *vfmt = &video_output->format->es->video;

      CHECK_STATUS((video_output->format->type == MMAL_ES_TYPE_VIDEO) ? MMAL_SUCCESS : MMAL_EINVAL,
                   "Output port isn't video format");

      vfmt->width = SVP_CAMERA_WIDTH;
      vfmt->height = SVP_CAMERA_HEIGHT;
      vfmt->crop.x = 0;
      vfmt->crop.y = 0;
      vfmt->crop.width = vfmt->width;
      vfmt->crop.height = vfmt->height;
      vfmt->frame_rate.num = SVP_CAMERA_FRAMERATE;
      vfmt->frame_rate.den = 1;
   }

   st = mmal_port_format_commit(video_output);
   CHECK_STATUS(st, "Failed to set output port format");

   /* Finally, set buffer num/size. N.B. For container_reader/video_decode, must be after
    * connection created, in order for port format to propagate.
    * Don't enable video output port until want to produce frames. */
   video_output->buffer_num = video_output->buffer_num_recommended;
   video_output->buffer_size = video_output->buffer_size_recommended;

   /* Pool + queue to hold decoded video frames */
   svp->out_pool = mmal_port_pool_create(video_output, video_output->buffer_num,
                                         video_output->buffer_size);
   CHECK_STATUS((svp->out_pool ? MMAL_SUCCESS : MMAL_ENOMEM), "Error allocating pool");
   svp->queue = mmal_queue_create();
   CHECK_STATUS((svp ? MMAL_SUCCESS : MMAL_ENOMEM), "Error allocating queue");

   svp->video_output = video_output;

   return svp;

error:
   svp_destroy(svp);
   return NULL;
}

/**
 * Setup container reader component.
 * Sets URI, to initialize processing, and finds a video track.
 * @param reader      Container reader component.
 * @param uri         Media URI.
 * @param video_port  On success, the output port for the first video track is returned here.
 * @return MMAL_SUCCESS if the container reader was successfully set up and a video track located.
 */
static MMAL_STATUS_T svp_setup_reader(MMAL_COMPONENT_T *reader, const char *uri,
                                      MMAL_PORT_T **video_port)
{
   MMAL_STATUS_T st;
   uint32_t track;

   st = mmal_util_port_set_uri(reader->control, uri);
   if (st != MMAL_SUCCESS)
   {
      LOG_ERROR("%s: couldn't open uri %s", reader->name, uri);
      return st;
   }

   /* Find a video track */
   for (track = 0; track < reader->output_num; track++)
   {
      if (reader->output[track]->format->type == MMAL_ES_TYPE_VIDEO)
      {
         break;
      }
   }

   if (track == reader->output_num)
   {
      LOG_ERROR("%s: no video track", uri);
      return MMAL_EINVAL;
   }

   *video_port = reader->output[track];
   return MMAL_SUCCESS;
}

/* Destroy SVP instance. svp may be NULL. */
void svp_destroy(SVP_T *svp)
{
   if (svp)
   {
      MMAL_COMPONENT_T *components[] = { svp->reader, svp->video_decode, svp->camera };
      MMAL_COMPONENT_T **comp;

      /* Stop thread, disable connection and components */
      svp_stop(svp);

      for (comp = components; comp < components + vcos_countof(components); comp++)
      {
         mmal_component_disable(*comp);
      }

      /* Destroy connection + components */
      if (svp->connection)
      {
         mmal_connection_destroy(svp->connection);
      }

      for (comp = components; comp < components + vcos_countof(components); comp++)
      {
         mmal_component_destroy(*comp);
      }

      /* Free remaining resources */
      if (svp->out_pool)
      {
         mmal_pool_destroy(svp->out_pool);
      }

      if (svp->queue)
      {
         mmal_queue_destroy(svp->queue);
      }

      if (svp->created & SVP_CREATED_WD_TIMER)
      {
         vcos_timer_delete(&svp->wd_timer);
      }

      if (svp->created & SVP_CREATED_TIMER)
      {
         vcos_timer_delete(&svp->timer);
      }

      if (svp->created & SVP_CREATED_MUTEX)
      {
         vcos_mutex_delete(&svp->mutex);
      }

      if (svp->created & SVP_CREATED_SEM)
      {
         vcos_semaphore_delete(&svp->sema);
      }

      vcos_free(svp);
   }
}

/* Start SVP. Enables MMAL connection + creates worker thread. */
int svp_start(SVP_T *svp)
{
   MMAL_STATUS_T st;
   VCOS_STATUS_T vst;

   /* Ensure SVP is stopped first */
   svp_stop(svp);

   /* Reset the worker thread stop status, before enabling ports that might trigger a stop */
   svp_reset_stop(svp);

   if (svp->connection)
   {
      /* Enable reader->decoder connection */
      st = mmal_connection_enable(svp->connection);
      CHECK_STATUS(st, "Failed to create connection");
   }

   /* Enable video output port */
   st = svp_port_enable(svp, svp->video_output, svp_bh_output_cb);
   CHECK_STATUS(st, "Failed to enable output port");

   /* Reset stats */
   svp->stats.video_frame_count = 0;

   /* Create worker thread */
   vst = vcos_thread_create(&svp->thread, "svp-worker", NULL, svp_worker, svp);
   CHECK_STATUS((vst == VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOMEM), "Failed to create connection");

   svp->created |= SVP_CREATED_THREAD;

   /* Set timer */
   if (svp->camera)
   {
      unsigned ms = svp->opts.duration_ms;
      vcos_timer_set(&svp->timer, ((ms == 0) ? SVP_CAMERA_DURATION_MS : ms));
   }

   /* Start watchdog timer */
   vcos_timer_set(&svp->wd_timer, SVP_WATCHDOG_TIMEOUT_MS);

   return 0;

error:
   return -1;
}

/* Stop SVP. Stops worker thread + disables MMAL connection. */
void svp_stop(SVP_T *svp)
{
   vcos_timer_cancel(&svp->wd_timer);
   vcos_timer_cancel(&svp->timer);

   /* Stop worker thread */
   if (svp->created & SVP_CREATED_THREAD)
   {
      svp_set_stop(svp, SVP_STOP_USER);
      vcos_semaphore_post(&svp->sema);
      vcos_thread_join(&svp->thread, NULL);
      svp->created &= ~SVP_CREATED_THREAD;
   }

   if (svp->connection)
   {
      mmal_connection_disable(svp->connection);
   }

   mmal_port_disable(svp->video_output);
}

/* Get stats since last call to svp_start() */
void svp_get_stats(SVP_T *svp, SVP_STATS_T *stats)
{
   vcos_mutex_lock(&svp->mutex);
   *stats = svp->stats;
   vcos_mutex_unlock(&svp->mutex);
}

/** Timer callback - stops playback */
static void svp_timer_cb(void *ctx)
{
   SVP_T *svp = ctx;
   svp_set_stop(svp, SVP_STOP_TIMEUP);
   vcos_semaphore_post(&svp->sema);
}

/** Watchdog timer callback - stops playback */
static void svp_watchdog_cb(void *ctx)
{
   SVP_T *svp = ctx;
   LOG_ERROR("%s: no frames received for %d ms, aborting", svp->video_output->name,
             SVP_WATCHDOG_TIMEOUT_MS);
   svp_set_stop(svp, SVP_STOP_ERROR);
   vcos_semaphore_post(&svp->sema);
}

/** Enable MMAL port, setting SVP instance as port userdata. */
static MMAL_STATUS_T svp_port_enable(SVP_T *svp, MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   port->userdata = (struct MMAL_PORT_USERDATA_T *)svp;
   return mmal_port_enable(port, cb);
}

/** Process decoded buffers queued by video decoder output callback */
static void svp_process_returned_bufs(SVP_T *svp)
{
   SVP_CALLBACKS_T *callbacks = &svp->callbacks;
   MMAL_BUFFER_HEADER_T *buf;

   while ((buf = mmal_queue_get(svp->queue)) != NULL)
   {
      if ((svp_get_stop(svp) & SVP_STOP_ERROR) == 0)
      {
         callbacks->video_frame_cb(callbacks->ctx, buf->data);
      }

      svp->stats.video_frame_count++;
      mmal_buffer_header_release(buf);
   }
}

/** Worker thread. Ensures video decoder output is supplied with buffers and sends decoded frames
 * via user-supplied callback.
 * @param arg  Pointer to SVP instance.
 * @return NULL always.
 */
static void *svp_worker(void *arg)
{
   SVP_T *svp = arg;
   MMAL_PORT_T *video_output = svp->video_output;
   SVP_CALLBACKS_T *callbacks = &svp->callbacks;
   MMAL_BUFFER_HEADER_T *buf;
   MMAL_STATUS_T st;
   uint32_t stop;

   while (svp_get_stop(svp) == 0)
   {
      /* Send empty buffers to video decoder output port */
      while ((buf = mmal_queue_get(svp->out_pool->queue)) != NULL)
      {
         st = mmal_port_send_buffer(video_output, buf);
         if (st != MMAL_SUCCESS)
         {
            LOG_ERROR("Failed to send buffer to %s", video_output->name);
         }
      }

      /* Process returned buffers */
      svp_process_returned_bufs(svp);

      /* Block for buffer release */
      vcos_semaphore_wait(&svp->sema);
   }

   /* Might have the last few buffers queued */
   svp_process_returned_bufs(svp);

   /* Notify caller if we stopped unexpectedly */
   stop = svp_get_stop(svp);
   LOG_TRACE("Worker thread exiting: stop=0x%x", (unsigned)stop);
   callbacks->stop_cb(callbacks->ctx, stop);

   return NULL;
}

/** Callback from a control port. */
static void svp_bh_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
   SVP_T *svp = (SVP_T *)port->userdata;

   switch (buf->cmd)
   {
   case MMAL_EVENT_EOS:
      LOG_TRACE("%s: EOS", port->name);
      svp_set_stop(svp, SVP_STOP_EOS);
      break;

   case MMAL_EVENT_ERROR:
      LOG_ERROR("%s: MMAL error: %s", port->name,
                mmal_status_to_string(*(MMAL_STATUS_T *)buf->data));
      svp_set_stop(svp, SVP_STOP_ERROR);
      break;

   default:
      LOG_TRACE("%s: buf %p, event %4.4s", port->name, buf, (char *)&buf->cmd);
      break;
   }

   mmal_buffer_header_release(buf);

   vcos_semaphore_post(&svp->sema);
}

/** Callback from video decode output port. */
static void svp_bh_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
   SVP_T *svp = (SVP_T *)port->userdata;

   if (buf->length == 0)
   {
      LOG_TRACE("%s: zero-length buffer => EOS", port->name);
      svp_set_stop(svp, SVP_STOP_EOS); // This shouldn't be necessary, but it is ...
      mmal_buffer_header_release(buf);
   }
   else if (buf->data == NULL)
   {
      LOG_ERROR("%s: zero buffer handle", port->name);
      mmal_buffer_header_release(buf);
   }
   else
   {
      /* Reset watchdog timer */
      vcos_timer_set(&svp->wd_timer, SVP_WATCHDOG_TIMEOUT_MS);

      /* Enqueue the decoded frame so we can return quickly to MMAL core */
      mmal_queue_put(svp->queue, buf);
   }

   /* Notify worker */
   vcos_semaphore_post(&svp->sema);
}

/** Reset svp->stop to 0, with locking. */
static void svp_reset_stop(SVP_T *svp)
{
   vcos_mutex_lock(&svp->mutex);
   svp->stop = 0;
   vcos_mutex_unlock(&svp->mutex);
}

/** Set additional flags in svp->stop, with locking. */
static void svp_set_stop(SVP_T *svp, uint32_t stop_flags)
{
   vcos_mutex_lock(&svp->mutex);
   svp->stop |= stop_flags;
   vcos_mutex_unlock(&svp->mutex);
}

/** Get value of svp->stop, with locking. */
static uint32_t svp_get_stop(SVP_T *svp)
{
   uint32_t stop;

   vcos_mutex_lock(&svp->mutex);
   stop = svp->stop;
   vcos_mutex_unlock(&svp->mutex);

   return stop;
}
