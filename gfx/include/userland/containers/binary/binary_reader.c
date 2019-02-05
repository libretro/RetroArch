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

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/
#define DEFAULT_BLOCK_SIZE (1024*16)
/* Work-around for JPEG because our decoder expects that much at the start */
#define DEFAULT_JPEG_BLOCK_SIZE (1024*80)

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   unsigned int default_block_size;
   unsigned int block_size;
   bool init;

   VC_CONTAINER_STATUS_T status;

} VC_CONTAINER_MODULE_T;

static struct
{
   const char *ext;
   VC_CONTAINER_ES_TYPE_T type;
   VC_CONTAINER_FOURCC_T codec;

} extension_to_format_table[] =
{
   /* Audio */
   {"mp3",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_MPGA},
   {"aac",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_MP4A},
   {"adts",    VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_MP4A},
   {"ac3",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_AC3},
   {"ec3",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_EAC3},
   {"amr",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_AMRNB},
   {"awb",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_AMRWB},
   {"evrc",    VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_EVRC},
   {"dts",     VC_CONTAINER_ES_TYPE_AUDIO, VC_CONTAINER_CODEC_DTS},

   /* Video */
   {"m1v",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MP1V},
   {"m2v",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MP2V},
   {"m4v",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MP4V},
   {"mp4v",    VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MP4V},
   {"h263",    VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H263},
   {"263",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H263},
   {"h264",    VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H264},
   {"264",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H264},
   {"mvc",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MVC},
   {"vc1l",    VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_WVC1},

   /* Image */
   {"gif",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_GIF},
   {"jpg",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_JPEG},
   {"jpeg",    VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_JPEG},
   {"png",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_PNG},
   {"ppm",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_PPM},
   {"tga",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_TGA},
   {"bmp",     VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_BMP},

   {"bin",     0, 0},
   {0, 0, 0}
};

static struct
{
   const char *ext;
   VC_CONTAINER_ES_TYPE_T type;
   VC_CONTAINER_FOURCC_T codec;

} bin_extension_to_format_table[] =
{
   {"m4v.bin", VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_MP4V},
   {"263.bin", VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H263},
   {"264.bin", VC_CONTAINER_ES_TYPE_VIDEO, VC_CONTAINER_CODEC_H264},
   {0, 0, 0}
};

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T binary_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T binary_reader_read( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int size;

   if(module->status != VC_CONTAINER_SUCCESS)
      return module->status;

   if(!module->block_size)
   {
      module->block_size = module->default_block_size;
      module->init = 0;
   }

   packet->size = module->block_size;
   packet->dts = packet->pts = VC_CONTAINER_TIME_UNKNOWN;
   if(module->init) packet->dts = packet->pts = 0;
   packet->track = 0;
   packet->flags = 0;

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      size = SKIP_BYTES(p_ctx, module->block_size);
      module->block_size -= size;
      module->status = STREAM_STATUS(p_ctx);
      return module->status;
   }

   if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   size = MIN(module->block_size, packet->buffer_size);
   size = READ_BYTES(p_ctx, packet->data, size);
   module->block_size -= size;
   packet->size = size;

   module->status = size ? VC_CONTAINER_SUCCESS : STREAM_STATUS(p_ctx);
   return module->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T binary_reader_seek( VC_CONTAINER_T *p_ctx, int64_t *offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(module);
   VC_CONTAINER_PARAM_UNUSED(offset);
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);
   return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T binary_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T binary_reader_open( VC_CONTAINER_T *p_ctx )
{
   const char *extension = vc_uri_path_extension(p_ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   VC_CONTAINER_ES_TYPE_T es_type = 0;
   VC_CONTAINER_FOURCC_T codec = 0;
   unsigned int i;

   /* Check if the user has specified a container */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   /* Check if the extension is supported */
   if(!extension || !vc_uri_path(p_ctx->priv->uri))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   for(i = 0; extension_to_format_table[i].ext; i++)
   {
      if(strcasecmp(extension, extension_to_format_table[i].ext))
         continue;

      es_type = extension_to_format_table[i].type;
      codec = extension_to_format_table[i].codec;
      break;
   }
   if(!extension_to_format_table[i].ext) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* If this a .bin file we look in our bin list */
   for(i = 0; !codec && bin_extension_to_format_table[i].ext; i++)
   {
      if(!strstr(vc_uri_path(p_ctx->priv->uri), bin_extension_to_format_table[i].ext) &&
         !strstr(extension, bin_extension_to_format_table[i].ext))
         continue;

      es_type = bin_extension_to_format_table[i].type;
      codec = bin_extension_to_format_table[i].codec;
      break;
   }
   if(!codec) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks_num = 1;
   p_ctx->tracks = &module->track;
   p_ctx->tracks[0] = vc_container_allocate_track(p_ctx, 0);
   if(!p_ctx->tracks[0]) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   p_ctx->tracks[0]->format->es_type = es_type;
   p_ctx->tracks[0]->format->codec = codec;
   p_ctx->tracks[0]->is_enabled = true;
   module->default_block_size = DEFAULT_BLOCK_SIZE;
   if(codec == VC_CONTAINER_CODEC_JPEG)
      module->default_block_size = DEFAULT_JPEG_BLOCK_SIZE;
   module->block_size = module->default_block_size;
   module->init = 1;

   /*
    *  We now have all the information we really need to start playing the stream
    */

   p_ctx->priv->pf_close = binary_reader_close;
   p_ctx->priv->pf_read = binary_reader_read;
   p_ctx->priv->pf_seek = binary_reader_seek;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "binary: error opening stream (%i)", status);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open binary_reader_open
#endif
