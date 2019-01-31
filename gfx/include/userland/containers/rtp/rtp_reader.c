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

#define CONTAINER_IS_BIG_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define CONTAINER_HELPER_LOG_INDENT(a) 0
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_uri.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_bits.h"
#include "containers/core/containers_list.h"

#include "rtp_priv.h"
#include "rtp_mpeg4.h"
#include "rtp_h264.h"

#ifdef _DEBUG
/* Validates static sorted lists are correctly constructed */
#define RTP_DEBUG 1
#endif

/******************************************************************************
Configurable defines and constants.
******************************************************************************/

/** Maximum size of an RTP packet */
#define MAXIMUM_PACKET_SIZE   2048

/** Maximum number of RTP packets that can be missed without restarting. */
#define MAX_DROPOUT           3000
/** Maximum number of out of sequence RTP packets that are accepted. */
#define MAX_MISORDER          0
/** Minimum number of sequential packets required for an acceptable connection
 * when restarting. */
#define MIN_SEQUENTIAL        2

/******************************************************************************
Defines and constants.
******************************************************************************/

#define RTP_SCHEME                     "rtp:"

/** The RTP PKT scheme is used with test pkt files */
#define RTP_PKT_SCHEME                     "rtppkt:"

/** \name RTP URI parameter names
 * @{ */
#define PAYLOAD_TYPE_NAME              "rtppt"
#define MIME_TYPE_NAME                 "mime-type"
#define CHANNELS_NAME                  "channels"
#define RATE_NAME                      "rate"
#define SSRC_NAME                      "ssrc"
#define SEQ_NAME                       "seq"
/* @} */

/** A sentinel codec that is not supported */
#define UNSUPPORTED_CODEC              VC_FOURCC(0,0,0,0)

/** Evaluates to true if the given payload type is in the supported static audio range. */
#define IS_STATIC_AUDIO_TYPE(PT)       ((PT) < countof(static_audio_payload_types))

/** Payload type number for the first static video type */
#define FIRST_STATIC_VIDEO_TYPE   24
/** Evaluates to true if the given payload type is in the supported static video range. */
#define IS_STATIC_VIDEO_TYPE(PT)       ((PT) >= FIRST_STATIC_VIDEO_TYPE && \
                                        (PT) < (FIRST_STATIC_VIDEO_TYPE + countof(static_video_payload_types)))

/** Evaluates to true if the given payload type is in the dynamic range. */
#define IS_DYNAMIC_TYPE(PT)            ((PT) >= 96 && (PT) < 128)

/** All sequence numbers are modulo this value. */
#define RTP_SEQ_MOD                    (1 << 16)

/** All the static video payload types use a 90kHz timestamp clock */
#define STATIC_VIDEO_TIMESTAMP_CLOCK   90000

/** Number of microseconds in a second, used to convert RTP timestamps to microseconds */
#define MICROSECONDS_PER_SECOND        1000000

/******************************************************************************
Type definitions
******************************************************************************/

/** \name MIME type parameter handlers
 * Function prototypes for payload parameter handlers */
/* @{ */
static VC_CONTAINER_STATUS_T audio_parameter_handler(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);
static VC_CONTAINER_STATUS_T l8_parameter_handler(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);
static VC_CONTAINER_STATUS_T l16_parameter_handler(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);
/* @} */

/** \name MIME type payload handlers */
/* @{ */
static VC_CONTAINER_STATUS_T l16_payload_handler(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track,
      VC_CONTAINER_PACKET_T *p_packet, uint32_t flags);
/* @} */

/** Static audio payload type data */
typedef struct audio_payload_type_data_tag
{
   VC_CONTAINER_FOURCC_T codec;        /**< FourCC codec for this payload type */
   uint32_t channels;                  /**< Number of audio channels */
   uint32_t sample_rate;               /**< Sample rate */
   uint32_t bits_per_sample;           /**< Bits per sample, or 1 if not applicable */
   PARAMETER_HANDLER_T param_handler;  /**< Optional parameter handler */
   PAYLOAD_HANDLER_T payload_handler;  /**< Optional payload handler */
} AUDIO_PAYLOAD_TYPE_DATA_T;

