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
#define AVISF_DISABLED        0x00000001 /*< If set stream should not be enabled by default. */
#define AVIF_MUSTUSEINDEX     0x00000020
#define AVIF_TRUSTCKTYPE      0x00000800 /*< (OpenDML) keyframe information reliable. */

#define AVIIF_LIST            0x00000001
#define AVIIF_KEYFRAME        0x00000010
#define AVIIF_NOTIME          0x00000100

#define AVI_INDEX_OF_INDEXES  0x00
#define AVI_INDEX_OF_CHUNKS   0x01
#define AVI_INDEX_2FIELD      0x01
#define AVI_INDEX_DELTAFRAME  0x80000000
 
#define AVI_TRACKS_MAX 16 /*< We won't try to handle streams with more tracks than this */

#define AVI_TWOCC(a,b) ((a) | (b << 8))

#define AVI_SYNC_CHUNK(ctx)                               \
      while(STREAM_POSITION(ctx) & 1)                     \
      {                                                   \
         if (SKIP_BYTES(ctx, 1) != 1) break;              \
      }

#define AVI_SKIP_CHUNK(ctx, size)                         \
   do {                                                   \
      SKIP_BYTES(ctx, size);                              \
      AVI_SYNC_CHUNK(ctx);                                \
   } while(0)
      
/******************************************************************************
Type definitions
******************************************************************************/
typedef struct AVI_TRACK_STREAM_STATE_T
{
   unsigned current_track_num;     /**< Number of track currently being read */
   int64_t data_offset;            /**< Offset within the stream to find the track data */
   uint32_t chunk_size;            /**< Size of the current chunk being read */
   uint32_t chunk_data_left;       /**< Data left from the current chunk being read */

   unsigned extra_chunk_track_num; /**< Temporary storage for in-band data e.g. 'dd'
                                        chunks */
   uint32_t extra_chunk_data[4];
   uint32_t extra_chunk_data_offs;
   uint32_t extra_chunk_data_len;
} AVI_TRACK_STREAM_STATE_T;

typedef struct AVI_TRACK_CHUNK_STATE_T
{
   uint64_t index;
   uint64_t offs;        /**< offset into bytestream consisting of all chunks for this track  */
   int64_t  time_pos;    /**< pts of chunk (if known) */
   uint32_t flags;       /**< flags associated with chunk */
   AVI_TRACK_STREAM_STATE_T local_state;
   AVI_TRACK_STREAM_STATE_T *state;
} AVI_TRACK_CHUNK_STATE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   int64_t  time_start;    /**< i.e. 'dwStart' in 'strh' (converted to microseconds) */
   int64_t  duration;      /**< i.e. 'dwLength' in 'strh' (converted to microseconds) */
   uint32_t time_num;      /**< i.e. 'dwScale' in 'strh' */
   uint32_t time_den;      /**< i.e. 'dwRate' in 'strh', time_num / time_den = 
                                samples (or frames) / second for audio (or video) */
   uint32_t sample_size;   /**< i.e. 'dwSampleSize' in 'strh' */

   uint64_t index_offset;  /**< Offset to the start of an OpenDML index i.e. 'indx' 
                                (if available) */
   uint32_t index_size;    /**< Size of the OpenDML index chunk */
   AVI_TRACK_CHUNK_STATE_T chunk;
} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{ 
   VC_CONTAINER_TRACK_T *tracks[AVI_TRACKS_MAX];
   uint64_t data_offset;           /**< Offset to the start of data packets i.e. 
                                        the data in the 'movi' list */
   uint64_t data_size;             /**< Size of the chunk containing data packets */
   uint64_t index_offset;          /**< Offset to the start of index data e.g. 
                                        the data in a 'idx1' list */
   uint32_t index_size;            /**< Size of the chunk containing index data */
   AVI_TRACK_STREAM_STATE_T state;
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T avi_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

static VC_CONTAINER_STATUS_T avi_find_chunk(VC_CONTAINER_T *p_ctx, VC_CONTAINER_FOURCC_T id, uint32_t *size)
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_FOURCC_T chunk_id;
   uint32_t chunk_size;

   do {
      chunk_id = READ_FOURCC(p_ctx, "Chunk ID");
      chunk_size = READ_U32(p_ctx, "Chunk size");
      if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

      if(chunk_id == id) 
      {
         *size = chunk_size;
         return VC_CONTAINER_SUCCESS;
      }
      /* Not interested in this chunk, skip it. */
      AVI_SKIP_CHUNK(p_ctx, chunk_size);
   } while((status = STREAM_STATUS(p_ctx)) == VC_CONTAINER_SUCCESS);

   return status; /* chunk not found */
}

static VC_CONTAINER_STATUS_T avi_find_list(VC_CONTAINER_T *p_ctx, VC_CONTAINER_FOURCC_T fourcc, uint32_t *size)
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_FOURCC_T chunk_id;
   uint32_t chunk_size;
   uint32_t peek_buf[1];
   
   do {
      chunk_id = READ_FOURCC(p_ctx, "Chunk ID");
      chunk_size = READ_U32(p_ctx, "Chunk size");
      if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

      if(chunk_id == VC_FOURCC('L','I','S','T')) 
      {
         if (PEEK_BYTES(p_ctx, (uint8_t*)peek_buf, 4) != 4)
            return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
         if (peek_buf[0] == fourcc)
         {
            *size = chunk_size;
            return VC_CONTAINER_SUCCESS;
         }
      }
      /* Not interested in this chunk, skip it. */
      AVI_SKIP_CHUNK(p_ctx, chunk_size);
   } while((status = STREAM_STATUS(p_ctx)) == VC_CONTAINER_SUCCESS);

   return status; /* list not found */
}

static int64_t avi_stream_ticks_to_us(VC_CONTAINER_TRACK_MODULE_T *track_module, uint64_t ticks)
{
   int64_t time;
   vc_container_assert(track_module->time_den != 0);
   time = INT64_C(1000000) * track_module->time_num * ticks / track_module->time_den;
   return time;
}

static int64_t avi_calculate_chunk_time(VC_CONTAINER_TRACK_MODULE_T *track_module)
{
   if (track_module->sample_size == 0)
      return track_module->time_start + avi_stream_ticks_to_us(track_module, track_module->chunk.index);
   else
      return track_module->time_start + avi_stream_ticks_to_us(track_module, 
         ((track_module->chunk.offs + (track_module->sample_size >> 1)) / track_module->sample_size));
}

