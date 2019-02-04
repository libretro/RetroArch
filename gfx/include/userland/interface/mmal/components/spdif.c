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

#define SPDIF_AC3_FRAME_SIZE 6144
#define SPDIF_EAC3_FRAME_SIZE (6144*4)
#define SPDIF_FRAME_SIZE SPDIF_EAC3_FRAME_SIZE

/* Buffering requirements */
#define INPUT_MIN_BUFFER_SIZE SPDIF_FRAME_SIZE
#define INPUT_MIN_BUFFER_NUM 2
#define OUTPUT_MIN_BUFFER_SIZE SPDIF_FRAME_SIZE
#define OUTPUT_MIN_BUFFER_NUM 2

/*****************************************************************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status; /**< current status of the component */

} MMAL_COMPONENT_MODULE_T;

typedef struct MMAL_PORT_MODULE_T
{
   MMAL_QUEUE_T *queue; /**< queue for the buffers sent to the ports */
   MMAL_BOOL_T needs_configuring; /**< port is waiting for a format commit */

} MMAL_PORT_MODULE_T;

/*****************************************************************************/

/*****************************************************************************/

static MMAL_STATUS_T spdif_send_event_format_changed(MMAL_COMPONENT_T *component, MMAL_PORT_T *port,
   MMAL_ES_FORMAT_T *format)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer = NULL;
   MMAL_EVENT_FORMAT_CHANGED_T *event;

   /* Get an event buffer */
   module->status = mmal_port_event_get(port, &buffer, MMAL_EVENT_FORMAT_CHANGED);
   if (module->status != MMAL_SUCCESS)
   {
      LOG_ERROR("unable to get an event buffer");
      return module->status;
   }
   /* coverity[returned_null] Can't return null or call above would have failed */
   event = mmal_event_format_changed_get(buffer);

   /* Fill in the new format */
   if (port->format->encoding == MMAL_ENCODING_PCM_SIGNED)
      mmal_format_copy(event->format, port->format);
   else
      mmal_format_copy(event->format, format);

   event->format->es->audio.sample_rate = format->es->audio.sample_rate;

   /* Pass on the buffer requirements */
   event->buffer_num_min = port->buffer_num_min;
   event->buffer_size_min = port->buffer_size_min;
   event->buffer_size_recommended = event->buffer_size_min;
   event->buffer_num_recommended = port->buffer_num_recommended;

   port->priv->module->needs_configuring = 1;
   mmal_port_event_send(port, buffer);
   return MMAL_SUCCESS;
}

