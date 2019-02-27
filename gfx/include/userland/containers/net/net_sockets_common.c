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

#include <stdio.h>
#include <stdlib.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "net_sockets.h"
#include "net_sockets_priv.h"

/*****************************************************************************/

struct vc_container_net_tag
{
   /** The underlying socket */
   SOCKET_T socket;
   /** Last error raised on the socket instance. */
   vc_container_net_status_t status;
   /** Simple socket type */
   vc_container_net_type_t type;
   /** Socket address, used for sending datagrams. */
   union {
      struct sockaddr_storage storage;
      struct sockaddr     sa;
      struct sockaddr_in  in;
      struct sockaddr_in6 in6;
   } to_addr;
   /** Number of bytes in to_addr that have been filled. */
   SOCKADDR_LEN_T to_addr_len;
   /** Maximum size of datagrams. */
   size_t max_datagram_size;
   /** Timeout to use when reading from a socket. INFINITE_TIMEOUT_MS waits forever. */
   uint32_t read_timeout_ms;
};

/*****************************************************************************/
static void socket_clear_address(struct sockaddr *p_addr)
{
   switch (p_addr->sa_family)
   {
   case AF_INET:
      {
         struct sockaddr_in *p_addr_v4 = (struct sockaddr_in *)p_addr;

         memset(&p_addr_v4->sin_addr, 0, sizeof(p_addr_v4->sin_addr));
      }
      break;
   case AF_INET6:
      {
         struct sockaddr_in6 *p_addr_v6 = (struct sockaddr_in6 *)p_addr;

         memset(&p_addr_v6->sin6_addr, 0, sizeof(p_addr_v6->sin6_addr));
      }
      break;
   default:
      /* Invalid or unsupported address family */
      vc_container_assert(0);
   }
}

/*****************************************************************************/
static vc_container_net_status_t socket_set_read_buffer_size(VC_CONTAINER_NET_T *p_ctx,
      uint32_t buffer_size)
{
   int result;
   const SOCKOPT_CAST_T optptr = (const SOCKOPT_CAST_T)&buffer_size;

   result = setsockopt(p_ctx->socket, SOL_SOCKET, SO_RCVBUF, optptr, sizeof(buffer_size));

   if (result == SOCKET_ERROR)
      return vc_container_net_private_last_error();

   return VC_CONTAINER_NET_SUCCESS;
}

/*****************************************************************************/
static vc_container_net_status_t socket_set_read_timeout_ms(VC_CONTAINER_NET_T *p_ctx,
      uint32_t timeout_ms)
{
   p_ctx->read_timeout_ms = timeout_ms;
   return VC_CONTAINER_NET_SUCCESS;
}

/*****************************************************************************/
static bool socket_wait_for_data( VC_CONTAINER_NET_T *p_ctx, uint32_t timeout_ms )
{
   int result;
   fd_set set;
   struct timeval tv;

   if (timeout_ms == INFINITE_TIMEOUT_MS)
      return true;

   FD_ZERO(&set);
   FD_SET(p_ctx->socket, &set);
   tv.tv_sec = timeout_ms / 1000;
   tv.tv_usec = (timeout_ms - tv.tv_sec * 1000) * 1000;
   result = select(p_ctx->socket + 1, &set, NULL, NULL, &tv);

   if (result == SOCKET_ERROR)
      p_ctx->status = vc_container_net_private_last_error();
   else
      p_ctx->status = VC_CONTAINER_NET_SUCCESS;

   return (result == 1);
}

