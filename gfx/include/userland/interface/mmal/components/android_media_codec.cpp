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
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "mmal_logging.h"

#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/ICrypto.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <binder/ProcessState.h>

using namespace android;

/* List of variants supported by this component */
#define AMC_VARIANT_UNKNOWN 0
#define AMC_VARIANT_AUDIO_DECODE 1
#define AMC_VARIANT_AUDIO_DECODE_NAME "audio_decode"

/*****************************************************************************/
struct AmcHandler :  public AHandler
{
   AmcHandler(MMAL_COMPONENT_T *component) :
      mComponent(component), mNotificationRequested(false) {}

   void requestNotification(sp<MediaCodec> &codec)
   {
      if (mActivityNotify == NULL)
         mActivityNotify = new AMessage(0, id());

      if (!mNotificationRequested)
      {
         mNotificationRequested = true;
         codec->requestActivityNotification(mActivityNotify->dup());
      }
   }

   void reset()
   {
      mNotificationRequested = false;
   }

protected:
   virtual void onMessageReceived(const sp<AMessage> &msg)
   {
      (void)msg;
      mNotificationRequested = false;
      mmal_component_action_trigger(mComponent);
   }

private:
   MMAL_COMPONENT_T *mComponent;
   sp<AMessage> mActivityNotify;
   bool mNotificationRequested;
};

typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status; /**< current status of the component */

   sp<MediaCodec> codec;
   sp<AmcHandler> ahandler;
   sp<ALooper> alooper;
   Vector<sp<ABuffer> > input_buffers; /**< list of buffers exported by mediacodec */
   Vector<sp<ABuffer> > output_buffers; /**< list of buffers exported by mediacodec */

} MMAL_COMPONENT_MODULE_T;

typedef struct MMAL_PORT_MODULE_T
{
   MMAL_ES_FORMAT_T *format; /**< format currently configured */
   MMAL_QUEUE_T *queue; /**< queue for the buffers sent to the ports */
   MMAL_BOOL_T needs_configuring; /**< port is waiting for a format commit */

   List<size_t> *dequeued; /* buffers already dequeued from the codec */

   const char *mime;
   unsigned int actual_channels;

} MMAL_PORT_MODULE_T;

