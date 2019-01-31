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

#include "interface/mmal/vc/mmal_vc_opaque_alloc.h"
#include "mmal_vc_msgs.h"
#include "mmal_vc_client_priv.h"

MMAL_OPAQUE_IMAGE_HANDLE_T mmal_vc_opaque_alloc_desc(const char *description)
{
   MMAL_STATUS_T ret;
   MMAL_OPAQUE_IMAGE_HANDLE_T h = 0;
   mmal_worker_opaque_allocator msg;
   size_t len = sizeof(msg);
   msg.op = MMAL_WORKER_OPAQUE_MEM_ALLOC;
   vcos_safe_strcpy(msg.description, description, sizeof(msg.description), 0);
   ret = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                  &msg.header, sizeof(msg),
                                  MMAL_WORKER_OPAQUE_ALLOCATOR_DESC,
                                  &msg, &len, MMAL_FALSE);
   if (ret == MMAL_SUCCESS)
   {
      h = msg.handle;
   }
   return h;
}

MMAL_OPAQUE_IMAGE_HANDLE_T mmal_vc_opaque_alloc(void)
{
   return mmal_vc_opaque_alloc_desc("?");
}

MMAL_STATUS_T mmal_vc_opaque_acquire(unsigned int handle)
{
   MMAL_STATUS_T ret;
   mmal_worker_opaque_allocator msg;
   size_t len = sizeof(msg);
   msg.handle = handle;
   msg.op = MMAL_WORKER_OPAQUE_MEM_ACQUIRE;
   ret = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                  &msg.header, sizeof(msg),
                                  MMAL_WORKER_OPAQUE_ALLOCATOR,
                                  &msg, &len, MMAL_FALSE);
   if (ret == MMAL_SUCCESS)
      ret = msg.status;
   return ret;
}

MMAL_STATUS_T mmal_vc_opaque_release(unsigned int handle)
{
   MMAL_STATUS_T ret;
   mmal_worker_opaque_allocator msg;
   size_t len = sizeof(msg);
   msg.handle = handle;
   msg.op = MMAL_WORKER_OPAQUE_MEM_RELEASE;
   ret = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                  &msg.header, sizeof(msg),
                                  MMAL_WORKER_OPAQUE_ALLOCATOR,
                                  &msg, &len, MMAL_FALSE);
   if (ret == MMAL_SUCCESS)
      ret = msg.status;
   return ret;
}

