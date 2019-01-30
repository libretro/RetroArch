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
#include "containers/core/containers_bits.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"
#undef CONTAINER_HELPER_LOG_INDENT
#define CONTAINER_HELPER_LOG_INDENT(a) (2*(a)->priv->module->level)

/******************************************************************************
Defines.
******************************************************************************/
#define PS_TRACKS_MAX 2
#define PS_EXTRADATA_MAX 256

#define PS_SYNC_FAIL_MAX 65536 /** Maximum number of byte-wise sync attempts,
                                   should be enough to stride at least one
                                   PES packet (length encoded using 16 bits). */

/** Maximum number of pack/packet start codes scanned when searching for tracks
    at open time or when resyncing. */
#define PS_PACK_SCAN_MAX 128

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   /** Coding and elementary stream id of the track */
   uint32_t stream_id;

   /** Sub-stream id (for private_stream_1 only) */
   uint32_t substream_id;
      
   /** PES packet payload offset (for private_stream_1) */
   unsigned int payload_offset;
   
   uint8_t extradata[PS_EXTRADATA_MAX];

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   /** Logging indentation level */
   uint32_t level;
   
   /** Track data */
   int tracks_num;
   VC_CONTAINER_TRACK_T *tracks[PS_TRACKS_MAX];

   /** State flag denoting whether or not we are searching
       for tracks (at open time) */
   bool searching_tracks;

   /** Size of program stream data (if known) */
   uint64_t data_size;

   /** Offset to the first pack or PES packet start code we've seen */
   uint64_t data_offset;

   /** The first system_clock_reference value we've seen, in (27MHz ticks) */
   int64_t scr_offset;
   
   /** Most recent system_clock_reference value we've seen, in (27MHz ticks) */
   int64_t scr;

   /** Global offset we add to PES timestamps to make them zero based and 
       to work around discontinuity in the system_clock_reference */
   int64_t scr_bias;

   /** Most recent program stream mux rate (in units of 50 bytes/second). */
   uint32_t mux_rate;

   /** Offset to the most recent pack start code we've seen */
   uint64_t pack_offset;
   
   /** Program stream mux rate is often incorrect or fixed to 25200 (10.08 
       Mbit/s) which yields inaccurate duration estimate for most files. We
       maintain a moving average data rate (in units of bytes/second) based 
       on the system_clock_reference to give better estimates. */
   int64_t data_rate;

   /** Offset to the most recent PES packet start code prefix we've seen */
   unsigned int packet_data_size;
   unsigned int packet_data_left;
   int64_t packet_pts;
   int64_t packet_dts;
   int packet_track;
   
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/

VC_CONTAINER_STATUS_T ps_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Prototypes for local functions
******************************************************************************/

/******************************************************************************
Local Functions
******************************************************************************/

/** Find the track associated with a PS stream_id */
static VC_CONTAINER_TRACK_T *ps_find_track( VC_CONTAINER_T *ctx, uint32_t stream_id,
   uint32_t substream_id, bool b_create )
{
   VC_CONTAINER_TRACK_T *track = 0;
   unsigned int i;

   for(i = 0; i < ctx->tracks_num; i++)
      if(ctx->tracks[i]->priv->module->stream_id == stream_id && 
         ctx->tracks[i]->priv->module->substream_id == substream_id) break;

   if(i < ctx->tracks_num) /* We found it */
      track = ctx->tracks[i];

   if(!track && b_create && i < PS_TRACKS_MAX)
   {
      /* Allocate and initialise a new track */
      ctx->tracks[i] = track =
         vc_container_allocate_track(ctx, sizeof(*ctx->tracks[0]->priv->module));
      if(track)
      {
         track->priv->module->stream_id = stream_id;
         track->priv->module->substream_id = substream_id;
         ctx->tracks_num++;
      }
   }

   if(!track && b_create)
      LOG_DEBUG(ctx, "could not create track for stream id: %i", stream_id);

   return track;
}

