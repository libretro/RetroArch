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
#include "mmal_vc_api_drm.h"
#include "mmal_vc_api.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal.h"
#include "mmal_vc_api.h"
#include "mmal_vc_msgs.h"
#include "mmal_vc_client_priv.h"
#include "mmal_vc_opaque_alloc.h"
#include "mmal_vc_shm.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/core/mmal_component_private.h"
#include "interface/mmal/core/mmal_port_private.h"
#include "interface/mmal/core/mmal_buffer_private.h"
#include "interface/vcos/vcos.h"


int mmal_vc_drm_get_time(unsigned int * time)
{
   MMAL_STATUS_T status;
   mmal_worker_msg_header req;
   mmal_worker_drm_get_time_reply reply;
   size_t len = sizeof(reply);
   status = mmal_vc_init();
   if (status != MMAL_SUCCESS) return status;
   status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                     &req, sizeof(req),
                                     MMAL_WORKER_DRM_GET_TIME,
                                     &reply, &len, MMAL_FALSE);
   *time = reply.time;
   mmal_vc_deinit();
   return status;
}


int mmal_vc_drm_get_lhs32(unsigned char * into)
{
   MMAL_STATUS_T status;
   mmal_worker_msg_header req;
   mmal_worker_drm_get_lhs32_reply reply;
   size_t len = sizeof(reply);
   status = mmal_vc_init();
   if (status != MMAL_SUCCESS) return status;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                     &req, sizeof(req),
                                     MMAL_WORKER_DRM_GET_LHS32,
                                     &reply, &len, MMAL_FALSE);
   memcpy(into, reply.secret, 32);
   mmal_vc_deinit();
   return status;
}
