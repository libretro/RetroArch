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

#ifndef _NET_SOCKETS_PRIV_H_
#define _NET_SOCKETS_PRIV_H_

#include "net_sockets.h"

#ifdef WIN32
#include "net_sockets_win32.h"
#else
#include "net_sockets_bsd.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   STREAM_CLIENT = 0,   /**< TCP client */
   STREAM_SERVER,       /**< TCP server */
   DATAGRAM_SENDER,     /**< UDP sender */
   DATAGRAM_RECEIVER    /**< UDP receiver */
} vc_container_net_type_t;

/** Perform implementation-specific per-socket initialization.
 *
 * \return VC_CONTAINER_NET_SUCCESS or one of the error codes on failure. */
vc_container_net_status_t vc_container_net_private_init( void );

/** Perform implementation-specific per-socket deinitialization.
 * This function is always called once for each successful call to socket_private_init(). */
void vc_container_net_private_deinit( void );

/** Return the last error from the socket implementation. */
vc_container_net_status_t vc_container_net_private_last_error( void );

/** Implementation-specific internal socket close.
 *
 * \param sock Internal socket to be closed. */
void vc_container_net_private_close( SOCKET_T sock );

/** Enable or disable socket address reusability.
 *
 * \param sock Internal socket to be closed.
 * \param enable True to enable reusability, false to clear it. */
void vc_container_net_private_set_reusable( SOCKET_T sock, bool enable );

/** Query the maximum datagram size for the socket.
 *
 * \param sock The socket to query.
 * \return The maximum supported datagram size on the socket. */
size_t vc_container_net_private_maximum_datagram_size( SOCKET_T sock );

#ifdef __cplusplus
}
#endif

#endif   /* _NET_SOCKETS_PRIV_H_ */
