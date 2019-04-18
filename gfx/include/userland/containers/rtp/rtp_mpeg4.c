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
#include <stdio.h>
#include <string.h>

#include "containers/containers.h"

#include "containers/core/containers_logging.h"
#include "containers/core/containers_bits.h"
#include "containers/core/containers_list.h"
#include "rtp_priv.h"
#include "rtp_mpeg4.h"

#ifdef _DEBUG
#define RTP_DEBUG 1
#endif

/******************************************************************************
Defines and constants.
******************************************************************************/

/******************************************************************************
Type definitions
******************************************************************************/

/** MPEG-4 stream types, ISO/IEC 14496-1:2010 Table 6 */
typedef enum
{
   MPEG4_OBJECT_DESCRIPTOR_STREAM = 1,
   MPEG4_CLOCK_REFERENCE_STREAM = 2,
   MPEG4_SCENE_DESCRIPTION_STREAM = 3,
   MPEG4_VISUAL_STREAM = 4,
   MPEG4_AUDIO_STREAM = 5,
   MPEG4_MPEG7_STREAM = 6,
   MPEG4_IPMP_STREAM = 7,
   MPEG4_OBJECT_CONTENT_INFO_STREAM = 8,
   MPEG4_MPEGJ_STREAM = 9,
   MPEG4_INTERACTION_STREAM = 10,
   MPEG4_IPMP_TOOL_STREAM = 11,
} mp4_stream_type_t;

/** MPEG-4 audio object types, ISO/IEC 14496-3:2009 Table 1.17 */
typedef enum
{
   MPEG4A_AAC_MAIN = 1,
   MPEG4A_AAC_LC = 2,
   MPEG4A_AAC_SSR = 3,
   MPEG4A_AAC_LTP = 4,
   MPEG4A_SBR = 5,
   MPEG4A_AAC_SCALABLE = 6,
   MPEG4A_TWIN_VQ = 7,
   MPEG4A_CELP = 8,
   MPEG4A_HVXC = 9,
   MPEG4A_TTSI = 12,
   MPEG4A_MAIN_SYNTHETIC = 13,
   MPEG4A_WAVETABLE_SYNTHESIS = 14,
   MPEG4A_GENERAL_MIDI = 15,
   MPEG4A_ALGORITHMIC_SYNTHESIS = 16,
   MPEG4A_ER_AAC_LC = 17,
   MPEG4A_ER_AAC_LTP = 19,
   MPEG4A_ER_AAC_SCALABLE = 20,
   MPEG4A_ER_TWIN_VQ = 21,
   MPEG4A_ER_BSAC = 22,
   MPEG4A_ER_AAC_LD = 23,
   MPEG4A_ER_CELP = 24,
   MPEG4A_ER_HVXC = 25,
   MPEG4A_ER_HILN = 26,
   MPEG4A_ER_PARAMETERIC = 27,
   MPEG4A_SSC = 28,
   MPEG4A_PS = 29,
   MPEG4A_MPEG_SURROUND = 30,
   MPEG4A_LAYER_1 = 32,
   MPEG4A_LAYER_2 = 33,
   MPEG4A_LAYER_3 = 34,
   MPEG4A_DST = 35,
   MPEG4A_ALS = 36,
   MPEG4A_SLS = 37,
   MPEG4A_SLS_NON_CORE = 38,
   MPEG4A_ER_AAC_ELD = 39,
   MPEG4A_SMR_SIMPLE = 40,
   MPEG4A_SMR_MAIN = 41,
} mp4_audio_object_type_t;

/** RTP MPEG-4 modes */
typedef enum
{
   MP4_GENERIC_MODE = 0,
   MP4_CELP_CBR_MODE,
   MP4_CELP_VBR_MODE,
   MP4_AAC_LBR_MODE,
   MP4_AAC_HBR_MODE
} mp4_mode_t;

typedef struct mp4_mode_detail_tag
{
   const char *name;
   mp4_mode_t mode;
} MP4_MODE_ENTRY_T;

/* RTP MPEG-4 mode look-up table.
 * Note: case-insensitive sort by name */
static MP4_MODE_ENTRY_T mp4_mode_array[] = {
   { "aac-hbr", MP4_AAC_HBR_MODE },
   { "aac-lbr", MP4_AAC_LBR_MODE },
   { "celp-cbr", MP4_CELP_CBR_MODE },
   { "celp-vbr", MP4_CELP_VBR_MODE },
   { "generic", MP4_GENERIC_MODE },
};

