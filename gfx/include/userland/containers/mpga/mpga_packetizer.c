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
 * Implementation of an MPEG1/2/2.5 audio Layer I/II/III and AAC ADTS packetizer.
 */

#include <stdlib.h>
#include <string.h>

#include "containers/packetizers.h"
#include "containers/core/packetizers_private.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_time.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_bytestream.h"
#include "mpga_common.h"

#define MAX_FRAME_SIZE 2881 /* MPEG 2.5 Layer II, 8000 Hz, 160 kbps */

VC_CONTAINER_STATUS_T mpga_packetizer_open( VC_PACKETIZER_T * );

/*****************************************************************************/
typedef struct VC_PACKETIZER_MODULE_T {
   enum {
      STATE_SYNC = 0,
      STATE_SYNC_LOST,
      STATE_SYNC_NEXT,
      STATE_SYNC_DONE,
      STATE_HEADER,
      STATE_DATA,
   } state;

   VC_CONTAINER_STATUS_T (*pf_read_header)( uint8_t frame_header[MPGA_HEADER_SIZE],
      uint32_t *p_frame_size, unsigned int *p_frame_bitrate, unsigned int *p_version,
      unsigned int *p_layer, unsigned int *p_sample_rate, unsigned int *p_channels,
      unsigned int *p_frame_size_samples, unsigned int *p_offset);

   uint32_t frame_size;
   unsigned int frame_bitrate;
   unsigned int version;
   unsigned int layer;
   unsigned int sample_rate;
   unsigned int channels;
   unsigned int frame_size_samples;
   unsigned int offset;

   unsigned int lost_sync;

   unsigned int stream_version;
   unsigned int stream_layer;

   uint32_t bytes_read;

} VC_PACKETIZER_MODULE_T;

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_packetizer_close( VC_PACKETIZER_T *p_ctx )
{
   free(p_ctx->priv->module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_packetizer_reset( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   module->lost_sync = 0;
   module->state = STATE_SYNC;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_packetizer_packetize( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T *out, VC_PACKETIZER_FLAGS_T flags )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_BYTESTREAM_T *stream = &p_ctx->priv->stream;
   VC_CONTAINER_TIME_T *time = &p_ctx->priv->time;
   uint8_t header[MPGA_HEADER_SIZE];
   VC_CONTAINER_STATUS_T status;
   unsigned int version, layer;
   int64_t pts, dts;

   while(1) switch (module->state)
   {
   case STATE_SYNC_LOST:
      bytestream_skip_byte( stream );
      if( !module->lost_sync++ )
         LOG_DEBUG(0, "lost sync");
      module->state = STATE_SYNC;

   case STATE_SYNC:
      while( bytestream_peek( stream, header, 2 ) == VC_CONTAINER_SUCCESS )
      {
          /* 11 bits sync work (0xffe) */
          if( header[0] == 0xff && (header[1] & 0xe0) == 0xe0 )
          {
             module->state = STATE_HEADER;
             break;
          }
          bytestream_skip_byte( stream );
          module->lost_sync++;
      }
      if( module->state != STATE_HEADER )
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA; /* We need more data */

   case STATE_HEADER:
      if( bytestream_peek( stream, header, MPGA_HEADER_SIZE ) != VC_CONTAINER_SUCCESS )
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA;

      status = mpga_read_header( header,
         &module->frame_size, &module->frame_bitrate, &module->version,
         &module->layer, &module->sample_rate, &module->channels,
         &module->frame_size_samples, &module->offset );
      if (status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(0, "invalid header");
         module->state = STATE_SYNC_LOST;
         break;
      }

      /* Version and layer are not allowed to change mid-stream */
      if ((module->stream_version && module->stream_version != module->version) ||
          (module->stream_layer && module->stream_layer != module->layer))
      {
         LOG_ERROR(0, "invalid header");
         module->state = STATE_SYNC_LOST;
         break;
      }
      /* We currently do not support free format streams  */
      if (!module->frame_size)
      {
         LOG_ERROR(0, "free format not supported");
         module->state = STATE_SYNC_LOST;
         break;
      }
      module->state = STATE_SYNC_NEXT;
      /* fall through to the next state */

   case STATE_SYNC_NEXT:
      /* To avoid being caught by emulated start codes, we also look at where the next frame is supposed to be */
      if( bytestream_peek_at( stream, module->frame_size, header, MPGA_HEADER_SIZE ) != VC_CONTAINER_SUCCESS )
      {
         /* If we know there won't be anymore data then we can just assume
          * we've got the frame we're looking for */
         if (flags & VC_PACKETIZER_FLAG_FLUSH)
         {
            module->state = STATE_SYNC_DONE;
            break;
         }
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
      }

      status = mpga_read_header( header, 0, 0, &version, &layer, 0, 0, 0, 0 );
      if (status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(0, "invalid next header");
         module->state = STATE_SYNC_LOST;
         break;
      }

      /* Version and layer are not allowed to change mid-stream */
      if (module->version != version || module->layer != layer)
      {
         LOG_ERROR(0, "invalid header");
         module->state = STATE_SYNC_LOST;
         break;
      }

      module->state = STATE_SYNC_DONE;
      /* fall through to the next state */

   case STATE_SYNC_DONE:
      if( module->lost_sync )
         LOG_DEBUG(0, "recovered sync after %i bytes", module->lost_sync);
      module->lost_sync = 0;

      bytestream_skip( stream, module->offset );
      module->stream_version = module->version;
      module->stream_layer = module->layer;

      vc_container_time_set_samplerate(time, module->sample_rate, 1);
      bytestream_get_timestamps(stream, &pts, &dts, true);

      vc_container_time_set(time, pts);

      module->bytes_read = 0;
      module->state = STATE_DATA;
      /* fall through to the next state */

   case STATE_DATA:
      if( bytestream_size( stream ) < module->frame_size)
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA;

      out->size = module->frame_size - module->bytes_read;
      out->pts = out->dts = VC_CONTAINER_TIME_UNKNOWN;
      out->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END;

      if(!module->bytes_read)
      {
         out->pts = out->dts = vc_container_time_get(time);
         out->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
      }

      if(flags & VC_PACKETIZER_FLAG_INFO)
         return VC_CONTAINER_SUCCESS;

      if(flags & VC_PACKETIZER_FLAG_SKIP)
      {
         bytestream_skip( stream, out->size );
      }
      else
      {
         out->size = MIN(out->size, out->buffer_size);
         bytestream_get( stream, out->data, out->size );
      }
      module->bytes_read += out->size;

      if(module->bytes_read == module->frame_size)
      {
         vc_container_time_add(time, module->frame_size_samples);
         module->state = STATE_HEADER;
      }
      return VC_CONTAINER_SUCCESS;

   default:
      break;
   };

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T mpga_packetizer_open( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module;

   if(p_ctx->in->codec != VC_CONTAINER_CODEC_MPGA &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_MP4A)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   p_ctx->priv->module = module = malloc(sizeof(*module));
   if(!module)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));

   if(p_ctx->in->codec == VC_CONTAINER_CODEC_MPGA)
      module->pf_read_header = mpga_read_header;
   else
      module->pf_read_header = adts_read_header;

   vc_container_format_copy( p_ctx->out, p_ctx->in, 0);
   p_ctx->max_frame_size = MAX_FRAME_SIZE;
   p_ctx->priv->pf_close = mpga_packetizer_close;
   p_ctx->priv->pf_packetize = mpga_packetizer_packetize;
   p_ctx->priv->pf_reset = mpga_packetizer_reset;
   LOG_DEBUG(0, "using mpeg audio packetizer");
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_PACKETIZER_REGISTER(mpga_packetizer_open,  "mpga");
