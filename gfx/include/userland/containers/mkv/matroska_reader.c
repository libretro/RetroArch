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

//#define ENABLE_MKV_EXTRA_LOGGING
#define CONTAINER_IS_BIG_ENDIAN
#define CONTAINER_HELPER_LOG_INDENT(a) (a)->priv->module->element_level
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/
#define MKV_TRACKS_MAX 16
#define MKV_CODECID_MAX 32
#define MKV_MAX_LACING_NUM 64

#define MKV_MAX_ENCODINGS 1
#define MKV_MAX_ENCODING_DATA 256

#define MKV_MAX_ELEMENT_LEVEL 8
#define MKV_MAX_CONSECUTIVE_UNKNOWN_ELEMENTS 5
#define MKV_MAX_ELEMENT_SIZE  (1<<29) /* Does not apply to the data element */
#define MKV_MAX_STRING_SIZE 256
#define MKV_ELEMENT_MIN_HEADER_SIZE 2

#define MKV_MAX_READER_STATE_LEVEL 4

#define MKV_SKIP_U8(ctx,n)   (size -= 1, SKIP_U8(ctx,n))
#define MKV_SKIP_U16(ctx,n)  (size -= 2, SKIP_U16(ctx,n))
#define MKV_SKIP_U24(ctx,n)  (size -= 3, SKIP_U24(ctx,n))
#define MKV_SKIP_U32(ctx,n)  (size -= 4, SKIP_U32(ctx,n))
#define MKV_SKIP_U64(ctx,n)  (size -= 8, SKIP_U64(ctx,n))
#define MKV_READ_U8(ctx,n)   (size -= 1, READ_U8(ctx,n))
#define MKV_READ_U16(ctx,n)  (size -= 2, READ_U16(ctx,n))
#define MKV_READ_U24(ctx,n)  (size -= 3, READ_U24(ctx,n))
#define MKV_READ_U32(ctx,n)  (size -= 4, READ_U32(ctx,n))
#define MKV_READ_U64(ctx,n)  (size -= 8, READ_U64(ctx,n))
#define MKV_READ_BYTES(ctx,buffer,sz) (size -= sz, READ_BYTES(ctx,buffer,sz))
#define MKV_SKIP_BYTES(ctx,sz) (size -= sz, SKIP_BYTES(ctx,sz))

#define CHECK_POINT(a) do { \
   /*if(size < 0 && size != INT64_C(-1)) return VC_CONTAINER_ERROR_CORRUPTED;*/ \
   if(STREAM_STATUS(p_ctx)) return STREAM_STATUS(p_ctx); } while(0)

static uint32_t mkv_io_read_id(VC_CONTAINER_IO_T *io, int64_t *size)
{
   uint32_t value, mask;

   value = vc_container_io_read_uint8(io); (*size)--;
   for(mask = 0x80; mask; mask <<= 7)
   {
      if(value & mask) return value;
      value = (value << 8) | vc_container_io_read_uint8(io); (*size)--;
   }
   return 0;
}

static int64_t mkv_io_read_uint(VC_CONTAINER_IO_T *io, int64_t *size)
{
   uint64_t value, mask;

   value = vc_container_io_read_uint8(io); (*size)--;
   if(value == 0xFF) return -1;

   for(mask = 0x80; mask; mask <<= 7)
   {
      if(value & mask) return value & ~mask;
      value = (value << 8) | vc_container_io_read_uint8(io); (*size)--;
   }
   return 0;
}

static int64_t mkv_io_read_sint(VC_CONTAINER_IO_T *io, int64_t *size)
{
   int64_t value, count = io->offset;
   value = mkv_io_read_uint(io, size);
   count = io->offset - count;

   switch(count)
   {
   case 1: value -= 0x3F; break;
   case 2: value -= 0x1FFF; break;
   case 3: value -= 0xFFFFF; break;
   case 4: value -= 0x7FFFFFF; break;
   default: break;
   }
   return value;
}

#define MKV_READ_ID(ctx, n) mkv_io_read_id((ctx)->priv->io, &size)
#define MKV_READ_UINT(ctx, n) mkv_io_read_uint((ctx)->priv->io, &size)
#define MKV_READ_SINT(ctx, n) mkv_io_read_sint((ctx)->priv->io, &size)

/******************************************************************************
Type definitions.
******************************************************************************/

typedef enum
{
   MKV_ELEMENT_ID_UNKNOWN = 0,

   /* EBML Basics */
   MKV_ELEMENT_ID_EBML = 0x1A45DFA3,
   MKV_ELEMENT_ID_EBML_VERSION = 0x4286,
   MKV_ELEMENT_ID_EBML_READ_VERSION = 0x42F7,
   MKV_ELEMENT_ID_EBML_MAX_ID_LENGTH = 0x42F2,
   MKV_ELEMENT_ID_EBML_MAX_SIZE_LENGTH = 0x42F3,
   MKV_ELEMENT_ID_DOCTYPE = 0x4282,
   MKV_ELEMENT_ID_DOCTYPE_VERSION = 0x4287,
   MKV_ELEMENT_ID_DOCTYPE_READ_VERSION = 0x4285,

   /* Global Elements */
   MKV_ELEMENT_ID_CRC32 = 0xBF,
   MKV_ELEMENT_ID_VOID = 0xEC,

   /* Segment */
   MKV_ELEMENT_ID_SEGMENT = 0x18538067,

   /* Meta Seek Information */
   MKV_ELEMENT_ID_SEEK_HEAD = 0x114D9B74,
   MKV_ELEMENT_ID_SEEK = 0x4DBB,
   MKV_ELEMENT_ID_SEEK_ID = 0x53AB,
   MKV_ELEMENT_ID_SEEK_POSITION = 0x53AC,

   /* Segment Information */
   MKV_ELEMENT_ID_INFO = 0x1549A966,
   MKV_ELEMENT_ID_SEGMENT_UID = 0x73A4,
   MKV_ELEMENT_ID_SEGMENT_FILENAME = 0x7384,
   MKV_ELEMENT_ID_PREV_UID = 0x3CB923,
   MKV_ELEMENT_ID_PREV_FILENAME = 0x3C83AB,
   MKV_ELEMENT_ID_NEXT_UID = 0x3EB923,
   MKV_ELEMENT_ID_NEXT_FILENAME = 0x3E83BB,
   MKV_ELEMENT_ID_SEGMENT_FAMILY = 0x4444,
   MKV_ELEMENT_ID_CHAPTER_TRANSLATE = 0x6924,
   MKV_ELEMENT_ID_CHAPTER_TRANSLATE_EDITION_UID = 0x69FC,
   MKV_ELEMENT_ID_CHAPTER_TRANSLATE_CODEC = 0x69BF,
   MKV_ELEMENT_ID_CHAPTER_TRANSLATE_ID = 0x69A5,
   MKV_ELEMENT_ID_TIMECODE_SCALE = 0x2AD7B1,
   MKV_ELEMENT_ID_DURATION = 0x4489,
   MKV_ELEMENT_ID_DATE_UTC = 0x4461,
   MKV_ELEMENT_ID_TITLE = 0x7BA9,
   MKV_ELEMENT_ID_MUXING_APP = 0x4D80,
   MKV_ELEMENT_ID_WRITING_APP = 0x5741,

   /* Cluster */
   MKV_ELEMENT_ID_CLUSTER = 0x1F43B675,
   MKV_ELEMENT_ID_TIMECODE = 0xE7,
   MKV_ELEMENT_ID_SILENT_TRACKS = 0x5854,
   MKV_ELEMENT_ID_SILENT_TRACK_NUMBER = 0x58D7,
   MKV_ELEMENT_ID_POSITION = 0xA7,
   MKV_ELEMENT_ID_PREV_SIZE = 0xAB,
   MKV_ELEMENT_ID_BLOCKGROUP = 0xA0,
   MKV_ELEMENT_ID_BLOCK = 0xA1,
   MKV_ELEMENT_ID_BLOCK_ADDITIONS = 0x75A1,
   MKV_ELEMENT_ID_BLOCK_MORE = 0xA6,
   MKV_ELEMENT_ID_BLOCK_ADD_ID = 0xEE,
   MKV_ELEMENT_ID_BLOCK_ADDITIONAL = 0xA5,
   MKV_ELEMENT_ID_BLOCK_DURATION = 0x9B,
   MKV_ELEMENT_ID_REFERENCE_PRIORITY = 0xFA,
   MKV_ELEMENT_ID_REFERENCE_BLOCK = 0xFB,
   MKV_ELEMENT_ID_CODEC_STATE = 0xA4,
   MKV_ELEMENT_ID_SLICES = 0x8E,
   MKV_ELEMENT_ID_TIME_SLICE = 0xE8,
   MKV_ELEMENT_ID_LACE_NUMBER = 0xCC,
   MKV_ELEMENT_ID_SIMPLE_BLOCK = 0xA3,

   /* Track */
   MKV_ELEMENT_ID_TRACKS = 0x1654AE6B,
   MKV_ELEMENT_ID_TRACK_ENTRY = 0xAE,
   MKV_ELEMENT_ID_TRACK_NUMBER = 0xD7,
   MKV_ELEMENT_ID_TRACK_UID = 0x73C5,
   MKV_ELEMENT_ID_TRACK_TYPE = 0x83,
   MKV_ELEMENT_ID_FLAG_ENABLED = 0xB9,
   MKV_ELEMENT_ID_FLAG_DEFAULT = 0x88,
   MKV_ELEMENT_ID_FLAG_FORCED = 0x55AA,
   MKV_ELEMENT_ID_FLAG_LACING = 0x9C,
   MKV_ELEMENT_ID_MIN_CACHE = 0x6DE7,
   MKV_ELEMENT_ID_MAX_CACHE = 0x6DF8,
   MKV_ELEMENT_ID_DEFAULT_DURATION = 0x23E383,
   MKV_ELEMENT_ID_TRACK_TIMECODE_SCALE = 0x23314F,
   MKV_ELEMENT_ID_MAX_BLOCK_ADDITION_ID = 0x55EE,
   MKV_ELEMENT_ID_NAME = 0x536E,
   MKV_ELEMENT_ID_LANGUAGE = 0x22B59C,
   MKV_ELEMENT_ID_TRACK_CODEC_ID = 0x86,
   MKV_ELEMENT_ID_TRACK_CODEC_PRIVATE = 0x63A2,
   MKV_ELEMENT_ID_TRACK_CODEC_NAME = 0x258688,
   MKV_ELEMENT_ID_ATTACHMENT_LINK = 0x7446,
   MKV_ELEMENT_ID_CODEC_DECODE_ALL = 0xAA,
   MKV_ELEMENT_ID_TRACK_OVERLAY = 0x6FAB,
   MKV_ELEMENT_ID_TRACK_TRANSLATE = 0x6624,
   MKV_ELEMENT_ID_TRACK_TRANSLATE_EDITION_UID = 0x66FC,
   MKV_ELEMENT_ID_TRACK_TRANSLATE_CODEC = 0x66BF,
   MKV_ELEMENT_ID_TRACK_TRANSLATE_TRACK_ID = 0x66A5,

   /* Video */
   MKV_ELEMENT_ID_VIDEO = 0xE0,
   MKV_ELEMENT_ID_FLAG_INTERLACED = 0x9A,
   MKV_ELEMENT_ID_STEREO_MODE = 0x53B8,
   MKV_ELEMENT_ID_PIXEL_WIDTH = 0xB0,
   MKV_ELEMENT_ID_PIXEL_HEIGHT = 0xBA,
   MKV_ELEMENT_ID_PIXEL_CROP_BOTTOM = 0x54AA,
   MKV_ELEMENT_ID_PIXEL_CROP_TOP = 0x54BB,
   MKV_ELEMENT_ID_PIXEL_CROP_LEFT = 0x54CC,
   MKV_ELEMENT_ID_PIXEL_CROP_RIGHT = 0x54DD,
   MKV_ELEMENT_ID_DISPLAY_WIDTH = 0x54B0,
   MKV_ELEMENT_ID_DISPLAY_HEIGHT = 0x54BA,
   MKV_ELEMENT_ID_DISPLAY_UNIT = 0x54B2,
   MKV_ELEMENT_ID_ASPECT_RATIO_TYPE = 0x54B3,
   MKV_ELEMENT_ID_COLOUR_SPACE = 0x2EB524,
   MKV_ELEMENT_ID_FRAME_RATE = 0x2383E3,

   /* Audio */
   MKV_ELEMENT_ID_AUDIO = 0xE1,
   MKV_ELEMENT_ID_SAMPLING_FREQUENCY = 0xB5,
   MKV_ELEMENT_ID_OUTPUT_SAMPLING_FREQUENCY = 0x78B5,
   MKV_ELEMENT_ID_CHANNELS = 0x9F,
   MKV_ELEMENT_ID_BIT_DEPTH = 0x6264,

   /* Content Encoding */
   MKV_ELEMENT_ID_CONTENT_ENCODINGS = 0x6D80,
   MKV_ELEMENT_ID_CONTENT_ENCODING = 0x6240,
   MKV_ELEMENT_ID_CONTENT_ENCODING_ORDER = 0x5031,
   MKV_ELEMENT_ID_CONTENT_ENCODING_SCOPE = 0x5032,
   MKV_ELEMENT_ID_CONTENT_ENCODING_TYPE = 0x5033,
   MKV_ELEMENT_ID_CONTENT_COMPRESSION = 0x5034,
   MKV_ELEMENT_ID_CONTENT_COMPRESSION_ALGO = 0x4254,
   MKV_ELEMENT_ID_CONTENT_COMPRESSION_SETTINGS = 0x4255,
   MKV_ELEMENT_ID_CONTENT_ENCRYPTION = 0x5035,
   MKV_ELEMENT_ID_CONTENT_ENCRYPTION_ALGO = 0x47E1,
   MKV_ELEMENT_ID_CONTENT_ENCRYPTION_KEY_ID = 0x47E2,
   MKV_ELEMENT_ID_CONTENT_SIGNATURE = 0x47E3,
   MKV_ELEMENT_ID_CONTENT_SIGNATURE_KEY_ID = 0x47E4,
   MKV_ELEMENT_ID_CONTENT_SIGNATURE_ALGO = 0x47E5,
   MKV_ELEMENT_ID_CONTENT_SIGNATURE_HASH_ALGO = 0x47E6,

   /* Cueing Data */
   MKV_ELEMENT_ID_CUES = 0x1C53BB6B,
   MKV_ELEMENT_ID_CUE_POINT = 0xBB,
   MKV_ELEMENT_ID_CUE_TIME = 0xB3,
   MKV_ELEMENT_ID_CUE_TRACK_POSITIONS = 0xB7,
   MKV_ELEMENT_ID_CUE_TRACK = 0xF7,
   MKV_ELEMENT_ID_CUE_CLUSTER_POSITION = 0xF1,
   MKV_ELEMENT_ID_CUE_BLOCK_NUMBER = 0x5378,

   /* Attachments */
   MKV_ELEMENT_ID_ATTACHMENTS = 0x1941A469,

   /* Chapters */
   MKV_ELEMENT_ID_CHAPTERS = 0x1043A770,

   /* Tagging */
   MKV_ELEMENT_ID_TAGS = 0x1254C367,
   MKV_ELEMENT_ID_TAG = 0x7373,
   MKV_ELEMENT_ID_TAG_TARGETS = 0x63C0,
   MKV_ELEMENT_ID_TAG_TARGET_TYPE_VALUE = 0x68CA,
   MKV_ELEMENT_ID_TAG_TARGET_TYPE = 0x63CA,
   MKV_ELEMENT_ID_TAG_TRACK_UID = 0x63C5,
   MKV_ELEMENT_ID_TAG_EDITION_UID = 0x63C9,
   MKV_ELEMENT_ID_TAG_CHAPTER_UID = 0x63C4,
   MKV_ELEMENT_ID_TAG_ATTACHMENT_UID = 0x63C6,
   MKV_ELEMENT_ID_TAG_SIMPLE_TAG = 0x67C8,
   MKV_ELEMENT_ID_TAG_NAME = 0x45A3,
   MKV_ELEMENT_ID_TAG_LANGUAGE = 0x447A,
   MKV_ELEMENT_ID_TAG_DEFAULT = 0x4484,
   MKV_ELEMENT_ID_TAG_STRING = 0x4487,
   MKV_ELEMENT_ID_TAG_BINARY = 0x4485,

   MKV_ELEMENT_ID_INVALID = 0xFFFFFFFF
} MKV_ELEMENT_ID_T;

