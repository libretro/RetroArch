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

#if !defined(WFC_CLIENT_IPC_H)
#define WFC_CLIENT_IPC_H

#include "interface/vcos/vcos.h"
#include "interface/khronos/wf/wfc_ipc.h"

/** Send a message and wait for a reply.
 *
 * @param msg       message to send
 * @param size      length of message, including header
 * @param dest      destination for reply data, or NULL
 * @param destlen   size of destination, updated with actual length
 * @return Success or the error code for failure.
 */
VCOS_STATUS_T wfc_client_ipc_sendwait(WFC_IPC_MSG_HEADER_T *msg,
                                       size_t size,
                                       void *dest,
                                       size_t *destlen);

/** Send a message and do not wait for a reply.
 *
 * @param msg_header   message header to send
 * @param size         length of message, including header
 * @param msgid        message id
 * @return Success or the error code for failure.
 */
VCOS_STATUS_T wfc_client_ipc_send(WFC_IPC_MSG_HEADER_T *msg,
                                    size_t size);

/** Initialise the OpenWF-C client IPC. If successful, this must be balanced by
 * a call to wfc_client_ipc_deinit(). This must be called at least once before
 * sending a message.
 *
 * @return Success or the error code for failure.
 */
VCOS_STATUS_T wfc_client_ipc_init(void);

/** Deinitialise the OpenWF-C client IPC.
 *
 * @return True if the service has been destroyed, false if there are other
 *    users still active.
 */
bool wfc_client_ipc_deinit(void);

/** Increase the keep alive count by one. If it rises from zero, the VideoCore
 * will be prevented from being suspended.
 */
void wfc_client_ipc_use_keep_alive(void);

/** Drop the keep alive count by one. If it reaches zero, the VideoCore may be
 * suspended.
 */
void wfc_client_ipc_release_keep_alive(void);

#endif   /* WFC_IPC_H */