/** Actual processing function */
static MMAL_BOOL_T spdif_do_processing(MMAL_COMPONENT_T *component)
{
   static const uint8_t ac3_spdif_header[6] = {0x72,0xF8,0x1F,0x4E,0x1, 0};
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port_in = component->input[0];
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_BUFFER_HEADER_T *in, *out;
   unsigned int i, sample_rate, frame_size, spdif_frame_size;
   uint8_t *in_data;

   if (port_out->priv->module->needs_configuring)
      return 0;

   in = mmal_queue_get(port_in->priv->module->queue);
   if (!in)
      return 0;

   /* Handle event buffers */
   if (in->cmd)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(in);
      if (event)
      {
         module->status = mmal_format_full_copy(port_in->format, event->format);
         if (module->status == MMAL_SUCCESS)
            module->status = port_in->priv->pf_set_format(port_in);
         if (module->status != MMAL_SUCCESS)
         {
            LOG_ERROR("format not set on port %s %p (%i)", port_in->name, port_in, module->status);
            if (mmal_event_error_send(component, module->status) != MMAL_SUCCESS)
               LOG_ERROR("unable to send an error event buffer");
         }
      }
      else
      {
         LOG_ERROR("discarding event %i on port %s %p", (int)in->cmd, port_in->name, port_in);
      }

      in->length = 0;
      mmal_port_buffer_header_callback(port_in, in);
      return 1;
   }

   /* Don't do anything if we've already seen an error */
   if (module->status != MMAL_SUCCESS)
   {
      mmal_queue_put_back(port_in->priv->module->queue, in);
      return 0;
   }

   /* Discard empty buffers */
   if (!in->length && !in->flags)
   {
      mmal_port_buffer_header_callback(port_in, in);
      return 1;
   }
   /* Discard codec config data as it's not needed */
   if (in->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
   {
      LOG_DEBUG("config buffer %ibytes", in->length);
      in->length = 0;
      mmal_port_buffer_header_callback(port_in, in);
      return 1;
   }

   out = mmal_queue_get(port_out->priv->module->queue);
   if (!out)
   {
      mmal_queue_put_back(port_in->priv->module->queue, in);
      return 0;
   }

   spdif_frame_size = SPDIF_AC3_FRAME_SIZE;
   if (port_out->format->encoding == MMAL_ENCODING_EAC3)
      spdif_frame_size = SPDIF_EAC3_FRAME_SIZE;

   /* Sanity check the output buffer is big enough */
   if (out->alloc_size < spdif_frame_size)
   {
      module->status = MMAL_EINVAL;
      if (mmal_event_error_send(component, module->status) != MMAL_SUCCESS)
         LOG_ERROR("unable to send an error event buffer");
      mmal_queue_put_back(port_in->priv->module->queue, in);
      mmal_queue_put_back(port_out->priv->module->queue, out);
      return 0;
   }

   /* Special case for empty buffers carrying a flag */
   if (!in->length && in->flags)
   {
      out->length = 0;
      goto end;
   }

   LOG_DEBUG("frame: %lld, size %i", in->pts, in->length);
   mmal_buffer_header_mem_lock(out);
   mmal_buffer_header_mem_lock(in);
   in_data = in->data + in->offset;

   /* Sanity check we're dealing with an AC3 frame */
   if (in->length < 5)
   {
      LOG_ERROR("invalid data size (%i bytes)", in->length);
      goto discard;
   }

   if (!(in_data[0] == 0x0B || in_data[1] == 0x77) &&
       !(in_data[0] == 0x77 || in_data[1] == 0x0B))
   {
      LOG_ERROR("invalid data (%i bytes): %2.2x,%2.2x,%2.2x,%2.2x",
         in->length, in_data[0], in_data[1], in_data[2], in_data[3]);
      goto discard;
   }

   /* We need to make sure we use the right sample rate
    * to be able to play this at the right rate */
   if ((in_data[4] & 0xC0) == 0x40) sample_rate = 44100;
   else if ((in_data[4] & 0xC0) == 0x80) sample_rate = 32000;
   else sample_rate = 48000;

   /* If the sample rate changes, stop everything we're doing
    * and signal the format change. */
   if (sample_rate != port_out->format->es->audio.sample_rate)
   {
      LOG_INFO("format change: %i->%i",
         port_out->format->es->audio.sample_rate, sample_rate);
      port_in->format->es->audio.sample_rate = sample_rate;
      spdif_send_event_format_changed(component, port_out, port_in->format);
      mmal_buffer_header_mem_unlock(in);
      mmal_buffer_header_mem_unlock(out);
      mmal_queue_put_back(port_in->priv->module->queue, in);
      mmal_queue_put_back(port_out->priv->module->queue, out);
      return 0;
   }

   /* Build our S/PDIF frame. We assume that we need to send
    * little endian S/PDIF data. */
   if (in->length > spdif_frame_size - 8)
      LOG_ERROR("frame too big, truncating (%i/%i bytes)",
         in->length, spdif_frame_size - 8);
   frame_size = MMAL_MIN(in->length, spdif_frame_size - 8) / 2;
   memcpy(out->data, ac3_spdif_header, sizeof(ac3_spdif_header));
   out->data[5] = in_data[5] & 0x7; /* bsmod */
   out->data[6] = frame_size & 0xFF;
   out->data[7] = frame_size >> 8;

   /* Copy the AC3 data, reverting the endianness if required */
   if (in_data[0] == 0x0B)
   {
      for (i = 0; i < frame_size; i++)
      {
         out->data[8+i*2] = in_data[in->offset+i*2+1];
         out->data[8+i*2+1] = in_data[in->offset+i*2];
      }
   }
   else
      memcpy(out->data + 8, in_data, in->length);

   /* The S/PDIF frame needs to be padded */
   memset(out->data + 8 + frame_size * 2, 0,
      spdif_frame_size - frame_size * 2 - 8);
   mmal_buffer_header_mem_unlock(in);
   mmal_buffer_header_mem_unlock(out);
   out->length     = spdif_frame_size;
 end:
   out->offset     = 0;
   out->flags      = in->flags;
   out->pts        = in->pts;
   out->dts        = in->dts;

   /* Send buffers back */
   in->length = 0;
   mmal_port_buffer_header_callback(port_in, in);
   mmal_port_buffer_header_callback(port_out, out);
   return 1;

 discard:
   mmal_buffer_header_mem_unlock(in);
   mmal_buffer_header_mem_unlock(out);
   in->length = 0;
   mmal_queue_put_back(port_out->priv->module->queue, out);
   mmal_port_buffer_header_callback(port_in, in);
   return 1;
}

