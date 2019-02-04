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

/** \file
 * MMAL test application which plays back video files
 * Note: this is test code. Do not use this in your app. It *will* change or even be removed without notice.
 */

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>

#include "mmalplay.h"

VCOS_LOG_CAT_T mmalplay_log_category;

#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"

#define MMALPLAY_STILL_IMAGE_PAUSE 2000

#define MMALPLAY_MAX_STRING 128
#define MMALPLAY_MAX_CONNECTIONS 16
#define MMALPLAY_MAX_RENDERERS 3

static unsigned int video_render_num;

/** Structure describing a mmal component */
typedef struct {
   struct MMALPLAY_T *ctx;
   MMAL_COMPONENT_T *comp;

   /* Used for debug / statistics */
   char name[MMALPLAY_MAX_STRING];
   int64_t time_setup;
   int64_t time_cleanup;
} MMALPLAY_COMPONENT_T;

/** Context for a mmalplay playback instance */
struct MMALPLAY_T {
   const char *uri;
   VCOS_SEMAPHORE_T event;

   unsigned int component_num;
   MMALPLAY_COMPONENT_T component[MMALPLAY_MAX_CONNECTIONS*2];

   unsigned int connection_num;
   MMAL_CONNECTION_T *connection[MMALPLAY_MAX_CONNECTIONS];

   MMAL_STATUS_T status;
   unsigned int stop;

   MMAL_BOOL_T is_still_image;
   MMAL_PORT_T *reader_video;
   MMAL_PORT_T *reader_audio;
   MMAL_PORT_T *video_out_port;
   MMAL_PORT_T *converter_out_port;
   MMAL_PORT_T *audio_clock;
   MMAL_PORT_T *video_clock;

   MMALPLAY_OPTIONS_T options;

   /* Used for debug / statistics */
   int64_t time_playback;
   unsigned int decoded_frames;
};

typedef struct MMALPLAY_CONNECTIONS_T {
   unsigned int num;

   struct {
      MMAL_PORT_T *in;
      MMAL_PORT_T *out;
      uint32_t flags;
   } connection[MMALPLAY_MAX_CONNECTIONS+1];

} MMALPLAY_CONNECTIONS_T;
#define MMALPLAY_CONNECTION_IN(cx) (cx)->connection[(cx)->num].in
#define MMALPLAY_CONNECTION_OUT(cx) (cx)->connection[(cx)->num].out
#define MMALPLAY_CONNECTION_ADD(cx) if (++(cx)->num > MMALPLAY_MAX_CONNECTIONS) goto error
#define MMALPLAY_CONNECTION_SET(cx, f) do { (cx)->connection[(cx)->num].flags |= (f); } while(0)

static MMAL_COMPONENT_T *mmalplay_component_create(MMALPLAY_T *ctx, const char *name, MMAL_STATUS_T *status);
static MMAL_STATUS_T mmalplay_connection_create(MMALPLAY_T *ctx, MMAL_PORT_T *out, MMAL_PORT_T *in, uint32_t flags);

/* Utility function to setup the container reader component */
static MMAL_STATUS_T mmalplay_setup_container_reader(MMALPLAY_T *, MMAL_COMPONENT_T *,
   const char *uri);
/* Utility function to setup the container writer component */
static MMAL_STATUS_T mmalplay_setup_container_writer(MMALPLAY_T *, MMAL_COMPONENT_T *,
   const char *uri);
/* Utility function to setup the video decoder component */
static MMAL_STATUS_T mmalplay_setup_video_decoder(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);
/* Utility function to setup the splitter component */
static MMAL_STATUS_T mmalplay_setup_splitter(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);
/* Utility function to setup the video converter component */
static MMAL_STATUS_T mmalplay_setup_video_converter(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);
/* Utility function to setup the video render component */
static MMAL_STATUS_T mmalplay_setup_video_render(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);
/* Utility function to setup the video scheduler component */
static MMAL_STATUS_T mmalplay_setup_video_scheduler(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);
/* Utility function to setup the audio decoder component */
static MMAL_STATUS_T mmalplay_setup_audio_decoder(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);
/* Utility function to setup the audio render component */
static MMAL_STATUS_T mmalplay_setup_audio_render(MMALPLAY_T *ctx, MMAL_COMPONENT_T *);

static void log_format(MMAL_ES_FORMAT_T *format, MMAL_PORT_T *port);

/*****************************************************************************/

/** Callback from a control port. Error and EOS events stop playback. */
static void mmalplay_bh_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMALPLAY_T *ctx = (MMALPLAY_T *)port->userdata;
   LOG_TRACE("%s(%p),%p,%4.4s", port->name, port, buffer, (char *)&buffer->cmd);

   if (buffer->cmd == MMAL_EVENT_ERROR || buffer->cmd == MMAL_EVENT_EOS)
   {
      if (buffer->cmd == MMAL_EVENT_ERROR)
      {
         LOG_INFO("error event from %s: %s", port->name,
                  mmal_status_to_string(*(MMAL_STATUS_T*)buffer->data));
         ctx->status = *(MMAL_STATUS_T *)buffer->data;
      }
      else if (buffer->cmd == MMAL_EVENT_EOS)
         LOG_INFO("%s: EOS received", port->name);
      mmalplay_stop(ctx);
   }

   mmal_buffer_header_release(buffer);
}

