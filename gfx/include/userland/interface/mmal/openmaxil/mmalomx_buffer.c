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

#include "mmalomx.h"
#include "mmalomx_buffer.h"
#include "mmalomx_commands.h"
#include "mmalomx_marks.h"
#include "mmalomx_logging.h"

#include <util/mmal_util.h>

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_buffer_send(
   MMALOMX_COMPONENT_T *component,
   OMX_BUFFERHEADERTYPE *omx_buffer,
   OMX_DIRTYPE direction)
{
   OMX_ERRORTYPE status = OMX_ErrorNone;
   MMAL_BUFFER_HEADER_T *mmal_buffer;
   MMAL_STATUS_T mmal_status;
   MMALOMX_PORT_T *port;
   unsigned int index;

   /* Sanity checks */
   if (!component)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   if (!omx_buffer || omx_buffer->nSize != sizeof(OMX_BUFFERHEADERTYPE) ||
       omx_buffer->nOffset + omx_buffer->nFilledLen > omx_buffer->nAllocLen)
      return OMX_ErrorBadParameter;

   index = direction == OMX_DirInput ? omx_buffer->nInputPortIndex : omx_buffer->nOutputPortIndex;
   if (index >= component->ports_num)
      return OMX_ErrorBadPortIndex;

   port = &component->ports[index];
   if (port->direction != direction)
      return OMX_ErrorBadPortIndex;

   MMALOMX_LOCK_PORT(component, port);

   if (component->state != OMX_StatePause && component->state != OMX_StateExecuting)
      status = OMX_ErrorIncorrectStateOperation;
   if (!port->enabled  /* FIXME: || flushing || pending idle */)
      status = OMX_ErrorIncorrectStateOperation;
   if (status != OMX_ErrorNone)
      goto error;

   mmal_buffer = mmal_queue_get( port->pool->queue );
   if (!vcos_verify(mmal_buffer)) /* Should never happen */
   {
      status = OMX_ErrorUndefined;
      goto error;
   }

   mmalomx_mark_process_incoming(component, port, omx_buffer);

   mmal_buffer->user_data = (void *)omx_buffer;
   mmalil_buffer_header_to_mmal(mmal_buffer, omx_buffer);

   mmal_status = mmal_port_send_buffer(port->mmal, mmal_buffer);
   if (!vcos_verify(mmal_status == MMAL_SUCCESS))
   {
      LOG_ERROR("failed to send buffer on %s", port->mmal->name);
      mmal_queue_put_back( port->pool->queue, mmal_buffer );
      status = mmalil_error_to_omx(mmal_status);
   }
   else
   {
      port->buffers_in_transit++;
   }

error:
   MMALOMX_UNLOCK_PORT(component, port);
   return status;
}

