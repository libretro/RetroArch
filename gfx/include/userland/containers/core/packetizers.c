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

#include <stdlib.h>
#include <string.h>

#include "containers/packetizers.h"
#include "containers/core/packetizers_private.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_utils.h"

/** List of registered packetizers. */
static VC_PACKETIZER_REGISTRY_ENTRY_T *registry;

/*****************************************************************************/
void vc_packetizer_register(VC_PACKETIZER_REGISTRY_ENTRY_T *entry)
{
   LOG_DEBUG(0, "registering packetizer %s", entry->name);
   entry->next = registry;
   registry = entry;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T vc_packetizer_load(VC_PACKETIZER_T *p_ctx)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   VC_PACKETIZER_REGISTRY_ENTRY_T *entry;

   /* Try all the packetizers until we find the right one */
   for (entry = registry; entry; entry = entry->next)
   {
      status = entry->open(p_ctx);
      if(status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED)
         break;
   }

   return status;
}

/*****************************************************************************/
static void vc_packetizer_unload(VC_PACKETIZER_T *p_ctx)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
}

/*****************************************************************************/
VC_PACKETIZER_T *vc_packetizer_open( VC_CONTAINER_ES_FORMAT_T *in,
   VC_CONTAINER_FOURCC_T out_variant, VC_CONTAINER_STATUS_T *p_status )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_PACKETIZER_T *p_ctx = 0;

   /* Allocate our context before trying out the different packetizers */
   p_ctx = malloc( sizeof(*p_ctx) + sizeof(*p_ctx->priv));
   if(!p_ctx) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(p_ctx, 0, sizeof(*p_ctx) + sizeof(*p_ctx->priv));
   p_ctx->priv = (VC_PACKETIZER_PRIVATE_T *)(p_ctx + 1);
   bytestream_init( &p_ctx->priv->stream );

   p_ctx->in = vc_container_format_create(in->extradata_size);
   if(!p_ctx->in) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   p_ctx->out = vc_container_format_create(in->extradata_size);
   if(!p_ctx->out) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }

   vc_container_format_copy( p_ctx->in, in, in->extradata_size );
   p_ctx->in->extradata_size = 0;
   vc_container_format_copy( p_ctx->out, p_ctx->in, in->extradata_size );
   p_ctx->in->extradata_size = in->extradata_size;
   p_ctx->out->extradata = p_ctx->in->extradata;
   p_ctx->out->extradata_size = p_ctx->in->extradata_size;
   p_ctx->out->codec_variant = out_variant;

   vc_container_time_init(&p_ctx->priv->time, 1000000);

   status = vc_packetizer_load(p_ctx);
   if(status != VC_CONTAINER_SUCCESS)
      goto error;

 end:
   if(p_status) *p_status = status;
   return p_ctx;

 error:
   if(p_ctx) vc_packetizer_close(p_ctx);
   p_ctx = NULL;
   goto end;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_packetizer_close( VC_PACKETIZER_T *p_ctx )
{
   VC_CONTAINER_BYTESTREAM_T *stream;
   VC_CONTAINER_PACKET_T *packet, *next;

   if(!p_ctx) return VC_CONTAINER_SUCCESS;

   stream = &p_ctx->priv->stream;

   if(p_ctx->in) vc_container_format_delete(p_ctx->in);
   if(p_ctx->out) vc_container_format_delete(p_ctx->out);
   if(p_ctx->priv->pf_close) p_ctx->priv->pf_close(p_ctx);
   if(p_ctx->priv->module_handle) vc_packetizer_unload(p_ctx);

   /* Free the bytestream  */
   for(packet = stream->first; packet; packet = next)
   {
      next = packet->next;
      if(packet->framework_data) free(packet);
   }

   free(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_packetizer_push( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T *in)
{
   /* Do some sanity checking on packet ? */

   in->framework_data = 0;
   bytestream_push(&p_ctx->priv->stream, in);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_packetizer_pop( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T **in, VC_PACKETIZER_FLAGS_T flags)
{
   VC_CONTAINER_BYTESTREAM_T *stream = &p_ctx->priv->stream;
   VC_CONTAINER_PACKET_T *packet, *new, **prev;

   /* Release the packets which have been read */
   while((*in = bytestream_pop(stream)) != NULL)
   {
      if(*in && (*in)->framework_data)
      {
         free(*in);
         continue;
      }

      if(*in)
         return VC_CONTAINER_SUCCESS;
   }

   if(!(flags & VC_PACKETIZER_FLAG_FORCE_RELEASE_INPUT))
      return VC_CONTAINER_ERROR_INCOMPLETE_DATA;

   /* Look for the 1st non-framework packet */
   for (packet = stream->first, prev = &stream->first;
        packet && packet->framework_data; prev = &packet->next, packet = packet->next);

   if (!packet || (packet && packet->framework_data))
      return VC_CONTAINER_ERROR_INCOMPLETE_DATA;

   /* We'll currently alloc an internal packet for each packet the client forcefully releases.
    * We could probably do something a bit more clever than that though. */
   /* Replace the packet with a newly allocated one */
   new = malloc(sizeof(*packet) + packet->size);
   if(!new)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   *new = *packet;
   new->framework_data = new;
   if(!new->next)
      stream->last = &new->next;
   if(stream->current == packet)
      stream->current = new;
   *prev = new;
   new->data = (uint8_t *)&new[1];
   memcpy(new->data, packet->data, packet->size);
   *in = packet;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_packetizer_read( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet, VC_PACKETIZER_FLAGS_T flags)
{
   if(!packet && !(flags & VC_CONTAINER_READ_FLAG_SKIP))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   if(!packet && (flags & VC_CONTAINER_READ_FLAG_INFO))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   if(packet && !packet->data &&
      (!(flags & VC_CONTAINER_READ_FLAG_INFO) &&
       !(flags & VC_CONTAINER_READ_FLAG_SKIP)))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   /* Always having a packet structure to work with simplifies things */
   if(!packet)
      packet = &p_ctx->priv->packet;

   return p_ctx->priv->pf_packetize(p_ctx, packet, flags);
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_packetizer_reset( VC_PACKETIZER_T *p_ctx )
{
   VC_CONTAINER_BYTESTREAM_T *stream = &p_ctx->priv->stream;

   bytestream_skip( stream, stream->bytes - stream->current_offset - stream->offset );

   if (p_ctx->priv->pf_reset)
      return p_ctx->priv->pf_reset(p_ctx);
   else
      return VC_CONTAINER_SUCCESS;
}
