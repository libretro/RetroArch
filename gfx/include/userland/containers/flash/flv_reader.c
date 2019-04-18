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

/* Work-around for MSVC debugger issue */
#define VC_CONTAINER_MODULE_T VC_CONTAINER_MODULE_FLV_READER_T
#define VC_CONTAINER_TRACK_MODULE_T VC_CONTAINER_TRACK_MODULE_FLV_READER_T

//#define ENABLE_FLV_EXTRA_LOGGING
#define CONTAINER_IS_BIG_ENDIAN
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_index.h"
#include "containers/core/containers_logging.h"
#undef CONTAINER_HELPER_LOG_INDENT
#define CONTAINER_HELPER_LOG_INDENT(a) 0

VC_CONTAINER_STATUS_T flv_reader_open( VC_CONTAINER_T *p_ctx );

/******************************************************************************
Defines.
******************************************************************************/
#define FLV_TRACKS_MAX 2

#define FLV_TAG_TYPE_AUDIO 8
#define FLV_TAG_TYPE_VIDEO 9
#define FLV_TAG_TYPE_METADATA 18
#define FLV_TAG_HEADER_SIZE 15

#define FLV_SCRIPT_DATA_TYPE_NUMBER      0
#define FLV_SCRIPT_DATA_TYPE_BOOL        1
#define FLV_SCRIPT_DATA_TYPE_STRING      2
#define FLV_SCRIPT_DATA_TYPE_ECMA        8
#define FLV_SCRIPT_DATA_TYPE_LONGSTRING 12

#define FLV_FLAG_DISCARD    1
#define FLV_FLAG_KEYFRAME   2
#define FLV_FLAG_INTERFRAME 4

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct
{
   VC_CONTAINER_STATUS_T status;

   int64_t tag_position; /* position of the current tag we're reading */
   int64_t data_position; /* position to the start of the data within the tag */
   int     data_offset; /* current position inside the tag's data */
   int     data_size; /* size of the data from the current tag */
   int     tag_prev_size; /* size of the previous tag in the stream */
   int     flags; /* flags for the current tag */
   uint32_t timestamp; /* timestamp for the current tag */
   uint32_t track; /* track the current tag belongs to */
   VC_CONTAINER_INDEX_T *index; /* index of key frames */

} FLV_READER_STATE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   FLV_READER_STATE_T *state;
   FLV_READER_STATE_T track_state;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *tracks[FLV_TRACKS_MAX];

   int64_t data_offset; /*< offset to the first FLV tag in the stream */

   FLV_READER_STATE_T state; /*< global state of the reader */

   int audio_track;
   int video_track;

   uint32_t meta_videodatarate;
   uint32_t meta_audiodatarate;
   float meta_framerate;
   uint32_t meta_width;
   uint32_t meta_height;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Static functions within this file.
******************************************************************************/
/** Reads an FLV tag header
  *
  * @param p_ctx              pointer to our context
  * @param[out] p_prev_size   size of the previous tag
  * @param[out] p_type        type of the tag
  * @param[out] p_type        size of the tag
  * @param[out] p_type        timestamp for the tag
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_read_tag_header(VC_CONTAINER_T *p_ctx, int *p_prev_size,
                                                 int *p_type, int *p_size, uint32_t *p_timestamp)
{
   int prev_size, type, size;
   uint32_t timestamp;

   prev_size = READ_U32(p_ctx, "PreviousTagSize");
   type = READ_U8(p_ctx, "TagType");
   size = READ_U24(p_ctx, "DataSize");
   timestamp = READ_U24(p_ctx, "Timestamp");
   timestamp |= (READ_U8(p_ctx, "TimestampExtended") << 24);
   SKIP_U24(p_ctx, "StreamID");

   if(p_prev_size) *p_prev_size = prev_size + 4;
   if(p_type) *p_type = type;
   if(p_size) *p_size = size;
   if(p_timestamp) *p_timestamp = timestamp;

   return STREAM_STATUS(p_ctx);
}

/** Reads an FLV video data header.
  * This contains the codec id and the current frame type.
  *
  * @param p_ctx              pointer to our context
  * @param[out] codec         video codec
  * @param[out] frame_type    type of the current frame
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_read_videodata_header(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_FOURCC_T *codec, int *frame_type)
{
   uint8_t header = READ_U8(p_ctx, "FrameType/CodecID");

   if(frame_type)
      *frame_type = (header >> 4) == 1 ? FLV_FLAG_KEYFRAME :
         (header >> 4) == 3 ? FLV_FLAG_INTERFRAME : 0;

   switch(header &0xF)
   {
   case 2: *codec = VC_CONTAINER_CODEC_SPARK; break;
   case 3: *codec = VC_FOURCC('s','c','r','1'); break; /* screen video */
   case 4: *codec = VC_CONTAINER_CODEC_VP6; break;
   case 5: *codec = VC_FOURCC('v','p','6','a'); break; /* vp6 alpha */
   case 6: *codec = VC_FOURCC('s','c','r','2'); break; /* screen video 2 */
   case 7: *codec = VC_CONTAINER_CODEC_H264; break;
   default: *codec = 0; break;
   }

   return STREAM_STATUS(p_ctx);
}

