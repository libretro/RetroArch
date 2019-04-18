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

#include "mmal.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "mmal_logging.h"

#include "containers/containers.h"
#include "containers/containers_codecs.h"
#include "containers/core/containers_utils.h"

#define READER_MAX_URI_LENGTH 1024

#define WRITER_PORTS_NUM 3 /**< 3 ports should be enough for video + audio + subpicture */

/* Buffering requirements */
#define READER_MIN_BUFFER_SIZE (2*1024)
#define READER_MIN_BUFFER_NUM 1
#define READER_RECOMMENDED_BUFFER_SIZE (32*1024)
#define READER_RECOMMENDED_BUFFER_NUM 10

/*****************************************************************************/

/** Private context for this component */
typedef struct MMAL_COMPONENT_MODULE_T
{
   VC_CONTAINER_T *container;
   char uri[READER_MAX_URI_LENGTH+1];
   unsigned int ports;

   MMAL_BOOL_T writer;
   MMAL_BOOL_T error;

   /* Reader specific */
   MMAL_BOOL_T packet_logged;

   /* Writer specific */
   unsigned int port_last_used;
   unsigned int port_writing_frame;

} MMAL_COMPONENT_MODULE_T;

typedef struct MMAL_PORT_MODULE_T
{
   unsigned int track;
   MMAL_QUEUE_T *queue;

   MMAL_BOOL_T flush;
   MMAL_BOOL_T eos;

   VC_CONTAINER_ES_FORMAT_T *format; /**< Format description for the elementary stream */

} MMAL_PORT_MODULE_T;