/*****************************************************************************/
static void spdif_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (spdif_do_processing(component));
}

/** Destroy a previously created component */
static MMAL_STATUS_T spdif_component_destroy(MMAL_COMPONENT_T *component)
{
   unsigned int i;

   for(i = 0; i < component->input_num; i++)
      if(component->input[i]->priv->module->queue)
         mmal_queue_destroy(component->input[i]->priv->module->queue);
   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);

   for(i = 0; i < component->output_num; i++)
      if(component->output[i]->priv->module->queue)
         mmal_queue_destroy(component->output[i]->priv->module->queue);
   if(component->output_num)
      mmal_ports_free(component->output, component->output_num);

   vcos_free(component->priv->module);
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T spdif_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(cb);

   /* We need to propagate the buffer requirements when the input port is
    * enabled */
   if (port->type == MMAL_PORT_TYPE_INPUT)
      return port->priv->pf_set_format(port);

   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T spdif_port_flush(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   /* Flush buffers that our component is holding on to */
   buffer = mmal_queue_get(port_module->queue);
   while(buffer)
   {
      mmal_port_buffer_header_callback(port, buffer);
      buffer = mmal_queue_get(port_module->queue);
   }

   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T spdif_port_disable(MMAL_PORT_T *port)
{
   /* We just need to flush our internal queue */
   return spdif_port_flush(port);
}

/** Send a buffer header to a port */
static MMAL_STATUS_T spdif_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   mmal_queue_put(port->priv->module->queue, buffer);
   mmal_component_action_trigger(port->component);
   return MMAL_SUCCESS;
}

/** Set format on input port */
static MMAL_STATUS_T spdif_input_port_format_commit(MMAL_PORT_T *in)
{
   MMAL_COMPONENT_T *component = in->component;
   MMAL_PORT_T *out = component->output[0];

   /* Sanity check we cope with this format */
   if (in->format->encoding != MMAL_ENCODING_AC3 &&
       in->format->encoding != MMAL_ENCODING_EAC3)
      return MMAL_ENXIO;

   LOG_INFO("%4.4s, %iHz, %ichan, %ibps", (char *)&in->format->encoding,
      in->format->es->audio.sample_rate, in->format->es->audio.channels,
      in->format->bitrate);

   /* TODO: should we check the bitrate to see if that fits in an S/PDIF
    * frame? */

   /* Check if there's anything to propagate to the output port */
   if (!mmal_format_compare(in->format, out->format))
      return MMAL_SUCCESS;
   if (out->format->encoding == MMAL_ENCODING_PCM_SIGNED &&
       in->format->es->audio.sample_rate ==
          out->format->es->audio.sample_rate)
      return MMAL_SUCCESS;

   /* If the output port is not enabled we just need to update its format.
    * Otherwise we'll have to trigger a format changed event for it. */
   if (!out->is_enabled)
   {
      if (out->format->encoding != MMAL_ENCODING_PCM_SIGNED)
         mmal_format_copy(out->format, in->format);
      out->format->es->audio.sample_rate = in->format->es->audio.sample_rate;
      return MMAL_SUCCESS;
   }

   /* Send an event on the output port */
   return spdif_send_event_format_changed(component, out, in->format);
}