/** Get the properties of a video frame
  * This is only really useful at setup time when trying to detect
  * the type of content we are dealing with.
  * This will try to get some of the properties of the video stream
  * as well as codec configuration data if there is any.
  *
  * @param p_ctx              pointer to our context
  * @param track              track number this data/tag belongs to
  * @param size               size of the data we are parsing
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_read_videodata_properties(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *track, int size)
{
   VC_CONTAINER_STATUS_T status;
   int width = 0, height = 0;

   if(track->format->codec == VC_CONTAINER_CODEC_VP6 ||
      track->format->codec == VC_FOURCC('v','p','6','a'))
   {
      /* Just extract the width / height */
      uint8_t data = _READ_U8(p_ctx);
      _SKIP_U16(p_ctx);
      height = _READ_U8(p_ctx) * 16;
      width = _READ_U8(p_ctx) * 16;
      width -= data >> 4;
      height -= data & 0xf;
   }
   else if(track->format->codec == VC_CONTAINER_CODEC_H264)
   {
      uint8_t type = _READ_U8(p_ctx); size--;
      if(type || size <= 8) return VC_CONTAINER_ERROR_CORRUPTED;
      _SKIP_U24(p_ctx); size-=3;

      track->format->codec_variant = VC_FOURCC('a','v','c','C');
      status = vc_container_track_allocate_extradata(p_ctx, track, size);
      if(status != VC_CONTAINER_SUCCESS) return status;
      track->format->extradata_size = READ_BYTES(p_ctx, track->format->extradata, size);
   }

   track->format->type->video.width = width;
   track->format->type->video.height = height;

   return STREAM_STATUS(p_ctx);
}

/** Reads an FLV audio data header.
  * This contains the codec id, samplerate, number of channels and bitrate.
  *
  * @param p_ctx              pointer to our context
  * @param[out] codec         audio codec
  * @param[out] p_samplerate  audio sampling rate
  * @param[out] p_channels    number of audio channels
  * @param[out] p_bps         bits per sample
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_read_audiodata_header(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_FOURCC_T *codec, int *p_samplerate, int *p_channels, int *p_bps)
{
   int samplerate, channels, bps;
   uint8_t header = _READ_U8(p_ctx);

   switch((header >> 2) & 0x3)
   {
   case 0: samplerate = 5512; break;
   case 1: samplerate = 11025; break;
   case 2: samplerate = 22050; break;
   default:
   case 3: samplerate = 44100; break;
   }

   channels = 1 << (header & 1);
   bps = 8 << ((header >> 1) & 1);

   switch(header >> 4)
   {
   case 0: *codec = bps == 8 ? VC_CONTAINER_CODEC_PCM_UNSIGNED : VC_CONTAINER_CODEC_PCM_SIGNED; break;
   case 1: *codec = VC_CONTAINER_CODEC_ADPCM_SWF; break;
   case 2: *codec = VC_CONTAINER_CODEC_MPGA; break;
   case 3: *codec = bps == 8 ? VC_CONTAINER_CODEC_PCM_UNSIGNED : VC_CONTAINER_CODEC_PCM_SIGNED_LE; break;
   case 4: *codec = VC_CONTAINER_CODEC_NELLYMOSER; samplerate = 8000; channels = 1; break;
   case 5: *codec = VC_CONTAINER_CODEC_NELLYMOSER; samplerate = 16000; channels = 1; break;
   case 6: *codec = VC_CONTAINER_CODEC_NELLYMOSER; channels = 1; break;
   case 7: *codec = VC_CONTAINER_CODEC_ALAW; break;
   case 8: *codec = VC_CONTAINER_CODEC_MULAW; break;
   case 10: *codec = VC_CONTAINER_CODEC_MP4A; samplerate = 44100; channels = 2; break;
   case 11: *codec = VC_CONTAINER_CODEC_SPEEX; break;
   case 14: *codec = VC_CONTAINER_CODEC_MPGA; samplerate = 8000; break;
   default: *codec = 0; break;
   }

   if(p_samplerate) *p_samplerate = samplerate;
   if(p_channels) *p_channels = channels;
   if(p_bps) *p_bps = bps;

   return STREAM_STATUS(p_ctx);
}

/** Get the properties of an audio frame
  * This is only really useful at setup time when trying to detect
  * the type of content we are dealing with.
  * This will try to get some of the properties of the audio stream
  * as well as codec configuration data if there is any.
  *
  * @param p_ctx              pointer to our context
  * @param track              track number this data/tag belongs to
  * @param size               size of the data we are parsing
  * @param samplerate         sampling rate of the audio data
  * @param channels           number of channels of the audio data
  * @param bps                bits per sample
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_read_audiodata_properties(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *track, int size, int samplerate, int channels, int bps)
{
   static const int aac_freq[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                    22050, 16000, 12000, 11025, 8000, 7350};
   VC_CONTAINER_STATUS_T status;

   if(track->format->codec == VC_CONTAINER_CODEC_MP4A)
   {
      uint8_t *p_data, data = _READ_U8(p_ctx);
      size--;
      if(data || size <= 0) return VC_CONTAINER_ERROR_FAILED;

      /* Read the AudioSpecificConfig */
      status = vc_container_track_allocate_extradata(p_ctx, track, size);
      if(status != VC_CONTAINER_SUCCESS) return status;
      track->format->extradata_size = READ_BYTES(p_ctx, track->format->extradata, size);

      if(track->format->extradata_size >= 2)
      {
         p_data = track->format->extradata;
         data = ((p_data[0] & 0x7) << 1) | (p_data[1] >> 7);
         if(data >= countof(aac_freq))
            return VC_CONTAINER_ERROR_FAILED;

         samplerate = aac_freq[data];
         channels = (p_data[1] >> 3) & 0xf;
         if(track->format->extradata_size >= 5 && data == 0xf)
         {
            samplerate = ((p_data[1] & 0x7f) << 17) | (p_data[2] << 9) |
                          (p_data[3] << 1) | (p_data[4] >> 7);
            channels = (p_data[4] >> 3) & 0xf;
         }
      }
   }

   track->format->type->audio.sample_rate = samplerate;
   track->format->type->audio.channels = channels;
   track->format->type->audio.bits_per_sample = bps;

   return STREAM_STATUS(p_ctx);
}

