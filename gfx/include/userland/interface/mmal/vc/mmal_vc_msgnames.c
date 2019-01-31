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

#include "mmal_vc_msgnames.h"
#include "mmal_vc_msgs.h"

/** Convert a message id to a name.
  */
const char *mmal_msgname(uint32_t id)
{
#define MSGNAME(x) { MMAL_WORKER_##x, #x }
   static struct {
      uint32_t id;
      const char *name;
   } msgnames[] = {
      MSGNAME(QUIT),
      MSGNAME(SERVICE_CLOSED),
      MSGNAME(GET_VERSION),
      MSGNAME(COMPONENT_CREATE),
      MSGNAME(COMPONENT_DESTROY),
      MSGNAME(COMPONENT_ENABLE),
      MSGNAME(COMPONENT_DISABLE),
      MSGNAME(PORT_INFO_GET),
      MSGNAME(PORT_INFO_SET),
      MSGNAME(PORT_ACTION),
      MSGNAME(BUFFER_FROM_HOST),
      MSGNAME(BUFFER_TO_HOST),
      MSGNAME(GET_STATS),
      MSGNAME(PORT_PARAMETER_SET),
      MSGNAME(PORT_PARAMETER_GET),
      MSGNAME(EVENT_TO_HOST),
      MSGNAME(GET_CORE_STATS_FOR_PORT),
      MSGNAME(OPAQUE_ALLOCATOR),
      MSGNAME(CONSUME_MEM),
      MSGNAME(LMK),
      MSGNAME(OPAQUE_ALLOCATOR_DESC),
      MSGNAME(DRM_GET_LHS32),
      MSGNAME(DRM_GET_TIME),
      MSGNAME(BUFFER_FROM_HOST_ZEROLEN),
      MSGNAME(PORT_FLUSH),
      MSGNAME(HOST_LOG),
      MSGNAME(COMPACT),
      { 0, NULL },
   };
   vcos_static_assert(sizeof(msgnames)/sizeof(msgnames[0]) == MMAL_WORKER_MSG_LAST);
   int i = 0;
   while (msgnames[i].name)
   {
      if (msgnames[i].id == id)
         return msgnames[i].name;
      i++;
   }
   return "unknown-message";
}
