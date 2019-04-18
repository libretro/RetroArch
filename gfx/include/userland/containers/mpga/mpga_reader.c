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

#define CONTAINER_IS_BIG_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define CONTAINER_HELPER_LOG_INDENT(a) 0
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"
#include "mpga_common.h"

/******************************************************************************
Defines and constants.
******************************************************************************/
#define MPGA_XING_HAS_FRAMES   0x00000001
#define MPGA_XING_HAS_BYTES    0x00000002
#define MPGA_XING_HAS_TOC      0x00000004
#define MPGA_XING_HAS_QUALITY  0x00000008

#define MPGA_MAX_BAD_FRAMES    4096 /*< Maximum number of failed byte-wise syncs,
                                        should be at least 2881+4 to cover the largest
                                        frame size (MPEG2.5 Layer 2, 160kbit/s 8kHz)
                                        + next frame header */

static const unsigned int mpga_sample_rate_adts[16] =
{96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};

static const GUID_T asf_guid_header =
{0x75B22630, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   uint64_t data_offset;
   uint64_t data_size;
   uint64_t num_frames;             /**< Total number of frames (if known) */
   unsigned int frame_size_samples; /**< Frame size in samples */
   unsigned int bitrate;            /**< Bitrate (might change on a per-frame basis if VBR) */
   unsigned int sample_rate;
   unsigned int channels;

   /* MPEG audio header information */
   unsigned int version; /**< 1 for MPEG1, 2 for MPEG2, etc. */
   unsigned int layer;

   /* VBR header information */
   uint8_t xing_toc[100];
   int xing_toc_valid;

   /* Per-frame state (updated upon a read or a seek) */
   unsigned int frame_size;
   unsigned int frame_data_left;
   uint64_t frame_index;
   int64_t frame_offset;
   int64_t  frame_time_pos;         /**< pts of current frame */
   unsigned int frame_bitrate;      /**< bitrate of current frame */

   VC_CONTAINER_STATUS_T (*pf_parse_header)( uint8_t frame_header[MPGA_HEADER_SIZE],
      uint32_t *p_frame_size, unsigned int *p_frame_bitrate, unsigned int *p_version,
      unsigned int *p_layer, unsigned int *p_sample_rate, unsigned int *p_channels,
      unsigned int *p_frame_size_samples, unsigned int *p_offset);

   uint8_t extradata[2];            /**< codec extra data for aac */

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T mpga_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static uint32_t PEEK_BYTES_AT( VC_CONTAINER_T *p_ctx, int64_t offset, uint8_t *buffer, int size )
{
   int ret;
   int64_t current_position = STREAM_POSITION(p_ctx);
   SEEK(p_ctx, current_position + offset);
   ret = PEEK_BYTES(p_ctx, buffer, size);
   SEEK(p_ctx, current_position);
   return ret;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_check_frame_header( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_MODULE_T *module, uint8_t frame_header[MPGA_HEADER_SIZE] )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return module->pf_parse_header(frame_header, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_sync( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint8_t frame_header[MPGA_HEADER_SIZE];
   uint32_t frame_size;
   unsigned int frame_bitrate, version, layer, sample_rate, channels;
   unsigned int frame_size_samples, offset;
   int sync_count = 0;

   /* If we can't see a full frame header, we treat this as EOS although it
      could be a bad stream as well, the caller should distinct between
      these two cases */
   if (PEEK_BYTES(p_ctx, (uint8_t*)frame_header, MPGA_HEADER_SIZE) != MPGA_HEADER_SIZE)
      return VC_CONTAINER_ERROR_EOS;

   while (sync_count++ < MPGA_MAX_BAD_FRAMES)
   {
      status = module->pf_parse_header(frame_header, &frame_size, &frame_bitrate,
                                       &version, &layer, &sample_rate, &channels,
                                       &frame_size_samples, &offset);
      if (status == VC_CONTAINER_SUCCESS &&
          frame_size /* We do not support free format streams */)
      {
         LOG_DEBUG(p_ctx, "MPEGv%d, layer %d, %d bps, %d Hz",
                   version, layer, frame_bitrate, sample_rate);
         if (PEEK_BYTES_AT(p_ctx, (int64_t)frame_size, frame_header, MPGA_HEADER_SIZE) != MPGA_HEADER_SIZE ||
             mpga_check_frame_header(p_ctx, module, frame_header) == VC_CONTAINER_SUCCESS)
            break;

         /* If we've reached an ID3 tag then the frame is valid as well */
         if((frame_header[0] == 'I' && frame_header[1] == 'D' && frame_header[2] == '3') ||
            (frame_header[0] == 'T' && frame_header[1] == 'A' && frame_header[2] == 'G'))
            break;
      }
      else if (status == VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "free format not supported");
      }

      if (SKIP_BYTES(p_ctx, 1) != 1 || PEEK_BYTES(p_ctx, (uint8_t*)frame_header, MPGA_HEADER_SIZE) != MPGA_HEADER_SIZE)
         return VC_CONTAINER_ERROR_EOS;
   }

   if(sync_count > MPGA_MAX_BAD_FRAMES) /* We didn't find a valid frame */
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   if (module->version)
   {
      /* FIXME: we don't currently care whether or not the number of channels changes mid-stream */
      if (version != module->version || layer != module->layer)
      {
         LOG_DEBUG(p_ctx, "version or layer not allowed to change mid-stream");
         return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      }
   }
   else
   {
      module->version = version;
      module->layer = layer;
      module->sample_rate = sample_rate;
      module->channels = channels;
      module->frame_size_samples = frame_size_samples;
   }

   if(offset) SKIP_BYTES(p_ctx, offset);
   module->frame_data_left = module->frame_size = frame_size - offset;
   module->frame_bitrate = frame_bitrate;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static int64_t mpga_calculate_frame_time( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   int64_t time;
   time = INT64_C(1000000) * module->frame_index *
      module->frame_size_samples / module->sample_rate;
   return time;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_read_vbr_headers( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[0];
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_NOT_FOUND;
   uint32_t peek_buf[1];
   int64_t offset, start = STREAM_POSITION(p_ctx);

   /* Look for XING header (immediately after layer 3 side information) */
   offset = (module->version == 1) ? ((module->channels == 1) ?  INT64_C(21) :  INT64_C(36)) :
      ((module->channels == 1) ?  INT64_C(13) :  INT64_C(21));

   if (PEEK_BYTES_AT(p_ctx, offset, (uint8_t*)peek_buf, 4) != 4)
      return VC_CONTAINER_ERROR_FORMAT_INVALID; /* File would be way too small */

   if (peek_buf[0] == VC_FOURCC('X','i','n','g') || peek_buf[0] == VC_FOURCC('I','n','f','o'))
   {
      uint32_t flags = 0, num_frames = 0, data_size = 0;

      /* If the first frame has a XING header then we know it's a valid (but empty) audio
         frame so we safely parse the header whilst skipping to the next frame */
      SKIP_BYTES(p_ctx, offset); /* FIXME: we don't care about layer 3 side information? */

      SKIP_FOURCC(p_ctx, "XING");
      flags = READ_U32(p_ctx, "XING flags");

      if (flags & MPGA_XING_HAS_FRAMES)
         num_frames = READ_U32(p_ctx, "XING frames");

      if (flags & MPGA_XING_HAS_BYTES)
         data_size = READ_U32(p_ctx, "XING bytes");

      if (flags & MPGA_XING_HAS_TOC)
      {
         READ_BYTES(p_ctx, module->xing_toc, sizeof(module->xing_toc));
         /* TOC is useful only if we know the number of frames */
         if (num_frames) module->xing_toc_valid = 1;
         /* Ensure time zero points to first frame even if TOC is broken */
         module->xing_toc[0] = 0;
      }

      if (flags & MPGA_XING_HAS_QUALITY)
         SKIP_U32(p_ctx, "XING quality");

      module->data_size = data_size;
      module->num_frames = num_frames;

      if (module->num_frames && module->data_size)
      {
         /* We can calculate average bitrate */
         module->bitrate =
            module->data_size * module->sample_rate * 8 / (module->num_frames * module->frame_size_samples);
      }

      p_ctx->duration = (module->num_frames * module->frame_size_samples * 1000000LL) / module->sample_rate;

      /* Look for additional LAME header (follows XING) */
      if (PEEK_BYTES(p_ctx, (uint8_t*)peek_buf, 4) != 4)
         return VC_CONTAINER_ERROR_FORMAT_INVALID; /* File would still be way too small */

      if (peek_buf[0] == VC_FOURCC('L','A','M','E'))
      {
         uint32_t encoder_delay;

         SKIP_FOURCC(p_ctx, "LAME");
         SKIP_STRING(p_ctx, 5, "LAME encoder version");
         SKIP_U8(p_ctx, "LAME tag revision/VBR method");
         SKIP_U8(p_ctx, "LAME LP filter value");
         SKIP_U32(p_ctx, "LAME peak signal amplitude");
         SKIP_U16(p_ctx, "LAME radio replay gain");
         SKIP_U16(p_ctx, "LAME audiophile replay gain");
         SKIP_U8(p_ctx, "LAME encoder flags");
         SKIP_U8(p_ctx, "LAME ABR/minimal bitrate");
         encoder_delay = READ_U24(p_ctx, "LAME encoder delay/padding");
         SKIP_U8(p_ctx, "LAME misc");
         SKIP_U8(p_ctx, "LAME MP3 gain");
         SKIP_U16(p_ctx, "LAME presets and surround info");
         SKIP_U32(p_ctx, "LAME music length");
         SKIP_U16(p_ctx, "LAME music CRC");
         SKIP_U16(p_ctx, "LAME tag CRC");
         track->format->type->audio.gap_delay = (encoder_delay >> 12) + module->frame_size_samples;
         track->format->type->audio.gap_padding  = encoder_delay & 0xfff;
      }

      SEEK(p_ctx, start);
      status = VC_CONTAINER_SUCCESS;
   }

   /* FIXME: if not success, try to read 'VBRI' header */

   return status;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_reader_read( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[0];
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   if (module->frame_data_left == 0)
   {
      status = mpga_sync(p_ctx);
      if (status != VC_CONTAINER_SUCCESS) goto error;
   }

   if (module->bitrate)
   {
      /* Simple moving average over bitrate values seen so far */
      module->bitrate = (module->bitrate * 31 + module->frame_bitrate) >> 5;
   }
   else
   {
      module->bitrate = module->frame_bitrate;
   }

   /* Check if we can skip the frame straight-away */
   if (!track->is_enabled ||
       ((flags & VC_CONTAINER_READ_FLAG_SKIP) && !(flags & VC_CONTAINER_READ_FLAG_INFO)))
   {
      /* Just skip the frame */
      SKIP_BYTES(p_ctx, module->frame_size);
      module->frame_data_left = 0;
      if(!track->is_enabled)
         status = VC_CONTAINER_ERROR_CONTINUE;
      goto end;
   }

   /* Fill in packet information */
   p_packet->flags = p_packet->track = 0;
   if (module->frame_data_left == module->frame_size)
      p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME;
   else
      p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;

   p_packet->size = module->frame_data_left;

   p_packet->pts = module->frame_time_pos;
   p_packet->dts = VC_CONTAINER_TIME_UNKNOWN;

   if ((flags & VC_CONTAINER_READ_FLAG_SKIP))
   {
      SKIP_BYTES(p_ctx, module->frame_size);
      module->frame_data_left = 0;
      goto end;
   }

   if (flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   p_packet->size = MIN(p_packet->buffer_size, module->frame_data_left);
   p_packet->size = READ_BYTES(p_ctx, p_packet->data, p_packet->size);
   module->frame_data_left -= p_packet->size;

 end:
   if (module->frame_data_left == 0)
   {
      module->frame_index++;
      module->frame_offset += module->frame_size;
      module->frame_time_pos = mpga_calculate_frame_time(p_ctx);

#if 0 /* FIXME: is this useful e.g. progressive download? */
      module->num_frames = MAX(module->num_frames, module->frame_index);
      module->data_size = MAX(module->data_size, module->frame_offset);
      p_ctx->duration = MAX(p_ctx->duration, mpga_calculate_frame_time(p_ctx));
#endif
   }

   return status == VC_CONTAINER_SUCCESS ? STREAM_STATUS(p_ctx) : status;

error:
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_reader_seek( VC_CONTAINER_T *p_ctx,
                                               int64_t *p_offset,
                                               VC_CONTAINER_SEEK_MODE_T mode,
                                               VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint64_t seekpos, position = STREAM_POSITION(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(flags);

   if (mode != VC_CONTAINER_SEEK_MODE_TIME || !STREAM_SEEKABLE(p_ctx))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   if (*p_offset != INT64_C(0))
   {
      if (!p_ctx->duration)
         return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

      if (module->xing_toc_valid)
      {
         int64_t ppm;
         int percent, lower, upper, delta;

         ppm = (*p_offset * module->sample_rate) / (module->num_frames * module->frame_size_samples);
         ppm = MIN(ppm, INT64_C(999999));

         percent = ppm / 10000;
         delta   = ppm % 10000;

         lower = module->xing_toc[percent];
         upper = percent < 99 ? module->xing_toc[percent + 1] : 256;

         seekpos = module->data_offset +
            (((module->data_size * lower) + (module->data_size * (upper - lower) * delta) / 10000) >> 8);
      }
      else
      {
         /* The following will be accurate for CBR only */
         seekpos = module->data_offset + (*p_offset * module->data_size) / p_ctx->duration;
      }
   }
   else
   {
      seekpos = module->data_offset;
   }

   SEEK(p_ctx, seekpos);
   status = mpga_sync(p_ctx);
   if (status && status != VC_CONTAINER_ERROR_EOS)
      goto error;

   module->frame_index = (*p_offset * module->num_frames + (p_ctx->duration >> 1)) / p_ctx->duration;
   module->frame_offset = STREAM_POSITION(p_ctx) - module->data_offset;

   *p_offset = module->frame_time_pos = mpga_calculate_frame_time(p_ctx);

   return STREAM_STATUS(p_ctx);

error:
   SEEK(p_ctx, position);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   if (p_ctx->tracks_num != 0)
      vc_container_free_track(p_ctx, p_ctx->tracks[0]);
   p_ctx->tracks = NULL;
   p_ctx->tracks_num = 0;
   free(module);
   p_ctx->priv->module = 0;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T mpga_reader_open( VC_CONTAINER_T *p_ctx )
{
   const char *extension = vc_uri_path_extension(p_ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_TRACK_T *track = NULL;
   unsigned int i;
   GUID_T guid;

   /* Check if the user has specified a container */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   /* Since mpeg audio is difficult to auto-detect, we use the extension as
      part of the autodetection */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "mp3") && strcasecmp(extension, "mp2") &&
      strcasecmp(extension, "aac") && strcasecmp(extension, "adts"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Check we're not in fact dealing with an ASF file */
   if(PEEK_BYTES(p_ctx, (uint8_t *)&guid, sizeof(guid)) == sizeof(guid) &&
      !memcmp(&guid, &asf_guid_header, sizeof(guid)))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(p_ctx, "using mpga reader");

   /* Allocate our context */
   if ((module = malloc(sizeof(*module))) == NULL)
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }

   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = &module->track;

   p_ctx->tracks[0] = vc_container_allocate_track(p_ctx, 0);
   if(!p_ctx->tracks[0])
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }
   p_ctx->tracks_num = 1;

   module->pf_parse_header = mpga_read_header;
   if(!strcasecmp(extension, "aac") || !strcasecmp(extension, "adts"))
      module->pf_parse_header = adts_read_header;

   if ((status = mpga_sync(p_ctx)) != VC_CONTAINER_SUCCESS)
   {
      /* An error here probably means it's not an mpga file at all */
      if(status == VC_CONTAINER_ERROR_FORMAT_INVALID)
         status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      goto error;
   }

   /* If we got this far, we're probably dealing with an mpeg audio file */
   track = p_ctx->tracks[0];
   track->format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   track->format->codec = VC_CONTAINER_CODEC_MPGA;
   if(module->pf_parse_header == adts_read_header)
   {
      uint8_t *extra = track->format->extradata = module->extradata;
      unsigned int sr_id;
      for( sr_id = 0; sr_id < 13; sr_id++ )
         if( mpga_sample_rate_adts[sr_id] == module->sample_rate ) break;
      extra[0] = (module->version << 3) | ((sr_id & 0xe) >> 1);
      extra[1] = ((sr_id & 0x1) << 7) | (module->channels << 3);
      track->format->extradata_size = 2;
      track->format->codec = VC_CONTAINER_CODEC_MP4A;
   }
   track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
   track->is_enabled = true;
   track->format->type->audio.channels = module->channels;
   track->format->type->audio.sample_rate = module->sample_rate;
   track->format->type->audio.bits_per_sample = 0;
   track->format->type->audio.block_align = 1;

   module->data_offset = STREAM_POSITION(p_ctx);

   /* Look for VBR headers within the first frame */
   status = mpga_read_vbr_headers(p_ctx);
   if (status && status != VC_CONTAINER_ERROR_NOT_FOUND) goto error;

   /* If we couldn't get this information from VBR headers, try to determine
      file size, bitrate, number of frames and duration */
   if (!module->data_size)
      module->data_size = MAX(p_ctx->priv->io->size - module->data_offset, INT64_C(0));

   if (!module->bitrate)
   {
      if (STREAM_SEEKABLE(p_ctx))
      {
         /* Scan past a few hundred frames (audio will often have
            silence in the beginning so we need to see more than
            just a few frames) and estimate bitrate */
         for (i = 0; i < 256; ++i)
            if (mpga_reader_read(p_ctx, NULL, VC_CONTAINER_READ_FLAG_SKIP)) break;
         /* Seek back to start of data */
         SEEK(p_ctx, module->data_offset);
         module->frame_index = 0;
         module->frame_offset = INT64_C(0);
         module->frame_time_pos = mpga_calculate_frame_time(p_ctx);
      }
      else
      {
         /* Bitrate will be correct for CBR only */
         module->bitrate = module->frame_bitrate;
      }
   }

   track->format->bitrate = module->bitrate;

   if (!module->num_frames)
   {
      module->num_frames = (module->data_size * module->sample_rate * 8LL) /
                              (module->bitrate * module->frame_size_samples);
   }

   if (!p_ctx->duration && module->bitrate)
   {
      p_ctx->duration = (INT64_C(8000000) * module->data_size) / module->bitrate;
   }

   p_ctx->priv->pf_close = mpga_reader_close;
   p_ctx->priv->pf_read = mpga_reader_read;
   p_ctx->priv->pf_seek = mpga_reader_seek;

   if(STREAM_SEEKABLE(p_ctx)) p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) goto error;
   return VC_CONTAINER_SUCCESS;

error:
   if(status == VC_CONTAINER_SUCCESS || status == VC_CONTAINER_ERROR_EOS)
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   LOG_DEBUG(p_ctx, "error opening stream (%i)", status);
   if (p_ctx->tracks_num != 0)
      vc_container_free_track(p_ctx, p_ctx->tracks[0]);
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
# pragma weak reader_open mpga_reader_open
#endif