/** Reads an FLV metadata tag.
  * This contains metadata information about the stream.
  * All the data we extract from this will be placed directly in the context.
  *
  * @param p_ctx              pointer to our context
  * @param size               size of the tag
  * @return                   FLV_FILE_NO_ERROR on success
  */
static int flv_read_metadata(VC_CONTAINER_T *p_ctx, int size)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
#define MAX_METADATA_STRING_SIZE 25
   char psz_string[MAX_METADATA_STRING_SIZE+1];
   uint16_t length, num_values;
   double f_value;
   uint64_t u_value;
   uint8_t type;

   /* We're looking for an onMetaData script */
   type = READ_U8(p_ctx, "Type"); size--;
   if(type != FLV_SCRIPT_DATA_TYPE_STRING) return VC_CONTAINER_SUCCESS;
   length = READ_U16(p_ctx, "StringLength"); size -= 2;
   if(!length || length > size || length > MAX_METADATA_STRING_SIZE) return VC_CONTAINER_SUCCESS;
   if(READ_BYTES(p_ctx, psz_string, length) != length) return VC_CONTAINER_SUCCESS;
   psz_string[length] = 0; size -= length;
   if(strcmp(psz_string, "onMetaData")) return VC_CONTAINER_SUCCESS;
   if(size < 5) return VC_CONTAINER_SUCCESS;
   type = READ_U8(p_ctx, "Type"); size--;
   if(type != FLV_SCRIPT_DATA_TYPE_ECMA) return VC_CONTAINER_SUCCESS;
   num_values = READ_U32(p_ctx, "ECMAArrayLength"); size -= 4;

   /* We found our script, now extract the metadata values */
   while(num_values-- && size >= 2)
   {
      uint16_t length = _READ_U16(p_ctx); size -= 2;
      if(!length || length >= size || length > MAX_METADATA_STRING_SIZE) break;
      if(READ_BYTES(p_ctx, psz_string, length) != length) break;
      psz_string[length] = 0; size -= length;
      type = _READ_U8(p_ctx); size--;

      switch(type)
      {
      case FLV_SCRIPT_DATA_TYPE_NUMBER:
         /* We only cope with DOUBLE types*/
         if(size < 8) return VC_CONTAINER_SUCCESS;

         u_value = _READ_U64(p_ctx); size -= 8;
         /* Convert value into a double */
         {
            int64_t value = ((u_value & ((UINT64_C(1)<<52)-1)) + (UINT64_C(1)<<52)) * ((((int64_t)u_value)>>63)|1);
            int exp = ((u_value>>52)&0x7FF)-1075 + 16;
            if(exp >= 0) value <<= exp;
            else value >>= -exp;
            f_value = ((double)value) / (1 << 16);
         }

         LOG_DEBUG(p_ctx, "metadata (%s=%i.%i)", psz_string,
                   ((int)(f_value*100))/100, ((int)(f_value*100))%100);

         if(!strcmp(psz_string, "duration"))
            p_ctx->duration = (int64_t)(f_value*INT64_C(1000000));
         if(!strcmp(psz_string, "videodatarate"))
            module->meta_videodatarate = (uint32_t)f_value;
         if(!strcmp(psz_string, "width"))
            module->meta_width = (uint32_t)f_value;
         if(!strcmp(psz_string, "height"))
            module->meta_height = (uint32_t)f_value;
         if(!strcmp(psz_string, "framerate"))
            module->meta_framerate = f_value;
         if(!strcmp(psz_string, "audiodatarate"))
            module->meta_audiodatarate = (uint32_t)f_value;
         continue;

      /* We skip these */
      case FLV_SCRIPT_DATA_TYPE_BOOL:
         if(size < 1) return VC_CONTAINER_SUCCESS;
         u_value = _READ_U8(p_ctx); size -= 1;
         LOG_DEBUG(p_ctx, "metadata (%s=%i)", psz_string, (int)u_value);
         continue;

      case FLV_SCRIPT_DATA_TYPE_STRING:
         if(size < 2) return VC_CONTAINER_SUCCESS;
         length = _READ_U16(p_ctx); size -= 2;
         if(length > size) return VC_CONTAINER_SUCCESS;
         SKIP_BYTES(p_ctx, length); size -= length;
         LOG_DEBUG(p_ctx, "metadata skipping (%s)", psz_string);
         continue;

      /* We can't cope with anything else */
      default:
         LOG_DEBUG(p_ctx, "unknown amf type (%s,%i)", psz_string, type);
         return VC_CONTAINER_SUCCESS;
      }
   }

   return STREAM_STATUS(p_ctx);
}

