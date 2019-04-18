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

#include <errno.h>
#include <unistd.h>

#include "net_sockets.h"
#include "net_sockets_priv.h"
#include "containers/core/containers_common.h"

/*****************************************************************************/

/** Default maximum datagram size.
 * This is based on the default Ethernet MTU size, less the IP and UDP headers.
 */
#define DEFAULT_MAXIMUM_DATAGRAM_SIZE (1500 - 20 - 8)

/** Maximum socket buffer size to use. */
#define MAXIMUM_BUFFER_SIZE   65536

/*****************************************************************************/
vc_container_net_status_t vc_container_net_private_last_error()
{
   switch (errno)
   {
   case EACCES:               return VC_CONTAINER_NET_ERROR_ACCESS_DENIED;
   case EFAULT:               return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case EINVAL:               return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case EMFILE:               return VC_CONTAINER_NET_ERROR_TOO_BIG;
   case EWOULDBLOCK:          return VC_CONTAINER_NET_ERROR_WOULD_BLOCK;
   case EINPROGRESS:          return VC_CONTAINER_NET_ERROR_IN_PROGRESS;
   case EALREADY:             return VC_CONTAINER_NET_ERROR_IN_PROGRESS;
   case EADDRINUSE:           return VC_CONTAINER_NET_ERROR_IN_USE;
   case EADDRNOTAVAIL:        return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case ENETDOWN:             return VC_CONTAINER_NET_ERROR_NETWORK;
   case ENETUNREACH:          return VC_CONTAINER_NET_ERROR_NETWORK;
   case ENETRESET:            return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ECONNABORTED:         return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ECONNRESET:           return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ENOBUFS:              return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case ENOTCONN:             return VC_CONTAINER_NET_ERROR_NOT_CONNECTED;
   case ESHUTDOWN:            return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case ETIMEDOUT:            return VC_CONTAINER_NET_ERROR_TIMED_OUT;
   case ECONNREFUSED:         return VC_CONTAINER_NET_ERROR_CONNECTION_REFUSED;
   case ELOOP:                return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case ENAMETOOLONG:         return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case EHOSTDOWN:            return VC_CONTAINER_NET_ERROR_NETWORK;
   case EHOSTUNREACH:         return VC_CONTAINER_NET_ERROR_NETWORK;
   case EUSERS:               return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case EDQUOT:               return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case ESTALE:               return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   /* All other errors are unexpected, so just map to a general purpose error code. */
   default:
      return VC_CONTAINER_NET_ERROR_GENERAL;
   }
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_private_init()
{
   /* No additional initialization required */
   return VC_CONTAINER_NET_SUCCESS;
}

/*****************************************************************************/
void vc_container_net_private_deinit()
{
   /* No additional deinitialization required */
}

/*****************************************************************************/
void vc_container_net_private_close( SOCKET_T sock )
{
   close(sock);
}

/*****************************************************************************/
void vc_container_net_private_set_reusable( SOCKET_T sock, bool enable )
{
   int opt = enable ? 1 : 0;

   (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
}

/*****************************************************************************/
size_t vc_container_net_private_maximum_datagram_size( SOCKET_T sock )
{
   (void)sock;

   /* No easy way to determine this, just use the default. */
   return DEFAULT_MAXIMUM_DATAGRAM_SIZE;
}
