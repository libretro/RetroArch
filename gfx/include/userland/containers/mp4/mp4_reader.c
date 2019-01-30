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
#define VC_CONTAINER_MODULE_T VC_CONTAINER_MODULE_MP4_READER_T
#define VC_CONTAINER_TRACK_MODULE_T VC_CONTAINER_TRACK_MODULE_MP4_READER_T

#define CONTAINER_IS_BIG_ENDIAN
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"
#include "containers/mp4/mp4_common.h"
#undef CONTAINER_HELPER_LOG_INDENT
#define CONTAINER_HELPER_LOG_INDENT(a) (a)->priv->module->box_level

VC_CONTAINER_STATUS_T mp4_reader_open( VC_CONTAINER_T *p_ctx );

/******************************************************************************
TODO:
- aspect ratio
- itunes gapless
- edit list
- subpicture track
******************************************************************************/

/******************************************************************************
Defines.
******************************************************************************/
#define MP4_TRACKS_MAX 16

#define MP4_BOX_MIN_HEADER_SIZE 8
#define MP4_MAX_BOX_SIZE  (1<<29) /* Does not apply to the mdat box */
#define MP4_MAX_BOX_LEVEL 10

#define MP4_MAX_SAMPLES_BATCH_SIZE (16*1024)

#define MP4_SKIP_U8(ctx,n)   (size -= 1, SKIP_U8(ctx,n))
#define MP4_SKIP_U16(ctx,n)  (size -= 2, SKIP_U16(ctx,n))
#define MP4_SKIP_U24(ctx,n)  (size -= 3, SKIP_U24(ctx,n))
#define MP4_SKIP_U32(ctx,n)  (size -= 4, SKIP_U32(ctx,n))
#define MP4_SKIP_U64(ctx,n)  (size -= 8, SKIP_U64(ctx,n))
#define MP4_READ_U8(ctx,n)   (size -= 1, READ_U8(ctx,n))
#define MP4_READ_U16(ctx,n)  (size -= 2, READ_U16(ctx,n))
#define MP4_READ_U24(ctx,n)  (size -= 3, READ_U24(ctx,n))
#define MP4_READ_U32(ctx,n)  (size -= 4, READ_U32(ctx,n))
#define MP4_READ_U64(ctx,n)  (size -= 8, READ_U64(ctx,n))
#define MP4_READ_FOURCC(ctx,n)  (size -= 4, READ_FOURCC(ctx,n))
#define MP4_SKIP_FOURCC(ctx,n)  (size -= 4, SKIP_FOURCC(ctx,n))
#define MP4_READ_BYTES(ctx,buffer,sz) (size -= sz, READ_BYTES(ctx,buffer,sz))
#define MP4_SKIP_BYTES(ctx,sz) (size -= sz, SKIP_BYTES(ctx,sz))
#define MP4_SKIP_STRING(ctx,sz,n) (size -= sz, SKIP_STRING(ctx,sz,n))

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct
{
   VC_CONTAINER_STATUS_T status;

   int64_t  duration;
   int64_t  pts;
   int64_t  dts;

   uint32_t sample;
   int64_t offset;
   unsigned int sample_offset;
   unsigned int sample_size;

   uint32_t sample_duration;
   uint32_t sample_duration_count;
   int32_t sample_composition_offset;
   uint32_t sample_composition_count;

   uint32_t next_sync_sample;
   bool keyframe;

   uint32_t samples_per_chunk;
   uint32_t chunks;
   uint32_t samples_in_chunk;

   struct {
      uint32_t entry;
   } sample_table[MP4_SAMPLE_TABLE_NUM];

} MP4_READER_STATE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   MP4_READER_STATE_T state;

   int64_t timescale;
   uint8_t object_type_indication;

   uint32_t sample_size;
   struct {
      int64_t offset;
      uint32_t entries;
      uint32_t entry_size;
   } sample_table[MP4_SAMPLE_TABLE_NUM];

   uint32_t samples_batch_size;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   int64_t box_offset;
   int box_level;

   MP4_BRAND_T brand;

   int64_t timescale;

   VC_CONTAINER_TRACK_T *tracks[MP4_TRACKS_MAX];
   unsigned int current_track;

   bool found_moov;
   int64_t data_offset;
   int64_t data_size;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Static functions within this file.
