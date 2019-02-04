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
 * Implementation of an MPEG1/2 video packetizer.
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

/** Arbitrary number which should be sufficiently high so that no sane frame will
 * be bigger than that. */
#define MAX_FRAME_SIZE (1920*1088*2)

static uint8_t mpgv_startcode[3] = {0x0, 0x0, 0x1};

#define PICTURE_CODING_TYPE_I 0x1
#define PICTURE_CODING_TYPE_P 0x2
#define PICTURE_CODING_TYPE_B 0x3

VC_CONTAINER_STATUS_T mpgv_packetizer_open( VC_PACKETIZER_T * );

/*****************************************************************************/
typedef struct VC_PACKETIZER_MODULE_T {
   enum {
      STATE_SYNC = 0,
      STATE_SYNC_NEXT,
      STATE_FRAME_DONE,
      STATE_UNIT_HEADER,
      STATE_UNIT_SEQUENCE,
      STATE_UNIT_GROUP,
      STATE_UNIT_PICTURE,
      STATE_UNIT_SLICE,
      STATE_UNIT_OTHER,
      STATE_DATA,
   } state;

   size_t frame_size;
   size_t unit_offset;

   unsigned int seen_sequence_header;
   unsigned int seen_picture_header;
   unsigned int seen_slice;
   unsigned int lost_sync;

   unsigned int picture_type;
   unsigned int picture_temporal_ref;
   int64_t pts;
   int64_t dts;

   uint32_t bytes_read;

   unsigned int width, height;
   unsigned int frame_rate_num, frame_rate_den;
   unsigned int aspect_ratio_num, aspect_ratio_den;
   bool low_delay;

} VC_PACKETIZER_MODULE_T;

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpgv_packetizer_close( VC_PACKETIZER_T *p_ctx )
{
   free(p_ctx->priv->module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpgv_packetizer_reset( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   module->lost_sync = 0;
   module->state = STATE_SYNC;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpgv_read_sequence_header(VC_CONTAINER_BYTESTREAM_T *stream,
   size_t offset, unsigned int *width, unsigned int *height,
   unsigned int *frame_rate_num, unsigned int *frame_rate_den,
   unsigned int *aspect_ratio_num, unsigned int *aspect_ratio_den)
{
   static const int frame_rate[16][2] =
   { {0, 0}, {24000, 1001}, {24, 1}, {25, 1}, {30000, 1001}, {30, 1}, {50, 1},
     {60000, 1001}, {60, 1},
     /* Unofficial values */
     {15, 1001}, /* From Xing */
     {5, 1001}, {10, 1001}, {12, 1001}, {15, 1001} /* From libmpeg3 */ };
   static const int aspect_ratio[16][2] =
   { {0, 0}, {1, 1}, {4, 3}, {16, 9}, {221, 100} };

   VC_CONTAINER_STATUS_T status;
   unsigned int w, h, fr, ar;
   int64_t ar_num, ar_den, div;
   uint8_t header[8];

   status = bytestream_peek_at( stream, offset, header, sizeof(header));
   if(status != VC_CONTAINER_SUCCESS)
      return status;

   w = (header[4] << 4) | (header[5] >> 4);
   h = ((header[5]&0x0f) << 8) | header[6];
   ar = header[7] >> 4;
   fr = header[7]&0x0f;
   if (!w || !h || !ar || !fr)
      return VC_CONTAINER_ERROR_CORRUPTED;

   *width = w;
   *height = h;
   *frame_rate_num = frame_rate[fr][0];
   *frame_rate_den = frame_rate[fr][1];
   ar_num = (int64_t)aspect_ratio[ar][0] * h;
   ar_den = (int64_t)aspect_ratio[ar][1] * w;
   div = vc_container_maths_gcd(ar_num, ar_den);
   if (div)
   {
      ar_num /= div;
      ar_den /= div;
   }
   *aspect_ratio_num = ar_num;
   *aspect_ratio_den = ar_den;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpgv_read_picture_header(VC_CONTAINER_BYTESTREAM_T *stream,
   size_t offset, unsigned int *type, unsigned int *temporal_ref)
{
   VC_CONTAINER_STATUS_T status;
   uint8_t h[2];

   status = bytestream_peek_at(stream, offset + sizeof(mpgv_startcode) + 1, h, sizeof(h));
   if(status != VC_CONTAINER_SUCCESS)
      return status;

   *temporal_ref = (h[0] << 2) | (h[1] >> 6);
   *type = (h[1] >> 3) & 0x7;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpgv_update_format( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;

   LOG_DEBUG(0, "mpgv format: width %i, height %i, rate %i/%i, ar %i/%i",
             module->width, module->height, module->frame_rate_num, module->frame_rate_den,
             module->aspect_ratio_num, module->aspect_ratio_den);

   p_ctx->out->type->video.width = p_ctx->out->type->video.visible_width = module->width;
   p_ctx->out->type->video.height = p_ctx->out->type->video.visible_height = module->height;
   p_ctx->out->type->video.par_num = module->aspect_ratio_num;
   p_ctx->out->type->video.par_den = module->aspect_ratio_den;
   p_ctx->out->type->video.frame_rate_num = module->frame_rate_num;
   p_ctx->out->type->video.frame_rate_den = module->frame_rate_den;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpgv_packetizer_packetize( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T *out, VC_PACKETIZER_FLAGS_T flags)
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_BYTESTREAM_T *stream = &p_ctx->priv->stream;
   VC_CONTAINER_TIME_T *time = &p_ctx->priv->time;
   VC_CONTAINER_STATUS_T status;
   uint8_t header[4];
   size_t offset;

   while(1) switch (module->state)
   {
   case STATE_SYNC:
      offset = 0;
      status = bytestream_find_startcode( stream, &offset,
         mpgv_startcode, sizeof(mpgv_startcode) );

      if(offset && !module->lost_sync)
         LOG_DEBUG(0, "lost sync");

      bytestream_skip(stream, offset);
      module->lost_sync += offset;

      if(status != VC_CONTAINER_SUCCESS)
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA; /* We need more data */

      if(module->lost_sync)
         LOG_DEBUG(0, "recovered sync after %i bytes", module->lost_sync);
      module->lost_sync = 0;
      module->state = STATE_UNIT_HEADER;
      module->frame_size = 0;
      module->unit_offset = 0;
      /* fall through to the next state */

   case STATE_UNIT_HEADER:
      status = bytestream_peek_at( stream, module->unit_offset, header, sizeof(header));
      if(status != VC_CONTAINER_SUCCESS)
      {
         if (!(flags & VC_PACKETIZER_FLAG_FLUSH) ||
             !module->seen_picture_header || !module->seen_slice)
            return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
         module->state = STATE_FRAME_DONE;
         break;
      }

#if defined(ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE)
      LOG_DEBUG(0, "found unit (%x)", header[3]);
#endif

      /* Detect start of new frame */
      if(module->seen_picture_header && module->seen_slice &&
         (header[3] == 0x00 /* A picture header */ ||
          (header[3] > 0xAF && header[3] != 0xB7) /* Not a slice or sequence end */))
      {
         module->state = STATE_FRAME_DONE;
         break;
      }

      module->frame_size += sizeof(mpgv_startcode);
      module->state = STATE_SYNC_NEXT;
      /* fall through to the next state */

   case STATE_SYNC_NEXT:
      status = bytestream_find_startcode( stream, &module->frame_size,
         mpgv_startcode, sizeof(mpgv_startcode) );

      /* Sanity check the size of frames. This makes sure we don't endlessly accumulate data
       * to make up a new frame. */
      if(module->frame_size > p_ctx->max_frame_size)
      {
         LOG_ERROR(0, "frame too big (%i/%i), dropping", module->frame_size, p_ctx->max_frame_size);
         bytestream_skip(stream, module->frame_size);
         module->state = STATE_SYNC;
         break;
      }
      if(status != VC_CONTAINER_SUCCESS)
      {
         if (!(flags & VC_PACKETIZER_FLAG_FLUSH) ||
             !module->seen_picture_header || !module->seen_slice)
            return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
         module->state = STATE_FRAME_DONE;
         break;
      }

      bytestream_peek_at( stream, module->unit_offset, header, sizeof(header));

      /* Drop everything until we've seen a sequence header */
      if(header[3] != 0xB3 && !module->seen_sequence_header)
      {
         LOG_DEBUG(0, "waiting for sequence header, dropping %i bytes", module->frame_size);
         module->state = STATE_UNIT_HEADER;
         bytestream_skip(stream, module->frame_size);
         module->unit_offset = module->frame_size = 0;
         break;
      }

      if(header[3] == 0x00)
         module->state = STATE_UNIT_PICTURE;
      else if(header[3] >= 0x01 && header[3] <= 0xAF)
         module->state = STATE_UNIT_SLICE;
      else if(header[3] == 0xB3)
         module->state = STATE_UNIT_SEQUENCE;
      else if(header[3] == 0xB8)
         module->state = STATE_UNIT_GROUP;
      else
         module->state = STATE_UNIT_OTHER;
      break;

   case STATE_UNIT_SEQUENCE:
      status = mpgv_read_sequence_header(stream, module->unit_offset, &module->width, &module->height,
         &module->frame_rate_num, &module->frame_rate_den, &module->aspect_ratio_num, &module->aspect_ratio_den);
      if(status != VC_CONTAINER_SUCCESS && !module->seen_sequence_header)
      {
         /* We need a sequence header so drop everything until we see one */
         LOG_DEBUG(0, "invalid first sequence header, dropping %i bytes", module->frame_size);
         bytestream_skip(stream, module->frame_size);
         module->state = STATE_SYNC;
         break;
      }
      mpgv_update_format(p_ctx);
      module->seen_sequence_header = true;
      vc_container_time_set_samplerate(time, module->frame_rate_num, module->frame_rate_den);
      module->state = STATE_UNIT_HEADER;
      module->unit_offset = module->frame_size;
      break;

   case STATE_UNIT_PICTURE:
      status = mpgv_read_picture_header(stream, module->unit_offset, &module->picture_type, &module->picture_temporal_ref);
      if(status != VC_CONTAINER_SUCCESS)
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
      module->seen_picture_header = true;
      module->state = STATE_UNIT_HEADER;
      module->unit_offset = module->frame_size;
      break;

   case STATE_UNIT_SLICE:
      module->seen_slice = true;
      module->state = STATE_UNIT_HEADER;
      module->unit_offset = module->frame_size;
      break;

   case STATE_UNIT_GROUP:
   case STATE_UNIT_OTHER:
      module->state = STATE_UNIT_HEADER;
      module->unit_offset = module->frame_size;
      break;

   case STATE_FRAME_DONE:
      bytestream_get_timestamps(stream, &module->pts, &module->dts, false);

      if(module->picture_type == PICTURE_CODING_TYPE_B || module->low_delay)
      {
         if(module->pts == VC_CONTAINER_TIME_UNKNOWN)
            module->pts = module->dts;
         if(module->dts == VC_CONTAINER_TIME_UNKNOWN)
            module->dts = module->pts;
      }
      vc_container_time_set(time, module->pts);

      module->bytes_read = 0;
      module->state = STATE_DATA;
      module->seen_slice = false;
      module->seen_picture_header = false;

#if defined(ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE)
      LOG_DEBUG(0, "new frame, type %x, size %i, temp_ref %i)", module->picture_type,
                module->frame_size, module->picture_temporal_ref);
#endif
      /* fall through to the next state */

   case STATE_DATA:
      out->size = module->frame_size - module->bytes_read;
      out->pts = out->dts = VC_CONTAINER_TIME_UNKNOWN;
      out->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END;

      if(!module->bytes_read)
      {
         out->pts = module->pts;
         out->dts = module->dts;
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
         vc_container_time_add(time, 1);
         module->state = STATE_UNIT_HEADER;
         module->frame_size = 0;
         module->unit_offset = 0;
      }
      else
         out->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;

      return VC_CONTAINER_SUCCESS;

   default:
      break;
   };

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T mpgv_packetizer_open( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module;

   if(p_ctx->in->codec != VC_CONTAINER_CODEC_MP1V &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_MP2V)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   p_ctx->priv->module = module = malloc(sizeof(*module));
   if(!module)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));

   vc_container_format_copy( p_ctx->out, p_ctx->in, 0);
   p_ctx->max_frame_size = MAX_FRAME_SIZE;
   p_ctx->priv->pf_close = mpgv_packetizer_close;
   p_ctx->priv->pf_packetize = mpgv_packetizer_packetize;
   p_ctx->priv->pf_reset = mpgv_packetizer_reset;
   LOG_DEBUG(0, "using mpeg video packetizer");
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_PACKETIZER_REGISTER(mpgv_packetizer_open,  "mpgv");