static struct encoding_table_t {
   const char *mime;
   MMAL_FOURCC_T encoding;
   MMAL_ES_TYPE_T type;
   MMAL_FOURCC_T encoding_variant;
} encoding_list[] =
{  {"audio/3gpp", MMAL_ENCODING_AMRNB, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/amr-wb", MMAL_ENCODING_AMRWB, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/mpeg", MMAL_ENCODING_MPGA, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/mp4a-latm", MMAL_ENCODING_MP4A, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/mp4a-latm", MMAL_ENCODING_MP4A, MMAL_ES_TYPE_AUDIO, MMAL_ENCODING_VARIANT_MP4A_ADTS},
  {"audio/vorbis", MMAL_ENCODING_VORBIS, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/g711-alaw", MMAL_ENCODING_ALAW, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/g711-ulaw", MMAL_ENCODING_MULAW, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/ac3", MMAL_ENCODING_AC3, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/ec3", MMAL_ENCODING_EAC3, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/eac3", MMAL_ENCODING_EAC3, MMAL_ES_TYPE_AUDIO, 0},
  {"audio/raw", MMAL_ENCODING_PCM_SIGNED, MMAL_ES_TYPE_AUDIO, 0},
  {"", MMAL_ENCODING_PCM_SIGNED, MMAL_ES_TYPE_AUDIO, 0},
  { 0, 0, MMAL_ES_TYPE_UNKNOWN, 0}
};

static const char *encoding_to_mime(MMAL_ES_TYPE_T type, MMAL_FOURCC_T encoding,
   MMAL_FOURCC_T encoding_variant)
{
   struct encoding_table_t *entry = encoding_list;

   for (entry = encoding_list; entry->mime; entry++)
      if (entry->encoding == encoding && entry->type == type &&
          entry->encoding_variant == encoding_variant)
         break;

   return entry->mime;
}

static void mime_to_encoding(const char *mime,
   MMAL_ES_TYPE_T *type, MMAL_FOURCC_T *encoding, MMAL_FOURCC_T *encoding_variant)
{
   struct encoding_table_t *entry = encoding_list;

   for (entry = encoding_list; entry->mime; entry++)
      if (!strcmp(mime, entry->mime))
         break;

   *encoding = entry->encoding;
   *type = entry->type;
   *encoding_variant = entry->encoding_variant;
}

/*****************************************************************************/

/** Actual processing functions */
static MMAL_BOOL_T amc_do_input_processing(MMAL_COMPONENT_T *component,
   MMAL_BOOL_T *notification)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port = component->input[0];
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_BUFFER_HEADER_T *in;
   status_t astatus;
   size_t size, index;
   uint32_t flags = 0;

   in = mmal_queue_get(port_module->queue);

   /* Get an input buffer from the codec. We dequeue input buffers even
    * if we do not any data to process to make sure that we do not get
    * flooded with notifications from the codec that buffers are
    * available. */
   if (port_module->dequeued->empty() || !in)
   {
      astatus = module->codec->dequeueInputBuffer(&index, 0ll);
      if (astatus == OK)
      {
         LOG_TRACE("dequeueInputBuffer %i", index);
         port_module->dequeued->push_back(index);
      }
      else if (astatus != -EAGAIN)
      {
         LOG_TRACE("dequeueInputBuffer failed (%i)", astatus);
      }
   }

   /* Check whether we can process data */
   if (!in)
   {
      return 0;
   }
   else if (port_module->dequeued->empty())
   {
      mmal_queue_put_back(port_module->queue, in);

      /* We have data we want to process so request to be notified as soon
       * as the codec is available to process it */
      *notification |= MMAL_TRUE;

      return 0;
   }

   /* We have some processing to do */

   index = *port_module->dequeued->begin();
   sp<ABuffer> inBuf = module->input_buffers.itemAt(index);
   if (inBuf->capacity() < in->length)
      LOG_ERROR("MediaCodec input buffer too small (%i/%i)",
         (int)inBuf->capacity(), (int)in->length);
   size = MMAL_MIN(inBuf->capacity(), in->length);

   if (in->length)
      memcpy(inBuf->data(), in->data + in->offset, size);
   if (in->flags & MMAL_BUFFER_HEADER_FLAG_EOS)
      flags |= MediaCodec::BUFFER_FLAG_EOS;
   if (in->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
      flags |= MediaCodec::BUFFER_FLAG_SYNCFRAME;
   if (in->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
      flags |= MediaCodec::BUFFER_FLAG_CODECCONFIG;

   LOG_TRACE("queueInputBuffer %i %ibytes, %lldus", index, in->length, in->pts);
   astatus = module->codec->queueInputBuffer(index, 0, size, in->pts, flags);
   if (astatus != OK)
   {
      LOG_ERROR("queueInputBuffer failed (%i)", astatus);
      mmal_event_error_send(component, MMAL_EIO);
      module->status = MMAL_EIO;
   }

   /* Send buffers back */
   in->length = 0;
   mmal_port_buffer_header_callback(port, in);
   port_module->dequeued->erase(port_module->dequeued->begin());

   return 1;
}

static void amc_output_format_changed(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port = component->output[0];
   MMAL_EVENT_FORMAT_CHANGED_T *event;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_STATUS_T status;
   int32_t value;

   sp<AMessage> format = new AMessage;
   status_t astatus = module->codec->getOutputFormat(&format);
   LOG_DEBUG("INFO_FORMAT_CHANGED (%i): %s", astatus,
      format->debugString().c_str());

   /* Get an event buffer */
   status = mmal_port_event_get(port, &buffer, MMAL_EVENT_FORMAT_CHANGED);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("unable to get an event buffer");
      return;
   }
   event = mmal_event_format_changed_get(buffer);

   AString amime;
   format->findString("mime", &amime);
   mime_to_encoding(amime.c_str(),
      &event->format->type, &event->format->encoding,
      &event->format->encoding_variant);

   switch (port->format->type)
   {
   case MMAL_ES_TYPE_VIDEO:
      if (format->findInt32("width", &value))
         event->format->es->video.width = value;
      if (format->findInt32("height", &value))
         event->format->es->video.height = value;
      break;
   case MMAL_ES_TYPE_AUDIO:
      if (format->findInt32("channel-count", &value))
         event->format->es->audio.channels = value;
      if (format->findInt32("sample-rate", &value))
         event->format->es->audio.sample_rate = value;
      if (format->findInt32("bitrate", &value))
         event->format->bitrate = value;
      if (event->format->encoding == MMAL_ENCODING_PCM_SIGNED)
         event->format->es->audio.bits_per_sample = 16;
      break;
   default:
      break;
   }

   /* Work-around for the ril audio_render component which only supports
    * power of 2 arrangements */
   if (event->format->type == MMAL_ES_TYPE_AUDIO &&
       event->format->encoding == MMAL_ENCODING_PCM_SIGNED)
   {
      port->priv->module->actual_channels = event->format->es->audio.channels;
      if (event->format->es->audio.channels == 6)
         event->format->es->audio.channels = 8;
   }

   /* Update current format */
   mmal_format_copy(port->priv->module->format, event->format);

   /* Pass on the buffer requirements */
   event->buffer_num_min = port->buffer_num_min;
   event->buffer_size_min = port->buffer_size_min;
   event->buffer_size_recommended = port->buffer_size_recommended;
   event->buffer_num_recommended = port->buffer_num_recommended;

   mmal_port_event_send(port, buffer);
}

static MMAL_BOOL_T amc_do_output_processing(MMAL_COMPONENT_T *component,
   MMAL_BOOL_T *notification)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_BUFFER_HEADER_T *out;
   status_t astatus;
   size_t size, offset, index;
   int64_t pts;
   uint32_t flags;

   if (port_out->priv->module->needs_configuring)
      return 0;

   out = mmal_queue_get(port_out->priv->module->queue);
   if (!out)
   {
      /* We do not want notifications in that case. We've already filled
       * our output buffers so we should really wait to receive more
       * output buffers before resuming the processing */
      *notification = MMAL_FALSE;
      return 0;
   }
   out->flags = 0;

   astatus = module->codec->dequeueOutputBuffer(&index, &offset, &size, &pts, &flags, 0ll);
   if (astatus != OK)
   {
      MMAL_BOOL_T do_more = 0;

      switch (astatus)
      {
      case INFO_OUTPUT_BUFFERS_CHANGED:
         LOG_DEBUG("INFO_OUTPUT_BUFFERS_CHANGED");
         astatus = module->codec->getOutputBuffers(&module->output_buffers);
         do_more = MMAL_TRUE;
         break;
      case INFO_FORMAT_CHANGED:
         amc_output_format_changed(component);
         port_out->priv->module->needs_configuring = 1;
         do_more = MMAL_TRUE;
         break;
      case -EAGAIN:
         /* We have data we want to process so request to be notified as soon
          * as the codec is available to process it */
         *notification |= MMAL_TRUE;
         break;
      default:
         LOG_ERROR("dequeueOutputBuffer failed (%i)", astatus);
      }

      mmal_queue_put_back(port_out->priv->module->queue, out);
      return do_more;
   }

   LOG_TRACE("dequeueOutputBuffer %i, %ibytes, %lldus, flags %x",
      index, size, pts, flags);
   sp<ABuffer> outBuf = module->output_buffers.itemAt(index);

   out->flags = 0;
   out->offset = 0;
   out->pts = pts;
   out->dts = 0;

   if (flags & MediaCodec::BUFFER_FLAG_EOS)
      out->flags |= MMAL_BUFFER_HEADER_FLAG_EOS;
   if (flags & MediaCodec::BUFFER_FLAG_SYNCFRAME)
      out->flags |= MMAL_BUFFER_HEADER_FLAG_KEYFRAME;
   if (flags & MediaCodec::BUFFER_FLAG_CODECCONFIG)
      out->flags |= MMAL_BUFFER_HEADER_FLAG_CONFIG;

   if (out->alloc_size < size)
      LOG_ERROR("MediaCodec output buffer too big (%i/%i)",
         (int)out->alloc_size, (int)size);
   size = MMAL_MIN(out->alloc_size, size);

   /* Audio render only accepts power of 2 channel configurations */
   if (port_out->format->type == MMAL_ES_TYPE_AUDIO &&
       port_out->format->encoding == MMAL_ENCODING_PCM_SIGNED &&
       port_out->priv->module->actual_channels !=
          port_out->format->es->audio.channels)
   {
      unsigned int valid = port_out->priv->module->actual_channels * 2;
      unsigned int pitch = port_out->format->es->audio.channels * 2;
      uint8_t *src = outBuf->data() + offset;
      uint8_t *dst = out->data;
      unsigned int i;

      size = size * port_out->format->es->audio.channels /
         port_out->priv->module->actual_channels;
      size = MMAL_MIN(out->alloc_size, size);
      memset(dst, 0, size);
      for (i = size / pitch; i; i--, src += valid, dst += pitch)
         memcpy(dst, src, valid);
   }
   else if (size)
      memcpy(out->data, outBuf->data() + offset, size);
   out->length = size;

   /* Send buffers back */
   module->codec->releaseOutputBuffer(index);
   mmal_port_buffer_header_callback(port_out, out);

   return 1;
}