******************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box( VC_CONTAINER_T *p_ctx, int64_t size, MP4_BOX_TYPE_T parent_type );
static VC_CONTAINER_STATUS_T mp4_read_boxes( VC_CONTAINER_T *p_ctx, int64_t size, MP4_BOX_TYPE_T type );
static VC_CONTAINER_STATUS_T mp4_read_box_ftyp( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_moov( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_mvhd( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_trak( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_tkhd( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_mdia( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_mdhd( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_hdlr( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_minf( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_vmhd( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_smhd( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_dinf( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_dref( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stbl( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stsd( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stts( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_ctts( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stsc( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stsz( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stco( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_co64( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_stss( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_vide( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_soun( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_text( VC_CONTAINER_T *p_ctx, int64_t size );

static VC_CONTAINER_STATUS_T mp4_read_box_esds( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_vide_avcC( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_vide_d263( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_soun_damr( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_soun_dawp( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_soun_devc( VC_CONTAINER_T *p_ctx, int64_t size );
static VC_CONTAINER_STATUS_T mp4_read_box_soun_wave( VC_CONTAINER_T *p_ctx, int64_t size );

static struct {
  const MP4_BOX_TYPE_T type;
  VC_CONTAINER_STATUS_T (*pf_func)( VC_CONTAINER_T *, int64_t );
  const MP4_BOX_TYPE_T parent_type;
} mp4_box_list[] =
{
   {MP4_BOX_TYPE_FTYP, mp4_read_box_ftyp, MP4_BOX_TYPE_ROOT},
   {MP4_BOX_TYPE_MDAT, 0,                 MP4_BOX_TYPE_ROOT},
   {MP4_BOX_TYPE_MOOV, mp4_read_box_moov, MP4_BOX_TYPE_ROOT},
   {MP4_BOX_TYPE_MVHD, mp4_read_box_mvhd, MP4_BOX_TYPE_MOOV},
   {MP4_BOX_TYPE_TRAK, mp4_read_box_trak, MP4_BOX_TYPE_MOOV},
   {MP4_BOX_TYPE_TKHD, mp4_read_box_tkhd, MP4_BOX_TYPE_TRAK},
   {MP4_BOX_TYPE_MDIA, mp4_read_box_mdia, MP4_BOX_TYPE_TRAK},
   {MP4_BOX_TYPE_MDHD, mp4_read_box_mdhd, MP4_BOX_TYPE_MDIA},
   {MP4_BOX_TYPE_HDLR, mp4_read_box_hdlr, MP4_BOX_TYPE_MDIA},
   {MP4_BOX_TYPE_MINF, mp4_read_box_minf, MP4_BOX_TYPE_MDIA},
   {MP4_BOX_TYPE_VMHD, mp4_read_box_vmhd, MP4_BOX_TYPE_MINF},
   {MP4_BOX_TYPE_SMHD, mp4_read_box_smhd, MP4_BOX_TYPE_MINF},
   {MP4_BOX_TYPE_DINF, mp4_read_box_dinf, MP4_BOX_TYPE_MINF},
   {MP4_BOX_TYPE_DREF, mp4_read_box_dref, MP4_BOX_TYPE_DINF},
   {MP4_BOX_TYPE_STBL, mp4_read_box_stbl, MP4_BOX_TYPE_MINF},
   {MP4_BOX_TYPE_STSD, mp4_read_box_stsd, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_STTS, mp4_read_box_stts, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_CTTS, mp4_read_box_ctts, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_STSC, mp4_read_box_stsc, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_STSZ, mp4_read_box_stsz, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_STCO, mp4_read_box_stco, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_CO64, mp4_read_box_co64, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_STSS, mp4_read_box_stss, MP4_BOX_TYPE_STBL},
   {MP4_BOX_TYPE_VIDE, mp4_read_box_vide, MP4_BOX_TYPE_STSD},
   {MP4_BOX_TYPE_SOUN, mp4_read_box_soun, MP4_BOX_TYPE_STSD},
   {MP4_BOX_TYPE_TEXT, mp4_read_box_text, MP4_BOX_TYPE_STSD},

   /* Codec specific boxes */
   {MP4_BOX_TYPE_AVCC, mp4_read_box_vide_avcC, MP4_BOX_TYPE_VIDE},
   {MP4_BOX_TYPE_D263, mp4_read_box_vide_d263, MP4_BOX_TYPE_VIDE},
   {MP4_BOX_TYPE_ESDS, mp4_read_box_esds, MP4_BOX_TYPE_VIDE},
   {MP4_BOX_TYPE_DAMR, mp4_read_box_soun_damr, MP4_BOX_TYPE_SOUN},
   {MP4_BOX_TYPE_DAWP, mp4_read_box_soun_dawp, MP4_BOX_TYPE_SOUN},
   {MP4_BOX_TYPE_DEVC, mp4_read_box_soun_devc, MP4_BOX_TYPE_SOUN},
   {MP4_BOX_TYPE_WAVE, mp4_read_box_soun_wave, MP4_BOX_TYPE_SOUN},
   {MP4_BOX_TYPE_ESDS, mp4_read_box_esds, MP4_BOX_TYPE_SOUN},

   {MP4_BOX_TYPE_UNKNOWN, 0,              MP4_BOX_TYPE_UNKNOWN}
};

static struct {
  const VC_CONTAINER_FOURCC_T type;
  const VC_CONTAINER_FOURCC_T codec;
  bool batch;
} mp4_codec_mapping[] =
{
  {VC_FOURCC('a','v','c','1'), VC_CONTAINER_CODEC_H264, 0},
  {VC_FOURCC('m','p','4','v'), VC_CONTAINER_CODEC_MP4V, 0},
  {VC_FOURCC('s','2','6','3'), VC_CONTAINER_CODEC_H263, 0},
  {VC_FOURCC('m','p','e','g'), VC_CONTAINER_CODEC_MP2V, 0},
  {VC_FOURCC('m','j','p','a'), VC_CONTAINER_CODEC_MJPEGA, 0},
  {VC_FOURCC('m','j','p','b'), VC_CONTAINER_CODEC_MJPEGB, 0},

  {VC_FOURCC('j','p','e','g'), VC_CONTAINER_CODEC_JPEG, 0},

  {VC_FOURCC('m','p','4','a'), VC_CONTAINER_CODEC_MP4A, 0},
  {VC_FOURCC('s','a','m','r'), VC_CONTAINER_CODEC_AMRNB, 0},
  {VC_FOURCC('s','a','w','b'), VC_CONTAINER_CODEC_AMRWB, 0},
  {VC_FOURCC('s','a','w','p'), VC_CONTAINER_CODEC_AMRWBP, 0},
  {VC_FOURCC('a','c','-','3'), VC_CONTAINER_CODEC_AC3, 0},
  {VC_FOURCC('e','c','-','3'), VC_CONTAINER_CODEC_EAC3, 0},
  {VC_FOURCC('s','e','v','c'), VC_CONTAINER_CODEC_EVRC, 0},
  {VC_FOURCC('e','v','r','c'), VC_CONTAINER_CODEC_EVRC, 0},
  {VC_FOURCC('s','q','c','p'), VC_CONTAINER_CODEC_QCELP, 0},
  {VC_FOURCC('a','l','a','w'), VC_CONTAINER_CODEC_ALAW, 1},
  {VC_FOURCC('u','l','a','w'), VC_CONTAINER_CODEC_MULAW, 1},
  {VC_FOURCC('t','w','o','s'), VC_CONTAINER_CODEC_PCM_SIGNED_BE, 1},
  {VC_FOURCC('s','o','w','t'), VC_CONTAINER_CODEC_PCM_SIGNED_LE, 1},

  {0, 0},
};
static VC_CONTAINER_FOURCC_T mp4_box_type_to_codec(VC_CONTAINER_FOURCC_T type)
{
   int i;
   for(i = 0; mp4_codec_mapping[i].type; i++ )
      if(mp4_codec_mapping[i].type == type) break;
   return mp4_codec_mapping[i].codec;
}

static bool codec_needs_batch_mode(VC_CONTAINER_FOURCC_T codec)
{
   int i;
   for(i = 0; mp4_codec_mapping[i].codec; i++ )
      if(mp4_codec_mapping[i].codec == codec) break;
   return mp4_codec_mapping[i].batch;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_header( VC_CONTAINER_T *p_ctx, int64_t size,
   MP4_BOX_TYPE_T *box_type, int64_t *box_size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   int64_t offset = STREAM_POSITION(p_ctx);

   module->box_offset = offset;

   *box_size = _READ_U32(p_ctx);
   *box_type = _READ_FOURCC(p_ctx);
   if(!*box_type) return VC_CONTAINER_ERROR_CORRUPTED;

   if(*box_size == 1) *box_size = _READ_U64(p_ctx);
   LOG_FORMAT(p_ctx, "- Box %4.4s, Size: %"PRIi64", Offset: %"PRIi64,
              (const char *)box_type, *box_size, offset);

   /* Sanity check the box size */
   if(*box_size < 0 /* Shouldn't ever get that big */ ||
      /* Only the mdat box can really be massive */
      (*box_type != MP4_BOX_TYPE_MDAT && *box_size > MP4_MAX_BOX_SIZE))
   {
      LOG_DEBUG(p_ctx, "box %4.4s has an invalid size (%"PRIi64")",
                (const char *)box_type, *box_size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

#if 0
   /* It is valid for a box to have a zero size (i.e unknown) if it is the last one */
   if(*box_size == 0 && size >= 0) *box_size = size;
   else if(*box_size == 0) *box_size = INT64_C(-1);
#else
   if(*box_size <= 0)
   {
      LOG_DEBUG(p_ctx, "box %4.4s has an invalid size (%"PRIi64")",
                (const char *)box_type, *box_size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }
#endif

   /* Sanity check box size against parent */
   if(size >= 0 && *box_size > size)
   {
      LOG_DEBUG(p_ctx, "box %4.4s is bigger than it should (%"PRIi64" > %"PRIi64")",
            (const char *)box_type, *box_size, size);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   *box_size -= (STREAM_POSITION(p_ctx) - offset);
   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_data( VC_CONTAINER_T *p_ctx,
   MP4_BOX_TYPE_T box_type, int64_t box_size, MP4_BOX_TYPE_T parent_type )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   int64_t offset = STREAM_POSITION(p_ctx);
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   unsigned int i;

   /* Check if the box is a recognised one */
   for(i = 0; mp4_box_list[i].type; i++)
      if(mp4_box_list[i].type == box_type &&
         mp4_box_list[i].parent_type == parent_type) break;
   if(mp4_box_list[i].type == MP4_BOX_TYPE_UNKNOWN)
      for(i = 0; mp4_box_list[i].type; i++)
         if(mp4_box_list[i].type == box_type) break;

   /* Sanity check that the box has the right parent */
   if(mp4_box_list[i].type != MP4_BOX_TYPE_UNKNOWN &&
      mp4_box_list[i].parent_type != MP4_BOX_TYPE_UNKNOWN &&
      parent_type != MP4_BOX_TYPE_UNKNOWN && parent_type != mp4_box_list[i].parent_type)
   {
      LOG_FORMAT(p_ctx, "Ignoring mis-placed box %4.4s", (const char *)&box_type);
      goto skip;
   }

   /* Sanity check that the element isn't too deeply nested */
   if(module->box_level >= 2 * MP4_MAX_BOX_LEVEL)
   {
      LOG_DEBUG(p_ctx, "box %4.4s is too deep. skipping", (const char *)&box_type);
      goto skip;
   }

   module->box_level++;

   /* Call the box specific parsing function */
   if(mp4_box_list[i].pf_func)
      status = mp4_box_list[i].pf_func(p_ctx, box_size);

   module->box_level--;

   if(status != VC_CONTAINER_SUCCESS)
      LOG_DEBUG(p_ctx, "box %4.4s appears to be corrupted (%i)", (const char *)&box_type, status);

 skip:
   /* Skip the rest of the box */
   box_size -= (STREAM_POSITION(p_ctx) - offset);
   if(box_size < 0) /* Check for overruns */
   {
      /* Things have gone really bad here and we ended up reading past the end of the
       * box. We could maybe try to be clever and recover by seeking back to the end
       * of the box. However if we get there, the file is clearly corrupted so there's
       * no guarantee it would work anyway. */
      LOG_DEBUG(p_ctx, "%"PRIi64" bytes overrun past the end of box %4.4s",
                -box_size, (const char *)&box_type);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if(box_size)
      LOG_FORMAT(p_ctx, "%"PRIi64" bytes left unread in box %4.4s",
                 box_size, (const char *)&box_type );

   if(box_size < MP4_MAX_BOX_SIZE) box_size = SKIP_BYTES(p_ctx, box_size);
   else SEEK(p_ctx, STREAM_POSITION(p_ctx) + box_size);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box( VC_CONTAINER_T *p_ctx, int64_t size,
   MP4_BOX_TYPE_T parent_type )
{
   VC_CONTAINER_STATUS_T status;
   MP4_BOX_TYPE_T box_type;
   int64_t box_size;

   status = mp4_read_box_header( p_ctx, size, &box_type, &box_size );
   if(status != VC_CONTAINER_SUCCESS) return status;
   return mp4_read_box_data( p_ctx, box_type, box_size, parent_type );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_boxes( VC_CONTAINER_T *p_ctx, int64_t size,
   MP4_BOX_TYPE_T type)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t offset = STREAM_POSITION(p_ctx);
   bool unknown_size = size < 0;

   /* Read contained boxes */
   module->box_level++;
   while(status == VC_CONTAINER_SUCCESS &&
         (unknown_size || size >= MP4_BOX_MIN_HEADER_SIZE))
   {
      offset = STREAM_POSITION(p_ctx);
      status = mp4_read_box(p_ctx, size, type);
      if(!unknown_size) size -= (STREAM_POSITION(p_ctx) - offset);
   }
   module->box_level--;
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_ftyp( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   module->brand = MP4_READ_FOURCC(p_ctx, "major_brand");
   MP4_SKIP_U32(p_ctx, "minor_version");
   while(size >= 4) MP4_SKIP_FOURCC(p_ctx, "compatible_brands");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_moov( VC_CONTAINER_T *p_ctx, int64_t size )
{
   return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_MOOV);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_mvhd( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t version, i;
   int64_t duration;

   version = MP4_READ_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   if(version)
   {
      MP4_SKIP_U64(p_ctx, "creation_time");
      MP4_SKIP_U64(p_ctx, "modification_time");
      module->timescale = MP4_READ_U32(p_ctx, "timescale");
      duration = MP4_READ_U64(p_ctx, "duration");
   }
   else
   {
      MP4_SKIP_U32(p_ctx, "creation_time");
      MP4_SKIP_U32(p_ctx, "modification_time");
      module->timescale = MP4_READ_U32(p_ctx, "timescale");
      duration = MP4_READ_U32(p_ctx, "duration");
   }

   if(module->timescale)
      p_ctx->duration = duration * 1000000 / module->timescale;

   MP4_SKIP_U32(p_ctx, "rate");
   MP4_SKIP_U16(p_ctx, "volume");
   MP4_SKIP_U16(p_ctx, "reserved");
   for(i = 0; i < 2; i++) MP4_SKIP_U32(p_ctx, "reserved");
   for(i = 0; i < 9; i++) MP4_SKIP_U32(p_ctx, "matrix");
   for(i = 0; i < 6; i++) MP4_SKIP_U32(p_ctx, "pre_defined");
   MP4_SKIP_U32(p_ctx, "next_track_ID");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_trak( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track;

   /* We have a new track. Allocate and initialise our track context */
   if(p_ctx->tracks_num >= MP4_TRACKS_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   p_ctx->tracks[p_ctx->tracks_num] = track =
      vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
   if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STTS].entry_size = 8;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STSZ].entry_size = 4;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STSC].entry_size = 12;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STCO].entry_size = 4;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STSS].entry_size = 4;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_CO64].entry_size = 8;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_CTTS].entry_size = 8;

   status = mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_TRAK);

   /* TODO: Sanity check track */

   track->is_enabled = true;
   track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
   module->current_track++;
   p_ctx->tracks_num++;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_tkhd( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t i, version;
   int64_t duration;

   version = MP4_READ_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   if(version)
   {
      MP4_SKIP_U64(p_ctx, "creation_time");
      MP4_SKIP_U64(p_ctx, "modification_time");
      MP4_SKIP_U32(p_ctx, "track_ID");
      MP4_SKIP_U32(p_ctx, "reserved");
      duration = MP4_READ_U64(p_ctx, "duration");
   }
   else
   {
      MP4_SKIP_U32(p_ctx, "creation_time");
      MP4_SKIP_U32(p_ctx, "modification_time");
      MP4_SKIP_U32(p_ctx, "track_ID");
      MP4_SKIP_U32(p_ctx, "reserved");
      duration = MP4_READ_U32(p_ctx, "duration");
   }

   if(module->timescale)
      duration = duration * 1000000 / module->timescale;

   for(i = 0; i < 2; i++) MP4_SKIP_U32(p_ctx, "reserved");
   MP4_SKIP_U16(p_ctx, "layer");
   MP4_SKIP_U16(p_ctx, "alternate_group");
   MP4_SKIP_U16(p_ctx, "volume");
   MP4_SKIP_U16(p_ctx, "reserved");
   for(i = 0; i < 9; i++) MP4_SKIP_U32(p_ctx, "matrix");

   MP4_SKIP_U32(p_ctx, "width");
   MP4_SKIP_U32(p_ctx, "height");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_mdia( VC_CONTAINER_T *p_ctx, int64_t size )
{
   return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_MDIA);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_mdhd( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   uint32_t version, timescale;
   int64_t duration;

   version = MP4_READ_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   if(version)
   {
      MP4_SKIP_U64(p_ctx, "creation_time");
      MP4_SKIP_U64(p_ctx, "modification_time");
      timescale = MP4_READ_U32(p_ctx, "timescale");
      duration = MP4_READ_U64(p_ctx, "duration");
   }
   else
   {
      MP4_SKIP_U32(p_ctx, "creation_time");
      MP4_SKIP_U32(p_ctx, "modification_time");
      timescale = MP4_READ_U32(p_ctx, "timescale");
      duration = MP4_READ_U32(p_ctx, "duration");
   }

   if(timescale) duration = duration * 1000000 / timescale;
   track_module->timescale = timescale;

   MP4_SKIP_U16(p_ctx, "language"); /* ISO-639-2/T language code */
   MP4_SKIP_U16(p_ctx, "pre_defined");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_hdlr( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   uint32_t i, fourcc, string_size;
   VC_CONTAINER_ES_TYPE_T es_type = VC_CONTAINER_ES_TYPE_UNKNOWN;

   if(size <= 24) return VC_CONTAINER_ERROR_CORRUPTED;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   MP4_SKIP_U32(p_ctx, "pre-defined");

   fourcc = MP4_READ_FOURCC(p_ctx, "handler_type");
   if(fourcc == MP4_BOX_TYPE_VIDE) es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   if(fourcc == MP4_BOX_TYPE_SOUN) es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   if(fourcc == MP4_BOX_TYPE_TEXT) es_type = VC_CONTAINER_ES_TYPE_SUBPICTURE;
   track->format->es_type = es_type;

   for(i = 0; i < 3; i++) MP4_SKIP_U32(p_ctx, "reserved");

   string_size = size;
   if(module->brand == MP4_BRAND_QT)
      string_size = MP4_READ_U8(p_ctx, "string_size");

   if(size < 0) return VC_CONTAINER_ERROR_CORRUPTED;
   if(string_size > size) string_size = size;

   MP4_SKIP_STRING(p_ctx, string_size, "name");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_minf( VC_CONTAINER_T *p_ctx, int64_t size )
{
   return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_MINF);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_vmhd( VC_CONTAINER_T *p_ctx, int64_t size )
{
   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   MP4_SKIP_U16(p_ctx, "graphicsmode");
   MP4_SKIP_U16(p_ctx, "opcolor");
   MP4_SKIP_U16(p_ctx, "opcolor");
   MP4_SKIP_U16(p_ctx, "opcolor");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_smhd( VC_CONTAINER_T *p_ctx, int64_t size )
{
   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   MP4_SKIP_U16(p_ctx, "balance");
   MP4_SKIP_U16(p_ctx, "reserved");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_dinf( VC_CONTAINER_T *p_ctx, int64_t size )
{
   return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_DINF);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_dref( VC_CONTAINER_T *p_ctx, int64_t size )
{
   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   MP4_SKIP_U32(p_ctx, "entry_count");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stbl( VC_CONTAINER_T *p_ctx, int64_t size )
{
   return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_STBL);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stsd( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   VC_CONTAINER_STATUS_T status;
   MP4_BOX_TYPE_T box_type;
   int64_t box_size;
   uint32_t count;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   count = MP4_READ_U32(p_ctx, "entry_count");
   if(!count) return VC_CONTAINER_ERROR_CORRUPTED;

   status = mp4_read_box_header( p_ctx, size, &box_type, &box_size );
   if(status != VC_CONTAINER_SUCCESS) return status;

   track->format->codec = mp4_box_type_to_codec(box_type);
   if(!track->format->codec) track->format->codec = box_type;

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO) box_type = MP4_BOX_TYPE_VIDE;
   if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO) box_type = MP4_BOX_TYPE_SOUN;
   if(track->format->es_type == VC_CONTAINER_ES_TYPE_SUBPICTURE) box_type = MP4_BOX_TYPE_TEXT;
   status = mp4_read_box_data( p_ctx, box_type, box_size, MP4_BOX_TYPE_STSD );
   if(status != VC_CONTAINER_SUCCESS) return status;

   /* Special treatment for MPEG4 */
   if(track->format->codec == VC_CONTAINER_CODEC_MP4A)
   {
      switch (track->priv->module->object_type_indication)
      {
      case MP4_MPEG4_AAC_LC_OBJECT_TYPE:
      case MP4_MPEG2_AAC_LC_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_MP4A; break;
      case MP4_MP3_OBJECT_TYPE:
      case MP4_MPEG1_AUDIO_OBJECT_TYPE:
      case MP4_KTF_MP3_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_MPGA; break;
      case MP4_SKT_EVRC_2V1_OBJECT_TYPE:
      case MP4_SKT_EVRC_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_EVRC; break;
      case MP4_3GPP2_QCELP_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_QCELP; break;
      default:
         track->format->codec = VC_CONTAINER_CODEC_UNKNOWN; break;
      }
   }
   else if(track->format->codec == VC_CONTAINER_CODEC_MP4V)
   {
      switch (track->priv->module->object_type_indication)
      {
      case MP4_MPEG4_VISUAL_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_MP4V; break;
      case MP4_JPEG_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_JPEG; break;
      case MP4_MPEG2_SP_OBJECT_TYPE:
      case MP4_MPEG2_SNR_OBJECT_TYPE:
      case MP4_MPEG2_AAC_LC_OBJECT_TYPE:
      case MP4_MPEG2_MP_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_MP2V; break;
      case MP4_MPEG1_VISUAL_OBJECT_TYPE:
         track->format->codec = VC_CONTAINER_CODEC_MP1V; break;
      default:
         track->format->codec = VC_CONTAINER_CODEC_UNKNOWN; break;
      }
   }

   /* For some codecs we process the samples in batches to be more efficient */
   if(codec_needs_batch_mode(track->format->codec))
      track->priv->module->samples_batch_size = MP4_MAX_SAMPLES_BATCH_SIZE;

   /* Fix-up some of the data */
   switch(track->format->codec)
   {
   case VC_CONTAINER_CODEC_ALAW:
   case VC_CONTAINER_CODEC_MULAW:
      track->format->type->audio.bits_per_sample = 8;
      track->priv->module->sample_size = track->format->type->audio.channels;
      break;
   case VC_CONTAINER_CODEC_PCM_SIGNED_LE:
   case VC_CONTAINER_CODEC_PCM_SIGNED_BE:
      track->priv->module->sample_size = (track->format->type->audio.bits_per_sample + 7) /
         8 * track->format->type->audio.channels;
      break;
   case VC_CONTAINER_CODEC_MP4A:
      /* samplerate / channels is sometimes invalid so sanity check it using the codec config data */
      if(track->format->extradata_size >= 2)
      {
         static unsigned int rate[] =
         { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
           16000, 12000, 11025, 8000, 7350 };
         unsigned int samplerate = 0, channels = 0;
         uint8_t *p = track->format->extradata;
         uint32_t index = (p[0] & 7) << 1 | (p[1] >> 7);
         if(index == 15 && track->format->extradata_size >= 5)
         {
            samplerate = (p[1] & 0x7f) << 17 | (p[2] << 9) | (p[3] << 1) | (p[4] >> 7);
            channels = (p[4] >> 3) & 15;
         }
         else if(index < 13)
         {
            samplerate = rate[index];
            channels = (p[1] >> 3) & 15;;
         }

         if(samplerate && samplerate != track->format->type->audio.sample_rate &&
               2 * samplerate != track->format->type->audio.sample_rate)
            track->format->type->audio.sample_rate = samplerate;
         if(channels && channels != track->format->type->audio.channels &&
               2 * channels != track->format->type->audio.channels)
            track->format->type->audio.channels = channels;
      }
      break;
   default: break;
   }

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_cache_table( VC_CONTAINER_T *p_ctx, MP4_SAMPLE_TABLE_T table,
   uint32_t entries, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   uint32_t available_entries, entries_size;

   if(size < 0) return VC_CONTAINER_ERROR_CORRUPTED;

   track_module->sample_table[table].offset = STREAM_POSITION(p_ctx);
   track_module->sample_table[table].entries = entries;

   available_entries = size / track_module->sample_table[table].entry_size;
   if(available_entries < entries)
   {
      LOG_DEBUG(p_ctx, "table has less entries than advertised (%i/%i)", available_entries, entries);
      entries = available_entries;
   }

   entries_size = entries * track_module->sample_table[table].entry_size;
   size = vc_container_io_cache(p_ctx->priv->io, entries_size );
   if(size != entries_size)
   {
      available_entries = size / track_module->sample_table[table].entry_size;
      LOG_DEBUG(p_ctx, "cached less table entries than advertised (%i/%i)", available_entries, entries);
      track_module->sample_table[table].entries = available_entries;
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stts( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   entries = MP4_READ_U32(p_ctx, "entry_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_STTS, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_ctts( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   entries = MP4_READ_U32(p_ctx, "entry_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_CTTS, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stsc( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   entries = MP4_READ_U32(p_ctx, "entry_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_STSC, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stsz( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   track_module->sample_size = READ_U32(p_ctx, "sample_size");
   if(track_module->sample_size) return STREAM_STATUS(p_ctx);

   entries = MP4_READ_U32(p_ctx, "sample_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_STSZ, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stco( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   entries = MP4_READ_U32(p_ctx, "entry_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_STCO, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_co64( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   entries = MP4_READ_U32(p_ctx, "entry_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_CO64, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_stss( VC_CONTAINER_T *p_ctx, int64_t size )
{
   uint32_t entries;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   entries = MP4_READ_U32(p_ctx, "entry_count");
   return mp4_cache_table( p_ctx, MP4_SAMPLE_TABLE_STSS, entries, size );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_esds_descriptor_header(VC_CONTAINER_T *p_ctx, int64_t *size,
   uint32_t *descriptor_length, uint8_t *descriptor_type)
{
   uint32_t byte, length = 0;

   if(*size <= 0) return VC_CONTAINER_ERROR_CORRUPTED;

   *descriptor_type = _READ_U8(p_ctx);
   (*size)--;

   /* Read descriptor size */
   while(*size)
   {
      byte = _READ_U8(p_ctx);
      (*size)--;
      length = (length << 7) | (byte&0x7F);
      if(!(byte & 0x80)) break;
   }

   if(*size <= 0 || length > *size)
   {
      LOG_FORMAT(p_ctx, "esds descriptor is corrupted");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   *descriptor_length = length;
   LOG_FORMAT(p_ctx, "esds descriptor %x, size %i", *descriptor_type, *descriptor_length);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_esds( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   VC_CONTAINER_STATUS_T status;
   uint32_t descriptor_length;
   uint8_t descriptor_type;

   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U24(p_ctx, "flags");

   status = mp4_read_esds_descriptor_header(p_ctx, &size, &descriptor_length, &descriptor_type);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(descriptor_type == 0x3) /* ES descriptor */
   {
      uint8_t flags;

      MP4_SKIP_U16(p_ctx, "es_id");
      flags = MP4_READ_U8(p_ctx, "flags");

      if(flags & 0x80) /* Stream dependence */
         MP4_SKIP_U16(p_ctx, "depend_on_es_id");

      if(flags & 0x40) /* URL */
      {
         uint32_t url_size = MP4_READ_U8(p_ctx, "url_size");
         MP4_SKIP_STRING(p_ctx, url_size, "url");
      }

      if(flags & 0x20) /* OCR_stream*/
         MP4_SKIP_U16(p_ctx, "OCR_es_id");

      status = mp4_read_esds_descriptor_header(p_ctx, &size, &descriptor_length, &descriptor_type);
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   if(descriptor_type == 0x4) /* Decoder Config descriptor */
   {
      track->priv->module->object_type_indication = MP4_READ_U8(p_ctx, "object_type_indication");
      MP4_SKIP_U8(p_ctx, "stream_type");
      MP4_SKIP_U24(p_ctx, "buffer_size_db");
      MP4_SKIP_U32(p_ctx, "max_bitrate");
      track->format->bitrate = MP4_READ_U32(p_ctx, "avg_bitrate");

      if(size <= 0 || descriptor_length <= 13) return STREAM_STATUS(p_ctx);

      status = mp4_read_esds_descriptor_header(p_ctx, &size, &descriptor_length, &descriptor_type);
      if(status != VC_CONTAINER_SUCCESS) return status;
      if(descriptor_type == 0x05 && descriptor_length)
      {
         status = vc_container_track_allocate_extradata(p_ctx, track, descriptor_length);
         if(status != VC_CONTAINER_SUCCESS) return status;
         track->format->extradata_size = MP4_READ_BYTES(p_ctx, track->format->extradata, descriptor_length);
      }
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_vide( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   unsigned int i;

   for(i = 0; i < 6; i++) MP4_SKIP_U8(p_ctx, "reserved");
   MP4_SKIP_U16(p_ctx, "data_reference_index");

   MP4_SKIP_U16(p_ctx, "pre_defined");
   MP4_SKIP_U16(p_ctx, "reserved");
   for(i = 0; i < 3; i++) MP4_SKIP_U32(p_ctx, "pre_defined");
   track->format->type->video.width = MP4_READ_U16(p_ctx, "width");
   track->format->type->video.height = MP4_READ_U16(p_ctx, "height");
   MP4_SKIP_U32(p_ctx, "horizresolution"); /* dpi */
   MP4_SKIP_U32(p_ctx, "vertresolution"); /* dpi */
   MP4_SKIP_U32(p_ctx, "reserved");
   MP4_SKIP_U16(p_ctx, "frame_count");
   MP4_SKIP_BYTES(p_ctx, 32);
   MP4_SKIP_U16(p_ctx, "depth");
   MP4_SKIP_U16(p_ctx, "pre_defined");

   if(size > 0)
      return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_VIDE );

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_vide_avcC( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   VC_CONTAINER_STATUS_T status;

   if(track->format->codec != VC_CONTAINER_CODEC_H264 || size <= 0)
      return VC_CONTAINER_ERROR_CORRUPTED;

   track->format->codec_variant = VC_FOURCC('a','v','c','C');

   status = vc_container_track_allocate_extradata(p_ctx, track, (unsigned int)size);
   if(status != VC_CONTAINER_SUCCESS) return status;
   track->format->extradata_size = READ_BYTES(p_ctx, track->format->extradata, size);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_vide_d263( VC_CONTAINER_T *p_ctx, int64_t size )
{
   MP4_SKIP_FOURCC(p_ctx, "vendor");
   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U8(p_ctx, "level");
   MP4_SKIP_U8(p_ctx, "profile");
   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_soun( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   unsigned int i, version = 0;

   for(i = 0; i < 6; i++) MP4_SKIP_U8(p_ctx, "reserved");
   MP4_SKIP_U16(p_ctx, "data_reference_index");

   version = MP4_READ_U16(p_ctx, "version");
   MP4_SKIP_U16(p_ctx, "revision_level");
   MP4_SKIP_U32(p_ctx, "vendor");

   track->format->type->audio.channels = MP4_READ_U16(p_ctx, "channelcount");
   track->format->type->audio.bits_per_sample = MP4_READ_U16(p_ctx, "samplesize");
   MP4_SKIP_U16(p_ctx, "pre_defined");
   MP4_SKIP_U16(p_ctx, "reserved");
   track->format->type->audio.sample_rate = MP4_READ_U16(p_ctx, "samplerate");
   MP4_SKIP_U16(p_ctx, "samplerate_fp_low");

   if(version == 1)
   {
      MP4_SKIP_U32(p_ctx, "samples_per_packet");
      MP4_SKIP_U32(p_ctx, "bytes_per_packet");
      MP4_SKIP_U32(p_ctx, "bytes_per_frame");
      MP4_SKIP_U32(p_ctx, "bytes_per_sample");
   }

   if(size > 0)
      return mp4_read_box( p_ctx, size, MP4_BOX_TYPE_SOUN );

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_soun_damr( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];

   MP4_SKIP_FOURCC(p_ctx, "vendor");
   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U8(p_ctx, "mode_set");
   MP4_SKIP_U8(p_ctx, "mode_change_period");
   MP4_SKIP_U8(p_ctx, "frame_per_second");

   track->format->type->audio.channels = 1;
   if(track->format->codec == VC_CONTAINER_CODEC_AMRNB)
      track->format->type->audio.sample_rate = 8000;
   else if(track->format->codec == VC_CONTAINER_CODEC_AMRWB)
      track->format->type->audio.sample_rate = 16000;

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_soun_dawp( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];

   MP4_SKIP_FOURCC(p_ctx, "vendor");
   MP4_SKIP_U8(p_ctx, "version");

   track->format->type->audio.channels = 2;
   track->format->type->audio.sample_rate = 16000;

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_soun_devc( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];

   MP4_SKIP_FOURCC(p_ctx, "vendor");
   MP4_SKIP_U8(p_ctx, "version");
   MP4_SKIP_U8(p_ctx, "samples_per_frame");

   track->format->type->audio.channels = 1;
   track->format->type->audio.sample_rate = 8000;

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_soun_wave( VC_CONTAINER_T *p_ctx, int64_t size )
{
   return mp4_read_boxes( p_ctx, size, MP4_BOX_TYPE_SOUN);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_box_text( VC_CONTAINER_T *p_ctx, int64_t size )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(module);

   /* TODO */if(1) return VC_CONTAINER_ERROR_FAILED;

   if(size > 0)
      return mp4_read_box( p_ctx, size, MP4_BOX_TYPE_TEXT );

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

   for(i = 0; i < p_ctx->tracks_num; i++)
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
#ifdef ENABLE_MP4_READER_LOG_STATE
static void mp4_log_state( VC_CONTAINER_T *p_ctx, MP4_READER_STATE_T *state )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   LOG_DEBUG(p_ctx, "state:");
   LOG_DEBUG(p_ctx, "duration: %i, pts %i, dts %i", (int)state->duration,
             (int)state->pts, (int)state->dts);
   LOG_DEBUG(p_ctx, "sample: %i, offset %i, sample_offset %i, sample_size %i",
             state->sample, (int)state->offset, state->sample_offset,
             state->sample_size);
   LOG_DEBUG(p_ctx, "sample_duration: %i, count %i",
             state->sample_duration, state->sample_duration_count);
   LOG_DEBUG(p_ctx, "sample_composition_offset: %i, count %i",
             state->sample_composition_offset, state->sample_composition_count);
   LOG_DEBUG(p_ctx, "next_sync_sample: %i, keyframe %i",
             state->next_sync_sample, state->keyframe);
   LOG_DEBUG(p_ctx, "samples_per_chunk: %i, chunks %i, samples_in_chunk %i",
             state->samples_per_chunk, state->chunks, state->samples_in_chunk);
   LOG_DEBUG(p_ctx, "MP4_SAMPLE_TABLE_STTS %i", state->sample_table[MP4_SAMPLE_TABLE_STTS].entry);
   LOG_DEBUG(p_ctx, "MP4_SAMPLE_TABLE_STSZ %i", state->sample_table[MP4_SAMPLE_TABLE_STSZ].entry);
   LOG_DEBUG(p_ctx, "MP4_SAMPLE_TABLE_STSC %i", state->sample_table[MP4_SAMPLE_TABLE_STSC].entry);
   LOG_DEBUG(p_ctx, "MP4_SAMPLE_TABLE_STCO %i", state->sample_table[MP4_SAMPLE_TABLE_STCO].entry);
   LOG_DEBUG(p_ctx, "MP4_SAMPLE_TABLE_CO64 %i", state->sample_table[MP4_SAMPLE_TABLE_CO64].entry);
   LOG_DEBUG(p_ctx, "MP4_SAMPLE_TABLE_CTTS %i", state->sample_table[MP4_SAMPLE_TABLE_CTTS].entry);
}
#endif /* ENABLE_MP4_READER_LOG_STATE */

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_seek_sample_table( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_MODULE_T *track_module, MP4_READER_STATE_T *state,
   MP4_SAMPLE_TABLE_T table)
{
   int64_t seek_offset;

   /* Seek to the next entry in the table */
   if(state->sample_table[table].entry >= track_module->sample_table[table].entries)
      return VC_CONTAINER_ERROR_EOS;

   seek_offset = track_module->sample_table[table].offset +
      track_module->sample_table[table].entry_size * state->sample_table[table].entry;

   return SEEK(p_ctx, seek_offset);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_sample_table( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_TRACK_MODULE_T *track_module, MP4_READER_STATE_T *state,
   MP4_SAMPLE_TABLE_T table, unsigned int seek)
{
   uint32_t value;

   if(table == MP4_SAMPLE_TABLE_STSZ && track_module->sample_size)
   {
      state->sample_size = track_module->sample_size;
      return state->status;
   }

   /* CO64 support */
   if(table == MP4_SAMPLE_TABLE_STCO &&
      track_module->sample_table[MP4_SAMPLE_TABLE_CO64].entries)
      table = MP4_SAMPLE_TABLE_CO64;

   /* Seek to the next entry in the table */
   if(seek)
   {
      state->status = mp4_seek_sample_table( p_ctx, track_module, state, table );
      if(state->status != VC_CONTAINER_SUCCESS) return state->status;
   }

   switch(table)
   {
   case MP4_SAMPLE_TABLE_STSZ:
      state->sample_size = _READ_U32(p_ctx);
      state->status = STREAM_STATUS(p_ctx);
      break;

   case MP4_SAMPLE_TABLE_STTS:
      state->sample_duration_count = _READ_U32(p_ctx);
      state->sample_duration = _READ_U32(p_ctx);
      state->status = STREAM_STATUS(p_ctx);
      if(state->status != VC_CONTAINER_SUCCESS) break;
      if(!state->sample_duration_count) state->status = VC_CONTAINER_ERROR_CORRUPTED;
      break;

   case MP4_SAMPLE_TABLE_CTTS:
      state->sample_composition_count = _READ_U32(p_ctx);
      state->sample_composition_offset = _READ_U32(p_ctx); /* Converted to signed */
      state->status = STREAM_STATUS(p_ctx);
      if(state->status != VC_CONTAINER_SUCCESS) break;
      if(!state->sample_composition_count) state->status = VC_CONTAINER_ERROR_CORRUPTED;
      break;

   case MP4_SAMPLE_TABLE_STSC:
      state->chunks = _READ_U32(p_ctx);
      state->samples_per_chunk = _READ_U32(p_ctx);
      _SKIP_U32(p_ctx);
      state->status = STREAM_STATUS(p_ctx);
      if(state->status != VC_CONTAINER_SUCCESS) break;

      if(state->sample_table[table].entry + 1 <
         track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entries) value = _READ_U32(p_ctx);
      else value = -1;

      if(!state->chunks || !state->samples_per_chunk || state->chunks >= value )
      {state->status = VC_CONTAINER_ERROR_CORRUPTED; break;}
      state->chunks = value - state->chunks;
      state->samples_in_chunk = state->samples_per_chunk;
      break;

   case MP4_SAMPLE_TABLE_STCO:
   case MP4_SAMPLE_TABLE_CO64:
      state->offset = table == MP4_SAMPLE_TABLE_STCO ? _READ_U32(p_ctx) : _READ_U64(p_ctx);
      state->status = STREAM_STATUS(p_ctx);
      if(state->status != VC_CONTAINER_SUCCESS) break;
      if(!state->offset) state->status = VC_CONTAINER_ERROR_CORRUPTED;
      state->samples_in_chunk = state->samples_per_chunk;
      break;

   case MP4_SAMPLE_TABLE_STSS:
      state->next_sync_sample = _READ_U32(p_ctx);
      state->status = STREAM_STATUS(p_ctx);
      break;

   default: break;
   }

   state->sample_table[table].entry++;
   return state->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_sample_header( VC_CONTAINER_T *p_ctx, uint32_t track,
   MP4_READER_STATE_T *state )
{
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[track]->priv->module;

   if(state->status != VC_CONTAINER_SUCCESS) return state->status;

   if(state->sample_offset < state->sample_size)
      return state->status; /* We still have data left from the current sample */

   /* Switch to the next sample */
   state->offset += state->sample_size;
   state->sample_offset = 0;
   state->sample_size = 0;
   state->sample++;

   if(!state->samples_in_chunk)
   {
      /* We're switching to the next chunk */
      if(!state->chunks)
      {
         /* Seek to the next entry in the STSC */
         state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSC, 1 );
         if(state->status != VC_CONTAINER_SUCCESS) goto error;
      }

      /* Get the offset of the new chunk */
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STCO, 1 );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;

      state->chunks--;
   }
   state->samples_in_chunk--;

   /* Get the new sample size */
   state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSZ, 1 );
   if(state->status != VC_CONTAINER_SUCCESS) goto error;

   /* Get the timestamp */
   if(track_module->timescale)
      state->pts = state->dts = state->duration * 1000000 / track_module->timescale;
   if(!state->sample_duration_count)
   {
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STTS, 1 );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;
   }
   state->sample_duration_count--;

   /* Get the composition time */
   if(track_module->sample_table[MP4_SAMPLE_TABLE_CTTS].entries)
   {
      if(!state->sample_composition_count)
      {
         state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_CTTS, 1 );
         if(state->status != VC_CONTAINER_SUCCESS) goto error;
      }
      if(track_module->timescale)
         state->pts = (state->duration + state->sample_composition_offset) * 1000000 / track_module->timescale;
      state->sample_composition_count--;
   }
   state->duration += state->sample_duration;

   /* Get the keyframe flag */
   if(state->sample_table[MP4_SAMPLE_TABLE_STSS].entry <
         track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries &&
      !state->next_sync_sample)
   {
      mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSS, 1 );
      state->status = VC_CONTAINER_SUCCESS; /* This isn't a critical error */
   }

   state->keyframe = 
      track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries &&
      state->sample == state->next_sync_sample;
   if(state->keyframe)
      state->next_sync_sample = 0;

   /* Try to batch several samples together if requested. We'll always stop at the chunk boundary */
   if(track_module->samples_batch_size)
   {
      uint32_t size = state->sample_size;
      while(state->samples_in_chunk && size < track_module->samples_batch_size)
      {
         if(mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSZ, 1 )) break;

         if(!state->sample_duration_count)
            if(mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STTS, 1 )) break;

         state->sample_duration_count--;
         state->duration += state->sample_duration;

         size += state->sample_size;
         state->samples_in_chunk--;
         state->sample++;
      }
      state->sample_size = size;
   }

#ifdef ENABLE_MP4_READER_LOG_STATE
   mp4_log_state(p_ctx, state);
#endif

 error:
   return state->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_read_sample_data( VC_CONTAINER_T *p_ctx, uint32_t track,
   MP4_READER_STATE_T *state, uint8_t *data, unsigned int *data_size )
{
   VC_CONTAINER_STATUS_T status;
   unsigned int size = state->sample_size - state->sample_offset;

   if(state->status != VC_CONTAINER_SUCCESS) return state->status;

   if(data_size && *data_size < size) size = *data_size;

   if(data)
   {
      state->status = SEEK(p_ctx, state->offset + state->sample_offset);
      if(state->status != VC_CONTAINER_SUCCESS) return state->status;

      size = READ_BYTES(p_ctx, data, size);
   }
   state->sample_offset += size;

   if(data_size) *data_size = size;
   state->status = STREAM_STATUS(p_ctx);
   if(state->status != VC_CONTAINER_SUCCESS) return state->status;

   status = state->status;

   /* Switch to the start of the next sample */
   if(state->sample_offset >= state->sample_size)
      mp4_read_sample_header(p_ctx, track, state);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_reader_read( VC_CONTAINER_T *p_ctx,
                                              VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_TRACK_MODULE_T *track_module;
   VC_CONTAINER_STATUS_T status;
   MP4_READER_STATE_T *state;
   uint32_t i, track;
   unsigned int data_size;
   uint8_t *data = 0;
   int64_t offset;

   /* Select the track to read from. If no specific track is requested by the caller, this
    * will be the track to which the next bit of data in the mdat belongs to */
   if(!(flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK))
   {
      for(i = 0, track = 0, offset = -1; i < p_ctx->tracks_num; i++)
      {
         track_module = p_ctx->tracks[i]->priv->module;

         /* Ignore tracks which have no more readable data */
         if(track_module->state.status != VC_CONTAINER_SUCCESS) continue;

         if(offset >= 0 && track_module->state.offset >= offset) continue;
         offset = track_module->state.offset;
         track = i;
      }
   }
   else track = packet->track;

   if(track >= p_ctx->tracks_num) return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   track_module = p_ctx->tracks[track]->priv->module;
   state = &track_module->state;

   status = mp4_read_sample_header(p_ctx, track, state);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(!packet) /* Skip packet */
      return mp4_read_sample_data(p_ctx, track, state, 0, 0);

   packet->dts = state->dts;
   packet->pts = state->pts;
   packet->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END;
   if(state->keyframe) packet->flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;
   if(!state->sample_offset) packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   packet->track = track;
   packet->frame_size = state->sample_size;
   packet->size = state->sample_size - state->sample_offset;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
      return mp4_read_sample_data(p_ctx, track, state, 0, 0);
   else if((flags & VC_CONTAINER_READ_FLAG_INFO) || !packet->data)
      return VC_CONTAINER_SUCCESS;

   data = packet->data;
   data_size = packet->buffer_size;

   status = mp4_read_sample_data(p_ctx, track, state, data, &data_size);
   if(status != VC_CONTAINER_SUCCESS)
   {
      /* FIXME */
      return status;
   }

   packet->size = data_size;
   if(state->sample_offset) //?
      packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;

   return status;
}

/*****************************************************************************/
static uint32_t mp4_find_sample( VC_CONTAINER_T *p_ctx, uint32_t track,
   MP4_READER_STATE_T *state, int64_t seek_time, VC_CONTAINER_STATUS_T *p_status )
{
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[track]->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t sample = 0, sample_duration_count;
   int64_t sample_duration, seek_time_up = seek_time + 1;
   unsigned int i;
   VC_CONTAINER_PARAM_UNUSED(state);

   seek_time = seek_time * track_module->timescale / 1000000;
   /* We also need to check against the time rounded up to account for
    * rounding errors in the timestamp (because of the timescale conversion) */
   seek_time_up = seek_time_up * track_module->timescale / 1000000;

   status = SEEK(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_STTS].offset);
   if(status != VC_CONTAINER_SUCCESS) goto end;

   /* Find the sample which corresponds to the requested time */
   for(i = 0; i < track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entries; i++)
   {
      sample_duration_count = _READ_U32(p_ctx);
      sample_duration = _READ_U32(p_ctx);
      status = STREAM_STATUS(p_ctx);
      if(status != VC_CONTAINER_SUCCESS) break;

      if(sample_duration_count * sample_duration <= seek_time)
      {
         seek_time -= sample_duration_count * sample_duration;
         seek_time_up -= sample_duration_count * sample_duration;
         sample += sample_duration_count;
         continue;
      }
      if(!sample_duration) break;

      seek_time /= sample_duration;
      seek_time_up /= sample_duration;
      sample += MAX(seek_time, seek_time_up);
      break;
   }

 end:
   if(p_status) *p_status = status;
   return sample;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_seek_track( VC_CONTAINER_T *p_ctx, uint32_t track,
   MP4_READER_STATE_T *state, uint32_t sample )
{
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[track]->priv->module;
   uint32_t chunk = 0, samples;
   unsigned int i;

   memset(state, 0, sizeof(*state));

   /* Find the right chunk */
   for(i = 0, samples = sample; i < track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entries; i++)
   {
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSC, 1 );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;

      if(state->chunks * state->samples_per_chunk <= samples)
      {
         samples -= state->chunks * state->samples_per_chunk;
         chunk += state->chunks;
         continue;
      }

      while(samples >= state->samples_per_chunk)
      {
         samples -= state->samples_per_chunk;
         state->chunks--;
         chunk++;
      }

      state->chunks--;
      break;
   }

   /* Get the offset of the selected chunk */
   state->sample_table[MP4_SAMPLE_TABLE_STCO].entry = chunk;
   state->sample_table[MP4_SAMPLE_TABLE_CO64].entry = chunk;
   state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STCO, 1 );
   if(state->status != VC_CONTAINER_SUCCESS) goto error;

   /* Find the sample offset within the chunk */
   state->sample_table[MP4_SAMPLE_TABLE_STSZ].entry = sample - samples;
   for(i = 0; i < samples; i++)
   {
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSZ, !i );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;
      state->offset += state->sample_size;
      state->samples_in_chunk--;
   }

   /* Get the timestamp */
   for(i = 0, samples = sample; i < track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entries; i++)
   {
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STTS, !i );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;

      if(state->sample_duration_count <= samples)
      {
         samples -= state->sample_duration_count;
         state->duration += state->sample_duration * state->sample_duration_count;
         continue;
      }

      state->sample_duration_count -= samples;
      state->duration += samples * state->sample_duration;
      break;
   }

   /* Find the right place in the sample composition table */
   for(i = 0, samples = sample; i < track_module->sample_table[MP4_SAMPLE_TABLE_CTTS].entries; i++)
   {
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_CTTS, !i );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;

      if(state->sample_composition_count <= samples)
      {
         samples -= state->sample_composition_count;
         continue;
      }

      state->sample_composition_count -= samples;
      break;
   }

   /* Find the right place in the synchronisation table */
   for(i = 0; i < track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries; i++)
   {
      state->status = mp4_read_sample_table( p_ctx, track_module, state, MP4_SAMPLE_TABLE_STSS, !i );
      if(state->status != VC_CONTAINER_SUCCESS) goto error;

      if(state->next_sync_sample >= sample + 1) break;
   }

   state->sample = sample;
   state->sample_size = 0;
   mp4_read_sample_header(p_ctx, track, state);

 error:
   return state->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_reader_seek(VC_CONTAINER_T *p_ctx,
   int64_t *offset, VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module;
   VC_CONTAINER_STATUS_T status;
   uint32_t i, track, sample, prev_sample, next_sample;
   int64_t seek_time = *offset;
   VC_CONTAINER_PARAM_UNUSED(module);
   VC_CONTAINER_PARAM_UNUSED(mode);

   /* Reset the states */
   for(i = 0; i < p_ctx->tracks_num; i++)
      memset(&p_ctx->tracks[i]->priv->module->state, 0, sizeof(p_ctx->tracks[i]->priv->module->state));

   /* Deal with the easy case first */
   if(!*offset)
   {
      /* Initialise tracks */
      for(i = 0; i < p_ctx->tracks_num; i++)
      {
         /* FIXME: we should check we've got at least one success */
        mp4_read_sample_header(p_ctx, i, &p_ctx->tracks[i]->priv->module->state);
      }
      return VC_CONTAINER_SUCCESS;
   }

   /* Find the first enabled video track */
   for(track = 0; track < p_ctx->tracks_num; track++)
      if(p_ctx->tracks[track]->is_enabled &&
         p_ctx->tracks[track]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO) break;
   if(track == p_ctx->tracks_num) goto seek_time_found; /* No video track found */
   track_module = p_ctx->tracks[track]->priv->module;

   /* Find the sample number for the requested time */
   sample = mp4_find_sample( p_ctx, track, &track_module->state, seek_time, &status );
   if(status != VC_CONTAINER_SUCCESS) goto seek_time_found;

   /* Find the closest sync sample */
   status = mp4_seek_sample_table( p_ctx, track_module, &track_module->state, MP4_SAMPLE_TABLE_STSS );
   if(status != VC_CONTAINER_SUCCESS) goto seek_time_found;
   for(i = 0, prev_sample = 0, next_sample = 0;
       i < track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries; i++)
   {
      next_sample = _READ_U32(p_ctx) - 1;
      if(next_sample > sample)
      {
         sample = (flags & VC_CONTAINER_SEEK_FLAG_FORWARD) ? next_sample : prev_sample;
         break;
      }
      prev_sample = next_sample;
   }

   /* Do the seek on this track and use its timestamp as the new seek point */
   status = mp4_seek_track(p_ctx, track, &track_module->state, sample);
   if(status != VC_CONTAINER_SUCCESS) goto seek_time_found;
   seek_time = track_module->state.pts;

 seek_time_found:

   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      uint32_t sample;
      track_module = p_ctx->tracks[i]->priv->module;
      if(track_module->state.offset) continue;
      sample = mp4_find_sample( p_ctx, i, &track_module->state, seek_time, &status );
      if(status != VC_CONTAINER_SUCCESS) return status; //FIXME

      status = mp4_seek_track(p_ctx, i, &track_module->state, sample);
   }

   *offset = seek_time;
   return VC_CONTAINER_SUCCESS;
}

/******************************************************************************
Global function definitions.
******************************************************************************/

VC_CONTAINER_STATUS_T mp4_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   VC_CONTAINER_MODULE_T *module = 0;
   unsigned int i;
   uint8_t h[8];

   /* Check for a known box type to see if we're dealing with mp4 */
   if( PEEK_BYTES(p_ctx, h, 8) != 8 )
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   switch(VC_FOURCC(h[4],h[5],h[6],h[7]))
   {
   case MP4_BOX_TYPE_FTYP:
   case MP4_BOX_TYPE_MDAT:
   case MP4_BOX_TYPE_MOOV:
   case MP4_BOX_TYPE_FREE:
   case MP4_BOX_TYPE_SKIP:
   case MP4_BOX_TYPE_WIDE:
   case MP4_BOX_TYPE_PNOT:
   case MP4_BOX_TYPE_PICT:
   case MP4_BOX_TYPE_UDTA:
   case MP4_BOX_TYPE_UUID:
      break;
   default:
      /* Couldn't recognize the box type. This doesn't look like an mp4. */
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   /*
    *  We are dealing with an MP4 file
    */

   LOG_DEBUG(p_ctx, "using mp4 reader");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;

   while(STREAM_STATUS(p_ctx) == VC_CONTAINER_SUCCESS)
   {
      MP4_BOX_TYPE_T box_type;
      int64_t box_size;

      status = mp4_read_box_header( p_ctx, INT64_C(-1), &box_type, &box_size );
      if(status != VC_CONTAINER_SUCCESS) goto error;

      if(box_type == MP4_BOX_TYPE_MDAT)
      {
         module->data_offset = STREAM_POSITION(p_ctx);
         module->data_size = box_size;
         if(module->found_moov) break; /* We've got everything we want */
      }
      else if(box_type == MP4_BOX_TYPE_MOOV)
         module->found_moov = true;

      status = mp4_read_box_data( p_ctx, box_type, box_size, MP4_BOX_TYPE_ROOT );
      if(status != VC_CONTAINER_SUCCESS) goto error;

      if(module->found_moov && module->data_offset) break; /* We've got everything we want */
   }

   /* Initialise tracks */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      /* FIXME: we should check we've got at least one success */
      status = mp4_read_sample_header(p_ctx, i, &p_ctx->tracks[i]->priv->module->state);
   }

   status = SEEK(p_ctx, module->data_offset);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   p_ctx->priv->pf_close = mp4_reader_close;
   p_ctx->priv->pf_read = mp4_reader_read;
   p_ctx->priv->pf_seek = mp4_reader_seek;

   if(STREAM_SEEKABLE(p_ctx))
      p_ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "mp4: error opening stream");
   if(module) mp4_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open mp4_reader_open
#endif
