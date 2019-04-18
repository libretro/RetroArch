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
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CONTAINER_IS_LITTLE_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define CONTAINER_HELPER_LOG_INDENT(a) 0
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_writer_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T avi_writer_open( VC_CONTAINER_T *p_ctx );

/******************************************************************************
Defines.
******************************************************************************/
#define AVISF_DISABLED        0x00000001 /*< If set stream should not be enabled by default. */
#define AVIF_HASINDEX         0x00000010
#define AVIF_TRUSTCKTYPE      0x00000800
#define AVIIF_KEYFRAME        0x00000010

#define AVI_INDEX_OF_INDEXES  0x00
#define AVI_INDEX_OF_CHUNKS   0x01
#define AVI_INDEX_DELTAFRAME  0x80000000

#define AVI_INDEX_ENTRY_SIZE        16
#define AVI_SUPER_INDEX_ENTRY_SIZE  16
#define AVI_STD_INDEX_ENTRY_SIZE     8
#define AVI_FRAME_BUFFER_SIZE       100000

#define AVI_TRACKS_MAX 3

#define AVI_AUDIO_CHUNK_SIZE_LIMIT 16384 /*< Watermark limit for data chunks when 'dwSampleSize'
                                             is non-zero */

#define AVI_END_CHUNK(ctx)                                            \
   do {                                                               \
      if(STREAM_POSITION(ctx) & 1) WRITE_U8(ctx, 0, "AVI_END_CHUNK"); \
   } while(0)

#define AVI_PACKET_KEYFRAME (VC_CONTAINER_PACKET_FLAG_KEYFRAME | VC_CONTAINER_PACKET_FLAG_FRAME_END)
#define AVI_PACKET_IS_KEYFRAME(flags) (((flags) & AVI_PACKET_KEYFRAME) == AVI_PACKET_KEYFRAME)

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   uint32_t chunk_index;      /**< index of current chunk */
   uint32_t chunk_offs;       /**< current offset into bytestream consisting of all
                                   chunks for this track  */
   uint32_t sample_size;      /**< i.e. 'dwSampleSize' in 'strh' */
   uint32_t max_chunk_size;   /**< largest chunk written so far */
   uint64_t index_offset;     /**< Offset to the start of an OpenDML index for this track
                                   i.e. 'indx' */
   uint32_t index_size;       /**< Size of the OpenDML index for this track i.e. 'indx' */
} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *tracks[AVI_TRACKS_MAX];
   VC_CONTAINER_WRITER_EXTRAIO_T null_io; /**< Null I/O for calculating chunk sizes, etc. */
   VC_CONTAINER_WRITER_EXTRAIO_T temp_io; /**< I/O for temporary storage of index data */
   int headers_written;

   uint32_t header_list_offset;           /**< Offset to the header list chunk ('hdrl') */
   uint32_t header_list_size;             /**< Size of the header list chunk ('hdrl') */
   uint32_t data_offset;                  /**< Offset to the start of data packets i.e.
                                               the data in the AVI RIFF 'movi' list */
   uint64_t data_size;                    /**< Size of the chunk containing data packets */
   uint32_t index_offset;                 /**< Offset to the start of index data e.g.
                                               the data in an 'idx1' list */
   unsigned current_track_num;            /**< Number of track currently being written */
   uint32_t chunk_size;                   /**< Final size of the current chunk being written (if known) */
   uint32_t chunk_data_written;           /**< Data written to the current chunk so far */
   uint8_t *avi_frame_buffer;             /**< For accumulating whole frames when seeking isn't available. */
   VC_CONTAINER_PACKET_T frame_packet;    /**< Packet header for whole frame. */

   VC_CONTAINER_STATUS_T index_status;
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Local Functions
******************************************************************************/
static void avi_chunk_id_from_track_num( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_FOURCC_T *p_chunk_id, unsigned int track_num )
{
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[track_num];
   VC_CONTAINER_FOURCC_T chunk_id = 0;
   char track_num_buf[3];

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
      chunk_id = VC_FOURCC('0','0','d','c');
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      chunk_id = VC_FOURCC('0','0','w','b');
   else
   {
      /* Note that avi_writer_add_track should ensure this
         can't happen */
      *p_chunk_id  = VC_FOURCC('J','U','N','K'); return;
   }

   snprintf(track_num_buf, sizeof(track_num_buf), "%02d", track_num);
   memcpy(&chunk_id, track_num_buf, 2);

   *p_chunk_id = chunk_id;
}

/*****************************************************************************/
static void avi_index_chunk_id_from_track_num(VC_CONTAINER_FOURCC_T *p_chunk_id,
   unsigned int track_num )
{
   VC_CONTAINER_FOURCC_T chunk_id = 0;
   char track_num_buf[3];

   chunk_id = VC_FOURCC('i','x','0','0');

   snprintf(track_num_buf, sizeof(track_num_buf), "%02d", track_num);
   memcpy(((uint8_t*)&chunk_id) + 2, track_num_buf, 2);

   *p_chunk_id = chunk_id;
}