/*****************************************************************************/
static void mmalomx_buffer_event(
   MMALOMX_PORT_T *port,
   MMAL_BUFFER_HEADER_T *mmal_buffer)
{
   MMALOMX_COMPONENT_T *component = port->component;
   MMAL_EVENT_FORMAT_CHANGED_T *event;

   LOG_TRACE("hComponent %p, port %i, event %4.4s", component, port->index,
             (char *)&mmal_buffer->cmd);

   if (mmal_buffer->cmd == MMAL_EVENT_ERROR )
   {
      mmalomx_callback_event_handler(component, OMX_EventError,
         mmalil_error_to_omx(*(MMAL_STATUS_T *)mmal_buffer->data), 0, NULL);
      return;
   }

   event = mmal_event_format_changed_get(mmal_buffer);
   if (event && port->mmal->type == MMAL_PORT_TYPE_OUTPUT &&
       port->mmal->format->type == MMAL_ES_TYPE_VIDEO)
   {
      uint32_t diff = mmal_format_compare(event->format, port->mmal->format);
      MMAL_ES_FORMAT_T *format = port->mmal->format;
      MMAL_VIDEO_FORMAT_T video = format->es->video;

      /* Update the port settings with the new values */
      mmal_format_copy(format, event->format);
      port->mmal->buffer_num_min = event->buffer_num_min;
      port->mmal->buffer_size_min = event->buffer_size_min;
      port->format_changed = MMAL_TRUE;

      if ((diff & MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_ASPECT_RATIO) &&
          /* Do not report a change if going from unspecified to 1:1 */
          !(format->es->video.par.num == format->es->video.par.den && !video.par.num))
      {
         LOG_DEBUG("aspect ratio change %ix%i->%ix%i", (int)video.par.num, (int)video.par.den,
                   (int)format->es->video.par.num, (int)format->es->video.par.den);
         mmalomx_callback_event_handler(component, OMX_EventPortSettingsChanged,
                                        port->index, OMX_IndexParamBrcmPixelAspectRatio, NULL);
      }

      if (diff & (MMAL_ES_FORMAT_COMPARE_FLAG_ENCODING|
                  MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_RESOLUTION|
                  MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_CROPPING))
      {
         LOG_DEBUG("format change %ix%i(%ix%i) -> %ix%i(%ix%i)",
                   (int)video.width, (int)video.height,
                   (int)video.crop.width, (int)video.crop.height,
                   (int)format->es->video.width, (int)format->es->video.height,
                   (int)format->es->video.crop.width, (int)format->es->video.crop.height);
         mmalomx_callback_event_handler(component, OMX_EventPortSettingsChanged,
                                        port->index, 0, NULL);
      }
   }
   else if (event && port->mmal->type == MMAL_PORT_TYPE_OUTPUT &&
       port->mmal->format->type == MMAL_ES_TYPE_AUDIO)
   {
      uint32_t diff = mmal_format_compare(event->format, port->mmal->format);
      MMAL_ES_FORMAT_T *format = port->mmal->format;
      MMAL_AUDIO_FORMAT_T audio = format->es->audio;

      /* Update the port settings with the new values */
      mmal_format_copy(format, event->format);
      port->mmal->buffer_num_min = event->buffer_num_min;
      port->mmal->buffer_size_min = event->buffer_size_min;
      port->format_changed = MMAL_TRUE;

      if (diff)
      {
         LOG_DEBUG("format change %ich, %iHz, %ibps -> %ich, %iHz, %ibps",
                   (int)audio.channels, (int)audio.sample_rate,
                   (int)audio.bits_per_sample,
                   (int)format->es->audio.channels,
                   (int)format->es->audio.sample_rate,
                   (int)format->es->audio.bits_per_sample);
         mmalomx_callback_event_handler(component, OMX_EventPortSettingsChanged,
                                        port->index, 0, NULL);
      }
   }
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_buffer_return(
   MMALOMX_PORT_T *port,
   MMAL_BUFFER_HEADER_T *mmal_buffer)
{
   MMALOMX_COMPONENT_T *component = port->component;
   OMX_BUFFERHEADERTYPE *omx_buffer = (OMX_BUFFERHEADERTYPE *)mmal_buffer->user_data;
   MMAL_BOOL_T signal;

   if (mmal_buffer->cmd)
   {
      mmalomx_buffer_event(port, mmal_buffer);
      mmal_buffer_header_release(mmal_buffer);
      return OMX_ErrorNone;
   }

   if (ENABLE_MMAL_EXTRA_LOGGING)
      LOG_TRACE("hComponent %p, port %i, pBuffer %p", component,
                port->index, omx_buffer);

   vcos_assert(omx_buffer->pBuffer == mmal_buffer->data);
   mmalil_buffer_header_to_omx(omx_buffer, mmal_buffer);
   mmal_buffer_header_release(mmal_buffer);

   if ((omx_buffer->nFlags & OMX_BUFFERFLAG_EOS) && port->direction == OMX_DirOutput)
   {
      mmalomx_callback_event_handler(component, OMX_EventBufferFlag,
                                     port->index, omx_buffer->nFlags, NULL);
   }

   mmalomx_mark_process_outgoing(component, port, omx_buffer);

   if (port->direction == OMX_DirInput)
      component->callbacks.EmptyBufferDone((OMX_HANDLETYPE)&component->omx,
         component->callbacks_data, omx_buffer );
   else
      component->callbacks.FillBufferDone((OMX_HANDLETYPE)&component->omx,
         component->callbacks_data, omx_buffer );

   MMALOMX_LOCK_PORT(component, port);
   signal = port->actions & MMALOMX_ACTION_CHECK_FLUSHED;
   port->buffers_in_transit--;
   MMALOMX_UNLOCK_PORT(component, port);

   if (signal)
      mmalomx_commands_actions_signal(component);

   return OMX_ErrorNone;
}