/** Reads an FLV frame header.
  * This reads the current tag header and matches the contained frame
  * with one of the tracks we have. If no match can be found, the frame is marked
  * for discarding. The current read position will be updated to the start
  * of the data (i.e. the frame) contained within the FLV tag.
  *
  * @param p_ctx              pointer to our context
  * @param[out] p_track       track this frame belongs to
  * @param[out] p_size        size of the frame
  * @param[out] p_timestamp   timestamp of the frame
  * @param[out] p_flags       flags associated with the frame
  * @param b_extra_check      whether to perform extra sanity checking on the tag
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static int flv_read_frame_header(VC_CONTAINER_T *p_ctx, int *p_prev_size,
   int *p_track, int *p_size, uint32_t *p_timestamp, int *p_flags,
   int b_extra_check)
{
   int64_t position = STREAM_POSITION(p_ctx);
   int type, size, flags = 0, frametype = 0;
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_ES_TYPE_T es_type = VC_CONTAINER_ES_TYPE_UNKNOWN;
   unsigned int track = p_ctx->tracks_num;
   uint32_t codec = 0;

   status = flv_read_tag_header(p_ctx, p_prev_size, &type, &size, p_timestamp);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) return status;
   if(position == STREAM_POSITION(p_ctx)) return VC_CONTAINER_ERROR_EOS;
   if(type == 0) return VC_CONTAINER_ERROR_CORRUPTED;

   /* Sanity checking */
   if(b_extra_check && type != FLV_TAG_TYPE_AUDIO &&
      type != FLV_TAG_TYPE_VIDEO && type != FLV_TAG_TYPE_METADATA)
      return VC_CONTAINER_ERROR_CORRUPTED;

   /* We're only interested in audio / video */
   if((type != FLV_TAG_TYPE_AUDIO && type != FLV_TAG_TYPE_VIDEO) || !size)
   {
      flags |= FLV_FLAG_DISCARD;
      goto end;
   }

   if(type == FLV_TAG_TYPE_AUDIO)
   {
      flv_read_audiodata_header(p_ctx, &codec, 0, 0, 0);
      es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   }
   else if(type == FLV_TAG_TYPE_VIDEO)
   {
      flv_read_videodata_header(p_ctx, &codec, &frametype);
      es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   }
   size--;

   /* Find which track this belongs to */
   for(track = 0; track < p_ctx->tracks_num; track++)
      if(es_type == p_ctx->tracks[track]->format->es_type) break;
   if(track == p_ctx->tracks_num)
      flags |= FLV_FLAG_DISCARD;

   /* Sanity checking */
   if(b_extra_check && codec != p_ctx->tracks[track]->format->codec)
      return VC_CONTAINER_ERROR_CORRUPTED;

 end:
   // add to the index if we have one, and we're not discarding this frame.
   // also require that we either have no video track or this is a video frame marked as a key frame.
   if(p_ctx->priv->module->state.index && !(flags & FLV_FLAG_DISCARD) &&
      (p_ctx->priv->module->video_track < 0 || (es_type == VC_CONTAINER_ES_TYPE_VIDEO && (frametype & FLV_FLAG_KEYFRAME))))
      vc_container_index_add(p_ctx->priv->module->state.index, (int64_t) (*p_timestamp) * 1000LL, position);

   *p_flags = flags | frametype;
   *p_size = size;
   *p_track = track;
   return VC_CONTAINER_SUCCESS;
}

/** Validate the data contained within the frame and update the read
  * position to the start of the frame data that we want to feed to the codec.
  *
  * Each codec is packed slightly differently so this function is necessary
  * to prepare for reading the actual codec data.
  *
  * @param p_ctx              pointer to our context
  * @param track              track this frame belongs to
  * @param[in] p_size         size of the frame
  * @param[out] p_size        updated size of the frame
  * @param[in] p_timestamp    timestamp for the frame
  * @param[out] p_timestamp   updated timestamp for the frame
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_validate_frame_data(VC_CONTAINER_T *p_ctx,
   int track, int *p_size, uint32_t *p_timestamp)
{
   int32_t time_offset;

   if(track >= (int)p_ctx->tracks_num)
      return *p_size ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_CONTINUE;

   switch(p_ctx->tracks[track]->format->codec)
   {
   case VC_CONTAINER_CODEC_VP6:
      if(*p_size < 1) return VC_CONTAINER_ERROR_CORRUPTED;
      _READ_U8(p_ctx); *p_size -= 1;
      break;
   case VC_CONTAINER_CODEC_MP4A:
      if(*p_size < 1) return VC_CONTAINER_ERROR_CORRUPTED;
      *p_size -= 1;
      if(_READ_U8(p_ctx)!=1) return VC_CONTAINER_ERROR_CONTINUE; /* empty frame*/
      break;
   case VC_CONTAINER_CODEC_H264:
      if(*p_size < 4) return VC_CONTAINER_ERROR_CORRUPTED;
      *p_size -= 1;
      if(_READ_U8(p_ctx)!=1) return VC_CONTAINER_ERROR_CONTINUE; /* empty frame*/
      time_offset = _READ_U24(p_ctx);
      time_offset <<= 8; time_offset >>= 8; /* change to signed */
      *p_timestamp += time_offset;
      *p_size -= 3;
      break;
   default:
      break;
   }

   return *p_size ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_CONTINUE;
}