/** The details for the statically defined audio payload types from RFC3551 */
static AUDIO_PAYLOAD_TYPE_DATA_T static_audio_payload_types[] =
{
   { VC_CONTAINER_CODEC_MULAW, 1, 8000,  8,  audio_parameter_handler, NULL },                /*  0 - PCMU */
   { UNSUPPORTED_CODEC },                                                                    /*  1 - reserved */
   { UNSUPPORTED_CODEC },                                                                    /*  2 - reserved */
   { UNSUPPORTED_CODEC,        1, 8000,  1,  NULL,                    NULL },                /*  3 - GSM */
   { UNSUPPORTED_CODEC,        1, 8000,  1,  NULL,                    NULL },                /*  4 - G723 */
   { UNSUPPORTED_CODEC,        1, 8000,  4,  NULL,                    NULL },                /*  5 - DVI4 */
   { UNSUPPORTED_CODEC,        1, 16000, 4,  NULL,                    NULL },                /*  6 - DVI4 */
   { UNSUPPORTED_CODEC,        1, 8000,  1,  NULL,                    NULL },                /*  7 - LPC */
   { VC_CONTAINER_CODEC_ALAW,  1, 8000,  8,  audio_parameter_handler, NULL },                /*  8 - PCMA */
   { UNSUPPORTED_CODEC,        1, 8000,  8,  NULL,                    NULL },                /*  9 - G722 */
   { VC_CONTAINER_CODEC_PCM_SIGNED,   2, 44100, 16, audio_parameter_handler, l16_payload_handler }, /* 10 - L16 */
   { VC_CONTAINER_CODEC_PCM_SIGNED,   1, 44100, 16, audio_parameter_handler, l16_payload_handler }, /* 11 - L16 */
   { VC_CONTAINER_CODEC_QCELP, 1, 8000,  16, NULL,                    NULL },                /* 12 - QCELP */
   { UNSUPPORTED_CODEC,        1, 8000,  1,  NULL,                    NULL },                /* 13 - CN */
   { VC_CONTAINER_CODEC_MPGA,  1, 90000, 1,  NULL,                    NULL },                /* 14 - MPA */
   { UNSUPPORTED_CODEC,        1, 8000,  1,  NULL,                    NULL },                /* 15 - G728 */
   { UNSUPPORTED_CODEC,        1, 11025, 4,  NULL,                    NULL },                /* 16 - DVI4 */
   { UNSUPPORTED_CODEC,        1, 22050, 4,  NULL,                    NULL },                /* 17 - DVI4 */
   { UNSUPPORTED_CODEC,        1, 8000,  1,  NULL,                    NULL },                /* 18 - G729 */
};

/** Static video payload type data */
typedef struct video_payload_type_data_tag
{
   VC_CONTAINER_FOURCC_T codec;        /**< FourCC codec for this payload type */
   PARAMETER_HANDLER_T param_handler;  /**< Optional parameter handler */
   PAYLOAD_HANDLER_T payload_handler;  /**< Optional payload handler */
} VIDEO_PAYLOAD_TYPE_DATA_T;

/** The details for the statically defined video payload types from RFC3551 */
static VIDEO_PAYLOAD_TYPE_DATA_T static_video_payload_types[] =
{
   { UNSUPPORTED_CODEC },                    /* 24 - unassigned */
   { UNSUPPORTED_CODEC },                    /* 25 - CelB */
   { UNSUPPORTED_CODEC },                    /* 26 - JPEG */
   { UNSUPPORTED_CODEC },                    /* 27 - unassigned */
   { UNSUPPORTED_CODEC },                    /* 28 - nv */
   { UNSUPPORTED_CODEC },                    /* 29 - unassigned */
   { UNSUPPORTED_CODEC },                    /* 30 - unassigned */
   { UNSUPPORTED_CODEC },                    /* 31 - H261 */
   { VC_CONTAINER_CODEC_MP2V, NULL, NULL },  /* 32 - MPV */
   { UNSUPPORTED_CODEC },                    /* 33 - MP2T */
   { VC_CONTAINER_CODEC_H263, NULL, NULL }   /* 34 - H263 */
};

/** MIME type details */
typedef struct mime_type_data_tag
{
   const char *name;                   /**< Name of MIME type */
   VC_CONTAINER_ES_TYPE_T es_type;     /**< Elementary stream type */
   VC_CONTAINER_FOURCC_T codec;        /**< Codec to be used */
   PARAMETER_HANDLER_T param_handler;  /**< Parameter handler for this MIME type */
} MIME_TYPE_DATA_T;

/** Comparator for MIME type details. */
static int mime_type_data_comparator(const MIME_TYPE_DATA_T *a, const MIME_TYPE_DATA_T *b);

/** Dynamic audio payload details
 * Note: case-insensitive sort by name */
static MIME_TYPE_DATA_T dynamic_mime_details[] = {
   { "audio/l16", VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_PCM_SIGNED, l16_parameter_handler },
   { "audio/l8", VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_PCM_SIGNED, l8_parameter_handler },
   { "audio/mpeg4-generic", VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_MP4A, mp4_parameter_handler },
   { "video/h264", VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H264, h264_parameter_handler },
   { "video/mpeg4-generic", VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MP4V, mp4_parameter_handler },
};

/** Sorted list of dynamic MIME type details */
VC_CONTAINERS_STATIC_LIST(dynamic_mime, dynamic_mime_details, mime_type_data_comparator);

/** RTP reader data. */
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T rtp_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