static MMAL_BOOL_T amc_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BOOL_T do_more, request_notification = MMAL_FALSE;

   if (module->codec == NULL)
      return 0;

   /* Don't do anything if we've already seen an error */
   if (module->status != MMAL_SUCCESS)
      return 0;

   do_more = amc_do_input_processing(component, &request_notification);
   do_more |= amc_do_output_processing(component, &request_notification);

   if (request_notification)
      module->ahandler->requestNotification(module->codec);

   return do_more;
}

/*****************************************************************************/
static void amc_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (amc_do_processing(component));
}

/** Destroy a previously created component */
static MMAL_STATUS_T amc_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned int i;

   for(i = 0; i < component->input_num; i++)
   {
      if(component->input[i]->priv->module->queue)
         mmal_queue_destroy(component->input[i]->priv->module->queue);
      if(component->input[i]->priv->module->format)
         mmal_format_free(component->input[i]->priv->module->format);
      if(component->input[i]->priv->module->dequeued)
         delete component->input[i]->priv->module->dequeued;
   }
   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);

   for(i = 0; i < component->output_num; i++)
   {
      if(component->output[i]->priv->module->queue)
         mmal_queue_destroy(component->output[i]->priv->module->queue);
      if(component->output[i]->priv->module->format)
         mmal_format_free(component->output[i]->priv->module->format);
   }
   if(component->output_num)
      mmal_ports_free(component->output, component->output_num);

   if (module->codec != NULL)
   {
      module->codec->stop();
      module->codec->release();
   }

   module->alooper->unregisterHandler(module->ahandler->id());
   module->alooper->stop();
   delete module;
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T amc_codec_start(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *in = component->input[0];
   sp<AMessage> format = new AMessage;
   status_t astatus;
   const char *mime;

   mime = encoding_to_mime(in->format->type, in->format->encoding,
      in->format->encoding_variant);
   if (!mime)
   {
      LOG_ERROR("cannot match codec %4.4s(%4.4s) with a mime type",
         (char *)&in->format->encoding, (char *)&in->format->encoding_variant);
      return MMAL_EINVAL;
   }

   /* We need to restart MediaCodec when the codec type changes */
   if (module->codec == NULL || mime != in->priv->module->mime)
   {
      LOG_DEBUG("creating codec for %s", mime);

      /* Start by releasing any instance we've previously created */
      if (module->codec != NULL)
      {
         module->codec->stop();
         module->codec->release();
      }

      module->codec = MediaCodec::CreateByType(module->alooper, mime, false);
      if (module->codec == NULL)
      {
         LOG_ERROR("cannot instantiate MediaCodec for mime: %s", mime);
         return MMAL_EINVAL;
      }

      in->priv->module->mime = mime;
      module->ahandler->reset();
      LOG_TRACE("creation done");
   }
   /* When reusing an existing instance, we just need to stop it */
   else
   {
      module->codec->stop();
   }

   /* Configure MediaCodec */
   switch (in->format->type)
   {
   case MMAL_ES_TYPE_VIDEO:
      format->setInt32("width", in->format->es->video.width);
      format->setInt32("height", in->format->es->video.height);
      if (in->format->es->video.frame_rate.num && in->format->es->video.frame_rate.den)
         format->setFloat("frame-rate", in->format->es->video.frame_rate.num /
                          (float)in->format->es->video.frame_rate.den);
      break;
   case MMAL_ES_TYPE_AUDIO:
      format->setInt32("channel-count", in->format->es->audio.channels);
      format->setInt32("sample-rate", in->format->es->audio.sample_rate);
      format->setInt32("bitrate", in->format->bitrate);
      break;
   default:
      break;
   }

   format->setString("mime", mime);

   /* Handle the codec specific data */
   if (in->format->extradata_size)
   {
      sp<ABuffer> csd = new ABuffer(in->format->extradata,
         in->format->extradata_size);
      csd->meta()->setInt32("csd", true);
      csd->meta()->setInt64("timeUs", 0);
      format->setBuffer("csd-0", csd);
   }

   /* Propagate the buffer size setting of the input port to the
    * codec */
   format->setInt32("max-input-size", in->buffer_size);

   LOG_TRACE("configuring: %s", format->debugString().c_str());
   astatus = module->codec->configure(format, NULL, NULL, 0);
   if (astatus)
   {
      LOG_ERROR("configure failed (%i)", astatus);
      return MMAL_EINVAL;
   }

   LOG_TRACE("starting");
   astatus = module->codec->start();
   if (astatus != OK)
   {
      LOG_ERROR("failed to start codec (%i)", astatus);
      return MMAL_EINVAL;
   }
   LOG_TRACE("started");

   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T amc_input_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_STATUS_T status;
   status_t astatus;
   MMAL_PARAM_UNUSED(cb);

   /* Make sure the format as been committed */
   status = port->priv->pf_set_format(port);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("cannot commit port format (%i)", status);
      return status;
   }

   status = amc_codec_start(component);
   if (status != MMAL_SUCCESS)
      return status;

   astatus = module->codec->getInputBuffers(&module->input_buffers);
   if (astatus != OK)
   {
      LOG_ERROR("failed to get codec input buffers (%i)", astatus);
      return MMAL_EINVAL;
   }

   if (module->input_buffers.size())
      LOG_TRACE("%i input buffers of size %i", module->input_buffers.size(),
         module->input_buffers.itemAt(0)->capacity());

   astatus = module->codec->getOutputBuffers(&module->output_buffers);
   if (astatus != OK)
   {
      LOG_ERROR("failed to get codec output buffers (%i)", astatus);
      return MMAL_EINVAL;
   }

   if (module->output_buffers.size())
      LOG_TRACE("%i output buffers of size %i", module->output_buffers.size(),
         module->output_buffers.itemAt(0)->capacity());

   module->status = MMAL_SUCCESS;
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T amc_output_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T amc_port_flush(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   status_t astatus;

   /* Flush buffers that our component is holding on to */
   buffer = mmal_queue_get(port_module->queue);
   while(buffer)
   {
      mmal_port_buffer_header_callback(port, buffer);
      buffer = mmal_queue_get(port_module->queue);
   }

   if (port_module->dequeued)
      port_module->dequeued->clear();

   /* Flush codec itself */
   if (port->type == MMAL_PORT_TYPE_INPUT && module->codec != NULL)
   {
      astatus = module->codec->flush();
      if (astatus != OK)
         LOG_ERROR("failed to flush codec (%i)", astatus);
   }

   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T amc_port_disable(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   status_t astatus;

   if (port->type == MMAL_PORT_TYPE_INPUT)
   {
      astatus = module->codec->stop();
      if (astatus != OK)
         LOG_ERROR("failed to stop codec (%i)", astatus);
      module->codec->release();
      module->codec = NULL;
   }

   /* We just need to flush our internal queue */
   return amc_port_flush(port);
}

/** Send a buffer header to a port */
static MMAL_STATUS_T amc_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   mmal_queue_put(port->priv->module->queue, buffer);
   mmal_component_action_trigger(port->component);
   return MMAL_SUCCESS;
}