/*****************************************************************************/
VC_CONTAINER_NET_T *vc_container_net_open( const char *address, const char *port,
      vc_container_net_open_flags_t flags, vc_container_net_status_t *p_status )
{
   VC_CONTAINER_NET_T *p_ctx;
   struct addrinfo hints, *info, *p;
   int result;
   vc_container_net_status_t status;
   SOCKET_T sock = INVALID_SOCKET;

   status = vc_container_net_private_init();
   if (status != VC_CONTAINER_NET_SUCCESS)
   {
      LOG_ERROR(NULL, "vc_container_net_open: platform initialization failure: %d", status);
      if (p_status)
         *p_status = status;
      return NULL;
   }

   p_ctx = (VC_CONTAINER_NET_T *)malloc(sizeof(VC_CONTAINER_NET_T));
   if (!p_ctx)
   {
      if (p_status)
         *p_status = VC_CONTAINER_NET_ERROR_NO_MEMORY;

      LOG_ERROR(NULL, "vc_container_net_open: malloc fail for VC_CONTAINER_NET_T");
      vc_container_net_private_deinit();
      return NULL;
   }

   /* Initialize the net socket instance structure */
   memset(p_ctx, 0, sizeof(*p_ctx));
   p_ctx->socket = INVALID_SOCKET;
   if (flags & VC_CONTAINER_NET_OPEN_FLAG_STREAM)
      p_ctx->type = address ? STREAM_CLIENT : STREAM_SERVER;
   else
      p_ctx->type = address ? DATAGRAM_SENDER : DATAGRAM_RECEIVER;

   /* Create the address info linked list from the data provided */
   memset(&hints, 0, sizeof(hints));
   switch (flags & VC_CONTAINER_NET_OPEN_FLAG_FORCE_MASK)
   {
   case 0:
      hints.ai_family = AF_UNSPEC;
      break;
   case VC_CONTAINER_NET_OPEN_FLAG_FORCE_IP4:
      hints.ai_family = AF_INET;
      break;
   case VC_CONTAINER_NET_OPEN_FLAG_FORCE_IP6:
      hints.ai_family = AF_INET6;
      break;
   default:
      status = VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
      LOG_ERROR(NULL, "vc_container_net_open: invalid address forcing flag");
      goto error;
   }
   hints.ai_socktype = (flags & VC_CONTAINER_NET_OPEN_FLAG_STREAM) ? SOCK_STREAM : SOCK_DGRAM;

   result = getaddrinfo(address, port, &hints, &info);
   if (result)
   {
      status = vc_container_net_private_last_error();
      LOG_ERROR(NULL, "vc_container_net_open: unable to get address info: %d", status);
      goto error;
   }

   /* Not all address infos may be useable. Search for one that is by skipping any
    * that provoke errors. */
   for(p = info; (p != NULL) && (sock == INVALID_SOCKET) ; p = p->ai_next)
   {
      sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sock == INVALID_SOCKET)
      {
         status = vc_container_net_private_last_error();
         continue;
      }

      switch (p_ctx->type)
      {
      case STREAM_CLIENT:
            /* Simply connect to the given address/port */
            if (connect(sock, p->ai_addr, p->ai_addrlen) == SOCKET_ERROR)
               status = vc_container_net_private_last_error();
            break;

      case DATAGRAM_SENDER:
            /* Nothing further to do */
            break;

      case STREAM_SERVER:
            /* Try to avoid socket reuse timing issues on TCP server sockets */
            vc_container_net_private_set_reusable(sock, true);

            /* Allow any source address */
            socket_clear_address(p->ai_addr);

            if (bind(sock, p->ai_addr, p->ai_addrlen) == SOCKET_ERROR)
               status = vc_container_net_private_last_error();
            break;

      case DATAGRAM_RECEIVER:
            /* Allow any source address */
            socket_clear_address(p->ai_addr);

            if (bind(sock, p->ai_addr, p->ai_addrlen) == SOCKET_ERROR)
               status = vc_container_net_private_last_error();
            break;
      }

      if (status == VC_CONTAINER_NET_SUCCESS)
      {
         /* Save addressing information for later use */
         p_ctx->to_addr_len = p->ai_addrlen;
         memcpy(&p_ctx->to_addr, p->ai_addr, p->ai_addrlen);
      } else {
         vc_container_net_private_close(sock);   /* Try next entry in list */
         sock = INVALID_SOCKET;
      }
   }

   freeaddrinfo(info);

   if (sock == INVALID_SOCKET)
   {
      LOG_ERROR(NULL, "vc_container_net_open: failed to open socket: %d", status);
      goto error;
   }

   p_ctx->socket = sock;
   p_ctx->max_datagram_size = vc_container_net_private_maximum_datagram_size(sock);
   p_ctx->read_timeout_ms = INFINITE_TIMEOUT_MS;

   if (p_status)
      *p_status = VC_CONTAINER_NET_SUCCESS;

   return p_ctx;