/*****************************************************************************/
static struct {
   VC_CONTAINER_FOURCC_T codec;
   MMAL_FOURCC_T encoding;
   VC_CONTAINER_FOURCC_T codec_variant;
   MMAL_FOURCC_T encoding_variant;
} encoding_table[] =
{
   {VC_CONTAINER_CODEC_H263,           MMAL_ENCODING_H263, 0, 0},
   {VC_CONTAINER_CODEC_H264,           MMAL_ENCODING_H264, 0, 0},
   {VC_CONTAINER_CODEC_H264,           MMAL_ENCODING_H264,
      VC_CONTAINER_VARIANT_H264_AVC1,           MMAL_ENCODING_VARIANT_H264_AVC1},
   {VC_CONTAINER_CODEC_H264,           MMAL_ENCODING_H264,
      VC_CONTAINER_VARIANT_H264_RAW,            MMAL_ENCODING_VARIANT_H264_RAW},
   {VC_CONTAINER_CODEC_MP4V,           MMAL_ENCODING_MP4V, 0, 0},
   {VC_CONTAINER_CODEC_MP2V,           MMAL_ENCODING_MP2V, 0, 0},
   {VC_CONTAINER_CODEC_MP1V,           MMAL_ENCODING_MP1V, 0, 0},
   {VC_CONTAINER_CODEC_WMV3,           MMAL_ENCODING_WMV3, 0, 0},
   {VC_CONTAINER_CODEC_WMV2,           MMAL_ENCODING_WMV2, 0, 0},
   {VC_CONTAINER_CODEC_WMV1,           MMAL_ENCODING_WMV1, 0, 0},
   {VC_CONTAINER_CODEC_WVC1,           MMAL_ENCODING_WVC1, 0, 0},
   {VC_CONTAINER_CODEC_VP6,            MMAL_ENCODING_VP6, 0, 0},
   {VC_CONTAINER_CODEC_VP7,            MMAL_ENCODING_VP7, 0, 0},
   {VC_CONTAINER_CODEC_VP8,            MMAL_ENCODING_VP8, 0, 0},
   {VC_CONTAINER_CODEC_THEORA,         MMAL_ENCODING_THEORA, 0, 0},
   {VC_CONTAINER_CODEC_SPARK,          MMAL_ENCODING_SPARK, 0, 0},

   {VC_CONTAINER_CODEC_GIF,            MMAL_ENCODING_GIF, 0, 0},
   {VC_CONTAINER_CODEC_JPEG,           MMAL_ENCODING_JPEG, 0, 0},
   {VC_CONTAINER_CODEC_PNG,            MMAL_ENCODING_PNG, 0, 0},
   {VC_CONTAINER_CODEC_PPM,            MMAL_ENCODING_PPM, 0, 0},
   {VC_CONTAINER_CODEC_TGA,            MMAL_ENCODING_TGA, 0, 0},
   {VC_CONTAINER_CODEC_BMP,            MMAL_ENCODING_BMP, 0, 0},

   {VC_CONTAINER_CODEC_PCM_SIGNED_BE,  MMAL_ENCODING_PCM_SIGNED_BE, 0, 0},
   {VC_CONTAINER_CODEC_PCM_UNSIGNED_BE,MMAL_ENCODING_PCM_UNSIGNED_BE, 0, 0},
   {VC_CONTAINER_CODEC_PCM_SIGNED_LE,  MMAL_ENCODING_PCM_SIGNED_LE, 0, 0},
   {VC_CONTAINER_CODEC_PCM_UNSIGNED_LE,MMAL_ENCODING_PCM_UNSIGNED_LE, 0, 0},
   {VC_CONTAINER_CODEC_PCM_FLOAT_BE,   MMAL_ENCODING_PCM_FLOAT_BE, 0, 0},
   {VC_CONTAINER_CODEC_PCM_FLOAT_LE,   MMAL_ENCODING_PCM_FLOAT_LE, 0, 0},

   {VC_CONTAINER_CODEC_MPGA,           MMAL_ENCODING_MPGA, 0, 0},
   {VC_CONTAINER_CODEC_MP4A,           MMAL_ENCODING_MP4A, 0, 0},
   {VC_CONTAINER_CODEC_ALAW,           MMAL_ENCODING_ALAW, 0, 0},
   {VC_CONTAINER_CODEC_MULAW,          MMAL_ENCODING_MULAW, 0, 0},
   {VC_CONTAINER_CODEC_ADPCM_MS,       MMAL_ENCODING_ADPCM_MS, 0, 0},
   {VC_CONTAINER_CODEC_ADPCM_IMA_MS,   MMAL_ENCODING_ADPCM_IMA_MS, 0, 0},
   {VC_CONTAINER_CODEC_ADPCM_SWF,      MMAL_ENCODING_ADPCM_SWF, 0, 0},
   {VC_CONTAINER_CODEC_WMA1,           MMAL_ENCODING_WMA1, 0, 0},
   {VC_CONTAINER_CODEC_WMA2,           MMAL_ENCODING_WMA2, 0, 0},
   {VC_CONTAINER_CODEC_WMAP,           MMAL_ENCODING_WMAP, 0, 0},
   {VC_CONTAINER_CODEC_WMAL,           MMAL_ENCODING_WMAL, 0, 0},
   {VC_CONTAINER_CODEC_WMAV,           MMAL_ENCODING_WMAV, 0, 0},
   {VC_CONTAINER_CODEC_AMRNB,          MMAL_ENCODING_AMRNB, 0, 0},
   {VC_CONTAINER_CODEC_AMRWB,          MMAL_ENCODING_AMRWB, 0, 0},
   {VC_CONTAINER_CODEC_AMRWBP,         MMAL_ENCODING_AMRWBP, 0, 0},
   {VC_CONTAINER_CODEC_AC3,            MMAL_ENCODING_AC3, 0, 0},
   {VC_CONTAINER_CODEC_EAC3,           MMAL_ENCODING_EAC3, 0, 0},
   {VC_CONTAINER_CODEC_DTS,            MMAL_ENCODING_DTS, 0, 0},
   {VC_CONTAINER_CODEC_MLP,            MMAL_ENCODING_MLP, 0, 0},
   {VC_CONTAINER_CODEC_FLAC,           MMAL_ENCODING_FLAC, 0, 0},
   {VC_CONTAINER_CODEC_VORBIS,         MMAL_ENCODING_VORBIS, 0, 0},
   {VC_CONTAINER_CODEC_SPEEX,          MMAL_ENCODING_SPEEX, 0, 0},
   {VC_CONTAINER_CODEC_ATRAC3,         MMAL_ENCODING_ATRAC3, 0, 0},
   {VC_CONTAINER_CODEC_ATRACX,         MMAL_ENCODING_ATRACX, 0, 0},
   {VC_CONTAINER_CODEC_ATRACL,         MMAL_ENCODING_ATRACL, 0, 0},
   {VC_CONTAINER_CODEC_MIDI,           MMAL_ENCODING_MIDI, 0, 0},
   {VC_CONTAINER_CODEC_EVRC,           MMAL_ENCODING_EVRC, 0, 0},
   {VC_CONTAINER_CODEC_NELLYMOSER,     MMAL_ENCODING_NELLYMOSER, 0, 0},
   {VC_CONTAINER_CODEC_QCELP,          MMAL_ENCODING_QCELP, 0, 0},

   {VC_CONTAINER_CODEC_UNKNOWN,        MMAL_ENCODING_UNKNOWN, 0, 0}
};

static MMAL_FOURCC_T container_to_mmal_encoding(VC_CONTAINER_FOURCC_T codec)
{
   unsigned int i;
   for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(encoding_table[i].codec == codec)
         break;
   return encoding_table[i].encoding;
}