static int mp4_mode_comparator(const MP4_MODE_ENTRY_T *a, const MP4_MODE_ENTRY_T *b);

VC_CONTAINERS_STATIC_LIST(mp4_mode_lookup, mp4_mode_array, mp4_mode_comparator);

typedef struct au_info_tag
{
   uint32_t available;
   uint32_t index;
   int32_t cts_delta;
   int32_t dts_delta;
} AU_INFO_T;

typedef struct mp4_payload_tag
{
   mp4_stream_type_t stream_type;
   uint32_t profile_level_id;
   mp4_mode_t mode;
   uint32_t size_length;
   uint32_t index_length;
   uint32_t index_delta_length;
   uint32_t cts_delta_length;
   uint32_t dts_delta_length;
   uint32_t object_type;
   uint32_t constant_size;
   uint32_t constant_duration;
   uint32_t auxiliary_length;
   VC_CONTAINER_BITS_T au_headers;
   AU_INFO_T au_info;
} MP4_PAYLOAD_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T mp4_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);

/******************************************************************************
Local Functions
******************************************************************************/

/**************************************************************************//**
 * Convert a hexadecimal character to a value between 0 and 15.
 * Upper and lower case characters are supported. An invalid chacter return zero.
 *
 * @param hex  The character to convert.
 * @return  The value of the character.
 */
static uint8_t hex_to_nybble(char hex)
{
   if (hex >= '0' && hex <= '9')
      return hex - '0';
   if (hex >= 'A' && hex <= 'F')
      return hex - 'A' + 10;
   if (hex >= 'a' && hex <= 'f')
      return hex - 'a' + 10;
   return 0;   /* Illegal character (not hex) */
}

/**************************************************************************//**
 * Convert a sequence of hexadecimal characters to consecutive entries in a
 * byte array.
 * The string must contain at least twice as many characters as the number of
 * bytes to convert.
 *
 * @param hex              The hexadecimal string.
 * @param buffer           The buffer into which bytes are to be stored.
 * @param bytes_to_convert The number of bytes in the array to be filled.
 */
static void hex_to_byte_buffer(const char *hex,
      uint8_t *buffer,
      uint32_t bytes_to_convert)
{
   uint8_t value;

   while (bytes_to_convert--)
   {
      value = hex_to_nybble(*hex++) << 4;
      value |= hex_to_nybble(*hex++);
      *buffer++ = value;
   }
}

/**************************************************************************//**
 * Retrieves and checks the stream type in the URI parameters.
 *
 * @param p_ctx   The RTP container context.
 * @param track   The track being constructed.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_get_stream_type(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   MP4_PAYLOAD_T *extra = (MP4_PAYLOAD_T *)track->priv->module->extra;
   uint32_t stream_type;
   VC_CONTAINER_ES_TYPE_T expected_es_type;

   if (!rtp_get_parameter_u32(params, "streamType", &stream_type))
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   switch (stream_type)
   {
   case MPEG4_AUDIO_STREAM:
      extra->stream_type = MPEG4_AUDIO_STREAM;
      expected_es_type = VC_CONTAINER_ES_TYPE_AUDIO;
      break;
   default:
      LOG_ERROR(p_ctx, "Unsupported MPEG-4 stream type: %u", stream_type);
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   if (track->format->es_type != expected_es_type)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Decode and store audio configuration information from an MP4 audio
 * configuration bit stream.
 *
 * @param p_ctx      The RTP container context.
 * @param track      The track being constructed.
 * @param bit_stream The bit stream containing the audio configuration.
 * @return  True if the configuration was decoded successfully, false otherwise.
 */