/** Set format on input port */
static MMAL_STATUS_T amc_input_port_format_commit(MMAL_PORT_T *in)
{
   MMAL_STATUS_T status;
   const char *mime;

   if (in->is_enabled)
      return MMAL_EINVAL;

   if (!mmal_format_compare(in->format, in->priv->module->format))
      return MMAL_SUCCESS;

   mime = encoding_to_mime(in->format->type, in->format->encoding,
      in->format->encoding_variant);
   if (!mime)
   {
      LOG_ERROR("cannot match codec %4.4s(%4.4s) with a mime type",
         (char *)&in->format->encoding, (char *)&in->format->encoding_variant);
      return MMAL_EINVAL;
   }

   /* Note that the MediaCodec object is only created when the input
    * port is enabled to avoid deadlocking when we're running inside an
    * Android OMX component (you can't create an OMX odec instance while
    * you're already instantiating one) */

   status = mmal_format_full_copy(in->priv->module->format, in->format);
   if (status != MMAL_SUCCESS)
      return status;

   /* No need to propagate anything to the output port since
    * we'll generate a format change event for it later on */

   return status;
}

/** Set format on output port */
static MMAL_STATUS_T amc_output_port_format_commit(MMAL_PORT_T *out)
{
   /* The format of the output port needs to match the output of
    * MediaCodec */
   if (mmal_format_compare(out->format, out->priv->module->format))
      return MMAL_EINVAL;

   out->priv->module->needs_configuring = 0;
   mmal_component_action_trigger(out->component);
   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_android_media_codec(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   const char *variant_name = name + sizeof("amc");
   unsigned int variant = AMC_VARIANT_UNKNOWN;

   if (!strcmp(variant_name, AMC_VARIANT_AUDIO_DECODE_NAME))
      variant = AMC_VARIANT_AUDIO_DECODE;

   if (variant == AMC_VARIANT_UNKNOWN)
   {
      LOG_ERROR("unsupported variant %s", variant_name);
      return MMAL_ENOENT;
   }

   /* Allocate the context for our module */
   component->priv->module = module = new MMAL_COMPONENT_MODULE_T;
   if (!module)
      return MMAL_ENOMEM;

   component->priv->pf_destroy = amc_component_destroy;
   module->status = MMAL_SUCCESS;
   ProcessState::self()->startThreadPool();
   module->ahandler = new AmcHandler(component);
   module->alooper = new ALooper;
   module->alooper->setName("amc_looper");
   module->alooper->registerHandler(module->ahandler);
   module->alooper->start(false);

   /* Allocate and initialise all the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->input)
      goto error;
   component->input_num = 1;
   component->input[0]->priv->pf_enable = amc_input_port_enable;
   component->input[0]->priv->pf_disable = amc_port_disable;
   component->input[0]->priv->pf_flush = amc_port_flush;
   component->input[0]->priv->pf_send = amc_port_send;
   component->input[0]->priv->pf_set_format = amc_input_port_format_commit;
   component->input[0]->buffer_num_min = 1;
   component->input[0]->buffer_num_recommended = 3;
   component->input[0]->priv->module->queue = mmal_queue_create();
   if(!component->input[0]->priv->module->queue)
      goto error;
   component->input[0]->priv->module->format = mmal_format_alloc();
   if(!component->input[0]->priv->module->format)
      goto error;
   component->input[0]->priv->module->dequeued = new List<size_t>;
   if(!component->input[0]->priv->module->dequeued)
      goto error;

   component->output = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_OUTPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->output)
      goto error;
   component->output_num = 1;
   component->output[0]->priv->pf_enable = amc_output_port_enable;
   component->output[0]->priv->pf_disable = amc_port_disable;
   component->output[0]->priv->pf_flush = amc_port_flush;
   component->output[0]->priv->pf_send = amc_port_send;
   component->output[0]->priv->pf_set_format = amc_output_port_format_commit;
   component->output[0]->buffer_num_min = 1;
   component->output[0]->buffer_num_recommended = 3;
   component->output[0]->priv->module->queue = mmal_queue_create();
   if(!component->output[0]->priv->module->queue)
      goto error;
   component->output[0]->priv->module->format = mmal_format_alloc();
   if(!component->output[0]->priv->module->format)
      goto error;

   status = mmal_component_action_register(component, amc_do_processing_loop);
   if (status != MMAL_SUCCESS)
      goto error;

   /* Setup ports according to selected variant */
   if (variant == AMC_VARIANT_AUDIO_DECODE)
   {
      component->input[0]->format->type = MMAL_ES_TYPE_AUDIO;
      component->input[0]->format->encoding = MMAL_ENCODING_EAC3;
      component->input[0]->format->es->audio.sample_rate = 48000;
      component->input[0]->format->es->audio.channels = 2;
      component->input[0]->buffer_size_min = 4 * 1024;

      component->output[0]->format->type = MMAL_ES_TYPE_AUDIO;
      component->output[0]->format->encoding = MMAL_ENCODING_PCM_SIGNED;
      component->output[0]->format->es->audio.sample_rate = 48000;
      component->output[0]->format->es->audio.channels = 2;
      component->output[0]->format->es->audio.bits_per_sample = 16;
      component->output[0]->buffer_size_min = 32 * 1024;
   }

   /* Update our current view of the output format */
   mmal_format_copy(component->output[0]->priv->module->format,
      component->output[0]->format);

   return MMAL_SUCCESS;

 error:
   amc_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_android_media_codec);
void mmal_register_component_android_media_codec(void)
{
   mmal_component_supplier_register("amc", mmal_component_create_android_media_codec);
}