static VC_CONTAINER_FOURCC_T mmal_to_container_encoding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(encoding_table[i].encoding == encoding)
         break;
   return encoding_table[i].codec;
}

static MMAL_FOURCC_T container_to_mmal_variant(VC_CONTAINER_FOURCC_T codec,
   VC_CONTAINER_FOURCC_T codec_variant)
{
   unsigned int i;
   for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(encoding_table[i].codec == codec &&
         encoding_table[i].codec_variant == codec_variant)
         break;
   return encoding_table[i].encoding_variant;
}

static VC_CONTAINER_FOURCC_T mmal_to_container_variant(MMAL_FOURCC_T encoding,
   MMAL_FOURCC_T encoding_variant)
{
   unsigned int i;
   for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(encoding_table[i].encoding == encoding &&
         encoding_table[i].encoding_variant == encoding_variant)
         break;
   return encoding_table[i].codec_variant;
}

/*****************************************************************************/
static struct {
   VC_CONTAINER_ES_TYPE_T container_type;
   MMAL_ES_TYPE_T type;
} es_type_table[] =
{
   {VC_CONTAINER_ES_TYPE_VIDEO,          MMAL_ES_TYPE_VIDEO},
   {VC_CONTAINER_ES_TYPE_AUDIO,          MMAL_ES_TYPE_AUDIO},
   {VC_CONTAINER_ES_TYPE_SUBPICTURE,     MMAL_ES_TYPE_SUBPICTURE},
   {VC_CONTAINER_ES_TYPE_UNKNOWN,        MMAL_ES_TYPE_UNKNOWN}
};

static MMAL_ES_TYPE_T container_to_mmal_es_type(VC_CONTAINER_ES_TYPE_T type)
{
   unsigned int i;
   for(i = 0; es_type_table[i].container_type != VC_CONTAINER_ES_TYPE_UNKNOWN; i++)
      if(es_type_table[i].container_type == type)
         break;
   return es_type_table[i].type;
}

static VC_CONTAINER_ES_TYPE_T mmal_to_container_es_type(MMAL_ES_TYPE_T type)
{
   unsigned int i;
   for(i = 0; es_type_table[i].container_type != VC_CONTAINER_ES_TYPE_UNKNOWN; i++)
      if(es_type_table[i].type == type)
         break;
   return es_type_table[i].container_type;
}

static MMAL_STATUS_T container_map_to_mmal_status(VC_CONTAINER_STATUS_T cstatus)
{
   switch (cstatus)
   {
      case VC_CONTAINER_SUCCESS: return MMAL_SUCCESS;
      case VC_CONTAINER_ERROR_CORRUPTED: return MMAL_ECORRUPT;
      case VC_CONTAINER_ERROR_OUT_OF_MEMORY: return MMAL_ENOMEM;
      case VC_CONTAINER_ERROR_OUT_OF_RESOURCES: return MMAL_ENOSPC;
      case VC_CONTAINER_ERROR_NOT_READY: return MMAL_ENOTREADY;
      case VC_CONTAINER_ERROR_NOT_FOUND: return MMAL_ENOENT;
      case VC_CONTAINER_ERROR_URI_NOT_FOUND: return MMAL_ENOENT;
      default: return MMAL_EINVAL;
   }
}

