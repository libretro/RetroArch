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

/* prevent double-defines when it is defined on the command line - as in the test app */
#ifndef ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT
#endif
#ifndef ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#endif

#define CONTAINER_HELPER_LOG_INDENT(a) (a)->priv->module->object_level

/* For the sanity of the Visual Studio debugger make local names for structures */
#define  VC_CONTAINER_TRACK_MODULE_T ASF_VC_CONTAINER_TRACK_MODULE_T
#define  VC_CONTAINER_MODULE_T ASF_VC_CONTAINER_MODULE_T
#define  VC_CONTAINER_T ASF_VC_CONTAINER_T

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/
#define ASF_TRACKS_MAX 2
#define ASF_EXTRADATA_MAX 256

#define ASF_MAX_OBJECT_LEVEL 4
#define ASF_MAX_CONSECUTIVE_UNKNOWN_OBJECTS 5
#define ASF_MAX_OBJECT_SIZE  (1<<29) /* Does not apply to the data object */
#define ASF_OBJECT_HEADER_SIZE (16+8)
#define ASF_UNKNOWN_PTS ((uint32_t)(-1))
#define ASF_MAX_CONSECUTIVE_CORRUPTED_PACKETS 100
#define ASF_MAX_SEARCH_PACKETS 1000

#define ASF_SKIP_GUID(ctx, size, n) (size -= 16, SKIP_GUID(ctx,n))
#define ASF_SKIP_U8(ctx, size, n)   (size -= 1, SKIP_U8(ctx,n))
#define ASF_SKIP_U16(ctx, size, n)  (size -= 2, SKIP_U16(ctx,n))
#define ASF_SKIP_U24(ctx, size, n)  (size -= 3, SKIP_U24(ctx,n))
#define ASF_SKIP_U32(ctx, size, n)  (size -= 4, SKIP_U32(ctx,n))
#define ASF_SKIP_U64(ctx, size, n)  (size -= 8, SKIP_U64(ctx,n))
#define ASF_READ_GUID(ctx, size, buffer, n) (size -= 16, READ_GUID(ctx,(uint8_t *)buffer,n))
#define ASF_READ_U8(ctx, size, n)   (size -= 1, READ_U8(ctx,n))
#define ASF_READ_U16(ctx, size, n)  (size -= 2, READ_U16(ctx,n))
#define ASF_READ_U24(ctx, size, n)  (size -= 3, READ_U24(ctx,n))
#define ASF_READ_U32(ctx, size, n)  (size -= 4, READ_U32(ctx,n))
#define ASF_READ_U64(ctx, size, n)  (size -= 8, READ_U64(ctx,n))
#define ASF_READ_STRING(ctx, size, buffer, to_read, n) (size -= to_read, READ_STRING_UTF16(ctx,buffer,to_read,n))
#define ASF_SKIP_STRING(ctx, size, to_read, n) (size -= to_read, SKIP_STRING_UTF16(ctx,to_read,n))
#define ASF_READ_BYTES(ctx, size, buffer, to_read) (size -= to_read, READ_BYTES(ctx,buffer,to_read))
#define ASF_SKIP_BYTES(ctx, size, to_read) (size -= to_read, SKIP_BYTES(ctx,to_read))

/* Read variable length field from p_context. */
#define READ_VLC(p_context, length, value_if_missing, txt) \
   (length) == 1 ? READ_U8(p_context, txt) : \
   (length) == 2 ? READ_U16(p_context, txt) : \
   (length) == 3 ? READ_U32(p_context, txt) : value_if_missing

#define CHECK_POINT(p_context, amount_to_read) do { \
   if(amount_to_read < 0) return VC_CONTAINER_ERROR_CORRUPTED; \
   if(STREAM_STATUS(p_context)) return STREAM_STATUS(p_context); } while(0)

/******************************************************************************
Type definitions.
******************************************************************************/

/** Context for our reader
  */
typedef struct
{
   uint64_t start;            /* The byte offset start of the current packet in the file */
   uint32_t size;
   uint32_t padding_size;
   uint64_t send_time;        /* read in mS, stored in uS */
   bool     eos;
   bool     corrupted;
   uint16_t bad_packets;

   /* All the different Length Types for the VLC codes */
   unsigned int replicated_data_lt;
   unsigned int offset_into_media_object_lt;
   unsigned int media_object_number_lt;
   unsigned int payload_lt;

   unsigned int multiple_payloads;
   unsigned int compressed_payloads;

   uint8_t num_payloads;
   uint8_t current_payload;
   uint32_t current_offset;            /* The offset in the current packet for the next byte to be read */

   /* Info already read */
   uint32_t stream_num;                /* Stream number and key-frame flag */
   uint32_t media_object_num;
   uint32_t media_object_off;
   uint32_t payload_size;
   uint32_t subpayload_size;

   /* Info read from the replicated data */
   uint32_t media_object_size;
   uint64_t media_object_pts;         /**< Presentation timestamp in microseconds */
   uint64_t media_object_pts_delta;   /**< Presentation timestamp delta in microseconds */

} ASF_PACKET_STATE;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   /* The ID of the stream (the index in the containing array need not be the ID) */
   unsigned int stream_id;
   bool b_valid;

   uint8_t extradata[ASF_EXTRADATA_MAX];

   ASF_PACKET_STATE *p_packet_state;
   ASF_PACKET_STATE local_packet_state;

   /* Simple index structure. Corresponds to the simple index in 6.1 of the spec
    * This index has locations in packets, not in bytes, and relies on the
    * file having fixed-length packets - as is required */
   struct
   {
      uint64_t offset;        /**< Offset to the start of the simple index data */
      uint32_t num_entries;
      int64_t  time_interval; /* in uS */
      bool     incomplete;    /* The index does not go to the end of the file */
   } simple_index;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   int object_level;

   uint32_t packet_size;   /**< Size of a data packet */
   uint64_t packets_num;   /**< Number of packets contained in the data object */

   bool broadcast;         /**< Specifies if we are dealing with a broadcast stream */
   int64_t duration;       /**< Duration of the stream in microseconds */
   int64_t preroll;        /**< Duration of the preroll in microseconds. */
                           /* This is the PTS of the first packet; all are offset by this amount. */
   uint64_t  time_offset;  /**< Offset added to timestamps in microseconds */

   uint64_t data_offset;   /**< Offset to the start of the data packets */
   int64_t data_size;      /**< Size of the data contained in the data object */

   /* The track objects. There's a count of these in VC_CONTAINER_T::tracks_num */
   VC_CONTAINER_TRACK_T *tracks[ASF_TRACKS_MAX];

   /* Translation table from stream_number to index in the tracks array */
   unsigned char stream_number_to_index[128];

   /* Data for a top-level index structure as defined in 6.2 of the spec */
   struct
   {
      uint64_t entry_time_interval;                /* The time interval between specifiers, scaled to uS */
      uint32_t specifiers_count;                   /* The number of specifiers in the file, 0 if no index */
      uint64_t active_specifiers[ASF_TRACKS_MAX];  /* the specifier in use for each track,
                                                    * or >=specifiers_count if none */
      uint64_t specifiers_offset;                  /* The file address of the first specifier. */
      uint32_t block_count;                        /* The number of index blocks */
      uint64_t blocks_offset;                      /* The file address of the first block */
   } top_level_index;

   /* A pointer to the track (in the tracks array) which is to be used with a simple index.
    * null if there is no such track */
   ASF_VC_CONTAINER_TRACK_MODULE_T *simple_index_track;

   /* Shared packet state. This is used when the tracks are in sync,
      and for the track at the earliest position in the file when they are not in sync */
   ASF_PACKET_STATE packet_state;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T asf_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Prototypes for local functions