/** Context for our reader
 */

typedef struct
{
   unsigned int track;
   unsigned int flags;
   int64_t pts;
   int64_t cluster_timecode;
   int64_t prev_cluster_size; /* Size of the previous cluster if available */
   int64_t frame_duration;

   int level;
   struct {
      int64_t offset;
      int64_t data_start;
      int64_t data_offset;
      int64_t size;
      MKV_ELEMENT_ID_T id;
   } levels[MKV_MAX_READER_STATE_LEVEL];

   bool     eos;
   bool     corrupted;
   bool     seen_ref_block;

   uint32_t lacing_num_frames;
   uint32_t lacing_size;
   uint16_t lacing_sizes[MKV_MAX_LACING_NUM];
   uint32_t lacing_current_size;

   /* For header stripping compression */
   uint32_t header_size;
   uint8_t *header_data;
   uint32_t header_size_backup;
} MKV_READER_STATE_T;

typedef struct
{
   const MKV_ELEMENT_ID_T id;
   const MKV_ELEMENT_ID_T parent_id;
   const char *psz_name;
   VC_CONTAINER_STATUS_T (*pf_func)(VC_CONTAINER_T *, MKV_ELEMENT_ID_T, int64_t);

} MKV_ELEMENT_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   MKV_READER_STATE_T *state;
   MKV_READER_STATE_T track_state;

   /* Information extracted from the track entry */
   uint32_t number;
   uint32_t type;
   int64_t  timecode_scale;
   uint32_t duration;
   int64_t  frame_duration;
   char codecid[MKV_CODECID_MAX];

   union {
      /* video specific */
      struct {
         unsigned int interlaced:1;
         unsigned int stereo_mode:2;
         uint32_t pixel_width;
         uint32_t pixel_height;
         uint32_t pixel_crop_bottom;
         uint32_t pixel_crop_top;
         uint32_t pixel_crop_left;
         uint32_t pixel_crop_right;
         uint32_t display_width;
         uint32_t display_height;
         uint32_t display_unit;
         uint32_t aspect_ratio_type;
         float frame_rate;
      } video;

      /* audio specific */
      struct {
         uint32_t sampling_frequency;
         uint32_t output_sampling_frequency;
         uint32_t channels;
         uint32_t bit_depth;
      } audio;
   } es_type;

   /* content encoding (i.e. lossless compression and encryption) */
   unsigned int encodings_num;
   struct {
      enum {
         MKV_CONTENT_ENCODING_COMPRESSION_ZLIB,
         MKV_CONTENT_ENCODING_COMPRESSION_HEADER,
         MKV_CONTENT_ENCODING_ENCRYPTION,
         MKV_CONTENT_ENCODING_UNKNOWN
      } type;
      unsigned int data_size;
      uint8_t *data;
   } encodings[MKV_MAX_ENCODINGS];

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   MKV_ELEMENT_T *elements_list;
   int element_level;
   MKV_ELEMENT_ID_T parent_id;

   uint64_t element_offset; /**< Offset to the start of the current element */

   uint64_t segment_offset; /**< Offset to the start of the data packets */
   int64_t segment_size;

   int tracks_num;
   VC_CONTAINER_TRACK_T *tracks[MKV_TRACKS_MAX];

   MKV_READER_STATE_T state;
   int64_t timecode_scale;
   float duration;

   uint64_t cluster_offset; /**< Offset to the first cluster */
   uint64_t cues_offset; /**< Offset to the start of the seeking cues */
   uint64_t tags_offset; /**< Offset to the start of the tags */

   /*
    * Variables only used during parsing of the header
    */

   VC_CONTAINER_TRACK_T *parsing; /**< Current track being parsed */
   bool is_doctype_valid;

   MKV_ELEMENT_ID_T seekhead_elem_id;
   int64_t seekhead_elem_offset;

   /* Cues */
   unsigned int cue_track;
   int64_t cue_timecode;
   uint64_t cue_cluster_offset;
   unsigned int cue_block;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T mkv_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Prototypes for local functions
