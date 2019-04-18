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

#define CONTAINER_IS_LITTLE_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define CONTAINER_HELPER_LOG_INDENT(a) 0
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_waveformat.h"

/******************************************************************************
Defines.
******************************************************************************/
#define WAV_EXTRADATA_MAX 16
#define BLOCK_SIZE (16*1024)

/******************************************************************************
GUID list for the different codecs
******************************************************************************/
static const GUID_T atracx_guid = {0xbfaa23e9, 0x58cb, 0x7144, {0xa1, 0x19, 0xff, 0xfa, 0x01, 0xe4, 0xce, 0x62}};
static const GUID_T pcm_guid = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   uint64_t data_offset; /**< Offset to the start of the data packets */
   int64_t data_size;    /**< Size of the data contained in the data element */
   uint32_t block_size;   /**< Size of a block of audio data */
   int64_t position;
   uint64_t frame_data_left;

   VC_CONTAINER_TRACK_T *track;
   uint8_t extradata[WAV_EXTRADATA_MAX];

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T wav_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/

static VC_CONTAINER_STATUS_T wav_reader_read( VC_CONTAINER_T *p_ctx,
                                              VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t packet_flags = 0, size, data_size;
   int64_t pts;

   pts = module->position * 8000000 / p_ctx->tracks[0]->format->bitrate;
   data_size = module->frame_data_left;
   if(!data_size)
   {
      data_size = module->block_size;
      packet_flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   }
   module->frame_data_left = 0;

   if(module->position + data_size > module->data_size)
      data_size = module->data_size - module->position;
   if(data_size == 0) return VC_CONTAINER_ERROR_EOS;

   if((flags & VC_CONTAINER_READ_FLAG_SKIP) && !(flags & VC_CONTAINER_READ_FLAG_INFO)) /* Skip packet */
   {
      size = SKIP_BYTES(p_ctx, data_size);
      module->frame_data_left = data_size - size;
      module->position += size;
      return STREAM_STATUS(p_ctx);
   }

   p_packet->flags = packet_flags;
   p_packet->dts = p_packet->pts = pts;
   p_packet->track = 0;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      size = SKIP_BYTES(p_ctx, data_size);
      module->frame_data_left = data_size - size;
      if(!module->frame_data_left) p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
      module->position += size;
      p_packet->size += size;
      return STREAM_STATUS(p_ctx);
   }

   if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   size = MIN(data_size, p_packet->buffer_size - p_packet->size);
   size = READ_BYTES(p_ctx, p_packet->data, size);
   module->frame_data_left = data_size - size;
   if(!module->frame_data_left) p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
   module->position += size;
   p_packet->size += size;

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T wav_reader_seek( VC_CONTAINER_T *p_ctx, int64_t *p_offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   int64_t position;
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);

   position = *p_offset * p_ctx->tracks[0]->format->bitrate / 8000000;
   if(p_ctx->tracks[0]->format->type->audio.block_align)
      position = position / p_ctx->tracks[0]->format->type->audio.block_align *
         p_ctx->tracks[0]->format->type->audio.block_align;
   if(position > module->data_size) position = module->data_size;

   module->position = position;
   module->frame_data_left = 0;

   if(position >= module->data_size) return VC_CONTAINER_ERROR_EOS;
   return SEEK(p_ctx, module->data_offset + position);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T wav_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T wav_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_FOURCC_T codec;
   int64_t chunk_size, chunk_pos;
   uint32_t format, channels, samplerate, bitrate, block_align, bps, cbsize = 0;
   uint8_t buffer[12];

   /* Check the RIFF chunk descriptor */
   if( PEEK_BYTES(p_ctx, buffer, 12) != 12 )
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if( VC_FOURCC(buffer[0], buffer[1], buffer[2], buffer[3]) !=
       VC_FOURCC('R','I','F','F') )
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if( VC_FOURCC(buffer[8], buffer[9], buffer[10], buffer[11]) !=
       VC_FOURCC('W','A','V','E') )
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /*
    *  We are dealing with a WAV file
    */
   SKIP_FOURCC(p_ctx, "Chunk ID");
   SKIP_U32(p_ctx, "Chunk size");
   SKIP_FOURCC(p_ctx, "WAVE ID");

   /* We're looking for the 'fmt' sub-chunk */
   do {
      chunk_pos = STREAM_POSITION(p_ctx) + 8;
      if( READ_FOURCC(p_ctx, "Chunk ID") == VC_FOURCC('f','m','t',' ') ) break;

      /* Not interested in this chunk. Skip it. */
      chunk_size = READ_U32(p_ctx, "Chunk size");
      SKIP_BYTES(p_ctx, chunk_size);
   } while(STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS);

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED; /* 'fmt' not found */

   /* Parse the 'fmt' sub-chunk */
   chunk_size  = READ_U32(p_ctx, "Chunk size");
   format      = READ_U16(p_ctx, "wFormatTag");
   channels    = READ_U16(p_ctx, "nChannels");
   samplerate  = READ_U32(p_ctx, "nSamplesPerSec");
   bitrate     = READ_U32(p_ctx, "nAvgBytesPerSec") * 8;
   block_align = READ_U16(p_ctx, "nBlockAlign");
   bps         = READ_U16(p_ctx, "wBitsPerSample");

   if(STREAM_POSITION(p_ctx) - chunk_pos <= chunk_size - 2)
      cbsize = READ_U16(p_ctx, "cbSize");

   if(format == WAVE_FORMAT_EXTENSIBLE &&
      chunk_size >= 18 + 22 && cbsize >= 22)
   {
      GUID_T guid;
      codec = VC_CONTAINER_CODEC_UNKNOWN;

      SKIP_U16(p_ctx, "wValidBitsPerSample");
      SKIP_U32(p_ctx, "dwChannelMask");
      READ_GUID(p_ctx, &guid, "SubFormat");

      if(!memcmp(&guid, &pcm_guid, sizeof(guid)))
         codec = VC_CONTAINER_CODEC_PCM_SIGNED_LE;
      else if(!memcmp(&guid, &atracx_guid, sizeof(guid)))
         codec = VC_CONTAINER_CODEC_ATRAC3;

      cbsize -= 22;

      /* TODO: deal with channel mapping */
   }
   else
   {
      codec = waveformat_to_codec(format);
   }

   /* Bail out if we don't recognise the codec */
   if(codec == VC_CONTAINER_CODEC_UNKNOWN)
      return VC_CONTAINER_ERROR_FORMAT_FEATURE_NOT_SUPPORTED;

   /* Do some sanity checking on the info we got */
   if(!channels || !samplerate || !block_align || !bitrate)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   if(codec == VC_CONTAINER_CODEC_ATRAC3 && channels > 2)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks_num = 1;
   p_ctx->tracks = &module->track;
   p_ctx->tracks[0] = vc_container_allocate_track(p_ctx, 0);
   if(!p_ctx->tracks[0]) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   p_ctx->tracks[0]->format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   p_ctx->tracks[0]->format->codec = codec;
   p_ctx->tracks[0]->format->type->audio.channels = channels;
   p_ctx->tracks[0]->format->type->audio.sample_rate = samplerate;
   p_ctx->tracks[0]->format->type->audio.block_align = block_align;
   p_ctx->tracks[0]->format->type->audio.bits_per_sample = bps;
   p_ctx->tracks[0]->format->bitrate = bitrate;
   p_ctx->tracks[0]->is_enabled = true;
   p_ctx->tracks[0]->format->extradata_size = 0;
   p_ctx->tracks[0]->format->extradata = module->extradata;
   module->block_size = block_align;

   /* Prepare the codec extradata */
   if(codec == VC_CONTAINER_CODEC_ATRAC3)
   {
      uint16_t h, mode;

      SKIP_U16(p_ctx, "len");
      SKIP_U16(p_ctx, "layer");
      SKIP_U32(p_ctx, "bytes_per_frame");
      mode = READ_U16(p_ctx, "mode");
      SKIP_U16(p_ctx, "mode_ext");
      SKIP_U16(p_ctx, "num_subframes");
      SKIP_U16(p_ctx, "flags");

      h = (1 << 15);
      if(channels == 2)
      {
         h |= (1 << 13);
         if(mode == 1) h |= (1 << 14);
      }
      h |= block_align & 0x7ff;

      p_ctx->tracks[0]->format->extradata[0] = h >> 8;
      p_ctx->tracks[0]->format->extradata[1] = h & 255;
      p_ctx->tracks[0]->format->extradata_size = 2;
   }
   else if(codec == VC_CONTAINER_CODEC_ATRACX && cbsize >= 6)
   {
      SKIP_BYTES(p_ctx, 2);
      p_ctx->tracks[0]->format->extradata_size =
         READ_BYTES(p_ctx, p_ctx->tracks[0]->format->extradata, 6);
   }
   else if(codec == VC_CONTAINER_CODEC_PCM_SIGNED_LE)
   {
      /* Audioplus can no longer be given anything other than a multiple-of-16 number of samples */
      block_align *= 16;
      module->block_size = (BLOCK_SIZE / block_align) * block_align;
   }

   /* Skip the rest of the 'fmt' sub-chunk */
   SKIP_BYTES(p_ctx, chunk_pos + chunk_size - STREAM_POSITION(p_ctx));

   /* We also need the 'data' sub-chunk */
   do {
      chunk_pos = STREAM_POSITION(p_ctx) + 8;
      if( READ_FOURCC(p_ctx, "Chunk ID") == VC_FOURCC('d','a','t','a') ) break;

      /* Not interested in this chunk. Skip it. */
      chunk_size = READ_U32(p_ctx, "Chunk size");
      SKIP_BYTES(p_ctx, chunk_size);
   } while(STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS);

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS)
   {
      status = VC_CONTAINER_ERROR_FORMAT_INVALID; /* 'data' not found */;
      goto error;
   }

   module->data_offset = chunk_pos;
   module->data_size = READ_U32(p_ctx, "Chunk size");
   p_ctx->duration = module->data_size * 8000000 / bitrate;
   if(STREAM_SEEKABLE(p_ctx))
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   /*
    *  We now have all the information we really need to start playing the stream
    */

   p_ctx->priv->pf_close = wav_reader_close;
   p_ctx->priv->pf_read = wav_reader_read;
   p_ctx->priv->pf_seek = wav_reader_seek;

   /* Seek back to the start of the data */
   status = SEEK(p_ctx, module->data_offset);
   if(status != VC_CONTAINER_SUCCESS) goto error;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "wav: error opening stream (%i)", status);
   if(module) wav_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open wav_reader_open
#endif
