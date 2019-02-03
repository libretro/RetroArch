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

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/

#define BI32(b) (((b)[0]<<24)|((b)[1]<<16)|((b)[2]<<8)|((b)[3]))
#define BI16(b) (((b)[0]<<8)|((b)[1]))

#define FRAME_HEADER_LEN 20
#define MAX_NUM_SEGMENTS 64

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct {
   uint32_t len;
   uint32_t timestamp;
   uint16_t sequence;
   uint16_t flags;
   uint32_t num_segments;
   uint32_t seg_offset;
} RV9_FRAME_HEADER_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   uint8_t mid_frame;
   uint32_t frame_read;
   uint32_t frame_len;
   RV9_FRAME_HEADER_T hdr;
   uint8_t data[FRAME_HEADER_LEN + (MAX_NUM_SEGMENTS<<3) + 1];
   uint32_t data_len;
   uint8_t type;
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T rv9_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

static VC_CONTAINER_STATUS_T rv9_read_file_header(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *track)
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_FOURCC_T codec;
   uint8_t dummy[12];
   uint32_t length;

   if(PEEK_BYTES(p_ctx, dummy, sizeof(dummy)) != sizeof(dummy)) return VC_CONTAINER_ERROR_EOS;

   length = BI32(dummy);
   if(length < 12 || length > 1024) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if(dummy[4] != 'V' || dummy[5] != 'I' || dummy[6] != 'D' || dummy[7] != 'O' ||
      dummy[8] != 'R' || dummy[9] != 'V' ||                    dummy[11] != '0')
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   switch(dummy[10]) {
   case '4': codec = VC_CONTAINER_CODEC_RV40; break;
   case '3': codec = VC_CONTAINER_CODEC_RV30; break;
   case '2': codec = VC_CONTAINER_CODEC_RV20; break;
   case '1': codec = VC_CONTAINER_CODEC_RV10; break;
   default: return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   if (!track)
      return VC_CONTAINER_SUCCESS;

   status = vc_container_track_allocate_extradata(p_ctx, track, length);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(READ_BYTES(p_ctx, track->format->extradata, length) != length) return VC_CONTAINER_ERROR_EOS;
   track->format->extradata_size = length;

   track->format->codec = codec;
   return STREAM_STATUS(p_ctx);
}

static VC_CONTAINER_STATUS_T rv9_read_frame_header(VC_CONTAINER_T *p_ctx)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t seg_offset = (uint32_t) -1;
   uint8_t *buffer = module->data + FRAME_HEADER_LEN;

   if(READ_BYTES(p_ctx, module->data, FRAME_HEADER_LEN) != FRAME_HEADER_LEN) return VC_CONTAINER_ERROR_EOS;
   module->data_len = FRAME_HEADER_LEN;

   module->hdr.len = BI32(module->data);
   module->hdr.timestamp = BI32(module->data+4);
   module->hdr.sequence = BI16(module->data+8);
   module->hdr.flags = BI16(module->data+10);
   module->hdr.num_segments = BI32(module->data+16);

   module->frame_len = FRAME_HEADER_LEN + (module->hdr.num_segments * 8) + module->hdr.len;

   // if we have space, we store up the segments in memory so we can tell the frame
   // type, since most streams have their type byte as the first follow the segment information.
   // if we don't have space, then we just don't know the frame type, so will not emit timestamp
   // information as we don't know if it's reliable.
   if(module->hdr.num_segments <= MAX_NUM_SEGMENTS)
   {
      uint32_t i;

      if(READ_BYTES(p_ctx, buffer, 8*module->hdr.num_segments) != 8*module->hdr.num_segments) return VC_CONTAINER_ERROR_EOS;
      module->data_len += (module->hdr.num_segments * 8);

      for (i=0; i<module->hdr.num_segments; i++)
      {
         uint32_t valid_seg;
         uint32_t offset;

         valid_seg = BI32(buffer);
         offset = BI32(buffer+4);

         if (valid_seg && seg_offset > offset)
            seg_offset = offset;

         // this boolean field should have only 0 or 1 values
         if(valid_seg > 1) return VC_CONTAINER_ERROR_FORMAT_INVALID;

         buffer += 8;
      }
   }

   if(seg_offset == 0)
   {
      if (READ_BYTES(p_ctx, buffer, 1) != 1) return VC_CONTAINER_ERROR_EOS;
      module->data_len += 1;

      module->type = (*buffer >> 5) & 3;
   }
   else
      module->type = (uint8_t) -1;

   return VC_CONTAINER_SUCCESS;
}