static VC_CONTAINER_STATUS_T avi_read_stream_header_list(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track,
                                                         VC_CONTAINER_TRACK_MODULE_T *track_module)
{
   VC_CONTAINER_STATUS_T status;
   int64_t list_offset;
   uint32_t list_size, chunk_id, chunk_size;

   int stream_header_chunk_read = 0, stream_format_chunk_read = 0;
   
   /* Look for a 'strl' LIST (sub)chunk */   
   status = avi_find_list(p_ctx, VC_FOURCC('s','t','r','l'), &chunk_size);
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_DEBUG(p_ctx, "'strl' LIST not found for stream");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   list_offset = STREAM_POSITION(p_ctx);
   list_size = chunk_size;
   SKIP_FOURCC(p_ctx, "strl");

   while((status = STREAM_STATUS(p_ctx)) == VC_CONTAINER_SUCCESS && STREAM_POSITION(p_ctx) < list_offset + list_size)
   {
      int64_t offset = STREAM_POSITION(p_ctx);
      chunk_id = READ_FOURCC(p_ctx, "Chunk ID");
      chunk_size = READ_U32(p_ctx, "Chunk size");
      LOG_FORMAT(p_ctx, "chunk %4.4s, offset: %"PRIi64", size: %i", (char *)&chunk_id, offset, chunk_size);

      if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) return status;

      if(chunk_id == VC_FOURCC('s','t','r','h'))
      {
         VC_CONTAINER_FOURCC_T fourcc_type, fourcc_handler;
         uint32_t flags, scale, rate, div, start, length, sample_size;
         
         /* We won't accept more than one 'strh' per stream */
         if (stream_header_chunk_read)
         {
            LOG_DEBUG(p_ctx, "rejecting invalid 'strl', found more than one 'strh'");
            return VC_CONTAINER_ERROR_FORMAT_INVALID;
         }
         
         fourcc_type = READ_FOURCC(p_ctx, "fccType");
         fourcc_handler = READ_FOURCC(p_ctx, "fccHandler");
         flags = READ_U32(p_ctx, "dwFlags");
         SKIP_U16(p_ctx, "wPriority");
         SKIP_U16(p_ctx, "wLanguage");
         SKIP_U32(p_ctx, "dwInitialFrames");
         scale = READ_U32(p_ctx, "dwScale");
         rate = READ_U32(p_ctx, "dwRate");
         start = READ_U32(p_ctx, "dwStart");
         length = READ_U32(p_ctx, "dwLength");
         SKIP_U32(p_ctx, "dwSuggestedBufferSize");
         SKIP_U32(p_ctx, "dwQuality");
         sample_size = READ_U32(p_ctx, "dwSampleSize");
         SKIP_U16(p_ctx, "rcFrame.left");
         SKIP_U16(p_ctx, "rcFrame.top");
         SKIP_U16(p_ctx, "rcFrame.right");
         SKIP_U16(p_ctx, "rcFrame.bottom");

         /* In AVI, sec/frame = scale/rate and frames/sec = rate/scale */
         if (rate == 0)
         {
            LOG_DEBUG(p_ctx, "invalid dwRate: 0, using 1 as a guess");
            LOG_DEBUG(p_ctx, "timestamps will almost certainly be wrong");
            rate = 1;
         }

         div = vc_container_maths_gcd((int64_t)scale, (int64_t)rate);
         scale = scale / div; 
         rate = rate / div;

         track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         if(fourcc_type == VC_FOURCC('v','i','d','s'))
         {
            track->format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
            track->format->type->video.frame_rate_num = rate;
            track->format->type->video.frame_rate_den = scale;
            
            if (sample_size != 0)
            {
               LOG_DEBUG(p_ctx, "ignoring dwSampleSize (%d) for video stream", sample_size);
               sample_size = 0;
            }
         }
         else if(fourcc_type == VC_FOURCC('a','u','d','s'))
         {
            track->format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
            /* VBR audio is going to be non-framed */
            track->format->flags &= ~VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         }
         else if(fourcc_type == VC_FOURCC('t','x','t','s'))
            track->format->es_type = VC_CONTAINER_ES_TYPE_SUBPICTURE;
   
         /* Don't overwrite any existing value (i.e. in the unlikely case where we 
            see 'strf' before 'strh') */
         if(!track->format->codec) track->format->codec = vfw_fourcc_to_codec(fourcc_handler);
   
         /* FIXME: enable this once read_media does the right thing */
         if (!(flags & AVISF_DISABLED) || 1)
            track->is_enabled = 1;
            
         track_module->time_num = scale;
         track_module->time_den = rate;
         track_module->time_start = avi_stream_ticks_to_us(track_module, (uint64_t)start);
         track_module->duration = avi_stream_ticks_to_us(track_module, (uint64_t)length);
         track_module->sample_size = sample_size;
         
         p_ctx->duration = MAX(p_ctx->duration, track_module->duration);
         
         stream_header_chunk_read = 1;
      }
      else if(chunk_id == VC_FOURCC('s','t','r','f')) 
      {
         uint8_t *buffer;
         unsigned extra_offset = 0, extra_size = 0;

         /* We won't accept more than one 'strf' per stream */
         if (stream_format_chunk_read) 
         {
            LOG_DEBUG(p_ctx, "rejecting invalid 'strl', found more than one 'strf'");
            return VC_CONTAINER_ERROR_FORMAT_INVALID;
         }

         /* Use the extradata buffer for reading in the entire 'strf' (should not be a large chunk) */
         if ((status = vc_container_track_allocate_extradata(p_ctx, track, chunk_size)) != VC_CONTAINER_SUCCESS)
         {
            LOG_DEBUG(p_ctx, "failed to allocate memory for 'strf' (%d bytes)", chunk_size);
            return status;
         }
         
         buffer = track->priv->extradata;
         if(READ_BYTES(p_ctx, buffer, chunk_size) != chunk_size) 
            return VC_CONTAINER_ERROR_FORMAT_INVALID;
         AVI_SYNC_CHUNK(p_ctx);
         
         if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
         {
            status = vc_container_bitmapinfoheader_to_es_format(buffer, chunk_size, &extra_offset, &extra_size, track->format);
         }
         else if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
         {
            status = vc_container_waveformatex_to_es_format(buffer, chunk_size, &extra_offset, &extra_size, track->format);
            if (track_module->sample_size != 0 && track_module->sample_size != track->format->type->audio.block_align)
            {
               LOG_DEBUG(p_ctx, "invalid dwSampleSize (%d), should match nBlockAlign (%d) for audio streams.", 
                  track_module->sample_size, track->format->type->audio.block_align);
   
               /* Note that if nBlockAlign really is 0, strf is seriously broken... */
               if (track->format->type->audio.block_align != 0)
                  track_module->sample_size = track->format->type->audio.block_align;
            }
            else
            {
               /* Flawed muxers might only set nBlockAlign (i.e. not set dwSampleSize correctly). */
               if (track->format->type->audio.block_align == 1) 
                  track_module->sample_size = 1;
            }
         }

         if (status != VC_CONTAINER_SUCCESS) return status;
   
         if (extra_size)
         {
            track->format->extradata = buffer + extra_offset;
            track->format->extradata_size = extra_size;
         }

         /* Codec specific fix-up */
         if (track->format->codec == VC_CONTAINER_CODEC_MP4A &&
             track->format->extradata_size)
         {
            /* This is going to be raw AAC so it will be framed */
            track_module->sample_size = 0;
            track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         }

         /* WMA specific fix-up */
         if ((track->format->codec == VC_CONTAINER_CODEC_WMA1 ||
              track->format->codec == VC_CONTAINER_CODEC_WMA2 ||
              track->format->codec == VC_CONTAINER_CODEC_WMAP ||
              track->format->codec == VC_CONTAINER_CODEC_WMAL ||
              track->format->codec == VC_CONTAINER_CODEC_WMAV) &&
              track->format->extradata_size)
         {
            track_module->sample_size = 0;
            track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         }

         stream_format_chunk_read = 1;
      }
      else if(chunk_id == VC_FOURCC('s','t','r','d'))
      {
         /* The data in a 'strd' chunk is either codec configuration data or DRM information, 
            we can safely assume it might be either as long as we don't overwrite any config 
            data read previously from a 'strf' chunk */
         if ((status = vc_container_track_allocate_drmdata(p_ctx, track, chunk_size)) != VC_CONTAINER_SUCCESS)
         {
            LOG_DEBUG(p_ctx, "failed to allocate memory for 'strd' (%d bytes)", chunk_size);
            return status;
         }
                 
         if(READ_BYTES(p_ctx, track->priv->drmdata, chunk_size) != chunk_size) 
            return VC_CONTAINER_ERROR_FORMAT_INVALID;
         AVI_SYNC_CHUNK(p_ctx);
         
         if (!track->format->extradata)
         {
            if (vc_container_track_allocate_extradata(p_ctx, track, chunk_size) == VC_CONTAINER_SUCCESS)
            {
               memcpy(track->format->extradata, track->priv->drmdata, chunk_size);
               
               track->format->extradata = track->priv->extradata;
               track->format->extradata_size = chunk_size;
            }
            else
            {
               LOG_DEBUG(p_ctx, "failed to allocate memory for 'strd' (%d bytes)", chunk_size);
               LOG_DEBUG(p_ctx, "no codec configuration data set");
            }
         }
      }
      else if(chunk_id == VC_FOURCC('i','n','d','x'))
      {
         track_module->index_offset = STREAM_POSITION(p_ctx);
         track_module->index_size = chunk_size;
      }
      else
      {
         /* Not interested in this chunk, skip it. */
      }

      /* Skip any left-over data */
      AVI_SKIP_CHUNK(p_ctx, offset + chunk_size + 8 - STREAM_POSITION(p_ctx) );
   }

   if (!stream_header_chunk_read || !stream_format_chunk_read)
   {
      LOG_DEBUG(p_ctx, "invalid 'strl', 'strh' and 'strf' are both required");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   return status;
}