/** Small utility function to update the reading position of a track
  */
static void flv_update_track_position(VC_CONTAINER_T *p_ctx, int track,
   int64_t tag_position, int tag_prev_size, int64_t data_position,
   int data_size, uint32_t timestamp, int flags)
{
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[track]->priv->module;
   track_module->state->tag_position = tag_position;
   track_module->state->tag_prev_size = tag_prev_size;
   track_module->state->data_position = data_position;
   track_module->state->data_size = data_size;
   track_module->state->data_offset = 0;
   track_module->state->timestamp = timestamp;
   track_module->state->flags = flags;
   track_module->state->track = track;
}

/** Utility function to find the next frame of a given track in the stream.
  *
  * This will basically walk through all the tags in the file until it
  * finds a tag/frame which belongs to the given track.
  *
  * @param p_ctx              pointer to our context
  * @param track              track wanted
  * @param[out] p_size        size of the frame
  * @param[out] p_timestamp   timestamp of the frame
  * @param[out] p_flags       flags associated with the frame
  * @param b_keyframe         whether we specifically want a keyframe or not
  * @param b_extra_check      whether to perform extra sanity checking on the tag
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_find_next_frame(VC_CONTAINER_T *p_ctx, int track, int *p_size,
   uint32_t *p_timestamp, int *p_flags, int b_keyframe, int b_extra_check)
{
   int frame_track, prev_size, size, flags;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   FLV_READER_STATE_T *state = p_ctx->tracks[track]->priv->module->state;
   uint32_t timestamp;
   int64_t position;
   VC_CONTAINER_PARAM_UNUSED(b_extra_check);

   /* Seek to the next tag in the stream or the current position
    * if none of its data has been consumed */
   position = state->tag_position;
   if(state->data_offset)
      position = state->data_position + state->data_size;
   status = SEEK(p_ctx, position);
   if(status != VC_CONTAINER_SUCCESS) return status;

   /* Look for the next frame we want */
   while (status == VC_CONTAINER_SUCCESS)
   {
      position = STREAM_POSITION(p_ctx);
      status = flv_read_frame_header(p_ctx, &prev_size, &frame_track,
                                     &size, &timestamp, &flags, 0);
      if(status != VC_CONTAINER_SUCCESS) break;

      if(flags & FLV_FLAG_DISCARD) goto skip;
      if(frame_track != track) goto skip;

      if(b_keyframe && p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO &&
         !(flags & FLV_FLAG_KEYFRAME)) goto skip;

      if(flv_validate_frame_data(p_ctx, track, &size, &timestamp) != VC_CONTAINER_SUCCESS)
         goto skip;

      /* We have what we need */
      flv_update_track_position(p_ctx, track, position, prev_size, STREAM_POSITION(p_ctx),
                                size, timestamp, flags);
      break;

   skip:
      flv_update_track_position(p_ctx, track, position, prev_size, STREAM_POSITION(p_ctx),
                                size, timestamp, 0);
      state->data_offset = size; /* consume data */

      if(SKIP_BYTES(p_ctx, size) != (size_t)size) status = STREAM_STATUS(p_ctx);
   }

   if(!status)
   {
      if(p_size) *p_size = size;
      if(p_timestamp) *p_timestamp = timestamp;
      if(p_flags) *p_flags = flags;
   }

   return status;
}

/** Utility function to find the previous frame of a given track in the stream.
  *
  * This will basically walk back through all the tags in the file until it
  * finds a tag/frame which belongs to the given track.
  *
  * @param p_ctx              pointer to our context
  * @param track              track wanted
  * @param[out] p_size        size of the frame
  * @param[out] p_timestamp   timestamp of the frame
  * @param[out] p_flags       flags associated with the frame
  * @param b_keyframe         whether we specifically want a keyframe or not
  * @param b_extra_check      whether to perform extra sanity checking on the tag
  * @return                   VC_CONTAINER_SUCCESS on success
  */