/**************************************************************************//**
 * Parameter comparison function.
 * Compare two parameter structures and return whether the first is less than,
 * equal to or greater than the second.
 *
 * @param first   The first structure to be compared.
 * @param second  The second structure to be compared.
 * @return  Negative if first is less than second, positive if first is greater
 *          and zero if they are equal.
 */
static int parameter_comparator(const PARAMETER_T *first, const PARAMETER_T *second)
{
   return strcasecmp(first->name, second->name);
}

/**************************************************************************//**
 * Creates and populates a parameter list from a URI structure.
 * The list does not copy the parameter strings themselves, so the URI structure
 * must be retained (and its parameters unmodified) while the list is in use.
 *
 * @param uri  The URI containing the parameters.
 * @return  List created from the parameters of the URI, or NULL on error.
 */
static VC_CONTAINERS_LIST_T *fill_parameter_list(VC_URI_PARTS_T *uri)
{
   uint32_t num_parameters = vc_uri_num_queries(uri);
   VC_CONTAINERS_LIST_T *parameters;
   uint32_t ii;

   parameters = vc_containers_list_create(num_parameters, sizeof(PARAMETER_T), (VC_CONTAINERS_LIST_COMPARATOR_T)parameter_comparator);
   if (!parameters)
      return NULL;

   for (ii = 0; ii < num_parameters; ii++)
   {
      PARAMETER_T param;

      vc_uri_query(uri, ii, &param.name, &param.value);
      if (!vc_containers_list_insert(parameters, &param, false))
      {
         vc_containers_list_destroy(parameters);
         return NULL;
      }
   }

#ifdef RTP_DEBUG
   vc_containers_list_validate(parameters);
#endif

   return parameters;
}

/**************************************************************************//**
 * Decodes a static audio payload type into track information.
 * The static parameters may be overridden by URI parameters.
 *
 * @param p_ctx         The reader context.
 * @param track         The track to be populated.
 * @param param_list    The URI parameter list.
 * @param payload_type  The static payload type.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T decode_static_audio_type(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *param_list,
      uint32_t payload_type)
{
   VC_CONTAINER_ES_FORMAT_T *format = track->format;
   AUDIO_PAYLOAD_TYPE_DATA_T *data = &static_audio_payload_types[payload_type];

   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(param_list);

   vc_container_assert(payload_type < countof(static_audio_payload_types));

   if (data->codec == UNSUPPORTED_CODEC)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   format->codec = data->codec;
   format->type->audio.channels = data->channels;
   format->type->audio.sample_rate = data->sample_rate;
   format->type->audio.bits_per_sample = data->bits_per_sample;
   format->type->audio.block_align = data->channels * BITS_TO_BYTES(data->bits_per_sample);
   track->priv->module->timestamp_clock = format->type->audio.sample_rate;
   track->priv->module->payload_handler = data->payload_handler;

   if (data->param_handler)
      data->param_handler(p_ctx, track, param_list);

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Decodes a static video payload type into track information.
 * The static parameters may be overridden by URI parameters.
 *
 * @param p_ctx         The reader context.
 * @param track         The track to be populated.
 * @param param_list    The URI parameter list.
 * @param payload_type  The static payload type.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T decode_static_video_type(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *param_list,
      uint32_t payload_type)
{
   VC_CONTAINER_ES_FORMAT_T *format = track->format;
   VIDEO_PAYLOAD_TYPE_DATA_T *data = &static_video_payload_types[payload_type - FIRST_STATIC_VIDEO_TYPE];

   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(param_list);

   vc_container_assert(payload_type >= FIRST_STATIC_VIDEO_TYPE);
   vc_container_assert(payload_type < FIRST_STATIC_VIDEO_TYPE + countof(static_video_payload_types));

   if (data->codec == UNSUPPORTED_CODEC)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   format->codec = data->codec;
   track->priv->module->timestamp_clock = STATIC_VIDEO_TIMESTAMP_CLOCK;
   track->priv->module->payload_handler = data->payload_handler;

   if (data->param_handler)
      data->param_handler(p_ctx, track, param_list);

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Compare two MIME type structures and return whether the first is less than,
 * equal to or greater than the second.
 *
 * @param a The first parameter structure to be compared.
 * @param b The second parameter structure to be compared.
 * @return  Negative if a is less than b, positive if a is greater and zero if
 *          they are equal.
 */
static int mime_type_data_comparator(const MIME_TYPE_DATA_T *a, const MIME_TYPE_DATA_T *b)
{
   return strcasecmp(a->name, b->name);
}