/*****************************************************************************/
STATIC_INLINE VC_CONTAINER_STATUS_T ps_find_start_code( VC_CONTAINER_T *ctx, uint8_t *buffer )
{
   unsigned int i;

   /* Scan for a pack or PES packet start code prefix */
   for (i = 0; i < PS_SYNC_FAIL_MAX; ++i)
   {
      if(PEEK_BYTES(ctx, buffer, 4) < 4)
         return VC_CONTAINER_ERROR_EOS;

      if(buffer[0] == 0x0 && buffer[1] == 0x0 && buffer[2] == 0x1 && buffer[3] >= 0xB9) 
         break;

      if (SKIP_BYTES(ctx, 1) != 1)
         return VC_CONTAINER_ERROR_EOS;
   }

   if(i == PS_SYNC_FAIL_MAX) /* We didn't find a valid pack or PES packet */
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if (buffer[3] == 0xB9) /* MPEG_program_end_code */
      return VC_CONTAINER_ERROR_EOS;
      
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_system_header( VC_CONTAINER_T *ctx )
{
   uint8_t header[8];
   uint32_t length;
   VC_CONTAINER_BITS_T bits;

   if(_READ_U32(ctx) != 0x1BB) return VC_CONTAINER_ERROR_CORRUPTED;
   LOG_FORMAT(ctx, "system_header");
   ctx->priv->module->level++;

   length = READ_U16(ctx, "header_length");
   if(length < 6) return VC_CONTAINER_ERROR_CORRUPTED;
   if(READ_BYTES(ctx, header, 6) != 6) return VC_CONTAINER_ERROR_EOS;

   BITS_INIT(ctx, &bits, header, 6);

   if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
   BITS_SKIP(ctx, &bits, 22, "rate_bound");
   if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
   BITS_SKIP(ctx, &bits, 6, "audio_bound");
   BITS_SKIP(ctx, &bits, 1, "fixed_flag");
   BITS_SKIP(ctx, &bits, 1, "CSPS_flag");
   BITS_SKIP(ctx, &bits, 1, "system_audio_lock_flag");
   BITS_SKIP(ctx, &bits, 1, "system_video_lock_flag");
   if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
   BITS_SKIP(ctx, &bits, 5, "video_bound");
   BITS_SKIP(ctx, &bits, 1, "packet_rate_restriction_flag");
   BITS_SKIP(ctx, &bits, 7, "reserved_bits");
   length -= 6;

   while(length >= 3 && (PEEK_U8(ctx) & 0x80))
   {
      SKIP_U8(ctx, "stream_id");
      SKIP_BYTES(ctx, 2);
      length -= 3;
   }
   SKIP_BYTES(ctx, length);

   ctx->priv->module->level--;
   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_pack_header( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   uint8_t header[10];
   int64_t scr, scr_base, scr_ext = INT64_C(0);
   uint64_t pack_offset = STREAM_POSITION(ctx);
   uint32_t mux_rate, stuffing;
   VC_CONTAINER_BITS_T bits;
   VC_CONTAINER_STATUS_T status;

   if(_READ_U32(ctx) != 0x1BA) return VC_CONTAINER_ERROR_CORRUPTED;
   LOG_FORMAT(ctx, "pack_header");
   
   module->level++;

   if (PEEK_U8(ctx) & 0x40)  /* program stream */
   {
      if(READ_BYTES(ctx, header, 10) != 10) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 10);
      if(BITS_READ_U32(ctx, &bits, 2, "'01' marker bits") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_base = BITS_READ_U32(ctx, &bits, 3, "system_clock_reference_base [32..30]") << 30;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_base |= BITS_READ_U32(ctx, &bits, 15, "system_clock_reference_base [29..15]") << 15;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_base |= BITS_READ_U32(ctx, &bits, 15, "system_clock_reference_base [14..0]");
      LOG_FORMAT(ctx, "system_clock_reference_base %"PRId64, scr_base);
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_ext = BITS_READ_U32(ctx, &bits, 9, "system_clock_reference_extension");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      mux_rate = BITS_READ_U32(ctx, &bits, 22, "program_mux_rate");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      BITS_SKIP(ctx, &bits, 5, "reserved");
      stuffing = BITS_READ_U32(ctx, &bits, 3, "pack_stuffing_length");
      SKIP_BYTES(ctx, stuffing);
   }
   else /* system stream */
   {
      if(READ_BYTES(ctx, header, 8) != 8) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 8);
      if(BITS_READ_U32(ctx, &bits, 4, "'0010' marker bits") != 0x2) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_base = BITS_READ_U32(ctx, &bits, 3, "system_clock_reference_base [32..30]") << 30;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_base |= BITS_READ_U32(ctx, &bits, 15, "system_clock_reference_base [29..15]") << 15;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      scr_base |= BITS_READ_U32(ctx, &bits, 15, "system_clock_reference_base [14..0]");
      LOG_FORMAT(ctx, "system_clock_reference_base %"PRId64, scr_base);
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      mux_rate = BITS_READ_U32(ctx, &bits, 22, "program_mux_rate");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if ((status = STREAM_STATUS(ctx)) != VC_CONTAINER_SUCCESS) return status;
   
   module->level--;

   /* Set or update system_clock_reference, adjust bias if necessary */
   scr = scr_base * INT64_C(300) + scr_ext;

   if (module->scr_offset == VC_CONTAINER_TIME_UNKNOWN)
      module->scr_offset = scr;

   if (module->scr == VC_CONTAINER_TIME_UNKNOWN)
      module->scr_bias = -scr;
   else if (scr < module->scr)
      module->scr_bias = module->scr - scr;

   if (module->scr != VC_CONTAINER_TIME_UNKNOWN)
   {
      /* system_clock_reference is not necessarily continuous across the entire stream */
      if (scr > module->scr)
      {
         int64_t data_rate;
         data_rate = INT64_C(27000000) * (pack_offset - module->pack_offset) / (scr - module->scr);

         if (module->data_rate)
         {
            /* Simple moving average over data rate seen so far */
            module->data_rate = (module->data_rate * 31 + data_rate) >> 5;
         }
         else
         {
            module->data_rate = mux_rate * 50;
         }
      }

      module->pack_offset = pack_offset;
   }

   module->scr = scr;
   module->mux_rate = mux_rate;

   /* Check for a system header */
   if(PEEK_U32(ctx) == 0x1BB)
      return ps_read_system_header(ctx);

   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static void ps_get_stream_coding( VC_CONTAINER_T *ctx, unsigned int stream_id, 
   VC_CONTAINER_ES_TYPE_T *p_type, VC_CONTAINER_FOURCC_T *p_codec,
   VC_CONTAINER_FOURCC_T *p_variant)
{
   VC_CONTAINER_ES_TYPE_T type = VC_CONTAINER_ES_TYPE_UNKNOWN;
   VC_CONTAINER_FOURCC_T codec = VC_CONTAINER_CODEC_UNKNOWN;
   VC_CONTAINER_FOURCC_T variant = 0;

   VC_CONTAINER_PARAM_UNUSED(ctx);

   if (stream_id == 0xE2) /* FIXME: why is this stream number reserved for H264? */
   {
      type = VC_CONTAINER_ES_TYPE_VIDEO;
      codec = VC_CONTAINER_CODEC_H264;
   }
   else if ((stream_id & 0xF0) == 0xE0)
   {
      type = VC_CONTAINER_ES_TYPE_VIDEO;
      codec = VC_CONTAINER_CODEC_MP2V;
   }
   else if ((stream_id & 0xE0) == 0xC0)
   {
      type = VC_CONTAINER_ES_TYPE_AUDIO;
      codec = VC_CONTAINER_CODEC_MPGA;
      variant = VC_CONTAINER_VARIANT_MPGA_L2;
   }   

   /* FIXME: PRIVATE_EVOB_PES_PACKET with stream_id 0xFD ? */
   
   *p_type = type;
   *p_codec = codec;
   *p_variant = variant;
}

/*****************************************************************************/
static int64_t ps_pes_time_to_us( VC_CONTAINER_T *ctx, int64_t time )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   
   if (time == VC_CONTAINER_TIME_UNKNOWN)
      return VC_CONTAINER_TIME_UNKNOWN;

   /* Need to wait for system_clock_reference first */
   if (module->scr_bias == VC_CONTAINER_TIME_UNKNOWN)
      return VC_CONTAINER_TIME_UNKNOWN;

   /* Can't have valid bias without known system_clock_reference */
   vc_container_assert(module->scr != VC_CONTAINER_TIME_UNKNOWN);
   
   /* 90kHz (PES) clock --> (zero based) 27MHz system clock --> microseconds */
   return (INT64_C(300) * time + module->scr_bias) / INT64_C(27);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_pes_time( VC_CONTAINER_T *ctx,
   uint32_t *p_length, unsigned int pts_dts, int64_t *p_pts, int64_t *p_dts )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint8_t header[10];
   uint32_t length = *p_length;
   VC_CONTAINER_BITS_T bits;
   int64_t pts, dts;

   if (p_pts) *p_pts = VC_CONTAINER_TIME_UNKNOWN;
   if (p_dts) *p_dts = VC_CONTAINER_TIME_UNKNOWN;

   if (pts_dts == 0x2)
   {
      /* PTS only */
      LOG_FORMAT(ctx, "PTS");
      ctx->priv->module->level++;
      if(length < 5) return VC_CONTAINER_ERROR_CORRUPTED;
      if(READ_BYTES(ctx, header, 5) != 5) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 5);

      if(BITS_READ_U32(ctx, &bits, 4, "'0010' marker bits") != 0x2) return VC_CONTAINER_ERROR_CORRUPTED;
      pts = BITS_READ_U32(ctx, &bits, 3, "PTS [32..30]") << 30;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      pts |= BITS_READ_U32(ctx, &bits, 15, "PTS [29..15]") << 15;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      pts |= BITS_READ_U32(ctx, &bits, 15, "PTS [14..0]");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      LOG_FORMAT(ctx, "PTS %"PRId64, pts);
      if (p_pts) *p_pts = pts;
      length -= 5;
      ctx->priv->module->level--;
   }
   else if (pts_dts == 0x3)
   {
      /* PTS & DTS */
      LOG_FORMAT(ctx, "PTS DTS");
      ctx->priv->module->level++;
      if(length < 10) return VC_CONTAINER_ERROR_CORRUPTED;
      if(READ_BYTES(ctx, header, 10) != 10) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 10);

      /* PTS */
      if(BITS_READ_U32(ctx, &bits, 4, "'0011' marker bits") != 0x3) return VC_CONTAINER_ERROR_CORRUPTED;
      pts = BITS_READ_U32(ctx, &bits, 3, "PTS [32..30]") << 30;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      pts |= BITS_READ_U32(ctx, &bits, 15, "PTS [29..15]") << 15;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      pts |= BITS_READ_U32(ctx, &bits, 15, "PTS [14..0]");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      
      /* DTS */
      if(BITS_READ_U32(ctx, &bits, 4, "'0001' marker bits") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      dts = BITS_READ_U32(ctx, &bits, 3, "DTS [32..30]") << 30;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      dts |= BITS_READ_U32(ctx, &bits, 15, "DTS [29..15]") << 15;
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      dts |= BITS_READ_U32(ctx, &bits, 15, "DTS [14..0]");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      LOG_FORMAT(ctx, "PTS %"PRId64, pts);
      LOG_FORMAT(ctx, "DTS %"PRId64, dts);
      if (p_pts) *p_pts = pts;
      if (p_dts) *p_dts = dts;
      length -= 10;
      ctx->priv->module->level--;
   }
   else
   {
      status = VC_CONTAINER_ERROR_NOT_FOUND;
   }
   
   *p_length = *p_length - length;
   
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_pes_extension( VC_CONTAINER_T *ctx,
   uint32_t *p_length )
{
   unsigned int pes_private_data, pack_header, packet_seq_counter, pstd_buffer, extension2;
   uint8_t header[2];
   uint32_t length = *p_length;
   VC_CONTAINER_BITS_T bits;
   unsigned int i;

   LOG_FORMAT(ctx, "PES_extension");
   ctx->priv->module->level++;
   if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
   if(READ_BYTES(ctx, header, 1) != 1) return VC_CONTAINER_ERROR_EOS;
   BITS_INIT(ctx, &bits, header, 1);

   pes_private_data = BITS_READ_U32(ctx, &bits, 1, "PES_private_data_flag");
   pack_header = BITS_READ_U32(ctx, &bits, 1, "pack_header_field_flag");
   packet_seq_counter = BITS_READ_U32(ctx, &bits, 1, "program_packet_sequence_counter_flag");
   pstd_buffer = BITS_READ_U32(ctx, &bits, 1, "P-STD_buffer_flag");
   BITS_SKIP(ctx, &bits, 3, "3 reserved_bits");
   extension2 = BITS_READ_U32(ctx, &bits, 1, "PES_extension_flag_2");
   length -= 1;

   if (pes_private_data) 
   {
      if(length < 16) return VC_CONTAINER_ERROR_CORRUPTED;
      SKIP_BYTES(ctx, 16); /* PES_private_data */
      length -= 16;
   }

   if (pack_header) 
   {
      unsigned int pack_field_len;
      if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
      pack_field_len = READ_U8(ctx, "pack_field_length");
      length -= 1;
      if(length < pack_field_len) return VC_CONTAINER_ERROR_CORRUPTED;
      SKIP_BYTES(ctx, pack_field_len); /* pack_header */
      length -= pack_field_len;
   }

   if (packet_seq_counter)
   {
      if(length < 2) return VC_CONTAINER_ERROR_CORRUPTED;
      if(READ_BYTES(ctx, header, 2) != 2) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 2);

      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      BITS_SKIP(ctx, &bits, 7, "program_packet_sequence_counter");
      if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      BITS_SKIP(ctx, &bits, 1, "MPEG1_MPEG2_identifier");
      BITS_SKIP(ctx, &bits, 6, "original_stuff_length");
      length -= 2;
   }

   if (pstd_buffer) 
   {
      if(length < 2) return VC_CONTAINER_ERROR_CORRUPTED;
      if(READ_BYTES(ctx, header, 2) != 2) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 2);

      if(BITS_READ_U32(ctx, &bits, 2, "'01' marker bits") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
      BITS_SKIP(ctx, &bits, 1, "P-STD_buffer_scale");
      BITS_SKIP(ctx, &bits, 13, "P-STD_buffer_size");
      length -= 2;
   }

   if (extension2)
   {
      uint8_t ext_field_len;
      
      if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
      if(READ_BYTES(ctx, &ext_field_len, 1) != 1) return VC_CONTAINER_ERROR_EOS;
      length -= 1;

      if((ext_field_len & 0x80) != 0x80) return VC_CONTAINER_ERROR_CORRUPTED; /* marker_bit */
      ext_field_len &= ~0x80;  
      LOG_FORMAT(ctx, "PES_extension_field_length %d", ext_field_len);

      for (i = 0; i < ext_field_len; i++) 
      {
         SKIP_U8(ctx, "reserved");
         length--;
      }
   }

   ctx->priv->module->level--;
   
   *p_length = *p_length - length; /* Number of bytes read from stream */

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_pes_packet_header( VC_CONTAINER_T *ctx,
   uint32_t *p_length, int64_t *p_pts, int64_t *p_dts )
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_BITS_T bits;
   uint32_t size, length = *p_length;
   unsigned int pts_dts;
   uint8_t header[10];
   
   if(length < 3) return VC_CONTAINER_ERROR_CORRUPTED;
    
   if ((PEEK_U8(ctx) & 0xC0) == 0x80) /* program stream */
   {
      unsigned int escr, es_rate, dsm_trick_mode, additional_copy_info, pes_crc, pes_extension;
      unsigned int header_length;
      
      if(READ_BYTES(ctx, header, 3) != 3) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 3);
     
      if (BITS_READ_U32(ctx, &bits, 2, "'10' marker bits") != 0x2) return VC_CONTAINER_ERROR_CORRUPTED;
      BITS_SKIP(ctx, &bits, 2, "PES_scrambling_control");
      BITS_SKIP(ctx, &bits, 1, "PES_priority");
      BITS_SKIP(ctx, &bits, 1, "data_alignment_indicator");
      BITS_SKIP(ctx, &bits, 1, "copyright");
      BITS_SKIP(ctx, &bits, 1, "original_or_copy");
      pts_dts = BITS_READ_U32(ctx, &bits, 2, "PTS_DTS_flags");
      escr = BITS_READ_U32(ctx, &bits, 1, "ESCR_flag");
      es_rate = BITS_READ_U32(ctx, &bits, 1, "ES_rate_flag");
      dsm_trick_mode = BITS_READ_U32(ctx, &bits, 1, "DSM_trick_mode_flag");
      additional_copy_info = BITS_READ_U32(ctx, &bits, 1, "additional_copy_info_flag");
      pes_crc = BITS_READ_U32(ctx, &bits, 1, "PES_CRC_flag");
      pes_extension = BITS_READ_U32(ctx, &bits, 1, "PES_extension_flag");
      header_length = BITS_READ_U32(ctx, &bits, 8, "PES_header_data_length");
      length -= 3;

      size = length;
      status = ps_read_pes_time(ctx, &size, pts_dts, p_pts, p_dts);
      if (status && status != VC_CONTAINER_ERROR_NOT_FOUND) return status;
      length -= size;
      header_length -= size;

      if (escr)
      {
         /* Elementary stream clock reference */
         int64_t escr;
         
         ctx->priv->module->level++;
         if(length < 6) return VC_CONTAINER_ERROR_CORRUPTED;
         if(READ_BYTES(ctx, header, 6) != 6) return VC_CONTAINER_ERROR_EOS;
         BITS_INIT(ctx, &bits, header, 6);
         
         BITS_SKIP(ctx, &bits, 2, "reserved_bits");
         escr = BITS_READ_U32(ctx, &bits, 3, "ESCR_base [32..30]") << 30;
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
         escr |= BITS_READ_U32(ctx, &bits, 15, "ESCR_base [29..15]") << 15;
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
         escr |= BITS_READ_U32(ctx, &bits, 15, "ESCR_base [14..0]");
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
         BITS_READ_U32(ctx, &bits, 9, "ESCR_extension");
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
            
         LOG_FORMAT(ctx, "ESCR_base %"PRId64, escr);
         length -= 6;
         header_length -= 6;
         ctx->priv->module->level--;
      }
   
      if (es_rate) 
      {
         /* Elementary stream rate */
         if(length < 3) return VC_CONTAINER_ERROR_CORRUPTED;
         if(READ_BYTES(ctx, header, 3) != 3) return VC_CONTAINER_ERROR_EOS;
         BITS_INIT(ctx, &bits, header, 3);
   
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
         BITS_READ_U32(ctx, &bits, 22, "ES_rate");
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
         length -= 3;
         header_length -= 3;
      }
      
      if (dsm_trick_mode)
      {
         unsigned int trick_mode;
   
         if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
         if(READ_BYTES(ctx, header, 1) != 1) return VC_CONTAINER_ERROR_EOS;
         BITS_INIT(ctx, &bits, header, 1);
   
         trick_mode = BITS_READ_U32(ctx, &bits, 3, "trick_mode_control");
         if (trick_mode == 0x0 /* fast_forward */)
         {
            BITS_SKIP(ctx, &bits, 2, "field_id");
            BITS_SKIP(ctx, &bits, 1, "intra_slice_refresh");
            BITS_SKIP(ctx, &bits, 2, "frequency_truncation");
         }
         else if (trick_mode == 0x1 /* slow_motion */) 
         {
            BITS_SKIP(ctx, &bits, 5, "rep_cntrl");
         }
         else if (trick_mode == 0x2 /* freeze_frame */)
         {
            BITS_SKIP(ctx, &bits, 2, "field_id");
            BITS_SKIP(ctx, &bits, 3, "reserved_bits");
         }
         else if (trick_mode == 0x3 /* fast_reverse */)
         {
            BITS_SKIP(ctx, &bits, 2, "field_id");
            BITS_SKIP(ctx, &bits, 1, "intra_slice_refresh");
            BITS_SKIP(ctx, &bits, 2, "frequency_truncation");
         }
         else if (trick_mode == 0x4 /* slow_reverse */)
            BITS_SKIP(ctx, &bits, 5, "rep_cntrl");
         else
            BITS_SKIP(ctx, &bits, 5, "5 reserved_bits");
            
         length -= 1;
         header_length -= 1;
      }
      
      if (additional_copy_info)
      {
         if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
         if(READ_BYTES(ctx, header, 1) != 1) return VC_CONTAINER_ERROR_EOS;
         BITS_INIT(ctx, &bits, header, 1);
   
         if(BITS_READ_U32(ctx, &bits, 1, "marker_bit") != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
         BITS_SKIP(ctx, &bits, 7, "additional_copy_info");
         
         length -= 1;
         header_length -= 1;
      }
   
      if (pes_crc) 
      {
         SKIP_U16(ctx, "previous_PES_packet_CRC");
         length -= 2;
         header_length -= 2;
      }
   
      if (pes_extension)
      {
         size = length;
         if ((status = ps_read_pes_extension(ctx, &size)) != VC_CONTAINER_SUCCESS) return status;
         length -= size;
         header_length -= size;
      }
      
      if (header_length <= length)
      {
         SKIP_BYTES(ctx, header_length); /* header stuffing */
         length -= header_length;
      }
   }
   else /* MPEG 1 PES header */
   {
      if(length < 12) return VC_CONTAINER_ERROR_CORRUPTED;

      while (PEEK_U8(ctx) == 0xFF && length > 0)
      {
         SKIP_U8(ctx, "stuffing");
         length--;
      }
      
      if (length == 0) return VC_CONTAINER_ERROR_CORRUPTED;

      if ((PEEK_U8(ctx) & 0xC0) == 0x40)
      {
         if(length < 2) return VC_CONTAINER_ERROR_CORRUPTED;
         SKIP_U8(ctx, "???");
         SKIP_U8(ctx, "???");
         length -= 2;
      }

      pts_dts = (PEEK_U8(ctx) & 0x30) >> 4;
      size = length;
      status = ps_read_pes_time(ctx, &size, pts_dts, p_pts, p_dts);
      if (status && status != VC_CONTAINER_ERROR_NOT_FOUND) 
         return status;
      length -= size;

      if (status == VC_CONTAINER_ERROR_NOT_FOUND)
      {
         if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
         SKIP_U8(ctx, "???");
         length -= 1;
      }
   }

   *p_length = length;
   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_private_stream_1_coding( VC_CONTAINER_T *ctx,
   VC_CONTAINER_ES_TYPE_T *p_type, VC_CONTAINER_FOURCC_T *p_codec,
   uint32_t *substream_id, uint32_t *p_length )
{
   VC_CONTAINER_ES_TYPE_T type = VC_CONTAINER_ES_TYPE_UNKNOWN;
   VC_CONTAINER_FOURCC_T codec = VC_CONTAINER_CODEC_UNKNOWN;
   uint32_t length;
   uint8_t id = 0;
   
   length = *p_length;

   if(length < 1) return VC_CONTAINER_ERROR_CORRUPTED;
   if(READ_BYTES(ctx, &id, 1) != 1) return VC_CONTAINER_ERROR_EOS;
   length -= 1;

   LOG_FORMAT(ctx, "private_stream_1 byte: 0x%x (%u)", id, id);

   if (id >= 0x20 && id <= 0x3f)
   {
      type = VC_CONTAINER_ES_TYPE_SUBPICTURE;
      codec = VC_CONTAINER_CODEC_UNKNOWN;
   } 
   else if ((id >= 0x80 && id <= 0x87) || (id >= 0xC0 && id <= 0xCF)) 
   {
      type = VC_CONTAINER_ES_TYPE_AUDIO;
      codec = VC_CONTAINER_CODEC_AC3;
   } 
   else if ((id >= 0x88 && id <= 0x8F) || (id >= 0x98 && id <= 0x9F)) 
   {
      type = VC_CONTAINER_ES_TYPE_AUDIO;
      codec = VC_CONTAINER_CODEC_DTS;
   } 
   else if (id >= 0xA0 && id <= 0xBF) 
   {
      type = VC_CONTAINER_ES_TYPE_AUDIO;
      codec = VC_CONTAINER_CODEC_PCM_SIGNED;
   }
   else
   {
      LOG_FORMAT(ctx, "Unknown private_stream_1 byte: 0x%x (%u)", id, id);
   }
   
   *substream_id = id;
   *p_type = type;
   *p_codec = codec;
   *p_length = length;
   
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_private_stream_1_format( VC_CONTAINER_T *ctx,
    VC_CONTAINER_ES_FORMAT_T *format, uint32_t *length )
{
   uint8_t header[8];
   VC_CONTAINER_BITS_T bits;

   if (format->codec == VC_CONTAINER_CODEC_PCM_SIGNED)
   {
      static const unsigned fs_tab[4] = { 48000, 96000, 44100, 32000 };
      static const unsigned bps_tab[] = {16, 20, 24, 0};
      
      unsigned fs, bps, nchan;
      
      if(*length < 6) return VC_CONTAINER_ERROR_CORRUPTED;
      if(READ_BYTES(ctx, header, 6) != 6) return VC_CONTAINER_ERROR_EOS;
      BITS_INIT(ctx, &bits, header, 6);
   
      BITS_SKIP(ctx, &bits, 8, "???");
      BITS_SKIP(ctx, &bits, 8, "???");
      BITS_SKIP(ctx, &bits, 8, "???");
      BITS_SKIP(ctx, &bits, 1, "emphasis");
      BITS_SKIP(ctx, &bits, 1, "mute");
      BITS_SKIP(ctx, &bits, 1, "reserved");
      BITS_SKIP(ctx, &bits, 5, "frame number");
      bps = BITS_READ_U32(ctx, &bits, 2, "quant");
      fs = BITS_READ_U32(ctx, &bits, 2, "freq");
      BITS_SKIP(ctx, &bits, 1, "reserved");
      nchan = BITS_READ_U32(ctx, &bits, 3, "channels");
      *length -= 6;
      
      format->type->audio.sample_rate = fs_tab[fs];
      format->type->audio.bits_per_sample = bps_tab[bps];
      format->type->audio.channels = nchan + 1;
      format->type->audio.block_align = 
         (format->type->audio.channels * format->type->audio.bits_per_sample + 7 ) / 8;
   }
   else
   {
      if(*length < 3) return VC_CONTAINER_ERROR_CORRUPTED;
      SKIP_U8(ctx, "num of frames");
      SKIP_U8(ctx, "start pos hi");
      SKIP_U8(ctx, "start pos lo");
      *length -= 3;
   }
   
   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_read_pes_packet( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   uint8_t header[10];
   VC_CONTAINER_BITS_T bits;
   uint32_t length, stream_id, substream_id = 0;
   VC_CONTAINER_ES_TYPE_T type;
   VC_CONTAINER_FOURCC_T codec, variant = 0;
   VC_CONTAINER_TRACK_T *track;
   int64_t pts, dts;
   
   if(_READ_U24(ctx) != 0x1) return VC_CONTAINER_ERROR_CORRUPTED;
   if(READ_BYTES(ctx, header, 3) != 3) return VC_CONTAINER_ERROR_EOS;
   LOG_FORMAT(ctx, "pes_packet_header");
   module->level++;

   BITS_INIT(ctx, &bits, header, 3);
   stream_id = BITS_READ_U32(ctx, &bits, 8, "stream_id");
   length = BITS_READ_U32(ctx, &bits, 16, "PES_packet_length");

   if (stream_id < 0xBC) return VC_CONTAINER_ERROR_CORRUPTED;

   if (stream_id == 0xBC /* program_stream_map */ || stream_id == 0xBE /* padding_stream */ ||
       stream_id == 0xBF /* private_stream_2 */ || stream_id == 0xF0 /* ECM */ ||
       stream_id == 0xF1 /* EMM */ || stream_id == 0xFF /* program_stream_directory */ ||
       stream_id == 0xF2 /* DSMCC_stream */ || stream_id == 0xF8 /* ITU-T Rec. H.222.1 type E */)
       goto skip;

   /* Parse PES packet header */
   if ((status = ps_read_pes_packet_header(ctx, &length, &pts, &dts)) != VC_CONTAINER_SUCCESS) 
      return status;

   /* For private_stream_1, encoding format is stored in the payload */
   if (stream_id == 0xBD)
   {
      status = ps_read_private_stream_1_coding(ctx, &type, &codec, &substream_id, &length);
      if (status) return status;
   }
   else
      ps_get_stream_coding(ctx, stream_id, &type, &codec, &variant);

   /* Check that we know what to do with this track */
   if(type == VC_CONTAINER_ES_TYPE_UNKNOWN || codec == VC_CONTAINER_CODEC_UNKNOWN)
      goto skip;
   
   track = ps_find_track(ctx, stream_id, substream_id, module->searching_tracks);
   if(!track) goto skip;

   if (module->searching_tracks)
   {
      track->is_enabled = true;
      track->format->es_type = type;
      track->format->codec = codec;
      track->format->codec_variant = variant;

      /* For private_stream_1, we need to parse payload further to get elementary stream
         format */
      if (stream_id == 0xBD)
      {
         uint32_t current_length = length;
         status = ps_read_private_stream_1_format(ctx, track->format, &length);
         if (status) return status;
         track->priv->module->payload_offset = current_length - length;
      }

      goto skip;
   }
   else
   {
      unsigned i;
      SKIP_BYTES(ctx, track->priv->module->payload_offset);
      length -= track->priv->module->payload_offset;

      /* Find track index */
      for(i = 0; i < ctx->tracks_num; i++)
         if(ctx->tracks[i] == track) break;
      vc_container_assert(i < ctx->tracks_num);

      module->packet_track = i;
      module->packet_data_size = length;
      module->packet_pts = pts;
      module->packet_dts = dts;
   }

end:
   module->level--;
   return STREAM_STATUS(ctx);

skip:
   SKIP_BYTES(ctx, length); /* remaining PES_packet_data */
   goto end;
}

/*****************************************************************************/
STATIC_INLINE VC_CONTAINER_STATUS_T ps_find_pes_packet( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   uint8_t buffer[4];
   unsigned int i;

   module->packet_data_size = 0;

   for (i = 0; i != PS_PACK_SCAN_MAX; ++i)
   {
      if((status = ps_find_start_code(ctx, buffer)) != VC_CONTAINER_SUCCESS)
         break;

      if (buffer[3] == 0xBA && ((status = ps_read_pack_header(ctx)) != VC_CONTAINER_SUCCESS))
         continue; /* pack start code but parsing failed, goto resync */

      if ((status = ps_read_pes_packet(ctx)) == VC_CONTAINER_SUCCESS)
         break;
   }

   return status;
}

/*****************************************************************************
Functions exported as part of the Container Module API
*****************************************************************************/

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_reader_read( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;

   vc_container_assert(!module->searching_tracks);

   while(!module->packet_data_left)
   {
      if(ps_find_pes_packet(ctx) != VC_CONTAINER_SUCCESS)
      {
         status = VC_CONTAINER_ERROR_EOS;
         goto error;
      }

      module->packet_data_left = module->packet_data_size;
   }

   p_packet->track = module->packet_track;
   p_packet->size = module->packet_data_left;
   p_packet->flags = 0;
   p_packet->pts = ps_pes_time_to_us(ctx, module->packet_pts);
   p_packet->dts = ps_pes_time_to_us(ctx, module->packet_dts);

   if (flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      SKIP_BYTES(ctx, module->packet_data_left);
      module->packet_data_left = 0;
      return VC_CONTAINER_SUCCESS;
   }

   if (flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   p_packet->size = MIN(p_packet->buffer_size, module->packet_data_left);
   p_packet->size = READ_BYTES(ctx, p_packet->data, p_packet->size);
   module->packet_data_left -= p_packet->size;

   /* Temporary work-around for lpcm audio */
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[module->packet_track];
      if (track->format->codec == VC_CONTAINER_CODEC_PCM_SIGNED)
      {
         unsigned i;
         uint16_t *ptr = (uint16_t *)p_packet->data;

         for (i = 0; i < p_packet->size / 2; i ++)
         {
            uint32_t v = *ptr;
            *ptr++ = v >> 8 | ( (v & 0xFF) << 8 );
         }
      }
   }

   if (module->packet_data_left)
      module->packet_pts = module->packet_dts = VC_CONTAINER_TIME_UNKNOWN;

   return STREAM_STATUS(ctx);

error:
   if (status == VC_CONTAINER_ERROR_EOS)
   {
      /* Reset time reference and calculation state */
      ctx->priv->module->scr = VC_CONTAINER_TIME_UNKNOWN;
      ctx->priv->module->scr_bias = -module->scr_offset;
   }
   
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_reader_seek( VC_CONTAINER_T *ctx,
   int64_t *p_offset, VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint64_t seekpos, position;
   int64_t scr;
   
   VC_CONTAINER_PARAM_UNUSED(flags);

   if (mode != VC_CONTAINER_SEEK_MODE_TIME || !STREAM_SEEKABLE(ctx))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   position = STREAM_POSITION(ctx);
   scr = module->scr;

   if (*p_offset == INT64_C(0))
      seekpos = module->data_offset;
   else
   {
      if (!ctx->duration)
         return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

      /* The following is an estimate that might be quite inaccurate */
      seekpos = module->data_offset + (*p_offset * module->data_size) / ctx->duration;
   }

   SEEK(ctx, seekpos);
   module->scr = module->scr_offset;
   status = ps_find_pes_packet(ctx);
   if (status && status != VC_CONTAINER_ERROR_EOS)
      goto error;

   module->packet_data_left = module->packet_data_size;

   if (module->packet_pts != VC_CONTAINER_TIME_UNKNOWN)
      *p_offset = ps_pes_time_to_us(ctx, module->packet_pts);
   else if (module->data_size)
      *p_offset = (STREAM_POSITION(ctx) - module->data_offset) * ctx->duration / module->data_size;

   return STREAM_STATUS(ctx);

error:
   module->scr = scr;
   SEEK(ctx, position);
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ps_reader_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int i;

   for(i = 0; i < ctx->tracks_num; i++)
      vc_container_free_track(ctx, ctx->tracks[i]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T ps_reader_open( VC_CONTAINER_T *ctx )
{
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   uint8_t buffer[4];
   unsigned int i;

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Since MPEG is difficult to auto-detect, we use the extension as
      part of the autodetection */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "ps") && strcasecmp(extension, "vob") &&
      strcasecmp(extension, "mpg") && strcasecmp(extension, "mp2") &&
      strcasecmp(extension, "mp3") && strcasecmp(extension, "mpeg"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if((status = ps_find_start_code(ctx, buffer)) != VC_CONTAINER_SUCCESS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;  /* We didn't find a valid pack or PES packet  */

   LOG_DEBUG(ctx, "using ps reader");

   /* We are probably dealing with a PS file */
   LOG_FORMAT(ctx, "MPEG PS reader, found start code: 0x%02x%02x%02x%02x",
      buffer[0], buffer[1], buffer[2], buffer[3]);

   /* Need to allocate context before searching for streams */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = module->tracks;

   /* Store offset so we can get back to what we consider the first pack or
      packet */
   module->data_offset = STREAM_POSITION(ctx);

   /* Search for tracks, reset time reference and calculation state first */
   ctx->priv->module->scr_offset = ctx->priv->module->scr = VC_CONTAINER_TIME_UNKNOWN;
   ctx->priv->module->searching_tracks = true;

   for (i = 0; i != PS_PACK_SCAN_MAX; ++i)
   {
      if (buffer[3] == 0xBA && (ps_read_pack_header(ctx) != VC_CONTAINER_SUCCESS))
         goto resync;

      if (ps_read_pes_packet(ctx) == VC_CONTAINER_SUCCESS)
         continue;

resync:
      LOG_DEBUG(ctx, "Lost sync, scanning for start code");
      if((status = ps_find_start_code(ctx, buffer)) != VC_CONTAINER_SUCCESS)
         return VC_CONTAINER_ERROR_CORRUPTED;
      LOG_DEBUG(ctx, "MPEG PS reader, found start code: 0x%"PRIx64" (%"PRId64"): 0x%02x%02x%02x%02x",
         STREAM_POSITION(ctx), STREAM_POSITION(ctx), buffer[0], buffer[1], buffer[2], buffer[3]);
   }

   /* Seek back to the start of data */
   SEEK(ctx, module->data_offset);

   /* Bail out if we didn't find any tracks */
   if(!ctx->tracks_num)
   {
      status = VC_CONTAINER_ERROR_NO_TRACK_AVAILABLE;
      goto error;
   }

   /* Set data size (necessary for seeking) */
   module->data_size = MAX(ctx->priv->io->size - module->data_offset, INT64_C(0));

   /* Estimate data rate (necessary for seeking) */
   if(STREAM_SEEKABLE(ctx))
   {
      /* Estimate data rate by jumping in the stream */
      #define PS_PACK_SEARCH_MAX 64
      uint64_t position = module->data_offset;
      for (i = 0; i != PS_PACK_SEARCH_MAX; ++i)
      {
         position += (module->data_size / (PS_PACK_SEARCH_MAX + 1));
         SEEK(ctx, position);

         for(;;)
         {
            if(ps_find_start_code(ctx, buffer) != VC_CONTAINER_SUCCESS)
               break;
   
            if (buffer[3] == 0xBA)
            {
               if (ps_read_pack_header(ctx) == VC_CONTAINER_SUCCESS)
                  break;
            }
            else
            {
               /* Skip PES packet */
               unsigned length;
               SKIP_U32(ctx, "PES packet startcode");
               length = READ_U16(ctx, "PES packet length");
               SKIP_BYTES(ctx, length);
            }
         }
      }

      ctx->duration = (INT64_C(1000000) * module->data_size) / (module->data_rate);
      
      if (module->scr > module->scr_offset)
      {
         int64_t delta = (module->scr - module->scr_offset) / INT64_C(27);
      
         if (delta > ctx->duration)
            ctx->duration = delta;
      }

      /* Seek back to the start of data */
      SEEK(ctx, module->data_offset);
   }
   else
   {
      /* For most files, program_mux_rate is not reliable at all */
      ctx->duration = (INT64_C(100000) * module->data_size) / (INT64_C(5) * module->mux_rate);
   }

   /* Reset time reference and calculation state, we're now ready to read data */
   module->scr = VC_CONTAINER_TIME_UNKNOWN;
   module->scr_bias = VC_CONTAINER_TIME_UNKNOWN;

   ctx->priv->module->searching_tracks = false;

   if(STREAM_SEEKABLE(ctx)) ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   ctx->priv->pf_close = ps_reader_close;
   ctx->priv->pf_read = ps_reader_read;
   ctx->priv->pf_seek = ps_reader_seek;

   return STREAM_STATUS(ctx);

 error:
   LOG_DEBUG(ctx, "ps: error opening stream (%i)", status);
   if(module) ps_reader_close(ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open ps_reader_open
#endif
