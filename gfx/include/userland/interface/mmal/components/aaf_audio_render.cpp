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

#include "mmal.h"
#include "mmal_logging.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "core/mmal_clock_private.h"

#include <media/AudioTrack.h>
#include <utils/Mutex.h>

using namespace android;

/* Buffering requirements */
#define INPUT_MIN_BUFFER_NUM 4
#define INPUT_RECOMMENDED_BUFFER_NUM 8

#define SPDIF_AC3_FRAME_SIZE 6144

/*****************************************************************************/
enum TRACK_STATE_T {
   TRACK_STATE_STOPPED,
   TRACK_STATE_RUNNING,
   TRACK_STATE_PAUSED
};

typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;
   MMAL_QUEUE_T *queue;

   android::sp<android::AudioTrack> track;

   Mutex *lock;
   uint32_t bytes_queued;

   MMAL_BOOL_T is_enabled;
   TRACK_STATE_T state;

   MMAL_ES_FORMAT_T *format; /**< format currently configured */

} MMAL_COMPONENT_MODULE_T;

/*****************************************************************************/

static void aaf_track_callback(int event, void *user, void *info);

static struct encoding_table_t {
   MMAL_FOURCC_T encoding;
   audio_format_t format;
} encoding_list[] =
{ {MMAL_ENCODING_PCM_SIGNED, AUDIO_FORMAT_PCM_16_BIT},
#ifdef ANDROID_SUPPORTS_AC3
  {MMAL_ENCODING_AC3, AUDIO_FORMAT_AC3_SPDIF},
  {MMAL_ENCODING_EAC3, AUDIO_FORMAT_EAC3_SPDIF},
#endif
  {0, AUDIO_FORMAT_INVALID}
};

static audio_format_t encoding_to_audio_format(MMAL_FOURCC_T encoding)
{
   struct encoding_table_t *entry = encoding_list;

   for (entry = encoding_list; entry->encoding; entry++)
      if (entry->encoding == encoding)
         break;

   return entry->format;
}

static void aaf_track_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   if (module->track != NULL)
   {
      module->track->stop();
      module->track = NULL;
   }
}

static MMAL_STATUS_T aaf_track_create(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port = component->input[0];
   audio_channel_mask_t channels_mask;
   int frame_count = 256;

   /* Reset track on format change.  Can do better than that? */
   if (module->track != NULL)
      aaf_track_destroy(component);

   channels_mask = audio_channel_out_mask_from_count(port->format->es->audio.channels);
   LOG_INFO("%s(%p) %4.4s, %i Hz, mask %x, chan %i", port->name, port,
            (char *)&port->format->encoding,
            (int)port->format->es->audio.sample_rate, channels_mask,
            (int)port->format->es->audio.channels);

   AudioTrack::getMinFrameCount(&frame_count);
   if (port->format->encoding == MMAL_ENCODING_AC3)
      frame_count = SPDIF_AC3_FRAME_SIZE;
   else if (port->format->encoding == MMAL_ENCODING_EAC3)
      frame_count = SPDIF_AC3_FRAME_SIZE * 4;
   frame_count *= 2; /* Twice the minimum should be enough */

   module->track = new AudioTrack(AUDIO_STREAM_MUSIC,
      port->format->es->audio.sample_rate,
      encoding_to_audio_format(port->format->encoding), channels_mask,
      frame_count, port->format->encoding == MMAL_ENCODING_PCM_SIGNED ?
         AUDIO_OUTPUT_FLAG_NONE : AUDIO_OUTPUT_FLAG_DIRECT,
      &aaf_track_callback, port, 0);

   if (module->track == NULL || module->track->initCheck() != OK)
   {
      LOG_ERROR("%s(%p): track creation failed", port->name, port);
      module->track = NULL;
      return MMAL_ENOSYS;
   }

   return MMAL_SUCCESS;
}