/** Callback from the connection. Buffer is available. */
static void mmalplay_connection_cb(MMAL_CONNECTION_T *connection)
{
   MMALPLAY_T *ctx = (MMALPLAY_T *)connection->user_data;
   vcos_semaphore_post(&ctx->event);
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_event_handle(MMAL_CONNECTION_T *connection, MMAL_PORT_T *port,
   MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;

   LOG_INFO("%s(%p) received event %4.4s (%i bytes)", port->name, port,
            (char *)&buffer->cmd, (int)buffer->length);

   if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED && port->type == MMAL_PORT_TYPE_OUTPUT)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(buffer);
      if (event)
      {
         LOG_INFO("----------Port format changed----------");
         log_format(port->format, port);
         LOG_INFO("-----------------to---------------------");
         log_format(event->format, 0);
         LOG_INFO(" buffers num (opt %i, min %i), size (opt %i, min: %i)",
                  event->buffer_num_recommended, event->buffer_num_min,
                  event->buffer_size_recommended, event->buffer_size_min);
         LOG_INFO("----------------------------------------");
      }

      status = mmal_connection_event_format_changed(connection, buffer);
   }

   mmal_buffer_header_release(buffer);
   return status;
}

static MMAL_STATUS_T mmalplay_pipeline_audio_create(MMALPLAY_T *ctx, MMAL_PORT_T *source,
   MMALPLAY_CONNECTIONS_T *connections)
{
   MMAL_STATUS_T status;
   MMAL_COMPONENT_T *component;
   unsigned int save = connections->num;

   if (!source  || ctx->options.disable_audio)
      return MMAL_EINVAL;

   MMALPLAY_CONNECTION_OUT(connections) = source;

   /* Create and setup audio decoder component */
   component = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_AUDIO_DECODER, &status);
   if (!component)
      goto error;
   MMALPLAY_CONNECTION_IN(connections) = component->input[0];
   MMALPLAY_CONNECTION_ADD(connections);
   MMALPLAY_CONNECTION_OUT(connections) = component->output[0];

   /* Create and setup audio render component */
   component = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER, &status);
   if (!component)
      goto error;
   MMALPLAY_CONNECTION_IN(connections) = component->input[0];
   MMALPLAY_CONNECTION_ADD(connections);

   return MMAL_SUCCESS;

 error:
   connections->num = save;
   return status == MMAL_SUCCESS ? MMAL_ENOSPC : status;
}

static MMAL_STATUS_T mmalplay_pipeline_video_create(MMALPLAY_T *ctx, MMAL_PORT_T *source,
   MMALPLAY_CONNECTIONS_T *connections)
{
   MMAL_STATUS_T status;
   MMAL_COMPONENT_T *component, *previous = NULL;
   unsigned int i, save = connections->num;

   if (!source || ctx->options.disable_video)
      return MMAL_EINVAL;

   MMALPLAY_CONNECTION_OUT(connections) = source;

   if (ctx->options.output_uri)
   {
      /* Create and setup container writer component */
      component = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_CONTAINER_WRITER, &status);
      if (!component)
         goto error;
      MMALPLAY_CONNECTION_IN(connections) = component->input[0];
      MMALPLAY_CONNECTION_ADD(connections);
      return MMAL_SUCCESS;
   }

   /* Create and setup video decoder component */
   if (!ctx->options.disable_video_decode)
   {
      component = previous = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &status);
      if (!component)
         goto error;
      MMALPLAY_CONNECTION_IN(connections) = component->input[0];
      MMALPLAY_CONNECTION_ADD(connections);
      MMALPLAY_CONNECTION_OUT(connections) = component->output[0];
   }
   else ctx->video_out_port = source;

   if (ctx->options.enable_scheduling)
   {
      /* Create a scheduler component */
      component = previous = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_SCHEDULER, &status);
      if (!component)
         goto error;
      MMALPLAY_CONNECTION_IN(connections) = component->input[0];
      MMALPLAY_CONNECTION_ADD(connections);
      MMALPLAY_CONNECTION_OUT(connections) = component->output[0];
   }

   /* Create and setup video converter component */
   if (ctx->options.render_format ||
       (ctx->options.render_rect.width && ctx->options.render_rect.height))
   {
      component = previous = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_VIDEO_CONVERTER, &status);
      if (!component)
         goto error;
      MMALPLAY_CONNECTION_IN(connections) = component->input[0];
      MMALPLAY_CONNECTION_ADD(connections);
      MMALPLAY_CONNECTION_OUT(connections) = component->output[0];
   }

   if (ctx->options.output_num > 1)
   {
      /* Create and setup splitter component */
      component = previous = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_SPLITTER, &status);
      if (!component || component->output_num < ctx->options.output_num)
         goto error;
      MMALPLAY_CONNECTION_IN(connections) = component->input[0];
      MMALPLAY_CONNECTION_ADD(connections);
      MMALPLAY_CONNECTION_OUT(connections) = component->output[0];
   }

   /* Create and setup video render components */
   for (i = 0; i < ctx->options.output_num; i++)
   {
      component = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &status);
      if (!component)
         goto error;
      MMALPLAY_CONNECTION_OUT(connections) = previous ? previous->output[i] : source;
      MMALPLAY_CONNECTION_IN(connections) = component->input[0];
      MMALPLAY_CONNECTION_ADD(connections);
   }

   return MMAL_SUCCESS;

 error:
   connections->num = save;
   return status == MMAL_SUCCESS ? MMAL_ENOSPC : status;
}

