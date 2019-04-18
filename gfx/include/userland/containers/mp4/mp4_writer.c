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
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_writer_utils.h"
#include "containers/core/containers_logging.h"
#include "containers/mp4/mp4_common.h"
#undef CONTAINER_HELPER_LOG_INDENT
#define CONTAINER_HELPER_LOG_INDENT(a) (a)->priv->module->box_level

VC_CONTAINER_STATUS_T mp4_writer_open( VC_CONTAINER_T *p_ctx );

/******************************************************************************
Defines.
******************************************************************************/
#define MP4_TRACKS_MAX 16
#define MP4_TIMESCALE 1000

#define MP4_64BITS_TIME 0 /* 0 to disable / 1 to enable */

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   uint32_t fourcc;
   uint32_t samples;
   uint32_t chunks;

   int64_t offset;
   int64_t timestamp;
   int64_t delta_timestamp;
   int64_t samples_in_chunk;
   int64_t samples_in_prev_chunk;
   struct {
      uint32_t entries;
      uint32_t entry_size;
   } sample_table[MP4_SAMPLE_TABLE_NUM];

   int64_t first_pts;
   int64_t last_pts;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   int box_level;
   MP4_BRAND_T brand;

   VC_CONTAINER_TRACK_T *tracks[MP4_TRACKS_MAX];
   bool tracks_add_done;

   VC_CONTAINER_WRITER_EXTRAIO_T null;

   unsigned int current_track;

   unsigned moov_size;
   int64_t mdat_offset;
   int64_t data_offset;

   uint32_t samples;
   VC_CONTAINER_WRITER_EXTRAIO_T temp;
   VC_CONTAINER_PACKET_T sample;
   int64_t sample_offset;
   int64_t prev_sample_dts;

   int64_t duration;
   /**/

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Static functions within this file.
******************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_extended( VC_CONTAINER_T *p_ctx, MP4_BOX_TYPE_T box_type, uint32_t fourcc );
static VC_CONTAINER_STATUS_T mp4_write_box( VC_CONTAINER_T *p_ctx, MP4_BOX_TYPE_T box_type );
static VC_CONTAINER_STATUS_T mp4_write_box_ftyp( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_moov( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_mvhd( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_trak( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_tkhd( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_mdia( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_mdhd( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_hdlr( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_minf( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_vmhd( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_smhd( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_dinf( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_dref( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stbl( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stsd( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stts( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_ctts( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stsc( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stsz( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stco( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_co64( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_stss( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_vide( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_soun( VC_CONTAINER_T *p_ctx );
static VC_CONTAINER_STATUS_T mp4_write_box_esds( VC_CONTAINER_T *p_ctx );

static struct {
  const MP4_BOX_TYPE_T type;
  VC_CONTAINER_STATUS_T (*pf_func)( VC_CONTAINER_T * );
} mp4_box_list[] =
{
   {MP4_BOX_TYPE_FTYP, mp4_write_box_ftyp},
   {MP4_BOX_TYPE_MOOV, mp4_write_box_moov},
   {MP4_BOX_TYPE_MVHD, mp4_write_box_mvhd},
   {MP4_BOX_TYPE_TRAK, mp4_write_box_trak},
   {MP4_BOX_TYPE_TKHD, mp4_write_box_tkhd},
   {MP4_BOX_TYPE_MDIA, mp4_write_box_mdia},
   {MP4_BOX_TYPE_MDHD, mp4_write_box_mdhd},
   {MP4_BOX_TYPE_HDLR, mp4_write_box_hdlr},
   {MP4_BOX_TYPE_MINF, mp4_write_box_minf},
   {MP4_BOX_TYPE_VMHD, mp4_write_box_vmhd},
   {MP4_BOX_TYPE_SMHD, mp4_write_box_smhd},
   {MP4_BOX_TYPE_DINF, mp4_write_box_dinf},
   {MP4_BOX_TYPE_DREF, mp4_write_box_dref},
   {MP4_BOX_TYPE_STBL, mp4_write_box_stbl},
   {MP4_BOX_TYPE_STSD, mp4_write_box_stsd},
   {MP4_BOX_TYPE_STTS, mp4_write_box_stts},
   {MP4_BOX_TYPE_CTTS, mp4_write_box_ctts},
   {MP4_BOX_TYPE_STSC, mp4_write_box_stsc},
   {MP4_BOX_TYPE_STSZ, mp4_write_box_stsz},
   {MP4_BOX_TYPE_STCO, mp4_write_box_stco},
   {MP4_BOX_TYPE_CO64, mp4_write_box_co64},
   {MP4_BOX_TYPE_STSS, mp4_write_box_stss},
   {MP4_BOX_TYPE_VIDE, mp4_write_box_vide},
   {MP4_BOX_TYPE_SOUN, mp4_write_box_soun},
   {MP4_BOX_TYPE_ESDS, mp4_write_box_esds},
   {MP4_BOX_TYPE_UNKNOWN, 0}
};

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_extended( VC_CONTAINER_T *p_ctx, MP4_BOX_TYPE_T type, uint32_t fourcc )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int64_t box_size = 0;
   unsigned int i;

   /* Find out which object we want to write */
   for( i = 0; mp4_box_list[i].type && mp4_box_list[i].type != type; i++ );

   /* Check we found the requested type */
   if(!mp4_box_list[i].type)
   {
      vc_container_assert(0);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   /* We need to find out the size of the object we're going to write it. */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null))
   {
      status = mp4_write_box_extended( p_ctx, type, fourcc );
      box_size = STREAM_POSITION(p_ctx);
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null);
   if(status != VC_CONTAINER_SUCCESS) return status;

   /* Write the object header */
   LOG_FORMAT(p_ctx, "- Box %4.4s, size: %"PRIi64, (const char *)&fourcc, box_size);
   _WRITE_U32(p_ctx, (uint32_t)box_size);
   _WRITE_FOURCC(p_ctx, fourcc);

   module->box_level++;

   /* Call the object specific writing function */
   status = mp4_box_list[i].pf_func(p_ctx);

   module->box_level--;

   if(status != VC_CONTAINER_SUCCESS)
      LOG_DEBUG(p_ctx, "box %4.4s appears to be corrupted", (char *)mp4_box_list[i].type);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box( VC_CONTAINER_T *p_ctx, MP4_BOX_TYPE_T type )
{
   return mp4_write_box_extended( p_ctx, type, type );
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_ftyp( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   WRITE_FOURCC(p_ctx, module->brand, "major_brand");
   WRITE_U32(p_ctx, 512, "minor_version");
   if(module->brand == MP4_BRAND_QT)
   {
      WRITE_FOURCC(p_ctx, MP4_BRAND_QT, "compatible_brands");
      return STREAM_STATUS(p_ctx);
   }

   if(module->brand == MP4_BRAND_SKM2)
      WRITE_FOURCC(p_ctx, MP4_BRAND_SKM2, "compatible_brands");
   WRITE_FOURCC(p_ctx, MP4_BRAND_ISOM, "compatible_brands");
   WRITE_FOURCC(p_ctx, MP4_BRAND_MP42, "compatible_brands");
   WRITE_FOURCC(p_ctx, MP4_BRAND_3GP4, "compatible_brands");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_moov( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_MVHD);
   if(status != VC_CONTAINER_SUCCESS) return status;

   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      module->current_track = i;
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_TRAK);
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_mvhd( VC_CONTAINER_T *p_ctx )
{
   static uint32_t matrix[] = { 0x10000,0,0,0,0x10000,0,0,0,0x40000000 };
   unsigned int version = MP4_64BITS_TIME;
   unsigned int i;

   WRITE_U8(p_ctx,  version, "version");
   WRITE_U24(p_ctx, 0, "flags");

   /**/
   p_ctx->duration = 0;
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = p_ctx->tracks[i];
      VC_CONTAINER_TRACK_MODULE_T *track_module = track->priv->module;
      int64_t track_duration = track_module->last_pts - track_module->first_pts;
      if(track_duration > p_ctx->duration)
         p_ctx->duration = track_duration;
   }

   if(version)
   {
      WRITE_U64(p_ctx, 0, "creation_time");
      WRITE_U64(p_ctx, 0, "modification_time");
      WRITE_U32(p_ctx, MP4_TIMESCALE, "timescale");
      WRITE_U64(p_ctx, p_ctx->duration * MP4_TIMESCALE / 1000000, "duration");
   }
   else
   {
      WRITE_U32(p_ctx, 0, "creation_time");
      WRITE_U32(p_ctx, 0, "modification_time");
      WRITE_U32(p_ctx, MP4_TIMESCALE, "timescale");
      WRITE_U32(p_ctx, p_ctx->duration * MP4_TIMESCALE / 1000000, "duration");
   }

   WRITE_U32(p_ctx, 0x10000, "rate"); /* 1.0 */
   WRITE_U16(p_ctx, 0x100, "volume"); /* full volume */
   WRITE_U16(p_ctx, 0, "reserved");
   for(i = 0; i < 2; i++)
      WRITE_U32(p_ctx, 0, "reserved");
   for(i = 0; i < 9; i++) /* unity matrix */
      WRITE_U32(p_ctx, matrix[i], "matrix");
   for(i = 0; i < 6; i++)
      WRITE_U32(p_ctx, 0, "pre_defined");
   WRITE_U32(p_ctx, p_ctx->tracks_num + 1, "next_track_ID");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_trak( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_TKHD);
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_MDIA);
   if(status != VC_CONTAINER_SUCCESS) return status;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_tkhd( VC_CONTAINER_T *p_ctx )
{
   static uint32_t matrix[] = { 0x10000,0,0,0,0x10000,0,0,0,0x40000000 };
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   unsigned int version = MP4_64BITS_TIME;
   uint32_t i, width = 0, height = 0;

   WRITE_U8(p_ctx,  version, "version");
   WRITE_U24(p_ctx, 0x7, "flags"); /* track enabled */

   if(version)
   {
      WRITE_U64(p_ctx, 0, "creation_time");
      WRITE_U64(p_ctx, 0, "modification_time");
      WRITE_U32(p_ctx, module->current_track + 1, "track_ID");
      WRITE_U32(p_ctx, 0, "reserved");
      WRITE_U64(p_ctx, p_ctx->duration * MP4_TIMESCALE / 1000000, "duration");
   }
   else
   {
      WRITE_U32(p_ctx, 0, "creation_time");
      WRITE_U32(p_ctx, 0, "modification_time");
      WRITE_U32(p_ctx, module->current_track + 1, "track_ID");
      WRITE_U32(p_ctx, 0, "reserved");
      WRITE_U32(p_ctx, p_ctx->duration * MP4_TIMESCALE / 1000000, "duration");
   }

   for(i = 0; i < 2; i++)
      WRITE_U32(p_ctx, 0, "reserved");
   WRITE_U16(p_ctx, 0, "layer");
   WRITE_U16(p_ctx, 0, "alternate_group");
   WRITE_U16(p_ctx, track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO ? 0x100 : 0, "volume");
   WRITE_U16(p_ctx, 0, "reserved");
   for(i = 0; i < 9; i++) /* unity matrix */
      WRITE_U32(p_ctx, matrix[i], "matrix");

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      width = track->format->type->video.width << 16;
      height = track->format->type->video.height << 16;
      if(track->format->type->video.par_num && track->format->type->video.par_den)
         width = width * (uint64_t)track->format->type->video.par_num /
            track->format->type->video.par_den;
   }
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_SUBPICTURE)
   {
      /* FIXME */
   }

   WRITE_U32(p_ctx, width, "width");
   WRITE_U32(p_ctx, height, "height");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_mdia( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_MDHD);
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_HDLR);
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_MINF);
   if(status != VC_CONTAINER_SUCCESS) return status;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_mdhd( VC_CONTAINER_T *p_ctx )
{
   unsigned int version = MP4_64BITS_TIME;

   WRITE_U8(p_ctx,  version, "version");
   WRITE_U24(p_ctx, 0, "flags");

   // FIXME: take a better timescale ??
   if(version)
   {
      WRITE_U64(p_ctx, 0, "creation_time");
      WRITE_U64(p_ctx, 0, "modification_time");
      WRITE_U32(p_ctx, MP4_TIMESCALE, "timescale");
      WRITE_U64(p_ctx, p_ctx->duration * MP4_TIMESCALE / 1000000, "duration");
   }
   else
   {
      WRITE_U32(p_ctx, 0, "creation_time");
      WRITE_U32(p_ctx, 0, "modification_time");
      WRITE_U32(p_ctx, MP4_TIMESCALE, "timescale");
      WRITE_U32(p_ctx, p_ctx->duration * MP4_TIMESCALE / 1000000, "duration");
   }

   WRITE_U16(p_ctx, 0x55c4, "language"); /* ISO-639-2/T language code */
   WRITE_U16(p_ctx, 0, "pre_defined");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_hdlr( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   uint32_t i, handler_size, fourcc = 0;
   const char *handler_name;

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO) fourcc = VC_FOURCC('v','i','d','e');
   if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO) fourcc = VC_FOURCC('s','o','u','n');
   if(track->format->es_type == VC_CONTAINER_ES_TYPE_SUBPICTURE) fourcc = VC_FOURCC('t','e','x','t');

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   if(module->brand == MP4_BRAND_QT)
      WRITE_FOURCC(p_ctx,  VC_FOURCC('m','h','l','r'), "component_type");
   else
      WRITE_U32(p_ctx,  0, "pre-defined");

   WRITE_FOURCC(p_ctx,  fourcc, "handler_type");
   for(i = 0; i < 3; i++)
      WRITE_U32(p_ctx,  0, "reserved");

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   { handler_name = "Video Media Handler"; handler_size = sizeof("Video Media Handler"); }
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
   { handler_name = "Audio Media Handler"; handler_size = sizeof("Audio Media Handler"); }
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_SUBPICTURE)
   { handler_name = "Text Media Handler"; handler_size = sizeof("Text Media Handler"); }
   else { handler_name = ""; handler_size = sizeof(""); }

   if(module->brand == MP4_BRAND_QT)
   { handler_size--; WRITE_U8(p_ctx, handler_size, "string_size"); }

   WRITE_STRING(p_ctx, handler_name, handler_size, "name");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_minf( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_VMHD);
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_SMHD);
#if 0
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_SUBPICTURE)
      /*FIXME */;
#endif
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_DINF);
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STBL);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_vmhd( VC_CONTAINER_T *p_ctx )
{
   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 1, "flags");

   WRITE_U16(p_ctx,  0, "graphicsmode");
   WRITE_U16(p_ctx,  0, "opcolor");
   WRITE_U16(p_ctx,  0, "opcolor");
   WRITE_U16(p_ctx,  0, "opcolor");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_smhd( VC_CONTAINER_T *p_ctx )
{
   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   WRITE_U16(p_ctx,  0, "balance");
   WRITE_U16(p_ctx,  0, "reserved");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_dinf( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_DREF);
   if(status != VC_CONTAINER_SUCCESS) return status;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_dref( VC_CONTAINER_T *p_ctx )
{
   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   WRITE_U32(p_ctx,  1, "entry_count");

   /* Add a URL box */
   WRITE_U32(p_ctx,  12, "box_size");
   WRITE_FOURCC(p_ctx,  VC_FOURCC('u','r','l',' '), "box_type");
   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0x1, "flags");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stbl( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STSD);
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STTS);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if( 0 && track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_CTTS);
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STSC);
   if(status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STSZ);
   if(status != VC_CONTAINER_SUCCESS) return status;

   if(1)
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STCO);
   else
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_CO64);

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
   {
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_STSS);
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stsd( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   WRITE_U32(p_ctx, 1, "entry_count");

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
      status = mp4_write_box_extended(p_ctx, MP4_BOX_TYPE_VIDE, track->priv->module->fourcc);
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      status = mp4_write_box_extended(p_ctx, MP4_BOX_TYPE_SOUN, track->priv->module->fourcc);
#if 0
   else if(track->format->es_type == VC_CONTAINER_ES_TYPE_SUBPICTURE)
      /*FIXME*/;
#endif

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_write_sample_to_temp( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   int32_t dts_diff = packet->dts - module->prev_sample_dts;
   uint8_t keyframe = (packet->flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ? 0x80 : 0;

   vc_container_io_write_be_uint32(module->temp.io, packet->size);
   vc_container_io_write_be_uint32(module->temp.io, dts_diff);
   vc_container_io_write_be_uint24(module->temp.io, (uint32_t)(packet->pts - packet->dts));
   vc_container_io_write_uint8(module->temp.io, packet->track | keyframe);
   module->prev_sample_dts = packet->dts;
   return module->temp.io->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_read_sample_from_temp( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   packet->size = vc_container_io_read_be_uint32(module->temp.io);
   packet->dts += (int32_t)vc_container_io_read_be_uint32(module->temp.io);
   packet->pts = packet->dts + vc_container_io_read_be_uint24(module->temp.io);
   packet->track = vc_container_io_read_uint8(module->temp.io);
   packet->flags = (packet->track & 0x80) ? VC_CONTAINER_PACKET_FLAG_KEYFRAME : 0;
   packet->track &= 0x7F;
   return module->temp.io->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stts( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PACKET_T sample;
   unsigned int entries = 0;
   int64_t last_dts = 0, delta;

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entries, "entry_count");

   if(module->null.refcount)
   {
      /* We're not actually writing the data, we just want the size */
      WRITE_BYTES(p_ctx, 0, track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entries * 8);
      return STREAM_STATUS(p_ctx);
   }

   /* Go through all the samples written */
   vc_container_io_seek(module->temp.io, INT64_C(0));
   sample.dts = 0;

   status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   while(status == VC_CONTAINER_SUCCESS)
   {
      if(sample.track != module->current_track) goto skip;

      delta = sample.dts * MP4_TIMESCALE / 1000000 - last_dts;
      if(delta < 0) delta = 0;
      WRITE_U32(p_ctx, 1, "sample_count");
      WRITE_U32(p_ctx, delta, "sample_delta");
      entries++;
      last_dts += delta;

     skip:
      status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   }
   vc_container_assert(entries == track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entries);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_ctts( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");
   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_CTTS].entries, "entry_count");
   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stsc( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PACKET_T sample;
   int64_t offset = 0, track_offset = -1;
   unsigned int entries = 0, chunks = 0, first_chunk = 0, samples_in_chunk = 0;

   memset(&sample, 0, sizeof(VC_CONTAINER_PACKET_T));

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");
   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entries, "entry_count");

   if(module->null.refcount)
   {
      /* We're not actually writing the data, we just want the size */
      WRITE_BYTES(p_ctx, 0, track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entries * 12);
      return STREAM_STATUS(p_ctx);
   }

   /* Go through all the samples written */
   vc_container_io_seek(module->temp.io, INT64_C(0));

   status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   while(status == VC_CONTAINER_SUCCESS)
   {
      if(sample.track != module->current_track) goto skip;

      /* Is it a new chunk ? */
      if(track_offset != offset)
      {
         chunks++;
         if(samples_in_chunk)
         {
            WRITE_U32(p_ctx,  first_chunk, "first_chunk");
            WRITE_U32(p_ctx,  samples_in_chunk, "samples_per_chunk");
            WRITE_U32(p_ctx,  1, "sample_description_index");
            entries++;
         }
         first_chunk = chunks;
         samples_in_chunk = 0;
      }
      track_offset = offset + sample.size;
      samples_in_chunk++;

     skip:
      offset += sample.size;
      status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   }

   if(samples_in_chunk)
   {
      WRITE_U32(p_ctx,  first_chunk, "first_chunk");
      WRITE_U32(p_ctx,  samples_in_chunk, "samples_per_chunk");
      WRITE_U32(p_ctx,  1, "sample_description_index");
      entries++;
   }

   vc_container_assert(entries == track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entries);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stsz( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PACKET_T sample;
   unsigned int entries = 0;

   memset(&sample, 0, sizeof(VC_CONTAINER_PACKET_T));

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   WRITE_U32(p_ctx, 0, "sample_size");
   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_STSZ].entries, "sample_count");

   if(module->null.refcount)
   {
      /* We're not actually writing the data, we just want the size */
      WRITE_BYTES(p_ctx, 0, track_module->sample_table[MP4_SAMPLE_TABLE_STSZ].entries * 4);
      return STREAM_STATUS(p_ctx);
   }

   /* Go through all the samples written */
   vc_container_io_seek(module->temp.io, INT64_C(0));

   status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   while(status == VC_CONTAINER_SUCCESS)
   {
      if(sample.track != module->current_track) goto skip;

      WRITE_U32(p_ctx, sample.size, "entry_size");
      entries++;

     skip:
      status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   }
   vc_container_assert(entries == track_module->sample_table[MP4_SAMPLE_TABLE_STSZ].entries);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stco( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PACKET_T sample;
   int64_t offset = module->data_offset, track_offset = -1;
   unsigned int entries = 0;

   memset(&sample, 0, sizeof(VC_CONTAINER_PACKET_T));

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");
   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_STCO].entries, "entry_count");

   if(module->null.refcount)
   {
      /* We're not actually writing the data, we just want the size */
      WRITE_BYTES(p_ctx, 0, track_module->sample_table[MP4_SAMPLE_TABLE_STCO].entries * 4);
      return STREAM_STATUS(p_ctx);
   }

   /* Go through all the samples written */
   vc_container_io_seek(module->temp.io, INT64_C(0));

   status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   while(status == VC_CONTAINER_SUCCESS)
   {
      if(sample.track != module->current_track) goto skip;

      /* Is it a new chunk ? */
      if(track_offset != offset)
      {
         WRITE_U32(p_ctx, offset, "chunk_offset");
         entries++;
      }
      track_offset = offset + sample.size;

     skip:
      offset += sample.size;
      status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   }
   vc_container_assert(entries == track_module->sample_table[MP4_SAMPLE_TABLE_STCO].entries);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_co64( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");
   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_CO64].entries, "entry_count");

   if(module->null.refcount)
   {
      /* We're not actually writing the data, we just want the size */
      WRITE_BYTES(p_ctx, 0, track_module->sample_table[MP4_SAMPLE_TABLE_CO64].entries * 8);
      return STREAM_STATUS(p_ctx);
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_stss( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module = p_ctx->tracks[module->current_track]->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PACKET_T sample;
   unsigned int entries = 0, samples = 0;

   memset(&sample, 0, sizeof(VC_CONTAINER_PACKET_T));

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");
   WRITE_U32(p_ctx, track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries, "entry_count");

   if(module->null.refcount)
   {
      /* We're not actually writing the data, we just want the size */
      WRITE_BYTES(p_ctx, 0, track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries * 4);
      return STREAM_STATUS(p_ctx);
   }

   /* Go through all the samples written */
   vc_container_io_seek(module->temp.io, INT64_C(0));

   status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   while(status == VC_CONTAINER_SUCCESS)
   {
      if(sample.track != module->current_track) goto skip;

      samples++;
      if(sample.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME)
      {
         WRITE_U32(p_ctx, samples, "sample_number");
         entries++;
      }

     skip:
      status = mp4_writer_read_sample_from_temp(p_ctx, &sample);
   }
   vc_container_assert(entries == track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_vide_avcC( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];

   WRITE_U32(p_ctx, track->format->extradata_size + 8, "size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('a','v','c','C'), "type");
   WRITE_BYTES(p_ctx, track->format->extradata, track->format->extradata_size);

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_vide_d263( VC_CONTAINER_T *p_ctx )
{
   WRITE_U32(p_ctx, 8 + 7, "size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('d','2','6','3'), "type");
   WRITE_FOURCC(p_ctx, VC_FOURCC('B','R','C','M'), "vendor");
   WRITE_U8(p_ctx, 0, "version");
   WRITE_U8(p_ctx, 10, "level");
   WRITE_U8(p_ctx, 0, "profile");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_vide( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   unsigned int i;

   for(i = 0; i < 6; i++) WRITE_U8(p_ctx, 0, "reserved");
   WRITE_U16(p_ctx, 1, "data_reference_index");

   WRITE_U16(p_ctx, 0, "pre_defined");
   WRITE_U16(p_ctx, 0, "reserved");
   for(i = 0; i < 3; i++) WRITE_U32(p_ctx, 0, "pre_defined");
   WRITE_U16(p_ctx, track->format->type->video.width, "width");
   WRITE_U16(p_ctx, track->format->type->video.height, "height");
   WRITE_U32(p_ctx, 0x480000, "horizresolution"); /* 72 dpi */
   WRITE_U32(p_ctx, 0x480000, "vertresolution"); /* 72 dpi */
   WRITE_U32(p_ctx, 0, "reserved");
   WRITE_U16(p_ctx, 1, "frame_count");
   for(i = 0; i < 32; i++) _WRITE_U8(p_ctx, 0);
   WRITE_U16(p_ctx, 0x18, "depth");
   WRITE_U16(p_ctx, -1, "pre_defined");

   switch(track->format->codec)
   {
   case VC_CONTAINER_CODEC_H264: return mp4_write_box_vide_avcC(p_ctx);
   case VC_CONTAINER_CODEC_H263: return mp4_write_box_vide_d263(p_ctx);
   case VC_CONTAINER_CODEC_MP4V: return mp4_write_box(p_ctx, MP4_BOX_TYPE_ESDS);
   default: break;
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_soun_damr( VC_CONTAINER_T *p_ctx )
{
   WRITE_U32(p_ctx, 8 + 8, "size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('d','a','m','r'), "type");
   WRITE_FOURCC(p_ctx, VC_FOURCC('B','R','C','M'), "vendor");
   WRITE_U8(p_ctx, 0, "version");
   WRITE_U8(p_ctx, 0x80, "mode_set");
   WRITE_U8(p_ctx, 0, "mode_change_period");
   WRITE_U8(p_ctx, 1, "frame_per_second");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_soun_dawp( VC_CONTAINER_T *p_ctx )
{
   WRITE_U32(p_ctx, 8 + 5, "size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('d','a','w','p'), "type");
   WRITE_FOURCC(p_ctx, VC_FOURCC('B','R','C','M'), "vendor");
   WRITE_U8(p_ctx, 0, "version");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_soun_devc( VC_CONTAINER_T *p_ctx )
{
   WRITE_U32(p_ctx, 8 + 6, "size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('d','e','v','c'), "type");
   WRITE_FOURCC(p_ctx, VC_FOURCC('B','R','C','M'), "vendor");
   WRITE_U8(p_ctx, 0, "version");
   WRITE_U8(p_ctx, 1, "samples_per_frame");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_soun( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   unsigned int i, version = 0;

   for(i = 0; i < 6; i++) WRITE_U8(p_ctx, 0, "reserved");
   WRITE_U16(p_ctx, 1, "data_reference_index");

   if(module->brand == MP4_BRAND_QT)
   {
      if(track->format->codec == VC_CONTAINER_CODEC_MP4A) version = 1;
      WRITE_U16(p_ctx, version, "version");
      WRITE_U16(p_ctx, 0, "revision_level");
      WRITE_U32(p_ctx, 0, "vendor");
   }
   else
   {
      for(i = 0; i < 2; i++) WRITE_U32(p_ctx, 0, "reserved");
   }

   WRITE_U16(p_ctx, track->format->type->audio.channels, "channelcount");
   WRITE_U16(p_ctx, 0, "samplesize");
   WRITE_U16(p_ctx, 0, "pre_defined");
   WRITE_U16(p_ctx, 0, "reserved");
   WRITE_U32(p_ctx, track->format->type->audio.sample_rate << 16, "samplerate");

   if(module->brand == MP4_BRAND_QT && version == 1) /* FIXME */
   {
      WRITE_U32(p_ctx, 1024, "samples_per_packet");
      WRITE_U32(p_ctx, 1536, "bytes_per_packet");
      WRITE_U32(p_ctx, 2, "bytes_per_frame");
      WRITE_U32(p_ctx, 2, "bytes_per_sample");
   }

   switch(track->format->codec)
   {
   case VC_CONTAINER_CODEC_AMRNB:
   case VC_CONTAINER_CODEC_AMRWB:
      return mp4_write_box_soun_damr(p_ctx);
   case VC_CONTAINER_CODEC_AMRWBP:
      return mp4_write_box_soun_dawp(p_ctx);
   case VC_CONTAINER_CODEC_EVRC:
      return mp4_write_box_soun_devc(p_ctx);
   case VC_CONTAINER_CODEC_MP4A:
   case VC_CONTAINER_CODEC_MPGA:
      return mp4_write_box(p_ctx, MP4_BOX_TYPE_ESDS);
   default: break;
   }

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_write_box_esds( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[module->current_track];
   unsigned int decoder_specific_size = 0, decoder_config_size, sl_size;
   unsigned int stream_type, object_type;

#define MP4_GET_DESCRIPTOR_SIZE(size) \
   ((size) < 0x0080) ? 2 + (size) : ((size) < 0x4000) ? 3 + (size) : 4 + (size)
#define MP4_WRITE_DESCRIPTOR_HEADER(type, size) \
   LOG_FORMAT(p_ctx, "descriptor %x, size %i", type, size); _WRITE_U8(p_ctx, type); \
   if((size) >= 0x4000) _WRITE_U8(p_ctx, (((size) >> 14) & 0x7F) | 0x80); \
   if((size) >= 0x80  ) _WRITE_U8(p_ctx, (((size) >> 7 ) & 0x7F) | 0x80); \
   _WRITE_U8(p_ctx, (size) & 0x7F)

   /* We only support small size descriptors */
   if(track->format->extradata_size > 0x200000 - 100)
      return VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED;

   switch(track->format->es_type)
   {
   case VC_CONTAINER_ES_TYPE_VIDEO: stream_type = 0x4; break;
   case VC_CONTAINER_ES_TYPE_AUDIO: stream_type = 0x5; break;
   case VC_CONTAINER_ES_TYPE_SUBPICTURE: stream_type = 0x20; break;
   default: return VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED;
   }
   switch(track->format->codec)
   {
   case VC_CONTAINER_CODEC_MP4V: object_type = 0x20; break;
   case VC_CONTAINER_CODEC_MP1V: object_type = 0x6B; break;
   case VC_CONTAINER_CODEC_MP2V: object_type = 0x60; break;
   case VC_CONTAINER_CODEC_JPEG: object_type = 0x6C; break;
   case VC_CONTAINER_CODEC_MP4A: object_type = 0x40; break;
   case VC_CONTAINER_CODEC_MPGA:
      object_type = track->format->type->audio.sample_rate < 32000 ? 0x69 : 0x6B; break;
   default: return VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED;
   }

   decoder_specific_size = MP4_GET_DESCRIPTOR_SIZE(track->format->extradata_size);
   decoder_config_size = MP4_GET_DESCRIPTOR_SIZE(13 + decoder_specific_size);
   sl_size = MP4_GET_DESCRIPTOR_SIZE(1);

   WRITE_U8(p_ctx,  0, "version");
   WRITE_U24(p_ctx, 0, "flags");

   /* Write the ES descriptor */
   MP4_WRITE_DESCRIPTOR_HEADER(0x3, 3 + decoder_config_size + sl_size);
   WRITE_U16(p_ctx, module->current_track + 1, "es_id");
   WRITE_U8(p_ctx, 0x1f, "flags"); /* stream_priority = 0x1f */

   /* Write the Decoder Config descriptor */
   MP4_WRITE_DESCRIPTOR_HEADER(0x4, 13 + decoder_specific_size);
   WRITE_U8(p_ctx, object_type, "object_type_indication");
   WRITE_U8(p_ctx, (stream_type << 2) | 1, "stream_type");
   WRITE_U24(p_ctx, 8000, "buffer_size_db");
   WRITE_U32(p_ctx, track->format->bitrate, "max_bitrate");
   WRITE_U32(p_ctx, track->format->bitrate, "avg_bitrate");
   if(track->format->extradata_size)
   {
      MP4_WRITE_DESCRIPTOR_HEADER(0x5, track->format->extradata_size);
      WRITE_BYTES(p_ctx, track->format->extradata, track->format->extradata_size);
   }

   /* Write the SL descriptor */
   MP4_WRITE_DESCRIPTOR_HEADER(0x6, 1);
   WRITE_U8(p_ctx, 0x2, "flags");

   return STREAM_STATUS(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   int64_t mdat_size;

   mdat_size = STREAM_POSITION(p_ctx) - module->mdat_offset;

   /* Write the moov box */
   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_MOOV);

   /* Finalise the mdat box */
   SEEK(p_ctx, module->mdat_offset);
   WRITE_U32(p_ctx, (uint32_t)mdat_size, "mdat size" );

   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);

   vc_container_writer_extraio_delete(p_ctx, &module->temp);
   vc_container_writer_extraio_delete(p_ctx, &module->null);
   free(module);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_add_track( VC_CONTAINER_T *p_ctx, VC_CONTAINER_ES_FORMAT_T *format )
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_TRACK_T *track;
   uint32_t type = 0;

   if(!(format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   /* Check we support this format */
   switch(format->codec)
   {
   case VC_CONTAINER_CODEC_AMRNB:  type = VC_FOURCC('s','a','m','r'); break;
   case VC_CONTAINER_CODEC_AMRWB:  type = VC_FOURCC('s','a','w','b'); break;
   case VC_CONTAINER_CODEC_AMRWBP: type = VC_FOURCC('s','a','w','p'); break;
   case VC_CONTAINER_CODEC_EVRC:   type = VC_FOURCC('s','e','v','c'); break;
   case VC_CONTAINER_CODEC_MP4A:   type = VC_FOURCC('m','p','4','a'); break;
   case VC_CONTAINER_CODEC_MPGA:   type = VC_FOURCC('m','p','4','a'); break;

   case VC_CONTAINER_CODEC_MP4V:   type = VC_FOURCC('m','p','4','v'); break;
   case VC_CONTAINER_CODEC_JPEG:   type = VC_FOURCC('m','p','4','v'); break;
   case VC_CONTAINER_CODEC_H263:   type = VC_FOURCC('s','2','6','3'); break;
   case VC_CONTAINER_CODEC_H264:
      if(format->codec_variant == VC_FOURCC('a','v','c','C')) type = VC_FOURCC('a','v','c','1'); break;
   case VC_CONTAINER_CODEC_MJPEG:  type = VC_FOURCC('j','p','e','g'); break;
   case VC_CONTAINER_CODEC_MJPEGA: type = VC_FOURCC('m','j','p','a'); break;
   case VC_CONTAINER_CODEC_MJPEGB: type = VC_FOURCC('m','j','p','b'); break;
   case VC_CONTAINER_CODEC_MP1V:   type = VC_FOURCC('m','p','e','g'); break;
   case VC_CONTAINER_CODEC_MP2V:   type = VC_FOURCC('m','p','e','g'); break;

   default: type = 0; break;
   }

   if(!type) return VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED;

   /* Allocate and initialise track data */
   if(p_ctx->tracks_num >= MP4_TRACKS_MAX) return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   p_ctx->tracks[p_ctx->tracks_num] = track =
      vc_container_allocate_track(p_ctx, sizeof(*p_ctx->tracks[0]->priv->module));
   if(!track) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   if(format->extradata_size)
   {
      status = vc_container_track_allocate_extradata( p_ctx, track, format->extradata_size );
      if(status) goto error;
   }

   vc_container_format_copy(track->format, format, format->extradata_size);
   track->priv->module->fourcc = type;
   track->priv->module->offset = -1;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STTS].entry_size = 8;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STSZ].entry_size = 4;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STSC].entry_size = 12;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STCO].entry_size = 4;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_STSS].entry_size = 4;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_CO64].entry_size = 8;
   track->priv->module->sample_table[MP4_SAMPLE_TABLE_CTTS].entry_size = 8;

   p_ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;

 error:
   vc_container_free_track(p_ctx, track);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_add_track_done( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   if(module->tracks_add_done) return status;

   /* We need to find out the size of the object we're going to write it. */
   if(!vc_container_writer_extraio_enable(p_ctx, &module->null))
   {
      status = mp4_write_box(p_ctx, MP4_BOX_TYPE_MOOV);
      module->moov_size = STREAM_POSITION(p_ctx);
      p_ctx->size = module->moov_size;
   }
   vc_container_writer_extraio_disable(p_ctx, &module->null);

   if(status == VC_CONTAINER_SUCCESS) module->tracks_add_done = true;
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_control( VC_CONTAINER_T *p_ctx, VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   switch(operation)
   {
   case VC_CONTAINER_CONTROL_TRACK_ADD:
      {
         VC_CONTAINER_ES_FORMAT_T *p_format =
            (VC_CONTAINER_ES_FORMAT_T *)va_arg( args, VC_CONTAINER_ES_FORMAT_T * );
         if(module->tracks_add_done) return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
         return mp4_writer_add_track(p_ctx, p_format);
      }

   case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      return mp4_writer_add_track_done(p_ctx);

   default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_add_sample( VC_CONTAINER_T *p_ctx,
                                                    VC_CONTAINER_PACKET_T *packet )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = p_ctx->tracks[packet->track];
   VC_CONTAINER_TRACK_MODULE_T *track_module = track->priv->module;

   track_module->last_pts = packet->pts;
   if(!track_module->samples) track_module->first_pts = packet->pts;

   track_module->samples++;
   track_module->sample_table[MP4_SAMPLE_TABLE_STSZ].entries++; /* sample size */
   p_ctx->size += track_module->sample_table[MP4_SAMPLE_TABLE_STSZ].entry_size;
   track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entries++; /* time to sample */
   p_ctx->size += track_module->sample_table[MP4_SAMPLE_TABLE_STTS].entry_size;

   #if 0
   delta_ts = packet->dts - track_module->timestamp;
   track_module->timestamp = packet->dts;
   if(!track_module->samples) track_module->delta_ts =
   if()
#endif

   /* Is it a new chunk ? */
   if(module->sample_offset != track_module->offset)
   {
      track_module->chunks++;
      track_module->sample_table[MP4_SAMPLE_TABLE_STCO].entries++; /* chunk offset */
      p_ctx->size += track_module->sample_table[MP4_SAMPLE_TABLE_STCO].entry_size;
      track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entries++; /* sample to chunk */
      p_ctx->size += track_module->sample_table[MP4_SAMPLE_TABLE_STSC].entry_size;
   }
   track_module->offset = module->sample_offset + packet->size;

   if(track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO &&
      (packet->flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME))
   {
      track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entries++; /* sync sample */
      p_ctx->size += track_module->sample_table[MP4_SAMPLE_TABLE_STSS].entry_size;
   }

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mp4_writer_write( VC_CONTAINER_T *p_ctx,
                                               VC_CONTAINER_PACKET_T *packet )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PACKET_T *sample = &module->sample;
   VC_CONTAINER_STATUS_T status;

   if(!module->tracks_add_done)
   {
      status = mp4_writer_add_track_done(p_ctx);
      if(status != VC_CONTAINER_SUCCESS) return status;
   }

   if(packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_START)
      ++module->samples; /* Switching to a new sample */

   if(packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_START)
   {
      module->sample_offset = STREAM_POSITION(p_ctx);
      sample->size = packet->size;
      sample->pts = packet->pts;
      sample->dts = packet->pts;
      sample->track = packet->track;
      sample->flags = packet->flags;
   }
   else
   {
      sample->size += packet->size;
      sample->flags |= packet->flags;
   }

   if(WRITE_BYTES(p_ctx, packet->data, packet->size) != packet->size)
      return STREAM_STATUS(p_ctx); // TODO do something
   p_ctx->size += packet->size;

   //
   if(packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_END)
   {
      status = mp4_writer_write_sample_to_temp(p_ctx, sample);
      status = mp4_writer_add_sample(p_ctx, sample);
   }

   return VC_CONTAINER_SUCCESS;
}

/******************************************************************************
Global function definitions.
******************************************************************************/

VC_CONTAINER_STATUS_T mp4_writer_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   const char *extension = vc_uri_path_extension(p_ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   MP4_BRAND_T brand;

   /* Check if the user has specified a container */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "3gp") && strcasecmp(extension, "skm") &&
      strcasecmp(extension, "mov") && strcasecmp(extension, "mp4") &&
      strcasecmp(extension, "m4v") && strcasecmp(extension, "m4a"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;

   /* Find out which brand we're going write */
   if(!strcasecmp(extension, "3gp")) brand = MP4_BRAND_3GP5;
   else if(!strcasecmp(extension, "skm")) brand = MP4_BRAND_SKM2;
   else if(!strcasecmp(extension, "mov")) brand = MP4_BRAND_QT;
   else brand = MP4_BRAND_ISOM;
   module->brand = brand;

   /* Create a null i/o writer to help us out in writing our data */
   status = vc_container_writer_extraio_create_null(p_ctx, &module->null);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   /* Create a temporary i/o writer to help us out in writing our data */
   status = vc_container_writer_extraio_create_temp(p_ctx, &module->temp);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   status = mp4_write_box(p_ctx, MP4_BOX_TYPE_FTYP);
   if(status != VC_CONTAINER_SUCCESS) goto error;

   /* Start the mdat box */
   module->mdat_offset = STREAM_POSITION(p_ctx);
   WRITE_U32(p_ctx, 0, "size");
   WRITE_FOURCC(p_ctx, VC_FOURCC('m','d','a','t'), "type");
   module->data_offset = STREAM_POSITION(p_ctx);

   p_ctx->priv->pf_close = mp4_writer_close;
   p_ctx->priv->pf_write = mp4_writer_write;
   p_ctx->priv->pf_control = mp4_writer_control;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "mp4: error opening stream");
   if(module)
   {
      if(module->null.io) vc_container_writer_extraio_delete(p_ctx, &module->null);
      free(module);
   }
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open mp4_writer_open
#endif