static VC_CONTAINER_STATUS_T flv_find_previous_frame(VC_CONTAINER_T *p_ctx, int track, int *p_size,
   uint32_t *p_timestamp, int *p_flags, int b_keyframe, int b_extra_check)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   FLV_READER_STATE_T *state = p_ctx->tracks[track]->priv->module->state;
   int frame_track, prev_size, size, flags;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t timestamp;
   int64_t position;

   /* Look for the previous frame we want */
   while (status == VC_CONTAINER_SUCCESS)
   {
      /* Seek to the previous tag in the stream */
      position = state->tag_position - state->tag_prev_size;
      if(position < module->data_offset) position = module->data_offset;
      status = SEEK(p_ctx, position);
      if(status != VC_CONTAINER_SUCCESS) return status;

      status = flv_read_frame_header(p_ctx, &prev_size, &frame_track,
                                     &size, &timestamp, &flags, 0);
      if(status) break;

      if(flags & FLV_FLAG_DISCARD) goto skip;
      if(frame_track != track) goto skip;

      if(b_keyframe && p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO &&
         !(flags & FLV_FLAG_KEYFRAME)) goto skip;

      if(flv_validate_frame_data(p_ctx, track, &size, &timestamp) != VC_CONTAINER_SUCCESS)
         goto skip;

      /* We have what we need */
      flv_update_track_position(p_ctx, track, position, prev_size, STREAM_POSITION(p_ctx),
                                size, timestamp, flags);
      break;

   skip:
      if(position <= module->data_offset)
      {
         /* We're back at the beginning but we still want to return something */
         flv_update_track_position(p_ctx, track, (int64_t)module->data_offset, 0,
                                   (int64_t)module->data_offset, 0, 0, 0);
         return flv_find_next_frame(p_ctx, track, p_size, p_timestamp, p_flags, b_keyframe, b_extra_check);
      }
      flv_update_track_position(p_ctx, track, position, prev_size, STREAM_POSITION(p_ctx),
                                size, timestamp, 0);
      state->data_offset = size; /* consume data */
   }

   if(!status)
   {
      if(p_size) *p_size = size;
      if(p_timestamp) *p_timestamp = timestamp;
      if(p_flags) *p_flags = flags;
   }
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T flv_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);

   if(module->state.index)
      vc_container_index_free(module->state.index);

   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T flv_read_sample_header( VC_CONTAINER_T *p_ctx,
   FLV_READER_STATE_T *state)
{
   int track, prev_size, size, flags;
   uint32_t timestamp;
   int64_t position;

   /* Check if we still have some data left to read from the current frame */
   if(state->data_offset < state->data_size)
      return state->status;

   /* Read the next tag header */
   position = STREAM_POSITION(p_ctx);
   state->status = flv_read_frame_header(p_ctx, &prev_size, &track,
                                         &size, &timestamp, &flags, 0);
   if(state->status != VC_CONTAINER_SUCCESS)
      return state->status;

   state->status = flv_validate_frame_data(p_ctx, track, &size, &timestamp);
   if(state->status == VC_CONTAINER_ERROR_CONTINUE)
   {
      /* Skip it */
      state->status = VC_CONTAINER_SUCCESS;
      track = p_ctx->tracks_num;
   }
   if(state->status != VC_CONTAINER_SUCCESS)
      return state->status;

   state->tag_position = position;
   state->data_position = STREAM_POSITION(p_ctx);
   state->data_size = size;
   state->data_offset = 0;
   state->flags = flags;
   state->tag_prev_size = prev_size;
   state->timestamp = timestamp;
   state->track = track;
   return state->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T flv_read_sample_data( VC_CONTAINER_T *p_ctx,
   FLV_READER_STATE_T *state, uint8_t *data, unsigned int *data_size )
{
   unsigned int size = state->data_size - state->data_offset;

   if(state->status != VC_CONTAINER_SUCCESS) return state->status;

   if(data_size && *data_size < size) size = *data_size;

   if(!data) size = SKIP_BYTES(p_ctx, size);
   else size = READ_BYTES(p_ctx, data, size);
   state->data_offset += size;

   if(data_size) *data_size = size;
   state->status = STREAM_STATUS(p_ctx);

   return state->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T flv_reader_read( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   FLV_READER_STATE_T *state = &module->state;
   unsigned int data_size;

   /* TODO: select right state */

   status = flv_read_sample_header(p_ctx, state);
   if(status != VC_CONTAINER_SUCCESS) return status;

#ifdef ENABLE_FLV_EXTRA_LOGGING
   LOG_DEBUG(p_ctx, "read_sample_header (%i,%i,%i/%i/%i/%i)", state->timestamp, state->flags,
         (int)state->tag_position, (int)(state->data_position-state->tag_position), state->data_offset, state->data_size);
#endif

   if(state->track >= p_ctx->tracks_num || !p_ctx->tracks[state->track]->is_enabled)
   {
      /* Skip packet */
      status = flv_read_sample_data(p_ctx, state, 0, 0);
      if(status != VC_CONTAINER_SUCCESS) return status;
      return VC_CONTAINER_ERROR_CONTINUE;
   }

   if((flags & VC_CONTAINER_READ_FLAG_SKIP) && !(flags & VC_CONTAINER_READ_FLAG_INFO)) /* Skip packet */
      return flv_read_sample_data(p_ctx, state, 0, 0);

   packet->dts = packet->pts = state->timestamp * (int64_t)1000;
   packet->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END;
   if(state->flags & FLV_FLAG_KEYFRAME) packet->flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;
   if(!state->data_offset) packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   packet->track = state->track;

   // The frame size is all the data
   packet->frame_size = state->data_size;

   // the size is what's left
   packet->size = state->data_size - state->data_offset;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
      return flv_read_sample_data(p_ctx, state, 0, 0);
   else if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   data_size = packet->buffer_size;
   status = flv_read_sample_data(p_ctx, state, packet->data, &data_size);
   if(status != VC_CONTAINER_SUCCESS)
   {
      /* FIXME */
      return status;
   }

   packet->size = data_size;
   if(state->data_offset != state->data_size)
      packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T flv_reader_seek(VC_CONTAINER_T *p_ctx,
   int64_t *offset, VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   FLV_READER_STATE_T last_state = {0};
   FLV_READER_STATE_T *state;
   uint32_t time = (*offset / 1000), timestamp, previous_time;
   unsigned int i, track;
   int size, past = 0;
   int64_t position;
   VC_CONTAINER_PARAM_UNUSED(mode);

   /* If we have a video track, then we want to find the keyframe closest to
    * the requested time, otherwise we just look for the tag with the
    * closest timestamp */

   /* Select the track on which we'll do our seeking */
   for(i = 0, track = 0; i < p_ctx->tracks_num; i++)
   {
      if(p_ctx->tracks[i]->format->es_type != VC_CONTAINER_ES_TYPE_VIDEO) continue;
      track = i;
      break;
   }
   if(track >= p_ctx->tracks_num) return VC_CONTAINER_ERROR_CORRUPTED;
   state = p_ctx->tracks[track]->priv->module->state;
   previous_time = state->timestamp;

   LOG_DEBUG(p_ctx, "seek (%i, prev %i)", time, previous_time);

   if(state->index && vc_container_index_get(state->index, flags & VC_CONTAINER_SEEK_FLAG_FORWARD,
                                             offset, &position, &past) == VC_CONTAINER_SUCCESS)
   {
      flv_update_track_position(p_ctx, track, position, 0, position, 0, (uint32_t) (*offset / 1000LL), 0);
   }
   else
   {
      if(time < state->timestamp / 2)
         flv_update_track_position(p_ctx, track, (int64_t)module->data_offset, 0,
                                   (int64_t)module->data_offset, 0, 0, 0);
      past = 1;
   }

   /* If past it clear then we're done, otherwise we need to find our point from here */
   if(past == 0)
   {
      status = flv_find_next_frame(p_ctx, track, &size, &timestamp, 0, 1 /*keyframe*/, 0);
   }
   else
   {
      if(time > previous_time)
      {
         while(!status)
         {
            status = flv_find_next_frame(p_ctx, track, &size, &timestamp, 0, 1 /*keyframe*/, 0);
            if(status) break;

            /* Check if we have our frame */
            if(time <= timestamp) break;

            last_state = *state;
            state->data_offset = size; /* consume data */
         }
      }
      else
      {
         while(!status)
         {
            status = flv_find_previous_frame(p_ctx, track, &size, &timestamp, 0, 1 /*keyframe*/, 0);
            if(status) break;

            /* Check if we have our frame */
            if(time >= timestamp) break;

            /* Detect when we've reached the 1st keyframe to avoid an infinite loop */
            if(state->timestamp == last_state.timestamp) break;

            last_state = *state;
            state->data_offset = size; /* consume data */
         }
      }
   }

   if(status != VC_CONTAINER_SUCCESS && (flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
   {
      LOG_DEBUG(p_ctx, "seek failed (%i)", status);
      return status;
   }
   else if(status != VC_CONTAINER_SUCCESS)
   {
      LOG_DEBUG(p_ctx, "seek failed (%i), look for previous frame", status);
      if(last_state.tag_position) *state = last_state;
      else status = flv_find_previous_frame(p_ctx, track, &size, &timestamp, 0, 1 /*keyframe*/, 0);
   }

   LOG_DEBUG(p_ctx, "seek done (%i)", timestamp);
   state->status = VC_CONTAINER_SUCCESS;
   last_state.status = VC_CONTAINER_SUCCESS;

   if(past == 1)
   {
      /* Make adjustment based on seek mode */
      if((flags & VC_CONTAINER_SEEK_FLAG_FORWARD) && timestamp < time && timestamp < previous_time)
      {
         if(last_state.tag_position) *state = last_state;
         else status = flv_find_next_frame(p_ctx, track, &size, &timestamp, 0, 1 /*keyframe*/, 0);
      }
      else if(!(flags & VC_CONTAINER_SEEK_FLAG_FORWARD) && timestamp > time)
      {
         if(last_state.tag_position) *state = last_state;
         else status = flv_find_previous_frame(p_ctx, track, &size, &timestamp, 0, 1 /*keyframe*/, 0);
      }

      LOG_DEBUG(p_ctx, "seek adjustment (%i)", timestamp);
   }

   if(state->data_position == last_state.data_position)
      status = SEEK(p_ctx, state->data_position);

   *offset = timestamp * INT64_C(1000);

   return VC_CONTAINER_SUCCESS;
}

/******************************************************************************
Global function definitions.
******************************************************************************/

VC_CONTAINER_STATUS_T flv_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = 0;
   uint8_t buffer[4], type_flags;
   unsigned int i, frames, audio_present, video_present;
   uint32_t data_offset;

   /* Check the FLV marker */
   if( PEEK_BYTES(p_ctx, buffer, 4) < 4 ) goto error;
   if( buffer[0] != 'F' || buffer[1] != 'L' || buffer[2] != 'V' )
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   /* Check FLV version */
   if( buffer[3] > 4 )
   {
      LOG_DEBUG(p_ctx, "Version too high: %d", buffer[3]);
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   SKIP_BYTES(p_ctx, 4); /* FLV marker and version */

   /* Find out which tracks should be available.
    * FLV can only have up to 1 audio track and 1 video track. */
   type_flags = READ_U8(p_ctx, "TypeFlags");
   audio_present = !!(type_flags & 0x04);
   video_present = !!(type_flags & 0x01);

   /* Sanity check DataOffset */
   data_offset = READ_U32(p_ctx, "DataOffset");
   if(data_offset < 9) goto error;

   /*
    *  We are dealing with an FLV file
    */

   LOG_DEBUG(p_ctx, "using flv reader");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;
   module->data_offset = data_offset;
   module->audio_track = -1;
   module->video_track = -1;

   /* Skip to the start of the actual data */
   SKIP_BYTES(p_ctx, data_offset - 9);

   /* We'll start parsing a few of the FLV tags to find out the
    * metadata / audio / video properties.
    * The first tag we should see is the metadata one which will give us all the
    * properties of the stream. However we do not rely on that being there and we
    * actually look at the first audio / video tags as well. */
   for(frames = 0; frames < 20; frames++)
   {
      VC_CONTAINER_TRACK_T *track;
      int64_t offset, skip;
      int prev_size, type, size, channels, samplerate, bps;
      uint32_t codec, timestamp;

      /* Stop there if we have everything we want */
      if(audio_present == (module->audio_track >= 0) &&
         video_present == (module->video_track >= 0)) break;
      if(module->audio_track >= 0 && module->video_track >= 0) break;

      /* Start reading the next tag */
      if(flv_read_tag_header(p_ctx, &prev_size, &type, &size, &timestamp)) break;
      if(!size) continue;

      offset = STREAM_POSITION(p_ctx); /* to keep track of how much data we read */

      switch(type)
      {
      case FLV_TAG_TYPE_AUDIO:
         if(module->audio_track >= 0) break; /* We already have our audio track */
         flv_read_audiodata_header(p_ctx, &codec, &samplerate, &channels, &bps);

         p_ctx->tracks[p_ctx->tracks_num] = track =
            vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
         if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

         track->format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
         track->format->codec = codec;
         flv_read_audiodata_properties(p_ctx, track, size - 1, samplerate, channels, bps);

         module->audio_track = p_ctx->tracks_num++;
         track->is_enabled = 1;
         track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         break;

      case FLV_TAG_TYPE_VIDEO:
         if(module->video_track >= 0) break; /* We already have our video track */
         flv_read_videodata_header(p_ctx, &codec, 0);

         p_ctx->tracks[p_ctx->tracks_num] = track =
            vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
         if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

         track->format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
         track->format->codec = codec;

         status = flv_read_videodata_properties(p_ctx, track, size - 1);
         if(status != VC_CONTAINER_SUCCESS) { vc_container_free_track(p_ctx, track); break; }

         module->video_track = p_ctx->tracks_num++;
         track->is_enabled = 1;
         track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         break;

      case FLV_TAG_TYPE_METADATA:
         flv_read_metadata(p_ctx, size);
         break;

      default: break;
      }

      /* Skip any data that's left unparsed from the current tag */
      skip = size - (STREAM_POSITION(p_ctx) - offset);
      if(skip < 0) break;
      SKIP_BYTES(p_ctx, (size_t)skip);
   }

   /* Make sure we found something we can play */
   if(!p_ctx->tracks_num) {LOG_DEBUG(p_ctx, "didn't find any track"); goto error;}

   /* Try and create an index.  All times are signed, so adding a base timestamp
    * of zero means that we will always seek back to the start of the file, even if
    * the actual frame timestamps start at some higher number. */
   if(vc_container_index_create(&module->state.index, 512) == VC_CONTAINER_SUCCESS)
      vc_container_index_add(module->state.index, 0LL, (int64_t) data_offset);

   /* Use the metadata we read */
   if(module->audio_track >= 0)
   {
      VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->audio_track];
      track->format->bitrate = module->meta_audiodatarate;
   }
   if(module->video_track >= 0)
   {
      VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->video_track];
      track->format->bitrate = module->meta_videodatarate;
      if(module->meta_framerate)
      {
         track->format->type->video.frame_rate_num = (uint32_t)(100 * module->meta_framerate);
         track->format->type->video.frame_rate_den = 100;
      }

      if(module->meta_width && module->meta_width > track->format->type->video.width)
         track->format->type->video.width = module->meta_width;
      if(module->meta_height && module->meta_height > track->format->type->video.height)
         track->format->type->video.height = module->meta_height;
   }

   status = SEEK(p_ctx, data_offset);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   /* Some initialisation */
   module->state.tag_position = data_offset;
   module->state.data_position = data_offset;
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[i]->priv->module;
      track_module->state = &module->state;
   }

   if(STREAM_SEEKABLE(p_ctx))
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   p_ctx->priv->pf_close = flv_reader_close;
   p_ctx->priv->pf_read = flv_reader_read;
   p_ctx->priv->pf_seek = flv_reader_seek;

   return VC_CONTAINER_SUCCESS;

 error:
   if(status == VC_CONTAINER_SUCCESS) status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   LOG_DEBUG(p_ctx, "flv: error opening stream");
   if(module) flv_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open flv_reader_open
#endif