static VC_CONTAINER_STATUS_T avi_find_next_data_chunk(VC_CONTAINER_T *p_ctx, uint32_t *id, uint32_t *size)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_FOURCC_T chunk_id;
   uint32_t chunk_size = 0;
   uint32_t peek_buf[1];

   do
   {
      chunk_id = READ_FOURCC(p_ctx, "Chunk ID");
      chunk_size = READ_U32(p_ctx, "Chunk size");
      if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS)
         break;

      /* Check if this is a 'rec ' or a 'movi' LIST instead of a plain data chunk */
      if(chunk_id == VC_FOURCC('L','I','S','T')) 
      {
         if (PEEK_BYTES(p_ctx, (uint8_t*)peek_buf, 4) != 4)
            return VC_CONTAINER_ERROR_EOS;
         if (peek_buf[0] == VC_FOURCC('r','e','c',' '))
            SKIP_FOURCC(p_ctx, "rec ");
         else if (peek_buf[0] == VC_FOURCC('m','o','v','i'))
            SKIP_FOURCC(p_ctx, "movi");
         else
            AVI_SKIP_CHUNK(p_ctx, chunk_size); /* Not interested in this LIST chunk, skip it. */
         continue;
      }

      /* Check if this is a 'AVIX' RIFF header instead of a data chunk */
      if(chunk_id == VC_FOURCC('R','I','F','F'))
      {
         if (PEEK_BYTES(p_ctx, (uint8_t*)peek_buf, 4) != 4)
            return VC_CONTAINER_ERROR_EOS;
         if (peek_buf[0] == VC_FOURCC('A','V','I','X'))
            SKIP_FOURCC(p_ctx, "AVIX");
         else
            AVI_SKIP_CHUNK(p_ctx, chunk_size); /* Not interested in this RIFF header, skip it. */
         continue;
      }

      /* We treat only db/dc/dd or wb chunks as data */
      if((uint32_t)chunk_id >> 16 == AVI_TWOCC('d','c') ||
         (uint32_t)chunk_id >> 16 == AVI_TWOCC('d','b') ||
         (uint32_t)chunk_id >> 16 == AVI_TWOCC('d','d') ||
         (uint32_t)chunk_id >> 16 == AVI_TWOCC('w','b'))
      {
         *id = chunk_id;
         *size = chunk_size;
         break;
      }

      /* Need to exit if a zero sized chunk encountered so we don't loop forever. */
      if(chunk_size == 0 && chunk_id == 0) return VC_CONTAINER_ERROR_EOS;

      /* Not interested in this chunk, skip it */
      AVI_SKIP_CHUNK(p_ctx, chunk_size);
   } while ((status = STREAM_STATUS(p_ctx)) == VC_CONTAINER_SUCCESS);

   return status;
}

static void avi_track_from_chunk_id(VC_CONTAINER_FOURCC_T chunk_id, uint16_t *data_type, uint16_t *track_num)
{
   *track_num = (((uint8_t*)&chunk_id)[0] - 48) * 10 + ((uint8_t*)&chunk_id)[1] - 48;
   *data_type = (uint32_t)chunk_id >> 16;
}

static VC_CONTAINER_STATUS_T avi_check_track(VC_CONTAINER_T *p_ctx, uint16_t data_type, uint16_t track_num)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   
   if (track_num < p_ctx->tracks_num)
   {
      if (data_type == AVI_TWOCC('w','b') && p_ctx->tracks[track_num]->format->es_type != VC_CONTAINER_ES_TYPE_AUDIO)
      {
         LOG_DEBUG(p_ctx, "suspicious track type ('wb'), track %d is not an audio track", track_num);
         status = VC_CONTAINER_ERROR_FAILED;
      }
      if (data_type == AVI_TWOCC('d','b') && p_ctx->tracks[track_num]->format->es_type != VC_CONTAINER_ES_TYPE_VIDEO)
      {
         LOG_DEBUG(p_ctx, "suspicious track type ('db'), track %d is not a video track", track_num);
         status = VC_CONTAINER_ERROR_FAILED;
      }
      if (data_type == AVI_TWOCC('d','c') && p_ctx->tracks[track_num]->format->es_type != VC_CONTAINER_ES_TYPE_VIDEO)
      {
         LOG_DEBUG(p_ctx, "suspicious track type ('dc'), track %d is not a video track", track_num);
         status = VC_CONTAINER_ERROR_FAILED;
      }
      if (data_type == AVI_TWOCC('d','d') && p_ctx->tracks[track_num]->format->es_type != VC_CONTAINER_ES_TYPE_VIDEO)
      {
         LOG_DEBUG(p_ctx, "suspicious track type ('dd'), track %d is not a video track", track_num);
         status = VC_CONTAINER_ERROR_FAILED;
      }
   }
   else
   {
      LOG_DEBUG(p_ctx, "invalid track number %d (no more than %d tracks expected)", 
         track_num, p_ctx->tracks_num);
      status = VC_CONTAINER_ERROR_FAILED;        
   }
      
   return status;
}