/**************************************************************************//**
 * Generic audio parameter handler.
 * Updates the track information with generic audio parameters, such as "rate"
 * and "channels".
 *
 * @param p_ctx   The reader context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T audio_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   VC_CONTAINER_AUDIO_FORMAT_T *audio = &track->format->type->audio;

   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   /* See RFC3555. Generic audio parameters that can override static payload
    * type defaults. */
   if (rtp_get_parameter_u32(params, RATE_NAME, &audio->sample_rate))
      track->priv->module->timestamp_clock = audio->sample_rate;
   if (rtp_get_parameter_u32(params, CHANNELS_NAME, &audio->channels))
      audio->block_align = audio->channels * BITS_TO_BYTES(audio->bits_per_sample);

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * L8 specific audio parameter handler.
 * Updates the track information with audio parameters needed by the audio/L8
 * MIME type. For example, the "rate" parameter is mandatory.
 *
 * @param p_ctx   The reader context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T l8_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   VC_CONTAINER_AUDIO_FORMAT_T *audio = &track->format->type->audio;

   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   /* See RFC3555, section 4.1.14, for parameter names and details. */
   if (!rtp_get_parameter_u32(params, RATE_NAME, &audio->sample_rate))
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   if (!rtp_get_parameter_u32(params, CHANNELS_NAME, &audio->channels))
      audio->channels = 1;
   audio->bits_per_sample = 8;
   audio->block_align = audio->channels;
   track->priv->module->timestamp_clock = audio->sample_rate;

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * L16 specific audio parameter handler.
 * Updates the track information with audio parameters needed by the audio/L16
 * MIME type. For example, the "rate" parameter is mandatory.
 *
 * @param p_ctx   The reader context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T l16_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   VC_CONTAINER_AUDIO_FORMAT_T *audio = &track->format->type->audio;

   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   /* See RFC3555, section 4.1.15, for parameter names and details. */
   if (!rtp_get_parameter_u32(params, RATE_NAME, &audio->sample_rate))
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   if (!rtp_get_parameter_u32(params, CHANNELS_NAME, &audio->channels))
      audio->channels = 1;
   audio->bits_per_sample = 16;
   audio->block_align = audio->channels * 2;
   track->priv->module->timestamp_clock = audio->sample_rate;
   track->priv->module->payload_handler = l16_payload_handler;

   /* TODO: add support for "channel-order" to set channel mapping */

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Decode a dynamic payload type from parameters.
 * Populates the track information with data for supported dynamic media types.
 *
 * @param p_ctx      The reader context.
 * @param track      The track to be updated.
 * @param param_list The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T decode_dynamic_type(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *param_list)
{
   VC_CONTAINER_ES_FORMAT_T *format = track->format;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   PARAMETER_T mime_type;
   MIME_TYPE_DATA_T mime_details;

   /* Get MIME type parameter */
   mime_type.name = MIME_TYPE_NAME;
   if (!vc_containers_list_find_entry(param_list, &mime_type))
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

#ifdef RTP_DEBUG
   vc_containers_list_validate(&dynamic_mime);
#endif

   /* Look up MIME type to see if it can be handled */
   mime_details.name = mime_type.value;
   if (!vc_containers_list_find_entry(&dynamic_mime, &mime_details))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   format->codec = mime_details.codec;
   format->es_type = mime_details.es_type;

   /* Default number of channels for audio is one */
   if (mime_details.es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      format->type->audio.channels = 1;

   /* Lete MIME type specific parameter handler deal with any other parameters */
   status = mime_details.param_handler(p_ctx, track, param_list);

   /* Ensure that the sample rate has been set for audio formats */
   if (mime_details.es_type == VC_CONTAINER_ES_TYPE_AUDIO && !format->type->audio.sample_rate)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   return status;
}

/**************************************************************************//**
 * Decode the RTP payload type.
 * Populates track information with data from static tables and the URI
 * parameter list, according to the payload and MIME types.
 *
 * @param p_ctx   The reader context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T  decode_payload_type(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *param_list,
      uint32_t payload_type)
{
   VC_CONTAINER_TRACK_MODULE_T *module = track->priv->module;
   VC_CONTAINER_STATUS_T status;

   if (IS_STATIC_AUDIO_TYPE(payload_type))
      status = decode_static_audio_type(p_ctx, track, param_list, payload_type);
   else if (IS_STATIC_VIDEO_TYPE(payload_type))
      status = decode_static_video_type(p_ctx, track, param_list, payload_type);
   else if (IS_DYNAMIC_TYPE(payload_type))
      status = decode_dynamic_type(p_ctx, track, param_list);
   else
      status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   module->payload_type = (uint8_t)payload_type;

   return status;
}

/**************************************************************************//**
 * Initialises the RTP sequence number algorithm with a new sequence number.
 *
 * @param t_module   The track module.
 * @param seq        The new sequence number.
 */
static void init_sequence_number(VC_CONTAINER_TRACK_MODULE_T *t_module,
      uint16_t seq)
{
   t_module->base_seq = seq;
   t_module->max_seq_num = seq;
   t_module->bad_seq = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
   t_module->received = 0;
}

/**************************************************************************//**
 * Checks whether the sequence number for a packet is acceptable or not.
 * The packet will be unacceptable if it is out of sequence by some degree, or
 * if the packet sequence is still being established.
 *
 * @param t_module   The track module.
 * @param seq        The new sequence number.
 * @return  True if the sequence number indicates the packet is acceptable
 */
static bool update_sequence_number(VC_CONTAINER_TRACK_MODULE_T *t_module,
      uint16_t seq)
{
   uint16_t udelta = seq - t_module->max_seq_num;

   /* NOTE: This source is derived from the example code in RFC3550, section A.1 */

   /* Source is not valid until MIN_SEQUENTIAL packets with
    * sequential sequence numbers have been received. */
   if (t_module->probation)
   {
      /* packet is in sequence */
      if (seq == t_module->max_seq_num + 1)
      {
         t_module->probation--;
         t_module->max_seq_num = seq;
         LOG_INFO(0, "RTP: Probation, %u more packet(s) to go at 0x%4.4hx", t_module->probation, seq);

         if (!t_module->probation)
         {
            init_sequence_number(t_module, seq);
            t_module->received++;
            return 1;
         }
      } else {
         t_module->probation = MIN_SEQUENTIAL - 1;
         t_module->max_seq_num = seq;
         LOG_INFO(0, "RTP: Probation reset, wait for %u packet(s) at 0x%4.4hx", t_module->probation, seq);
      }
      return 0;
   } else if (udelta < MAX_DROPOUT)
   {
      if (!udelta)
      {
         /* Duplicate packet, drop it */
         LOG_INFO(0, "RTP: Drop duplicate packet at 0x%4.4hx", seq);
         return 0;
      }
      if (udelta > 1)
      {
         LOG_INFO(0, "RTP: Jumped by %hu packets to 0x%4.4hx", udelta, seq);
      }
      /* in order, with permissible gap */
      t_module->max_seq_num = seq;
   } else
#if (MAX_MISORDER != 0)
      /* When MAX_MISORDER is zero, always treat as out of order */
      if (udelta <= RTP_SEQ_MOD - MAX_MISORDER)
#endif
      {
         /* the sequence number made a very large jump */
         if (seq == t_module->bad_seq)
         {
            LOG_INFO(0, "RTP: Misorder restart at 0x%4.4hx", seq);
            /* Two sequential packets -- assume that the other side
             * restarted without telling us so just re-sync
             * (i.e., pretend this was the first packet). */
            init_sequence_number(t_module, seq);
         } else {
            LOG_INFO(0, "RTP: Misorder at 0x%4.4hx, expected 0x%4.4hx", seq, t_module->max_seq_num);
            t_module->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
            return 0;
         }
      }
#if (MAX_MISORDER != 0)
   else {
      /* duplicate or reordered packet */

      /* TODO: handle out of order packets */
   }
#endif
   t_module->received++;
   return 1;
}

/**************************************************************************//**
 * Extract the fields of an RTP packet and validate it.
 *
 * @param p_ctx      The reader context.
 * @param t_module   The track module.
 * @return  True if successful, false if there were not enough bits in the
 *          packet or the packet was invalid.
 */
static void decode_rtp_packet_header(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module)
{
   VC_CONTAINER_BITS_T *payload = &t_module->payload;
   uint32_t version, has_padding, has_extension, csrc_count, has_marker;
   uint32_t payload_type, ssrc;
   uint16_t seq_num;

   /* Break down fixed header area into component parts */
   version              = BITS_READ_U32(p_ctx, payload, 2, "Version");
   has_padding          = BITS_READ_U32(p_ctx, payload, 1, "Has padding");
   has_extension        = BITS_READ_U32(p_ctx, payload, 1, "Has extension");
   csrc_count           = BITS_READ_U32(p_ctx, payload, 4, "CSRC count");
   has_marker           = BITS_READ_U32(p_ctx, payload, 1, "Has marker");
   payload_type         = BITS_READ_U32(p_ctx, payload, 7, "Payload type");
   seq_num              = BITS_READ_U16(p_ctx, payload, 16, "Sequence number");
   t_module->timestamp  = BITS_READ_U32(p_ctx, payload, 32, "Timestamp");
   ssrc                 = BITS_READ_U32(p_ctx, payload, 32, "SSRC");

   /* If there was only a partial header, abort immediately */
   if (!BITS_VALID(p_ctx, payload))
      return;

   /* Validate version, payload type, sequence number and SSRC, if set */
   if (version != 2 || payload_type != t_module->payload_type)
   {
      BITS_INVALIDATE(p_ctx, payload);
      return;
   }
   if (BIT_IS_SET(t_module->flags, TRACK_SSRC_SET) && (ssrc != t_module->expected_ssrc))
   {
      LOG_DEBUG(p_ctx, "RTP: Unexpected SSRC (0x%8.8X)", ssrc);
      BITS_INVALIDATE(p_ctx, payload);
      return;
   }

   /* Check sequence number indicates packet is usable */
   if (!update_sequence_number(t_module, seq_num))
   {
      BITS_INVALIDATE(p_ctx, payload);
      return;
   }

   /* Adjust to account for padding, CSRCs and extension */
   if (has_padding)
   {
      VC_CONTAINER_BITS_T bit_stream;
      uint32_t available = BITS_BYTES_AVAILABLE(p_ctx, payload);
      uint8_t padding;

      BITS_COPY_STREAM(p_ctx, &bit_stream, payload);
      /* The last byte of the payload is the number of padding bytes, including itself */
      BITS_SKIP_BYTES(p_ctx, &bit_stream, available - 1, "Skip to padding length");
      padding = BITS_READ_U8(p_ctx, &bit_stream, 8, "Padding length");

      BITS_REDUCE_BYTES(p_ctx, payload, padding, "Remove padding");
   }

   /* Each CSRC is 32 bits, so shift count up to skip the right number of bits */
   BITS_SKIP(p_ctx, payload, csrc_count << 5, "CSRC section");

   if (has_extension)
   {
      uint32_t extension_bits;

      /* Extension header is 16-bit ID (which isn't needed), then 16-bit length in 32-bit words */
      BITS_SKIP(p_ctx, payload, 16, "Extension ID");
      extension_bits = BITS_READ_U32(p_ctx, payload, 16, "Extension length") << 5;
      BITS_SKIP(p_ctx, payload, extension_bits, "Extension data");
   }

   /* Record whether or not this RTP packet had the marker bit set */
   if (has_marker)
      SET_BIT(t_module->flags, TRACK_HAS_MARKER);
   else
      CLEAR_BIT(t_module->flags, TRACK_HAS_MARKER);

   /* If it hasn't been set independently, use the first timestamp as a baseline */
   if (!t_module->timestamp_base)
      t_module->timestamp_base = t_module->timestamp;
   t_module->timestamp -= t_module->timestamp_base;
}

/**************************************************************************//**
 * Generic payload handler.
 * Copies/skips data verbatim from the packet payload.
 *
 * @param p_ctx      The reader context.
 * @param track      The track being read.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T generic_payload_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      VC_CONTAINER_PACKET_T *p_packet,
      uint32_t flags)
{
   VC_CONTAINER_TRACK_MODULE_T *t_module = track->priv->module;
   VC_CONTAINER_BITS_T *payload = &t_module->payload;
   uint32_t size;

   VC_CONTAINER_PARAM_UNUSED(p_ctx);

   if (!p_packet)
   {
      /* Skip the rest of this RTP packet */
      BITS_INVALIDATE(p_ctx, payload);
      return VC_CONTAINER_SUCCESS;
   }

   /* Copy as much as possible into the client packet buffer */
   size = BITS_BYTES_AVAILABLE(p_ctx, payload);

   if (flags & VC_CONTAINER_READ_FLAG_SKIP)
      BITS_SKIP_BYTES(p_ctx, payload, size, "Packet data");
   else {
      if (!(flags & VC_CONTAINER_READ_FLAG_INFO))
      {
         if (size > p_packet->buffer_size)
            size = p_packet->buffer_size;

         BITS_COPY_BYTES(p_ctx, payload, size, p_packet->data, "Packet data");
      }
      p_packet->size = size;
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * L16 payload handler.
 * Copies/skips data from the packet payload. On copy, swaps consecutive bytes
 * in the data in order to get expected byte order.
 *
 * @param p_ctx      The reader context.
 * @param track      The track being read.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T l16_payload_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      VC_CONTAINER_PACKET_T *p_packet,
      uint32_t flags)
{
   VC_CONTAINER_STATUS_T status;

   /* Most aspects are handled adequately by the generic handler */
   status = generic_payload_handler(p_ctx, track, p_packet, flags);
   if (status != VC_CONTAINER_SUCCESS)
      return status;

   if (p_packet && !(flags & (VC_CONTAINER_READ_FLAG_SKIP | VC_CONTAINER_READ_FLAG_INFO)))
   {
      uint8_t *ptr, *end_ptr;

      /* Ensure packet size is even */
      p_packet->size &= ~1;

      /* Swap bytes of each sample, to get host order instead of network order */
      for (ptr = p_packet->data, end_ptr = ptr + p_packet->size; ptr < end_ptr; ptr += 2)
      {
         uint8_t high_byte = ptr[0];
         ptr[0] = ptr[1];
         ptr[1] = high_byte;
      }
   }

   return status;
}


