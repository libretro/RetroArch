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

#ifndef MMAL_VC_CLIENT_H
#define MMAL_VC_CLIENT_H

/** @file mmal_vc_client_priv.h
  *
  * Internal API for vchiq_arm MMAL client.
  */

struct MMAL_CLIENT_T;
typedef struct MMAL_CLIENT_T MMAL_CLIENT_T;

void mmal_vc_client_init(void);

/** Hold the context required when sending a buffer to the copro.
 */
typedef struct MMAL_VC_CLIENT_BUFFER_CONTEXT_T
{
   uint32_t magic;

   /** Called when VC is done with the buffer */
   void (*callback)(struct mmal_worker_buffer_from_host *);

   /** Called when VC sends an event */
   void (*callback_event)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *event);

   /** The port this buffer was sent to */
   MMAL_PORT_T *port;

   /** The original buffer from the host. */
   MMAL_BUFFER_HEADER_T *buffer;

   /** The actual message sent to the host */
   struct mmal_worker_buffer_from_host msg;
} MMAL_VC_CLIENT_BUFFER_CONTEXT_T;


MMAL_CLIENT_T *mmal_vc_get_client(void);

MMAL_STATUS_T mmal_vc_sendwait_message(MMAL_CLIENT_T *client,
                                       mmal_worker_msg_header *header,
                                       size_t size,
                                       uint32_t msgid,
                                       void *dest,
                                       size_t *destlen,
                                       MMAL_BOOL_T send_dummy_bulk);

MMAL_STATUS_T mmal_vc_send_message(MMAL_CLIENT_T *client,
                                   mmal_worker_msg_header *header, size_t size,
                                   uint8_t *data, size_t data_size,
                                   uint32_t msgid);

#endif

