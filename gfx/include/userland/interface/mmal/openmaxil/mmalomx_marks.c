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
 * OpenMAX IL adaptation layer for MMAL - Marking related functions
 *
 * Note that we do not support buffer marks properly other than for conformance
 * testing. For input ports, we just move the mark over to the output port.
 */

#include "mmalomx.h"
#include "mmalomx_buffer.h"
#include "mmalomx_marks.h"
#include "mmalomx_commands.h"
#include "mmalomx_logging.h"

#define MMALOMX_GET_MARK(port, mark) \
   mark = &port->marks[port->marks_first]; \
   port->marks_num--; \
   port->marks_first = ++port->marks_first == MAX_MARKS_NUM ? 0 : port->marks_first
#define MMALOMX_PUT_MARK(port, mark) \
   port->marks[(port->marks_first + port->marks_num) % MAX_MARKS_NUM] = *mark; \
   port->marks_num++;

void mmalomx_mark_process_incoming(MMALOMX_COMPONENT_T *component,
   MMALOMX_PORT_T *port, OMX_BUFFERHEADERTYPE *omx_buffer)
{
   /* Tag buffers with OMX marks */
   if (!omx_buffer->hMarkTargetComponent && port->marks_num > 0 &&
       port->direction == OMX_DirInput)
   {
      OMX_MARKTYPE *mark;
      MMALOMX_GET_MARK(port, mark);
      omx_buffer->hMarkTargetComponent = mark->hMarkTargetComponent;
      omx_buffer->pMarkData = mark->pMarkData;

      mmalomx_callback_event_handler(component, OMX_EventCmdComplete,
         OMX_CommandMarkBuffer, port->index, NULL);
   }
   /* We do not support buffer marks properly other than for conformance testing.
    * For input ports, we just move the mark over to the output port. */
   if (port->direction == OMX_DirInput && omx_buffer->hMarkTargetComponent)
   {
      OMX_MARKTYPE mark = {omx_buffer->hMarkTargetComponent, omx_buffer->pMarkData};
      unsigned int i;
      for (i = 0; i < component->ports_num; i++)
      {
         if (component->ports[i].direction != OMX_DirOutput ||
             component->ports[i].marks_num >= MAX_MARKS_NUM)
            continue;

         MMALOMX_PUT_MARK((&component->ports[i]), (&mark));
      }
   }
}

void mmalomx_mark_process_outgoing(MMALOMX_COMPONENT_T *component,
   MMALOMX_PORT_T *port, OMX_BUFFERHEADERTYPE *omx_buffer)
{
   /* Tag buffers with OMX marks */
   if (port->direction == OMX_DirOutput &&
       !omx_buffer->hMarkTargetComponent && port->marks_num)
   {
         OMX_MARKTYPE *mark;
         MMALOMX_GET_MARK(port, mark);
         omx_buffer->hMarkTargetComponent = mark->hMarkTargetComponent;
         omx_buffer->pMarkData = mark->pMarkData;
   }
   /* Check if we need to trigger a Mark event */
   if (omx_buffer->hMarkTargetComponent &&
       omx_buffer->hMarkTargetComponent == (OMX_HANDLETYPE)&component->omx)
   {
      mmalomx_callback_event_handler(component, OMX_EventMark, 0, 0, omx_buffer->pMarkData);
      omx_buffer->hMarkTargetComponent = NULL;
      omx_buffer->pMarkData = NULL;
   }
}