/*****************************************************************************/
static uint32_t avi_num_chunks( VC_CONTAINER_T *p_ctx )
{
   unsigned int i;
   uint32_t num_chunks = 0;
   for (i = 0; i < p_ctx->tracks_num; i++)
      num_chunks += p_ctx->tracks[i]->priv->module->chunk_index;

   return num_chunks;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_finish_data_chunk( VC_CONTAINER_T *p_ctx, uint32_t chunk_size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   if (chunk_size)
   {
      /* Rewrite the chunk size, this won't be efficient if it happens often */
      if (STREAM_SEEKABLE(p_ctx))
      {
         SEEK(p_ctx, STREAM_POSITION(p_ctx) - chunk_size - 4);
         WRITE_U32(p_ctx, chunk_size, "Chunk Size");
         SKIP_BYTES(p_ctx, chunk_size);
      }
      else
      {
         LOG_DEBUG(p_ctx, "warning, can't rewrite chunk size, data will be malformed");
         status = VC_CONTAINER_ERROR_FAILED;
      }
   }

   AVI_END_CHUNK(p_ctx);

   if (status != VC_CONTAINER_SUCCESS) status = STREAM_STATUS(p_ctx);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_index_entry( VC_CONTAINER_T *p_ctx, uint8_t track_num,
   uint32_t chunk_size, int keyframe )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t deltaframe = keyframe ? 0 : AVI_INDEX_DELTAFRAME;

   vc_container_io_write_uint8(module->temp_io.io, track_num);
   vc_container_io_write_be_uint32(module->temp_io.io, chunk_size | deltaframe);

   if (module->temp_io.io->status != VC_CONTAINER_SUCCESS)
   {
      module->index_status = module->temp_io.io->status;
      LOG_DEBUG(p_ctx, "warning, couldn't store index data, index data will be incorrect");
   }

   return module->temp_io.io->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_read_index_entry( VC_CONTAINER_T *p_ctx,
   unsigned int *p_track_num, uint32_t *p_chunk_size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t chunk_size;
   uint8_t track_num;

   track_num = vc_container_io_read_uint8(module->temp_io.io);
   chunk_size = vc_container_io_read_be_uint32(module->temp_io.io);

   /* This shouldn't really happen if the temporary I/O is reliable */
   if (track_num >= p_ctx->tracks_num) return VC_CONTAINER_ERROR_FAILED;

   *p_track_num = track_num;
   *p_chunk_size = chunk_size;

   return module->temp_io.io->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_stream_format_chunk(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *track, uint32_t chunk_size)
{
   VC_CONTAINER_STATUS_T status;

   WRITE_FOURCC(p_ctx, VC_FOURCC('s','t','r','f'), "Chunk ID");
   WRITE_U32(p_ctx, chunk_size, "Chunk Size");

   if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
      status = vc_container_write_bitmapinfoheader(p_ctx, track->format);
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      status = vc_container_write_waveformatex(p_ctx, track->format);

   if (status != VC_CONTAINER_SUCCESS) return status;

   AVI_END_CHUNK(p_ctx);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_stream_header_chunk(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *track)
{
   VC_CONTAINER_FOURCC_T fourcc_type = 0, fourcc_handler = 0;
   uint32_t flags, scale = 0, rate = 0, div, start = 0, sample_size = 0;
   uint16_t left = 0, right = 0, top = 0, bottom = 0;
   uint32_t max_chunk_size, length = 0;

   WRITE_FOURCC(p_ctx, VC_FOURCC('s','t','r','h'), "Chunk ID");
   WRITE_U32(p_ctx, 56, "Chunk Size");

   if (!track->is_enabled)
      flags = 0; /* AVISF_DISABLED; FIXME: write_media should set this correctly! */
   else
      flags = 0;

   if (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      fourcc_type = VC_FOURCC('v','i','d','s');
      sample_size = 0;
      scale = track->format->type->video.frame_rate_den;
      rate = track->format->type->video.frame_rate_num;
      if (rate == 0 || scale == 0)
      {
         LOG_DEBUG(p_ctx, "invalid video framerate (%d/%d)", rate, scale);
         LOG_DEBUG(p_ctx, "using 30/1 (playback timing will almost certainly be incorrect)");
         scale = 1; rate = 30;
      }

      top = track->format->type->video.y_offset;
      left = track->format->type->video.x_offset;
      bottom = track->format->type->video.y_offset + track->format->type->video.visible_height;
      right = track->format->type->video.x_offset + track->format->type->video.visible_width;
   }
   else if (track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
   {
      fourcc_type = VC_FOURCC('a','u','d','s');
      sample_size = track->format->type->audio.block_align;
      scale = 1;

      if (track->format->type->audio.block_align)
         rate = (track->format->bitrate / track->format->type->audio.block_align) >> 3;

      if (rate == 0)
      {
         rate = track->format->type->audio.sample_rate ? track->format->type->audio.sample_rate : 32000;
         LOG_DEBUG(p_ctx, "invalid audio rate, using %d (playback timing will almost certainly be incorrect)",
                   rate);
      }
   }
   else
   {
      /* avi_writer_add_track should ensure this can't happen */
      vc_container_assert(0);
   }

   fourcc_handler = codec_to_vfw_fourcc(track->format->codec);

   div = vc_container_maths_gcd((int64_t)scale, (int64_t)rate);
   scale /= div;
   rate /= div;

   length = sample_size ? track->priv->module->chunk_offs : track->priv->module->chunk_index;
   max_chunk_size = track->priv->module->max_chunk_size;
   track->priv->module->sample_size = sample_size;

   WRITE_FOURCC(p_ctx, fourcc_type, "fccType");
   WRITE_FOURCC(p_ctx, fourcc_handler, "fccHandler");
   WRITE_U32(p_ctx, flags, "dwFlags");
   WRITE_U16(p_ctx, 0, "wPriority");
   WRITE_U16(p_ctx, 0, "wLanguage");
   WRITE_U32(p_ctx, 0, "dwInitialFrames");
   WRITE_U32(p_ctx, scale, "dwScale");
   WRITE_U32(p_ctx, rate, "dwRate");
   WRITE_U32(p_ctx, start, "dwStart");
   WRITE_U32(p_ctx, length, "dwLength");
   WRITE_U32(p_ctx, max_chunk_size, "dwSuggestedBufferSize");
   WRITE_U32(p_ctx, 0, "dwQuality");
   WRITE_U32(p_ctx, sample_size, "dwSampleSize");
   WRITE_U16(p_ctx, left, "rcFrame.left");
   WRITE_U16(p_ctx, top, "rcFrame.top");
   WRITE_U16(p_ctx, right, "rcFrame.right");
   WRITE_U16(p_ctx, bottom, "rcFrame.bottom");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_super_index_chunk(VC_CONTAINER_T *p_ctx, unsigned int index_track_num,
   uint32_t index_size)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[index_track_num]->priv->module;
   VC_CONTAINER_FOURCC_T chunk_id;
   uint32_t num_indices = 1; /* FIXME: support for multiple RIFF chunks (AVIX) */
   unsigned int i;

   if(module->null_io.refcount)
   {
      /* Assume that we're not actually writing the data, just want know the index chunk size */
      WRITE_BYTES(p_ctx, NULL, 8 + 24 + num_indices * (int64_t)AVI_SUPER_INDEX_ENTRY_SIZE);
      return STREAM_STATUS(p_ctx);
   }

   if (track_module->index_offset)
      WRITE_FOURCC(p_ctx, VC_FOURCC('i','n','d','x'), "Chunk ID");
   else
      WRITE_FOURCC(p_ctx, VC_FOURCC('J','U','N','K'), "Chunk ID");

   WRITE_U32(p_ctx, index_size, "Chunk Size");

   avi_chunk_id_from_track_num(p_ctx, &chunk_id, index_track_num);
   WRITE_U16(p_ctx, 4, "wLongsPerEntry");
   WRITE_U8(p_ctx, 0, "bIndexSubType");
   WRITE_U8(p_ctx, AVI_INDEX_OF_INDEXES, "bIndexType");
   WRITE_U32(p_ctx, num_indices, "nEntriesInUse");
   WRITE_FOURCC(p_ctx, chunk_id, "dwChunkId");
   WRITE_U32(p_ctx, 0, "dwReserved0");
   WRITE_U32(p_ctx, 0, "dwReserved1");
   WRITE_U32(p_ctx, 0, "dwReserved2");

   for (i = 0; i < num_indices; ++i)
   {
      uint64_t index_offset = track_module->index_offset;
      uint32_t chunk_size = track_module->index_size;
      uint32_t length = track_module->sample_size ?
         track_module->chunk_offs : track_module->chunk_index;
      WRITE_U64(p_ctx, index_offset, "qwOffset");
      WRITE_U32(p_ctx, chunk_size, "dwSize");
      WRITE_U32(p_ctx, length, "dwDuration");
   }

   AVI_END_CHUNK(p_ctx);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_stream_header_list(VC_CONTAINER_T *p_ctx,
   unsigned int track_num, uint32_t list_size)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[track_num];
   VC_CONTAINER_STATUS_T status;
   uint32_t chunk_size = 0;

   WRITE_FOURCC(p_ctx, VC_FOURCC('L','I','S','T'), "Chunk ID");
   WRITE_U32(p_ctx, list_size, "LIST Size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('s','t','r','l'), "Chunk ID");

   if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

   /* Write the stream header chunk ('strh') */
   status = avi_write_stream_header_chunk(p_ctx, track);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* Write the stream format chunk ('strf') */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null_io))
   {
      status = avi_write_stream_format_chunk(p_ctx, track, 0);
      chunk_size = STREAM_POSITION(p_ctx) - 8;
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null_io);

   status = avi_write_stream_format_chunk(p_ctx, track, chunk_size);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* If the track has DRM data, write it into the 'strd' chunk (we don't write
      write codec configuration data into 'strd') */
   if (track->priv->drmdata && track->priv->drmdata_size)
   {
      WRITE_FOURCC(p_ctx, VC_FOURCC('s','t','r','d'), "Chunk ID");
      WRITE_U32(p_ctx, track->priv->drmdata_size, "Chunk Size");
      WRITE_BYTES(p_ctx, track->priv->drmdata, track->priv->drmdata_size);
      AVI_END_CHUNK(p_ctx);
      if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;
   }

   /* Write the super index chunk ('indx') */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null_io))
   {
      status = avi_write_super_index_chunk(p_ctx, track_num, 0);
      chunk_size = STREAM_POSITION(p_ctx) - 8;
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null_io);

   status = avi_write_super_index_chunk(p_ctx, track_num, chunk_size);
   if (status != VC_CONTAINER_SUCCESS) return status;

   AVI_END_CHUNK(p_ctx);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_avi_header_chunk(VC_CONTAINER_T *p_ctx)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t bitrate = 0, width = 0, height = 0, frame_interval = 0;
   uint32_t flags, num_chunks = 0, max_video_chunk_size = 0;
   uint32_t num_streams = p_ctx->tracks_num;
   unsigned int i;

   for (i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = p_ctx->tracks[i];
      VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[i]->priv->module;
      if (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
      {
         width = track->format->type->video.width;
         height = track->format->type->video.height;
         if (track->format->type->video.frame_rate_num)
            frame_interval = track->format->type->video.frame_rate_den * UINT64_C(1000000) /
                              track->format->type->video.frame_rate_num;
         num_chunks = track_module->chunk_index;
         max_video_chunk_size = track_module->max_chunk_size;
         break;
      }
   }

   flags = (module->index_offset && module->index_status == VC_CONTAINER_SUCCESS) ?
      (AVIF_HASINDEX | AVIF_TRUSTCKTYPE) : 0;

   WRITE_FOURCC(p_ctx, VC_FOURCC('a','v','i','h'), "Chunk ID");
   WRITE_U32(p_ctx, 56, "Chunk Size");
   WRITE_U32(p_ctx, frame_interval, "dwMicroSecPerFrame");
   WRITE_U32(p_ctx, bitrate >> 3, "dwMaxBytesPerSec");
   WRITE_U32(p_ctx, 0, "dwPaddingGranularity");
   WRITE_U32(p_ctx, flags, "dwFlags");
   WRITE_U32(p_ctx, num_chunks, "dwTotalFrames");
   WRITE_U32(p_ctx, 0, "dwInitialFrames");
   WRITE_U32(p_ctx, num_streams, "dwStreams");
   WRITE_U32(p_ctx, max_video_chunk_size, "dwSuggestedBufferSize");
   WRITE_U32(p_ctx, width, "dwWidth");
   WRITE_U32(p_ctx, height, "dwHeight");
   WRITE_U32(p_ctx, 0, "dwReserved0");
   WRITE_U32(p_ctx, 0, "dwReserved1");
   WRITE_U32(p_ctx, 0, "dwReserved2");
   WRITE_U32(p_ctx, 0, "dwReserved3");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_header_list( VC_CONTAINER_T *p_ctx, uint32_t header_list_size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   unsigned int i;

   WRITE_FOURCC(p_ctx, VC_FOURCC('L','I','S','T'), "Chunk ID");
   WRITE_U32(p_ctx, header_list_size, "LIST Size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('h','d','r','l'), "Chunk ID");
   if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

   /* Write the main AVI header chunk ('avih') */
   if ((status = avi_write_avi_header_chunk(p_ctx)) != VC_CONTAINER_SUCCESS)
      return status;

   for (i = 0; i < p_ctx->tracks_num; i++)
   {
      uint32_t list_size = 0;

      /* Write a stream header list chunk ('strl') */
      if(!vc_container_writer_extraio_enable(p_ctx, &module->null_io))
      {
         status = avi_write_stream_header_list(p_ctx, i, 0);
         if (status != VC_CONTAINER_SUCCESS) return status;
         list_size = STREAM_POSITION(p_ctx) - 8;
      }
      vc_container_writer_extraio_disable(p_ctx, &module->null_io);

      status = avi_write_stream_header_list(p_ctx, i, list_size);
      if (status != VC_CONTAINER_SUCCESS) return status;
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_headers( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint32_t header_list_offset, header_list_size = 0;

   /* Write the header list chunk ('hdrl') */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null_io))
   {
      status = avi_write_header_list(p_ctx, 0);
      if (status != VC_CONTAINER_SUCCESS) return status;
      header_list_size = STREAM_POSITION(p_ctx) - 8;
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null_io);

   header_list_offset = STREAM_POSITION(p_ctx);
   status = avi_write_header_list(p_ctx, header_list_size);
   if (status == VC_CONTAINER_SUCCESS && !module->header_list_offset)
   {
      module->header_list_offset = header_list_offset;
      module->header_list_size = header_list_size;
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_legacy_index_chunk( VC_CONTAINER_T *p_ctx, uint32_t index_size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint32_t chunk_offset = 4;
   unsigned int track_num;

   vc_container_assert(8 + avi_num_chunks(p_ctx) * INT64_C(16) <= (int64_t)ULONG_MAX);

   if(module->null_io.refcount)
   {
      /* Assume that we're not actually writing the data,
         just want know the index size */
      WRITE_BYTES(p_ctx, NULL, 8 + avi_num_chunks(p_ctx) * (int64_t)AVI_INDEX_ENTRY_SIZE);
      return STREAM_STATUS(p_ctx);
   }

   module->index_offset = STREAM_POSITION(p_ctx);

   WRITE_FOURCC(p_ctx, VC_FOURCC('i','d','x','1'), "Chunk ID");
   WRITE_U32(p_ctx, index_size, "Chunk Size");

   /* Scan through all written entries, convert to appropriate index format */
   vc_container_io_seek(module->temp_io.io, INT64_C(0));

   while((status = STREAM_STATUS(p_ctx)) == VC_CONTAINER_SUCCESS)
   {
      VC_CONTAINER_FOURCC_T chunk_id;
      uint32_t chunk_size, flags;

      status = avi_read_index_entry(p_ctx, &track_num, &chunk_size);
      if (status != VC_CONTAINER_SUCCESS) break;

      avi_chunk_id_from_track_num(p_ctx, &chunk_id, track_num);
      flags = (chunk_size & AVI_INDEX_DELTAFRAME) ? 0 : AVIIF_KEYFRAME;
      chunk_size &= ~AVI_INDEX_DELTAFRAME;

      WRITE_FOURCC(p_ctx, chunk_id, "Chunk ID");
      WRITE_U32(p_ctx, flags, "dwFlags");
      WRITE_U32(p_ctx, chunk_offset, "dwOffset");
      WRITE_U32(p_ctx, chunk_size, "dwSize");

      chunk_offset += ((chunk_size + 1) & ~1) + 8;
   }

   AVI_END_CHUNK(p_ctx);

   /* Note that currently, we might write a partial index but still set AVIF_HASINDEX */
   /* if ( STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS ) module->index_offset = 0 */

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_standard_index_chunk( VC_CONTAINER_T *p_ctx, unsigned int index_track_num,
   uint32_t index_size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[index_track_num]->priv->module;
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_FOURCC_T chunk_id;
   int64_t base_offset = module->data_offset + 12;
   uint32_t num_chunks = track_module->chunk_index;
   uint32_t chunk_offset = 4;

   vc_container_assert(32 + num_chunks * (int64_t)AVI_STD_INDEX_ENTRY_SIZE <= (int64_t)ULONG_MAX);

   if(module->null_io.refcount)
   {
      /* Assume that we're not actually writing the data, just want know the index chunk size */
      WRITE_BYTES(p_ctx, NULL, 8 + 24 + num_chunks * INT64_C(8));
      return STREAM_STATUS(p_ctx);
   }

   track_module->index_offset = STREAM_POSITION(p_ctx);
   track_module->index_size = index_size ? (index_size - 8) : 0;

   avi_index_chunk_id_from_track_num(&chunk_id, index_track_num);
   WRITE_FOURCC(p_ctx, chunk_id, "Chunk ID");
   WRITE_U32(p_ctx, index_size, "Chunk Size");

   avi_chunk_id_from_track_num(p_ctx, &chunk_id, index_track_num);
   WRITE_U16(p_ctx, 2, "wLongsPerEntry");
   WRITE_U8(p_ctx, 0, "bIndexSubType");
   WRITE_U8(p_ctx, AVI_INDEX_OF_CHUNKS, "bIndexType");
   WRITE_U32(p_ctx, num_chunks, "nEntriesInUse");
   WRITE_FOURCC(p_ctx, chunk_id, "dwChunkId");
   WRITE_U64(p_ctx, base_offset, "qwBaseOffset");
   WRITE_U32(p_ctx, 0, "dwReserved");

   /* Scan through all written entries, convert to appropriate index format */
   vc_container_io_seek(module->temp_io.io, INT64_C(0));

   while(STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS)
   {
      uint32_t chunk_size;
      unsigned int track_num;

      status = avi_read_index_entry(p_ctx, &track_num, &chunk_size);
      if (status != VC_CONTAINER_SUCCESS) break;

      if(track_num != index_track_num) continue;

      WRITE_U32(p_ctx, chunk_offset, "dwOffset");
      WRITE_U32(p_ctx, chunk_size, "dwSize");

      chunk_offset += ((chunk_size + 1) & ~(1 | AVI_INDEX_DELTAFRAME)) + 12;
   }

   AVI_END_CHUNK(p_ctx);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_legacy_index_data( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t chunk_size = 0;

   /* Write the legacy index chunk ('idx1') */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null_io))
   {
      status = avi_write_legacy_index_chunk(p_ctx, 0);
      if (status != VC_CONTAINER_SUCCESS) return status;
      chunk_size = STREAM_POSITION(p_ctx) - 8;
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null_io);

   status = avi_write_legacy_index_chunk(p_ctx, chunk_size);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_write_standard_index_data( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t chunk_size = 0;
   unsigned int i;

   /* Write the standard index chunks ('ix00') */
   for (i = 0; i < p_ctx->tracks_num; i++)
   {
      if(!vc_container_writer_extraio_enable(p_ctx, &module->null_io))
      {
         status = avi_write_standard_index_chunk(p_ctx, i, 0);
         if (status != VC_CONTAINER_SUCCESS) return status;
         chunk_size = STREAM_POSITION(p_ctx) - 8;
      }
      vc_container_writer_extraio_disable(p_ctx, &module->null_io);

      status = avi_write_standard_index_chunk(p_ctx, i, chunk_size);
      if (status != VC_CONTAINER_SUCCESS) return status;
   }

   return status;
}

/*****************************************************************************/
static int64_t avi_calculate_file_size( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *p_packet )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t filesize = 0;
   int refcount;

   /* Start from current file position */
   filesize = STREAM_POSITION(p_ctx);

   refcount = vc_container_writer_extraio_enable(p_ctx, &module->null_io);
   vc_container_assert(refcount == 0); /* Although perfectly harmless, we should
                                          not be called with the null i/o enabled. */
   VC_CONTAINER_PARAM_UNUSED(refcount);

   do {
      /* If we know what the final size of the chunk is going to be,
         we can use that here to avoid writing a partial final packet */
      WRITE_BYTES(p_ctx, NULL, p_packet->frame_size ? p_packet->frame_size : p_packet->size);
      AVI_END_CHUNK(p_ctx);

      /* Index entries for the chunk */
      WRITE_BYTES(p_ctx, NULL, AVI_INDEX_ENTRY_SIZE + AVI_STD_INDEX_ENTRY_SIZE);

      /* Current standard index data */
      if (avi_write_standard_index_data(p_ctx) != VC_CONTAINER_SUCCESS) break;

      /* Current legacy index data */
      status = avi_write_legacy_index_data(p_ctx);
      if (status != VC_CONTAINER_SUCCESS) break;
   } while(0);

   filesize += STREAM_POSITION(p_ctx);

   vc_container_writer_extraio_disable(p_ctx, &module->null_io);

   return filesize;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/

static VC_CONTAINER_STATUS_T avi_writer_write( VC_CONTAINER_T *p_ctx,
                                               VC_CONTAINER_PACKET_T *p_packet )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_TRACK_T *track = NULL;
   VC_CONTAINER_TRACK_MODULE_T *track_module = NULL;

   /* Check we have written headers before any data */
   if(!module->headers_written)
   {
      if ((status = avi_write_headers(p_ctx)) != VC_CONTAINER_SUCCESS) return status;
      module->headers_written = 1;
   }

   /* Check that we have started the 'movi' list */
   if (!module->data_offset)
   {
      module->data_offset = STREAM_POSITION(p_ctx);
      vc_container_assert(module->data_offset != INT64_C(0));

      WRITE_FOURCC(p_ctx, VC_FOURCC('L','I','S','T'), "Chunk ID");
      WRITE_U32(p_ctx, 0, "LIST Size");
      WRITE_FOURCC(p_ctx, VC_FOURCC('m','o','v','i'), "Chunk ID");
      if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;
   }

   /* If the container is passing in a frame from a new track but we
      arent't finished with a chunk from another track we need to finish
      that chunk first */
   if (module->chunk_data_written && p_packet->track != module->current_track_num)
   {
      track_module = p_ctx->tracks[module->current_track_num]->priv->module;
      status = avi_finish_data_chunk(p_ctx, module->chunk_data_written);
      avi_write_index_entry(p_ctx, module->current_track_num, module->chunk_data_written, 0);
      track_module->chunk_index++;
      track_module->chunk_offs += module->chunk_data_written;
      track_module->max_chunk_size = MAX(track_module->max_chunk_size, module->chunk_data_written);
      module->chunk_data_written = 0;
      if (status != VC_CONTAINER_SUCCESS) return status;
   }

   /* Check we are not about to go over the limit of total number of chunks */
   if (avi_num_chunks(p_ctx) == (uint32_t)ULONG_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

    if(STREAM_SEEKABLE(p_ctx))
    {
       /* Check we are not about to go over the maximum file size */
       if (avi_calculate_file_size(p_ctx, p_packet) >= (int64_t)ULONG_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
    }

   /* FIXME: are we expected to handle this case or should it be picked up by the above layer? */
   vc_container_assert(!(module->chunk_data_written && (p_packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_START)));

   track = p_ctx->tracks[p_packet->track];
   track_module = p_ctx->tracks[p_packet->track]->priv->module;
   module->current_track_num = p_packet->track;

   if (module->chunk_data_written == 0)
   {
      /* This is the first fragment of the chunk */
      VC_CONTAINER_FOURCC_T chunk_id;
      uint32_t chunk_size;

      avi_chunk_id_from_track_num(p_ctx, &chunk_id, p_packet->track);

      if (p_packet->frame_size)
      {
         /* We know what the final size of the chunk is going to be */
         chunk_size = module->chunk_size = p_packet->frame_size;
      }
      else
      {
         chunk_size = p_packet->size;
         module->chunk_size = 0;
      }

      WRITE_FOURCC(p_ctx, chunk_id, "Chunk ID");
      if(STREAM_SEEKABLE(p_ctx) || p_packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_END)
      {
         /* If the output stream can seek we can fix up the frame size later, and if the
          * packet holds the whole frame we won't need to, so write data straight out. */
         WRITE_U32(p_ctx, chunk_size, "Chunk Size");
         WRITE_BYTES(p_ctx, p_packet->data, p_packet->size);
      }
      else
      {
         vc_container_assert(module->avi_frame_buffer);
         if(p_packet->size > AVI_FRAME_BUFFER_SIZE)
            return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
         module->frame_packet = *p_packet;
         module->frame_packet.data = module->avi_frame_buffer;
         memcpy(module->frame_packet.data,
                  p_packet->data, module->frame_packet.size);
      }

      module->chunk_data_written = p_packet->size;
   }
   else
   {
      if(module->frame_packet.size > 0 && module->avi_frame_buffer)
      {
         if(module->frame_packet.size > 0)
         {
            if(module->frame_packet.size + p_packet->size > AVI_FRAME_BUFFER_SIZE)
               return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
            memcpy(module->frame_packet.data + module->frame_packet.size,
                     p_packet->data, p_packet->size);
            module->frame_packet.size += p_packet->size;
         }
      }
      else
      {
         WRITE_BYTES(p_ctx, p_packet->data, p_packet->size);
      }
      module->chunk_data_written += p_packet->size;
   }

   if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS)
      return status;

   if ((p_packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_END) ||
       (track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO &&
        track->format->type->audio.block_align &&
        module->chunk_data_written > AVI_AUDIO_CHUNK_SIZE_LIMIT))
   {
      if(module->frame_packet.size > 0)
      {
         WRITE_U32(p_ctx, module->frame_packet.size, "Chunk Size");
         WRITE_BYTES(p_ctx, module->frame_packet.data, module->frame_packet.size);
         p_packet->size = module->frame_packet.size;
         module->frame_packet.size = 0;
      }

      if (!module->chunk_size && module->chunk_data_written > p_packet->size)
      {
         /* The chunk size needs to be rewritten */
         status = avi_finish_data_chunk(p_ctx, module->chunk_data_written);
      }
      else
      {
         status = avi_finish_data_chunk(p_ctx, 0);
      }

      if(!STREAM_SEEKABLE(p_ctx))
      {
         /* If we are streaming then flush to avoid delaying data transport. */
         vc_container_control(p_ctx, VC_CONTAINER_CONTROL_IO_FLUSH);
      }

      if(STREAM_SEEKABLE(p_ctx))
      {
          /* Keep track of data written so we can check we don't exceed file size and also for doing
           * index fix-ups, but only do this if we are writing to a seekable IO. */
          avi_write_index_entry(p_ctx, p_packet->track, module->chunk_data_written, AVI_PACKET_IS_KEYFRAME(p_packet->flags));
      }
      track_module->chunk_index++;
      track_module->chunk_offs += module->chunk_data_written;
      track_module->max_chunk_size = MAX(track_module->max_chunk_size, module->chunk_data_written);
      module->chunk_data_written = 0;

      if (status != VC_CONTAINER_SUCCESS) return status;
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_writer_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

   /* If we arent't finished with a chunk we need to finish it first */
   if (module->chunk_data_written)
   {
      VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track_num]->priv->module;
      status = avi_finish_data_chunk(p_ctx, module->chunk_data_written);
      if (status != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "warning, writing failed, last chunk truncated");
      }
      avi_write_index_entry(p_ctx, module->current_track_num, module->chunk_data_written, 0);
      track_module->chunk_index++;
      track_module->chunk_offs += module->chunk_data_written;
      track_module->max_chunk_size = MAX(track_module->max_chunk_size, module->chunk_data_written);
      module->chunk_data_written = 0;
   }

   if(STREAM_SEEKABLE(p_ctx))
   {
      uint32_t filesize;

      /* Write standard index data before finalising the size of the 'movi' list */
      status = avi_write_standard_index_data(p_ctx);
      if (status != VC_CONTAINER_SUCCESS)
      {
         module->index_status = status;
         LOG_DEBUG(p_ctx, "warning, writing standard index data failed, file will be malformed");
      }

      /* FIXME: support for multiple RIFF chunks (AVIX) */
      module->data_size = STREAM_POSITION(p_ctx) - module->data_offset - 8;

      /* Now write the legacy index */
      status = avi_write_legacy_index_data(p_ctx);
      if (status != VC_CONTAINER_SUCCESS)
      {
         module->index_status = status;
         LOG_DEBUG(p_ctx, "warning, writing legacy index data failed, file will be malformed");
      }

      /* If we can, do the necessary fixups for values not know at the
       time of writing chunk headers */

      /* Rewrite the AVI RIFF chunk size */
      filesize = (uint32_t)STREAM_POSITION(p_ctx);
      SEEK(p_ctx, 4);
      WRITE_U32(p_ctx, filesize, "fileSize");
      if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "warning, rewriting 'fileSize' failed, file will be malformed");
      }

      /* Rewrite the header list chunk ('hdrl') */
      SEEK(p_ctx, module->header_list_offset);
      status = avi_write_header_list(p_ctx, module->header_list_size);
      if (status != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "warning, rewriting 'hdrl' failed, file will be malformed");
      }

      /* Rewrite the AVI RIFF 'movi' list size */
      SEEK(p_ctx, module->data_offset + 4);
      WRITE_U32(p_ctx, module->data_size, "Chunk Size");
      if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "warning, rewriting 'movi' list size failed, file will be malformed");
      }
   }

   vc_container_writer_extraio_delete(p_ctx, &module->null_io);
   if(module->temp_io.io) vc_container_writer_extraio_delete(p_ctx, &module->temp_io);

   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   p_ctx->tracks_num = 0;
   p_ctx->tracks = NULL;

   if(module->avi_frame_buffer) free(module->avi_frame_buffer);
   free(module);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_writer_add_track( VC_CONTAINER_T *p_ctx, VC_CONTAINER_ES_FORMAT_T *format )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_TRACK_T *track = NULL;

   if (module->headers_written) return VC_CONTAINER_ERROR_FAILED;

   /* FIXME: should we check the format in more detail? */
   if((format->es_type != VC_CONTAINER_ES_TYPE_VIDEO && format->es_type != VC_CONTAINER_ES_TYPE_AUDIO) ||
      format->codec == VC_CONTAINER_CODEC_UNKNOWN)
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   if(!(format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   /* Allocate new track */
   if(p_ctx->tracks_num >= AVI_TRACKS_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   p_ctx->tracks[p_ctx->tracks_num] = track =
      vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
   if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   if(format->extradata_size)
   {
      status = vc_container_track_allocate_extradata( p_ctx, track, format->extradata_size );
      if(status) goto error;
   }

   status = vc_container_format_copy(track->format, format, format->extradata_size);
   if(status) goto error;

   p_ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;

error:
   vc_container_free_track(p_ctx, track);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_writer_control( VC_CONTAINER_T *p_ctx, VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;

   switch(operation)
   {
      case VC_CONTAINER_CONTROL_TRACK_ADD:
      {
         VC_CONTAINER_ES_FORMAT_T *format =
            (VC_CONTAINER_ES_FORMAT_T *)va_arg( args, VC_CONTAINER_ES_FORMAT_T * );
         return avi_writer_add_track(p_ctx, format);
      }
      case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      {
         if(!module->headers_written)
         {
            if ((status = avi_write_headers(p_ctx)) != VC_CONTAINER_SUCCESS) return status;
            module->headers_written = 1;
            return VC_CONTAINER_SUCCESS;
         }
         else
            return VC_CONTAINER_ERROR_FAILED;
      }
      default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/******************************************************************************
Global function definitions.
******************************************************************************/
VC_CONTAINER_STATUS_T avi_writer_open( VC_CONTAINER_T *p_ctx )
{
   const char *extension = vc_uri_path_extension(p_ctx->priv->uri);
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = 0;

   /* Check if the user has specified a container */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "avi") && strcasecmp(extension, "divx"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;

   /* Create a null i/o writer to help us out in writing our data */
   status = vc_container_writer_extraio_create_null(p_ctx, &module->null_io);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   if(STREAM_SEEKABLE(p_ctx))
   {
       /* Create a temporary i/o writer for storage of index data while we are writing */
       status = vc_container_writer_extraio_create_temp(p_ctx, &module->temp_io);
       if(status != VC_CONTAINER_SUCCESS) goto error;
   }
   else
   {
      module->avi_frame_buffer = malloc(AVI_FRAME_BUFFER_SIZE);
      if(!module->avi_frame_buffer)
         { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   }
   module->frame_packet.size = 0;

   p_ctx->tracks = module->tracks;

   /* Write the RIFF chunk descriptor */
   WRITE_FOURCC(p_ctx, VC_FOURCC('R','I','F','F'), "RIFF ID");
   WRITE_U32(p_ctx, 0, "fileSize");
   WRITE_FOURCC(p_ctx, VC_FOURCC('A','V','I',' '), "fileType");

   if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;

   p_ctx->priv->pf_close = avi_writer_close;
   p_ctx->priv->pf_write = avi_writer_write;
   p_ctx->priv->pf_control = avi_writer_control;

   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "error opening stream");
   p_ctx->tracks_num = 0;
   p_ctx->tracks = NULL;
   if(module)
   {
      if(module->avi_frame_buffer) free(module->avi_frame_buffer);
      free(module);
   }
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/
#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open avi_writer_open
#endif