static MMAL_STATUS_T container_to_mmal_format(MMAL_ES_FORMAT_T *format,
   VC_CONTAINER_ES_FORMAT_T *container_format)
{
   format->type = container_to_mmal_es_type(container_format->es_type);
   if(format->type == MMAL_ES_TYPE_UNKNOWN)
      return MMAL_EINVAL;

   format->encoding = container_to_mmal_encoding(container_format->codec);
   format->encoding_variant = container_to_mmal_variant(container_format->codec, container_format->codec_variant);
   format->bitrate = container_format->bitrate;
   format->flags = (container_format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED) ?
      MMAL_ES_FORMAT_FLAG_FRAMED : 0;
   memset(format->es, 0, sizeof(*format->es));

   switch(format->type)
   {
   case MMAL_ES_TYPE_VIDEO:
      format->es->video.width = container_format->type->video.width;
      format->es->video.height = container_format->type->video.height;
      format->es->video.crop.width = container_format->type->video.visible_width;
      format->es->video.crop.height = container_format->type->video.visible_height;
      format->es->video.frame_rate.num = container_format->type->video.frame_rate_num;
      format->es->video.frame_rate.den = container_format->type->video.frame_rate_den;
      format->es->video.par.num = container_format->type->video.par_num;
      format->es->video.par.den = container_format->type->video.par_den;
      break;
   case MMAL_ES_TYPE_AUDIO:
      format->es->audio.channels = container_format->type->audio.channels;
      format->es->audio.sample_rate = container_format->type->audio.sample_rate;
      format->es->audio.bits_per_sample = container_format->type->audio.bits_per_sample;
      format->es->audio.block_align = container_format->type->audio.block_align;
      break;
   default:
      LOG_ERROR("format es type not handled (%i)", (int)format->type);
      break;
   }

   if(container_format->extradata_size)
   {
      MMAL_STATUS_T status = mmal_format_extradata_alloc(format, container_format->extradata_size);
      if(status != MMAL_SUCCESS)
      {
         LOG_ERROR("couldn't allocate extradata");
         return status;
      }
      format->extradata_size = container_format->extradata_size;
      memcpy(format->extradata, container_format->extradata, format->extradata_size);
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_to_container_format(VC_CONTAINER_ES_FORMAT_T *container_format,
   MMAL_ES_FORMAT_T *format)
{
   container_format->es_type = mmal_to_container_es_type(format->type);
   if(container_format->es_type == VC_CONTAINER_ES_TYPE_UNKNOWN)
      return MMAL_EINVAL;

   container_format->codec = mmal_to_container_encoding(format->encoding);
   container_format->codec_variant = mmal_to_container_variant(format->encoding, format->encoding_variant);
   container_format->bitrate = format->bitrate;
   container_format->flags = 0;
   if(format->flags & MMAL_ES_FORMAT_FLAG_FRAMED)
      container_format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
   memset(container_format->type, 0, sizeof(*container_format->type));

   /* Auto-detect H264 AVC1 variant */
   if(format->encoding == MMAL_ENCODING_H264 && !format->encoding_variant &&
      format->extradata_size >= 5 && *format->extradata == 1)
      container_format->codec_variant = VC_CONTAINER_VARIANT_H264_AVC1;

   switch(format->type)
   {
   case MMAL_ES_TYPE_VIDEO:
      container_format->type->video.width = format->es->video.width;
      container_format->type->video.height = format->es->video.height;
      container_format->type->video.frame_rate_num = format->es->video.frame_rate.num;
      container_format->type->video.frame_rate_den = format->es->video.frame_rate.den;
      container_format->type->video.par_num = format->es->video.par.num;
      container_format->type->video.par_den = format->es->video.par.den;
      break;
   case MMAL_ES_TYPE_AUDIO:
      container_format->type->audio.channels = format->es->audio.channels;
      container_format->type->audio.sample_rate = format->es->audio.sample_rate;
      container_format->type->audio.bits_per_sample = format->es->audio.bits_per_sample;
      container_format->type->audio.block_align = format->es->audio.block_align;
      break;
   default:
      LOG_ERROR("format es type not handled (%i)", (int)format->type);
      break;
   }

   container_format->extradata_size = format->extradata_size;
   container_format->extradata = format->extradata;

   return MMAL_SUCCESS;
}

/*****************************************************************************/
static void reader_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   VC_CONTAINER_STATUS_T cstatus;
   VC_CONTAINER_PACKET_T packet;
   MMAL_STATUS_T status;
   unsigned int i;

   memset(&packet, 0, sizeof(packet));

   while(1)
   {
      cstatus = vc_container_read(module->container, &packet, VC_CONTAINER_READ_FLAG_INFO);
      if(cstatus == VC_CONTAINER_ERROR_CONTINUE)
         continue;
      if(cstatus != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG("READ EOF (%i)", cstatus);
         break;
      }

      if (!module->packet_logged)
         LOG_DEBUG("packet info: track %i, size %i/%i, pts %"PRId64"%s, dts %"PRId64"%s, flags %x%s",
                   packet.track, packet.size, packet.frame_size,
                   packet.pts == VC_CONTAINER_TIME_UNKNOWN ? 0 : packet.pts,
                   packet.pts == VC_CONTAINER_TIME_UNKNOWN ? ":unknown" : "",
                   packet.dts == VC_CONTAINER_TIME_UNKNOWN ? 0 : packet.dts,
                   packet.dts == VC_CONTAINER_TIME_UNKNOWN ? ":unknown" : "",
                   packet.flags, (packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ? " (keyframe)" : "");

      /* Find the port corresponding to that track */
      for(i = 0; i < module->ports; i++)
         if(component->output[i]->priv->module->track == packet.track)
            break;
      if(i == module->ports)
      {
         vc_container_read(module->container, 0, VC_CONTAINER_READ_FLAG_SKIP);
         continue;
      }

      /* Get a buffer from this port */
      buffer = mmal_queue_get(component->output[i]->priv->module->queue);
      if(!buffer)
      {
         module->packet_logged = 1;
         break; /* Try again next time */
      }
      module->packet_logged = 0;

      if(component->output[i]->priv->module->flush)
      {
         buffer->length = 0;
         component->output[i]->priv->module->flush = MMAL_FALSE;
      }

      mmal_buffer_header_mem_lock(buffer);
      packet.data = buffer->data + buffer->length;
      packet.buffer_size = buffer->alloc_size - buffer->length;
      packet.size = 0;
      cstatus = vc_container_read(module->container, &packet, 0);
      mmal_buffer_header_mem_unlock(buffer);
      if(cstatus != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG("TEST read status: %i", cstatus);
         mmal_queue_put_back(component->output[i]->priv->module->queue, buffer);
         break;
      }

      if(!buffer->length)
      {
         buffer->pts = packet.pts == VC_CONTAINER_TIME_UNKNOWN ? MMAL_TIME_UNKNOWN : packet.pts;
         buffer->dts = packet.dts == VC_CONTAINER_TIME_UNKNOWN ? MMAL_TIME_UNKNOWN : packet.dts;
         buffer->flags = 0;
         if(packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME)
            buffer->flags |= MMAL_BUFFER_HEADER_FLAG_KEYFRAME;
         if(packet.flags & VC_CONTAINER_PACKET_FLAG_FRAME_START)
            buffer->flags |= MMAL_BUFFER_HEADER_FLAG_FRAME_START;
      }
      if(packet.flags & VC_CONTAINER_PACKET_FLAG_FRAME_END)
         buffer->flags |= MMAL_BUFFER_HEADER_FLAG_FRAME_END;
#ifdef VC_CONTAINER_PACKET_FLAG_CONFIG
      if(packet.flags & VC_CONTAINER_PACKET_FLAG_CONFIG)
         buffer->flags |= MMAL_BUFFER_HEADER_FLAG_CONFIG;
#endif

      buffer->length += packet.size;

      if((component->output[i]->format->flags & MMAL_ES_FORMAT_FLAG_FRAMED) &&
         buffer->length != buffer->alloc_size &&
         !(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END))
      {
         mmal_queue_put_back(component->output[i]->priv->module->queue, buffer);
         continue;
      }

      /* Send buffer back */
      mmal_port_buffer_header_callback(component->output[i], buffer);
   }

   if(cstatus == VC_CONTAINER_ERROR_EOS)
   {
      /* Send an empty EOS buffer for each port */
      for(i = 0; i < component->output_num; i++)
      {
         MMAL_PORT_T *port = component->output[i];
         if(!port->is_enabled)
            continue;
         if(port->priv->module->eos)
            continue;
         /* Get a buffer from this port */
         buffer = mmal_queue_get(port->priv->module->queue);
         if(!buffer)
            continue; /* Try again next time */
         buffer->length = 0;
         buffer->pts = buffer->dts = MMAL_TIME_UNKNOWN;
         buffer->flags = MMAL_BUFFER_HEADER_FLAG_EOS;
         /* Send buffer back */
         port->priv->module->eos = 1;
         mmal_port_buffer_header_callback(port, buffer);
      }
   }
   else if(cstatus != VC_CONTAINER_SUCCESS && !module->error)
   {
      status = mmal_event_error_send(component, container_map_to_mmal_status(cstatus));
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("unable to send an error event buffer (%i)", (int)status);
         return;
      }
      module->error = 1;
   }

   return;
}

/*****************************************************************************/
static void writer_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   VC_CONTAINER_STATUS_T cstatus;
   MMAL_PORT_MODULE_T *port_module;
   MMAL_PORT_T *port;
   MMAL_STATUS_T status;
   MMAL_BOOL_T eos;
   int64_t timestamp, timestamp_current;
   MMAL_BUFFER_HEADER_T *buffer;
   VC_CONTAINER_PACKET_T packet;
   unsigned int i, index;

   if(module->error)
      return;

   /* Select the next port to read from based on earliest timestamp. Buffers without
    * timestamps will end-up being prioritised. */
   for(i = 0, index = module->port_last_used, port = 0, timestamp = INT64_C(0);
       i < component->input_num; i++, index++)
   {
      if(index == component->input_num)
         index = 0;

      if(!component->input[index]->is_enabled)
         continue;

      buffer = mmal_queue_get(component->input[index]->priv->module->queue);
      if(!buffer)
         continue;

      timestamp_current = buffer->dts;
      if (timestamp_current == MMAL_TIME_UNKNOWN)
         timestamp_current = buffer->pts;
      if(!port)
         timestamp = timestamp_current;

      if(timestamp_current <= timestamp)
      {
         port = component->input[index];
         timestamp = timestamp_current;
         module->port_last_used = index;
      }
      mmal_queue_put_back(component->input[index]->priv->module->queue, buffer);
   }

   /* If a port is currently writing a frame then we override the decision to avoid
    * splitting frames */
   if(module->port_writing_frame && module->port_writing_frame - 1 < component->input_num &&
      component->input[module->port_writing_frame-1]->is_enabled)
      port = component->input[module->port_writing_frame-1];

   if(!port)
      return; /* nothing to write */

   port_module = port->priv->module;

   /* Get a buffer from this port */
   buffer = mmal_queue_get(port_module->queue);
   if(!buffer)
      return; /* nothing to write */

   mmal_buffer_header_mem_lock(buffer);
   memset(&packet, 0, sizeof(packet));
   packet.track = port_module->track;
   packet.size = buffer->length;
   packet.data = buffer->data + buffer->offset;
   packet.pts = buffer->pts == MMAL_TIME_UNKNOWN ? VC_CONTAINER_TIME_UNKNOWN : buffer->pts;
   packet.dts = buffer->dts == MMAL_TIME_UNKNOWN ? VC_CONTAINER_TIME_UNKNOWN  : buffer->dts;
   if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
      packet.flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;
   if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START)
      packet.flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
      packet.flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
   eos = buffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS;

   if ((packet.flags & VC_CONTAINER_PACKET_FLAG_FRAME) == VC_CONTAINER_PACKET_FLAG_FRAME)
      packet.frame_size = packet.size;
   else
      packet.frame_size = 0;

   module->port_writing_frame = port->index + 1;
   if((buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) ||
      !(port->format->flags & MMAL_ES_FORMAT_FLAG_FRAMED))
      module->port_writing_frame = 0;

   LOG_DEBUG("packet info: track %i, size %i/%i, pts %"PRId64", flags %x%s",
             packet.track, packet.size, packet.frame_size, packet.pts,
             packet.flags, (packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ? " (keyframe)" : "");

   cstatus = vc_container_write(module->container, &packet);
   mmal_buffer_header_mem_unlock(buffer);

   /* Send buffer back */
   buffer->length = 0;
   mmal_port_buffer_header_callback(port, buffer);

   /* Check for errors */
   if(cstatus != VC_CONTAINER_SUCCESS)
   {
      LOG_ERROR("write failed (%i)", (int)cstatus);
      status = mmal_event_error_send(component, container_map_to_mmal_status(cstatus));
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("unable to send an error event buffer (%i)", (int)status);
         return;
      }
      module->error = 1;
      return;
   }

   /* Generate EOS events */
   if(eos)
   {
      MMAL_EVENT_END_OF_STREAM_T *event;
      status = mmal_port_event_get(component->control, &buffer, MMAL_EVENT_EOS);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("unable to get an event buffer");
         return;
      }

      buffer->length = sizeof(*event);
      event = (MMAL_EVENT_END_OF_STREAM_T *)buffer->data;
      event->port_type = port->type;
      event->port_index = port->index;
      mmal_port_event_send(component->control, buffer);
   }
}