/** Set format on output port */
static MMAL_STATUS_T spdif_output_port_format_commit(MMAL_PORT_T *out)
{
   int supported = 0;

   if (out->format->type == MMAL_ES_TYPE_AUDIO &&
       out->format->encoding == MMAL_ENCODING_PCM_SIGNED &&
       out->format->es->audio.channels == 2 &&
       out->format->es->audio.bits_per_sample == 16)
      supported = 1;
   if (out->format->type == MMAL_ES_TYPE_AUDIO &&
       (out->format->encoding == MMAL_ENCODING_AC3 ||
        out->format->encoding == MMAL_ENCODING_EAC3))
      supported = 1;

   if (!supported)
   {
      LOG_ERROR("invalid format %4.4s, %ichan, %ibps",
         (char *)&out->format->encoding, out->format->es->audio.channels,
         out->format->es->audio.bits_per_sample);
      return MMAL_EINVAL;
   }

   out->priv->module->needs_configuring = 0;
   mmal_component_action_trigger(out->component);
   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_spdif(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   MMAL_PARAM_UNUSED(name);

   /* Allocate the context for our module */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;
   memset(module, 0, sizeof(*module));

   component->priv->pf_destroy = spdif_component_destroy;

   /* Allocate and initialise all the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->input)
      goto error;
   component->input_num = 1;
   component->input[0]->priv->pf_enable = spdif_port_enable;
   component->input[0]->priv->pf_disable = spdif_port_disable;
   component->input[0]->priv->pf_flush = spdif_port_flush;
   component->input[0]->priv->pf_send = spdif_port_send;
   component->input[0]->priv->pf_set_format = spdif_input_port_format_commit;
   component->input[0]->priv->module->queue = mmal_queue_create();
   if(!component->input[0]->priv->module->queue)
      goto error;

   component->output = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_OUTPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->output)
      goto error;
   component->output_num = 1;
   component->output[0]->priv->pf_enable = spdif_port_enable;
   component->output[0]->priv->pf_disable = spdif_port_disable;
   component->output[0]->priv->pf_flush = spdif_port_flush;
   component->output[0]->priv->pf_send = spdif_port_send;
   component->output[0]->priv->pf_set_format = spdif_output_port_format_commit;
   component->output[0]->priv->module->queue = mmal_queue_create();
   if(!component->output[0]->priv->module->queue)
      goto error;

   status = mmal_component_action_register(component, spdif_do_processing_loop);
   if (status != MMAL_SUCCESS)
      goto error;

   component->input[0]->format->type = MMAL_ES_TYPE_AUDIO;
   component->input[0]->format->encoding = MMAL_ENCODING_AC3;
   component->input[0]->buffer_size_min =
      component->input[0]->buffer_size_recommended = INPUT_MIN_BUFFER_SIZE;
   component->input[0]->buffer_num_min =
      component->input[0]->buffer_num_recommended = INPUT_MIN_BUFFER_NUM;

   component->output[0]->format->type = MMAL_ES_TYPE_AUDIO;
   component->output[0]->format->encoding = MMAL_ENCODING_AC3;
   component->output[0]->format->es->audio.channels = 2;
   component->output[0]->format->es->audio.bits_per_sample = 16;
   component->output[0]->buffer_size_min =
      component->output[0]->buffer_size_recommended = OUTPUT_MIN_BUFFER_SIZE;
   component->output[0]->buffer_num_min =
      component->output[0]->buffer_num_recommended = OUTPUT_MIN_BUFFER_NUM;

   return MMAL_SUCCESS;

 error:
   spdif_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_spdif);
void mmal_register_component_spdif(void)
{
   mmal_component_supplier_register("spdif", mmal_component_create_spdif);
}
