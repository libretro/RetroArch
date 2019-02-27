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

/** \file
 * Implementation of a PCM packetizer.
 */

#include <stdlib.h>
#include <string.h>

#include "containers/packetizers.h"
#include "containers/core/packetizers_private.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_time.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_bytestream.h"

#define FRAME_SIZE (16*1024) /**< Arbitrary value which is neither too small nor too big */
#define FACTOR_SHIFT 4 /**< Shift applied to the conversion factor */

VC_CONTAINER_STATUS_T pcm_packetizer_open( VC_PACKETIZER_T * );

/*****************************************************************************/
enum conversion {
   CONVERSION_NONE = 0,
   CONVERSION_U8_TO_S16L,
   CONVERSION_UNKNOWN
};

typedef struct VC_PACKETIZER_MODULE_T {
   enum {
      STATE_NEW_PACKET = 0,
      STATE_DATA
   } state;

   unsigned int samples_per_frame;
   unsigned int bytes_per_sample;
   unsigned int max_frame_size;

   uint32_t bytes_read;
   unsigned int frame_size;

   enum conversion conversion;
   unsigned int conversion_factor;
} VC_PACKETIZER_MODULE_T;

/*****************************************************************************/
static VC_CONTAINER_STATUS_T pcm_packetizer_close( VC_PACKETIZER_T *p_ctx )
{
   free(p_ctx->priv->module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T pcm_packetizer_reset( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   module->state = STATE_NEW_PACKET;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static void convert_pcm_u8_to_s16l( uint8_t **p_out, uint8_t *in, size_t size)
{
   int16_t *out = (int16_t *)*p_out;
   uint8_t tmp;

   while(size--)
   {
      tmp = *in++;
      *out++ = ((tmp - 128) << 8) | tmp;
   }
   *p_out = (uint8_t *)out;
}

/*****************************************************************************/
static void convert_pcm( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_BYTESTREAM_T *stream, size_t size, uint8_t *out )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   uint8_t tmp[256];
   size_t tmp_size;

   while(size)
   {
      tmp_size = MIN(sizeof(tmp), size);
      bytestream_get(stream, tmp, tmp_size);
      if (module->conversion == CONVERSION_U8_TO_S16L)
         convert_pcm_u8_to_s16l(&out, tmp, tmp_size);
      else
         bytestream_skip(stream, tmp_size);
      size -= tmp_size;
   }
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T pcm_packetizer_packetize( VC_PACKETIZER_T *p_ctx,
   VC_CONTAINER_PACKET_T *out, VC_PACKETIZER_FLAGS_T flags )
{
   VC_PACKETIZER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_BYTESTREAM_T *stream = &p_ctx->priv->stream;
   VC_CONTAINER_TIME_T *time = &p_ctx->priv->time;
   int64_t pts, dts;
   size_t offset, size;

   while(1) switch (module->state)
   {
   case STATE_NEW_PACKET:
      /* Make sure we've got enough data */
      if(bytestream_size(stream) < module->max_frame_size &&
         !(flags & VC_PACKETIZER_FLAG_FLUSH))
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA;
      if(!bytestream_size(stream))
         return VC_CONTAINER_ERROR_INCOMPLETE_DATA;

      module->frame_size = bytestream_size(stream);
      if(module->frame_size > module->max_frame_size)
         module->frame_size = module->max_frame_size;
      bytestream_get_timestamps_and_offset(stream, &pts, &dts, &offset, true);
      vc_container_time_set(time, pts);
      if(pts != VC_CONTAINER_TIME_UNKNOWN)
         vc_container_time_add(time, offset / module->bytes_per_sample);

      module->bytes_read = 0;
      module->state = STATE_DATA;
      /* fall through to the next state */

   case STATE_DATA:
      size = module->frame_size - module->bytes_read;
      out->pts = out->dts = VC_CONTAINER_TIME_UNKNOWN;
      out->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END;
      out->size = (size * module->conversion_factor) >> FACTOR_SHIFT;

      if(!module->bytes_read)
      {
         out->pts = out->dts = vc_container_time_get(time);
         out->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
      }

      if(flags & VC_PACKETIZER_FLAG_INFO)
         return VC_CONTAINER_SUCCESS;

      if(flags & VC_PACKETIZER_FLAG_SKIP)
      {
         bytestream_skip( stream, size );
      }
      else
      {
         out->size = MIN(out->size, out->buffer_size);
         size = (out->size << FACTOR_SHIFT) / module->conversion_factor;
         out->size = (size * module->conversion_factor) >> FACTOR_SHIFT;

         if(module->conversion != CONVERSION_NONE)
            convert_pcm(p_ctx, stream, size, out->data);
         else
            bytestream_get(stream, out->data, out->size);
      }
      module->bytes_read += size;

      if(module->bytes_read == module->frame_size)
      {
         vc_container_time_add(time, module->samples_per_frame);
         module->state = STATE_NEW_PACKET;
      }
      return VC_CONTAINER_SUCCESS;

   default:
      break;
   };

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T pcm_packetizer_open( VC_PACKETIZER_T *p_ctx )
{
   VC_PACKETIZER_MODULE_T *module;
   unsigned int bytes_per_sample = 0;
   enum conversion conversion = CONVERSION_NONE;

   if(p_ctx->in->codec != VC_CONTAINER_CODEC_PCM_UNSIGNED_BE &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_PCM_UNSIGNED_LE &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_PCM_SIGNED_BE &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_PCM_SIGNED_LE &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_PCM_FLOAT_BE &&
      p_ctx->in->codec != VC_CONTAINER_CODEC_PCM_FLOAT_LE)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if(p_ctx->in->type->audio.block_align)
      bytes_per_sample = p_ctx->in->type->audio.block_align;
   else if(p_ctx->in->type->audio.bits_per_sample && p_ctx->in->type->audio.channels)
      bytes_per_sample = p_ctx->in->type->audio.bits_per_sample *
         p_ctx->in->type->audio.channels / 8;

   if(!bytes_per_sample)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Check if we support any potential conversion we've been asked to do */
   if(p_ctx->out->codec_variant)
      conversion = CONVERSION_UNKNOWN;
   if(p_ctx->out->codec_variant == VC_FOURCC('s','1','6','l') &&
      p_ctx->in->codec == VC_CONTAINER_CODEC_PCM_SIGNED_LE &&
      p_ctx->in->type->audio.bits_per_sample == 16)
      conversion = CONVERSION_NONE;
   if(p_ctx->out->codec_variant == VC_FOURCC('s','1','6','l') &&
      (p_ctx->in->codec == VC_CONTAINER_CODEC_PCM_UNSIGNED_LE ||
       p_ctx->in->codec == VC_CONTAINER_CODEC_PCM_UNSIGNED_BE) &&
      p_ctx->in->type->audio.bits_per_sample == 8)
      conversion = CONVERSION_U8_TO_S16L;
   if(conversion == CONVERSION_UNKNOWN)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   p_ctx->priv->module = module = malloc(sizeof(*module));
   if(!module)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));
   module->conversion = conversion;
   module->conversion_factor = 1 << FACTOR_SHIFT;

   p_ctx->out->codec_variant = 0;
   if(conversion == CONVERSION_U8_TO_S16L)
   {
      module->conversion_factor = 2 << FACTOR_SHIFT;
      p_ctx->out->type->audio.bits_per_sample *= 2;
      p_ctx->out->type->audio.block_align *= 2;
      p_ctx->out->codec = VC_CONTAINER_CODEC_PCM_SIGNED_LE;
   }

   vc_container_time_set_samplerate(&p_ctx->priv->time, p_ctx->in->type->audio.sample_rate, 1);

   p_ctx->max_frame_size = FRAME_SIZE;
   module->max_frame_size = (FRAME_SIZE << FACTOR_SHIFT) / module->conversion_factor;
   module->bytes_per_sample = bytes_per_sample;
   module->samples_per_frame = module->max_frame_size / bytes_per_sample;
   p_ctx->priv->pf_close = pcm_packetizer_close;
   p_ctx->priv->pf_packetize = pcm_packetizer_packetize;
   p_ctx->priv->pf_reset = pcm_packetizer_reset;

   LOG_DEBUG(0, "using pcm audio packetizer");
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_PACKETIZER_REGISTER(pcm_packetizer_open,  "pcm");