/*****************************************************************************
Utility functions for use by RTP payload handlers
 *****************************************************************************/

/**************************************************************************//**
 * Gets the value of a parameter as an unsigned 32-bit decimal integer.
 *
 * @param param_list The URI parameter list.
 * @param name       The name of the parameter.
 * @param value      Pointer to the variable to receive the value.
 * @return  True if the parameter value was read and stored correctly, false
 *          otherwise.
 */
bool rtp_get_parameter_u32(const VC_CONTAINERS_LIST_T *param_list,
      const char *name,
      uint32_t *value)
{
   PARAMETER_T param;

   param.name = name;
   if (vc_containers_list_find_entry(param_list, &param) && param.value)
   {
      char *end;

      *value = strtoul(param.value, &end, 10);
      return (end != param.value) && (*end == '\0');
   }

   return false;
}

/**************************************************************************//**
 * Gets the value of a parameter as an unsigned 32-bit hexadecimal integer.
 *
 * @param param_list The URI parameter list.
 * @param name       The name of the parameter.
 * @param value      Pointer to the variable to receive the value.
 * @return  True if the parameter value was read and stored correctly, false
 *          otherwise.
 */
bool rtp_get_parameter_x32(const VC_CONTAINERS_LIST_T *param_list,
      const char *name,
      uint32_t *value)
{
   PARAMETER_T param;

   param.name = name;
   if (vc_containers_list_find_entry(param_list, &param) && param.value)
   {
      char *end;

      *value = strtoul(param.value, &end, 16);
      return (end != param.value) && (*end == '\0');
   }

   return false;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/

/**************************************************************************//**
 * Read/skip data from the container.
 * Can also be used to query information about the next block of data.
 *
 * @param p_ctx      The reader context.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtp_reader_read( VC_CONTAINER_T *p_ctx,
                                               VC_CONTAINER_PACKET_T *p_packet,
                                               uint32_t flags )
{
   VC_CONTAINER_TRACK_T *track;
   VC_CONTAINER_TRACK_MODULE_T *t_module;
   VC_CONTAINER_STATUS_T status;

   if((flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK) && p_packet->track)
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   track = p_ctx->tracks[0];
   t_module = track->priv->module;

   CLEAR_BIT(t_module->flags, TRACK_NEW_PACKET);

   while (!BITS_AVAILABLE(p_ctx, &t_module->payload))
   {
      uint32_t bytes_read;

      /* No data left from last RTP packet, get another one */
      bytes_read = READ_BYTES(p_ctx, t_module->buffer, MAXIMUM_PACKET_SIZE);
      if (!bytes_read)
         return STREAM_STATUS(p_ctx);

      BITS_INIT(p_ctx, &t_module->payload, t_module->buffer, bytes_read);

      decode_rtp_packet_header(p_ctx, t_module);
      SET_BIT(t_module->flags, TRACK_NEW_PACKET);
   }

   if (p_packet)
   {
      uint32_t timestamp_top = t_module->timestamp >> 30;

      /* Determine whether timestamp has wrapped forwards or backwards around zero */
      if ((timestamp_top == 0) && (t_module->last_timestamp_top == 3))
         t_module->timestamp_wraps++;
      else if ((timestamp_top == 3) && (t_module->last_timestamp_top == 0))
         t_module->timestamp_wraps--;
      t_module->last_timestamp_top = timestamp_top;

      p_packet->dts = p_packet->pts = ((int64_t)t_module->timestamp_wraps << 32) | t_module->timestamp;
      p_packet->track = 0;
      p_packet->flags = 0;
   }

   status = t_module->payload_handler(p_ctx, track, p_packet, flags);
   if (p_packet && status == VC_CONTAINER_SUCCESS)
   {
      /* Adjust timestamps from RTP clock rate to microseconds */
      p_packet->pts = p_packet->pts * MICROSECONDS_PER_SECOND / t_module->timestamp_clock;
      p_packet->dts = p_packet->dts * MICROSECONDS_PER_SECOND / t_module->timestamp_clock;
   }

   STREAM_STATUS(p_ctx) = status;
   return status;
}

