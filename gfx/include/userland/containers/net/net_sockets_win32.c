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

#include "net_sockets.h"
#include "net_sockets_priv.h"
#include "containers/core/containers_common.h"

#pragma comment(lib, "Ws2_32.lib")

/*****************************************************************************/

/** Default maximum datagram size.
 * This is based on the default Ethernet MTU size, less the IP and UDP headers.
 */
#define DEFAULT_MAXIMUM_DATAGRAM_SIZE (1500 - 20 - 8)

/** Maximum socket buffer size to use. */
#define MAXIMUM_BUFFER_SIZE   65536

/*****************************************************************************/
static vc_container_net_status_t translate_error_status( int error )
{
   switch (error)
   {
   case WSA_INVALID_HANDLE:      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
   case WSA_NOT_ENOUGH_MEMORY:   return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case WSA_INVALID_PARAMETER:   return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case WSAEACCES:               return VC_CONTAINER_NET_ERROR_ACCESS_DENIED;
   case WSAEFAULT:               return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case WSAEINVAL:               return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case WSAEMFILE:               return VC_CONTAINER_NET_ERROR_TOO_BIG;
   case WSAEWOULDBLOCK:          return VC_CONTAINER_NET_ERROR_WOULD_BLOCK;
   case WSAEINPROGRESS:          return VC_CONTAINER_NET_ERROR_IN_PROGRESS;
   case WSAEALREADY:             return VC_CONTAINER_NET_ERROR_IN_PROGRESS;
   case WSAEADDRINUSE:           return VC_CONTAINER_NET_ERROR_IN_USE;
   case WSAEADDRNOTAVAIL:        return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case WSAENETDOWN:             return VC_CONTAINER_NET_ERROR_NETWORK;
   case WSAENETUNREACH:          return VC_CONTAINER_NET_ERROR_NETWORK;
   case WSAENETRESET:            return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case WSAECONNABORTED:         return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case WSAECONNRESET:           return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case WSAENOBUFS:              return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case WSAENOTCONN:             return VC_CONTAINER_NET_ERROR_NOT_CONNECTED;
   case WSAESHUTDOWN:            return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case WSAETIMEDOUT:            return VC_CONTAINER_NET_ERROR_TIMED_OUT;
   case WSAECONNREFUSED:         return VC_CONTAINER_NET_ERROR_CONNECTION_REFUSED;
   case WSAELOOP:                return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case WSAENAMETOOLONG:         return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   case WSAEHOSTDOWN:            return VC_CONTAINER_NET_ERROR_NETWORK;
   case WSAEHOSTUNREACH:         return VC_CONTAINER_NET_ERROR_NETWORK;
   case WSAEPROCLIM:             return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case WSAEUSERS:               return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case WSAEDQUOT:               return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case WSAESTALE:               return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
   case WSAEDISCON:              return VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
   case WSAHOST_NOT_FOUND:       return VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND;
   case WSATRY_AGAIN:            return VC_CONTAINER_NET_ERROR_TRY_AGAIN;
   case WSANO_RECOVERY:          return VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND;
   case WSANO_DATA:              return VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND;

   /* All other errors are unexpected, so just map to a general purpose error code. */
   default:
      return VC_CONTAINER_NET_ERROR_GENERAL;
   }
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_private_last_error()
{
   return translate_error_status( WSAGetLastError() );
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_private_init()
{
   WSADATA wsa_data;
   int result;

   result = WSAStartup(MAKEWORD(2,2), &wsa_data);
   if (result)
      return translate_error_status( result );

   return VC_CONTAINER_NET_SUCCESS;
}

/*****************************************************************************/
void vc_container_net_private_deinit()
{
   WSACleanup();
}

/*****************************************************************************/
void vc_container_net_private_close( SOCKET_T sock )
{
   closesocket(sock);
}

/*****************************************************************************/
void vc_container_net_private_set_reusable( SOCKET_T sock, bool enable )
{
   BOOL opt = enable ? TRUE : FALSE;

   (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
}

/*****************************************************************************/
size_t vc_container_net_private_maximum_datagram_size( SOCKET_T sock )
{
   size_t max_datagram_size = DEFAULT_MAXIMUM_DATAGRAM_SIZE;
   int opt_size = sizeof(max_datagram_size);

   /* Ignore errors and use the default if necessary */
   (void)getsockopt(sock, SOL_SOCKET, SO_MAX_MSG_SIZE, (char *)&max_datagram_size, &opt_size);

   return max_datagram_size;
}