/** Destroy a previously created component */
static MMAL_STATUS_T container_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned int i;

   if(module->container)
      vc_container_close(module->container);

   for(i = 0; i < component->input_num; i++)
   {
      if(component->input[i]->priv->module->queue)
         mmal_queue_destroy(component->input[i]->priv->module->queue);
      if(component->input[i]->priv->module->format)
         vc_container_format_delete(component->input[i]->priv->module->format);
   }
   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);

   for(i = 0; i < component->output_num; i++)
      if(component->output[i]->priv->module->queue)
         mmal_queue_destroy(component->output[i]->priv->module->queue);
   if(component->output_num)
      mmal_ports_free(component->output, component->output_num);

   vcos_free(module);
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T container_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_PARAM_UNUSED(cb);

   if(!module->container)
      return MMAL_EINVAL;

   if(module->writer)
   {
      VC_CONTAINER_STATUS_T cstatus;
      port_module->track = module->container->tracks_num;
      cstatus = vc_container_control(module->container, VC_CONTAINER_CONTROL_TRACK_ADD,
                                     port_module->format);
      if(cstatus != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR("error adding track %4.4s (%i)", (char *)&port->format->encoding, (int)cstatus);
         return container_map_to_mmal_status(cstatus);
      }
   }

   if(port_module->track >= module->container->tracks_num)
      {
      LOG_ERROR("error 1 adding track %4.4s (%i/%i)", (char *)&port->format->encoding, port_module->track, module->container->tracks_num);
      return MMAL_EINVAL;
      }
   module->container->tracks[port_module->track]->is_enabled = 1;
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T container_port_flush(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   /* Flush buffers that our component is holding on to.
    * If the reading thread is holding onto a buffer it will send it back ASAP as well
    * so no need to care about that.  */
   buffer = mmal_queue_get(port_module->queue);
   while(buffer)
   {
      buffer->length = 0;
      mmal_port_buffer_header_callback(port, buffer);
      buffer = mmal_queue_get(port_module->queue);
   }

   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T container_port_disable(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned int track = port->priv->module->track;
   MMAL_STATUS_T status;

   if(!module->container || track >= module->container->tracks_num)
      return MMAL_EINVAL;

   /* Actions are prevented from running at that point so a flush
    * will return all buffers. */
   status = container_port_flush(port);
   if(status != MMAL_SUCCESS)
      return status;

   module->container->tracks[track]->is_enabled = 0;
   return MMAL_SUCCESS;
}

/** Send a buffer header to a port */
static MMAL_STATUS_T container_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   mmal_queue_put(port->priv->module->queue, buffer);
   mmal_component_action_trigger(port->component);
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T container_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_STATUS_T status;

   if(!module->writer)
      return MMAL_EINVAL;

   /* Set format of the track */
   status = mmal_to_container_format(port->priv->module->format, port->format);
   if (status != MMAL_SUCCESS)
      return status;

   port->buffer_num_min = READER_MIN_BUFFER_NUM;
   port->buffer_num_recommended = READER_RECOMMENDED_BUFFER_NUM;
   port->buffer_size_min = READER_MIN_BUFFER_SIZE;
   port->buffer_size_recommended = READER_RECOMMENDED_BUFFER_SIZE;
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T reader_container_open(MMAL_COMPONENT_T *component, const char *uri)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   VC_CONTAINER_STATUS_T cstatus;
   VC_CONTAINER_T *container;
   unsigned int i, port, track;

   /* Open container */
   module->container = container =
      vc_container_open_reader(uri, &cstatus, 0, 0);
   if(!container)
   {
     LOG_ERROR("error opening file %s (%i)", uri, cstatus);
     return container_map_to_mmal_status(cstatus);
   }

   /* Disable all tracks */
   for(track = 0; track < container->tracks_num; track++)
      container->tracks[track]->is_enabled = 0;

   /* Fill-in the ports */
   for(i = 0, port = 0; i < component->output_num; i++)
   {
      VC_CONTAINER_ES_TYPE_T type = VC_CONTAINER_ES_TYPE_VIDEO;
      if(i == 1) type = VC_CONTAINER_ES_TYPE_AUDIO;
      if(i == 2) type = VC_CONTAINER_ES_TYPE_SUBPICTURE;

      /* Look for the first track with the specified type */
      for(track = 0; track < container->tracks_num; track++)
         if(container->tracks[track]->format->es_type == type)
            break;
      if(track == container->tracks_num)
         continue;

      if(container_to_mmal_encoding(container->tracks[track]->format->codec) == MMAL_ENCODING_UNKNOWN)
         continue;

      /* Set format of the port */
      if(container_to_mmal_format(component->output[port]->format,
                                  container->tracks[track]->format) != MMAL_SUCCESS)
         continue;

      component->output[port]->buffer_num_min = READER_MIN_BUFFER_NUM;
      component->output[port]->buffer_num_recommended = READER_RECOMMENDED_BUFFER_NUM;
      component->output[port]->buffer_size_min = READER_MIN_BUFFER_SIZE;
      component->output[port]->buffer_size_recommended = READER_RECOMMENDED_BUFFER_SIZE;
      component->output[port]->priv->module->track = track;

      /* We're done with this port */
      port++;
   }
   module->ports = port;

   /* Reset the left over ports */
   for(i = port; i < component->output_num; i++)
   {
      component->output[i]->format->type = MMAL_ES_TYPE_UNKNOWN;
      component->output[i]->format->encoding = MMAL_ENCODING_UNKNOWN;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T reader_container_seek(MMAL_COMPONENT_T *component, const MMAL_PARAMETER_SEEK_T *seek)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   VC_CONTAINER_SEEK_FLAGS_T flags = 0;
   int64_t offset = seek->offset;
   VC_CONTAINER_STATUS_T cstatus;
   unsigned int i;

   if(seek->flags & MMAL_PARAM_SEEK_FLAG_PRECISE)
      flags |= VC_CONTAINER_SEEK_FLAG_PRECISE;
   if(seek->flags & MMAL_PARAM_SEEK_FLAG_FORWARD)
      flags |= VC_CONTAINER_SEEK_FLAG_FORWARD;

   mmal_component_action_lock(component);
   for(i = 0; i < component->output_num; i++)
   {
      component->output[i]->priv->module->eos = MMAL_FALSE;
      component->output[i]->priv->module->flush = MMAL_TRUE;
   }
   cstatus = vc_container_seek( module->container, &offset, VC_CONTAINER_SEEK_MODE_TIME, flags);
   mmal_component_action_unlock(component);
   return container_map_to_mmal_status(cstatus);
}

static MMAL_STATUS_T reader_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   switch(param->id)
   {
   case MMAL_PARAMETER_URI:
      if(module->container)
         return MMAL_EINVAL;

      memset(module->uri, 0, sizeof(module->uri));
      strncpy(module->uri, ((const MMAL_PARAMETER_STRING_T *)param)->str, sizeof(module->uri)-1 );
      return reader_container_open(component, module->uri);

   case MMAL_PARAMETER_SEEK:
      if(!module->container || param->size < sizeof(MMAL_PARAMETER_SEEK_T))
         return MMAL_EINVAL;

      return reader_container_seek(component, (const MMAL_PARAMETER_SEEK_T *)param);

   default:
      return MMAL_ENOSYS;
   }

   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_reader(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   unsigned int outputs_num, i;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   MMAL_PARAM_UNUSED(name);

   /* Allocate the context for our module */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;
   memset(module, 0, sizeof(*module));

   component->priv->pf_destroy = container_component_destroy;

   /* Create 3 tracks for now (audio/video/subpicture).
    * FIXME: ideally we should create 1 track per elementary stream. */
   outputs_num = 3;

   /* Now the component on reader has been created, we can allocate
    * the ports for this component */
   component->output = mmal_ports_alloc(component, outputs_num, MMAL_PORT_TYPE_OUTPUT,
                                        sizeof(MMAL_PORT_MODULE_T));
   if(!component->output)
      goto error;
   component->output_num = outputs_num;

   for(i = 0; i < outputs_num; i++)
   {
      component->output[i]->priv->pf_enable = container_port_enable;
      component->output[i]->priv->pf_disable = container_port_disable;
      component->output[i]->priv->pf_flush = container_port_flush;
      component->output[i]->priv->pf_send = container_port_send;
      component->output[i]->priv->module->queue = mmal_queue_create();
      if(!component->output[i]->priv->module->queue)
         goto error;
   }
   component->control->priv->pf_parameter_set = reader_parameter_set;

   status = mmal_component_action_register(component, reader_do_processing);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

 error:
   container_component_destroy(component);
   return status;
}

static MMAL_STATUS_T writer_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   VC_CONTAINER_STATUS_T cstatus;

   switch(param->id)
   {
   case MMAL_PARAMETER_URI:
      if(module->container)
         return MMAL_EINVAL;

      memset(module->uri, 0, sizeof(module->uri));
      strncpy(module->uri, ((const MMAL_PARAMETER_URI_T *)param)->uri, sizeof(module->uri)-1 );

      /* Open container */
      module->container = vc_container_open_writer(module->uri, &cstatus, 0, 0);
      if(!module->container)
      {
         LOG_ERROR("error opening file %s (%i)", module->uri, cstatus);
         return container_map_to_mmal_status(cstatus);
      }
      return MMAL_SUCCESS;

   default:
      return MMAL_ENOSYS;
   }

   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_writer(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int i;
   MMAL_PARAM_UNUSED(name);

   /* Allocate the context for our module */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;
   memset(module, 0, sizeof(*module));
   module->writer = 1;

   component->priv->pf_destroy = container_component_destroy;

   /* Now the component on reader has been created, we can allocate
    * the ports for this component */
   component->input = mmal_ports_alloc(component, WRITER_PORTS_NUM, MMAL_PORT_TYPE_INPUT,
                                        sizeof(MMAL_PORT_MODULE_T));
   if(!component->input)
      goto error;
   component->input_num = WRITER_PORTS_NUM;

   for(i = 0; i < component->input_num; i++)
   {
      component->input[i]->priv->pf_enable = container_port_enable;
      component->input[i]->priv->pf_disable = container_port_disable;
      component->input[i]->priv->pf_flush = container_port_flush;
      component->input[i]->priv->pf_send = container_port_send;
      component->input[i]->priv->pf_set_format = container_port_set_format;

      component->input[i]->priv->module->queue = mmal_queue_create();
      if(!component->input[i]->priv->module->queue)
         goto error;
      component->input[i]->priv->module->format = vc_container_format_create(0);
      if(!component->input[i]->priv->module->format)
         goto error;
   }
   component->control->priv->pf_parameter_set = writer_parameter_set;

   status = mmal_component_action_register(component, writer_do_processing);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

 error:
   container_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_container_reader);
void mmal_register_component_container_reader(void)
{
   mmal_component_supplier_register("container_reader", mmal_component_create_reader);
}

MMAL_CONSTRUCTOR(mmal_register_component_container_writer);
void mmal_register_component_container_writer(void)
{
   mmal_component_supplier_register("container_writer", mmal_component_create_writer);
}