/**************************************************************************//**
 * Seek over data in the container.
 *
 * @param p_ctx      The reader context.
 * @param p_offset   The seek offset.
 * @param mode       The seek mode.
 * @param flags      The seek flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtp_reader_seek( VC_CONTAINER_T *p_ctx,
                                               int64_t *p_offset,
                                               VC_CONTAINER_SEEK_MODE_T mode,
                                               VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(p_offset);
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);

   /* RTP is a non-seekable container */
   return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/**************************************************************************//**
 * Apply a control operation to the container.
 *
 * @param p_ctx      The reader context.
 * @param operation  The control operation.
 * @param args       Optional additional arguments for the operation.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtp_reader_control( VC_CONTAINER_T *p_ctx,
                                                VC_CONTAINER_CONTROL_T operation,
                                                va_list args)
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_TRACK_MODULE_T *t_module = p_ctx->tracks[0]->priv->module;

   switch (operation)
   {
   case VC_CONTAINER_CONTROL_SET_TIMESTAMP_BASE:
      {
         t_module->timestamp_base = va_arg(args, uint32_t);
         if (!t_module->timestamp_base)
            t_module->timestamp_base = 1;    /* Zero is used to mean "not set" */
         status = VC_CONTAINER_SUCCESS;
      }
      break;
   case VC_CONTAINER_CONTROL_SET_NEXT_SEQUENCE_NUMBER:
      {
         init_sequence_number(t_module, (uint16_t)va_arg(args, uint32_t));
         t_module->probation = 0;
         status = VC_CONTAINER_SUCCESS;
      }
      break;
   case VC_CONTAINER_CONTROL_SET_SOURCE_ID:
      {
         t_module->expected_ssrc = va_arg(args, uint32_t);
         SET_BIT(t_module->flags, TRACK_SSRC_SET);
         status = VC_CONTAINER_SUCCESS;
      }
      break;
   default:
      status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }

   return status;
}