static int avi_compare_seek_time(int64_t chunk_time, int64_t seek_time, 
   int chunk_is_keyframe, VC_CONTAINER_SEEK_FLAGS_T seek_flags)
{
   if (chunk_time == seek_time && chunk_is_keyframe && !(seek_flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
      return 0;
   
   if (chunk_time > seek_time && chunk_is_keyframe && (seek_flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
      return 0;

   if (chunk_time > seek_time && !(seek_flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
      return 1; /* Chunk time is past seek time, caller should use the previous keyframe */
      
   return -1;
}

static VC_CONTAINER_STATUS_T avi_scan_legacy_index_chunk(VC_CONTAINER_T *p_ctx, int seek_track_num,
   int64_t *time, VC_CONTAINER_SEEK_FLAGS_T flags, uint64_t *pos)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_TRACK_MODULE_T *track_module;
   AVI_TRACK_CHUNK_STATE_T selected_chunk;
   int64_t base_offset = module->data_offset;
   int64_t selected_chunk_offset = base_offset + 4;
   int32_t extra_offset = 0;
   int first_chunk_offset = 1;
   uint64_t position;

   SEEK(p_ctx, module->index_offset);
   memset(&selected_chunk, 0, sizeof(selected_chunk));

   while((status = STREAM_STATUS(p_ctx)) == VC_CONTAINER_SUCCESS &&
         (uint64_t)STREAM_POSITION(p_ctx) < module->index_offset + module->index_size)
   {
      VC_CONTAINER_FOURCC_T chunk_id;
      uint16_t data_type, track_num;
      uint32_t chunk_flags, offset, size;

      chunk_id     = READ_FOURCC(p_ctx, "Chunk ID");
      chunk_flags  = READ_U32(p_ctx, "dwFlags");
      offset       = READ_U32(p_ctx, "dwOffset");
      size         = READ_U32(p_ctx, "dwSize");

      if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) break;
   
      /* Although it's rare, the offsets might be given from the start of the file 
      instead of the data chunk, we have to handle both cases. */
      if (first_chunk_offset)
      {
         if (offset > module->data_offset) base_offset = INT64_C(0);
         selected_chunk_offset = base_offset + 4;
         first_chunk_offset = 0;
      }
   
      avi_track_from_chunk_id(chunk_id, &data_type, &track_num);
      LOG_DEBUG(p_ctx, "reading track %"PRIu16, track_num);
      
      if (avi_check_track(p_ctx, data_type, track_num) != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "skipping index entry for track %d/%d", track_num, p_ctx->tracks_num);
         continue;
      }

      track_module = p_ctx->tracks[track_num]->priv->module;

      if (data_type == AVI_TWOCC('d','d'))
      {
         if (track_num == seek_track_num)
            track_module->chunk.flags |= VC_CONTAINER_PACKET_FLAG_ENCRYPTED;
         extra_offset = -(size + 8);
      }

      /* If this entry does not affect timing, skip it */
      if ((chunk_flags & (AVIIF_LIST | AVIIF_NOTIME)) || data_type == AVI_TWOCC('d','d'))
         continue;
   
      position = base_offset + offset + extra_offset;
      extra_offset = INT64_C(0);

      /* Check validity of position */
      if (position <= module->data_offset /* || (*pos > module->data_offset + module->data_size*/)
          return VC_CONTAINER_ERROR_FORMAT_INVALID;
     
      if (track_num == seek_track_num)
      {
         bool is_keyframe = true;
         int res;

         if (p_ctx->tracks[track_num]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
            is_keyframe = chunk_flags & AVIIF_KEYFRAME;

         if (is_keyframe)
            track_module->chunk.flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;
         else
            track_module->chunk.flags &= ~(VC_CONTAINER_PACKET_FLAG_KEYFRAME);

         res = avi_compare_seek_time(track_module->chunk.time_pos, *time, is_keyframe, flags);
         if (res > 0)
            break; /* We've found the keyframe we wanted */

         if (is_keyframe)
         {
            selected_chunk_offset = position;
            selected_chunk = track_module->chunk;
         }

         if (res == 0)
            break; /* We've found the keyframe we wanted */

         track_module->chunk.index++;
         track_module->chunk.offs += size;
         track_module->chunk.time_pos = avi_calculate_chunk_time(track_module);

         LOG_DEBUG(p_ctx, "index %"PRIu64", offs %"PRIu64", time %"PRIi64"us", track_module->chunk.index,
                   track_module->chunk.offs, track_module->chunk.time_pos);
      }
   }

   if (status == VC_CONTAINER_SUCCESS ||
       /* When seeking backwards, always return the last good position */
       !(flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
   {
      *pos = selected_chunk_offset;
      track_module = p_ctx->tracks[seek_track_num]->priv->module;
      track_module->chunk.index = selected_chunk.index;
      track_module->chunk.offs = selected_chunk.offs;
      track_module->chunk.flags = selected_chunk.flags;
      track_module->chunk.time_pos = selected_chunk.time_pos;
      *time = track_module->chunk.time_pos;
      return VC_CONTAINER_SUCCESS;
   }

   return VC_CONTAINER_ERROR_NOT_FOUND;
}

static VC_CONTAINER_STATUS_T avi_scan_standard_index_chunk(VC_CONTAINER_T *p_ctx, uint64_t index_offset, 
   unsigned seek_track_num, int64_t *time, VC_CONTAINER_SEEK_FLAGS_T flags, uint64_t *pos) 
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_NOT_FOUND;
   VC_CONTAINER_TRACK_MODULE_T *track_module = NULL;
   VC_CONTAINER_FOURCC_T chunk_id; 
   uint32_t chunk_size;
   uint16_t data_type, track_num;
   uint8_t index_type, index_sub_type;
   uint32_t entry, entry_count = 0;
   uint16_t entry_size;
   uint64_t base_offset = UINT64_C(0);
   uint64_t position = UINT64_C(0);
   uint64_t prev_keyframe_offs = INT64_C(0);
   AVI_TRACK_CHUNK_STATE_T prev_keyframe_chunk = { 0 };

   SEEK(p_ctx, index_offset);

   chunk_id = READ_FOURCC(p_ctx, "Chunk ID");
   chunk_size = READ_U32(p_ctx, "Chunk Size");

   entry_size = READ_U16(p_ctx, "wLongsPerEntry");
   index_sub_type = READ_U8(p_ctx, "bIndexSubType");
   index_type = READ_U8(p_ctx, "bIndexType");
   entry_count = READ_U32(p_ctx, "nEntriesInUse");
   chunk_id = READ_FOURCC(p_ctx, "dwChunkId");
   base_offset = READ_U64(p_ctx, "qwBaseOffset");
   SKIP_U32(p_ctx, "dwReserved");

   if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS)
      return status;

   avi_track_from_chunk_id(chunk_id, &data_type, &track_num);   
   status = avi_check_track(p_ctx, data_type, track_num);
   if (status || chunk_size < 24 || track_num != seek_track_num)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   if (entry_size != 2 || index_sub_type != 0 || index_type != AVI_INDEX_OF_CHUNKS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   entry_count = MIN(entry_count, (chunk_size - 24) / (entry_size * 4));

   track_module = p_ctx->tracks[seek_track_num]->priv->module;

   for (entry = 0; entry < entry_count; ++entry)
   {
      uint32_t chunk_offset;
      int key_frame = 0;
      
      chunk_offset = READ_U32(p_ctx, "dwOffset");
      chunk_size = READ_U32(p_ctx, "dwSize");
      
      if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS)
         break;

      status = VC_CONTAINER_ERROR_NOT_FOUND;

      if (!(chunk_size & AVI_INDEX_DELTAFRAME)) 
         key_frame = 1;
      chunk_size &= ~AVI_INDEX_DELTAFRAME;

      position = base_offset + chunk_offset - 8;

      if (key_frame)
         track_module->chunk.flags = VC_CONTAINER_PACKET_FLAG_KEYFRAME;
      else
         track_module->chunk.flags = 0;

      if (time != NULL)
      {
         int res;
         status = VC_CONTAINER_ERROR_NOT_FOUND;
         res = avi_compare_seek_time(track_module->chunk.time_pos, *time, key_frame, flags);

         if (res == 0)
         {
            *pos = position;
            *time = track_module->chunk.time_pos;
            status = VC_CONTAINER_SUCCESS;
            break;
         }
         else if (res > 0)
         {
            if (prev_keyframe_offs)
            {
               *pos = prev_keyframe_offs;
               track_module->chunk = prev_keyframe_chunk;
               *time = track_module->chunk.time_pos;
               status = VC_CONTAINER_SUCCESS;
            }
            break;
         }
           
         if (key_frame)
         {
            prev_keyframe_offs = position;
            prev_keyframe_chunk = track_module->chunk;
         }
      }
      else
      {
         /* Not seeking to a time position, but scanning
            track chunk state up to a certain file position 
            instead */
         if (position >= *pos)
         {
            status = VC_CONTAINER_SUCCESS;
            break;
         }
      }

      track_module->chunk.index++;
      track_module->chunk.offs += chunk_size;
      track_module->chunk.time_pos = avi_calculate_chunk_time(track_module);
   }

   return status;
}

static VC_CONTAINER_STATUS_T avi_scan_super_index_chunk(VC_CONTAINER_T *p_ctx, unsigned index_track_num, 
   int64_t *time, VC_CONTAINER_SEEK_FLAGS_T flags, uint64_t *pos)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_NOT_FOUND;
   VC_CONTAINER_FOURCC_T chunk_id;
   uint64_t index_offset;
   uint32_t index_size;
   uint16_t data_type, track_num;
   uint32_t entry, entry_count;
   uint16_t entry_size; 
   uint8_t index_sub_type, index_type;
   
   index_offset = p_ctx->tracks[index_track_num]->priv->module->index_offset;
   index_size = p_ctx->tracks[index_track_num]->priv->module->index_size;
   
   SEEK(p_ctx, index_offset);
   
   entry_size = READ_U16(p_ctx, "wLongsPerEntry");
   index_sub_type = READ_U8(p_ctx, "bIndexSubType");
   index_type = READ_U8(p_ctx, "bIndexType");
   entry_count = READ_U32(p_ctx, "nEntriesInUse");
   chunk_id = READ_FOURCC(p_ctx, "dwChunkId");
   SKIP_U32(p_ctx, "dwReserved0");
   SKIP_U32(p_ctx, "dwReserved1");
   SKIP_U32(p_ctx, "dwReserved2");

   if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS)
      return status;
      
   if (index_type == AVI_INDEX_OF_INDEXES)
   {
      avi_track_from_chunk_id(chunk_id, &data_type, &track_num);
      status = avi_check_track(p_ctx, data_type, track_num);
      if (status || index_size < 24 || track_num != index_track_num) return VC_CONTAINER_ERROR_FORMAT_INVALID;
      
      /* FIXME: We should probably support AVI_INDEX_2FIELD as well */
      if (entry_size != 4 || index_sub_type != 0)
         return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   
      entry_count = MIN(entry_count, (index_size - 24) / entry_size);

      for (entry = 0; entry < entry_count; ++entry)
      {
         uint64_t entry_offset, standard_index_offset;
         standard_index_offset = READ_U64(p_ctx, "qwOffset");
         SKIP_U32(p_ctx, "dwSize");
         SKIP_U32(p_ctx, "dwDuration");
         
         if ((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) 
            break;            

         if (standard_index_offset == UINT64_C(0))
         {
            status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED; /* Not plausible */
            break;
         }

         entry_offset = STREAM_POSITION(p_ctx);
         status = avi_scan_standard_index_chunk(p_ctx, standard_index_offset, index_track_num, time, flags, pos);
         if (status != VC_CONTAINER_ERROR_NOT_FOUND) break;
         SEEK(p_ctx, entry_offset); /* Move to next entry ('ix' chunk); */
      }
   }
   else if (index_type == AVI_INDEX_OF_CHUNKS)
   {
      /* It seems we are dealing with a standard index instead... */
      status = avi_scan_standard_index_chunk(p_ctx, index_offset, index_track_num, time, flags, pos);
   }
   else
   {
      status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }
   
   return status;
}

static VC_CONTAINER_STATUS_T avi_read_dd_chunk( VC_CONTAINER_T *p_ctx,
   AVI_TRACK_STREAM_STATE_T *p_state, uint16_t data_type, uint32_t chunk_size,
   uint16_t track_num )
{
   if (data_type == AVI_TWOCC('d','d'))
   {
      if (p_state->extra_chunk_data_len ||
          chunk_size > sizeof(p_state->extra_chunk_data))
      {
         LOG_DEBUG(p_ctx, "cannot handle multiple consecutive 'dd' chunks");
         return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      }
      if(READ_BYTES(p_ctx, p_state->extra_chunk_data, chunk_size) != chunk_size)
         return VC_CONTAINER_ERROR_FORMAT_INVALID;

      AVI_SYNC_CHUNK(p_ctx);
      p_state->extra_chunk_track_num = track_num;
      p_state->extra_chunk_data_len = chunk_size;
      p_state->extra_chunk_data_offs = 0;

      return VC_CONTAINER_ERROR_CONTINUE;
   }
   else if (p_state->extra_chunk_data_len &&
      p_state->extra_chunk_track_num != track_num)
   {
      LOG_DEBUG(p_ctx, "dropping data from '%02ddd' chunk, not for this track (%d)",
         p_state->extra_chunk_track_num, track_num);
      p_state->extra_chunk_data_len = 0;
   }

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/

static VC_CONTAINER_STATUS_T avi_reader_read( VC_CONTAINER_T *p_ctx,
                                              VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = NULL;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   AVI_TRACK_STREAM_STATE_T *p_state = &module->state;

   if (flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
   {
      p_state = p_ctx->tracks[p_packet->track]->priv->module->chunk.state;
   }

   LOG_DEBUG(p_ctx, "seeking to %"PRIi64, p_state->data_offset);
   SEEK(p_ctx, p_state->data_offset);

   if (p_state->chunk_data_left == 0)
   {
      VC_CONTAINER_FOURCC_T chunk_id;
      uint32_t chunk_size;
      uint16_t data_type, track_num;
      
      if ((status = avi_find_next_data_chunk(p_ctx, &chunk_id, &chunk_size)) != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "unable to find the next data chunk %d", status);
         p_state->data_offset = STREAM_POSITION(p_ctx);
         return status;
      }

      avi_track_from_chunk_id(chunk_id, &data_type, &track_num);

      if (avi_check_track(p_ctx, data_type, track_num) != VC_CONTAINER_SUCCESS)
      {
         AVI_SKIP_CHUNK(p_ctx, chunk_size);
         LOG_DEBUG(p_ctx, "skipping data for track %d/%d", track_num, p_ctx->tracks_num);

         p_state->data_offset = STREAM_POSITION(p_ctx);
         return VC_CONTAINER_ERROR_CONTINUE;
      }

      /* If we are reading from the global state (i.e. normal read or forced
         read from the track on the global state), and the track we found is
         not on the global state, connect the two */
      if (p_state == &module->state &&
         p_ctx->tracks[track_num]->priv->module->chunk.state != &module->state)
      {
         int64_t next_chunk;

         /* The track's offset is past the current position, skip it as we are
            not interested in track data from before the track's offset. If we
            were to read it we would return the same data multiple times. */
         next_chunk = (STREAM_POSITION(p_ctx) + chunk_size + 1) & ~1;
         if (p_ctx->tracks[track_num]->priv->module->chunk.state->data_offset > next_chunk)
         {
            AVI_SKIP_CHUNK(p_ctx, chunk_size);
            LOG_DEBUG(p_ctx, "skipping track %d/%d as we have already read it", track_num, p_ctx->tracks_num);
            p_state->data_offset = STREAM_POSITION(p_ctx);
            return VC_CONTAINER_ERROR_CONTINUE;
         }

         /* The track state must be pointing to the current chunk. We need to
            reconnect the track to the global state. */
         LOG_DEBUG(p_ctx, "reconnect track %u to the global state", track_num);

         p_ctx->tracks[track_num]->priv->module->chunk.state = &module->state;

         module->state = p_ctx->tracks[track_num]->priv->module->chunk.local_state;

         vc_container_assert(chunk_size >= p_state->chunk_data_left);
         vc_container_assert(!p_state->chunk_data_left ||
            ((p_state->data_offset + p_state->chunk_data_left + 1) & ~1) == next_chunk);
         vc_container_assert(p_state->current_track_num == track_num);

         return VC_CONTAINER_ERROR_CONTINUE;
      }

      /* If we are not forcing, or if we are and found the track we are
         interested in, check for dd data and set the track module for the later code */
      if (!(flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK) ||
          (track_num == p_packet->track))
      {
         if ((status = avi_read_dd_chunk(p_ctx, p_state, data_type, chunk_size, track_num)) != VC_CONTAINER_SUCCESS)
         {
            p_state->data_offset = STREAM_POSITION(p_ctx);
            return status;
         }
      }

      p_state->chunk_size = p_state->chunk_data_left = chunk_size;
      p_state->current_track_num = track_num;
   }

   /* If there is data from another track skip past it */
   if (flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK &&
       p_state->current_track_num != p_packet->track)
   {
      p_state->data_offset = STREAM_POSITION(p_ctx);

      AVI_SKIP_CHUNK(p_ctx, p_state->chunk_data_left);
      LOG_DEBUG(p_ctx, "skipping track %d/%d as we are ignoring it",
         p_state->current_track_num, p_ctx->tracks_num);

      track_module = p_ctx->tracks[p_packet->track]->priv->module;

      /* Handle disconnection from global state */
      if (p_state == &module->state &&
         p_ctx->tracks[p_state->current_track_num]->priv->module->chunk.state == &module->state)
      {
         /* Make a copy of the global state */
         LOG_DEBUG(p_ctx, "using local state on track %d", p_packet->track);
         track_module->chunk.local_state = module->state;
         track_module->chunk.state = &track_module->chunk.local_state;
      }

      track_module->chunk.state->data_offset = STREAM_POSITION(p_ctx);
      track_module->chunk.state->chunk_data_left = 0;

      return VC_CONTAINER_ERROR_CONTINUE;
   }

   track_module = p_ctx->tracks[p_state->current_track_num]->priv->module;

   if (flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
   {
      vc_container_assert(p_state->current_track_num == p_packet->track);
   }

   LOG_DEBUG(p_ctx, "reading track %u chunk at time %"PRIi64"us, offset %"PRIu64,
             p_state->current_track_num, track_module->chunk.time_pos,
             track_module->chunk.offs);
   if (p_state->extra_chunk_data_len)
      track_module->chunk.flags |= VC_CONTAINER_PACKET_FLAG_ENCRYPTED;
   else
      track_module->chunk.flags &= ~VC_CONTAINER_PACKET_FLAG_ENCRYPTED;

   if (p_packet)
   {
      p_packet->track = p_state->current_track_num;
      p_packet->size = p_state->chunk_data_left +
                       p_state->extra_chunk_data_len;
      p_packet->flags = track_module->chunk.flags;

      if (p_state->chunk_data_left == p_state->chunk_size)
      {
         p_packet->pts = track_module->chunk.time_pos;
         if (track_module->sample_size == 0)
            p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME;
      }
      else
      {
         p_packet->pts = VC_CONTAINER_TIME_UNKNOWN;
         if (track_module->sample_size == 0)
            p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
      }
    
      p_packet->dts = VC_CONTAINER_TIME_UNKNOWN;
   }

   if (flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      SKIP_BYTES(p_ctx, p_state->chunk_data_left);
      AVI_SYNC_CHUNK(p_ctx);
      p_state->chunk_data_left = 0;
      p_state->extra_chunk_data_len = 0;
   }

   if (flags & VC_CONTAINER_READ_FLAG_INFO)
   {
      p_state->data_offset = STREAM_POSITION(p_ctx);

      LOG_DEBUG(p_ctx, "data position %"PRIi64, p_state->data_offset);

      return VC_CONTAINER_SUCCESS;
   }
   
   if (p_packet)
   {
      uint8_t *data = p_packet->data;
      uint32_t buffer_size = p_packet->buffer_size;
      uint32_t size = 0;
      uint32_t len;     
      
      /* See if we need to insert extra data */
      if (p_state->extra_chunk_data_len)
      {
         len = MIN(buffer_size, p_state->extra_chunk_data_len);
         memcpy(data, p_state->extra_chunk_data + p_state->extra_chunk_data_offs, len);
         data += len;
         buffer_size -= len;
         size = len;
         p_state->extra_chunk_data_len -= len;
         p_state->extra_chunk_data_offs += len;
      }

      /* Now try to read data into buffer */
      len = MIN(buffer_size, p_state->chunk_data_left);
      READ_BYTES(p_ctx, data, len);
      size += len;
      p_state->chunk_data_left -= len;
      p_packet->size = size;

      if (p_state->chunk_data_left)
         p_packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;
   }

   if (p_state->chunk_data_left == 0)
   {
      AVI_SYNC_CHUNK(p_ctx);
      track_module->chunk.index++;
      track_module->chunk.offs += p_state->chunk_size;
      track_module->chunk.flags = 0;
      track_module->chunk.time_pos = avi_calculate_chunk_time(track_module);
   }

   /* Update the track's position */
   p_state->data_offset = STREAM_POSITION(p_ctx);

   LOG_DEBUG(p_ctx, "data position %"PRIi64, p_state->data_offset);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_reader_seek( VC_CONTAINER_T *p_ctx, int64_t *p_offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint64_t position, pos;
   AVI_TRACK_CHUNK_STATE_T chunk_state[AVI_TRACKS_MAX];
   AVI_TRACK_STREAM_STATE_T global_state;
   unsigned seek_track_num, i;

   if (mode != VC_CONTAINER_SEEK_MODE_TIME || !STREAM_SEEKABLE(p_ctx))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   LOG_DEBUG(p_ctx, "AVI seeking to %"PRIi64"us", *p_offset);

   /* Save current position and chunk state so we can restore it if we 
      hit an error whilst scanning index data */
   position = STREAM_POSITION(p_ctx);
   for(i = 0; i < p_ctx->tracks_num; i++)
     chunk_state[i] = p_ctx->tracks[i]->priv->module->chunk;
   global_state = p_ctx->priv->module->state;

   /* Clear track state affected by a seek operation of any kind */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      p_ctx->tracks[i]->priv->module->chunk.index = INT64_C(0);
      p_ctx->tracks[i]->priv->module->chunk.offs = INT64_C(0);
      p_ctx->tracks[i]->priv->module->chunk.flags = 0;
      p_ctx->tracks[i]->priv->module->chunk.time_pos = p_ctx->tracks[i]->priv->module->time_start;
      p_ctx->tracks[i]->priv->module->chunk.state = &p_ctx->tracks[i]->priv->module->chunk.local_state;
      p_ctx->tracks[i]->priv->module->chunk.local_state.chunk_data_left = UINT64_C(0);
      p_ctx->tracks[i]->priv->module->chunk.local_state.chunk_size = UINT64_C(0);
      p_ctx->tracks[i]->priv->module->chunk.local_state.extra_chunk_data_len = 0;
      p_ctx->tracks[i]->priv->module->chunk.local_state.data_offset = module->data_offset + 4;
   }

   /* Clear the global state */
   p_ctx->priv->module->state.chunk_data_left = UINT64_C(0);
   p_ctx->priv->module->state.chunk_size = UINT64_C(0);
   p_ctx->priv->module->state.extra_chunk_data_len = 0;
   p_ctx->priv->module->state.data_offset = module->data_offset + 4;

   /* Choose track to use for seeking, favor video tracks and tracks
      that are enabled */
   for(i = 0; i < p_ctx->tracks_num; i++)
      if(p_ctx->tracks[i]->is_enabled &&
         p_ctx->tracks[i]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO) break;
   if(i == p_ctx->tracks_num)
      for(i = 0; i < p_ctx->tracks_num; i++)
         if(p_ctx->tracks[i]->is_enabled) break;
   if(i == p_ctx->tracks_num) i = 0;

   LOG_DEBUG(p_ctx, "seek on track %d/%d", i, p_ctx->tracks_num);
   seek_track_num = i;

   if (p_ctx->tracks[seek_track_num]->priv->module->index_offset)
   {
      LOG_DEBUG(p_ctx, "seeking using the super index");
      status = avi_scan_super_index_chunk(p_ctx, seek_track_num, p_offset, flags, &pos);
      if (status != VC_CONTAINER_SUCCESS) goto error;

      /* As AVI chunks don't convey timestamp information, we need to scan all tracks
         to the seek file position */
      for(i = 0; i < p_ctx->tracks_num; i++)
      {
         if (p_ctx->tracks[i]->priv->module->index_offset && i != seek_track_num)
         {
            uint64_t track_pos;
            int64_t track_time = *p_offset;

            status = avi_scan_super_index_chunk(p_ctx, i, &track_time, flags, &track_pos);
            if (status != VC_CONTAINER_SUCCESS) goto error;
            p_ctx->tracks[i]->priv->module->chunk.local_state.data_offset = track_pos;
         }
      }
   }
   else
   {
      LOG_DEBUG(p_ctx, "seeking using the legacy index");

      /* The legacy index comes after data so it might not have been available at the
         time the container was opened; if this is the case, see if we can find an index
         now, if we can't, then there's no way we can proceed with the seek. */
      if(!module->index_offset)
      {
         uint32_t chunk_size;

         LOG_DEBUG(p_ctx, "no index offset, searching for one");

         /* Locate data chunk and skip it */
         SEEK(p_ctx, module->data_offset);
         AVI_SKIP_CHUNK(p_ctx, module->data_size);
         /* Now search for the index */
         status = avi_find_chunk(p_ctx, VC_FOURCC('i','d','x','1'), &chunk_size);
         if (status == VC_CONTAINER_SUCCESS)
         {
            /* Store offset to index data */
            module->index_offset = STREAM_POSITION(p_ctx);
            module->index_size = chunk_size;
            p_ctx->capabilities |= VC_CONTAINER_CAPS_HAS_INDEX;
            p_ctx->capabilities |= VC_CONTAINER_CAPS_DATA_HAS_KEYFRAME_FLAG;
         }
      }
      /* Check again, we may or may not have an index */
      if (!module->index_offset)
      {
         /* If there is no index and we are seeking to 0 we can assume the
            correct location is the start of the data. Otherwise we are unable
            to seek to a specified non-zero location without an index */
         if (*p_offset != INT64_C(0))
         {
            LOG_DEBUG(p_ctx, "failed to find the legacy index, unable to seek");
            status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
            goto error;
         }
         pos = module->data_offset;
      }
      else
      {
         LOG_DEBUG(p_ctx, "scanning the legacy index chunk");
         status = avi_scan_legacy_index_chunk(p_ctx, seek_track_num, p_offset, flags, &pos);
         if (status != VC_CONTAINER_SUCCESS) goto error;

         for (i = 0; i < p_ctx->tracks_num; i++)
         {
            if (i != seek_track_num)
            {
               uint64_t track_pos = pos;
               int64_t track_time = *p_offset;

               status = avi_scan_legacy_index_chunk(p_ctx, i, &track_time, flags, &track_pos);
               if (status != VC_CONTAINER_SUCCESS) goto error;
               p_ctx->tracks[i]->priv->module->chunk.local_state.data_offset = track_pos;
               p_ctx->tracks[i]->priv->module->chunk.local_state.current_track_num = i;
            }
         }
      }
   }

   position = pos;

   /* Set the seek track's data offset */
   p_ctx->tracks[seek_track_num]->priv->module->chunk.local_state.data_offset = position;
   p_ctx->tracks[seek_track_num]->priv->module->chunk.local_state.current_track_num = seek_track_num;

   /* Connect the earlier track(s) to the global state. Needs 2 passes */
   module->state.data_offset = INT64_MAX;
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      if(p_ctx->tracks[i]->priv->module->chunk.local_state.data_offset <
         module->state.data_offset)
         module->state = p_ctx->tracks[i]->priv->module->chunk.local_state;
   }
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      if(p_ctx->tracks[i]->priv->module->chunk.local_state.data_offset ==
         module->state.data_offset)
         p_ctx->tracks[i]->priv->module->chunk.state = &module->state;
   }

   LOG_DEBUG(p_ctx, "seek to %"PRIi64", position %"PRIu64, *p_offset, pos);

   return SEEK(p_ctx, position);

error:
   p_ctx->priv->module->state = global_state;
   for(i = 0; i < p_ctx->tracks_num; i++)
     p_ctx->tracks[i]->priv->module->chunk = chunk_state[i];
   SEEK(p_ctx, position);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T avi_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;   
   unsigned int i;

   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   p_ctx->tracks = NULL;
   p_ctx->tracks_num = 0;
   free(module);
   p_ctx->priv->module = 0;  
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T avi_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   uint32_t chunk_size;
   uint32_t peek_buf[3];
   unsigned int i;
   uint32_t flags, num_streams;
   int64_t offset;

   /* Check the RIFF chunk descriptor */
   if (PEEK_BYTES(p_ctx, (uint8_t*)peek_buf, 12) != 12)
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if( peek_buf[0] != VC_FOURCC('R','I','F','F') )
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if( peek_buf[2] != VC_FOURCC('A','V','I',' ') )
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /*
    *  We now know we are dealing with an AVI file
    */
   SKIP_FOURCC(p_ctx, "RIFF ID");
   SKIP_U32(p_ctx, "fileSize");
   SKIP_FOURCC(p_ctx, "fileType");

   /* Look for the 'hdrl' LIST (sub)chunk */   
   status = avi_find_list(p_ctx, VC_FOURCC('h','d','r','l'), &chunk_size);
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_DEBUG(p_ctx, "'hdrl' LIST not found");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   SKIP_FOURCC(p_ctx, "hdrl");

   /* Now look for the 'avih' sub-chunk */
   status = avi_find_chunk(p_ctx, VC_FOURCC('a','v','i','h'), &chunk_size);
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_DEBUG(p_ctx, "'avih' not found");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   /* Parse the 'avih' sub-chunk */
   SKIP_U32(p_ctx, "dwMicroSecPerFrame");
   SKIP_U32(p_ctx, "dwMaxBytesPerSec");
   SKIP_U32(p_ctx, "dwPaddingGranularity");
   flags = READ_U32(p_ctx, "dwFlags");
   SKIP_U32(p_ctx, "dwTotalFrames");
   SKIP_U32(p_ctx, "dwInitialFrames");
   num_streams = READ_U32(p_ctx, "dwStreams");
   SKIP_U32(p_ctx, "dwSuggestedBufferSize");
   SKIP_U32(p_ctx, "dwWidth");
   SKIP_U32(p_ctx, "dwHeight");
   SKIP_U32(p_ctx, "dwReserved0");
   SKIP_U32(p_ctx, "dwReserved1");
   SKIP_U32(p_ctx, "dwReserved2");
   SKIP_U32(p_ctx, "dwReserved3");

   if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS)
      goto error;

   /* Allocate our context and tracks */
   if ((module = malloc(sizeof(*module))) == NULL)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY; 
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;

   if (num_streams > AVI_TRACKS_MAX)
   {
      LOG_DEBUG(p_ctx, "cannot handle %u tracks, restricted to %d", num_streams, AVI_TRACKS_MAX);
      num_streams = AVI_TRACKS_MAX; 
   }

   for (p_ctx->tracks_num = 0; p_ctx->tracks_num != num_streams; p_ctx->tracks_num++)
   {
      p_ctx->tracks[p_ctx->tracks_num] = vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
      if(!p_ctx->tracks[p_ctx->tracks_num]) break;
   }
   if(p_ctx->tracks_num != num_streams)
   { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }

   /* Try to read stream header list chunks ('strl') */
   for (i = 0; i != num_streams; ++i)
   {
      status = avi_read_stream_header_list(p_ctx, p_ctx->tracks[i], p_ctx->tracks[i]->priv->module);
      if(status != VC_CONTAINER_SUCCESS) goto error;
   }

   /* Look for the 'movi' LIST (sub)chunk */
   status = avi_find_list(p_ctx, VC_FOURCC('m','o','v','i'), &chunk_size);
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_DEBUG(p_ctx, "'movi' LIST not found");
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      goto error;
   }
         
   /* Store offset to the start and size of data (the 'movi' LIST) */
   module->data_offset = STREAM_POSITION(p_ctx);
   module->data_size = chunk_size;

   p_ctx->priv->pf_close = avi_reader_close;
   p_ctx->priv->pf_read = avi_reader_read;
   p_ctx->priv->pf_seek = avi_reader_seek;

   if (flags & AVIF_MUSTUSEINDEX)
   {
      LOG_DEBUG(p_ctx, "AVIF_MUSTUSEINDEX not supported, playback might not work properly");
   }

   /* If the stream is seekable, see if we can find an index (for at 
      least one of the tracks); even if we cannot find an index now, 
      one might become available later (e.g. when the stream grows
      run-time), in that case we might want to report that we can seek 
      and re-search for the index again if or when we're requested to 
      seek. */
   if(STREAM_SEEKABLE(p_ctx))
   {
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;
      p_ctx->capabilities |= VC_CONTAINER_CAPS_FORCE_TRACK;

      for(i = 0; i < p_ctx->tracks_num; i++)
         if(p_ctx->tracks[i]->priv->module->index_offset) break;

      if (i < p_ctx->tracks_num)
      {
         p_ctx->capabilities |= VC_CONTAINER_CAPS_HAS_INDEX;
         if (flags & AVIF_TRUSTCKTYPE)
            p_ctx->capabilities |= VC_CONTAINER_CAPS_DATA_HAS_KEYFRAME_FLAG;
      }
      else
      {
         /* Skip data first */
         AVI_SKIP_CHUNK(p_ctx, module->data_size);
         /* Now search for the index */
         status = avi_find_chunk(p_ctx, VC_FOURCC('i','d','x','1'), &chunk_size);
         if (status == VC_CONTAINER_SUCCESS)
         {
            LOG_DEBUG(p_ctx, "'idx1' found");
            /* Store offset to index data */
            module->index_offset = STREAM_POSITION(p_ctx);
            module->index_size = chunk_size;
            p_ctx->capabilities |= VC_CONTAINER_CAPS_HAS_INDEX;
            p_ctx->capabilities |= VC_CONTAINER_CAPS_DATA_HAS_KEYFRAME_FLAG;
         }
   
         /* Seek back to the start of the data */
         SEEK(p_ctx, module->data_offset);
      }
   }

   SKIP_FOURCC(p_ctx, "movi");

   for (i = 0; i != num_streams; i++)
   {
      p_ctx->tracks[i]->priv->module->chunk.state = &p_ctx->priv->module->state;
   }
   p_ctx->priv->module->state.data_offset = STREAM_POSITION(p_ctx);

   /* Update the tracks to set their data offsets. This help with bad
      interleaving, for example when there is all the video tracks followed
      by all the audio tracks. It means we don't have to read through the
      tracks we are not interested in when forcing a read from a given track,
      as could be the case in the above example. If this fails we will fall
      back to skipping track data. */
   offset = INT64_C(0);
   avi_reader_seek(p_ctx, &offset, VC_CONTAINER_SEEK_MODE_TIME, VC_CONTAINER_SEEK_FLAG_PRECISE);

   if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;
   return VC_CONTAINER_SUCCESS;

error:
   LOG_DEBUG(p_ctx, "error opening stream (%i)", status);
   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   p_ctx->tracks = NULL;
   p_ctx->tracks_num = 0;
   if (module) free(module);
   p_ctx->priv->module = NULL;
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open avi_reader_open
#endif