******************************************************************************/
static VC_CONTAINER_STATUS_T asf_read_object( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_header( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_header_ext( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_file_properties( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_stream_properties( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_ext_stream_properties( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_simple_index( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_index( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_index_parameters( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_data( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_codec_list( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_content_description( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_stream_bitrate_props( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_content_encryption( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_ext_content_encryption( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_read_object_adv_content_encryption( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T asf_skip_unprocessed_object( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T seek_to_positions(VC_CONTAINER_T *p_ctx,
   uint64_t track_positions[ASF_TRACKS_MAX], int64_t *p_time,
   VC_CONTAINER_SEEK_FLAGS_T flags, unsigned int start_track,
   bool seek_on_start_track);

/******************************************************************************
GUID list for the different ASF objects
******************************************************************************/
static const GUID_T asf_guid_header = {0x75B22630, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const GUID_T asf_guid_file_props = {0x8CABDCA1, 0xA947, 0x11CF, {0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const GUID_T asf_guid_stream_props = {0xB7DC0791, 0xA9B7, 0x11CF, {0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const GUID_T asf_guid_ext_stream_props = {0x14E6A5CB, 0xC672, 0x4332, {0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A}};
static const GUID_T asf_guid_data = {0x75B22636, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const GUID_T asf_guid_simple_index = {0x33000890, 0xE5B1, 0x11CF, {0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB}};
static const GUID_T asf_guid_index = {0xD6E229D3, 0x35DA, 0x11D1, {0x90, 0x34, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xBE}};
static const GUID_T asf_guid_index_parameters = {0xD6E229DF, 0x35DA, 0x11D1, {0x90, 0x34, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xBE}};
static const GUID_T asf_guid_header_ext = {0x5FBF03B5, 0xA92E, 0x11CF, {0x8E, 0xE3, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const GUID_T asf_guid_codec_list = {0x86D15240, 0x311D, 0x11D0, {0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};
static const GUID_T asf_guid_content_description = {0x75B22633, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const GUID_T asf_guid_ext_content_description = {0xD2D0A440, 0xE307, 0x11D2, {0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50}};
static const GUID_T asf_guid_stream_bitrate_props = {0x7BF875CE, 0x468D, 0x11D1, {0x8D, 0x82, 0x00, 0x60, 0x97, 0xC9, 0xA2, 0xB2}};
static const GUID_T asf_guid_language_list = {0x7C4346A9, 0xEFE0, 0x4BFC, {0xB2, 0x29, 0x39, 0x3E, 0xDE, 0x41, 0x5C, 0x85}};
static const GUID_T asf_guid_metadata = {0xC5F8CBEA, 0x5BAF, 0x4877, {0x84, 0x67, 0xAA, 0x8C, 0x44, 0xFA, 0x4C, 0xCA}};
static const GUID_T asf_guid_padding = {0x1806D474, 0xCADF, 0x4509, {0xA4, 0xBA, 0x9A, 0xAB, 0xCB, 0x96, 0xAA, 0xE8}};
static const GUID_T asf_guid_content_encryption = {0x2211B3FB, 0xBD23, 0x11D2, {0xB4, 0xB7, 0x00, 0xA0, 0xC9, 0x55, 0xFC, 0x6E}};
static const GUID_T asf_guid_ext_content_encryption = {0x298AE614, 0x2622, 0x4C17, {0xB9, 0x35, 0xDA, 0xE0, 0x7E, 0xE9, 0x28, 0x9C}};
static const GUID_T asf_guid_adv_content_encryption = {0x43058533, 0x6981, 0x49E6, {0x9B, 0x74, 0xAD, 0x12, 0xCB, 0x86, 0xD5, 0x8C}};
static const GUID_T asf_guid_compatibility = {0x26F18B5D, 0x4584, 0x47EC, {0x9F, 0x5F, 0x0E, 0x65, 0x1F, 0x04, 0x52, 0xC9}};
static const GUID_T asf_guid_script_command = {0x1EFB1A30, 0x0B62, 0x11D0, {0xA3, 0x9B, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};
static const GUID_T asf_guid_mutual_exclusion = {0xD6E229DC, 0x35DA, 0x11D1, {0x90, 0x34, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xBE}};

static const GUID_T asf_guid_stream_type_video = {0xBC19EFC0, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const GUID_T asf_guid_stream_type_audio = {0xF8699E40, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};

/******************************************************************************
List of GUIDs and their associated processing functions
******************************************************************************/
static struct {
  const GUID_T *guid;
  const char *psz_name;
  VC_CONTAINER_STATUS_T (*pf_func)( VC_CONTAINER_T *, int64_t );

} asf_object_list[] =
{
   {&asf_guid_header, "header", asf_read_object_header},
   {&asf_guid_file_props, "file properties", asf_read_object_file_properties},
   {&asf_guid_stream_props, "stream properties", asf_read_object_stream_properties},
   {&asf_guid_ext_stream_props, "extended stream properties", asf_read_object_ext_stream_properties},
   {&asf_guid_data, "data", asf_read_object_data},
   {&asf_guid_simple_index, "simple index", asf_read_object_simple_index},
   {&asf_guid_index, "index", asf_read_object_index},
   {&asf_guid_index_parameters, "index parameters", asf_read_object_index_parameters},
   {&asf_guid_header_ext, "header extension", asf_read_object_header_ext},
   {&asf_guid_codec_list, "codec list", asf_read_object_codec_list},
   {&asf_guid_content_description, "content description", asf_read_object_content_description},
   {&asf_guid_ext_content_description, "extended content description", asf_skip_unprocessed_object},
   {&asf_guid_stream_bitrate_props, "stream bitrate properties", asf_read_object_stream_bitrate_props},
   {&asf_guid_language_list, "language list", asf_skip_unprocessed_object},
   {&asf_guid_metadata, "metadata", asf_skip_unprocessed_object},
   {&asf_guid_padding, "padding", asf_skip_unprocessed_object},
   {&asf_guid_compatibility, "compatibility", asf_skip_unprocessed_object},
   {&asf_guid_script_command, "script command", asf_skip_unprocessed_object},
   {&asf_guid_mutual_exclusion, "mutual exclusion", asf_skip_unprocessed_object},
   {&asf_guid_content_encryption, "content encryption", &asf_read_object_content_encryption},
   {&asf_guid_ext_content_encryption, "extended content encryption", &asf_read_object_ext_content_encryption},
   {&asf_guid_adv_content_encryption, "advanced content encryption", &asf_read_object_adv_content_encryption},
   {0, "unknown", asf_skip_unprocessed_object}
};

/******************************************************************************
Local Functions
******************************************************************************/

/** Find the track associated with an ASF stream id */
static VC_CONTAINER_TRACK_T *asf_reader_find_track( VC_CONTAINER_T *p_ctx, unsigned int stream_id,
   bool b_create)
{
   VC_CONTAINER_TRACK_T *p_track = 0;
   VC_CONTAINER_MODULE_T * module = p_ctx->priv->module;
   unsigned int i;

   /* discard the key-frame flag */
   stream_id &= 0x7f;

   /* look to see if we have already allocated the stream */
   i = module->stream_number_to_index[stream_id];

   if(i < p_ctx->tracks_num) /* We found it */
      p_track = p_ctx->tracks[i];

   if(!p_track && b_create && p_ctx->tracks_num < ASF_TRACKS_MAX)
   {
      /* Allocate and initialise a new track */
      p_ctx->tracks[p_ctx->tracks_num] = p_track =
         vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
      if(p_track)
      {
         /* store the stream ID */
         p_track->priv->module->stream_id = stream_id;

         /* Store the translation table value */
         module->stream_number_to_index[stream_id] = p_ctx->tracks_num;

         /* count the track */
         p_ctx->tracks_num++;
      }
   }

   if(!p_track && b_create)
      LOG_DEBUG(p_ctx, "could not create track for stream id: %i", stream_id);

   return p_track;
}

/** Base function used to read an ASF object from the ASF header.
 * This will read the object header do lots of sanity checking and pass on the rest
 * of the reading to the object specific reading function */
static VC_CONTAINER_STATUS_T asf_read_object( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t object_size, offset = STREAM_POSITION(p_ctx);
   unsigned int i, unknown_objects = 0, is_data_object;
   GUID_T guid;

   /* Sanity check the size of the data */
   if(size && size < ASF_OBJECT_HEADER_SIZE)
   {
      LOG_DEBUG(p_ctx, "invalid object header (too small)");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if(READ_GUID(p_ctx, &guid, "Object ID") != sizeof(guid))
      return STREAM_STATUS(p_ctx);

   /* Find out which GUID we are dealing with */
   for( i = 0; asf_object_list[i].guid; i++ )
   {
      if(guid.word0 != asf_object_list[i].guid->word0) continue;
      if(!memcmp(&guid, asf_object_list[i].guid, sizeof(guid))) break;
   }

   LOG_FORMAT(p_ctx, "Object Name: %s", asf_object_list[i].psz_name);

   /* Bail out if we find too many consecutive unknown objects */
   if(!asf_object_list[i].guid) unknown_objects++;
   else unknown_objects = 0;
   if(unknown_objects >= ASF_MAX_CONSECUTIVE_UNKNOWN_OBJECTS)
   {
      LOG_DEBUG(p_ctx, "too many unknown objects");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   is_data_object = asf_object_list[i].pf_func == asf_read_object_data;

   object_size = READ_U64(p_ctx, "Object Size");

   /* Sanity check the object size */
   if(object_size < 0 /* Shouldn't ever get that big */ ||
      /* Minimum size check (data object can have a size == 0) */
      (object_size < ASF_OBJECT_HEADER_SIZE && !(is_data_object && !object_size)) ||
      /* Only the data object can really be massive */
      (!is_data_object && object_size > ASF_MAX_OBJECT_SIZE))
   {
      LOG_DEBUG(p_ctx, "object %s has an invalid size (%"PRIi64")",
                asf_object_list[i].psz_name, object_size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }
   if(size && object_size > size)
   {
      LOG_DEBUG(p_ctx, "object %s is bigger than it should (%"PRIi64" > %"PRIi64")",
                asf_object_list[i].psz_name, object_size, size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }
   size = object_size;

   if(module->object_level >= 2 * ASF_MAX_OBJECT_LEVEL)
   {
      LOG_DEBUG(p_ctx, "object %s is too deep. skipping", asf_object_list[i].psz_name);
      status = asf_skip_unprocessed_object(p_ctx, size - ASF_OBJECT_HEADER_SIZE);
      /* Just bail out, hoping we have enough data */
   }
   else
   {
      module->object_level++;

      /* Call the object specific parsing function */
      status = asf_object_list[i].pf_func(p_ctx, size - ASF_OBJECT_HEADER_SIZE);

      module->object_level--;

      if(status != VC_CONTAINER_SUCCESS)
         LOG_DEBUG(p_ctx, "object %s appears to be corrupted (%i)", asf_object_list[i].psz_name, status);
   }

   /* The stream position should be exactly at the end of the object */
   {
      int64_t bytes_processed = STREAM_POSITION(p_ctx) - offset;

      /* fail with overruns */
      if (bytes_processed > size)
      {
         /* Things have gone really bad here and we ended up reading past the end of the
          * object. We could maybe try to be clever and recover by seeking back to the end
          * of the object. However if we get there, the file is clearly corrupted so there's
          * no guarantee it would work anyway. */
         LOG_DEBUG(p_ctx, "%"PRIi64" bytes overrun past the end of object %s",
                   bytes_processed-size, asf_object_list[i].psz_name);
         return VC_CONTAINER_ERROR_CORRUPTED;
      }

      /* Handle underruns  by throwing away the data (this should never happen, but we don't really care if it does) */
      if (bytes_processed < size)
      {
         size -= bytes_processed;
         LOG_DEBUG(p_ctx, "%"PRIi64" bytes left unread in object %s", size, asf_object_list[i].psz_name);

         if(size < ASF_MAX_OBJECT_SIZE)
            SKIP_BYTES(p_ctx, size);                     /* read a small amount */
         else
            SEEK(p_ctx, STREAM_POSITION(p_ctx) + size);  /* seek a large distance */
      }
   }

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF header object */
static VC_CONTAINER_STATUS_T asf_read_object_header( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t offset = STREAM_POSITION(p_ctx);

   /* Sanity check the size of the data */
   if((size -= 6) < 0) return VC_CONTAINER_ERROR_CORRUPTED;

   SKIP_U32(p_ctx, "Number of Header Objects"); /* FIXME: could use that */
   SKIP_U8(p_ctx, "Reserved1");
   SKIP_U8(p_ctx, "Reserved2");

   /* Read contained objects */
   module->object_level++;
   while(status == VC_CONTAINER_SUCCESS && size >= ASF_OBJECT_HEADER_SIZE)
   {
      offset = STREAM_POSITION(p_ctx);
      status = asf_read_object(p_ctx, size);
      size -= (STREAM_POSITION(p_ctx) - offset);
   }
   module->object_level--;

   return status;
}

/** Reads an ASF extended header object */
static VC_CONTAINER_STATUS_T asf_read_object_header_ext( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t data_size, offset;

   ASF_SKIP_GUID(p_ctx, size, "Reserved Field 1");
   ASF_SKIP_U16(p_ctx, size, "Reserved Field 2");
   data_size = ASF_READ_U32(p_ctx, size, "Header Extension Data Size");

   if(data_size != size)
      LOG_DEBUG(p_ctx, "invalid header extension data size (%"PRIi64",%"PRIi64")", data_size, size);

   CHECK_POINT(p_ctx, size);

   /* Read contained objects */
   module->object_level++;
   while(status == VC_CONTAINER_SUCCESS && size >= ASF_OBJECT_HEADER_SIZE)
   {
      offset = STREAM_POSITION(p_ctx);
      status = asf_read_object(p_ctx, size);
      size -= (STREAM_POSITION(p_ctx) - offset);
   }
   module->object_level--;

   return status;
}

/** Reads an ASF file properties object */
static VC_CONTAINER_STATUS_T asf_read_object_file_properties( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t max_packet_size;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   ASF_SKIP_GUID(p_ctx, size, "File ID");
   ASF_SKIP_U64(p_ctx, size, "File Size");
   ASF_SKIP_U64(p_ctx, size, "Creation Date");
   ASF_SKIP_U64(p_ctx, size, "Data Packets Count");
   module->duration = ASF_READ_U64(p_ctx, size, "Play Duration") / UINT64_C(10);  /* read in 100nS units, stored in uS */
   ASF_SKIP_U64(p_ctx, size, "Send Duration");
   module->preroll = ASF_READ_U64(p_ctx, size, "Preroll") * UINT64_C(1000);      /* read in mS, storedin uS */
   module->broadcast = ASF_READ_U32(p_ctx, size, "Flags") & 0x1;
   module->packet_size = ASF_READ_U32(p_ctx, size, "Minimum Data Packet Size");
   max_packet_size = ASF_READ_U32(p_ctx, size, "Maximum Data Packet Size");
   ASF_SKIP_U32(p_ctx, size, "Maximum Bitrate");

   if(module->preroll < module->duration) module->duration -= module->preroll;
   else module->duration = 0;

   /* Sanity check the packet size */
   if(!module->packet_size)
   {
      LOG_DEBUG(p_ctx, "packet size cannot be 0");
      return VC_CONTAINER_ERROR_FORMAT_FEATURE_NOT_SUPPORTED;
   }

   if(max_packet_size != module->packet_size)
   {
      LOG_DEBUG(p_ctx, "asf stream not supported (min packet size: %i != max packet size: %i)",
                module->packet_size, max_packet_size);
      return VC_CONTAINER_ERROR_FORMAT_FEATURE_NOT_SUPPORTED;
   }

   return STREAM_STATUS(p_ctx);
}

/** Reads the bitmapinfoheader structure contained in a stream properties object */
static VC_CONTAINER_STATUS_T asf_read_bitmapinfoheader( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *p_track, int64_t size )
{
   uint32_t bmih_size, formatdata_size;
   uint32_t fourcc;

   /* Sanity check the size of the data */
   if(size < 40 + 11) return VC_CONTAINER_ERROR_CORRUPTED;

   /* Read the preamble to the BITMAPINFOHEADER */
   ASF_SKIP_U32(p_ctx, size, "Encoded Image Width");
   ASF_SKIP_U32(p_ctx, size, "Encoded Image Height");
   ASF_SKIP_U8(p_ctx, size, "Reserved Flags");
   formatdata_size = ASF_READ_U16(p_ctx, size, "Format Data Size");

   /* Sanity check the size of the data */
   if(formatdata_size < 40 || size < formatdata_size) return VC_CONTAINER_ERROR_CORRUPTED;
   bmih_size = ASF_READ_U32(p_ctx, size, "Format Data Size");
   if(bmih_size < 40 || bmih_size > formatdata_size) return VC_CONTAINER_ERROR_CORRUPTED;

   /* Read BITMAPINFOHEADER structure */
   p_track->format->type->video.width = ASF_READ_U32(p_ctx, size, "Image Width");
   p_track->format->type->video.height = ASF_READ_U32(p_ctx, size, "Image Height"); /* Signed */
   ASF_SKIP_U16(p_ctx, size, "Reserved");
   ASF_SKIP_U16(p_ctx, size, "Bits Per Pixel Count");
   ASF_READ_BYTES(p_ctx, size, (char *)&fourcc, 4); /* Compression ID */
   LOG_FORMAT(p_ctx, "Compression ID: %4.4s", (char *)&fourcc);
   p_track->format->codec = vfw_fourcc_to_codec(fourcc);
   if(p_track->format->codec == VC_CONTAINER_CODEC_UNKNOWN)
      p_track->format->codec = fourcc;
   ASF_SKIP_U32(p_ctx, size, "Image Size");
   ASF_SKIP_U32(p_ctx, size, "Horizontal Pixels Per Meter");
   ASF_SKIP_U32(p_ctx, size, "Vertical Pixels Per Meter");
   ASF_SKIP_U32(p_ctx, size, "Colors Used Count");
   ASF_SKIP_U32(p_ctx, size, "Important Colors Count");

   if(!(bmih_size -= 40))return VC_CONTAINER_SUCCESS;

   if(bmih_size > ASF_EXTRADATA_MAX)
   {
      LOG_DEBUG(p_ctx, "extradata truncated");
      bmih_size = ASF_EXTRADATA_MAX;
   }
   p_track->format->extradata = p_track->priv->module->extradata;
   p_track->format->extradata_size = ASF_READ_BYTES(p_ctx, size, p_track->format->extradata, bmih_size);

   return STREAM_STATUS(p_ctx);
}

/** Reads the waveformatex structure contained in a stream properties object */
static VC_CONTAINER_STATUS_T asf_read_waveformatex( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_T *p_track, int64_t size)
{
   uint16_t extradata_size;

   /* Read WAVEFORMATEX structure */
   p_track->format->codec = waveformat_to_codec(ASF_READ_U16(p_ctx, size, "Codec ID"));
   p_track->format->type->audio.channels = ASF_READ_U16(p_ctx, size, "Number of Channels");
   p_track->format->type->audio.sample_rate = ASF_READ_U32(p_ctx, size, "Samples per Second");
   p_track->format->bitrate = ASF_READ_U32(p_ctx, size, "Average Number of Bytes Per Second") * 8;
   p_track->format->type->audio.block_align = ASF_READ_U16(p_ctx, size, "Block Alignment");
   p_track->format->type->audio.bits_per_sample = ASF_READ_U16(p_ctx, size, "Bits Per Sample");
   extradata_size = ASF_READ_U16(p_ctx, size, "Codec Specific Data Size");

   CHECK_POINT(p_ctx, size);

   if(!extradata_size) return VC_CONTAINER_SUCCESS;

   /* Sanity check the size of the data */
   if(extradata_size > size) return VC_CONTAINER_ERROR_CORRUPTED;

   if(extradata_size > ASF_EXTRADATA_MAX)
   {
      LOG_DEBUG(p_ctx, "extradata truncated");
      extradata_size = ASF_EXTRADATA_MAX;
   }
   p_track->format->extradata = p_track->priv->module->extradata;
   p_track->format->extradata_size = ASF_READ_BYTES(p_ctx, size, p_track->format->extradata, extradata_size);

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF stream properties object */
static VC_CONTAINER_STATUS_T asf_read_object_stream_properties( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *p_track;
   unsigned int ts_length, flags;
   VC_CONTAINER_ES_TYPE_T type = VC_CONTAINER_ES_TYPE_UNKNOWN;
   GUID_T stream_type;
   int64_t offset;

   ASF_READ_GUID(p_ctx, size, &stream_type, "Stream Type");
   ASF_SKIP_GUID(p_ctx, size, "Error Correction Type");

   /* The time_offset field is in 100nS units. Scale back to uS */
   module->time_offset = ASF_READ_U64(p_ctx, size, "Time Offset") / UINT64_C(10);
   ts_length = ASF_READ_U32(p_ctx, size, "Type-Specific Data Length");
   ASF_SKIP_U32(p_ctx, size, "Error Correction Data Length");
   flags = ASF_READ_U16(p_ctx, size, "Flags");
   ASF_SKIP_U32(p_ctx, size, "Reserved");

   CHECK_POINT(p_ctx, size);

   /* Zero is not a valid stream id */
   if(!(flags & 0x7F)) goto skip;

   if(!memcmp(&stream_type, &asf_guid_stream_type_video, sizeof(GUID_T)))
      type = VC_CONTAINER_ES_TYPE_VIDEO;
   else if(!memcmp(&stream_type, &asf_guid_stream_type_audio, sizeof(GUID_T)))
      type = VC_CONTAINER_ES_TYPE_AUDIO;

   /* Check we know what to do with this track */
   if(type == VC_CONTAINER_ES_TYPE_UNKNOWN) goto skip;

   /* Sanity check sizes */
   if(ts_length > size) return VC_CONTAINER_ERROR_CORRUPTED;

   p_track = asf_reader_find_track( p_ctx, flags, true);
   if(!p_track) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

   p_track->format->es_type = type;

   offset = STREAM_POSITION(p_ctx);
   if(type == VC_CONTAINER_ES_TYPE_AUDIO)
      status = asf_read_waveformatex(p_ctx, p_track, (int64_t)ts_length);
   else if(type == VC_CONTAINER_ES_TYPE_VIDEO)
      status = asf_read_bitmapinfoheader(p_ctx, p_track, (int64_t)ts_length);
   size -= STREAM_POSITION(p_ctx) - offset;

   if(status) return status;

   p_track->priv->module->b_valid = true;
   p_track->is_enabled = true;
   p_track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;

   /* Codec specific work-arounds */
   switch(p_track->format->codec)
   {
   case VC_CONTAINER_CODEC_MPGA:
      /* Can't guarantee that the data is framed */
      p_track->format->flags &= ~VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
      break;
   default: break;
   }

 skip:
   if(size) SKIP_BYTES(p_ctx, size);
   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF extended stream properties object */
static VC_CONTAINER_STATUS_T asf_read_object_ext_stream_properties( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_TRACK_T *p_track;
   unsigned int i, name_count, pes_count, length, stream_id;

   ASF_SKIP_U64(p_ctx, size, "Start Time");
   ASF_SKIP_U64(p_ctx, size, "End Time");
   ASF_SKIP_U32(p_ctx, size, "Data Bitrate");
   ASF_SKIP_U32(p_ctx, size, "Buffer Size");
   ASF_SKIP_U32(p_ctx, size, "Initial Buffer Fullness");
   ASF_SKIP_U32(p_ctx, size, "Alternate Data Bitrate");
   ASF_SKIP_U32(p_ctx, size, "Alternate Buffer Size");
   ASF_SKIP_U32(p_ctx, size, "Alternate Initial Buffer Fullness");
   ASF_SKIP_U32(p_ctx, size, "Maximum Object Size");
   ASF_SKIP_U32(p_ctx, size, "Flags");
   stream_id = ASF_READ_U16(p_ctx, size, "Stream Number");
   ASF_SKIP_U16(p_ctx, size, "Stream Language ID Index");
   ASF_SKIP_U64(p_ctx, size, "Average Time Per Frame");
   name_count = ASF_READ_U16(p_ctx, size, "Stream Name Count");
   pes_count = ASF_READ_U16(p_ctx, size, "Payload Extension System Count");

   CHECK_POINT(p_ctx, size);

   p_track = asf_reader_find_track( p_ctx, stream_id, true);
   if(!p_track) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

   /* Stream Names */
   for(i = 0; i < name_count; i++)
   {
      if(size < 4) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_U16(p_ctx, size, "Language ID Index");
      length = ASF_READ_U16(p_ctx, size, "Stream Name Length");
      if(size < length) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_BYTES(p_ctx, size, length); /* Stream Name */
   }

   CHECK_POINT(p_ctx, size);

   /* Payload Extension Systems */
   for(i = 0; i < pes_count; i++)
   {
      if(size < 22) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_GUID(p_ctx, size, "Extension System ID");
      ASF_SKIP_U16(p_ctx, size, "Extension Data Size");
      length = ASF_READ_U32(p_ctx, size, "Extension System Info Length");
      if(size < length) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_BYTES(p_ctx, size, length); /* Extension System Info */
   }

   CHECK_POINT(p_ctx, size);

   /* Optional Stream Properties Object */
   if(size >= ASF_OBJECT_HEADER_SIZE)
      status = asf_read_object(p_ctx, size);

   return status;
}

/** Reads an ASF simple index object */
static VC_CONTAINER_STATUS_T asf_read_object_simple_index( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = 0;
   uint64_t time_interval, index_duration;
   uint32_t count;
   unsigned int i;

   ASF_SKIP_GUID(p_ctx, size, "File ID");

   /* time in 100nS units, converted to uS */
   time_interval = ASF_READ_U64(p_ctx, size, "Index Entry Time Interval") / UINT64_C(10);
   ASF_SKIP_U32(p_ctx, size, "Maximum Packet Count");
   count = ASF_READ_U32(p_ctx, size, "Index Entries Count");

   CHECK_POINT(p_ctx, size);

   if(count > size / 6)
   {
      LOG_DEBUG(p_ctx, "invalid number of entries in the index (%i, %"PRIi64")", count, size / 6);
      count = (uint32_t)(size / 6);
   }

   /* Find the track corresponding to this index */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      if(p_ctx->tracks[i]->format->es_type != VC_CONTAINER_ES_TYPE_VIDEO) continue;
      if(p_ctx->tracks[i]->priv->module->simple_index.offset) continue;
      break;
   }

   /* Skip the index if we can't find the associated track */
   if(i == p_ctx->tracks_num || !count || !time_interval) return VC_CONTAINER_SUCCESS;
   track_module = p_ctx->tracks[i]->priv->module;

   track_module->simple_index.offset = STREAM_POSITION(p_ctx);
   track_module->simple_index.time_interval = time_interval;
   track_module->simple_index.num_entries = count;

   /* Check that the index covers the whole duration of the stream */
   index_duration = (count * time_interval);
   if(module->preroll + module->time_offset < index_duration)
      index_duration -= module->preroll + module->time_offset;
   else
      index_duration = 0;

   if((uint64_t)module->duration > index_duration + time_interval)
   {
      track_module->simple_index.incomplete = true;
   }

   LOG_DEBUG(p_ctx, "index covers %fS on %fS",
      (float)index_duration / 1E6, (float)module->duration / 1E6);

#if defined(ENABLE_CONTAINERS_LOG_FORMAT) && defined(ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE)
   for(i = 0; i < count; i++)
   {
      LOG_FORMAT(p_ctx, "Entry: %u", i);
      ASF_SKIP_U32(p_ctx, size, "Packet Number");
      ASF_SKIP_U16(p_ctx, size, "Packet Count");
      if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) break;
   }
   size = i * 6;
#else
   size = CACHE_BYTES(p_ctx, count * 6);
#endif

   /* Check that the index is complete */
   if(size / 6 != count )
   {
      LOG_DEBUG(p_ctx, "index is incomplete (%i entries on %i)", (int)size / 6, count);
      track_module->simple_index.num_entries = (uint32_t)(size / 6);
      track_module->simple_index.incomplete = true;
   }

   /* If we haven't had an index before, or this track is enabled, we'll store this one.
    * (Usually there will only be one video track, and it will be enabled, so both tests
    * will pass. This check is an attempt to handle content not structured as it should be) */
   if ((!module->simple_index_track) || (p_ctx->tracks[i]->is_enabled))
   {
      /* Save the track so we don't have to look for it in when seeking */
      module->simple_index_track = track_module;
   }

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF index object */
static VC_CONTAINER_STATUS_T asf_read_object_index( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t i, specifiers_count, blocks_count;
   uint32_t best_specifier_type[ASF_TRACKS_MAX] = {0};

   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   /* Read the time interval and scale to microseconds */
   module->top_level_index.entry_time_interval
      = (uint64_t)ASF_READ_U32(p_ctx, size, "Index Entry Time Interval") * INT64_C(1000);

   module->top_level_index.specifiers_count
      = specifiers_count
      = (uint32_t)ASF_READ_U16(p_ctx, size, "Index Specifiers Count");

   module->top_level_index.block_count
      = blocks_count
      = ASF_READ_U32(p_ctx, size, "Index Blocks Count");

   CHECK_POINT(p_ctx, size);

   /* Index specifiers. Search for the one we like best */
   if(size < specifiers_count * 4) return VC_CONTAINER_ERROR_CORRUPTED;
   for(i = 0; i < specifiers_count; i++)
   {
      uint32_t stream_id = (uint32_t)ASF_READ_U16(p_ctx, size, "Stream Number");
      uint32_t index_type = (uint32_t)ASF_READ_U16(p_ctx, size, "Index Type");

      /* Find the track index for this stream */
      unsigned track = module->stream_number_to_index[stream_id];

      if ((track < ASF_TRACKS_MAX) &&
         (index_type > best_specifier_type[track]))
      {
         /* We like this better than any we have seen before. Note - if we don't like any
          * the file must be subtly corrupt - best we say nothing, and attempt a seek with
          * the data for the first specifier, it will be better than nothing. At worst it
          * will play until a seek is attempted */
         module->top_level_index.active_specifiers[track] = i;
         best_specifier_type[track] = index_type;
      }
   }

   for (i = 0; i < p_ctx->tracks_num; i++)
   {
      LOG_DEBUG(p_ctx, "indexing track %"PRIu32" with specifier %"PRIu32,
         i, module->top_level_index.active_specifiers[i]);
   }

   CHECK_POINT(p_ctx, size);

   /* The blocks start here */
   module->top_level_index.blocks_offset = STREAM_POSITION(p_ctx);

   /* Index blocks */
#if !(defined(ENABLE_CONTAINERS_LOG_FORMAT) && defined(ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE))
   blocks_count = 0; /* Don't log the index. Note we'll get a warning on unprocessed data */
#endif
   /* coverity[dead_error_condition] Code needs to stay there for debugging purposes */
   for(i = 0; i < blocks_count; i++)
   {
      uint32_t j, k, count = ASF_READ_U32(p_ctx, size, "Index Entry Count");

      for(j = 0; j < specifiers_count; j++)
      {
         ASF_SKIP_U64(p_ctx, size, "Block Positions");
      }
      for(j = 0; j < count; j++)
      {
         for(k = 0; k < specifiers_count; k++)
         {
            ASF_SKIP_U32(p_ctx, size, "Offsets");
         }
      }
   }

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF index parameters object */
static VC_CONTAINER_STATUS_T asf_read_object_index_parameters( VC_CONTAINER_T *p_ctx, int64_t size )
{
#ifdef ENABLE_CONTAINERS_LOG_FORMAT
   /* This is added for debugging only. The spec (section 4.9) states that this is also enclosed in
    * the index object (see above) and that they are identical. I think they aren't always... */
   uint32_t i, specifiers_count;

   /* Read the time interval in milliseconds */
   ASF_SKIP_U32(p_ctx, size, "Index Entry Time Interval");

   specifiers_count = (uint32_t)ASF_READ_U16(p_ctx, size, "Index Specifiers Count");

   CHECK_POINT(p_ctx, size);

   /* Index specifiers. Search for the one we like best */
   if(size < specifiers_count * 4) return VC_CONTAINER_ERROR_CORRUPTED;
   for(i = 0; i < specifiers_count; i++)
   {
      ASF_SKIP_U16(p_ctx, size, "Stream Number");
      ASF_SKIP_U16(p_ctx, size, "Index Type");
   }
#endif
   CHECK_POINT(p_ctx, size);

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF codec list object */
static VC_CONTAINER_STATUS_T asf_read_object_codec_list( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t i, count, length;

   ASF_SKIP_GUID(p_ctx, size, "Reserved");
   count = ASF_READ_U32(p_ctx, size, "Codec Entries Count");

   CHECK_POINT(p_ctx, size);

   /* Codec entries */
   for(i = 0; i < count; i++)
   {
      ASF_SKIP_U16(p_ctx, size, "Type");
      length = ASF_READ_U16(p_ctx, size, "Codec Name Length");
      if(size < length) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_STRING(p_ctx, size, length * 2, "Codec Name");
      length = ASF_READ_U16(p_ctx, size, "Codec Description Length");
      if(size < length) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_STRING(p_ctx, size, length * 2, "Codec Description");
      length = ASF_READ_U16(p_ctx, size, "Codec Information Length");
      if(size < length) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_BYTES(p_ctx, size, length);

      CHECK_POINT(p_ctx, size);
   }

   return status;
}

/** Reads an ASF content description object */
static VC_CONTAINER_STATUS_T asf_read_object_content_description( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint16_t t_length, a_length, c_length, d_length, r_length;

   t_length = ASF_READ_U16(p_ctx, size, "Title Length");
   a_length = ASF_READ_U16(p_ctx, size, "Author Length");
   c_length = ASF_READ_U16(p_ctx, size, "Copyright Length");
   d_length = ASF_READ_U16(p_ctx, size, "Description Length");
   r_length = ASF_READ_U16(p_ctx, size, "Rating Length");

   CHECK_POINT(p_ctx, size);

   if(size < t_length) return VC_CONTAINER_ERROR_CORRUPTED;
   ASF_SKIP_STRING(p_ctx, size, t_length, "Title");
   if(size < a_length) return VC_CONTAINER_ERROR_CORRUPTED;
   ASF_SKIP_STRING(p_ctx, size, a_length, "Author");
   if(size < c_length) return VC_CONTAINER_ERROR_CORRUPTED;
   ASF_SKIP_STRING(p_ctx, size, c_length, "Copyright");
   if(size < d_length) return VC_CONTAINER_ERROR_CORRUPTED;
   ASF_SKIP_STRING(p_ctx, size, d_length, "Description");
   if(size < r_length) return VC_CONTAINER_ERROR_CORRUPTED;
   ASF_SKIP_STRING(p_ctx, size, r_length, "Rating");

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF stream bitrate properties object */
static VC_CONTAINER_STATUS_T asf_read_object_stream_bitrate_props( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint16_t i, count;

   count = ASF_READ_U16(p_ctx, size, "Bitrate Records Count");

   /* Bitrate records */
   if(size < count * 6) return VC_CONTAINER_ERROR_CORRUPTED;
   for(i = 0; i < count; i++)
   {
      ASF_SKIP_U16(p_ctx, size, "Flags");
      ASF_SKIP_U32(p_ctx, size, "Average Bitrate");
   }

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF content encryption object */
static VC_CONTAINER_STATUS_T asf_read_object_content_encryption( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t length;

   length = ASF_READ_U32(p_ctx, size, "Secret Data Length");
   ASF_SKIP_BYTES(p_ctx, size, length);

   length = ASF_READ_U32(p_ctx, size, "Protection Type Length");
   ASF_SKIP_BYTES(p_ctx, size, length);

   length = ASF_READ_U32(p_ctx, size, "Key ID Length");
   ASF_SKIP_BYTES(p_ctx, size, length);

   length = ASF_READ_U32(p_ctx, size, "License URL Length");
   ASF_SKIP_BYTES(p_ctx, size, length); /* null-terminated ASCII string */

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF extended content encryption object */
static VC_CONTAINER_STATUS_T asf_read_object_ext_content_encryption( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t length;

   length = ASF_READ_U32(p_ctx, size, "Data Size");
   ASF_SKIP_BYTES(p_ctx, size, length);

   return STREAM_STATUS(p_ctx);
}

/** Reads an ASF advanced content encryption object */
static VC_CONTAINER_STATUS_T asf_read_object_adv_content_encryption( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t i, count;

   count = ASF_READ_U16(p_ctx, size, "Content Encryption Records Count");

   for(i = 0; i < count; i++)
   {
      uint32_t j, rec_count, data_size, length;

      ASF_SKIP_GUID(p_ctx, size, "System ID");
      ASF_SKIP_U32(p_ctx, size, "System Version");
      rec_count = ASF_READ_U16(p_ctx, size, "Encrypted Object Record Count");

      CHECK_POINT(p_ctx, size);

      for(j = 0; j < rec_count; j++)
      {
         ASF_SKIP_U16(p_ctx, size, "Encrypted Object ID Type");
         length = ASF_READ_U16(p_ctx, size, "Encrypted Object ID Length");
         if(length > size) return VC_CONTAINER_ERROR_CORRUPTED;
         ASF_SKIP_BYTES(p_ctx, size, length);
         CHECK_POINT(p_ctx, size);
      }

      data_size = ASF_READ_U32(p_ctx, size, "Data Size");
      if(data_size > size) return VC_CONTAINER_ERROR_CORRUPTED;
      ASF_SKIP_BYTES(p_ctx, size, data_size);
      CHECK_POINT(p_ctx, size);
   }

   return STREAM_STATUS(p_ctx);
}

/** Skips over an object that is if a type we don't handle, or is nested too deep */
static VC_CONTAINER_STATUS_T asf_skip_unprocessed_object( VC_CONTAINER_T *p_ctx, int64_t size )
{
   LOG_DEBUG(p_ctx, "%"PRIi64" bytes ignored in unhandled object", size);

   if(size < ASF_MAX_OBJECT_SIZE)
      SKIP_BYTES(p_ctx, size);                     /* read a small amount */
   else
      SEEK(p_ctx, STREAM_POSITION(p_ctx) + size);  /* seek a large distance */

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_find_packet_header( VC_CONTAINER_T *p_ctx,
   ASF_PACKET_STATE *p_state )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int search_size = 64*1024; /* should be max packet size according to spec */
#ifdef ENABLE_CONTAINER_LOG_DEBUG
   uint64_t offset = STREAM_POSITION(p_ctx);
#endif
   uint8_t h[3];
   VC_CONTAINER_PARAM_UNUSED(p_state);

   /* Limit the search up to what should theoretically be the packet boundary */
   if(module->packet_size)
      search_size = module->packet_size -
         (STREAM_POSITION(p_ctx) - module->data_offset) % module->packet_size;

   for(; search_size > sizeof(h); search_size--)
   {
      if(PEEK_BYTES(p_ctx, h, sizeof(h)) != sizeof(h))
         return STREAM_STATUS(p_ctx);

      if(!h[0] && !h[1] && h[2] == 0x82)
      {
         search_size = 2;
         break; /* Got it */
      }

      SKIP_BYTES(p_ctx, 1);
   }

   /* If we failed, we just skip to the theoretical packet boundary */
   SKIP_BYTES(p_ctx, search_size);

   LOG_DEBUG(p_ctx, "found potential sync, discarded %"PRIu64" bytes)",
             STREAM_POSITION(p_ctx) - offset);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_read_packet_header( VC_CONTAINER_T *p_ctx,
   ASF_PACKET_STATE *p_state, uint64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint64_t offset = STREAM_POSITION(p_ctx);
   uint8_t flags, property_flags, length;
   VC_CONTAINER_PARAM_UNUSED(size);

   p_state->start = offset;

   LOG_FORMAT(p_ctx, "Packet Offset: %"PRIu64, offset);

   if ((module->data_size > 0) && (offset >= (module->data_size + module->data_offset)))
   {
      return VC_CONTAINER_ERROR_EOS;
   }

   /* Find out whether we are dealing with error correction data or payload parsing info */
   if( PEEK_U8(p_ctx) >> 7 )
   {
      /* We have error correction data */
      flags = READ_U8(p_ctx, "Error Correction Flags");
      length = flags & 0xF;
      SKIP_BYTES(p_ctx, length); /* Error correction data */
   }

   /* Payload parsing information */
   flags = READ_U8(p_ctx, "Length Type Flags");
   p_state->multiple_payloads = flags & 1;
   property_flags = READ_U8(p_ctx, "Property Flags");
   p_state->replicated_data_lt = (property_flags >> 0) & 0x3;
   p_state->offset_into_media_object_lt = (property_flags >> 2) & 0x3;
   p_state->media_object_number_lt = (property_flags >> 4) & 0x3;

   /* Sanity check stream number length type */
   if(((property_flags >> 6) & 0x3) != 1)
      goto error;

   /* If there's no packet size field we default to the size in the file header. */
   p_state->size = READ_VLC(p_ctx, (flags >> 5) & 0x3 /* Packet length type */,
                             module->packet_size, "Packet Length");

   READ_VLC(p_ctx, (flags>>1)&0x3 /* Sequence type */, 0, "Sequence");
   p_state->padding_size = READ_VLC(p_ctx, (flags>>3)&0x3 /* Padding length type */, 0, "Padding Length");
   p_state->send_time = READ_U32(p_ctx, "Send Time") * UINT64_C(1000); /* Read in millisecond units, stored in uS */
   SKIP_U16(p_ctx, "Duration"); /* in milliseconds */

   p_state->num_payloads = 1;
   p_state->current_payload = 0;
   if(p_state->multiple_payloads)
   {
      LOG_FORMAT(p_ctx, "Multiple Payloads");
      flags = READ_U8(p_ctx, "Payload Flags");
      p_state->num_payloads = flags & 0x3F;
      LOG_FORMAT(p_ctx, "Number of Payloads: %i", p_state->num_payloads);
      p_state->payload_lt = (flags >> 6) & 3;

      /* Sanity check */
      if(!p_state->num_payloads) goto error;
   }

   /* Update the current offset in the packet. */
   p_state->current_offset = STREAM_POSITION(p_ctx) - offset;

   /* Sanity check offset */
   if(p_state->current_offset > p_state->size) goto error;

   /* Sanity check padding size */
   if(p_state->padding_size + p_state->current_offset > p_state->size) goto error;

   /* Sanity check packet size */
   if(!module->broadcast &&
      (p_state->size != module->packet_size)) goto error;

   return STREAM_STATUS(p_ctx);

 error:
   LOG_FORMAT(p_ctx, "Invalid payload parsing information (offset %"PRIu64")", STREAM_POSITION(p_ctx));
   return STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS ?
      VC_CONTAINER_ERROR_CORRUPTED : STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_read_payload_header( VC_CONTAINER_T *p_ctx,
   ASF_PACKET_STATE *p_state /* uint64_t size */ )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t rep_data_length;

   if(p_state->current_payload >= p_state->num_payloads)
      return VC_CONTAINER_ERROR_CORRUPTED;

   p_state->stream_num = READ_U8(p_ctx, "Stream Number");
   if(!(p_state->stream_num & 0x7F)) return VC_CONTAINER_ERROR_CORRUPTED;

   p_state->media_object_num = READ_VLC(p_ctx, p_state->media_object_number_lt, 0, "Media Object Number");

   /* For a compressed packet this field is a timestamp, and is moved to p_state->media_object_pts later */
   p_state->media_object_off = READ_VLC(p_ctx, p_state->offset_into_media_object_lt, 0, "Offset Into Media Object");
   rep_data_length = READ_VLC(p_ctx, p_state->replicated_data_lt, 0, "Replicated Data Length");

   /* Sanity check the replicated data length */
   if(rep_data_length && rep_data_length != 1 &&
      (rep_data_length < 8 ||
       STREAM_POSITION(p_ctx) - p_state->start + p_state->padding_size + rep_data_length > p_state->size))
   {
      LOG_FORMAT(p_ctx, "invalid replicated data length");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   /* Read what we need from the replicated data */
   if(rep_data_length > 1)
   {
      p_state->media_object_size = READ_U32(p_ctx, "Media Object Size");
      p_state->media_object_pts = READ_U32(p_ctx, "Presentation Time") * UINT64_C(1000);
      p_state->compressed_payloads = 0;
      SKIP_BYTES(p_ctx, rep_data_length - 8); /* Rest of replicated data */
   }
   else if(rep_data_length == 1)
   {
      LOG_FORMAT(p_ctx, "Compressed Payload Data");
      p_state->media_object_pts_delta = READ_U8(p_ctx, "Presentation Time Delta") * UINT64_C(1000);
      p_state->compressed_payloads = 1;

      /* Move the pts from media_object_off where it was read, and adjust it */
      p_state->media_object_off *= UINT64_C(1000);
      p_state->media_object_pts = p_state->media_object_off - p_state->media_object_pts_delta;
      p_state->media_object_off = 0;
      p_state->media_object_size = 0;
   }
   else
   {
      p_state->media_object_size = 0;
      p_state->media_object_pts = p_state->send_time;
      p_state->compressed_payloads = 0;
   }

   if(p_state->media_object_pts > module->preroll + module->time_offset)
      p_state->media_object_pts -= (module->preroll + module->time_offset);
   else p_state->media_object_pts = 0;

   p_state->payload_size = p_state->size - p_state->padding_size - (STREAM_POSITION(p_ctx) - p_state->start);
   if(p_state->multiple_payloads)
   {
      p_state->payload_size = READ_VLC(p_ctx, p_state->payload_lt, 0, "Payload Length");
      if(!p_state->payload_size) return VC_CONTAINER_ERROR_CORRUPTED;
   }
   else
      LOG_FORMAT(p_ctx, "Payload Length: %i", p_state->payload_size);

   if(p_state->payload_size >= p_state->size) return VC_CONTAINER_ERROR_CORRUPTED;

   p_state->subpayload_size = p_state->payload_size;

   /* Update current_offset to reflect the variable number of bytes we just read */
   p_state->current_offset = STREAM_POSITION(p_ctx) - p_state->start;

   /* Sanity check offset */
   if(p_state->current_offset > p_state->size) return VC_CONTAINER_ERROR_CORRUPTED;

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_read_object_data( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;
   VC_CONTAINER_PARAM_UNUSED(size);

   SKIP_GUID(p_ctx, "File ID");
   SKIP_U64(p_ctx, "Total Data Packets");
   SKIP_U16(p_ctx, "Reserved");
   module->data_offset = STREAM_POSITION(p_ctx);

   /* Initialise state for all tracks */
   module->packet_state.start = module->data_offset;
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *p_track = p_ctx->tracks[i];
      p_track->priv->module->p_packet_state = &module->packet_state;
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
/* Read the next sub-payload or next payload */
static VC_CONTAINER_STATUS_T asf_read_next_payload_header( VC_CONTAINER_T *p_ctx,
   ASF_PACKET_STATE *p_state, uint32_t *pi_track, uint32_t *pi_length)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;

   if(p_state->subpayload_size)
   {
      /* We still haven't read the current subpayload, return the info we already have */
      goto end;
   }

   /* Check if we're done reading a packet */
   if(p_state->current_payload >= p_state->num_payloads)
   {
      /* Skip the padding at the end */
      if(p_state->size)
      {
         int32_t pad_length = p_state->size - (STREAM_POSITION(p_ctx) - p_state->start);
         if(pad_length < 0) return VC_CONTAINER_ERROR_CORRUPTED;
         SKIP_BYTES(p_ctx, pad_length); /* Padding */
      }

      /* Read the header for the next packet */
      module->object_level = 0; /* For debugging */
      status = asf_read_packet_header( p_ctx, p_state, (uint64_t)0/*size???*/ );
      module->object_level = 1; /* For debugging */
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   /* Check if we're done reading a payload */
   if(!p_state->payload_size)
   {
      /* Read the payload header */
      status = asf_read_payload_header( p_ctx, p_state );
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   /* For compressed payloads, payload_size != subpayload_size */
   if(p_state->compressed_payloads && p_state->payload_size)
   {
      p_state->payload_size--;
      p_state->subpayload_size = READ_U8(p_ctx, "Sub-Payload Data Length");
      if(p_state->subpayload_size > p_state->payload_size)
      {
         /* TODO: do something ? */
         LOG_DEBUG(p_ctx, "subpayload is too big");
         p_state->subpayload_size = p_state->payload_size;
      }
      p_state->media_object_off = 0;
      p_state->media_object_size = p_state->subpayload_size;
      p_state->media_object_pts += p_state->media_object_pts_delta;
   }

 end:
   /* We've read the payload header, return the requested info */
   if(pi_track) *pi_track = module->stream_number_to_index[p_state->stream_num & 0x7F];
   if(pi_length) *pi_length = p_state->subpayload_size;

   p_state->current_offset = STREAM_POSITION(p_ctx) - p_state->start;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
/* read the next payload (not sub-payload) */
static VC_CONTAINER_STATUS_T asf_read_next_payload( VC_CONTAINER_T *p_ctx,
   ASF_PACKET_STATE *p_state, uint8_t *p_data, uint32_t *pi_size )
{
   uint32_t subpayload_size = p_state->subpayload_size;

   if(p_data && *pi_size < subpayload_size) subpayload_size = *pi_size;

   if(!p_state->subpayload_size)
      return VC_CONTAINER_SUCCESS;

   p_state->payload_size -= subpayload_size;
   if(!p_state->payload_size) p_state->current_payload++;
   p_state->subpayload_size -= subpayload_size;
   p_state->media_object_off += subpayload_size;

   if(p_data) *pi_size = READ_BYTES(p_ctx, p_data, subpayload_size);
   else *pi_size = SKIP_BYTES(p_ctx, subpayload_size);

   p_state->current_offset += subpayload_size;

   if(*pi_size!= subpayload_size)
      return STREAM_STATUS(p_ctx);

   return VC_CONTAINER_SUCCESS;
}

/******************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
/*****************************************************************************/
/** Read data from the ASF file

@param   p_ctx    Context for the file being read from

@param   p_packet Packet information. Includes data buffer and stream ID as aprropriate.

@param   flags    Flags controlling the read.
                  May request reading only, skipping a packet or force access to a set track.

******************************************************************************/
static VC_CONTAINER_STATUS_T asf_reader_read( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *global_module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   ASF_PACKET_STATE *p_state;
   uint32_t buffer_size = 0, track, data_size;
   uint64_t state_pos;

   LOG_DEBUG(p_ctx, "asf_reader_read track %"PRIu32" flags %u", p_packet->track, flags);

   /* If a specific track has been selected, we need to use the track packet state */
   if(flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
   {
      vc_container_assert(p_packet->track < p_ctx->tracks_num);
      /* The state to use is the one referred to by the track we selected */
      p_state = p_ctx->tracks[p_packet->track]->priv->module->p_packet_state;
   }
   else
   {
      /* No specific track was selected. Read the next data from the global position. */
       p_state = &global_module->packet_state;
   }

   /* Stop if the stream can't be read */
   if(p_state->eos) return VC_CONTAINER_ERROR_EOS;
   if(p_state->corrupted) return VC_CONTAINER_ERROR_CORRUPTED;

   /* If we aren't in the right position in the file go there now. */
   state_pos = p_state->start + p_state->current_offset;
   if ((uint64_t)STREAM_POSITION(p_ctx) != state_pos)
   {
      LOG_DEBUG(p_ctx, "seeking from %"PRIu64" to %"PRIu64, STREAM_POSITION(p_ctx), state_pos);
      SEEK(p_ctx, state_pos);
   }

   /* Look at the next payload header */
   status = asf_read_next_payload_header( p_ctx, p_state, &track, &data_size );
   if((status == VC_CONTAINER_ERROR_CORRUPTED)
      && (p_state->bad_packets < ASF_MAX_CONSECUTIVE_CORRUPTED_PACKETS))
   {
      /* If the current packet is corrupted we will try to search for the next packet */
      uint32_t corrupted = p_state->bad_packets;
      LOG_DEBUG(p_ctx, "packet offset %"PRIi64" is corrupted", p_state->start);
      memset(p_state, 0, sizeof(*p_state));
      p_state->bad_packets = corrupted + 1;

      /* TODO: flag discontinuity */

      if(asf_find_packet_header(p_ctx, p_state) == VC_CONTAINER_SUCCESS)
      {
         p_state->start = STREAM_POSITION(p_ctx);
         return VC_CONTAINER_ERROR_CONTINUE;
      }
   }
   if(status == VC_CONTAINER_ERROR_EOS) p_state->eos = true;
   if(status == VC_CONTAINER_ERROR_CORRUPTED) p_state->corrupted = true;
   if(status != VC_CONTAINER_SUCCESS)
   {
      return status;
   }

   p_state->bad_packets = 0;

   /* bad track number or track is disabled */
   if(track >= p_ctx->tracks_num || !p_ctx->tracks[track]->is_enabled)
   {
      LOG_DEBUG(p_ctx, "skipping packet because track %u is invalid or disabled", track);

      /* Skip payload by reading with a null buffer */
      status = asf_read_next_payload(p_ctx, p_state, 0, &data_size );
      if(status != VC_CONTAINER_SUCCESS) return status;
      return VC_CONTAINER_ERROR_CONTINUE;
   }

   track_module = p_ctx->tracks[track]->priv->module;

   /* If we are reading from the global state, and the track we found is not on the global state,
    * either skip the data or reconnect it to the global state */
   if ((p_state == &global_module->packet_state) &&
      (track_module->p_packet_state != &global_module->packet_state))
   {
      uint64_t track_pos =
         track_module->p_packet_state->start
         + track_module->p_packet_state->current_offset;

      /* Check if the end of the current packet is beyond the track's position */
      if (track_pos > state_pos + track_module->p_packet_state->size)
      {
         LOG_DEBUG(p_ctx, "skipping packet from track %u as it has already been read", track);
         status = asf_read_next_payload(p_ctx, p_state, 0, &data_size);

         if(status != VC_CONTAINER_SUCCESS) return status;
         return VC_CONTAINER_ERROR_CONTINUE;
      }
      else
      {
         LOG_DEBUG(p_ctx, "switching track index %u location %"PRIu64" back to global state", track, track_pos);
         track_module->p_packet_state = &global_module->packet_state;

         /* Update the global state to the precise position */
         global_module->packet_state = track_module->local_packet_state;
         return VC_CONTAINER_ERROR_CONTINUE;
      }
   }

   /* If we are forcing, and the data is from a different track, skip it.
    * We may need to move the track we want onto a local state. */
   if ((flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
      && (track != p_packet->track))
   {
      track_module = p_ctx->tracks[p_packet->track]->priv->module;

      /* If the track we found is on the same state as the track we want they must both be on the global state */
      if (p_ctx->tracks[track]->priv->module->p_packet_state == p_state)
      {
         LOG_DEBUG(p_ctx, "switching track index %u location %"PRIu64" away from global state", p_packet->track, state_pos);

         /* Change the track we want onto a local state */
         track_module->p_packet_state = &track_module->local_packet_state;

         /* Copy the global state into the local state for the track we are forcing */
         track_module->local_packet_state = global_module->packet_state;
      }

      LOG_DEBUG(p_ctx, "skipping packet from track %u while forcing %u", track, p_packet->track);
      status = asf_read_next_payload(p_ctx, track_module->p_packet_state, 0, &data_size );
      return VC_CONTAINER_ERROR_CONTINUE;
   }

   /* If we arrive here either the data is from the track we are forcing, or we are not forcing
    * and we haven't already read the data while forcing that track */

   /* If skip, and no info required, skip over it and return now. */
   if((flags & VC_CONTAINER_READ_FLAG_SKIP) && !(flags & VC_CONTAINER_READ_FLAG_INFO))
      return asf_read_next_payload(p_ctx, p_state, 0, &data_size );

   /* Fill-in the packet information */
   if(p_state->media_object_pts == ASF_UNKNOWN_PTS || p_state->media_object_off)
      p_packet->dts = p_packet->pts = VC_CONTAINER_TIME_UNKNOWN;
   else
      p_packet->dts = p_packet->pts = p_state->media_object_pts;

   p_packet->flags = 0;

   if(p_state->stream_num >> 7) p_packet->flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;

   if(!p_state->media_object_off) p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;

   if(p_state->media_object_size &&
         p_state->media_object_off + data_size >= p_state->media_object_size)
      p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;

   if(!p_state->media_object_size)
      p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;

   p_packet->track = track;

   p_packet->frame_size = p_state->media_object_size;

   p_packet->size = data_size;

   /* If the skip flag is set (Info must have been too) skip the data and return */
   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      /* Skip payload by reading with a null buffer */
      return asf_read_next_payload(p_ctx, p_state, 0, &data_size );
   }
   else if(flags & VC_CONTAINER_READ_FLAG_INFO)
   {
      return VC_CONTAINER_SUCCESS;
   }

   /* Read the payload data */
   buffer_size = p_packet->buffer_size;
   status = asf_read_next_payload(p_ctx, p_state, p_packet->data, &buffer_size );
   if(status != VC_CONTAINER_SUCCESS)
   {
      /* FIXME */
      return status;
   }

   p_packet->size = buffer_size;
   if(buffer_size != data_size)
      p_packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;
   LOG_DEBUG(p_ctx, "asf_reader_read exit %u PTS %"PRIi64" track %"PRIu32, status, p_packet->pts, track);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_reader_index_find_time( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_MODULE_T* track_module, int64_t time, uint32_t *packet_num, bool forward )
{
   VC_CONTAINER_STATUS_T status;
   uint32_t entry, previous_packet_num;
   bool eos = false;

   /* Default to beginning of file in case of error */
   *packet_num = 0;

   /* Special case - time zero is beginning of file */
   if(time == 0) {return VC_CONTAINER_SUCCESS;}

   /* Sanity checking */
   if(!track_module->simple_index.num_entries) return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   if(!track_module->simple_index.time_interval) return VC_CONTAINER_ERROR_CORRUPTED;

   entry = time / track_module->simple_index.time_interval;
   LOG_DEBUG(p_ctx, "entry: %i, offset: %"PRIi64", interv: %"PRIi64, entry,
             track_module->simple_index.offset, track_module->simple_index.time_interval);
   if(entry >= track_module->simple_index.num_entries)
   {
      entry = track_module->simple_index.num_entries - 1;
      eos = true;
   }

   /* Fetch the entry from the index */
   status = SEEK(p_ctx, track_module->simple_index.offset + 6 * entry);
   if(status != VC_CONTAINER_SUCCESS) return status;
   *packet_num = READ_U32(p_ctx, "Packet Number");
   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) return STREAM_STATUS(p_ctx);

   /* When asking for the following keyframe we need to find the next entry with a greater
    * packet number */
   previous_packet_num = *packet_num;
   while(!eos && forward && previous_packet_num == *packet_num)
   {
      if(++entry == track_module->simple_index.num_entries) {eos = true; break;}
      status = SEEK(p_ctx, track_module->simple_index.offset + 6 * entry);
      if(status != VC_CONTAINER_SUCCESS) break;
      *packet_num = READ_U32(p_ctx, "Packet Number");
      if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) break;
   }

   if(eos && track_module->simple_index.incomplete) return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
   else if(eos) return VC_CONTAINER_ERROR_EOS;
   else return STREAM_STATUS(p_ctx);
}

#if 0
/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_reader_index_find_packet( VC_CONTAINER_T *p_ctx,
   unsigned int track, uint32_t *packet_num, bool forward )
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = 0;
   uint32_t i, prev_packet_num = 0, next_packet_num;
   bool eos = false;

   /* Sanity checking */
   if(track >= p_ctx->tracks_num) return VC_CONTAINER_ERROR_FAILED;
   track_module = p_ctx->tracks[track]->priv->module;
   if(!track_module->num_index_entries) return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   if(!track_module->index_time_interval) return VC_CONTAINER_ERROR_CORRUPTED;

   status = SEEK(p_ctx, track_module->index_offset);
   if(status != VC_CONTAINER_SUCCESS) return status;

   /* Loop through all the entries in the index */
   for(i = 0; i < track_module->num_index_entries; i++)
   {
      next_packet_num = READ_U32(p_ctx, "Packet Number");
      SKIP_U16(p_ctx, "Packet Count");
      if(next_packet_num > *packet_num) break;
      if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) break;
      prev_packet_num = next_packet_num;
   }
   if(i == track_module->num_index_entries ) eos = true;

   if(STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS && !eos)
   {
      if(forward) *packet_num = next_packet_num;
      else *packet_num = prev_packet_num;
   }

   if(eos && track_module->index_incomplete) return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
   else if(eos) return VC_CONTAINER_ERROR_EOS;
   else return STREAM_STATUS(p_ctx);
}
#endif

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_reader_find_next_frame( VC_CONTAINER_T *p_ctx,
   unsigned int track, ASF_PACKET_STATE *p_state, bool keyframe, bool timeout )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t data_track, data_size;
   unsigned int packets = 0;

   if(p_ctx->tracks[track]->format->es_type != VC_CONTAINER_ES_TYPE_VIDEO)
      keyframe = false;

   /* We still need to go to the right payload */
   while(status == VC_CONTAINER_SUCCESS &&
         (!timeout || packets++ < ASF_MAX_SEARCH_PACKETS))
   {
      status = asf_read_next_payload_header( p_ctx, p_state, &data_track, &data_size );
      if(status != VC_CONTAINER_SUCCESS) break;

      if(data_track == track && ((p_state->stream_num >> 7) || !keyframe) &&
         !p_state->media_object_off) break;

      /* Skip payload */
      status = asf_read_next_payload(p_ctx, p_state, 0, &data_size );
   }

   return status;
}

/*****************************************************************************/
/* Helper for asf_reader_seek - seek when there is a top-level index (spec section 6.2) */
static VC_CONTAINER_STATUS_T seek_by_top_level_index(
   VC_CONTAINER_T *p_ctx,
   int64_t *p_time,
   VC_CONTAINER_SEEK_MODE_T mode,
   VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned index;
   uint64_t time = 0;
   uint64_t block_address = module->top_level_index.blocks_offset;
   uint64_t track_positions[ASF_TRACKS_MAX];

   VC_CONTAINER_PARAM_UNUSED(mode);
   LOG_DEBUG(p_ctx, "seek_by_top_level_index");

   for (index = 0; index < ASF_TRACKS_MAX; ++index)
   {
      /* Set all to a stupid value */
      track_positions[index] = UINT64_MAX;
   }

   /* Loop through the index blocks to find the one(s) that deal with the time(s) in question.
      * Note that most ASF files only have one index block. */
   for (index = 0; index < module->top_level_index.block_count; ++index)
   {
      uint64_t block_duration, block_position;
      uint32_t index_entry_count, stream;
      LOG_DEBUG(p_ctx, "looking for index blocks at offset %"PRIu64, block_address);
      status = SEEK(p_ctx, block_address);
      if(status != VC_CONTAINER_SUCCESS) return status;

      /* Read the number of entries for this index block. */
      index_entry_count = READ_U32(p_ctx, "Index Entry Count");

      /* Turn into a duration */
      block_duration = (uint64_t)index_entry_count * module->top_level_index.entry_time_interval;

      /* Go through each stream */
      for (stream = 0; stream < p_ctx->tracks_num; ++stream)
      {
         /* Work out the track's target time */
         uint64_t track_time = *p_time + module->preroll + module->time_offset;

         /* Have we the correct index block for the seek time? */
         if ((time <= track_time) && (track_time < time + block_duration))
         {
            /* We have the correct index block for the seek time. Work out where in it. */
            uint32_t block_index = (track_time - time) / module->top_level_index.entry_time_interval;
            uint64_t active_specifier = module->top_level_index.active_specifiers[stream];
            uint64_t new_position;

            /* Read the Block Positions value for the correct specifier */
            status = SEEK(p_ctx,
               block_address + INT64_C(4)
               + active_specifier * INT64_C(8));
            if (status != VC_CONTAINER_SUCCESS)
            {
               return status;
            }
            block_position = READ_U32(p_ctx, "Block Position");

            /* Read the target address for the stream */
            status = SEEK(p_ctx, block_address + 4                                        /* skip index entry count */
               + (UINT64_C(8) * module->top_level_index.specifiers_count)                 /* block positions */
               + (UINT64_C(4) * module->top_level_index.specifiers_count * block_index)   /* prior index entries */
               + (UINT64_C(4) * active_specifier));                                       /* correct specifier */
            LOG_DEBUG(p_ctx, "reading at %"PRIu64, STREAM_POSITION(p_ctx));

            new_position = module->data_offset + block_position + (uint64_t)READ_U32(p_ctx, "Offset");
            LOG_DEBUG(p_ctx, "actual address for stream %"PRIu32" = %"PRIu64, stream, new_position);
            track_positions[stream] = new_position;
         }
      }

      /* Work out where the next block is */
      block_address += (UINT64_C(8) * module->top_level_index.specifiers_count)
            +  (UINT64_C(4) * module->top_level_index.specifiers_count * index_entry_count);
   }

   return seek_to_positions(p_ctx, track_positions, p_time, flags, 0, 0);
}

/* Helper for asf_reader_seek -
 * Given a set of positions seek the tracks. The status is the result of physically seeking each one.
 * It is expected that the positions will be before *p_time; if the flags require it search
 * for the next keyframe that is at or above *p_time. */
static VC_CONTAINER_STATUS_T seek_to_positions(VC_CONTAINER_T *p_ctx, uint64_t track_positions[ASF_TRACKS_MAX],
   int64_t *p_time, VC_CONTAINER_SEEK_FLAGS_T flags,
   unsigned int start_track, bool seek_on_start_track)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint64_t global_position = UINT64_MAX;
   unsigned int lowest_track, index, tracks;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   int64_t track_best_pts[ASF_TRACKS_MAX];

   if (*p_time == 0)
   {
      // Special case: Time 0 means beginning of file. Don't search for the matching packet(s).
      memset(&module->packet_state, 0, sizeof(module->packet_state));
      module->packet_state.start = track_positions[0];
      status = SEEK(p_ctx, module->packet_state.start);

      // Set each track to using the global state
      for(index = 0; index < p_ctx->tracks_num; index++)
      {
         p_ctx->tracks[index]->priv->module->p_packet_state = &module->packet_state;
      }

      return status;
   }

   for(tracks = 0, index = start_track; tracks < p_ctx->tracks_num;
       tracks++, index = (index+1) % p_ctx->tracks_num)
   {
      uint32_t data_size;

      /* Use an on-stack packet state. We can't use the global state, as we must leave it at
       * the lowest position. We can't use any track's private state, as we will move it past
       * the desired location. */
      ASF_PACKET_STATE private_state;
      memset(&private_state, 0, sizeof(private_state));

      track_best_pts[index] = INT64_MAX;

      status = SEEK(p_ctx, track_positions[index]);

      /* loop until we find the packet we're looking for.
       * stop when we've seen a big enough PTS, and are on a key frame */
      while(status == VC_CONTAINER_SUCCESS)
      {
         /* Get the next key-frame */
         status = asf_reader_find_next_frame(p_ctx, index, &private_state, true, true);
         if(status == VC_CONTAINER_SUCCESS)
         {
            /* Get the PTS, if any */
            int64_t pts = (int64_t)private_state.media_object_pts;

            if(pts != ASF_UNKNOWN_PTS)
            {
               if ((track_best_pts[index] == INT64_MAX)        /* we don't have a time yet */
                  || (pts <= *p_time)                          /* it's before our target */
                  || (flags & VC_CONTAINER_SEEK_FLAG_FORWARD)) /* we want after target */
               {
                  /* Store this time. It's the best yet. */
                  track_best_pts[index] = pts;

                  /* Update the desired position */
                  track_positions[index] = private_state.start + private_state.current_offset;

                  /* Copy the local state into this track's private state */
                  p_ctx->tracks[index]->priv->module->local_packet_state = private_state;

                  LOG_DEBUG(p_ctx, "seek forward track %u to pts %"PRIu64,
                     index, track_best_pts[index]);
               }

               /* If we've got to our target time we can stop. */
               if (pts >= *p_time)
               {
                  /* Then stop. */
                  break;
               }
            }

            status = asf_read_next_payload(p_ctx, &private_state, 0, &data_size );
         }
      }

      /* If we are seeking using a specific track, usually this is the video track
       * and we want all the other tracks to start at the same time or later */
      if (seek_on_start_track && start_track == index)
      {
         flags |= VC_CONTAINER_SEEK_FLAG_FORWARD;
         *p_time = track_best_pts[index];
      }

      {
         ASF_PACKET_STATE *p_state = &p_ctx->tracks[index]->priv->module->local_packet_state;

         LOG_DEBUG(p_ctx, "seek track %u to pts %"PRIu64" (key:%i,moo:%i)",
            index, track_best_pts[index], p_state->stream_num >> 7, p_state->media_object_off);
      }
   }

   /* Find the smallest track address in track_positions. This will be the global position */
   /* Also the lowest PTS in track_best_pts, this will be the new global PTS */
   for (index = 0, lowest_track = 0; index < p_ctx->tracks_num; ++index)
   {
      /* If it is smaller, remember it */
      if (track_positions[index] < global_position)
      {
         global_position = track_positions[index];
         lowest_track = index;
      }

      /* Put the lowest PTS into entry 0 of the array */
      if ((track_best_pts[index] != INT64_MAX) && (track_best_pts[index] < track_best_pts[0]))
      {
         track_best_pts[0] = track_best_pts[index];
      }
   }

   /* Update the caller with the lowest real PTS, if any. (we may have already done this above) */
   if (track_best_pts[0] != INT64_MAX)
   {
      *p_time = track_best_pts[0];
   }
   else
   {
      LOG_DEBUG(p_ctx, "no PTS suitable to update the caller");
   }

   /* As we did an extra read on the index track past the desired location seek back to it */
   status = SEEK(p_ctx, global_position);

   /* Copy the packet state for the stream with the lowest address into the global state */
   module->packet_state = p_ctx->tracks[lowest_track]->priv->module->local_packet_state;

   for(index = 0; index < p_ctx->tracks_num; index++)
   {
      VC_CONTAINER_TRACK_MODULE_T* track_mod = p_ctx->tracks[index]->priv->module;

      /* If the track position is the global position, or it is invalid, use the global state */
      if ((track_positions[index] <= global_position) || (track_positions[index] == UINT64_MAX))
      {
         track_mod->p_packet_state = &module->packet_state;
      }
      else
      {
         /* Track is not at the global position. Use the local state. */
         LOG_DEBUG(p_ctx, "track %u local position %"PRIu64, index, track_positions[index]);
         track_mod->p_packet_state = &track_mod->local_packet_state;
      }
   }

   return status;
}

/*****************************************************************************/
/* Seek to a location in the file, using whatever indices are available
 * If flags bit VC_CONTAINER_SEEK_FLAG_FORWARD is set the position is guaranteed to
 * be a keyframe at or after the requested location. Conversely if it is not set
 * the position is guaranteed to be at or before the request. */
static VC_CONTAINER_STATUS_T asf_reader_seek( VC_CONTAINER_T *p_ctx, int64_t *p_time,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_EOS;      /* initialised to known fail state */
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int stream;

   VC_CONTAINER_PARAM_UNUSED(mode);

   LOG_DEBUG(p_ctx, "asf_reader_seek");

   /* Prefer the top-level index to the simple index - it has byte offsets not packet offsets,
   * and is likely to have separate tables for every track */
   if (module->top_level_index.block_count)
   {
      status = seek_by_top_level_index(p_ctx, p_time, mode, flags);
   }
   else
   {
      uint64_t track_positions[ASF_TRACKS_MAX];
      int seek_track = -1;
      uint32_t packet_num;
      uint64_t new_position;

      if (*p_time == 0)
      {
         // Special optimisation - for time zero just go to the beginning.
         packet_num = 0;
      }
      /* If there is a simple index use the packet number from it */
      else if (module->simple_index_track)
      {
         /* Correct time desired */
         uint64_t track_time = *p_time + module->preroll + module->time_offset;

         LOG_DEBUG(p_ctx, "using simple index");

         /* Search the index for the correct packet */
         status = asf_reader_index_find_time(p_ctx, module->simple_index_track, track_time,
            &packet_num, flags & VC_CONTAINER_SEEK_FLAG_FORWARD);
      }
      else
      {
         /* No index at all. Use arithmetic to guess the packet number. */
         LOG_DEBUG(p_ctx, "index not usable %u", (unsigned)status);

         if (module->packets_num == 0)
         {
            /* This is a broadcast stream, and we can't do the arithmetic.
            * Set it to a value that will guarantee a seek fail. */
            LOG_DEBUG(p_ctx, "no packets in file");
            packet_num = UINT32_MAX;
         }
         else
         {
            packet_num = *p_time * module->packets_num / module->duration;
         }
      }

      /* calculate the byte address of the packet, relative to the start of data */
      new_position = (uint64_t)packet_num * (uint64_t)module->packet_size;

      LOG_DEBUG(p_ctx, "packet number %"PRIu32" approx byte offset %"PRIu64 , packet_num, new_position + module->data_offset);
      if (new_position > (uint64_t)module->data_size)
      {
         new_position = module->data_size;
         LOG_DEBUG(p_ctx, "arithmetic error, seeking to end of file %" PRIu64 , new_position + module->data_offset);
      }

      new_position += module->data_offset;

      for(stream = 0; stream < p_ctx->tracks_num; stream++)
      {
         /* Use the 1st enabled video track as the seek stream */
         if(p_ctx->tracks[stream]->format->es_type ==
               VC_CONTAINER_ES_TYPE_VIDEO &&
            p_ctx->tracks[stream]->is_enabled && seek_track < 0)
            seek_track = stream;

         track_positions[stream] = new_position;
      }     /* repeat for all tracks */

      /* Work out if we actually got anywhere. If so, save the positions for the subsequent reads */
      status = seek_to_positions(p_ctx, track_positions, p_time, flags,
         seek_track < 0 ? 0 : seek_track, seek_track >= 0);
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T asf_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

/* FIXME: metadata is currently shared across all readers so freeing
          it is left to the common layer but this isn't necessarily
          the best solution.
   for(i = 0; i <p_ctx->meta_num; i++)
      free(p_ctx->meta[i]);
   if(p_ctx->meta_num) free(p_ctx->meta);
   p_ctx->meta_num = 0;
*/
   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   p_ctx->tracks_num = 0;
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T asf_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   unsigned int i;
   GUID_T guid;

   /* Check for an ASF top-level header object */
   if(PEEK_BYTES(p_ctx, (uint8_t *)&guid, sizeof(guid)) < sizeof(guid) ||
      memcmp(&guid, &asf_guid_header, sizeof(guid)))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /*
    *  We are dealing with an ASF file
    */

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));

   /* Set the translation table to all error values */
   memset(&module->stream_number_to_index, 0xff, sizeof(module->stream_number_to_index));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;

   /* Read the top level header object */
   status = asf_read_object(p_ctx, INT64_C(0));
   if(status != VC_CONTAINER_SUCCESS)
      goto error;

   /* Bail out if we didn't find a track */
   if(!p_ctx->tracks_num) {status = VC_CONTAINER_ERROR_NO_TRACK_AVAILABLE; goto error;}

   /*
    *  The top level data object must come next
    */
   if(READ_GUID(p_ctx, &guid, "Object ID") != sizeof(guid) ||
      memcmp(&guid, &asf_guid_data, sizeof(guid)))
      goto error;

   LOG_FORMAT(p_ctx, "Object Name: data");
   module->data_size = READ_U64(p_ctx, "Object Size");

   /* If the data size was supplied remove the size of the common object header and the local header for this object */
   if(module->data_size) module->data_size -= ASF_OBJECT_HEADER_SIZE + 16 + 8 + 2;

   /* Sanity check the data object size */
   if(module->data_size < 0)
      goto error;

   module->object_level++;
   SKIP_GUID(p_ctx, "File ID");
   module->packets_num = READ_U64(p_ctx, "Total Data Packets");
   if(module->broadcast) module->packets_num = 0;
   SKIP_U16(p_ctx, "Reserved");

   if (module->packet_size)
   {
      LOG_DEBUG(p_ctx, "object size %"PRIu64" means %f packets",
         module->data_size, (float)(module->data_size) / (float)(module->packet_size));
   }

   module->data_offset = STREAM_POSITION(p_ctx);
   LOG_DEBUG(p_ctx, "expect end of data at address %"PRIu64, module->data_size + module->data_offset);

   module->object_level--;

   /*
    *  We now have all the information we really need to start playing the stream
    */

   /* Initialise state for all tracks */
   module->packet_state.start = module->data_offset;
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *p_track = p_ctx->tracks[i];
      p_track->priv->module->p_packet_state = &module->packet_state;
   }

   p_ctx->priv->pf_close = asf_reader_close;
   p_ctx->priv->pf_read = asf_reader_read;
   p_ctx->priv->pf_seek = asf_reader_seek;

   if(STREAM_SEEKABLE(p_ctx))
   {
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;
      p_ctx->capabilities |= VC_CONTAINER_CAPS_FORCE_TRACK;
   }

   p_ctx->duration = module->duration;

   /* Check if we're done */
   if(!module->data_size || !STREAM_SEEKABLE(p_ctx))
      return VC_CONTAINER_SUCCESS;

   /* If the stream is seekable and not a broadcast stream, we want to use any index there
    * might be at the end of the stream */

   /* Seek back to the end of the data object */
   if( SEEK(p_ctx, module->data_offset + module->data_size) == VC_CONTAINER_SUCCESS)
   {
      /* This will catch the simple index object if it is there */
      do {
         status = asf_read_object(p_ctx, INT64_C(0));
      } while(status == VC_CONTAINER_SUCCESS);
   }

   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      if(p_ctx->tracks[i]->priv->module->simple_index.offset)
         LOG_DEBUG(p_ctx, "track %i has an index", i);
   }

   /* Seek back to the start of the data */
   return SEEK(p_ctx, module->data_offset);

 error:
   if(status == VC_CONTAINER_SUCCESS) status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   LOG_DEBUG(p_ctx, "asf: error opening stream (%i)", status);
   if(module) asf_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open asf_reader_open
#endif