static MMAL_STATUS_T mmalplay_pipeline_clock_create(MMALPLAY_T *ctx, MMALPLAY_CONNECTIONS_T *connections)
{
   MMAL_STATUS_T status = MMAL_EINVAL;

   if (!ctx->options.enable_scheduling)
      return MMAL_SUCCESS;

   if (!ctx->audio_clock || !ctx->video_clock)
   {
      LOG_ERROR("clock port(s) not present %p %p", ctx->audio_clock, ctx->video_clock);
      return MMAL_SUCCESS;
   }

   /* Connect audio clock to video clock */
   MMALPLAY_CONNECTION_SET(connections, MMAL_CONNECTION_FLAG_TUNNELLING);
   MMALPLAY_CONNECTION_OUT(connections) = ctx->audio_clock;
   MMALPLAY_CONNECTION_IN(connections) = ctx->video_clock;
   MMALPLAY_CONNECTION_ADD(connections);

   /* Set audio clock as master */
   status = mmal_port_parameter_set_boolean(ctx->audio_clock, MMAL_PARAMETER_CLOCK_REFERENCE, MMAL_TRUE);
   if (status != MMAL_SUCCESS)
      LOG_ERROR("failed to set clock reference");

 error:
   return status;
}

/** Create an instance of mmalplay.
 * Note: this is test code. Do not use it in your app. It *will* change or even be removed without notice.
 */