static void aaf_track_callback(int event, void *user, void *info)
{
   MMAL_PORT_T *port = (MMAL_PORT_T *)user;
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   AudioTrack::Buffer *trackbuf = NULL;
   unsigned int bytes, space;
   uint8_t *dest;

   if (event == AudioTrack::EVENT_UNDERRUN)
      LOG_ERROR("underrun");

   if (event != AudioTrack::EVENT_MORE_DATA)
      return;

   trackbuf = (AudioTrack::Buffer *)info;
   space = trackbuf->size;
   dest = (uint8_t *)trackbuf->raw;
   trackbuf->size = 0;

   if (!mmal_queue_length(module->queue))
   {
      LOG_ERROR("no buffers queued");
      return;
   }

   while (space > 0)
   {
      buffer = mmal_queue_get(module->queue);
      if (!buffer)
         break;

      bytes = MMAL_MIN(buffer->length, space);
      memcpy(dest, buffer->data + buffer->offset, bytes);
      buffer->offset += bytes;
      buffer->length -= bytes;
      dest += bytes;
      space -= bytes;
      trackbuf->size += bytes;

      if (buffer->length)
      {
         /* Re-queue */
         mmal_queue_put_back(module->queue, buffer);
         continue;
      }

      /* Handle the EOS */
      if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS)
         mmal_event_eos_send(port);

      buffer->offset = 0;
      mmal_port_buffer_header_callback(port, buffer);
   }

   module->lock->lock();
   module->bytes_queued -= trackbuf->size;
   module->lock->unlock();
}

static MMAL_STATUS_T aaf_track_state_update(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   TRACK_STATE_T new_state = TRACK_STATE_STOPPED;

   if (module->track == NULL)
      return MMAL_SUCCESS;

   if (module->is_enabled)
   {
      MMAL_RATIONAL_T scale = mmal_port_clock_scale_get(component->clock[0]);
      new_state = TRACK_STATE_PAUSED;
      if (scale.den && scale.den == scale.num)
         new_state = TRACK_STATE_RUNNING;
   }

   if (new_state == module->state)
      return MMAL_SUCCESS; /* Nothing to do */

   if (module->state == TRACK_STATE_STOPPED && new_state == TRACK_STATE_RUNNING)
   {
      module->track->start();
   }
   else if (module->state == TRACK_STATE_RUNNING)
   {
      if (new_state == TRACK_STATE_STOPPED)
         module->track->stop();
      else if (new_state == TRACK_STATE_PAUSED)
         module->track->pause();
   }
   else if (module->state == TRACK_STATE_PAUSED)
   {
      if (new_state == TRACK_STATE_STOPPED)
         module->track->stop();
      else if (new_state == TRACK_STATE_RUNNING)
         module->track->start();
   }

   module->state = new_state;
   return MMAL_SUCCESS;
}

/** Destroy a previously created component */
static MMAL_STATUS_T aaf_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   aaf_track_destroy(component);

   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);
   if(component->clock_num)
      mmal_ports_clock_free(component->clock, component->clock_num);
   if(module->format)
      mmal_format_free(module->format);
   if(module->queue)
      mmal_queue_destroy(module->queue);
   delete module->lock;
   vcos_free(module);
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T aaf_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;

   if (!mmal_format_compare(port->format, component->priv->module->format))
      return MMAL_SUCCESS;

   /* Check the format is supported */
   if (encoding_to_audio_format(port->format->encoding) == AUDIO_FORMAT_INVALID)
   {
      LOG_ERROR("port does not support '%4.4s'", (char *)&port->format->encoding);
      return MMAL_EINVAL;
   }

   /* Specific checks for PCM */
   if (port->format->encoding == MMAL_ENCODING_PCM_SIGNED)
   {

      if (port->format->es->audio.bits_per_sample != 16 &&
          port->format->es->audio.bits_per_sample != 32)
      {
         LOG_ERROR("port does not support '%4.4s' at %ibps",
                   (char *)&port->format->encoding,
                   port->format->es->audio.bits_per_sample);
         return MMAL_EINVAL;
      }

      if (!audio_channel_out_mask_from_count(port->format->es->audio.channels))
      {
         LOG_ERROR("%s invalid channels mask from %i", port->name,
            (int)port->format->es->audio.channels);
         return MMAL_ENOSYS;
      }
   }

   mmal_format_copy(component->priv->module->format, port->format);

   return aaf_track_create(component);
}

