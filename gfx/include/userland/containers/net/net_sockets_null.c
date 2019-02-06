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
#include "containers/core/containers_common.h"

/*****************************************************************************/

struct vc_container_net_tag
{
   uint32_t dummy;   /* C requires structs not to be empty. */
};

/*****************************************************************************/
VC_CONTAINER_NET_T *vc_container_net_open( const char *address, const char *port,
      vc_container_net_open_flags_t flags, vc_container_net_status_t *p_status )
{
   VC_CONTAINER_PARAM_UNUSED(address);
   VC_CONTAINER_PARAM_UNUSED(port);
   VC_CONTAINER_PARAM_UNUSED(flags);

   if (p_status)
      *p_status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;

   return NULL;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_close( VC_CONTAINER_NET_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_status( VC_CONTAINER_NET_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
}

/*****************************************************************************/
size_t vc_container_net_read( VC_CONTAINER_NET_T *p_ctx, void *buffer, size_t size )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(buffer);
   VC_CONTAINER_PARAM_UNUSED(size);

   return 0;
}

/*****************************************************************************/
size_t vc_container_net_write( VC_CONTAINER_NET_T *p_ctx, const void *buffer, size_t size )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(buffer);
   VC_CONTAINER_PARAM_UNUSED(size);

   return 0;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_listen( VC_CONTAINER_NET_T *p_ctx, uint32_t maximum_connections )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(maximum_connections);

   return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_accept( VC_CONTAINER_NET_T *p_server_ctx, VC_CONTAINER_NET_T **pp_client_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_server_ctx);
   VC_CONTAINER_PARAM_UNUSED(pp_client_ctx);

   return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
}

/*****************************************************************************/
bool vc_container_net_is_data_available( VC_CONTAINER_NET_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   return false;
}

/*****************************************************************************/
size_t vc_container_net_maximum_datagram_size( VC_CONTAINER_NET_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   return 0;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_get_client_name( VC_CONTAINER_NET_T *p_ctx, char *name, size_t name_len )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(name);
   VC_CONTAINER_PARAM_UNUSED(name_len);

   return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_get_client_port( VC_CONTAINER_NET_T *p_ctx , unsigned short *port )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(port);

   return VC_CONTAINER_NET_ERROR_INVALID_SOCKET;
}

/*****************************************************************************/
vc_container_net_status_t vc_container_net_control( VC_CONTAINER_NET_T *p_ctx,
      vc_container_net_control_t operation,
      va_list args)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(operation);
   VC_CONTAINER_PARAM_UNUSED(args);

   return VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
}

/*****************************************************************************/
uint32_t vc_container_net_to_host( uint32_t value )
{
   return value;
}

/*****************************************************************************/
uint32_t vc_container_net_from_host( uint32_t value )
{
   return value;
}

/*****************************************************************************/
uint16_t vc_container_net_to_host_16( uint16_t value )
{
   return value;
}

/*****************************************************************************/
uint16_t vc_container_net_from_host_16( uint16_t value )
{
   return value;
}