MMALPLAY_T *mmalplay_create(const char *uri, MMALPLAY_OPTIONS_T *opts, MMAL_STATUS_T *pstatus)
{
   MMAL_STATUS_T status = MMAL_SUCCESS, status_audio, status_video, status_clock;
   MMALPLAY_T *ctx;
   MMAL_COMPONENT_T *component;
   MMALPLAY_CONNECTIONS_T connections;
   unsigned int i;

   LOG_TRACE("%s", uri);

   if (pstatus) *pstatus = MMAL_ENOMEM;

   /* Allocate and initialise context */
   ctx = malloc(sizeof(MMALPLAY_T));
   if (!ctx)
      return NULL;
   memset(ctx, 0, sizeof(*ctx));
   memset(&connections, 0, sizeof(connections));
   if (vcos_semaphore_create(&ctx->event, "MMALTest", 1) != VCOS_SUCCESS)
   {
      free(ctx);
      return NULL;
   }

   ctx->uri = uri;
   if (opts)
      ctx->options = *opts;

   ctx->options.output_num = MMAL_MAX(ctx->options.output_num, 1);
   ctx->options.output_num = MMAL_MIN(ctx->options.output_num, MMALPLAY_MAX_RENDERERS);
   connections.num = 0;

   /* Create and setup the container reader component */
   component = mmalplay_component_create(ctx, MMAL_COMPONENT_DEFAULT_CONTAINER_READER, &status);
   if (!component)
      goto error;

   status_video = mmalplay_pipeline_video_create(ctx, ctx->reader_video, &connections);
   status_audio = mmalplay_pipeline_audio_create(ctx, ctx->reader_audio, &connections);
   status_clock = mmalplay_pipeline_clock_create(ctx, &connections);
   if (status_video != MMAL_SUCCESS && status_audio != MMAL_SUCCESS && status_clock != MMAL_SUCCESS)
   {
      status = status_video;
      goto error;
   }

   /* Create our connections */
   for (i = 0; i < connections.num; i++)
   {
      status = mmalplay_connection_create(ctx, connections.connection[i].out, connections.connection[i].in,
            connections.connection[i].flags);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   /* Enable our connections */
   for (i = 0; i < ctx->connection_num; i++)
   {
      status = mmal_connection_enable(ctx->connection[i]);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   if (pstatus) *pstatus = MMAL_SUCCESS;
   return ctx;

 error:
   mmalplay_destroy(ctx);
   if (status == MMAL_SUCCESS) status = MMAL_ENOSPC;
   if (pstatus) *pstatus = status;
   return NULL;
}

/** Start playback on an instance of mmalplay.
 * Note: this is test code. Do not use it in your app. It *will* change or even be removed without notice.
 */
MMAL_STATUS_T mmalplay_play(MMALPLAY_T *ctx)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   unsigned int i;

   LOG_TRACE("%p, %s", ctx, ctx->uri);

   ctx->time_playback = vcos_getmicrosecs();

   /* Start the clocks */
   if (ctx->video_clock)
      mmal_port_parameter_set_boolean(ctx->video_clock, MMAL_PARAMETER_CLOCK_ACTIVE, MMAL_TRUE);
   if (ctx->audio_clock)
      mmal_port_parameter_set_boolean(ctx->audio_clock, MMAL_PARAMETER_CLOCK_ACTIVE, MMAL_TRUE);

   while(1)
   {
      MMAL_BUFFER_HEADER_T *buffer;

      vcos_semaphore_wait(&ctx->event);
      if (ctx->stop || ctx->status != MMAL_SUCCESS)
      {
         status = ctx->status;
         break;
      }

      /* Loop through all the connections */
      for (i = 0; i < ctx->connection_num; i++)
      {
         MMAL_CONNECTION_T *connection = ctx->connection[i];

         if (connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
            continue; /* Nothing else to do in tunnelling mode */

         /* Send any queued buffer to the next component */
         buffer = mmal_queue_get(connection->queue);
         while (buffer)
         {
            if (buffer->cmd)
            {
               status = mmalplay_event_handle(connection, connection->out, buffer);
               if (status != MMAL_SUCCESS)
                  goto error;
               buffer = mmal_queue_get(connection->queue);
               continue;
            }

            /* Code specific to handling of the video output port */
            if (connection->out == ctx->video_out_port)
            {
               if (buffer->length)
                  ctx->decoded_frames++;

               if (ctx->options.stepping)
                  getchar();
            }

            status = mmal_port_send_buffer(connection->in, buffer);
            if (status != MMAL_SUCCESS)
            {
               LOG_ERROR("mmal_port_send_buffer failed (%i)", status);
               mmal_queue_put_back(connection->queue, buffer);
               goto error;
            }
            buffer = mmal_queue_get(connection->queue);
         }

         /* Send empty buffers to the output port of the connection */
         buffer = connection->pool ? mmal_queue_get(connection->pool->queue) : NULL;
         while (buffer)
         {
            status = mmal_port_send_buffer(connection->out, buffer);
            if (status != MMAL_SUCCESS)
            {
               LOG_ERROR("mmal_port_send_buffer failed (%i)", status);
               mmal_queue_put_back(connection->pool->queue, buffer);
               goto error;
            }
            buffer = mmal_queue_get(connection->pool->queue);
         }
      }
   }

 error:
   ctx->time_playback = vcos_getmicrosecs() - ctx->time_playback;

   /* For still images we want to pause a bit once they are displayed */
   if (ctx->is_still_image && status == MMAL_SUCCESS)
      vcos_sleep(MMALPLAY_STILL_IMAGE_PAUSE);

   return status;
}

/** Stop the playback on an instance of mmalplay.
 * Note: this is test code. Do not use it in your app. It *will* change or even be removed without notice.
 */
void mmalplay_stop(MMALPLAY_T *ctx)
{
   ctx->stop = 1;
   vcos_semaphore_post(&ctx->event);
}

/** Destroys an instance of mmalplay.
 * Note: this is test code. Do not use it in your app. It *will* change or even be removed without notice.
 */
void mmalplay_destroy(MMALPLAY_T *ctx)
{
   unsigned int i;

   LOG_TRACE("%p, %s", ctx, ctx->uri);

   /* Disable connections */
   for (i = ctx->connection_num; i; i--)
      mmal_connection_disable(ctx->connection[i-1]);

   LOG_INFO("--- statistics ---");
   LOG_INFO("decoded %i frames in %.2fs (%.2ffps)", (int)ctx->decoded_frames,
            ctx->time_playback / 1000000.0, ctx->decoded_frames * 1000000.0 / ctx->time_playback);

   for (i = 0; i < ctx->connection_num; i++)
   {
      LOG_INFO("%s", ctx->connection[i]->name);
      LOG_INFO("- setup time: %ims",
               (int)(ctx->connection[i]->time_setup / 1000));
      LOG_INFO("- enable time: %ims, disable time: %ims",
               (int)(ctx->connection[i]->time_enable / 1000),
               (int)(ctx->connection[i]->time_disable / 1000));
   }

   /* Destroy connections */
   for (i = ctx->connection_num; i; i--)
      mmal_connection_destroy(ctx->connection[i-1]);

   /* Destroy components */
   for (i = ctx->component_num; i; i--)
   {
      ctx->component[i-1].time_cleanup = vcos_getmicrosecs();
      mmal_component_destroy(ctx->component[i-1].comp);
      ctx->component[i-1].time_cleanup = vcos_getmicrosecs() - ctx->component[i-1].time_cleanup;
   }

   vcos_semaphore_delete(&ctx->event);

   for (i = 0; i < ctx->component_num; i++)
   {
      LOG_INFO("%s:", ctx->component[i].name);
      LOG_INFO("- setup time: %ims, cleanup time: %ims",
               (int)(ctx->component[i].time_setup / 1000),
               (int)(ctx->component[i].time_cleanup / 1000));
   }
   LOG_INFO("-----------------");

   free(ctx);
}

/*****************************************************************************/
static MMAL_COMPONENT_T *mmalplay_component_create(MMALPLAY_T *ctx,
   const char *name, MMAL_STATUS_T *status)
{
   MMALPLAY_COMPONENT_T *component = &ctx->component[ctx->component_num];
   const char *component_name = name;

   LOG_TRACE("%p, %s", ctx, name);

   if (ctx->component_num >= MMAL_COUNTOF(ctx->component))
   {
      *status = MMAL_ENOSPC;
      return NULL;
   }

   /* Decide whether we want an image decoder instead of a video_decoder */
   if (ctx->is_still_image &&
       !strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_DECODER) && !ctx->options.component_video_decoder)
      ctx->options.component_video_decoder = MMAL_COMPONENT_DEFAULT_IMAGE_DECODER;

   /* Override name if requested by the user */
   if (!strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_DECODER) && ctx->options.component_video_decoder)
      component_name = ctx->options.component_video_decoder;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_SPLITTER) && ctx->options.component_splitter)
      component_name = ctx->options.component_splitter;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER) && ctx->options.component_video_render)
      component_name = ctx->options.component_video_render;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_CONVERTER) && ctx->options.component_video_converter)
      component_name = ctx->options.component_video_converter;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_SCHEDULER) && ctx->options.component_video_scheduler)
      component_name = ctx->options.component_video_scheduler;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_AUDIO_DECODER) && ctx->options.component_audio_decoder)
      component_name = ctx->options.component_audio_decoder;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER) && ctx->options.component_audio_render)
      component_name = ctx->options.component_audio_render;
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_CONTAINER_READER) && ctx->options.component_container_reader)
      component_name = ctx->options.component_container_reader;

   component->comp = NULL;
   vcos_safe_strcpy(component->name, component_name, sizeof(component->name), 0);
   component->time_setup = vcos_getmicrosecs();

   /* Create the component */
   *status = mmal_component_create(component_name, &component->comp);
   if(*status != MMAL_SUCCESS)
   {
      LOG_ERROR("couldn't create %s", component_name);
      goto error;
   }

   if (!strcmp(name, MMAL_COMPONENT_DEFAULT_CONTAINER_READER))
      *status = mmalplay_setup_container_reader(ctx, component->comp, ctx->uri);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_CONTAINER_WRITER))
      *status = mmalplay_setup_container_writer(ctx, component->comp, ctx->options.output_uri);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_DECODER))
      *status = mmalplay_setup_video_decoder(ctx, component->comp);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_SPLITTER))
      *status = mmalplay_setup_splitter(ctx, component->comp);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_CONVERTER))
      *status = mmalplay_setup_video_converter(ctx, component->comp);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER))
      *status = mmalplay_setup_video_render(ctx, component->comp);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_SCHEDULER))
      *status = mmalplay_setup_video_scheduler(ctx, component->comp);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_AUDIO_DECODER))
      *status = mmalplay_setup_audio_decoder(ctx, component->comp);
   else if (!strcmp(name, MMAL_COMPONENT_DEFAULT_AUDIO_RENDERER))
      *status = mmalplay_setup_audio_render(ctx, component->comp);

   if (*status != MMAL_SUCCESS)
      goto error;

   /* Enable component */
   *status = mmal_component_enable(component->comp);
   if(*status)
   {
      LOG_ERROR("%s couldn't be enabled", component_name);
      goto error;
   }

   /* Enable control port. The callback specified here is the function which
    * will be called when an empty buffer header comes back to the port. */
   component->comp->control->userdata = (void *)ctx;
   *status = mmal_port_enable(component->comp->control, mmalplay_bh_control_cb);
   if (*status)
   {
      LOG_ERROR("control port couldn't be enabled");
      goto error;
   }

   component->time_setup = vcos_getmicrosecs() - component->time_setup;
   ctx->component_num++;
   return component->comp;

 error:
   component->time_setup = vcos_getmicrosecs() - component->time_setup;
   if (component->comp)
      mmal_component_destroy(component->comp);
   return NULL;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_connection_create(MMALPLAY_T *ctx, MMAL_PORT_T *out, MMAL_PORT_T *in, uint32_t flags)
{
   MMAL_CONNECTION_T **connection = &ctx->connection[ctx->connection_num];
   uint32_t encoding_override = MMAL_ENCODING_UNKNOWN;
   MMAL_RECT_T *rect_override = NULL;
   MMAL_BOOL_T format_override = MMAL_FALSE;
   MMAL_STATUS_T status;

   if (ctx->connection_num >= MMAL_COUNTOF(ctx->connection))
      return MMAL_ENOMEM;

   /* Globally enable tunnelling if requested by the user */
   flags |= ctx->options.tunnelling ? MMAL_CONNECTION_FLAG_TUNNELLING : 0;

   /* Apply any override to the format specified by the user */
   if (out == ctx->video_out_port)
   {
      encoding_override = ctx->options.output_format;
      rect_override = &ctx->options.output_rect;
   }
   else if (out == ctx->converter_out_port)
   {
      encoding_override = ctx->options.render_format;
      rect_override = &ctx->options.render_rect;
   }

   if (encoding_override != MMAL_ENCODING_UNKNOWN &&
       encoding_override != out->format->encoding)
   {
      out->format->encoding = encoding_override;
      format_override = MMAL_TRUE;
   }

   if (rect_override && rect_override->width && rect_override->height)
   {
      out->format->es->video.crop = *rect_override;
      out->format->es->video.width = rect_override->width;
      out->format->es->video.height = rect_override->height;
      format_override = MMAL_TRUE;
   }

   if (format_override)
   {
      status = mmal_port_format_commit(out);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("cannot override format on output port %s", out->name);
         return status;
      }
   }

   /* Create the actual connection */
   status = mmal_connection_create(connection, out, in, flags);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("cannot create connection");
      return status;
   }

   /* Set our connection callback */
   (*connection)->callback = mmalplay_connection_cb;
   (*connection)->user_data = (void *)ctx;

   log_format(out->format, out);
   log_format(in->format, in);

   ctx->connection_num++;
   return MMAL_SUCCESS;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_video_decoder(MMALPLAY_T *ctx, MMAL_COMPONENT_T *decoder)
{
   MMAL_PARAMETER_BOOLEAN_T param_zc =
      {{MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T)}, 1};
   MMAL_STATUS_T status = MMAL_EINVAL;

   if(!decoder->input_num || !decoder->output_num)
   {
      LOG_ERROR("%s doesn't have input/output ports", decoder->name);
      goto error;
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_input)
   {
      status = mmal_port_parameter_set(decoder->input[0], &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", decoder->input[0]->name);
         goto error;
      }
   }
   if (!ctx->options.copy_output)
   {
      status = mmal_port_parameter_set(decoder->output[0], &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", decoder->output[0]->name);
         goto error;
      }
   }

   ctx->video_out_port = decoder->output[0];
   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_splitter(MMALPLAY_T *ctx, MMAL_COMPONENT_T *splitter)
{
   MMAL_STATUS_T status = MMAL_EINVAL;

   if(!splitter->input_num || !splitter->output_num)
   {
      LOG_ERROR("%s doesn't have input ports", splitter->name);
      goto error;
   }
   if(splitter->output_num < ctx->options.output_num)
   {
      LOG_ERROR("%s doesn't have enough output ports (%u/%u)", splitter->name,
                (unsigned int)splitter->output_num, ctx->options.output_num);
      goto error;
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_output)
   {
      MMAL_PARAMETER_BOOLEAN_T param_zc =
         {{MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T)}, 1};
      unsigned int i;

      status = mmal_port_parameter_set(splitter->input[0], &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", splitter->input[0]->name);
         goto error;
      }
      for (i = 0; i < splitter->output_num; i++)
      {
         status = mmal_port_parameter_set(splitter->output[i], &param_zc.hdr);
         if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
         {
            LOG_ERROR("failed to set zero copy on %s", splitter->output[i]->name);
            goto error;
         }
      }
   }

   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_video_converter(MMALPLAY_T *ctx, MMAL_COMPONENT_T *converter)
{
   MMAL_PARAMETER_BOOLEAN_T param_zc =
      {{MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T)}, 1};
   MMAL_STATUS_T status = MMAL_EINVAL;

   if(!converter->input_num || !converter->output_num)
   {
      LOG_ERROR("%s doesn't have input/output ports", converter->name);
      goto error;
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_output)
   {
      status = mmal_port_parameter_set(converter->input[0], &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", converter->input[0]->name);
         goto error;
      }
      status = mmal_port_parameter_set(converter->output[0], &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", converter->output[0]->name);
         goto error;
      }
   }

   ctx->converter_out_port = converter->output[0];
   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_video_render(MMALPLAY_T *ctx, MMAL_COMPONENT_T *render)
{
   MMAL_STATUS_T status = MMAL_EINVAL;
   unsigned int render_num;

   if(!render->input_num)
   {
      LOG_ERROR("%s doesn't have input ports", render->name);
      goto error;
   }

   render_num = video_render_num++;

   /* Give higher priority to the overlay layer */
   MMAL_DISPLAYREGION_T param;
   param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
   param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
   param.set = MMAL_DISPLAY_SET_LAYER|MMAL_DISPLAY_SET_NUM;
   param.layer = ctx->options.render_layer + 2;   /* Offset of two to put it above the Android UI layer */
   param.display_num = ctx->options.video_destination;
   if (ctx->options.window)
   {
      param.dest_rect.x = 0;
      param.dest_rect.width = 512;
      param.dest_rect.height = 256;
      param.dest_rect.y = param.dest_rect.height * render_num;
      param.mode = MMAL_DISPLAY_MODE_LETTERBOX;
      param.fullscreen = 0;
      param.set |= MMAL_DISPLAY_SET_DEST_RECT|MMAL_DISPLAY_SET_MODE|MMAL_DISPLAY_SET_FULLSCREEN;
   }
   status = mmal_port_parameter_set( render->input[0], &param.hdr );
   if(status != MMAL_SUCCESS && status != MMAL_ENOSYS)
   {
      LOG_ERROR("could not set video render layer on %s", render->input[0]->name);
      goto error;
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_output)
   {
      MMAL_PARAMETER_BOOLEAN_T param_zc =
         {{MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T)}, 1};
      status = mmal_port_parameter_set(render->input[0], &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", render->input[0]->name);
         goto error;
      }
   }

   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_container_reader(MMALPLAY_T *ctx,
   MMAL_COMPONENT_T *reader, const char *uri)
{
   MMAL_PARAMETER_SEEK_T seek = {{MMAL_PARAMETER_SEEK, sizeof(MMAL_PARAMETER_SEEK_T)},0,0};
   MMAL_PARAMETER_STRING_T *param = 0;
   unsigned int param_size, track_audio, track_video;
   MMAL_STATUS_T status = MMAL_EINVAL;
   uint32_t encoding;
   size_t uri_len;

   if(!reader->output_num)
   {
      LOG_ERROR("%s doesn't have output ports", reader->name);
      goto error;
   }

   /* Open the given URI */
   uri_len = strlen(uri);
   param_size = sizeof(MMAL_PARAMETER_STRING_T) + uri_len;
   param = malloc(param_size);
   if(!param)
   {
      LOG_ERROR("out of memory");
      status = MMAL_ENOMEM;
      goto error;
   }
   memset(param, 0, param_size);
   param->hdr.id = MMAL_PARAMETER_URI;
   param->hdr.size = param_size;
   vcos_safe_strcpy(param->str, uri, uri_len + 1, 0);
   status = mmal_port_parameter_set(reader->control, &param->hdr);
   if(status != MMAL_SUCCESS && status != MMAL_ENOSYS)
   {
      LOG_ERROR("%s could not open file %s", reader->name, uri);
      goto error;
   }
   status = MMAL_SUCCESS;

   /* Look for a video track */
   for(track_video = 0; track_video < reader->output_num; track_video++)
      if(reader->output[track_video]->format->type == MMAL_ES_TYPE_VIDEO) break;
   if(track_video != reader->output_num)
   {
      ctx->reader_video = reader->output[track_video];
      /* Try to detect still images */
      encoding = ctx->reader_video->format->encoding;
      if (encoding == MMAL_ENCODING_JPEG ||
          encoding == MMAL_ENCODING_GIF  ||
          encoding == MMAL_ENCODING_PNG  ||
          encoding == MMAL_ENCODING_PPM  ||
          encoding == MMAL_ENCODING_TGA  ||
          encoding == MMAL_ENCODING_BMP)
         ctx->is_still_image = 1;
   }

   /* Look for an audio track */
   for(track_audio = 0; track_audio < reader->output_num; track_audio++)
      if(reader->output[track_audio]->format->type == MMAL_ES_TYPE_AUDIO) break;
   if(track_audio != reader->output_num)
      ctx->reader_audio = reader->output[track_audio];

   if(track_video == reader->output_num &&
      track_audio == reader->output_num)
   {
      LOG_ERROR("no track to decode");
      status = MMAL_EINVAL;
      goto error;
   }

   LOG_INFO("----Reader input port format-----");
   if(track_video != reader->output_num)
      log_format(reader->output[track_video]->format, 0);
   if(track_audio != reader->output_num)
      log_format(reader->output[track_audio]->format, 0);

   if(ctx->options.seeking)
   {
      LOG_DEBUG("seek to %fs", ctx->options.seeking);
      seek.offset = ctx->options.seeking * INT64_C(1000000);
      status = mmal_port_parameter_set(reader->control, &seek.hdr);
      if(status != MMAL_SUCCESS)
         LOG_ERROR("failed to seek to %fs", ctx->options.seeking);
   }

 error:
   if(param)
      free(param);
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_container_writer(MMALPLAY_T *ctx,
   MMAL_COMPONENT_T *writer, const char *uri)
{
   MMAL_PARAMETER_URI_T *param = 0;
   unsigned int param_size;
   MMAL_STATUS_T status = MMAL_EINVAL;
   size_t uri_len;
   MMAL_PARAM_UNUSED(ctx);

   if(!writer->input_num)
   {
      LOG_ERROR("%s doesn't have input ports", writer->name);
      goto error;
   }

   /* Open the given URI */
   uri_len = strlen(uri);
   param_size = sizeof(MMAL_PARAMETER_HEADER_T) + uri_len;
   param = malloc(param_size);
   if(!param)
   {
      LOG_ERROR("out of memory");
      status = MMAL_ENOMEM;
      goto error;
   }
   memset(param, 0, param_size);
   param->hdr.id = MMAL_PARAMETER_URI;
   param->hdr.size = param_size;
   vcos_safe_strcpy(param->uri, uri, uri_len + 1, 0);
   status = mmal_port_parameter_set(writer->control, &param->hdr);
   if(status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not open file %s", uri);
      goto error;
   }

 error:
   if(param)
      free(param);
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_video_scheduler(MMALPLAY_T *ctx, MMAL_COMPONENT_T *scheduler)
{
   MMAL_STATUS_T status = MMAL_EINVAL;

   if (!scheduler->input_num || !scheduler->output_num || !scheduler->clock_num)
   {
      LOG_ERROR("%s doesn't have input/output/clock ports", scheduler->name);
      goto error;
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_output)
   {
      status = mmal_port_parameter_set_boolean(scheduler->input[0],
            MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", scheduler->input[0]->name);
         goto error;
      }
      status = mmal_port_parameter_set_boolean(scheduler->output[0],
            MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", scheduler->output[0]->name);
         goto error;
      }
   }

   /* Save a copy of the clock port to connect to the audio clock port */
   ctx->video_clock = scheduler->clock[0];

   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_audio_decoder(MMALPLAY_T *ctx, MMAL_COMPONENT_T *decoder)
{
   MMAL_STATUS_T status = MMAL_EINVAL;

   if (!decoder->input_num || !decoder->output_num)
   {
      LOG_ERROR("%s doesn't have input/output ports", decoder->name);
      goto error;
   }

   if (ctx->options.audio_passthrough)
   {
      status = mmal_port_parameter_set_boolean(decoder->control,
         MMAL_PARAMETER_AUDIO_PASSTHROUGH, MMAL_TRUE);
      if (status != MMAL_SUCCESS)
         LOG_INFO("could not set audio passthrough mode");
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_input)
   {
      status = mmal_port_parameter_set_boolean(decoder->input[0],
            MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", decoder->input[0]->name);
         goto error;
      }
   }
   if (!ctx->options.copy_output)
   {
      status = mmal_port_parameter_set_boolean(decoder->output[0],
            MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", decoder->output[0]->name);
         goto error;
      }
   }

   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalplay_setup_audio_render(MMALPLAY_T *ctx, MMAL_COMPONENT_T *render)
{
   MMAL_STATUS_T status = MMAL_EINVAL;

   /* Set the audio destination - not all audio render components support this */
   if (ctx->options.audio_destination)
   {
      status = mmal_port_parameter_set_string(render->control,
            MMAL_PARAMETER_AUDIO_DESTINATION, ctx->options.audio_destination);
      if (status != MMAL_SUCCESS)
         LOG_INFO("could not set audio destination");
   }

   /* Enable Zero Copy if requested. This needs to be sent before enabling the port. */
   if (!ctx->options.copy_output)
   {
      status = mmal_port_parameter_set_boolean(render->input[0],
            MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", render->input[0]->name);
         goto error;
      }
   }

   if (render->clock_num)
      ctx->audio_clock = render->clock[0];
   else
      LOG_ERROR("%s doesn't have a clock port", render->name);

   status = MMAL_SUCCESS;

 error:
   return status;
}

/*****************************************************************************/
static void log_format(MMAL_ES_FORMAT_T *format, MMAL_PORT_T *port)
{
   const char *name_type;

   if(port)
      LOG_INFO("%s:%s:%i", port->component->name,
               port->type == MMAL_PORT_TYPE_CONTROL ? "ctr" :
                  port->type == MMAL_PORT_TYPE_INPUT ? "in" :
                  port->type == MMAL_PORT_TYPE_OUTPUT ? "out" : "invalid",
               (int)port->index);

   switch(format->type)
   {
   case MMAL_ES_TYPE_AUDIO: name_type = "audio"; break;
   case MMAL_ES_TYPE_VIDEO: name_type = "video"; break;
   case MMAL_ES_TYPE_SUBPICTURE: name_type = "subpicture"; break;
   default: name_type = "unknown"; break;
   }

   LOG_INFO("type: %s, fourcc: %4.4s", name_type, (char *)&format->encoding);
   LOG_INFO(" bitrate: %i, framed: %i", format->bitrate,
            !!(format->flags & MMAL_ES_FORMAT_FLAG_FRAMED));
   LOG_INFO(" extra data: %i, %p", format->extradata_size, format->extradata);
   switch(format->type)
   {
   case MMAL_ES_TYPE_AUDIO:
      LOG_INFO(" samplerate: %i, channels: %i, bps: %i, block align: %i",
               format->es->audio.sample_rate, format->es->audio.channels,
               format->es->audio.bits_per_sample, format->es->audio.block_align);
      break;

   case MMAL_ES_TYPE_VIDEO:
      LOG_INFO(" width: %i, height: %i, (%i,%i,%i,%i)",
               format->es->video.width, format->es->video.height,
               format->es->video.crop.x, format->es->video.crop.y,
               format->es->video.crop.width, format->es->video.crop.height);
      LOG_INFO(" pixel aspect ratio: %i/%i, frame rate: %i/%i",
               format->es->video.par.num, format->es->video.par.den,
               format->es->video.frame_rate.num, format->es->video.frame_rate.den);
      break;

   case MMAL_ES_TYPE_SUBPICTURE:
      break;

   default: break;
   }

   if(!port)
      return;

   LOG_INFO(" buffers num: %i(opt %i, min %i), size: %i(opt %i, min: %i), align: %i",
            port->buffer_num, port->buffer_num_recommended, port->buffer_num_min,
            port->buffer_size, port->buffer_size_recommended, port->buffer_size_min,
            port->buffer_alignment_min);
}