/** Enable processing on a port */
static MMAL_STATUS_T aaf_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PARAM_UNUSED(cb);

   if (module->track == NULL)
      status = aaf_port_set_format(port);
   if (status != MMAL_SUCCESS)
      return status;

   module->is_enabled = MMAL_TRUE;
   aaf_track_state_update(component);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T aaf_port_flush(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   while((buffer = mmal_queue_get(module->queue)))
      mmal_port_buffer_header_callback(port, buffer);

   module->lock->lock();
   module->bytes_queued = 0;
   module->lock->unlock();

   if (module->track == NULL)
      return MMAL_SUCCESS;

   module->track->stop();
   module->track->flush();
   module->state = TRACK_STATE_STOPPED;
   aaf_track_state_update(component);
   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T aaf_port_disable(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   module->is_enabled = MMAL_FALSE;
   aaf_track_state_update(component);

   return aaf_port_flush(port);
}

static MMAL_STATUS_T aaf_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned int bits_per_sample, channels, sample_rate;
   uint32_t aaf_bytes_queued = 0;
   int64_t latency, ts;

   /* Handle event buffers */
   if (buffer->cmd)
   {
      LOG_ERROR("discarding event %i on port %p", (int)buffer->cmd, port);
      buffer->length = 0;
      mmal_port_buffer_header_callback(port, buffer);
      return MMAL_SUCCESS;
   }

   if (module->status != MMAL_SUCCESS)
      return module->status;

   bits_per_sample = port->format->es->audio.bits_per_sample;
   channels = port->format->es->audio.channels;
   sample_rate = port->format->es->audio.sample_rate;

   if (port->format->encoding == MMAL_ENCODING_AC3 ||
       port->format->encoding == MMAL_ENCODING_EAC3)
   {
      uint32_t aaf_latency = 0;
      AudioSystem::getOutputLatency(&aaf_latency, AUDIO_STREAM_MUSIC);
      latency = aaf_latency * 1000LL;

      bits_per_sample = 16;
      channels = 2;
      if (port->format->encoding == MMAL_ENCODING_EAC3 &&
          sample_rate <= 48000)
         sample_rate *= 4;
      aaf_bytes_queued = module->track->frameCount();
   }
   else
   {
      latency = module->track->latency() * 1000LL;
   }

   /* Keep aaf_track_callback from sending more samples */
   module->lock->lock();

   module->bytes_queued += buffer->length;
   latency += (module->bytes_queued + aaf_bytes_queued) / channels /
      (bits_per_sample / 8) * 1000000LL / sample_rate;
   ts = buffer->pts - latency;

   module->lock->unlock();

   mmal_port_clock_media_time_set(component->clock[0], ts);

   mmal_queue_put(module->queue, buffer);

   return MMAL_SUCCESS;
}

void aaf_clock_event(MMAL_PORT_T *port, const MMAL_CLOCK_EVENT_T *event)
{
   MMAL_COMPONENT_T *component = port->component;

   switch (event->id)
   {
   case MMAL_CLOCK_EVENT_SCALE:
      aaf_track_state_update(component);
      break;
   case MMAL_CLOCK_EVENT_TIME:
   case MMAL_CLOCK_EVENT_REFERENCE:
   case MMAL_CLOCK_EVENT_ACTIVE:
      /* nothing to do */
      break;
   default:
      LOG_DEBUG("unknown event id %4.4s", (char*)&event->id);
      break;
   }
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_aaf(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;

   /* Check we're the requested component */
   if(strcmp(name, "aaf." MMAL_AUDIO_RENDER))
      return MMAL_ENOENT;

   /* Allocate our module context */
   component->priv->module = module = (MMAL_COMPONENT_MODULE_T *)vcos_calloc(1, sizeof(*module), "mmal module");
   if(!module)
      return MMAL_ENOMEM;
   module->lock = new Mutex();
   if (!module->lock)
      goto error;

   /* Allocate the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, 0);
   if(!component->input)
      goto error;
   component->input_num = 1;

   /* Create the clock port (clock ports are managed by the framework) */
   component->clock = mmal_ports_clock_alloc(component, 1, 0, aaf_clock_event);
   if (!component->clock)
      goto error;
   component->clock_num = 1;

   module->queue = mmal_queue_create();
   if(!module->queue)
      goto error;

   module->format = mmal_format_alloc();
   if(!module->format)
      goto error;

   component->input[0]->priv->pf_set_format = aaf_port_set_format;
   component->input[0]->priv->pf_enable = aaf_port_enable;
   component->input[0]->priv->pf_disable = aaf_port_disable;
   component->input[0]->priv->pf_flush = aaf_port_flush;
   component->input[0]->priv->pf_send = aaf_port_send;
   component->input[0]->buffer_num_min = INPUT_MIN_BUFFER_NUM;
   component->input[0]->buffer_num_recommended = INPUT_RECOMMENDED_BUFFER_NUM;
   component->input[0]->format->type = MMAL_ES_TYPE_AUDIO;

   component->priv->pf_destroy = aaf_component_destroy;
   return MMAL_SUCCESS;

 error:
   aaf_component_destroy(component);
   return MMAL_ENOMEM;
}

MMAL_CONSTRUCTOR(mmal_register_component_aaf_audio);
void mmal_register_component_aaf_audio(void)
{
   mmal_component_supplier_register("aaf", mmal_component_create_aaf);
}
