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

//#define ENABLE_CONTAINERS_LOG_FORMAT
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_writer_utils.h"
#include "containers/core/containers_logging.h"
#undef CONTAINER_HELPER_LOG_INDENT
#define CONTAINER_HELPER_LOG_INDENT(a) (a)->priv->module->object_level

VC_CONTAINER_STATUS_T asf_writer_open( VC_CONTAINER_T *p_ctx );

/******************************************************************************
Defines.
******************************************************************************/
#define ASF_TRACKS_MAX 16
#define ASF_OBJECT_HEADER_SIZE (16+8)

/******************************************************************************
Type definitions.
******************************************************************************/
typedef enum {
   ASF_OBJECT_TYPE_UNKNOWN = 0,
   ASF_OBJECT_TYPE_HEADER,
   ASF_OBJECT_TYPE_FILE_PROPS,
   ASF_OBJECT_TYPE_STREAM_PROPS,
   ASF_OBJECT_TYPE_EXT_STREAM_PROPS,
   ASF_OBJECT_TYPE_DATA,
   ASF_OBJECT_TYPE_SIMPLE_INDEX,
   ASF_OBJECT_TYPE_INDEX,
   ASF_OBJECT_TYPE_HEADER_EXT,
   ASF_OBJECT_TYPE_HEADER_EXT_INTERNAL,
   ASF_OBJECT_TYPE_CODEC_LIST,
   ASF_OBJECT_TYPE_CONTENT_DESCRIPTION,
   ASF_OBJECT_TYPE_EXT_CONTENT_DESCRIPTION,
   ASF_OBJECT_TYPE_STREAM_BITRATE_PROPS,
   ASF_OBJECT_TYPE_LANGUAGE_LIST,
   ASF_OBJECT_TYPE_METADATA,
   ASF_OBJECT_TYPE_PADDING,
} ASF_OBJECT_TYPE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   unsigned int stream_id;
   uint64_t time_offset;
   bool b_valid;

   uint64_t index_offset;
   uint32_t num_index_entries;
   int64_t  index_time_interval;
} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   int object_level;
   uint32_t packet_size;

   VC_CONTAINER_TRACK_T *tracks[ASF_TRACKS_MAX];

   VC_CONTAINER_WRITER_EXTRAIO_T null;
   bool b_header_done;

   unsigned int current_track;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Static functions within this file.