static bool mp4_decode_audio_config(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      VC_CONTAINER_BITS_T *bit_stream)
{
   static uint32_t mp4_audio_sample_rate[] =
         { 96000, 88200, 64000, 48000, 44100, 32000, 24000,
           22050, 16000, 12000, 11025, 8000, 7350, 0, 0 };

   VC_CONTAINER_AUDIO_FORMAT_T *audio = &track->format->type->audio;
   uint32_t audio_object_type;
   uint32_t sampling_frequency_index;
   uint32_t channel_configuration;

   audio_object_type = BITS_READ_U32(p_ctx, bit_stream, 5, "audioObjectType");
   if (audio_object_type == 31)
      audio_object_type = BITS_READ_U32(p_ctx, bit_stream, 6, "audioObjectTypeExt") + 32;

   sampling_frequency_index = BITS_READ_U32(p_ctx, bit_stream, 4, "samplingFrequencyIndex");
   if (sampling_frequency_index == 0xF)
      audio->sample_rate = BITS_READ_U32(p_ctx, bit_stream, 24, "samplingFrequency");
   else
      audio->sample_rate = mp4_audio_sample_rate[sampling_frequency_index];
   if (!audio->sample_rate) return false;

   track->priv->module->timestamp_clock = audio->sample_rate;

   channel_configuration = BITS_READ_U32(p_ctx, bit_stream, 4, "channelConfiguration");
   switch (channel_configuration)
   {
   case 1:  /* 1 channel, centre front */
   case 2:  /* 2 channel, stereo front */
   case 3:  /* 3 channel, centre and stereo front */
   case 4:  /* 4 channel, centre and stereo front, mono surround */
   case 5:  /* 5 channel, centre and stereo front, stereo surround */
   case 6:  /* 5.1 channel, centre and stereo front, stereo surround, low freq */
      audio->channels = channel_configuration;
      break;
   case 7:  /* 7.1 channel, centre, stereo and stereo outside front, stereo surround, low freq */
      audio->channels = channel_configuration + 1;
      break;
   default:
      LOG_DEBUG(p_ctx, "MPEG-4: Unsupported channel configuration (%u)", channel_configuration);
      return false;
   }

   switch (audio_object_type)
   {
   case MPEG4A_AAC_LC:
      {
         uint32_t ga_specific_config = BITS_READ_U32(p_ctx, bit_stream, 3, "GASpecificConfig");

         /* Make sure there are no unexpected (and unsupported) additional configuration elements */
         if (ga_specific_config != 0)
         {
            LOG_DEBUG(p_ctx, "MPEG-4: Unexpected additional configuration data (%u)", ga_specific_config);
            return false;
         }
      }
      break;
   /* Add any further supported codecs here */
   default:
      LOG_DEBUG(p_ctx, "MPEG-4: Unsupported Audio Object Type (%u)", audio_object_type);
      return false;
   }

   return true;
}

