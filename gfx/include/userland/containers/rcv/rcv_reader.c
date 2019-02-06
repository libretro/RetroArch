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
#include "containers/core/containers_index.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/

#define LI32(b) (((b)[3]<<24)|((b)[2]<<16)|((b)[1]<<8)|((b)[0]))
#define LI24(b) (((b)[2]<<16)|((b)[1]<<8)|((b)[0]))

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct {
   unsigned int num_frames : 24;
   unsigned int constant_c5 : 8;
   int constant_4;
   uint32_t struct_c;
   uint32_t vert_size;
   uint32_t horiz_size;
   int constant_c;
   uint32_t struct_b[2];
   uint32_t framerate;
} RCV_FILE_HEADER_T;

typedef struct {
   unsigned int framesize : 24;
   unsigned int res : 7;
   unsigned int keyframe : 1;
   uint32_t timestamp;
} RCV_FRAME_HEADER_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   uint8_t extradata[4];
   uint8_t mid_frame;
   uint32_t frame_read;
   RCV_FRAME_HEADER_T frame;
   VC_CONTAINER_INDEX_T *index; /* index of key frames */

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T rcv_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

static VC_CONTAINER_STATUS_T rcv_read_header(VC_CONTAINER_T *p_ctx)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   RCV_FILE_HEADER_T header;
   uint8_t dummy[36];

   if(PEEK_BYTES(p_ctx, dummy, sizeof(dummy)) != sizeof(dummy)) return VC_CONTAINER_ERROR_EOS;

   header.num_frames = LI24(dummy);
   header.constant_c5 = dummy[3];
   header.constant_4 = LI32(dummy+4);

   // extradata is just struct_c from the header
   memcpy(module->extradata, dummy+8, 4);
   module->track->format->extradata = module->extradata;
   module->track->format->extradata_size = 4;

   module->track->format->type->video.height = LI32(dummy+12);
   module->track->format->type->video.width = LI32(dummy+16);

   header.constant_c = LI32(dummy+20);
   memcpy(header.struct_b, dummy+24, 8);
   header.framerate = LI32(dummy+32);

   if(header.constant_c5 != 0xc5 || header.constant_4 != 0x4 || header.constant_c != 0xc)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if(header.framerate != 0 && header.framerate != 0xffffffffUL)
   {
      module->track->format->type->video.frame_rate_num = header.framerate;
      module->track->format->type->video.frame_rate_den = 1;
   }

   // fill in general information
   if(header.num_frames != (1<<24)-1 && header.framerate != 0 && header.framerate != 0xffffffffUL)
      p_ctx->duration = ((int64_t) header.num_frames * 1000000LL) / (int64_t) header.framerate;

   // we're happy that this is an rcv file
   SKIP_BYTES(p_ctx, sizeof(dummy));

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************
 * Utility function to seek to the keyframe nearest the given timestamp.
 *
 * @param p_ctx     Pointer to the container context.
 * @param timestamp The requested time.  On success, this is updated with the time of the selected keyframe.
 * @param later     If true, the selected frame is the earliest keyframe with a time greater or equal to timestamp.
 *                  If false, the selected frame is the latest keyframe with a time earlier or equal to timestamp.
 * @return          Status code.
 */
static VC_CONTAINER_STATUS_T rcv_seek_nearest_keyframe(VC_CONTAINER_T *p_ctx, int64_t *timestamp, int later)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t prev_keyframe_offset = sizeof(RCV_FILE_HEADER_T); /* set to very first frame */
   int64_t prev_keyframe_timestamp = 0;
   int use_prev_keyframe = !later;

   if(use_prev_keyframe || (module->frame.timestamp * 1000LL > *timestamp))
   {
      /* A seek has been requested to an earlier keyframe, so rewind to the beginning
       * of the stream since there's no information available on previous frames */
      SEEK(p_ctx, sizeof(RCV_FILE_HEADER_T));
      memset(&module->frame, 0, sizeof(RCV_FRAME_HEADER_T));
      module->mid_frame = 0;
      module->frame_read = 0;
   }

   if(module->mid_frame)
   {
      /* Seek back to the start of the current frame */
      SEEK(p_ctx, STREAM_POSITION(p_ctx) - module->frame_read - sizeof(RCV_FILE_HEADER_T));
      module->mid_frame = 0;
      module->frame_read = 0;
   }

   while(1)
   {
      if(PEEK_BYTES(p_ctx, &module->frame, sizeof(RCV_FRAME_HEADER_T)) != sizeof(RCV_FRAME_HEADER_T))
      {
         status = VC_CONTAINER_ERROR_EOS;
         break;
      }

      if(module->frame.keyframe)
      {
         if(module->index)
            vc_container_index_add(module->index, module->frame.timestamp * 1000LL, STREAM_POSITION(p_ctx));

         if((module->frame.timestamp * 1000LL) >= *timestamp)
         {
            if((module->frame.timestamp * 1000LL) == *timestamp)
               use_prev_keyframe = 0;

            *timestamp = module->frame.timestamp * 1000LL;

            break;
         }

         prev_keyframe_offset = STREAM_POSITION(p_ctx);
         prev_keyframe_timestamp = module->frame.timestamp * 1000LL;
      }

      SKIP_BYTES(p_ctx, module->frame.framesize + sizeof(RCV_FRAME_HEADER_T));
   }

   if(use_prev_keyframe)
   {
      *timestamp = prev_keyframe_timestamp;
      status = SEEK(p_ctx, prev_keyframe_offset);
   }

   return status;
}