******************************************************************************/
static VC_CONTAINER_STATUS_T asf_write_object( VC_CONTAINER_T *p_ctx, ASF_OBJECT_TYPE_T object_type );
static VC_CONTAINER_STATUS_T asf_write_object_header( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_header_ext( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_header_ext_internal( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_file_properties( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_stream_properties( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_ext_stream_properties( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_simple_index( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_index( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_data( VC_CONTAINER_T *p_ctx );
#if 0
static VC_CONTAINER_STATUS_T asf_write_object_codec_list( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_content_description( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T asf_write_object_stream_bitrate_props( VC_CONTAINER_T *p_ctx );
#endif

static const GUID_T asf_guid_header = {0x75B22630, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const GUID_T asf_guid_file_props = {0x8CABDCA1, 0xA947, 0x11CF, {0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const GUID_T asf_guid_stream_props = {0xB7DC0791, 0xA9B7, 0x11CF, {0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const GUID_T asf_guid_ext_stream_props = {0x14E6A5CB, 0xC672, 0x4332, {0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A}};
static const GUID_T asf_guid_data = {0x75B22636, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const GUID_T asf_guid_simple_index = {0x33000890, 0xE5B1, 0x11CF, {0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB}};
static const GUID_T asf_guid_index = {0xD6E229D3, 0x35DA, 0x11D1, {0x90, 0x34, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xBE}};
static const GUID_T asf_guid_header_ext = {0x5FBF03B5, 0xA92E, 0x11CF, {0x8E, 0xE3, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const GUID_T asf_guid_codec_list = {0x86D15240, 0x311D, 0x11D0, {0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};
static const GUID_T asf_guid_content_description = {0x75B22633, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const GUID_T asf_guid_ext_content_description = {0xD2D0A440, 0xE307, 0x11D2, {0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50}};
static const GUID_T asf_guid_stream_bitrate_props = {0x7BF875CE, 0x468D, 0x11D1, {0x8D, 0x82, 0x00, 0x60, 0x97, 0xC9, 0xA2, 0xB2}};
static const GUID_T asf_guid_language_list = {0x7C4346A9, 0xEFE0, 0x4BFC, {0xB2, 0x29, 0x39, 0x3E, 0xDE, 0x41, 0x5C, 0x85}};
static const GUID_T asf_guid_metadata = {0xC5F8CBEA, 0x5BAF, 0x4877, {0x84, 0x67, 0xAA, 0x8C, 0x44, 0xFA, 0x4C, 0xCA}};
static const GUID_T asf_guid_padding = {0x1806D474, 0xCADF, 0x4509, {0xA4, 0xBA, 0x9A, 0xAB, 0xCB, 0x96, 0xAA, 0xE8}};

static const GUID_T asf_guid_stream_type_video = {0xBC19EFC0, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const GUID_T asf_guid_stream_type_audio = {0xF8699E40, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const GUID_T asf_guid_error_correction = {0x20FB5700, 0x5B55, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};

static struct {
  const ASF_OBJECT_TYPE_T type;
  const GUID_T *guid;
  const char *psz_name;
  VC_CONTAINER_STATUS_T (*pf_func)( VC_CONTAINER_T * );

} asf_object_list[] =
{
   {ASF_OBJECT_TYPE_HEADER, &asf_guid_header, "header", asf_write_object_header},
   {ASF_OBJECT_TYPE_FILE_PROPS, &asf_guid_file_props, "file properties", asf_write_object_file_properties},
   {ASF_OBJECT_TYPE_STREAM_PROPS, &asf_guid_stream_props, "stream properties", asf_write_object_stream_properties},
   {ASF_OBJECT_TYPE_EXT_STREAM_PROPS, &asf_guid_ext_stream_props, "extended stream properties", asf_write_object_ext_stream_properties},
   {ASF_OBJECT_TYPE_DATA, &asf_guid_data, "data", asf_write_object_data},
   {ASF_OBJECT_TYPE_SIMPLE_INDEX, &asf_guid_simple_index, "simple index", asf_write_object_simple_index},
   {ASF_OBJECT_TYPE_INDEX, &asf_guid_index, "index", asf_write_object_index},
   {ASF_OBJECT_TYPE_HEADER_EXT, &asf_guid_header_ext, "header extension", asf_write_object_header_ext},
   {ASF_OBJECT_TYPE_HEADER_EXT_INTERNAL, &asf_guid_header_ext, "header extension", asf_write_object_header_ext_internal},
#if 0
   {ASF_OBJECT_TYPE_CODEC_LIST, &asf_guid_codec_list, "codec list", asf_write_object_codec_list},
   {ASF_OBJECT_TYPE_CONTENT_DESCRIPTION, &asf_guid_content_description, "content description", asf_write_object_content_description},
   {ASF_OBJECT_TYPE_EXT_CONTENT_DESCRIPTION, &asf_guid_ext_content_description, "extended content description", 0},
   {ASF_OBJECT_TYPE_STREAM_BITRATE_PROPS, &asf_guid_stream_bitrate_props, "stream bitrate properties", asf_write_object_stream_bitrate_props},
#endif
   {ASF_OBJECT_TYPE_UNKNOWN, 0, "unknown", 0}
};

static GUID_T guid_null;

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_write_object( VC_CONTAINER_T *p_ctx, ASF_OBJECT_TYPE_T type )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t object_size = 0;
   unsigned int i;

   /* Find out which object we want to write */
   for( i = 0; asf_object_list[i].type && asf_object_list[i].type != type; i++ );

   /* Check we found the requested type */
   if(!asf_object_list[i].type)
   {
      vc_container_assert(0);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   /* We need to find out the size of the object we're going to write.
    * Because we want to be streamable, we can't just come back later to update the size field.
    * The easiest way to find out the size of the data we're going to write is to write a dummy
    * version of it and get the size from that. It is a bit wasteful but is so much easier and
    * shouldn't really impact performance as there's no actual i/o involved. */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null))
   {
      asf_object_list[i].pf_func(p_ctx);
      object_size = STREAM_POSITION(p_ctx);
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null);

   /* Special case for header extension internal function */
   if(type == ASF_OBJECT_TYPE_HEADER_EXT_INTERNAL)
   {
      WRITE_U32(p_ctx, object_size, "Header Extension Data Size");
      /* Call the object specific writing function */
      status  = asf_object_list[i].pf_func(p_ctx);
      return status;
   }

   /* Write the object header */

   if(WRITE_GUID(p_ctx, asf_object_list[i].guid, "Object ID") != sizeof(GUID_T))
      return VC_CONTAINER_ERROR_EOS;

   LOG_FORMAT(p_ctx, "Object Name: %s", asf_object_list[i].psz_name);

   WRITE_U64(p_ctx, object_size + ASF_OBJECT_HEADER_SIZE, "Object Size");

   module->object_level++;

   /* Call the object specific writing function */
   status  = asf_object_list[i].pf_func(p_ctx);

   module->object_level--;

   if(status != VC_CONTAINER_SUCCESS)
      LOG_DEBUG(p_ctx, "object %s appears to be corrupted", asf_object_list[i].psz_name);

   return status;
}

static VC_CONTAINER_STATUS_T asf_write_object_header( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   WRITE_U32(p_ctx, 0, "Number of Header Objects"); /* FIXME: could use that */
   WRITE_U8(p_ctx, 0, "Reserved1");
   WRITE_U8(p_ctx, 0, "Reserved2");

   status = asf_write_object(p_ctx, ASF_OBJECT_TYPE_FILE_PROPS);
   status = asf_write_object(p_ctx, ASF_OBJECT_TYPE_HEADER_EXT);

   for(module->current_track = 0; module->current_track < p_ctx->tracks_num;
       module->current_track++)
   {
      status = asf_write_object(p_ctx, ASF_OBJECT_TYPE_STREAM_PROPS);
   }

   /* Codec List */
   /* Content Description */
   /* Stream Bitrate Properties */

   return status;
}

static VC_CONTAINER_STATUS_T asf_write_object_header_ext( VC_CONTAINER_T *p_ctx )
{
   WRITE_GUID(p_ctx, &guid_null, "Reserved Field 1");
   WRITE_U16(p_ctx, 0, "Reserved Field 2");

   return asf_write_object(p_ctx, ASF_OBJECT_TYPE_HEADER_EXT_INTERNAL);
}

static VC_CONTAINER_STATUS_T asf_write_object_header_ext_internal( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   for(module->current_track = 0; module->current_track < p_ctx->tracks_num;
       module->current_track++)
   {
      status = asf_write_object(p_ctx, ASF_OBJECT_TYPE_EXT_STREAM_PROPS);
   }

   return status;
}

static VC_CONTAINER_STATUS_T asf_write_object_file_properties( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   WRITE_GUID(p_ctx, &guid_null, "File ID");
   WRITE_U64(p_ctx, 0, "File Size");
   WRITE_U64(p_ctx, 0, "Creation Date");
   WRITE_U64(p_ctx, 0, "Data Packets Count");
   WRITE_U64(p_ctx, 0, "Play Duration");
   WRITE_U64(p_ctx, 0, "Send Duration");
   WRITE_U64(p_ctx, 0, "Preroll");
   WRITE_U32(p_ctx, 0, "Flags");
   WRITE_U32(p_ctx, module->packet_size, "Minimum Data Packet Size");
   WRITE_U32(p_ctx, module->packet_size, "Maximum Data Packet Size");
   WRITE_U32(p_ctx, 0, "Maximum Bitrate");

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T asf_write_bitmapinfoheader( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *p_track )
{
   uint32_t fourcc;

   /* Write the preamble to the BITMAPINFOHEADER */
   WRITE_U32(p_ctx, p_track->format->type->video.width, "Encoded Image Width");
   WRITE_U32(p_ctx, p_track->format->type->video.height, "Encoded Image Height");
   WRITE_U8(p_ctx, 0, "Reserved Flags");
   WRITE_U16(p_ctx, 40 + p_track->format->extradata_size, "Format Data Size");

   /* Write BITMAPINFOHEADER structure */
   WRITE_U32(p_ctx, 40 + p_track->format->extradata_size, "Format Data Size");
   WRITE_U32(p_ctx, p_track->format->type->video.width, "Image Width");
   WRITE_U32(p_ctx, p_track->format->type->video.height, "Image Height");
   WRITE_U16(p_ctx, 0, "Reserved");
   WRITE_U16(p_ctx, 0, "Bits Per Pixel Count");
   fourcc = codec_to_fourcc(p_track->format->codec);
   WRITE_BYTES(p_ctx, (char *)&fourcc, 4); /* Compression ID */
   LOG_FORMAT(p_ctx, "Compression ID: %4.4s", (char *)&fourcc);
   WRITE_U32(p_ctx, 0, "Image Size");
   WRITE_U32(p_ctx, 0, "Horizontal Pixels Per Meter");
   WRITE_U32(p_ctx, 0, "Vertical Pixels Per Meter");
   WRITE_U32(p_ctx, 0, "Colors Used Count");
   WRITE_U32(p_ctx, 0, "Important Colors Count");

   WRITE_BYTES(p_ctx, p_track->format->extradata, p_track->format->extradata_size);

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T asf_write_waveformatex( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *p_track)
{
   /* Write WAVEFORMATEX structure */
   WRITE_U16(p_ctx, codec_to_waveformat(p_track->format->codec), "Codec ID");
   WRITE_U16(p_ctx, p_track->format->type->audio.channels, "Number of Channels");
   WRITE_U32(p_ctx, p_track->format->type->audio.sample_rate, "Samples per Second");
   WRITE_U32(p_ctx, p_track->format->bitrate, "Average Number of Bytes Per Second");
   WRITE_U16(p_ctx, p_track->format->type->audio.block_align, "Block Alignment");
   WRITE_U16(p_ctx, p_track->format->type->audio.bits_per_sample, "Bits Per Sample");
   WRITE_U16(p_ctx, p_track->format->extradata_size, "Codec Specific Data Size");
   WRITE_BYTES(p_ctx, p_track->format->extradata, p_track->format->extradata_size);

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T asf_write_object_stream_properties( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int track = module->current_track, ts_size = 0;
   const GUID_T *p_guid = &guid_null;

   if(p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      p_guid = &asf_guid_stream_type_video;
      ts_size = 11 + 40 + p_ctx->tracks[track]->format->extradata_size;
   }
   else if(p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
   {
      p_guid = &asf_guid_stream_type_audio;
      ts_size = 18 + p_ctx->tracks[track]->format->extradata_size;
   }

   WRITE_GUID(p_ctx, p_guid, "Stream Type");
   WRITE_GUID(p_ctx, &asf_guid_error_correction, "Error Correction Type");
   WRITE_U64(p_ctx, 0, "Time Offset");
   WRITE_U32(p_ctx, ts_size, "Type-Specific Data Length");
   WRITE_U32(p_ctx, 0, "Error Correction Data Length");
   WRITE_U16(p_ctx, track + 1, "Flags");
   WRITE_U32(p_ctx, 0, "Reserved");

   /* Type-Specific Data */
   if(ts_size)
   {
      if(p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
         status = asf_write_bitmapinfoheader( p_ctx, p_ctx->tracks[track]);
      else if(p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
         status = asf_write_waveformatex( p_ctx, p_ctx->tracks[track]);
   }

   /* Error Correction Data */

   return status;
}

static VC_CONTAINER_STATUS_T asf_write_object_ext_stream_properties( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   WRITE_U64(p_ctx, 0, "Start Time");
   WRITE_U64(p_ctx, 0, "End Time");
   WRITE_U32(p_ctx, 0, "Data Bitrate");
   WRITE_U32(p_ctx, 0, "Buffer Size");
   WRITE_U32(p_ctx, 0, "Initial Buffer Fullness");
   WRITE_U32(p_ctx, 0, "Alternate Data Bitrate");
   WRITE_U32(p_ctx, 0, "Alternate Buffer Size");
   WRITE_U32(p_ctx, 0, "Alternate Initial Buffer Fullness");
   WRITE_U32(p_ctx, 0, "Maximum Object Size");
   WRITE_U32(p_ctx, 0, "Flags");
   WRITE_U16(p_ctx, module->current_track + 1, "Stream Number");
   WRITE_U16(p_ctx, 0, "Stream Language ID Index");
   WRITE_U64(p_ctx, 0, "Average Time Per Frame");
   WRITE_U16(p_ctx, 0, "Stream Name Count");
   WRITE_U16(p_ctx, 0, "Payload Extension System Count");
   /* Stream Names */
   /* Payload Extension Systems */
   /* Stream Properties Object */

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T asf_write_object_index( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T asf_write_object_simple_index( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T asf_write_object_data( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_write_header( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status;

   /* TODO Sanity check the tracks */

   status = asf_write_object(p_ctx, ASF_OBJECT_TYPE_HEADER);
   status = asf_write_object(p_ctx, ASF_OBJECT_TYPE_DATA);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_writer_write( VC_CONTAINER_T *p_ctx,
                                               VC_CONTAINER_PACKET_T *p_packet )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PARAM_UNUSED(p_packet);

   if(!module->b_header_done)
   {
      module->b_header_done = true;
      status = asf_write_header(p_ctx);
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_writer_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);

   vc_container_writer_extraio_delete(p_ctx, &module->null);
   free(module);

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_writer_add_track( VC_CONTAINER_T *p_ctx, VC_CONTAINER_ES_FORMAT_T *format )
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_TRACK_T *track;

   /* TODO check we support this format */

   if(!(format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   
   /* Allocate and initialise track data */
   if(p_ctx->tracks_num >= ASF_TRACKS_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   p_ctx->tracks[p_ctx->tracks_num] = track =
      vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
   if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   if(format->extradata_size)
   {
      status = vc_container_track_allocate_extradata( p_ctx, track, format->extradata_size );
      if(status) goto error;
   }

   vc_container_format_copy(track->format, format, format->extradata_size);
   p_ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;

 error:
   vc_container_free_track(p_ctx, track);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_writer_control( VC_CONTAINER_T *p_ctx, VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;

   switch(operation)
   {
   case VC_CONTAINER_CONTROL_TRACK_ADD:
      {
         VC_CONTAINER_ES_FORMAT_T *p_format =
            (VC_CONTAINER_ES_FORMAT_T *)va_arg( args, VC_CONTAINER_ES_FORMAT_T * );
         return asf_writer_add_track(p_ctx, p_format);
      }

   case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      if(!module->b_header_done)
      {
         status = asf_write_header(p_ctx);
         if(status != VC_CONTAINER_SUCCESS) return status;
         module->b_header_done = true;
      }
      return VC_CONTAINER_SUCCESS;

   default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/******************************************************************************
Global function definitions.
******************************************************************************/

VC_CONTAINER_STATUS_T asf_writer_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   const char *extension = vc_uri_path_extension(p_ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   unsigned int i;

   /* Check if the user has specified a container */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "asf") && strcasecmp(extension, "wmv") &&
      strcasecmp(extension, "wma"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;

   /* Create a null i/o writer to help us out in writing our data */
   status = vc_container_writer_extraio_create_null(p_ctx, &module->null);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   /* We'll only write the header once we've got all our tracks */

   p_ctx->priv->pf_close = asf_writer_close;
   p_ctx->priv->pf_write = asf_writer_write;
   p_ctx->priv->pf_control = asf_writer_control;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "asf: error opening stream");
   for(i = 0; i < ASF_TRACKS_MAX && p_ctx->tracks && p_ctx->tracks[i]; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   free(module);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open asf_writer_open
#endif