static uint32_t rv9_get_frame_data(VC_CONTAINER_T *p_ctx, uint32_t len, uint8_t *dest)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t ret = 0;

   // we may have read some data before into the data array, so
   // check whether we've copied all this data out first.
   if(module->frame_read < module->data_len)
   {
      uint32_t copy = MIN(len, module->data_len - module->frame_read);
      if(dest)
      {
         memcpy(dest, module->data + module->frame_read, copy);
         dest += copy;
      }
      ret += copy;
      len -= copy;
   }

   // if there is still more to do, we need to access the IO to do this.
   if(len > 0)
   {
      if(dest)
         ret += READ_BYTES(p_ctx, dest, len);
      else
         ret += SKIP_BYTES(p_ctx, len);
   }

   module->frame_read += ret;
   return ret;
}

/*****************************************************************************
Functions exported as part of the Container Module API
*****************************************************************************/
static VC_CONTAINER_STATUS_T rv9_reader_read( VC_CONTAINER_T *p_ctx,
                                              VC_CONTAINER_PACKET_T *packet,
                                              uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   unsigned int size;

   if(!module->mid_frame)
   {
      if((status = rv9_read_frame_header(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

      module->mid_frame = 1;
      module->frame_read = 0;
   }

   packet->size = module->frame_len;
   packet->pts = module->type < 3 ? module->hdr.timestamp * 1000LL : VC_CONTAINER_TIME_UNKNOWN;
   packet->dts = packet->pts;
   packet->track = 0;
   packet->flags = module->type < 2 ? VC_CONTAINER_PACKET_FLAG_KEYFRAME : 0;
   if(module->frame_read == 0)
      packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      size = rv9_get_frame_data(p_ctx, module->frame_len - module->frame_read, NULL);
      if(module->frame_read == module->frame_len)
      {
         module->frame_read = 0;
         module->mid_frame = 0;
      }
      return STREAM_STATUS(p_ctx);
   }

   if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   size = MIN(module->frame_len - module->frame_read, packet->buffer_size);
   size = rv9_get_frame_data(p_ctx, size, packet->data);
   if(module->frame_read == module->frame_len)
   {
      module->frame_read = 0;
      module->mid_frame = 0;
      packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
   }
   packet->size = size;

   return size ? VC_CONTAINER_SUCCESS : STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rv9_reader_seek( VC_CONTAINER_T *p_ctx,
                                              int64_t *offset,
                                              VC_CONTAINER_SEEK_MODE_T mode,
                                              VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(flags);

   if(*offset == 0LL && mode == VC_CONTAINER_SEEK_MODE_TIME)
   {
      SEEK(p_ctx, module->track->format->extradata_size);
      module->mid_frame = 0;
      module->frame_read = 0;
      return STREAM_STATUS(p_ctx);
   }
   else
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rv9_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T rv9_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Check the file header */
   if(rv9_read_file_header(p_ctx, 0) != VC_CONTAINER_SUCCESS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks_num = 1;
   p_ctx->tracks = &module->track;
   p_ctx->tracks[0] = vc_container_allocate_track(p_ctx, 0);
   if(!p_ctx->tracks[0]) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   p_ctx->tracks[0]->format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   p_ctx->tracks[0]->format->codec = VC_CONTAINER_CODEC_RV40;
   p_ctx->tracks[0]->is_enabled = true;

   if((status = rv9_read_file_header(p_ctx, p_ctx->tracks[0])) != VC_CONTAINER_SUCCESS) goto error;

   LOG_DEBUG(p_ctx, "using rv9 reader");

   p_ctx->priv->pf_close = rv9_reader_close;
   p_ctx->priv->pf_read = rv9_reader_read;
   p_ctx->priv->pf_seek = rv9_reader_seek;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "rv9: error opening stream (%i)", status);
   if(module) rv9_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open rv9_reader_open
#endif