/*****************************************************************************
Functions exported as part of the Container Module API
*****************************************************************************/
static VC_CONTAINER_STATUS_T rcv_reader_read( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int size;

   if(!module->mid_frame)
   {
      /* Save the current position for updating the indexer */
      int64_t position = STREAM_POSITION(p_ctx);

      if(READ_BYTES(p_ctx, &module->frame, sizeof(RCV_FRAME_HEADER_T)) != sizeof(RCV_FRAME_HEADER_T))
         return VC_CONTAINER_ERROR_EOS;
      module->mid_frame = 1;
      module->frame_read = 0;

      if(module->index && module->frame.keyframe)
         vc_container_index_add(module->index, (int64_t)module->frame.timestamp * 1000LL, position);
   }

   packet->size = module->frame.framesize;
   packet->dts = packet->pts = module->frame.timestamp * 1000LL;
   packet->track = 0;
   packet->flags = 0;
   if(module->frame_read == 0)
      packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   if(module->frame.keyframe)
      packet->flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      size = SKIP_BYTES(p_ctx, module->frame.framesize - module->frame_read);
      if((module->frame_read += size) == module->frame.framesize)
      {
         module->frame_read = 0;
         module->mid_frame = 0;
      }
      return STREAM_STATUS(p_ctx);
   }

   if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   size = MIN(module->frame.framesize - module->frame_read, packet->buffer_size);
   size = READ_BYTES(p_ctx, packet->data, size);
   if((module->frame_read += size) == module->frame.framesize)
   {
      module->frame_read = 0;
      module->mid_frame = 0;
      packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
   }
   packet->size = size;

   return size ? VC_CONTAINER_SUCCESS : STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rcv_reader_seek( VC_CONTAINER_T *p_ctx, int64_t *offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   int past = 1;
   int64_t position;
   int64_t timestamp = *offset;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FAILED;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(mode);

   if(module->index)
      status = vc_container_index_get(module->index, flags & VC_CONTAINER_SEEK_FLAG_FORWARD, &timestamp, &position, &past);

   if(status == VC_CONTAINER_SUCCESS && !past)
   {
      /* Indexed keyframe found */
      module->frame_read = 0;
      module->mid_frame = 0;
      *offset = timestamp;
      status = SEEK(p_ctx, position);
   }
   else
   {
      /* No indexed keyframe found, so seek through all frames */
      status = rcv_seek_nearest_keyframe(p_ctx, offset, flags & VC_CONTAINER_SEEK_FLAG_FORWARD);
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rcv_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);

   if(module->index)
      vc_container_index_free(module->index);

   free(module);

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T rcv_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   uint8_t dummy[8];

   /* Quick check for a valid file header */
   if((PEEK_BYTES(p_ctx, dummy, sizeof(dummy)) != sizeof(dummy)) ||
      dummy[3] != 0xc5 || LI32(dummy+4) != 0x4)
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
   p_ctx->tracks[0]->format->codec = VC_CONTAINER_CODEC_WMV3;
   p_ctx->tracks[0]->is_enabled = true;

   if((status = rcv_read_header(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;

   LOG_DEBUG(p_ctx, "using rcv reader");

   if(vc_container_index_create(&module->index, 512) == VC_CONTAINER_SUCCESS)
      vc_container_index_add(module->index, 0LL, STREAM_POSITION(p_ctx));

   if(STREAM_SEEKABLE(p_ctx))
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   p_ctx->priv->pf_close = rcv_reader_close;
   p_ctx->priv->pf_read = rcv_reader_read;
   p_ctx->priv->pf_seek = rcv_reader_seek;
   return VC_CONTAINER_SUCCESS;

 error:
   if(module) rcv_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open rcv_reader_open
#endif