/**************************************************************************//**
 * Get, store and decode the configuration information from the URI parameters.
 *
 * @param p_ctx   The RTP container context.
 * @param track   The track being constructed.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_get_config(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   MP4_PAYLOAD_T *extra = (MP4_PAYLOAD_T *)track->priv->module->extra;
   PARAMETER_T param;
   uint32_t config_len;
   VC_CONTAINER_STATUS_T status;
   uint8_t *config;
   VC_CONTAINER_BITS_T bit_stream;

   param.name = "config";
   if (!vc_containers_list_find_entry(params, &param) || !param.value)
   {
      LOG_ERROR(p_ctx, "MPEG-4: config parameter missing");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   config_len = strlen(param.value);
   if (config_len & 1)
   {
      LOG_ERROR(p_ctx, "MPEG-4: config parameter invalid");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }
   config_len /= 2;

   /* Copy AudioSpecificConfig into track extradata, to be decoded by client */
   status = vc_container_track_allocate_extradata(p_ctx, track, config_len);
   if(status != VC_CONTAINER_SUCCESS) return status;

   config = track->priv->extradata;
   track->format->extradata_size = config_len;
   hex_to_byte_buffer(param.value, config, config_len);

   /* Decode config locally, to determine sample rate, etc. */
   BITS_INIT(p_ctx, &bit_stream, config, config_len);

   switch (extra->stream_type)
   {
   case MPEG4_AUDIO_STREAM:
      if (!mp4_decode_audio_config(p_ctx, track, &bit_stream))
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      break;
   default:
      /* Other stream types not yet supported */
      LOG_ERROR(p_ctx, "MPEG-4: stream type %d not supported", extra->stream_type);
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * MP4 mode comparison function.
 * Compare two MP4 mode structures and return whether the first is less than,
 * equal to or greater than the second.
 *
 * @param first   The first structure to be compared.
 * @param second  The second structure to be compared.
 * @return  Negative if first is less than second, positive if first is greater
 *          and zero if they are equal.
 */
static int mp4_mode_comparator(const MP4_MODE_ENTRY_T *a, const MP4_MODE_ENTRY_T *b)
{
   return strcasecmp(a->name, b->name);
}

/**************************************************************************//**
 * Get and store the MP4 mode, if recognised, from the URI parameters.
 *
 * @param p_ctx   The RTP container context.
 * @param track   The track being constructed.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_get_mode(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   MP4_PAYLOAD_T *extra = (MP4_PAYLOAD_T *)track->priv->module->extra;
   PARAMETER_T param;
   MP4_MODE_ENTRY_T mode_entry;

   param.name = "mode";
   if (!vc_containers_list_find_entry(params, &param) || !param.value)
   {
      LOG_ERROR(p_ctx, "MPEG-4: mode parameter missing");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

#ifdef RTP_DEBUG
   vc_containers_list_validate(&mp4_mode_lookup);
#endif

   mode_entry.name = param.value;
   if (!vc_containers_list_find_entry(&mp4_mode_lookup, &mode_entry))
   {
      LOG_ERROR(p_ctx, "MPEG-4: Unrecognised mode parameter \"%s\"", mode_entry.name);
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   extra->mode = mode_entry.mode;

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Check URI parameters for unsupported features.
 *
 * @param p_ctx   The RTP container context.
 * @param track   The track being constructed.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_check_unsupported_features(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   uint32_t u32_unused;

   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(track);

   /* Limitation: RAP flag not yet supported */
   if (rtp_get_parameter_u32(params, "randomAccessIndication", &u32_unused))
   {
      LOG_ERROR(p_ctx, "MPEG-4: random access not supported");
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   /* Limitation: interleaving not yet supported */
   if (rtp_get_parameter_u32(params, "maxDisplacement", &u32_unused) ||
         rtp_get_parameter_u32(params, "de-interleaveBufferSize", &u32_unused))
   {
      LOG_ERROR(p_ctx, "MPEG-4: interleaved packetization not supported");
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   /* Limitation: system streams not supported */
   if (rtp_get_parameter_u32(params, "streamStateIndication", &u32_unused))
   {
      LOG_ERROR(p_ctx, "MPEG-4: system streams not supported");
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Validate parameters that have been read form the URI parameter list.
 *
 * @param p_ctx   The RTP container context.
 * @param track   The track being constructed.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_check_parameters(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track)
{
   MP4_PAYLOAD_T *extra = (MP4_PAYLOAD_T *)track->priv->module->extra;

   switch (extra->mode)
   {
   case MP4_CELP_CBR_MODE:
      if (!extra->constant_size)
      {
         LOG_ERROR(p_ctx, "MPEG-4: CELP-cbr requires constantSize parameter.");
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }
      break;
   case MP4_CELP_VBR_MODE:
   case MP4_AAC_LBR_MODE:
      if (extra->size_length != 6 || extra->index_length != 2 || extra->index_delta_length != 2)
      {
         LOG_ERROR(p_ctx, "MPEG-4: CELP-vbr/AAC-lbr invalid lengths (%u/%u/%u)",
               extra->size_length, extra->index_length, extra->index_delta_length);
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }
      break;
   case MP4_AAC_HBR_MODE:
      if (extra->size_length != 13 || extra->index_length != 3 || extra->index_delta_length != 3)
      {
         LOG_ERROR(p_ctx, "MPEG-4: AAC-hbr invalid lengths (%u/%u/%u)",
               extra->size_length, extra->index_length, extra->index_delta_length);
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }
      break;
   default: /* MP4_GENERIC_MODE */
      if (extra->size_length > 32 || extra->index_length > 32 || extra->index_delta_length > 32)
      {
         LOG_ERROR(p_ctx, "MPEG-4: generic invalid lengths (%u/%u/%u)",
               extra->size_length, extra->index_length, extra->index_delta_length);
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }
   }

   if (extra->cts_delta_length > 32 || extra->dts_delta_length > 32)
   {
      LOG_ERROR(p_ctx, "MPEG-4: CTS/DTS invalid lengths (%u/%u)",
            extra->cts_delta_length, extra->dts_delta_length);
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Initialise payload bit stream for a new RTP packet.
 *
 * @param p_ctx      The RTP container context.
 * @param t_module   The track module with the new RTP packet.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_new_rtp_packet(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module)
{
   VC_CONTAINER_BITS_T *payload = &t_module->payload;
   MP4_PAYLOAD_T *extra = (MP4_PAYLOAD_T *)t_module->extra;
   VC_CONTAINER_BITS_T *au_headers = &extra->au_headers;

   /* There will be an AU header section if any of its fields are non-zero. */
   if (extra->size_length || extra->index_length || extra->cts_delta_length || extra->dts_delta_length)
   {
      uint32_t au_headers_length;

      /* Calculate how far to advance the payload, to get past the AU headers */
      au_headers_length = BITS_READ_U32(p_ctx, payload, 16, "AU headers length");
      au_headers_length = BITS_TO_BYTES(au_headers_length); /* Round up to bytes */

      /* Record where the AU headers are in the payload */
      BITS_INIT(p_ctx, au_headers, BITS_CURRENT_POINTER(p_ctx, payload), au_headers_length);
      BITS_SKIP_BYTES(p_ctx, &t_module->payload, au_headers_length, "Move payload past AU headers");
   }

   /* Skip the auxiliary section, if present */
   if (extra->auxiliary_length)
   {
      uint32_t auxiliary_data_size;

      auxiliary_data_size = BITS_READ_U32(p_ctx, payload, extra->auxiliary_length, "Auxiliary length");
      auxiliary_data_size = BITS_TO_BYTES(auxiliary_data_size); /* Round up to bytes */
      BITS_SKIP_BYTES(p_ctx, payload, auxiliary_data_size, "Auxiliary data");
   }

   return BITS_VALID(p_ctx, payload) ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FORMAT_INVALID;
}

/**************************************************************************//**
 * Read a flagged delta from an AU header bit stream.
 * A flagged delta is an optional value in the stream that is preceded by a
 * flag bit that indicates whether the value is present in the stream. If the
 * length of the value is zero bits, the flag is never present.
 *
 * @pre The delta_length must be 32 or less.
 *
 * @param p_ctx         The container context.
 * @param au_headers    The AU header bit stream.
 * @param delta_length  The number of bits in the delta value.
 * @return  The delta value, or zero if not present.
 */
static int32_t mp4_flagged_delta(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_BITS_T *au_headers,
      uint32_t delta_length)
{
   uint32_t value = 0;

   /* Flag is only present if the delta length is non-zero */
   if (delta_length && BITS_READ_U32(p_ctx, au_headers, 1, "CTS/DTS delta present"))
   {
      value = BITS_READ_U32(p_ctx, au_headers, delta_length, "CTS/DTS delta");

      /* Sign extend value based on bit length */
      if (value & (1 << (delta_length - 1)))
         value |= ~((1 << delta_length) - 1);
   }

   return (int32_t)value;
}

/**************************************************************************//**
 * Read next AU header from the bit stream.
 *
 * @param p_ctx         The RTP container context.
 * @param extra         The MP4-specific track module information.
 * @param is_first_au   True if the first AU header in the packet is being read.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_next_au_header(VC_CONTAINER_T *p_ctx,
      MP4_PAYLOAD_T *extra,
      bool is_first_au)
{
   VC_CONTAINER_BITS_T *au_headers = &extra->au_headers;
   AU_INFO_T *au_info = &extra->au_info;

   /* See RFC3550 section 3.2.1.1 */

   if (extra->constant_size)
      au_info->available = extra->constant_size;
   else
      au_info->available = BITS_READ_U32(p_ctx, au_headers, extra->size_length, "AU size");

   if (is_first_au)
      au_info->index = BITS_READ_U32(p_ctx, au_headers, extra->index_length, "AU index");
   else
      au_info->index += BITS_READ_U32(p_ctx, au_headers, extra->index_delta_length, "AU index delta") + 1;

   au_info->cts_delta = mp4_flagged_delta(p_ctx, au_headers, extra->cts_delta_length);
   au_info->dts_delta = mp4_flagged_delta(p_ctx, au_headers, extra->dts_delta_length);

   /* RAP and stream state not supported yet */

   return BITS_VALID(p_ctx, au_headers) ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FORMAT_INVALID;
}

/**************************************************************************//**
 * MP4 payload handler.
 * Extracts/skips data from the payload according to the AU headers.
 *
 * @param p_ctx      The RTP container context.
 * @param track      The track being read.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T mp4_payload_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      VC_CONTAINER_PACKET_T *p_packet,
      uint32_t flags)
{
   VC_CONTAINER_TRACK_MODULE_T *t_module = track->priv->module;
   VC_CONTAINER_BITS_T *payload = &t_module->payload;
   MP4_PAYLOAD_T *extra = (MP4_PAYLOAD_T *)t_module->extra;
   AU_INFO_T *au_info = &extra->au_info;
   bool is_new_packet = BIT_IS_SET(t_module->flags, TRACK_NEW_PACKET);
   uint32_t bytes_left_in_payload;
   uint32_t size;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   if (is_new_packet)
   {
      status = mp4_new_rtp_packet(p_ctx, t_module);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   if (!au_info->available)
   {
      status = mp4_next_au_header(p_ctx, extra, is_new_packet);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   if (p_packet)
   {
      /* Adjust the packet time stamps using deltas */
      p_packet->pts += au_info->cts_delta;
      p_packet->dts += au_info->dts_delta;
   }

   size = au_info->available;
   bytes_left_in_payload = BITS_BYTES_AVAILABLE(p_ctx, payload);
   if (size > bytes_left_in_payload)
   {
      /* AU is fragmented across RTP packets */
      size = bytes_left_in_payload;
   }

   if (p_packet && !(flags & VC_CONTAINER_READ_FLAG_SKIP))
   {
      if (!(flags & VC_CONTAINER_READ_FLAG_INFO))
      {
         if (size > p_packet->buffer_size)
            size = p_packet->buffer_size;

         BITS_COPY_BYTES(p_ctx, payload, size, p_packet->data, "Packet data");
      }
      p_packet->size = size;
   } else {
      BITS_SKIP_BYTES(p_ctx, payload, size, "Packet data");
   }

   if (!(flags & VC_CONTAINER_READ_FLAG_INFO))
      au_info->available -= size;

   return BITS_VALID(p_ctx, payload) ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FORMAT_INVALID;
}

/*****************************************************************************
Functions exported as part of the RTP parameter handler API
 *****************************************************************************/

/**************************************************************************//**
 * MP4 parameter handler.
 * Parses the URI parameters to set up the track for an MP4 stream.
 *
 * @param p_ctx   The reader context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T mp4_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   MP4_PAYLOAD_T *extra;
   VC_CONTAINER_STATUS_T status;

   /* See RFC3640, section 4.1, for parameter names and details. */
   extra = (MP4_PAYLOAD_T *)malloc(sizeof(MP4_PAYLOAD_T));
   if (!extra)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   track->priv->module->extra = extra;
   memset(extra, 0, sizeof(MP4_PAYLOAD_T));

   /* Mandatory parameters */
   status = mp4_get_stream_type(p_ctx, track, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_get_config(p_ctx, track, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   status = mp4_get_mode(p_ctx, track, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* Unsupported parameters */
   status = mp4_check_unsupported_features(p_ctx, track, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* Optional parameters */
   rtp_get_parameter_u32(params, "sizeLength", &extra->size_length);
   rtp_get_parameter_u32(params, "indexLength", &extra->index_length);
   rtp_get_parameter_u32(params, "indexDeltaLength", &extra->index_delta_length);
   rtp_get_parameter_u32(params, "CTSDeltaLength", &extra->cts_delta_length);
   rtp_get_parameter_u32(params, "DTSDeltaLength", &extra->dts_delta_length);
   rtp_get_parameter_u32(params, "objectType", &extra->object_type);
   rtp_get_parameter_u32(params, "constantSize", &extra->constant_size);
   rtp_get_parameter_u32(params, "constantDuration", &extra->constant_duration);
   rtp_get_parameter_u32(params, "auxiliaryDataSizeLength", &extra->auxiliary_length);

   if (extra->constant_size && extra->size_length)
   {
      LOG_ERROR(p_ctx, "MPEG4: constantSize and sizeLength cannot both be set.");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   status = mp4_check_parameters(p_ctx, track);
   if (status != VC_CONTAINER_SUCCESS) return status;

   track->priv->module->payload_handler = mp4_payload_handler;

   return VC_CONTAINER_SUCCESS;
}
