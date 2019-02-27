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

/** \file
 * Implementation of an ISO 14496-15 to Annexe-B AVC video packetizer.
 */

#include <stdlib.h>
#include <string.h>

#include "containers/packetizers.h"
#include "containers/core/packetizers_private.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_bytestream.h"

#ifndef ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#endif

/** Arbitrary number which should be sufficiently high so that no sane frame will
 * be bigger than that. */
#define MAX_FRAME_SIZE (1920*1088*2)

VC_CONTAINER_STATUS_T avc1_packetizer_open( VC_PACKETIZER_T * );

/*****************************************************************************/
typedef struct VC_PACKETIZER_MODULE_T {
   enum {
      STATE_FRAME_WAIT = 0,
      STATE_BUFFER_INIT,
      STATE_NAL_START,
      STATE_NAL_DATA,
   } state;

   unsigned int length_size;

   unsigned int frame_size;
   unsigned int bytes_read;
   unsigned int start_code_bytes_left;
   unsigned int nal_bytes_left;

} VC_PACKETIZER_MODULE_T;

static const uint8_t h264_start_code[] = {0, 0, 0, 1};

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avc1_packetizer_close( VC_PACKETIZER_T *p_ctx )
{
   free(p_ctx->priv->module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avc1_packetizer_reset( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   module->state = STATE_FRAME_WAIT;
   module->frame_size = 0;
   module->bytes_read = 0;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avc1_packetizer_packetize( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T *out, VC_PACKETIZER_FLAGS_T flags)
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_BYTESTREAM_T *stream = &p_ctx->priv->stream;
   VC_CONTAINER_PACKET_T *packet;
   unsigned int offset, size, nal_num;
   uint8_t data[4];
   VC_CONTAINER_PARAM_UNUSED(nal_num);

   while(1) switch (module->state)
   {
   case STATE_FRAME_WAIT:
      for (packet = stream->current, size = 0;
           packet && !(packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_END);
           packet = packet->next)
         size += packet->size;
      if (!packet)
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA; /* We need more data */

      size += packet->size;

      /* We now have a complete frame available */

      module->nal_bytes_left = 0;
      module->start_code_bytes_left = 0;

      /* Find out the number of NAL units and size of the frame */
      for (offset = nal_num = 0; offset + module->length_size < size; nal_num++)
      {
         unsigned int nal_size;

         bytestream_peek_at(stream, offset, data, module->length_size);
         offset += module->length_size;

         nal_size = data[0];
         if (module->length_size > 1)
            nal_size = (nal_size << 8)|data[1];
         if (module->length_size > 2)
            nal_size = (nal_size << 8)|data[2];
         if (module->length_size > 3)
            nal_size = (nal_size << 8)|data[3];
         if (offset + nal_size > size)
            nal_size = size - offset;

         offset += nal_size;
         module->frame_size += nal_size + sizeof(h264_start_code);
#ifdef ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
         LOG_DEBUG(0, "nal unit size %u", nal_size);
#endif
      }
      LOG_DEBUG(0, "frame size: %u(%u/%u), pts: %"PRIi64, module->frame_size,
         size, nal_num, stream->current->pts);

      /* fall through to the next state */
      module->state = STATE_BUFFER_INIT;

   case STATE_BUFFER_INIT:
      packet = stream->current;
      out->size = module->frame_size - module->bytes_read;
      out->pts = out->dts = VC_CONTAINER_TIME_UNKNOWN;
      out->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END;

      if (!module->bytes_read)
      {
         out->pts = packet->pts;
         out->dts = packet->dts;
         out->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
      }

      if (flags & VC_PACKETIZER_FLAG_INFO)
         return VC_CONTAINER_SUCCESS;

      if (flags & VC_PACKETIZER_FLAG_SKIP)
      {
         /* The easiest is to just drop all the packets belonging to the frame */
         while (!(stream->current->flags & VC_CONTAINER_PACKET_FLAG_FRAME_END))
            bytestream_skip_packet(stream);
         bytestream_skip_packet(stream);

         module->frame_size = 0;
         module->bytes_read = 0;
         return VC_CONTAINER_SUCCESS;
      }

      /* We now know that we'll have to read some data so reset the output size */
      out->size = 0;

      /* Go to the next relevant state */
      module->state = STATE_NAL_START;
      if (module->nal_bytes_left || module->bytes_read == module->frame_size)
         module->state = STATE_NAL_DATA;
      break;

   case STATE_NAL_START:
      /* Extract the size of the current NAL */
      bytestream_get(stream, data, module->length_size);

      module->nal_bytes_left = data[0];
      if (module->length_size > 1)
         module->nal_bytes_left = (module->nal_bytes_left << 8)|data[1];
      if (module->length_size > 2)
         module->nal_bytes_left = (module->nal_bytes_left << 8)|data[2];
      if (module->length_size > 3)
        module->nal_bytes_left = (module->nal_bytes_left << 8)|data[3];

      if (module->bytes_read + module->nal_bytes_left + sizeof(h264_start_code) >
          module->frame_size)
      {
         LOG_ERROR(0, "truncating nal (%u/%u)", module->nal_bytes_left,
            module->frame_size - module->bytes_read - sizeof(h264_start_code));
         module->nal_bytes_left = module->frame_size - sizeof(h264_start_code);
      }

#ifdef ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
      LOG_DEBUG(0, "nal unit size %u", module->nal_bytes_left);
#endif

      module->start_code_bytes_left = sizeof(h264_start_code);

      /* fall through to the next state */
      module->state = STATE_NAL_DATA;

   case STATE_NAL_DATA:
      /* Start by adding the start code */
      if (module->start_code_bytes_left)
      {
         size = MIN(out->buffer_size - out->size, module->start_code_bytes_left);
         memcpy(out->data + out->size, h264_start_code + sizeof(h264_start_code) -
                module->start_code_bytes_left, size);
         module->start_code_bytes_left -= size;
         module->bytes_read += size;
         out->size += size;
      }

      /* Then append the NAL unit itself */
      if (module->nal_bytes_left)
      {
         size = MIN(out->buffer_size - out->size, module->nal_bytes_left);
         bytestream_get( stream, out->data + out->size, size );
         module->nal_bytes_left -= size;
         module->bytes_read += size;
         out->size += size;
      }

      /* Check whether we're done */
      if (module->bytes_read == module->frame_size)
      {
         bytestream_skip_packet(stream);
         module->state = STATE_FRAME_WAIT;
         module->frame_size = 0;
         module->bytes_read = 0;
         return VC_CONTAINER_SUCCESS;
      }
      else if (out->buffer_size == out->size)
      {
         out->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;
         module->state = STATE_BUFFER_INIT;
         return VC_CONTAINER_SUCCESS;
      }

      /* We're not done, go to the next relevant state */
      module->state = STATE_NAL_START;
      break;

   default:
      break;
   };

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avc1_packetizer_codecconfig( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint8_t *out, *extra = p_ctx->in->extradata + 5;
   uint8_t *extra_end = extra + p_ctx->in->extradata_size - 5;
   unsigned int i, j, nal_size, out_size = 0;

   if (p_ctx->in->extradata_size <= 5 ||
       p_ctx->in->extradata[0] != 1 /* configurationVersion */)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   status = vc_container_format_extradata_alloc(p_ctx->out, p_ctx->in->extradata_size);
   if (status != VC_CONTAINER_SUCCESS)
      return status;

   out = p_ctx->out->extradata;
   module->length_size = (*(p_ctx->in->extradata + 4) & 0x3) + 1;

   for (i = 0; i < 2 && extra < extra_end - 1; i++)
   {
      j = *(extra++) & (!i ? 0x1F : 0xFF);
      for (; j > 0 && extra < extra_end - 2; j--)
      {
         nal_size = (extra[0] << 8) | extra[1]; extra += 2;
         if (extra + nal_size > extra_end)
         {
            extra = extra_end;
            break;
         }

         out[0] = out[1] = out[2] = 0; out[3] = 1;
         memcpy(out + 4, extra, nal_size);
         out += nal_size + 4; extra += nal_size;
         out_size += nal_size + 4;
      }
   }

   p_ctx->out->extradata_size = out_size;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T avc1_packetizer_open( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module;
   VC_CONTAINER_STATUS_T status;

   if(p_ctx->in->codec != VC_CONTAINER_CODEC_H264 &&
      p_ctx->out->codec != VC_CONTAINER_CODEC_H264)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(p_ctx->in->codec_variant != VC_CONTAINER_VARIANT_H264_AVC1 &&
      p_ctx->out->codec_variant != VC_CONTAINER_VARIANT_H264_DEFAULT)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(!(p_ctx->in->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   p_ctx->priv->module = module = malloc(sizeof(*module));
   if(!module)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));

   vc_container_format_copy(p_ctx->out, p_ctx->in, 0);
   status = avc1_packetizer_codecconfig(p_ctx);
   if (status != VC_CONTAINER_SUCCESS)
   {
      free(module);
      return status;
   }

   p_ctx->out->codec_variant = VC_CONTAINER_VARIANT_H264_DEFAULT;
   p_ctx->max_frame_size = MAX_FRAME_SIZE;
   p_ctx->priv->pf_close = avc1_packetizer_close;
   p_ctx->priv->pf_packetize = avc1_packetizer_packetize;
   p_ctx->priv->pf_reset = avc1_packetizer_reset;
   LOG_DEBUG(0, "using avc1 video packetizer");
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_PACKETIZER_REGISTER(avc1_packetizer_open,  "avc1");