error:
   if (p_status)
      *p_status = status;
   (void)vc_container_net_close(p_ctx);
   return NULL;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_close( VC_CONTAINER_NET_T *p_ctx )
{
   if (!p_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   if (p_ctx->socket != INVALID_SOCKET)
   {
      vc_container_net_private_close(p_ctx->socket);
      p_ctx->socket = INVALID_SOCKET;
   }
   free(p_ctx);

   vc_container_net_private_deinit();

   return VC_CONTAINER_NET_SUCCESS;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_status( VC_CONTAINER_NET_T *p_ctx )
{
   if (!p_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
   return p_ctx->status;
}

/*****************************************************************************/
size_t vc_container_net_read( VC_CONTAINER_NET_T *p_ctx, void *buffer, size_t size )
{
   int result = 0;

   if (!p_ctx)
      return 0;

   if (!buffer)
   {
      p_ctx->status = VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
      return 0;
   }

   p_ctx->status = VC_CONTAINER_NET_SUCCESS;

   switch (p_ctx->type)
   {
   case STREAM_CLIENT:
   case STREAM_SERVER:
      /* Receive data from the stream */
      if (socket_wait_for_data(p_ctx, p_ctx->read_timeout_ms))
      {
         result = recv(p_ctx->socket, buffer, (int)size, 0);
         if (!result)
            p_ctx->status = VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
      } else
         p_ctx->status = VC_CONTAINER_NET_ERROR_TIMED_OUT;
      break;

   case DATAGRAM_RECEIVER:
      {
         /* Receive the packet */
         /* FIXME Potential for data loss, as rest of packet will be lost if buffer was not large enough */
         if (socket_wait_for_data(p_ctx, p_ctx->read_timeout_ms))
         {
            result = recvfrom(p_ctx->socket, buffer, size, 0, &p_ctx->to_addr.sa, &p_ctx->to_addr_len);
            if (!result)
               p_ctx->status = VC_CONTAINER_NET_ERROR_CONNECTION_LOST;
         } else
            p_ctx->status = VC_CONTAINER_NET_ERROR_TIMED_OUT;
      }
      break;

   default: /* DATAGRAM_SENDER */
      p_ctx->status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
      result = 0;
      break;
   }

   if (result == SOCKET_ERROR)
   {
      p_ctx->status = vc_container_net_private_last_error();
      result = 0;
   }

   return (size_t)result;
}

/*****************************************************************************/
size_t vc_container_net_write( VC_CONTAINER_NET_T *p_ctx, const void *buffer, size_t size )
{
   int result;

   if (!p_ctx)
      return 0;

   if (!buffer)
   {
      p_ctx->status = VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
      return 0;
   }

   p_ctx->status = VC_CONTAINER_NET_SUCCESS;

   switch (p_ctx->type)
   {
   case STREAM_CLIENT:
   case STREAM_SERVER:
      /* Send data to the stream */
      result = send(p_ctx->socket, buffer, (int)size, 0);
      break;

   case DATAGRAM_SENDER:
      /* Send the datagram */

      if (size > p_ctx->max_datagram_size)
         size = p_ctx->max_datagram_size;

      result = sendto(p_ctx->socket, buffer, size, 0, &p_ctx->to_addr.sa, p_ctx->to_addr_len);
      break;

   default: /* DATAGRAM_RECEIVER */
      p_ctx->status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
      result = 0;
      break;
   }

   if (result == SOCKET_ERROR)
   {
      p_ctx->status = vc_container_net_private_last_error();
      result = 0;
   }

   return (size_t)result;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_listen( VC_CONTAINER_NET_T *p_ctx, uint32_t maximum_connections )
{
   if (!p_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   p_ctx->status = VC_CONTAINER_NET_SUCCESS;

   if (p_ctx->type == STREAM_SERVER)
   {
      if (listen(p_ctx->socket, maximum_connections) == SOCKET_ERROR)
         p_ctx->status = vc_container_net_private_last_error();
   } else {
      p_ctx->status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
   }

   return p_ctx->status;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_accept( VC_CONTAINER_NET_T *p_server_ctx, VC_CONTAINER_NET_T **pp_client_ctx )
{
   VC_CONTAINER_NET_T *p_client_ctx = NULL;

   if (!p_server_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   if (!pp_client_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;

   *pp_client_ctx = NULL;

   if (p_server_ctx->type != STREAM_SERVER)
   {
      p_server_ctx->status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
      goto error;
   }

   p_client_ctx = (VC_CONTAINER_NET_T *)malloc(sizeof(VC_CONTAINER_NET_T));
   if (!p_client_ctx)
   {
      p_server_ctx->status = VC_CONTAINER_NET_ERROR_NO_MEMORY;
      goto error;
   }

   /* Initialise the new context with the address information from the server context */
   memset(p_client_ctx, 0, sizeof(*p_client_ctx));
   memcpy(&p_client_ctx->to_addr, &p_server_ctx->to_addr, p_server_ctx->to_addr_len);
   p_client_ctx->to_addr_len = p_server_ctx->to_addr_len;

   p_client_ctx->socket = accept(p_server_ctx->socket, &p_client_ctx->to_addr.sa, &p_client_ctx->to_addr_len);

   if (p_client_ctx->socket == INVALID_SOCKET)
   {
      p_server_ctx->status = vc_container_net_private_last_error();
      goto error;
   }

   /* Need to bump up the initialisation count, as a new context has been created */
   p_server_ctx->status = vc_container_net_private_init();
   if (p_server_ctx->status != VC_CONTAINER_NET_SUCCESS)
      goto error;

   p_client_ctx->type = STREAM_CLIENT;
   p_client_ctx->max_datagram_size = vc_container_net_private_maximum_datagram_size(p_client_ctx->socket);
   p_client_ctx->read_timeout_ms = INFINITE_TIMEOUT_MS;
   p_client_ctx->status = VC_CONTAINER_NET_SUCCESS;

   *pp_client_ctx = p_client_ctx;
   return VC_CONTAINER_NET_SUCCESS;

error:
   if (p_client_ctx)
      free(p_client_ctx);
   return p_server_ctx->status;
}

/*****************************************************************************/
bool vc_container_net_is_data_available( VC_CONTAINER_NET_T *p_ctx )
{
   if (!p_ctx)
      return false;

   if (p_ctx->type == DATAGRAM_SENDER)
   {
      p_ctx->status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
      return false;
   }

   return socket_wait_for_data(p_ctx, 0);
}

/*****************************************************************************/
size_t vc_container_net_maximum_datagram_size( VC_CONTAINER_NET_T *p_ctx )
{
   return p_ctx ? p_ctx->max_datagram_size : 0;
}

/*****************************************************************************/
static vc_container_net_status_t translate_getnameinfo_error( int error )
{
   switch (error)
   {
   case EAI_AGAIN:   return VC_CONTAINER_NET_ERROR_TRY_AGAIN;
   case EAI_FAIL:    return VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND;
   case EAI_MEMORY:  return VC_CONTAINER_NET_ERROR_NO_MEMORY;
   case EAI_NONAME:  return VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND;

   /* All other errors are unexpected, so just map to a general purpose error code. */
   default:
      return VC_CONTAINER_NET_ERROR_GENERAL;
   }
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_get_client_name( VC_CONTAINER_NET_T *p_ctx, char *name, size_t name_len )
{
   int result;

   if (!p_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   if (p_ctx->socket == INVALID_SOCKET)
      p_ctx->status = VC_CONTAINER_NET_ERROR_NOT_CONNECTED;
   else if (!name || !name_len)
      p_ctx->status = VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   else if ((result = getnameinfo(&p_ctx->to_addr.sa, p_ctx->to_addr_len, name, name_len, NULL, 0, 0)) != 0)
      p_ctx->status = translate_getnameinfo_error(result);
   else
      p_ctx->status = VC_CONTAINER_NET_SUCCESS;

   return p_ctx->status;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_get_client_port( VC_CONTAINER_NET_T *p_ctx , unsigned short *port )
{
   if (!p_ctx)
      return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;

   if (p_ctx->socket == INVALID_SOCKET)
      p_ctx->status = VC_CONTAINER_NET_ERROR_NOT_CONNECTED;
   else if (!port)
      p_ctx->status = VC_CONTAINER_NET_ERROR_INVALID_PARAMETER;
   else
   {
      p_ctx->status = VC_CONTAINER_NET_SUCCESS;
      switch (p_ctx->to_addr.sa.sa_family)
      {
      case AF_INET:
         *port = ntohs(p_ctx->to_addr.in.sin_port);
         break;
      case AF_INET6:
         *port = ntohs(p_ctx->to_addr.in6.sin6_port);
         break;
      default:
         /* Highly unexepcted address family! */
         p_ctx->status = VC_CONTAINER_NET_ERROR_GENERAL;
      }
   }

   return p_ctx->status;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_control( VC_CONTAINER_NET_T *p_ctx,
      vc_container_net_control_t operation,
      va_list args)
{
   vc_container_net_status_t status;

   switch (operation)
   {
   case VC_CONTAINER_NET_CONTROL_SET_READ_BUFFER_SIZE:
      status = socket_set_read_buffer_size(p_ctx, va_arg(args, uint32_t));
      break;
   case VC_CONTAINER_NET_CONTROL_SET_READ_TIMEOUT_MS:
      status = socket_set_read_timeout_ms(p_ctx, va_arg(args, uint32_t));
      break;
   default:
      status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
   }

   return status;
}

/*****************************************************************************/
uint32_t vc_container_net_to_host( uint32_t value )
{
   return ntohl(value);
}

/*****************************************************************************/
uint32_t vc_container_net_from_host( uint32_t value )
{
   return htonl(value);
}

/*****************************************************************************/
uint16_t vc_container_net_to_host_16( uint16_t value )
{
   return ntohs(value);
}

/*****************************************************************************/
uint16_t vc_container_net_from_host_16( uint16_t value )
{
   return htons(value);
}