******************************************************************************/
static VC_CONTAINER_STATUS_T mkv_read_element( VC_CONTAINER_T *p_ctx, int64_t size, MKV_ELEMENT_ID_T parent_id );
static VC_CONTAINER_STATUS_T mkv_read_elements( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_element_data_uint( VC_CONTAINER_T *p_ctx, int64_t size, uint64_t *value );
static VC_CONTAINER_STATUS_T mkv_read_element_data_float( VC_CONTAINER_T *p_ctx, int64_t size, double *value );
static VC_CONTAINER_STATUS_T mkv_read_element_ebml( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_ebml( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_element_segment( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_element_track_entry( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_track_entry( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_video( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_audio( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_info( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_element_encoding( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_encoding( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_compression( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_seek_head( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_element_cues( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );
static VC_CONTAINER_STATUS_T mkv_read_subelements_cue_point( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );

static VC_CONTAINER_STATUS_T mkv_read_subelements_cluster( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size );

/******************************************************************************
List of element IDs and their associated processing functions
******************************************************************************/
MKV_ELEMENT_T mkv_elements_list[] =
{
   /* EBML Basics */
   {MKV_ELEMENT_ID_EBML, MKV_ELEMENT_ID_UNKNOWN, "EBML", mkv_read_element_ebml},
   {MKV_ELEMENT_ID_EBML_VERSION, MKV_ELEMENT_ID_EBML, "EBMLVersion", mkv_read_subelements_ebml},
   {MKV_ELEMENT_ID_EBML_READ_VERSION, MKV_ELEMENT_ID_EBML, "EBMLReadVersion", mkv_read_subelements_ebml},
   {MKV_ELEMENT_ID_EBML_MAX_ID_LENGTH, MKV_ELEMENT_ID_EBML, "EBMLMaxIDLength", mkv_read_subelements_ebml},
   {MKV_ELEMENT_ID_EBML_MAX_SIZE_LENGTH, MKV_ELEMENT_ID_EBML, "EBMLMaxSizeLength", mkv_read_subelements_ebml},
   {MKV_ELEMENT_ID_DOCTYPE, MKV_ELEMENT_ID_EBML, "DocType", mkv_read_subelements_ebml},
   {MKV_ELEMENT_ID_DOCTYPE_VERSION, MKV_ELEMENT_ID_EBML, "DocTypeVersion", mkv_read_subelements_ebml},
   {MKV_ELEMENT_ID_DOCTYPE_READ_VERSION, MKV_ELEMENT_ID_EBML, "DocTypeReadVersion", mkv_read_subelements_ebml},

   /* Global Elements */
   {MKV_ELEMENT_ID_CRC32, MKV_ELEMENT_ID_INVALID, "CRC-32", 0},
   {MKV_ELEMENT_ID_VOID, MKV_ELEMENT_ID_INVALID, "Void", 0},

   /* Segment */
   {MKV_ELEMENT_ID_SEGMENT, MKV_ELEMENT_ID_UNKNOWN, "Segment", mkv_read_element_segment},

   /* Meta Seek Information */
   {MKV_ELEMENT_ID_SEEK_HEAD, MKV_ELEMENT_ID_SEGMENT, "SeekHead", mkv_read_elements},
   {MKV_ELEMENT_ID_SEEK, MKV_ELEMENT_ID_SEEK_HEAD, "Seek", mkv_read_subelements_seek_head},
   {MKV_ELEMENT_ID_SEEK_ID, MKV_ELEMENT_ID_SEEK, "SeekID", mkv_read_subelements_seek_head},
   {MKV_ELEMENT_ID_SEEK_POSITION, MKV_ELEMENT_ID_SEEK, "SeekPosition", mkv_read_subelements_seek_head},

   /* Segment Information */
   {MKV_ELEMENT_ID_INFO, MKV_ELEMENT_ID_SEGMENT, "Info", mkv_read_elements},
   {MKV_ELEMENT_ID_SEGMENT_UID, MKV_ELEMENT_ID_INFO, "SegmentUID", 0},
   {MKV_ELEMENT_ID_SEGMENT_FILENAME, MKV_ELEMENT_ID_INFO, "SegmentFilename", 0},
   {MKV_ELEMENT_ID_PREV_UID, MKV_ELEMENT_ID_INFO, "PrevUID", 0},
   {MKV_ELEMENT_ID_PREV_FILENAME, MKV_ELEMENT_ID_INFO, "PrevFilename", 0},
   {MKV_ELEMENT_ID_NEXT_UID, MKV_ELEMENT_ID_INFO, "NextUID", 0},
   {MKV_ELEMENT_ID_NEXT_FILENAME, MKV_ELEMENT_ID_INFO, "NextFilename", 0},
   {MKV_ELEMENT_ID_SEGMENT_FAMILY, MKV_ELEMENT_ID_INFO, "SegmentFamily", 0},
   {MKV_ELEMENT_ID_CHAPTER_TRANSLATE, MKV_ELEMENT_ID_INFO, "ChapterTranslate", 0},
   {MKV_ELEMENT_ID_CHAPTER_TRANSLATE_EDITION_UID, MKV_ELEMENT_ID_INFO, "ChapterTranslateEditionUID", 0},
   {MKV_ELEMENT_ID_CHAPTER_TRANSLATE_CODEC, MKV_ELEMENT_ID_INFO, "ChapterTranslateCodec", 0},
   {MKV_ELEMENT_ID_CHAPTER_TRANSLATE_ID, MKV_ELEMENT_ID_INFO, "ChapterTranslateID", 0},
   {MKV_ELEMENT_ID_TIMECODE_SCALE, MKV_ELEMENT_ID_INFO, "TimecodeScale", mkv_read_subelements_info},
   {MKV_ELEMENT_ID_DURATION, MKV_ELEMENT_ID_INFO, "Duration", mkv_read_subelements_info},
   {MKV_ELEMENT_ID_DATE_UTC, MKV_ELEMENT_ID_INFO, "DateUTC", 0},
   {MKV_ELEMENT_ID_TITLE, MKV_ELEMENT_ID_INFO, "Title", mkv_read_subelements_info},
   {MKV_ELEMENT_ID_MUXING_APP, MKV_ELEMENT_ID_INFO, "MuxingApp", mkv_read_subelements_info},
   {MKV_ELEMENT_ID_WRITING_APP, MKV_ELEMENT_ID_INFO, "WritingApp", mkv_read_subelements_info},

   /* Cluster */
   {MKV_ELEMENT_ID_CLUSTER, MKV_ELEMENT_ID_SEGMENT, "Cluster", 0},
   {MKV_ELEMENT_ID_TIMECODE, MKV_ELEMENT_ID_CLUSTER, "Timecode", 0},
   {MKV_ELEMENT_ID_SILENT_TRACKS, MKV_ELEMENT_ID_CLUSTER, "SilentTracks", 0},
   {MKV_ELEMENT_ID_SILENT_TRACK_NUMBER, MKV_ELEMENT_ID_CLUSTER, "SilentTrackNumber", 0},
   {MKV_ELEMENT_ID_POSITION, MKV_ELEMENT_ID_CLUSTER, "Position", 0},
   {MKV_ELEMENT_ID_PREV_SIZE, MKV_ELEMENT_ID_CLUSTER, "PrevSize", 0},
   {MKV_ELEMENT_ID_BLOCKGROUP, MKV_ELEMENT_ID_CLUSTER, "BlockGroup", 0},
   {MKV_ELEMENT_ID_BLOCK, MKV_ELEMENT_ID_BLOCKGROUP, "Block", 0},
   {MKV_ELEMENT_ID_BLOCK_ADDITIONS, MKV_ELEMENT_ID_BLOCKGROUP, "BlockAdditions", 0},
   {MKV_ELEMENT_ID_BLOCK_MORE, MKV_ELEMENT_ID_BLOCK_ADDITIONS, "BlockMore", 0},
   {MKV_ELEMENT_ID_BLOCK_ADD_ID, MKV_ELEMENT_ID_BLOCK_MORE, "BlockAddId", 0},
   {MKV_ELEMENT_ID_BLOCK_ADDITIONAL, MKV_ELEMENT_ID_BLOCK_MORE, "BlockAdditional", 0},
   {MKV_ELEMENT_ID_BLOCK_DURATION, MKV_ELEMENT_ID_BLOCKGROUP, "BlockDuration", 0},
   {MKV_ELEMENT_ID_REFERENCE_PRIORITY, MKV_ELEMENT_ID_BLOCKGROUP, "ReferencePriority", 0},
   {MKV_ELEMENT_ID_REFERENCE_BLOCK, MKV_ELEMENT_ID_BLOCKGROUP, "ReferenceBlock", 0},
   {MKV_ELEMENT_ID_CODEC_STATE, MKV_ELEMENT_ID_BLOCKGROUP, "CodecState", 0},
   {MKV_ELEMENT_ID_SLICES, MKV_ELEMENT_ID_BLOCKGROUP, "Slices", 0},
   {MKV_ELEMENT_ID_TIME_SLICE, MKV_ELEMENT_ID_SLICES, "TimeSlice", 0},
   {MKV_ELEMENT_ID_LACE_NUMBER, MKV_ELEMENT_ID_TIME_SLICE, "LaceNumber", 0},
   {MKV_ELEMENT_ID_SIMPLE_BLOCK, MKV_ELEMENT_ID_CLUSTER, "SimpleBlock", 0},

   /* Track */
   {MKV_ELEMENT_ID_TRACKS, MKV_ELEMENT_ID_SEGMENT, "Tracks", mkv_read_elements},
   {MKV_ELEMENT_ID_TRACK_ENTRY, MKV_ELEMENT_ID_TRACKS, "TrackEntry", mkv_read_element_track_entry},
   {MKV_ELEMENT_ID_TRACK_NUMBER, MKV_ELEMENT_ID_TRACK_ENTRY, "TrackNumber", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_TRACK_UID, MKV_ELEMENT_ID_TRACK_ENTRY, "TrackUID", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_TRACK_TYPE, MKV_ELEMENT_ID_TRACK_ENTRY, "TrackType", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_FLAG_ENABLED, MKV_ELEMENT_ID_TRACK_ENTRY, "FlagEnabled", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_FLAG_DEFAULT, MKV_ELEMENT_ID_TRACK_ENTRY, "FlagDefault", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_FLAG_FORCED, MKV_ELEMENT_ID_TRACK_ENTRY, "FlagForced", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_FLAG_LACING, MKV_ELEMENT_ID_TRACK_ENTRY, "FlagLacing", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_MIN_CACHE, MKV_ELEMENT_ID_TRACK_ENTRY, "MinCache", 0},
   {MKV_ELEMENT_ID_MAX_CACHE, MKV_ELEMENT_ID_TRACK_ENTRY, "MaxCache", 0},
   {MKV_ELEMENT_ID_DEFAULT_DURATION, MKV_ELEMENT_ID_TRACK_ENTRY, "DefaultDuration", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_TRACK_TIMECODE_SCALE, MKV_ELEMENT_ID_TRACK_ENTRY, "TrackTimecodeScale", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_MAX_BLOCK_ADDITION_ID, MKV_ELEMENT_ID_TRACK_ENTRY, "MaxBlockAdditionID", 0},
   {MKV_ELEMENT_ID_NAME, MKV_ELEMENT_ID_TRACK_ENTRY, "Name", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_LANGUAGE, MKV_ELEMENT_ID_TRACK_ENTRY, "Language", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_TRACK_CODEC_ID, MKV_ELEMENT_ID_TRACK_ENTRY, "CodecID", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_TRACK_CODEC_PRIVATE, MKV_ELEMENT_ID_TRACK_ENTRY, "CodecPrivate", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_TRACK_CODEC_NAME, MKV_ELEMENT_ID_TRACK_ENTRY, "CodecName", mkv_read_subelements_track_entry},
   {MKV_ELEMENT_ID_ATTACHMENT_LINK, MKV_ELEMENT_ID_TRACK_ENTRY, "AttachmentLink", 0},
   {MKV_ELEMENT_ID_CODEC_DECODE_ALL, MKV_ELEMENT_ID_TRACK_ENTRY, "DecodeAll", 0},
   {MKV_ELEMENT_ID_TRACK_OVERLAY, MKV_ELEMENT_ID_TRACK_ENTRY, "TrackOverlay", 0},
   {MKV_ELEMENT_ID_TRACK_TRANSLATE, MKV_ELEMENT_ID_TRACK_ENTRY, "TrackTranslate", 0},
   {MKV_ELEMENT_ID_TRACK_TRANSLATE_EDITION_UID, MKV_ELEMENT_ID_TRACK_TRANSLATE, "TrackTranslateEditionUID", 0},
   {MKV_ELEMENT_ID_TRACK_TRANSLATE_CODEC, MKV_ELEMENT_ID_TRACK_TRANSLATE, "TrackTranslateCodec", 0},
   {MKV_ELEMENT_ID_TRACK_TRANSLATE_TRACK_ID, MKV_ELEMENT_ID_TRACK_TRANSLATE, "TrackTranslateTrackID", 0},

   /* Video */
   {MKV_ELEMENT_ID_VIDEO, MKV_ELEMENT_ID_TRACK_ENTRY, "Video", mkv_read_elements},
   {MKV_ELEMENT_ID_FLAG_INTERLACED, MKV_ELEMENT_ID_VIDEO, "FlagInterlaced", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_STEREO_MODE, MKV_ELEMENT_ID_VIDEO, "StereoMode", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_PIXEL_WIDTH, MKV_ELEMENT_ID_VIDEO, "PixelWidth", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_PIXEL_HEIGHT, MKV_ELEMENT_ID_VIDEO, "PixelHeight", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_PIXEL_CROP_BOTTOM, MKV_ELEMENT_ID_VIDEO, "PixelCropBottom", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_PIXEL_CROP_TOP, MKV_ELEMENT_ID_VIDEO, "PixelCropTop", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_PIXEL_CROP_LEFT, MKV_ELEMENT_ID_VIDEO, "PixelCropLeft", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_PIXEL_CROP_RIGHT, MKV_ELEMENT_ID_VIDEO, "PixelCropRight", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_DISPLAY_WIDTH, MKV_ELEMENT_ID_VIDEO, "DisplayWidth", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_DISPLAY_HEIGHT, MKV_ELEMENT_ID_VIDEO, "DisplayHeight", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_DISPLAY_UNIT, MKV_ELEMENT_ID_VIDEO, "DisplayUnit", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_ASPECT_RATIO_TYPE, MKV_ELEMENT_ID_VIDEO, "AspectRatioType", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_COLOUR_SPACE, MKV_ELEMENT_ID_VIDEO, "ColourSpace", mkv_read_subelements_video},
   {MKV_ELEMENT_ID_FRAME_RATE, MKV_ELEMENT_ID_VIDEO, "FrameRate", mkv_read_subelements_video},

   /* Audio */
   {MKV_ELEMENT_ID_AUDIO, MKV_ELEMENT_ID_TRACK_ENTRY, "Audio", mkv_read_elements},
   {MKV_ELEMENT_ID_SAMPLING_FREQUENCY, MKV_ELEMENT_ID_AUDIO, "SamplingFrequency", mkv_read_subelements_audio},
   {MKV_ELEMENT_ID_OUTPUT_SAMPLING_FREQUENCY, MKV_ELEMENT_ID_AUDIO, "OutputSamplingFrequency", mkv_read_subelements_audio},
   {MKV_ELEMENT_ID_CHANNELS, MKV_ELEMENT_ID_AUDIO, "Channels", mkv_read_subelements_audio},
   {MKV_ELEMENT_ID_BIT_DEPTH, MKV_ELEMENT_ID_AUDIO, "BitDepth", mkv_read_subelements_audio},

   /* Content Encoding */
   {MKV_ELEMENT_ID_CONTENT_ENCODINGS, MKV_ELEMENT_ID_TRACK_ENTRY, "ContentEncodings", mkv_read_elements},
   {MKV_ELEMENT_ID_CONTENT_ENCODING, MKV_ELEMENT_ID_CONTENT_ENCODINGS, "ContentEncoding", mkv_read_element_encoding},
   {MKV_ELEMENT_ID_CONTENT_ENCODING_ORDER, MKV_ELEMENT_ID_CONTENT_ENCODING, "ContentEncodingOrder", mkv_read_subelements_encoding},
   {MKV_ELEMENT_ID_CONTENT_ENCODING_SCOPE, MKV_ELEMENT_ID_CONTENT_ENCODING, "ContentEncodingScope", mkv_read_subelements_encoding},
   {MKV_ELEMENT_ID_CONTENT_ENCODING_TYPE, MKV_ELEMENT_ID_CONTENT_ENCODING, "ContentEncodingType", mkv_read_subelements_encoding},
   {MKV_ELEMENT_ID_CONTENT_COMPRESSION, MKV_ELEMENT_ID_CONTENT_ENCODING, "ContentCompression", mkv_read_elements},
   {MKV_ELEMENT_ID_CONTENT_COMPRESSION_ALGO, MKV_ELEMENT_ID_CONTENT_COMPRESSION, "ContentCompAlgo", mkv_read_subelements_compression},
   {MKV_ELEMENT_ID_CONTENT_COMPRESSION_SETTINGS, MKV_ELEMENT_ID_CONTENT_COMPRESSION, "ContentCompSettings", mkv_read_subelements_compression},
   {MKV_ELEMENT_ID_CONTENT_ENCRYPTION, MKV_ELEMENT_ID_CONTENT_ENCODING, "ContentEncryption", mkv_read_elements},
   {MKV_ELEMENT_ID_CONTENT_ENCRYPTION_ALGO, MKV_ELEMENT_ID_CONTENT_ENCRYPTION, "ContentEncAlgo", 0},
   {MKV_ELEMENT_ID_CONTENT_ENCRYPTION_KEY_ID, MKV_ELEMENT_ID_CONTENT_ENCRYPTION, "ContentEncKeyID", 0},
   {MKV_ELEMENT_ID_CONTENT_SIGNATURE, MKV_ELEMENT_ID_CONTENT_ENCRYPTION, "ContentSignature", 0},
   {MKV_ELEMENT_ID_CONTENT_SIGNATURE_KEY_ID, MKV_ELEMENT_ID_CONTENT_ENCRYPTION, "ContentSigKeyID", 0},
   {MKV_ELEMENT_ID_CONTENT_SIGNATURE_ALGO, MKV_ELEMENT_ID_CONTENT_ENCRYPTION, "ContentSigAlgo", 0},
   {MKV_ELEMENT_ID_CONTENT_SIGNATURE_HASH_ALGO, MKV_ELEMENT_ID_CONTENT_ENCRYPTION, "ContentSigHashAlgo", 0},

   /* Cueing data */
   {MKV_ELEMENT_ID_CUES, MKV_ELEMENT_ID_SEGMENT, "Cues", mkv_read_element_cues},
   {MKV_ELEMENT_ID_CUE_POINT, MKV_ELEMENT_ID_CUES, "Cue Point", mkv_read_elements},
   {MKV_ELEMENT_ID_CUE_TIME, MKV_ELEMENT_ID_CUE_POINT, "Cue Time", 0},
   {MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, MKV_ELEMENT_ID_CUE_POINT, "Cue Track Positions", mkv_read_elements},
   {MKV_ELEMENT_ID_CUE_TRACK, MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, "Cue Track", 0},
   {MKV_ELEMENT_ID_CUE_CLUSTER_POSITION, MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, "Cue Cluster Position", 0},
   {MKV_ELEMENT_ID_CUE_BLOCK_NUMBER, MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, "Cue Block Number", 0},

   /* Attachments */
   {MKV_ELEMENT_ID_ATTACHMENTS, MKV_ELEMENT_ID_SEGMENT, "Attachments", 0},

   /* Chapters */
   {MKV_ELEMENT_ID_CHAPTERS, MKV_ELEMENT_ID_SEGMENT, "Chapters", 0},

   /* Tagging */
   {MKV_ELEMENT_ID_TAGS, MKV_ELEMENT_ID_SEGMENT, "Tags", mkv_read_elements},
   {MKV_ELEMENT_ID_TAG, MKV_ELEMENT_ID_TAGS, "Tag", mkv_read_elements},
   {MKV_ELEMENT_ID_TAG_TARGETS, MKV_ELEMENT_ID_TAG, "Tag Targets", mkv_read_elements},
   {MKV_ELEMENT_ID_TAG_TARGET_TYPE_VALUE, MKV_ELEMENT_ID_TAG_TARGETS, "Tag Target Type Value", 0},
   {MKV_ELEMENT_ID_TAG_TARGET_TYPE, MKV_ELEMENT_ID_TAG_TARGETS, "Tag Target Type", 0},
   {MKV_ELEMENT_ID_TAG_TRACK_UID, MKV_ELEMENT_ID_TAG_TARGETS, "Tag Track UID", 0},
   {MKV_ELEMENT_ID_TAG_EDITION_UID, MKV_ELEMENT_ID_TAG_TARGETS, "Tag Edition UID", 0},
   {MKV_ELEMENT_ID_TAG_CHAPTER_UID, MKV_ELEMENT_ID_TAG_TARGETS, "Tag Chapter UID", 0},
   {MKV_ELEMENT_ID_TAG_ATTACHMENT_UID, MKV_ELEMENT_ID_TAG_TARGETS, "Tag Attachment UID", 0},
   {MKV_ELEMENT_ID_TAG_SIMPLE_TAG, MKV_ELEMENT_ID_TAG, "Simple Tag", mkv_read_elements},
   {MKV_ELEMENT_ID_TAG_NAME, MKV_ELEMENT_ID_TAG_SIMPLE_TAG, "Tag Name", 0},
   {MKV_ELEMENT_ID_TAG_LANGUAGE, MKV_ELEMENT_ID_TAG_SIMPLE_TAG, "Tag Language", 0},
   {MKV_ELEMENT_ID_TAG_DEFAULT, MKV_ELEMENT_ID_TAG_SIMPLE_TAG, "Tag Default", 0},
   {MKV_ELEMENT_ID_TAG_STRING, MKV_ELEMENT_ID_TAG_SIMPLE_TAG, "Tag String", 0},
   {MKV_ELEMENT_ID_TAG_BINARY, MKV_ELEMENT_ID_TAG_SIMPLE_TAG, "Tag Binary", 0},

   {MKV_ELEMENT_ID_UNKNOWN, MKV_ELEMENT_ID_INVALID, "unknown", 0}
};

MKV_ELEMENT_T mkv_cluster_elements_list[] =
{
   /* Cluster */
   {MKV_ELEMENT_ID_CLUSTER, MKV_ELEMENT_ID_SEGMENT, "Cluster", 0},
   {MKV_ELEMENT_ID_TIMECODE, MKV_ELEMENT_ID_CLUSTER, "Timecode", mkv_read_subelements_cluster},
   {MKV_ELEMENT_ID_SILENT_TRACKS, MKV_ELEMENT_ID_CLUSTER, "SilentTracks", 0},
   {MKV_ELEMENT_ID_SILENT_TRACK_NUMBER, MKV_ELEMENT_ID_CLUSTER, "SilentTrackNumber", 0},
   {MKV_ELEMENT_ID_POSITION, MKV_ELEMENT_ID_CLUSTER, "Position", 0},
   {MKV_ELEMENT_ID_PREV_SIZE, MKV_ELEMENT_ID_CLUSTER, "PrevSize", 0},
   {MKV_ELEMENT_ID_BLOCKGROUP, MKV_ELEMENT_ID_CLUSTER, "BlockGroup", 0},
   {MKV_ELEMENT_ID_BLOCK, MKV_ELEMENT_ID_BLOCKGROUP, "Block", 0},
   {MKV_ELEMENT_ID_BLOCK_ADDITIONS, MKV_ELEMENT_ID_BLOCKGROUP, "BlockAdditions", 0},
   {MKV_ELEMENT_ID_BLOCK_MORE, MKV_ELEMENT_ID_BLOCK_ADDITIONS, "BlockMore", 0},
   {MKV_ELEMENT_ID_BLOCK_ADD_ID, MKV_ELEMENT_ID_BLOCK_MORE, "BlockAddId", 0},
   {MKV_ELEMENT_ID_BLOCK_ADDITIONAL, MKV_ELEMENT_ID_BLOCK_MORE, "BlockAdditional", 0},
   {MKV_ELEMENT_ID_BLOCK_DURATION, MKV_ELEMENT_ID_BLOCKGROUP, "BlockDuration", mkv_read_subelements_cluster},
   {MKV_ELEMENT_ID_REFERENCE_PRIORITY, MKV_ELEMENT_ID_BLOCKGROUP, "ReferencePriority", 0},
   {MKV_ELEMENT_ID_REFERENCE_BLOCK, MKV_ELEMENT_ID_BLOCKGROUP, "ReferenceBlock", 0},
   {MKV_ELEMENT_ID_CODEC_STATE, MKV_ELEMENT_ID_BLOCKGROUP, "CodecState", 0},
   {MKV_ELEMENT_ID_SLICES, MKV_ELEMENT_ID_BLOCKGROUP, "Slices", 0},
   {MKV_ELEMENT_ID_TIME_SLICE, MKV_ELEMENT_ID_SLICES, "TimeSlice", 0},
   {MKV_ELEMENT_ID_LACE_NUMBER, MKV_ELEMENT_ID_TIME_SLICE, "LaceNumber", 0},
   {MKV_ELEMENT_ID_SIMPLE_BLOCK, MKV_ELEMENT_ID_CLUSTER, "SimpleBlock", 0},

   /* Global Elements */
   {MKV_ELEMENT_ID_CRC32, MKV_ELEMENT_ID_INVALID, "CRC-32", 0},
   {MKV_ELEMENT_ID_VOID, MKV_ELEMENT_ID_INVALID, "Void", 0},

   {MKV_ELEMENT_ID_UNKNOWN, MKV_ELEMENT_ID_INVALID, "unknown", 0}
};

MKV_ELEMENT_T mkv_cue_elements_list[] =
{
   /* Cueing data */
   {MKV_ELEMENT_ID_CUES, MKV_ELEMENT_ID_SEGMENT, "Cues", 0},
   {MKV_ELEMENT_ID_CUE_POINT, MKV_ELEMENT_ID_CUES, "Cue Point", mkv_read_elements},
   {MKV_ELEMENT_ID_CUE_TIME, MKV_ELEMENT_ID_CUE_POINT, "Cue Time", mkv_read_subelements_cue_point},
   {MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, MKV_ELEMENT_ID_CUE_POINT, "Cue Track Positions", mkv_read_elements},
   {MKV_ELEMENT_ID_CUE_TRACK, MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, "Cue Track", mkv_read_subelements_cue_point},
   {MKV_ELEMENT_ID_CUE_CLUSTER_POSITION, MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, "Cue Cluster Position", mkv_read_subelements_cue_point},
   {MKV_ELEMENT_ID_CUE_BLOCK_NUMBER, MKV_ELEMENT_ID_CUE_TRACK_POSITIONS, "Cue Block Number", mkv_read_subelements_cue_point},

   /* Global Elements */
   {MKV_ELEMENT_ID_CRC32, MKV_ELEMENT_ID_INVALID, "CRC-32", 0},
   {MKV_ELEMENT_ID_VOID, MKV_ELEMENT_ID_INVALID, "Void", 0},

   {MKV_ELEMENT_ID_UNKNOWN, MKV_ELEMENT_ID_INVALID, "unknown", 0}
};

/******************************************************************************
List of codec mapping
******************************************************************************/
static const struct {
   VC_CONTAINER_FOURCC_T fourcc;
   const char *codecid;
   VC_CONTAINER_FOURCC_T variant;
} codecid_to_fourcc_table[] =
{
   /* Video */
   {VC_CONTAINER_CODEC_MP1V,    "V_MPEG1", 0},
   {VC_CONTAINER_CODEC_MP2V,    "V_MPEG2", 0},
   {VC_CONTAINER_CODEC_MP4V,    "V_MPEG4/ISO/ASP", 0},
   {VC_CONTAINER_CODEC_MP4V,    "V_MPEG4/ISO/SP", 0},
   {VC_CONTAINER_CODEC_MP4V,    "V_MPEG4/ISO/AP", 0},
   {VC_CONTAINER_CODEC_DIV3,    "V_MPEG4/MS/V3", 0},
   {VC_CONTAINER_CODEC_H264,    "V_MPEG4/ISO/AVC", VC_CONTAINER_VARIANT_H264_AVC1},
   {VC_CONTAINER_CODEC_MJPEG,   "V_MJPEG", 0},
   {VC_CONTAINER_CODEC_RV10,    "V_REAL/RV10", 0},
   {VC_CONTAINER_CODEC_RV20,    "V_REAL/RV20", 0},
   {VC_CONTAINER_CODEC_RV30,    "V_REAL/RV30", 0},
   {VC_CONTAINER_CODEC_RV40,    "V_REAL/RV40", 0},
   {VC_CONTAINER_CODEC_THEORA,  "V_THEORA", 0},
   {VC_CONTAINER_CODEC_DIRAC,   "V_DIRAC", 0},
   {VC_CONTAINER_CODEC_VP8,     "V_VP8", 0},

   /* Audio */
   {VC_CONTAINER_CODEC_MPGA,    "A_MPEG/L3", VC_CONTAINER_VARIANT_MPGA_L3},
   {VC_CONTAINER_CODEC_MPGA,    "A_MPEG/L2", VC_CONTAINER_VARIANT_MPGA_L2},
   {VC_CONTAINER_CODEC_MPGA,    "A_MPEG/L1", VC_CONTAINER_VARIANT_MPGA_L1},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG2/MAIN", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG2/LC", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG2/SSR", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG2/LC/SBR", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG4/MAIN", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG4/LC", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG4/SSR", 0},
   {VC_CONTAINER_CODEC_MP4A,    "A_AAC/MPEG4/LC/SBR", 0},
   {VC_CONTAINER_CODEC_AC3,     "A_AC3", 0},
   {VC_CONTAINER_CODEC_EAC3,    "A_EAC3", 0},
   {VC_CONTAINER_CODEC_DTS,     "A_DTS", 0},
   {VC_CONTAINER_CODEC_MLP,     "A_MLP", 0},
   {0,                          "A_TRUEHD", 0},
   {VC_CONTAINER_CODEC_VORBIS,  "A_VORBIS", 0},
   {VC_CONTAINER_CODEC_FLAC,    "A_FLAC", 0},
   {VC_CONTAINER_CODEC_PCM_SIGNED_LE, "A_PCM/INT/LIT", 0},
   {VC_CONTAINER_CODEC_PCM_SIGNED_BE, "A_PCM/INT/BIG", 0},
   {VC_CONTAINER_CODEC_PCM_FLOAT_LE,  "A_PCM/FLOAT/IEEE", 0},
   {0,                          "A_REAL/xyzt", 0},
   {0,                          "A_REAL/14_4", 0},

   /* Text */
   {VC_CONTAINER_CODEC_TEXT,    "S_TEXT/ASCII", 0},
   {VC_CONTAINER_CODEC_TEXT,    "S_TEXT/UTF8", 0},
   {VC_CONTAINER_CODEC_SSA,     "S_TEXT/ASS", 0},
   {VC_CONTAINER_CODEC_SSA,     "S_TEXT/SSA", 0},
   {VC_CONTAINER_CODEC_SSA,     "S_ASS", 0},
   {VC_CONTAINER_CODEC_SSA,     "S_SSA", 0},
   {VC_CONTAINER_CODEC_USF,     "S_TEXT/USF", 0},
   {VC_CONTAINER_CODEC_VOBSUB,  "S_VOBSUB", 0},

   {0, 0}
};

/******************************************************************************
Local Functions
******************************************************************************/

static VC_CONTAINER_FOURCC_T mkv_codecid_to_fourcc(const char *codecid,
   VC_CONTAINER_FOURCC_T *variant)
{
   unsigned int i;
   for(i = 0; codecid_to_fourcc_table[i].codecid; i++)
      if(!strcmp(codecid_to_fourcc_table[i].codecid, codecid)) break;
   if (variant) *variant = codecid_to_fourcc_table[i].variant;
   return codecid_to_fourcc_table[i].fourcc;
}

#if 0
/** Find the track associated with an MKV track number */
static VC_CONTAINER_TRACK_T *mkv_reader_find_track( VC_CONTAINER_T *p_ctx, unsigned int mkv_track_num)
{
   VC_CONTAINER_TRACK_T *p_track = 0;
   unsigned int i;

   for(i = 0; i < p_ctx->tracks_num; i++)
      if(p_ctx->tracks[i]->priv->module->number == mkv_track_num) break;

   if(i < p_ctx->tracks_num) /* We found it */
      p_track = p_ctx->tracks[i];

   return p_track;
}
#endif

/** Base function used to read an MKV/EBML element header.
 * This will read the element header do lots of sanity checking and return the element id
 * and the size of the data contained in the element */
static VC_CONTAINER_STATUS_T mkv_read_element_header(VC_CONTAINER_T *p_ctx, int64_t size,
   MKV_ELEMENT_ID_T *id, int64_t *element_size, MKV_ELEMENT_ID_T parent_id,
   MKV_ELEMENT_T **elem)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   MKV_ELEMENT_T *element;

   module->element_offset = STREAM_POSITION(p_ctx);

   *id = MKV_READ_ID(p_ctx, "Element ID");
   CHECK_POINT(p_ctx);
   if(!*id)
   {
      LOG_DEBUG(p_ctx, "invalid element id %i", *id);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if(elem) element = *elem;
   else element = mkv_elements_list;

   /* Find out which Element we are dealing with */
   while(element->id && *id != element->id) element++;

   *element_size = MKV_READ_UINT(p_ctx, "Element Size");
   CHECK_POINT(p_ctx);
   LOG_FORMAT(p_ctx, "- Element %s (ID 0x%x), Size: %"PRIi64", Offset: %"PRIi64,
              element->psz_name, *id, *element_size, module->element_offset);

   /* Sanity check the element size */
   if(*element_size + 1 < 0 /* Shouldn't ever get that big */ ||
      /* Only the segment / cluster elements can really be massive */
      (*id != MKV_ELEMENT_ID_SEGMENT && *id != MKV_ELEMENT_ID_CLUSTER &&
       *element_size > MKV_MAX_ELEMENT_SIZE))
   {
      LOG_DEBUG(p_ctx, "element %s has an invalid size (%"PRIi64")",
                element->psz_name, *element_size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }
   if(size >= 0 && *element_size > size)
   {
      LOG_DEBUG(p_ctx, "element %s is bigger than it should (%"PRIi64" > %"PRIi64")",
                element->psz_name, *element_size, size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   /* Sanity check that the element has the right parent */
   if(element->id && element->parent_id != MKV_ELEMENT_ID_INVALID &&
      parent_id != MKV_ELEMENT_ID_INVALID && parent_id != element->parent_id)
   {
      LOG_FORMAT(p_ctx, "Ignoring mis-placed element %s (ID 0x%x)", element->psz_name, *id);
      while(element->id != MKV_ELEMENT_ID_UNKNOWN) element++;
   }

   /* Sanity check that the element isn't too deeply nested */
   if(module->element_level >= MKV_MAX_ELEMENT_LEVEL)
   {
      LOG_DEBUG(p_ctx, "element %s is too deep. skipping", element->psz_name);
      while(element->id != MKV_ELEMENT_ID_UNKNOWN) element++;
   }

   if(elem) *elem = element;
   return STREAM_STATUS(p_ctx);
}

static VC_CONTAINER_STATUS_T mkv_read_element_data(VC_CONTAINER_T *p_ctx,
   MKV_ELEMENT_T *element, int64_t element_size, int64_t size)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t offset;

   offset = STREAM_POSITION(p_ctx);
   if (size < 0) size = element_size;
   if (size < 0) size = INT64_C(1) << 62;

   /* Call the element specific parsing function */
   if(element->pf_func)
      status = element->pf_func(p_ctx, element->id, element_size < 0 ? size : element_size);

   if(status != VC_CONTAINER_SUCCESS)
      LOG_DEBUG(p_ctx, "element %s appears to be corrupted (%i)", element->psz_name, status);

   if(element_size < 0) return STREAM_STATUS(p_ctx); /* Unknown size */

   /* Skip the rest of the element */
   element_size -= (STREAM_POSITION(p_ctx) - offset);
   if(element_size < 0) /* Check for overruns */
   {
      /* Things have gone really bad here and we ended up reading past the end of the
       * element. We could maybe try to be clever and recover by seeking back to the end
       * of the element. However if we get there, the file is clearly corrupted so there's
       * no guarantee it would work anyway. */
      LOG_DEBUG(p_ctx, "%"PRIi64" bytes overrun past the end of element %s",
                -element_size, element->psz_name);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if(element_size)
      LOG_FORMAT(p_ctx, "%"PRIi64" bytes left unread in element %s", element_size, element->psz_name);

   if(element_size < MKV_MAX_ELEMENT_SIZE) SKIP_BYTES(p_ctx, element_size);
   else SEEK(p_ctx, STREAM_POSITION(p_ctx) + element_size);

   return STREAM_STATUS(p_ctx);
}

/** Base function used to read an MKV/EBML element.
 * This will read the element header do lots of sanity checking and pass on the rest
 * of the reading to the element specific reading function */
static VC_CONTAINER_STATUS_T mkv_read_element(VC_CONTAINER_T *p_ctx,
   int64_t size, MKV_ELEMENT_ID_T parent_id)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   MKV_ELEMENT_T *element = p_ctx->priv->module->elements_list;
   int64_t element_size;
   MKV_ELEMENT_ID_T id;

   status = mkv_read_element_header(p_ctx, size, &id, &element_size, parent_id, &element);
   if(status != VC_CONTAINER_SUCCESS) return status;
   return mkv_read_element_data(p_ctx, element, element_size, size);
}

/** Reads an unsigned integer element */
static VC_CONTAINER_STATUS_T mkv_read_element_data_uint(VC_CONTAINER_T *p_ctx,
   int64_t size, uint64_t *value)
{
   switch(size)
   {
   case 1: *value = READ_U8(p_ctx, "u8-integer"); break;
   case 2: *value = READ_U16(p_ctx, "u16-integer"); break;
   case 3: *value = READ_U24(p_ctx, "u24-integer"); break;
   case 4: *value = READ_U32(p_ctx, "u32-integer"); break;
   case 5: *value = READ_U40(p_ctx, "u40-integer"); break;
   case 6: *value = READ_U48(p_ctx, "u48-integer"); break;
   case 7: *value = READ_U56(p_ctx, "u56-integer"); break;
   case 8: *value = READ_U64(p_ctx, "u64-integer"); break;
   default: return VC_CONTAINER_ERROR_CORRUPTED;
   }
   return STREAM_STATUS(p_ctx);
}

/** Reads a float element */
static VC_CONTAINER_STATUS_T mkv_read_element_data_float(VC_CONTAINER_T *p_ctx,
   int64_t size, double *value)
{
   union {
      uint32_t  u32;
      uint64_t  u64;
      float     f;
      double    d;
   } u;

   switch(size)
   {
   case 4: u.u32 = READ_U32(p_ctx, "f32-float"); *value = u.f; break;
   case 8: u.u64 = READ_U64(p_ctx, "f64-float"); *value = u.d; break;
   default: return VC_CONTAINER_ERROR_CORRUPTED;
   }
   LOG_FORMAT(p_ctx, "float: %f", *value);
   return STREAM_STATUS(p_ctx);
}

/** Reads an MKV EBML element */
static VC_CONTAINER_STATUS_T mkv_read_element_ebml(VC_CONTAINER_T *p_ctx,
   MKV_ELEMENT_ID_T id, int64_t size)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t offset = STREAM_POSITION(p_ctx);

   /* Read contained elements */
   module->element_level++;
   while(status == VC_CONTAINER_SUCCESS && size >= MKV_ELEMENT_MIN_HEADER_SIZE)
   {
      offset = STREAM_POSITION(p_ctx);
      status = mkv_read_element(p_ctx, size, id);
      size -= (STREAM_POSITION(p_ctx) - offset);
   }
   module->element_level--;
   return status;
}

/** Reads the MKV EBML sub-element */
static VC_CONTAINER_STATUS_T mkv_read_subelements_ebml( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint64_t value;

   /* Deal with DocType first since it's a special case */
   if(id == MKV_ELEMENT_ID_DOCTYPE)
   {
      char doctype[] = "matroska doctype";

      /* Check we've got the right doctype string for matroska */
      if(size <= 0) goto unknown_doctype;
      if(size > (int)sizeof(doctype)) goto unknown_doctype;
      if((int)READ_STRING(p_ctx, doctype, size, "string") != size) return STREAM_STATUS(p_ctx);
      if((size != sizeof("matroska")-1 || strncmp(doctype, "matroska", (int)size)) &&
         (size != sizeof("webm")-1 || strncmp(doctype, "webm", (int)size)))
         goto unknown_doctype;

      module->is_doctype_valid = true;
      return VC_CONTAINER_SUCCESS;

 unknown_doctype:
      LOG_DEBUG(p_ctx, "invalid doctype");
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   /* The rest are just unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   switch(id)
   {
   case MKV_ELEMENT_ID_EBML_VERSION:
   case MKV_ELEMENT_ID_EBML_READ_VERSION:
      if(value != 1) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      break;
   case MKV_ELEMENT_ID_EBML_MAX_ID_LENGTH:
      if(value > 4) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      break;
   case MKV_ELEMENT_ID_EBML_MAX_SIZE_LENGTH:
      if(value > 8) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      break;
   case MKV_ELEMENT_ID_DOCTYPE_VERSION:
   case MKV_ELEMENT_ID_DOCTYPE_READ_VERSION:
   default: break;
   }

   return STREAM_STATUS(p_ctx);
}

static VC_CONTAINER_STATUS_T mkv_read_elements( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t offset = STREAM_POSITION(p_ctx);
   bool unknown_size = size < 0;

   /* Read contained elements */
   module->element_level++;
   while(status == VC_CONTAINER_SUCCESS &&
         (unknown_size || size >= MKV_ELEMENT_MIN_HEADER_SIZE))
   {
      offset = STREAM_POSITION(p_ctx);
      status = mkv_read_element(p_ctx, size, id);
      if(!unknown_size) size -= (STREAM_POSITION(p_ctx) - offset);
   }
   module->element_level--;
   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_element_segment( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t offset = STREAM_POSITION(p_ctx);
   bool unknown_size = size < 0;

   /* Read contained elements */
   /* Initialise state used by reader */
   module->state.level = 0;
   module->state.levels[0].offset = STREAM_POSITION(p_ctx);
   module->state.levels[0].size = size;
   module->state.levels[0].id = MKV_ELEMENT_ID_SEGMENT;
   module->state.levels[0].data_start = 0;
   module->state.levels[0].data_offset = 0;
   module->timecode_scale = 1000000;
   module->duration = 0.0;
   module->segment_offset = STREAM_POSITION(p_ctx);
   module->segment_size = size;

   /* Read contained elements until we have all the information we need to start
    * playing the stream */
   module->element_level++;
   while(status == VC_CONTAINER_SUCCESS &&
         (unknown_size || size >= MKV_ELEMENT_MIN_HEADER_SIZE))
   {
      MKV_ELEMENT_T *child = mkv_elements_list;
      MKV_ELEMENT_ID_T child_id;
      int64_t child_size;

      offset = STREAM_POSITION(p_ctx);

      status = mkv_read_element_header(p_ctx, size, &child_id, &child_size, id, &child);
      if(status != VC_CONTAINER_SUCCESS) break;

      if(child_id == MKV_ELEMENT_ID_CLUSTER)
      {
         /* We found the start of the data */
         module->cluster_offset = module->element_offset;
         module->state.level = 1;
         module->state.levels[1].offset = STREAM_POSITION(p_ctx);
         module->state.levels[1].size = child_size;
         module->state.levels[1].id = MKV_ELEMENT_ID_CLUSTER;
         module->state.levels[1].data_start = 0;
         module->state.levels[1].data_offset = 0;
         break;
      }

      status = mkv_read_element_data(p_ctx, child, child_size, size);
      if(!unknown_size) size -= (STREAM_POSITION(p_ctx) - offset);
   }

   module->element_level--;
   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_element_track_entry( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_ES_TYPE_T es_type = VC_CONTAINER_ES_TYPE_UNKNOWN;
   VC_CONTAINER_TRACK_MODULE_T *track_module;
   VC_CONTAINER_TRACK_T *track;
   VC_CONTAINER_FOURCC_T fourcc = 0, variant = 0;
   unsigned int i, extra_size = 0, extra_offset = 0, is_wf = 0, is_bmih = 0;

   /* Allocate and initialise track data */
   if(p_ctx->tracks_num >= MKV_TRACKS_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   p_ctx->tracks[p_ctx->tracks_num] = track =
      vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
   if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   module->parsing = track;
   track_module = track->priv->module;
   track->is_enabled = true;
   track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
   track_module->timecode_scale = 1.0;
   track_module->es_type.video.frame_rate = 0;

   status = mkv_read_elements( p_ctx, id, size );
   if(status != VC_CONTAINER_SUCCESS) goto error;

   /* Sanity check the data we got from the track entry */
   if(!track_module->number || !track_module->type)
   { status = VC_CONTAINER_ERROR_FORMAT_INVALID; goto error; }

   /* Check the encodings for the track are supported */
   if(track_module->encodings_num > MKV_MAX_ENCODINGS)
   { status = VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED; goto error; }
   for(i = 0; i < track_module->encodings_num; i++)
   {
      if(track_module->encodings[i].type != MKV_CONTENT_ENCODING_COMPRESSION_HEADER ||
         !track_module->encodings[i].data_size)
      { status = VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED; goto error; }
   }

   /* Find out the track type */
   if(track_module->type == 0x1)
      es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   else if(track_module->type == 0x2)
      es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   else if(track_module->type == 0x11)
      es_type = VC_CONTAINER_ES_TYPE_SUBPICTURE;

   if(es_type == VC_CONTAINER_ES_TYPE_UNKNOWN)
   { status = VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED; goto error; }

   if(!strcmp(track_module->codecid, "V_MS/VFW/FOURCC"))
   {
      if(vc_container_bitmapinfoheader_to_es_format(track->format->extradata,
            track->format->extradata_size, &extra_offset, &extra_size,
            track->format) == VC_CONTAINER_SUCCESS)
      {
         fourcc = track->format->codec;
         is_bmih = 1;
      }
      track->format->extradata += extra_offset;
      track->format->extradata_size = extra_size;
   }
   else if(!strcmp(track_module->codecid, "A_MS/ACM"))
   {
      if(vc_container_waveformatex_to_es_format(track->format->extradata,
            track->format->extradata_size, &extra_offset, &extra_size,
            track->format) == VC_CONTAINER_SUCCESS)
      {
         fourcc = track->format->codec;
         is_wf = 1;
      }
      track->format->extradata += extra_offset;
      track->format->extradata_size = extra_size;
   }
   else if((!strncmp(track_module->codecid, "A_AAC/MPEG2/", sizeof("A_AAC/MPEG2/")-1) ||
            !strncmp(track_module->codecid, "A_AAC/MPEG4/", sizeof("A_AAC/MPEG4/")-1)) &&
            !track->format->extradata_size)
   {
      static const unsigned int sample_rates[16] =
      {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
      unsigned int samplerate, samplerate_idx, profile, sbr = 0;
      uint8_t *extra;

      fourcc = mkv_codecid_to_fourcc(track_module->codecid, &variant);

      /* Create extra data */
      if( !strcmp( &track_module->codecid[12], "MAIN" ) ) profile = 0;
      else if( !strcmp( &track_module->codecid[12], "LC" ) ) profile = 1;
      else if( !strcmp( &track_module->codecid[12], "SSR" ) ) profile = 2;
      else if( !strcmp( &track_module->codecid[12], "LC/SBR" ) ) { profile = 1; sbr = 1; }
      else profile = 3;

      samplerate = track_module->es_type.audio.sampling_frequency;
      for( samplerate_idx = 0; samplerate_idx < 13; samplerate_idx++ )
         if( sample_rates[samplerate_idx] == samplerate ) break;

      status = vc_container_track_allocate_extradata(p_ctx, track, sbr ? 5 : 2);
      if(status != VC_CONTAINER_SUCCESS) goto error;
      track->format->extradata_size = sbr ? 5 : 2;
      extra = track->format->extradata;

      extra[0] = ((profile + 1) << 3) | ((samplerate_idx & 0xe) >> 1);
      extra[1] = ((samplerate_idx & 0x1) << 7) | (track_module->es_type.audio.channels << 3);

      if(sbr)
      {
         unsigned int sync_extension_type = 0x2B7;
         samplerate = track_module->es_type.audio.output_sampling_frequency;
         for(samplerate_idx = 0; samplerate_idx < 13; samplerate_idx++)
            if(sample_rates[samplerate_idx] == samplerate) break;
         extra[2] = (sync_extension_type >> 3) & 0xFF;
         extra[3] = ((sync_extension_type & 0x7) << 5) | 5;
         extra[4] = (1 << 7) | (samplerate_idx << 3);
      }
   }
   else fourcc = mkv_codecid_to_fourcc(track_module->codecid, &variant);

   if(!fourcc)
   {
      LOG_DEBUG(p_ctx, "codec id %s is not supported", track_module->codecid);
   }

   LOG_DEBUG(p_ctx, "found track %4.4s", (char *)&fourcc);
   track->format->codec = fourcc;
   track->format->codec_variant = variant;
   track->format->es_type = es_type;

   switch(es_type)
   {
   case VC_CONTAINER_ES_TYPE_VIDEO:
      if(!is_bmih)
      {
         track->format->type->video.width = track_module->es_type.video.pixel_width;
         track->format->type->video.height = track_module->es_type.video.pixel_height;
      }
      track->format->type->video.visible_width = track->format->type->video.width;
      track->format->type->video.visible_height = track->format->type->video.height;
      if(track_module->es_type.video.pixel_crop_left < track->format->type->video.visible_width &&
         track_module->es_type.video.pixel_crop_top < track->format->type->video.visible_height)
      {
         track->format->type->video.x_offset = track_module->es_type.video.pixel_crop_left;
         track->format->type->video.y_offset = track_module->es_type.video.pixel_crop_right;
         track->format->type->video.visible_width -= track->format->type->video.x_offset;
         track->format->type->video.visible_height -= track->format->type->video.y_offset;
      }
      if(track_module->es_type.video.pixel_crop_right < track->format->type->video.visible_width &&
         track_module->es_type.video.pixel_crop_bottom < track->format->type->video.visible_height)
      {
         track->format->type->video.visible_width -= track_module->es_type.video.pixel_crop_right;
         track->format->type->video.visible_height -= track_module->es_type.video.pixel_crop_bottom;
      }
      if(track_module->es_type.video.frame_rate)
      {
         track->format->type->video.frame_rate_den = 100;
         track->format->type->video.frame_rate_num = 100 * track_module->es_type.video.frame_rate;
      }
      if(track_module->es_type.video.display_width && track_module->es_type.video.display_height)
      {
         track->format->type->video.par_num = track_module->es_type.video.display_width *
            track->format->type->video.visible_height;
         track->format->type->video.par_den = track_module->es_type.video.display_height *
            track->format->type->video.visible_width;
         vc_container_maths_rational_simplify(&track->format->type->video.par_num,
                                              &track->format->type->video.par_den);
      }
      break;
   case VC_CONTAINER_ES_TYPE_AUDIO:
      if(is_wf) break;
      track->format->type->audio.sample_rate = track_module->es_type.audio.sampling_frequency;
      if(track_module->es_type.audio.output_sampling_frequency)
         track->format->type->audio.sample_rate = track_module->es_type.audio.output_sampling_frequency;
      track->format->type->audio.channels = track_module->es_type.audio.channels;
      track->format->type->audio.bits_per_sample = track_module->es_type.audio.bit_depth;
      break;
   default:
   case VC_CONTAINER_ES_TYPE_SUBPICTURE:
      track->format->type->subpicture.encoding = VC_CONTAINER_CHAR_ENCODING_UTF8;
      if(!strcmp(track_module->codecid, "S_TEXT/ASCII"))
         track->format->type->subpicture.encoding = VC_CONTAINER_CHAR_ENCODING_UNKNOWN;
   }

   track->is_enabled = true;

   p_ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;

 error:
   for(i = 0; i < MKV_MAX_ENCODINGS; i++) free(track_module->encodings[i].data);
   vc_container_free_track(p_ctx, track);
   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_track_entry( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = module->parsing;
   VC_CONTAINER_TRACK_MODULE_T *track_module = track->priv->module;
   uint64_t value;

   /* Deal with string elements */
   if( id == MKV_ELEMENT_ID_NAME ||
       id == MKV_ELEMENT_ID_LANGUAGE ||
       id == MKV_ELEMENT_ID_TRACK_CODEC_ID ||
       id == MKV_ELEMENT_ID_TRACK_CODEC_NAME )
   {
      char stringbuf[MKV_MAX_STRING_SIZE+1];

      if(size > MKV_MAX_STRING_SIZE)
      {
         LOG_DEBUG(p_ctx, "string truncated (%i>%i)", (int)size, MKV_MAX_STRING_SIZE);
         size = MKV_MAX_STRING_SIZE;
      }
      if(READ_BYTES(p_ctx, stringbuf, size) != (size_t)size)
      {
         //XXX do sane thing
         return STREAM_STATUS(p_ctx);
      }
      stringbuf[size] = 0; /* null terminate */

      LOG_FORMAT(p_ctx, "%s", stringbuf);

      if(id == MKV_ELEMENT_ID_TRACK_CODEC_ID)
         strncpy(track_module->codecid, stringbuf, MKV_CODECID_MAX-1);

      return VC_CONTAINER_SUCCESS;
   }

   /* Deal with codec private data */
   if( id == MKV_ELEMENT_ID_TRACK_CODEC_PRIVATE )
   {
      status = vc_container_track_allocate_extradata(p_ctx, track, (unsigned int)size);
      if(status != VC_CONTAINER_SUCCESS) return status;
      track->format->extradata_size = READ_BYTES(p_ctx, track->format->extradata, size);
      return STREAM_STATUS(p_ctx);
   }

   /* The rest are just unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   switch(id)
   {
   case MKV_ELEMENT_ID_TRACK_NUMBER:
      track_module->number = value; break;
   case MKV_ELEMENT_ID_TRACK_TYPE:
      track_module->type = value; break;
   case MKV_ELEMENT_ID_DEFAULT_DURATION:
      track_module->frame_duration = value; break;
   default: break;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_video( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = module->parsing->priv->module;
   uint64_t value;

   /* Deal with floating point values first */
   if(id == MKV_ELEMENT_ID_FRAME_RATE)
   {
      double fvalue;
      status = mkv_read_element_data_float(p_ctx, size, &fvalue);
      if(status != VC_CONTAINER_SUCCESS) return status;
      track_module->es_type.video.frame_rate = fvalue;
      return status;
   }

   /* The rest are just unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   switch(id)
   {
   case MKV_ELEMENT_ID_PIXEL_WIDTH: track_module->es_type.video.pixel_width = value; break;
   case MKV_ELEMENT_ID_PIXEL_HEIGHT: track_module->es_type.video.pixel_height = value; break;
   case MKV_ELEMENT_ID_PIXEL_CROP_BOTTOM: track_module->es_type.video.pixel_crop_bottom = value; break;
   case MKV_ELEMENT_ID_PIXEL_CROP_TOP: track_module->es_type.video.pixel_crop_top = value; break;
   case MKV_ELEMENT_ID_PIXEL_CROP_LEFT: track_module->es_type.video.pixel_crop_left = value; break;
   case MKV_ELEMENT_ID_PIXEL_CROP_RIGHT: track_module->es_type.video.pixel_crop_right = value; break;
   case MKV_ELEMENT_ID_DISPLAY_WIDTH: track_module->es_type.video.display_width = value; break;
   case MKV_ELEMENT_ID_DISPLAY_HEIGHT: track_module->es_type.video.display_height = value; break;
   case MKV_ELEMENT_ID_DISPLAY_UNIT: track_module->es_type.video.display_unit = value; break;
   case MKV_ELEMENT_ID_ASPECT_RATIO_TYPE: track_module->es_type.video.aspect_ratio_type = value; break;
   default: break;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_audio( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = module->parsing->priv->module;
   uint64_t value;

   /* Deal with floating point values first */
   if(id == MKV_ELEMENT_ID_SAMPLING_FREQUENCY ||
      id == MKV_ELEMENT_ID_OUTPUT_SAMPLING_FREQUENCY)
   {
      double fvalue;
      status = mkv_read_element_data_float(p_ctx, size, &fvalue);
      if(status != VC_CONTAINER_SUCCESS) return status;

      if(id == MKV_ELEMENT_ID_SAMPLING_FREQUENCY)
         track_module->es_type.audio.sampling_frequency = fvalue;

      if(id == MKV_ELEMENT_ID_OUTPUT_SAMPLING_FREQUENCY)
         track_module->es_type.audio.output_sampling_frequency = fvalue;

      return status;
   }

   /* The rest are just unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   switch(id)
   {
   case MKV_ELEMENT_ID_CHANNELS: track_module->es_type.audio.channels = value; break;
   case MKV_ELEMENT_ID_BIT_DEPTH: track_module->es_type.audio.bit_depth = value; break;
   default: break;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_element_encoding( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = module->parsing->priv->module;
   VC_CONTAINER_STATUS_T status;
   status = mkv_read_elements(p_ctx, id, size);
   track_module->encodings_num++;
   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_encoding( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = module->parsing->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint64_t value;

   /* These are just unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(track_module->encodings_num >= MKV_MAX_ENCODINGS) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

   switch(id)
   {
   case MKV_ELEMENT_ID_CONTENT_ENCODING_TYPE:
      if(value == 0) track_module->encodings[track_module->encodings_num].type = MKV_CONTENT_ENCODING_COMPRESSION_ZLIB;
      if(value == 1) track_module->encodings[track_module->encodings_num].type = MKV_CONTENT_ENCODING_ENCRYPTION;
      else track_module->encodings[track_module->encodings_num].type = MKV_CONTENT_ENCODING_UNKNOWN;
      break;
   default: break;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_compression( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = module->parsing->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint64_t value;
   uint8_t *data;

   if(id == MKV_ELEMENT_ID_CONTENT_COMPRESSION_ALGO)
   {
      status = mkv_read_element_data_uint(p_ctx, size, &value);
      if(status != VC_CONTAINER_SUCCESS) return status;

      if(value == 0) track_module->encodings[track_module->encodings_num].type = MKV_CONTENT_ENCODING_COMPRESSION_ZLIB;
      if(value == 3) track_module->encodings[track_module->encodings_num].type = MKV_CONTENT_ENCODING_COMPRESSION_HEADER;
      else track_module->encodings[track_module->encodings_num].type = MKV_CONTENT_ENCODING_UNKNOWN;
      return status;
   }

   if(id == MKV_ELEMENT_ID_CONTENT_COMPRESSION_SETTINGS)
   {
      if(track_module->encodings[track_module->encodings_num].type == MKV_CONTENT_ENCODING_COMPRESSION_HEADER)
      {
         if(size > MKV_MAX_ENCODING_DATA) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

         data = malloc((int)size);
         if(!data) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

         track_module->encodings[track_module->encodings_num].data = data;
         track_module->encodings[track_module->encodings_num].data_size = READ_BYTES(p_ctx, data, size);
         if(track_module->encodings[track_module->encodings_num].data_size != size)
            track_module->encodings[track_module->encodings_num].data_size = 0;
         return STREAM_STATUS(p_ctx);
      }
      else
      {
         SKIP_BYTES(p_ctx, size);
      }
      return STREAM_STATUS(p_ctx);
   }

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_info( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint64_t value;
   double fvalue;

   switch(id)
   {
   case MKV_ELEMENT_ID_TIMECODE_SCALE:
      status = mkv_read_element_data_uint(p_ctx, size, &value);
      if(status != VC_CONTAINER_SUCCESS) break;
      module->timecode_scale = value;
      break;
   case MKV_ELEMENT_ID_DURATION:
      status = mkv_read_element_data_float(p_ctx, size, &fvalue);
      if(status != VC_CONTAINER_SUCCESS) break;
      module->duration = fvalue;
      break;
   case MKV_ELEMENT_ID_TITLE:
   case MKV_ELEMENT_ID_MUXING_APP:
   case MKV_ELEMENT_ID_WRITING_APP:
      SKIP_STRING(p_ctx, size, "string");
      break;

   default: break;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_seek_head( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint64_t value;

   if(id == MKV_ELEMENT_ID_SEEK)
   {
      module->seekhead_elem_id = MKV_ELEMENT_ID_UNKNOWN;
      module->seekhead_elem_offset = -1;
      status = mkv_read_elements(p_ctx, id, size);
      if(status == VC_CONTAINER_SUCCESS && !module->cues_offset &&
         module->seekhead_elem_id == MKV_ELEMENT_ID_CUES && module->seekhead_elem_offset)
         module->cues_offset = module->seekhead_elem_offset;
      if(status == VC_CONTAINER_SUCCESS && !module->tags_offset &&
         module->seekhead_elem_id == MKV_ELEMENT_ID_TAGS && module->seekhead_elem_offset)
         module->tags_offset = module->seekhead_elem_offset;
      return status;
   }

   if(id == MKV_ELEMENT_ID_SEEK_ID)
   {
      MKV_ELEMENT_T *element = mkv_elements_list;
      id = MKV_READ_ID(p_ctx, "Element ID");
      module->seekhead_elem_id = id;

      /* Find out which Element we are dealing with */
      while(element->id && id != element->id) element++;
      LOG_FORMAT(p_ctx, "element: %s (ID 0x%x)", element->psz_name, id);
   }
   else if(id == MKV_ELEMENT_ID_SEEK_POSITION)
   {
      status = mkv_read_element_data_uint(p_ctx, size, &value);
      if(status != VC_CONTAINER_SUCCESS) return status;
      LOG_FORMAT(p_ctx, "offset: %"PRIi64, value + module->segment_offset);
      module->seekhead_elem_offset = value + module->segment_offset;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_element_cues( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(id);
   VC_CONTAINER_PARAM_UNUSED(size);
   module->cues_offset = module->element_offset;
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_cue_point( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint64_t value;

   /* All the values are unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   switch(id)
   {
   case MKV_ELEMENT_ID_CUE_TIME:
      module->cue_timecode = value; break;
   case MKV_ELEMENT_ID_CUE_TRACK:
      module->cue_track = value; break;
   case MKV_ELEMENT_ID_CUE_CLUSTER_POSITION:
      module->cue_cluster_offset = value; break;
   case MKV_ELEMENT_ID_CUE_BLOCK_NUMBER:
      module->cue_block = value; break;
   default: break;
   }

   return status;
}

static VC_CONTAINER_STATUS_T mkv_read_subelements_cluster( VC_CONTAINER_T *p_ctx, MKV_ELEMENT_ID_T id, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint64_t value;

   /* The rest are just unsigned integers */
   status = mkv_read_element_data_uint(p_ctx, size, &value);
   if(status != VC_CONTAINER_SUCCESS) return status;

   switch(id)
   {
   //XXX
   case MKV_ELEMENT_ID_TIMECODE: module->state.cluster_timecode = value; break;
   case MKV_ELEMENT_ID_BLOCK_DURATION: module->state.frame_duration = value; break;
   default: break;
   }

   return status;
}

/*******************************/

static VC_CONTAINER_STATUS_T mkv_skip_element(VC_CONTAINER_T *p_ctx,
      MKV_READER_STATE_T *state)
{
   /* Skip any trailing data from the current element */
   int64_t skip = state->levels[state->level].offset +
      state->levels[state->level].size - STREAM_POSITION(p_ctx);

   if(skip < 0) /* Check for overruns */
   {
      /* Things have gone really bad here and we ended up reading past the end of the
       * element. We could maybe try to be clever and recover by seeking back to the end
       * of the element. However if we get there, the file is clearly corrupted so there's
       * no guarantee it would work anyway. */
      LOG_DEBUG(p_ctx, "%"PRIi64" bytes overrun past the end of the element", -skip);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if(skip)
      LOG_FORMAT(p_ctx, "%"PRIi64" bytes left unread at the end of element", skip);

   state->level--;

   if (skip >= MKV_MAX_ELEMENT_SIZE)
      return SEEK(p_ctx, STREAM_POSITION(p_ctx) + skip);
   SKIP_BYTES(p_ctx, skip);
   return STREAM_STATUS(p_ctx);
}

static VC_CONTAINER_STATUS_T mkv_find_next_element(VC_CONTAINER_T *p_ctx,
      MKV_READER_STATE_T *state, MKV_ELEMENT_ID_T element_id)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t element_size, element_offset;
   MKV_ELEMENT_ID_T id;

   /* Skip all elements until we find the next requested element */
   do
   {
      MKV_ELEMENT_T *element = mkv_cluster_elements_list;

      /* Check whether we've reach the end of the parent element */
      if(STREAM_POSITION(p_ctx) >= state->levels[state->level].offset +
         state->levels[state->level].size)
         return VC_CONTAINER_ERROR_NOT_FOUND;

      status = mkv_read_element_header(p_ctx, INT64_C(1) << 30, &id,
            &element_size, state->levels[state->level].id, &element);
      element_offset = STREAM_POSITION(p_ctx);
      if(status != VC_CONTAINER_SUCCESS) return status;
      if(id == element_id) break;
      if(element_id == MKV_ELEMENT_ID_BLOCKGROUP && id == MKV_ELEMENT_ID_SIMPLE_BLOCK) break;

      if(element_id == MKV_ELEMENT_ID_BLOCK && id == MKV_ELEMENT_ID_REFERENCE_BLOCK)
         state->seen_ref_block = 1;

      /* Check whether we've reached the end of the parent element */
      if(STREAM_POSITION(p_ctx) + element_size >= state->levels[state->level].offset +
         state->levels[state->level].size)
         return VC_CONTAINER_ERROR_NOT_FOUND;

      status = mkv_read_element_data(p_ctx, element, element_size, INT64_C(1) << 30);
#if 0
      if(element_size < MKV_MAX_ELEMENT_SIZE) SKIP_BYTES(p_ctx, element_size);
      else SEEK(p_ctx, STREAM_POSITION(p_ctx) + element_size);
#endif
   } while (status == VC_CONTAINER_SUCCESS && STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS);

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS)
      return STREAM_STATUS(p_ctx);

   state->level++;
   state->levels[state->level].offset = element_offset;
   state->levels[state->level].size = element_size;
   state->levels[state->level].id = id;
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T mkv_find_next_segment(VC_CONTAINER_T *p_ctx,
      MKV_READER_STATE_T *state)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t element_size, element_offset;
   MKV_ELEMENT_ID_T id;

   /* Skip all elements until we find the next segment */
   do
   {
      MKV_ELEMENT_T *element = mkv_cluster_elements_list;

      status = mkv_read_element_header(p_ctx, INT64_C(-1), &id,
            &element_size, MKV_ELEMENT_ID_INVALID, &element);

      element_offset = STREAM_POSITION(p_ctx);
      if(status != VC_CONTAINER_SUCCESS) return status;
      if(id == MKV_ELEMENT_ID_SEGMENT) break;

      status = mkv_read_element_data(p_ctx, element, element_size, INT64_C(-1));
   } while (status == VC_CONTAINER_SUCCESS && STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS);

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS)
      return STREAM_STATUS(p_ctx);

   state->level++;
   state->levels[state->level].offset = element_offset;
   state->levels[state->level].size = element_size;
   state->levels[state->level].id = id;
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T mkv_find_next_block(VC_CONTAINER_T *p_ctx, MKV_READER_STATE_T *state)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_NOT_FOUND;

   do
   {
      if(state->level < 0)
      {
#ifdef ENABLE_MKV_EXTRA_LOGGING
         LOG_DEBUG(p_ctx, "find segment");
#endif
         status = mkv_find_next_segment(p_ctx, state);
         if(status == VC_CONTAINER_SUCCESS)
         {
            LOG_ERROR(p_ctx, "multi-segment files not supported");
            status = VC_CONTAINER_ERROR_EOS;
            break;
         }
      }
      if(state->levels[state->level].id == MKV_ELEMENT_ID_BLOCK ||
         state->levels[state->level].id == MKV_ELEMENT_ID_SIMPLE_BLOCK)
      {
         status = mkv_skip_element(p_ctx, state);
      }
      if(state->levels[state->level].id == MKV_ELEMENT_ID_BLOCKGROUP)
      {
#ifdef ENABLE_MKV_EXTRA_LOGGING
         LOG_DEBUG(p_ctx, "find block");
#endif
         state->seen_ref_block = 0;
         state->frame_duration = 0;
         status = mkv_find_next_element(p_ctx, state, MKV_ELEMENT_ID_BLOCK);
         if(status == VC_CONTAINER_SUCCESS) break;

         if(status == VC_CONTAINER_ERROR_NOT_FOUND)
            status = mkv_skip_element(p_ctx, state);
      }
      if(state->levels[state->level].id == MKV_ELEMENT_ID_CLUSTER)
      {
#ifdef ENABLE_MKV_EXTRA_LOGGING
         LOG_DEBUG(p_ctx, "find blockgroup or simpleblock");
#endif
         state->frame_duration = 0;
         status = mkv_find_next_element(p_ctx, state, MKV_ELEMENT_ID_BLOCKGROUP);
         if(status == VC_CONTAINER_SUCCESS &&
            state->levels[state->level].id == MKV_ELEMENT_ID_SIMPLE_BLOCK) break;
         if(status == VC_CONTAINER_ERROR_NOT_FOUND)
            status = mkv_skip_element(p_ctx, state);
      }
      if(state->levels[state->level].id == MKV_ELEMENT_ID_SEGMENT)
      {
#ifdef ENABLE_MKV_EXTRA_LOGGING
         LOG_DEBUG(p_ctx, "find cluster");
#endif
         status = mkv_find_next_element(p_ctx, state, MKV_ELEMENT_ID_CLUSTER);
         if(status == VC_CONTAINER_ERROR_NOT_FOUND)
            status = mkv_skip_element(p_ctx, state);
      }

   } while(status == VC_CONTAINER_SUCCESS && STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS);

   return status == VC_CONTAINER_SUCCESS ? STREAM_STATUS(p_ctx) : status;
}

static VC_CONTAINER_STATUS_T mkv_read_next_frame_header(VC_CONTAINER_T *p_ctx,
      MKV_READER_STATE_T *state, uint32_t *pi_track, uint32_t *pi_length)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = 0;
   unsigned int i, track, flags, is_first_lace = 0;
   int64_t size, pts;

   if ((state->levels[state->level].id == MKV_ELEMENT_ID_BLOCK ||
        state->levels[state->level].id == MKV_ELEMENT_ID_SIMPLE_BLOCK) &&
       state->levels[state->level].data_start + state->levels[state->level].data_offset <
          state->levels[state->level].size)
      goto end;

   status = mkv_find_next_block(p_ctx, state);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* We have a block */
   size = state->levels[state->level].size;
   track = MKV_READ_UINT(p_ctx, "Track Number"); LOG_FORMAT(p_ctx, "Track Number: %u", track);
   pts = (int16_t)MKV_READ_U16(p_ctx, "Timecode");
   flags = MKV_READ_U8(p_ctx, "Flags");
   if(state->levels[state->level].id == MKV_ELEMENT_ID_BLOCK) flags &= 0x0F;

   //TODO improve sanity checking
   /* Sanity checking */
   if(size < 0) return VC_CONTAINER_ERROR_CORRUPTED;
   if(STREAM_STATUS(p_ctx)) return STREAM_STATUS(p_ctx);

   for (i = 0; i < p_ctx->tracks_num; i++)
      if (p_ctx->tracks[i]->priv->module->number == track)
      { track_module = p_ctx->tracks[i]->priv->module; break; }

   /* Finding out if we have a keyframe when dealing with an MKV_ELEMENT_ID_BLOCK is tricky */
   if(state->levels[state->level].id == MKV_ELEMENT_ID_BLOCK &&
      i < p_ctx->tracks_num && p_ctx->tracks[i]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      /* The absence of a ReferenceBlock element tells us if we are a keyframe or not.
       * The problem we have is that this element can appear before as well as after the block element.
       * To avoid seeking to look for this element, we do some guess work. */
      if(!state->seen_ref_block && state->level &&
         state->levels[state->level].offset + state->levels[state->level].size >=
            state->levels[state->level-1].offset + state->levels[state->level-1].size)
         flags |= 0x80;
   }

   /* Take care of the lacing */
   state->lacing_num_frames = 0;
   if(i < p_ctx->tracks_num && (flags & 0x06))
   {
      unsigned int i, value = 0;
      int32_t fs = 0;

      state->lacing_num_frames = MKV_READ_U8(p_ctx, "Lacing Head");
      state->lacing_size = 0;
      switch((flags & 0x06)>>1)
      {
      case 1:  /* Xiph lacing */
         for(i = 0; i < state->lacing_num_frames; i++, fs = 0)
         {
            do {
               value = vc_container_io_read_uint8(p_ctx->priv->io);
               fs += value; size--;
            } while(value == 255);
            LOG_FORMAT(p_ctx, "Frame Size: %i", (int)fs);
            if(state->lacing_num_frames > MKV_MAX_LACING_NUM) continue;
            state->lacing_sizes[state->lacing_num_frames-(i+1)] = fs;
         }
         break;
      case 3:  /* EBML lacing */
         for(i = 0; i < state->lacing_num_frames; i++)
         {
            if(!i) fs = MKV_READ_UINT(p_ctx, "Frame Size");
            else fs += MKV_READ_SINT(p_ctx, "Frame Size");
            LOG_FORMAT(p_ctx, "Frame Size: %i", (int)fs);
            if(state->lacing_num_frames > MKV_MAX_LACING_NUM) continue;
            state->lacing_sizes[state->lacing_num_frames-(i+1)] = fs;
         }
         break;
      default: /* Fixed-size lacing */
         state->lacing_size = size / (state->lacing_num_frames+1);
         break;
      }

      /* There is a max number of laced frames we can support but after we can still give back
       * all the other frames in 1 packet */
      if(state->lacing_num_frames > MKV_MAX_LACING_NUM)
         state->lacing_num_frames = MKV_MAX_LACING_NUM;

      /* Sanity check the size of the frames */
      if(size < 0) return VC_CONTAINER_ERROR_CORRUPTED;
      if(STREAM_STATUS(p_ctx)) return STREAM_STATUS(p_ctx);
      state->lacing_current_size = state->lacing_size;
      if(!state->lacing_size)
      {
         int64_t frames_size = 0;
         for(i = state->lacing_num_frames; i > 0; i--)
         {
            if(frames_size + state->lacing_sizes[i-1] > size) /* return error ? */
               state->lacing_sizes[i-1] = size - frames_size;
            frames_size += state->lacing_sizes[i-1];
         }
      }
      state->lacing_current_size = 0;
      state->lacing_num_frames++; /* Will be decremented further down */
      is_first_lace = 1;
   }

   state->track = i;
   state->pts = (state->cluster_timecode + pts) * module->timecode_scale;
   if(track_module) state->pts *= track_module->timecode_scale;
   state->pts /= 1000;
   state->flags = flags;

   state->frame_duration = state->frame_duration * module->timecode_scale / 1000;
   if(state->lacing_num_frames) state->frame_duration /= state->lacing_num_frames;
   if(!state->frame_duration && track_module)
      state->frame_duration = track_module->frame_duration / 1000;

   state->levels[state->level].data_start = STREAM_POSITION(p_ctx) -
      state->levels[state->level].offset;
   state->levels[state->level].data_offset = 0;

   /* Deal with header stripping compression */
   state->header_size = 0;
   if(track_module && track_module->encodings_num)
   {
      state->header_size = track_module->encodings[0].data_size;
      state->header_data = track_module->encodings[0].data;
   }
   state->header_size_backup = state->header_size;

 end:
   *pi_length = state->levels[state->level].size - state->levels[state->level].data_start -
      state->levels[state->level].data_offset + state->header_size;
   *pi_track = state->track;

   /* Special case for lacing */
   if(state->lacing_num_frames &&
      state->levels[state->level].data_offset >= state->lacing_current_size)
   {
      /* We need to switch to the next lace */
      if(--state->lacing_num_frames)
      {
         unsigned int lace_size = state->lacing_size;
         if(!state->lacing_size) lace_size = state->lacing_sizes[state->lacing_num_frames-1];
         state->lacing_current_size = lace_size;
      }
      state->levels[state->level].data_start += state->levels[state->level].data_offset;
      state->levels[state->level].data_offset = 0;
      if(!is_first_lace && state->frame_duration)
         state->pts += state->frame_duration;
      else if(!is_first_lace)
         state->pts = VC_CONTAINER_TIME_UNKNOWN;

      /* Deal with header stripping compression */
      state->header_data -= (state->header_size_backup - state->header_size);
      state->header_size = state->header_size_backup;
   }
   if(state->lacing_num_frames)
      *pi_length = state->lacing_current_size - state->levels[state->level].data_offset + state->header_size;

   return status == VC_CONTAINER_SUCCESS ? STREAM_STATUS(p_ctx) : status;
}

static VC_CONTAINER_STATUS_T mkv_read_frame_data(VC_CONTAINER_T *p_ctx,
      MKV_READER_STATE_T *state, uint8_t *p_data, uint32_t *pi_length)
{
   uint64_t size;
   uint32_t header_size;

   size = state->levels[state->level].size - state->levels[state->level].data_start -
      state->levels[state->level].data_offset;

   /* Special case for lacing */
   if(state->lacing_num_frames)
   {
      size = state->lacing_current_size - state->levels[state->level].data_offset;

      if(!p_data)
      {
         size = SKIP_BYTES(p_ctx, size);
         state->levels[state->level].data_offset += size;
         return STREAM_STATUS(p_ctx);
      }
   }

   size += state->header_size;

   if(!p_data) return mkv_skip_element(p_ctx, state);
   if(size > *pi_length) size = *pi_length;

   header_size = state->header_size;
   if(header_size)
   {
      if(header_size > size) header_size = size;
      memcpy(p_data, state->header_data, header_size);
      state->header_size -= header_size;
      state->header_data += header_size;
      size -= header_size;
   }

   size = READ_BYTES(p_ctx, p_data + header_size, size);
   state->levels[state->level].data_offset += size;
   *pi_length = size + header_size;

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************
 Functions exported as part of the Container Module API
 *****************************************************************************/

static VC_CONTAINER_STATUS_T mkv_reader_read(VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *p_packet, uint32_t flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *p_track = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t buffer_size = 0, track = 0, data_size;
   MKV_READER_STATE_T *state = &module->state;

   /* If a specific track has been selected, we need to use the track packet state */
   if(flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
   {
      p_track = p_ctx->tracks[p_packet->track];
      state = p_track->priv->module->state;
   }

   /**/
   if(state->eos) return VC_CONTAINER_ERROR_EOS;
   if(state->corrupted) return VC_CONTAINER_ERROR_CORRUPTED;

   /* Look at the next frame header */
   status = mkv_read_next_frame_header(p_ctx, state, &track, &data_size);
   if(status == VC_CONTAINER_ERROR_EOS) state->eos = true;
   if(status == VC_CONTAINER_ERROR_CORRUPTED) state->corrupted = true;
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(track >= p_ctx->tracks_num || !p_ctx->tracks[track]->is_enabled)
   {
      /* Skip frame */
      status = mkv_read_frame_data(p_ctx, state, 0, &data_size);
      if (status != VC_CONTAINER_SUCCESS) return status;
      return VC_CONTAINER_ERROR_CONTINUE;
   }

   if((flags & VC_CONTAINER_READ_FLAG_SKIP) && !(flags & VC_CONTAINER_READ_FLAG_INFO)) /* Skip packet */
      return mkv_read_frame_data(p_ctx, state, 0, &data_size);

   p_packet->dts = p_packet->pts = state->pts;
   p_packet->flags = 0;
   if(state->flags & 0x80) p_packet->flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;
   if(!state->levels[state->level].data_offset) p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   p_packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
   p_packet->size = data_size;
   p_packet->track = track;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
      return mkv_read_frame_data(p_ctx, state, 0, &data_size );
   else if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   /* Read the frame data */
   buffer_size = p_packet->buffer_size;
   status = mkv_read_frame_data(p_ctx, state, p_packet->data, &buffer_size);
   if(status != VC_CONTAINER_SUCCESS)
   {
      /* FIXME */
      return status;
   }

   p_packet->size = buffer_size;
   if(buffer_size != data_size)
      p_packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mkv_reader_seek(VC_CONTAINER_T *p_ctx,
   int64_t *p_offset, VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   MKV_READER_STATE_T *state = &module->state;
   uint64_t offset = 0, prev_offset = 0, position = STREAM_POSITION(p_ctx);
   int64_t time_offset = 0, prev_time_offset = 0;
   unsigned int i, video_track;
   MKV_ELEMENT_T *element = mkv_cue_elements_list;
   int64_t size, element_size;
   MKV_ELEMENT_ID_T id;
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);

   /* Find out if we have a video track */
   for(video_track = 0; video_track < p_ctx->tracks_num; video_track++)
      if(p_ctx->tracks[video_track]->is_enabled &&
         p_ctx->tracks[video_track]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO) break;

   if(!*p_offset) goto end; /* Nothing much to do */
   if(!module->cues_offset) {status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION; goto error;}

   /* We need to do a search in the cue list */
   status = SEEK(p_ctx, module->cues_offset);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   /* First read the header of the cues element */
   status = mkv_read_element_header(p_ctx, INT64_C(-1) /* TODO */, &id, &element_size,
                                    MKV_ELEMENT_ID_SEGMENT, &element);
   if(status != VC_CONTAINER_SUCCESS || id != MKV_ELEMENT_ID_CUES) goto error;
   size = element_size;

   module->elements_list = mkv_cue_elements_list;
   do
   {
      MKV_ELEMENT_T *element = mkv_cue_elements_list;
      int64_t element_offset = STREAM_POSITION(p_ctx);

      /* Exit condition for when we've scanned the whole cues list */
      if(size <= 0)
      {
         if(!(flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
            break; /* Just use the last valid entry in that case */
         status = VC_CONTAINER_ERROR_EOS;
         goto error;
      }

      status = mkv_read_element_header(p_ctx, size, &id, &element_size,
                                       MKV_ELEMENT_ID_CUES, &element);
      size -= STREAM_POSITION(p_ctx) - element_offset;
      if(status == VC_CONTAINER_SUCCESS && element->id != MKV_ELEMENT_ID_UNKNOWN)
         status = mkv_read_element_data(p_ctx, element, element_size, size);

      if(status != VC_CONTAINER_SUCCESS || element->id == MKV_ELEMENT_ID_UNKNOWN)
      {
         if(!(flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
            break; /* Just use the last valid entry in that case */
         goto error;
      }

      size -= element_size;
      if(id != MKV_ELEMENT_ID_CUE_POINT) continue;

      /* Ignore cue points which don't belong to the track we want */
      if(video_track != p_ctx->tracks_num &&
         p_ctx->tracks[video_track]->priv->module->number != module->cue_track) continue;

      time_offset = module->cue_timecode * module->timecode_scale / 1000;
      offset = module->cue_cluster_offset;
      LOG_DEBUG(p_ctx, "INDEX: %"PRIi64, time_offset);
      if( time_offset > *p_offset )
      {
         if(!(flags & VC_CONTAINER_SEEK_FLAG_FORWARD))
         {
            time_offset = prev_time_offset;
            offset = prev_offset;
         }
         break;
      }
      prev_time_offset = time_offset;
      prev_offset = offset;
   } while( 1 );
   module->elements_list = mkv_elements_list;
   *p_offset = time_offset;

 end:
   /* Try seeking to the requested position */
   status = SEEK(p_ctx, module->segment_offset + offset);
   if(status != VC_CONTAINER_SUCCESS && status != VC_CONTAINER_ERROR_EOS) goto error;

   /* Reinitialise the state */
   memset(state, 0, sizeof(*state));
   state->levels[0].offset = module->segment_offset;
   state->levels[0].size = module->segment_size;
   state->levels[0].id = MKV_ELEMENT_ID_SEGMENT;
   if(status == VC_CONTAINER_ERROR_EOS) state->eos = true;
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *p_track = p_ctx->tracks[i];
      p_track->priv->module->state = state;
   }

   /* If we have a video track, we skip frames until the next keyframe */
   for(i = 0; video_track != p_ctx->tracks_num && i < 200 /* limit search */; )
   {
      uint32_t track, data_size;
      status = mkv_read_next_frame_header(p_ctx, state, &track, &data_size);
      if(status != VC_CONTAINER_SUCCESS) break; //FIXME
      if(track == video_track) i++;
      if(track == video_track && (state->flags & 0x80) &&
         state->pts >= time_offset) break;

      /* Skip frame */
      status = mkv_read_frame_data(p_ctx, state, 0, &data_size);
   }

   return VC_CONTAINER_SUCCESS;

 error:
     /* Reset everything as it was before the seek */
     SEEK(p_ctx, position);
     if(status == VC_CONTAINER_SUCCESS) status = VC_CONTAINER_ERROR_FAILED;
     return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mkv_reader_close(VC_CONTAINER_T *p_ctx)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i, j;

   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      for(j = 0; j < MKV_MAX_ENCODINGS; j++)
         free(p_ctx->tracks[i]->priv->module->encodings[j].data);
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   }
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T mkv_reader_open(VC_CONTAINER_T *p_ctx)
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   uint8_t buffer[4];

   // Can start with ASCII strings ????

   /* Check for an EBML element */
   if(PEEK_BYTES(p_ctx, buffer, 4) < 4 ||
      buffer[0] != 0x1A || buffer[1] != 0x45 || buffer[2] != 0xDF || buffer[3] != 0xA3)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /*
    *  We are dealing with an MKV file
    */

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) {status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error;}
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;
   module->elements_list = mkv_elements_list;

   /* Read and sanity check the EBML header */
   status = mkv_read_element(p_ctx, INT64_C(-1), MKV_ELEMENT_ID_UNKNOWN);
   if(status != VC_CONTAINER_SUCCESS) goto error;
   if(!module->is_doctype_valid) {status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED; goto error;}

   /* Read the other elements until we find the start of the data */
   do
   {
      status = mkv_read_element(p_ctx, INT64_C(-1), MKV_ELEMENT_ID_UNKNOWN);
      if(status != VC_CONTAINER_SUCCESS) break;

      if(module->cluster_offset) break;
   } while(1);

   /* Bail out if we didn't find a track */
   if(!p_ctx->tracks_num)
   {
      status = VC_CONTAINER_ERROR_NO_TRACK_AVAILABLE;
      goto error;
   }

   /*
    *  We now have all the information we really need to start playing the stream
    */

   p_ctx->priv->pf_close = mkv_reader_close;
   p_ctx->priv->pf_read = mkv_reader_read;
   p_ctx->priv->pf_seek = mkv_reader_seek;
   p_ctx->duration = module->duration / 1000 * module->timecode_scale;

   /* Check if we're done */
   if(!STREAM_SEEKABLE(p_ctx))
      return VC_CONTAINER_SUCCESS;

   if(module->cues_offset && (int64_t)module->cues_offset < p_ctx->size)
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   if(module->tags_offset)
   {
      status = SEEK(p_ctx, module->tags_offset);
      if(status == VC_CONTAINER_SUCCESS)
         status = mkv_read_element(p_ctx, INT64_C(-1) /*FIXME*/, MKV_ELEMENT_ID_SEGMENT);
   }

   /* Seek back to the start of the data */
   return SEEK(p_ctx, module->state.levels[1].offset);

 error:
   LOG_DEBUG(p_ctx, "mkv: error opening stream (%i)", status);
   if(module) mkv_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open mkv_reader_open
#endif