/**************************************************************************//**
 * Close the container.
 *
 * @param p_ctx   The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtp_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   vc_container_assert(p_ctx->tracks_num < 2);

   if (p_ctx->tracks_num)
   {
      void *payload_extra;

      vc_container_assert(module);
      vc_container_assert(module->track);
      vc_container_assert(module->track->priv);
      vc_container_assert(module->track->priv->module);

      payload_extra = module->track->priv->module->extra;
      if (payload_extra)
         free(payload_extra);
      vc_container_free_track(p_ctx, module->track);
   }
   p_ctx->tracks = NULL;
   p_ctx->tracks_num = 0;
   if (module) free(module);
   p_ctx->priv->module = 0;
   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Open the container.
 * Uses the I/O URI and/or data to configure the container.
 *
 * @param p_ctx   The reader context.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T rtp_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_TRACK_T *track = 0;
   VC_CONTAINER_TRACK_MODULE_T *t_module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINERS_LIST_T *parameters = NULL;
   uint32_t payload_type;
   uint32_t initial_seq_num;

   /* Check the URI scheme looks valid */
   if (!vc_uri_scheme(p_ctx->priv->uri) ||
       (strcasecmp(vc_uri_scheme(p_ctx->priv->uri), RTP_SCHEME) &&
        strcasecmp(vc_uri_scheme(p_ctx->priv->uri), RTP_PKT_SCHEME)))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Make the query/parameter list more easily searchable */
   parameters = fill_parameter_list(p_ctx->priv->uri);
   if (!parameters) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }

   /* Payload type parameter is mandatory and must fit in 7 bits */
   if (!rtp_get_parameter_u32(parameters, PAYLOAD_TYPE_NAME, &payload_type) || payload_type > 127)
   {
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      goto error;
   }

   /* Allocate our context */
   module = (VC_CONTAINER_MODULE_T *)malloc(sizeof(VC_CONTAINER_MODULE_T));
   if (!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }

   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = &module->track;

   /* Allocate the track, including space for reading an RTP packet on the end */
   track = vc_container_allocate_track(p_ctx, sizeof(VC_CONTAINER_TRACK_MODULE_T) + MAXIMUM_PACKET_SIZE);
   if (!track)
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }
   module->track = track;
   p_ctx->tracks_num = 1;
   t_module = track->priv->module;

   /* Initialise the track data */
   t_module->buffer = (uint8_t *)(t_module + 1);
   status = decode_payload_type(p_ctx, track, parameters, payload_type);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   vc_container_assert(t_module->timestamp_clock != 0);

   /* Default to a generic, unstructured payload handler */
   if (!t_module->payload_handler)
      t_module->payload_handler = generic_payload_handler;

   if (rtp_get_parameter_x32(parameters, SSRC_NAME, &t_module->expected_ssrc))
      SET_BIT(t_module->flags, TRACK_SSRC_SET);

   t_module->probation = MIN_SEQUENTIAL;
   if (rtp_get_parameter_u32(parameters, SEQ_NAME, &initial_seq_num))
   {
      /* If an initial sequence number is provided, avoid probation period */
      t_module->max_seq_num = (uint16_t)initial_seq_num;
      t_module->probation = 0;
   }

   track->is_enabled = true;

   vc_containers_list_destroy(parameters);

   p_ctx->priv->pf_close = rtp_reader_close;
   p_ctx->priv->pf_read = rtp_reader_read;
   p_ctx->priv->pf_seek = rtp_reader_seek;
   p_ctx->priv->pf_control = rtp_reader_control;

   return VC_CONTAINER_SUCCESS;

error:
   if (parameters) vc_containers_list_destroy(parameters);
   if(status == VC_CONTAINER_SUCCESS || status == VC_CONTAINER_ERROR_EOS)
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   LOG_DEBUG(p_ctx, "error opening RTP (%i)", status);
   rtp_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open rtp_reader_open
#endif
