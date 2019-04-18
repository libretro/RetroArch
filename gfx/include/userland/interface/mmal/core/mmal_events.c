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
#include "mmal_port_private.h"
#include "mmal_buffer.h"
#include "mmal_logging.h"

MMAL_EVENT_FORMAT_CHANGED_T *mmal_event_format_changed_get(MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_EVENT_FORMAT_CHANGED_T *event;
   MMAL_ES_FORMAT_T *format;
   uint32_t size;

   size = sizeof(MMAL_EVENT_FORMAT_CHANGED_T);
   size += sizeof(MMAL_ES_FORMAT_T) + sizeof(MMAL_ES_SPECIFIC_FORMAT_T);

   if (!buffer || buffer->cmd != MMAL_EVENT_FORMAT_CHANGED || buffer->length < size)
      return 0;

   event = (MMAL_EVENT_FORMAT_CHANGED_T *)buffer->data;
   format = event->format = (MMAL_ES_FORMAT_T *)&event[1];
   format->es = (MMAL_ES_SPECIFIC_FORMAT_T *)&format[1];
   format->extradata = (uint8_t *)&format->es[1];
   format->extradata_size = buffer->length - size;
   return event;
}

MMAL_STATUS_T mmal_event_error_send(MMAL_COMPONENT_T *component, MMAL_STATUS_T error_status)
{
   MMAL_BUFFER_HEADER_T* event;
   MMAL_STATUS_T status;

   if(!component)
   {
      LOG_ERROR("invalid component");
      return MMAL_EINVAL;
   }

   status = mmal_port_event_get(component->control, &event, MMAL_EVENT_ERROR);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("event not available for component %s %p, result %d", component->name, component, status);
      return status;
   }

   event->length = sizeof(MMAL_STATUS_T);
   *(MMAL_STATUS_T *)event->data = error_status;
   mmal_port_event_send(component->control, event);

   return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_event_eos_send(MMAL_PORT_T *port)
{
   MMAL_EVENT_END_OF_STREAM_T *event;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_STATUS_T status;

   if(!port)
   {
      LOG_ERROR("invalid port");
      return MMAL_EINVAL;
   }

   status = mmal_port_event_get(port->component->control, &buffer, MMAL_EVENT_EOS);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("event not available for port %s %p, result %d", port->name, port, status);
      return status;
   }

   buffer->length = sizeof(*event);
   event = (MMAL_EVENT_END_OF_STREAM_T *)buffer->data;
   event->port_type = port->type;
   event->port_index = port->index;
   mmal_port_event_send(port->component->control, buffer);

   return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_event_forward(MMAL_BUFFER_HEADER_T *event, MMAL_PORT_T *port)
{
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_STATUS_T status;

   if(!port || port->type != MMAL_PORT_TYPE_OUTPUT)
   {
      LOG_ERROR("invalid port");
      return MMAL_EINVAL;
   }

   status = mmal_port_event_get(port->component->control, &buffer, event->cmd);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("event not available for port %s %p, result %d", port->name, port, status);
      return status;
   }

   if (buffer->alloc_size < event->length)
   {
      LOG_ERROR("event buffer too small (%i/%i)", buffer->alloc_size, event->length);
      mmal_buffer_header_release(buffer);
      return MMAL_ENOSPC;
   }

   memcpy(buffer->data, event->data, event->length);
   buffer->length = event->length;
   buffer->offset = event->offset;
   buffer->flags = event->flags;
   buffer->pts = event->pts;
   mmal_port_event_send(port->component->control, buffer);
   return MMAL_SUCCESS;
}
