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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

#include "raw_video_common.h"

/******************************************************************************
Defines.
******************************************************************************/
#define FILE_HEADER_SIZE_MAX 1024
#define FRAME_HEADER_SIZE_MAX 256
#define OPTION_SIZE_MAX 32

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   VC_CONTAINER_STATUS_T status;

   bool yuv4mpeg2;
   bool non_standard;
   char option[OPTION_SIZE_MAX];

   bool frame_header;
   unsigned int frame_header_size;

   int64_t data_offset;
   unsigned int block_size;
   unsigned int block_offset;
   unsigned int frames;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T rawvideo_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T read_yuv4mpeg2_option( VC_CONTAINER_T *ctx,
   unsigned int *bytes_left )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int size, i;

   /* Start by skipping spaces */
   while (*bytes_left && PEEK_U8(ctx) == ' ')
      (*bytes_left)--, _SKIP_U8(ctx);

   size = PEEK_BYTES(ctx, module->option,
      MIN(sizeof(module->option), *bytes_left));

   /* The config option ends at next space or newline */
   for (i = 0; i < size; i++)
   {
      if (module->option[i] == ' ' || module->option[i] == 0x0a)
      {
         module->option[i] = 0;
         break;
      }
   }
   if (i == 0)
      return VC_CONTAINER_ERROR_NOT_FOUND;

   *bytes_left -= i;
   SKIP_BYTES(ctx, i);

   /* If option is too long, we just discard it */
   if (i == size)
   {
      while (*bytes_left && PEEK_U8(ctx) != ' ' && PEEK_U8(ctx) != 0x0a)
         (*bytes_left)--, _SKIP_U8(ctx);
      return VC_CONTAINER_ERROR_NOT_FOUND;
   }

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T read_yuv4mpeg2_file_header( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int bytes_left = FILE_HEADER_SIZE_MAX - 10;
   unsigned int value1, value2;
   char codec[OPTION_SIZE_MAX] = "420";
   uint8_t h[10];

   /* Check for the YUV4MPEG2 signature */
   if (READ_BYTES(ctx, h, sizeof(h)) != sizeof(h))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if (memcmp(h, "YUV4MPEG2 ", sizeof(h)))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Parse parameters */
   while (read_yuv4mpeg2_option(ctx, &bytes_left) == VC_CONTAINER_SUCCESS)
   {
      if (sscanf(module->option, "W%i", &value1) == 1)
         ctx->tracks[0]->format->type->video.width = value1;
      else if (sscanf(module->option, "H%i", &value1) == 1)
         ctx->tracks[0]->format->type->video.height = value1;
      else if (sscanf(module->option, "S%i", &value1) == 1)
         module->block_size = value1;
      else if (sscanf(module->option, "F%i:%i", &value1, &value2) == 2)
      {
         ctx->tracks[0]->format->type->video.frame_rate_num = value1;
         ctx->tracks[0]->format->type->video.frame_rate_den = value2;
      }
      else if (sscanf(module->option, "A%i:%i", &value1, &value2) == 2)
      {
         ctx->tracks[0]->format->type->video.par_num = value1;
         ctx->tracks[0]->format->type->video.par_den = value2;
      }
      else if (module->option[0] == 'C')
      {
         strcpy(codec, module->option+1);
      }
   }

   /* Check the end marker */
   if (_READ_U8(ctx) != 0x0a)
   {
      LOG_ERROR(ctx, "missing end of header marker");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   /* Find out which codec we are dealing with */
   if (from_yuv4mpeg2(codec, &ctx->tracks[0]->format->codec, &value1, &value2))
   {
      module->block_size = ctx->tracks[0]->format->type->video.width *
         ctx->tracks[0]->format->type->video.height * value1 / value2;
   }
   else
   {
      memcpy(&ctx->tracks[0]->format->codec, codec, 4);
      module->non_standard = true;
   }

   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T read_yuv4mpeg2_frame_header( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int bytes_left = FRAME_HEADER_SIZE_MAX - 5;
   unsigned int value1;
   char header[5];

   if (READ_BYTES(ctx, header, sizeof(header)) != sizeof(header) ||
          memcmp(header, "FRAME", sizeof(header)))
   {
      LOG_ERROR(ctx, "missing frame marker");
      return STREAM_STATUS(ctx) != VC_CONTAINER_SUCCESS ?
         STREAM_STATUS(ctx) : VC_CONTAINER_ERROR_CORRUPTED;
   }

   /* Parse parameters */
   while (read_yuv4mpeg2_option(ctx, &bytes_left) == VC_CONTAINER_SUCCESS)
   {
      if (module->non_standard && sscanf(module->option, "S%i", &value1) == 1)
         module->block_size = value1;
   }

   /* Check the end marker */
   if (_READ_U8(ctx) != 0x0a)
   {
      LOG_ERROR(ctx, "missing end of frame header marker");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   module->frame_header_size = FRAME_HEADER_SIZE_MAX - bytes_left - 1;
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T rawvideo_parse_uri( VC_CONTAINER_T *ctx,
   VC_CONTAINER_FOURCC_T *c, unsigned int *w, unsigned int *h,
   unsigned int *fr_num, unsigned int *fr_den, unsigned *block_size )
{
   VC_CONTAINER_FOURCC_T codec = 0;
   unsigned int i, matches, width = 0, height = 0, fn = 0, fd = 0, size = 0;
   const char *uri = ctx->priv->io->uri;

   /* Try and find a match for the string describing the format */
   for (i = 0; uri[i]; i++)
   {
      if (uri[i] != '_' && uri[i+1] != 'C')
         continue;

      matches = sscanf(uri+i, "_C%4cW%iH%iF%i#%iS%i", (char *)&codec,
         &width, &height, &fn, &fd, &size);
      if (matches >= 3)
         break;
   }
   if (!uri[i])
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if (!size)
   {
      switch (codec)
      {
      case VC_CONTAINER_CODEC_I420:
      case VC_CONTAINER_CODEC_YV12:
         size = width * height * 3 / 2;
         break;
      default: break;
      }
   }

   if (!width || !height || !size)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if (block_size) *block_size = size;
   if (c) *c = codec;
   if (w) *w = width;
   if (h) *h = height;
   if (fr_num) *fr_num = fn;
   if (fr_den) *fr_den = fd;
   if (block_size) *block_size = size;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_reader_read( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int size;

   if (module->status != VC_CONTAINER_SUCCESS)
      return module->status;

   if (module->yuv4mpeg2 && !module->block_offset &&
         !module->frame_header)
   {
      module->status = read_yuv4mpeg2_frame_header(ctx);
      if (module->status != VC_CONTAINER_SUCCESS)
         return module->status;

      module->frame_header = true;
   }

   if (!module->block_offset)
      packet->pts = packet->dts = module->frames * INT64_C(1000000) *
         ctx->tracks[0]->format->type->video.frame_rate_den /
         ctx->tracks[0]->format->type->video.frame_rate_num;
   else
      packet->pts = packet->dts = VC_CONTAINER_TIME_UNKNOWN;
   packet->flags = VC_CONTAINER_PACKET_FLAG_FRAME_END |
      VC_CONTAINER_PACKET_FLAG_KEYFRAME;
   if (!module->block_offset)
      packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;
   packet->frame_size = module->block_size;
   packet->size = module->block_size - module->block_offset;
   packet->track = 0;

   if (flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      size = SKIP_BYTES(ctx, packet->size);
      module->block_offset = 0;
      module->frames++;
      module->frame_header = 0;
      module->status = STREAM_STATUS(ctx);
      return module->status;
   }

   if (flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   size = MIN(module->block_size - module->block_offset, packet->buffer_size);
   size = READ_BYTES(ctx, packet->data, size);
   module->block_offset += size;
   packet->size = size;

   if (module->block_offset == module->block_size)
   {
      module->block_offset = 0;
      module->frame_header = 0;
      module->frames++;
   }

   module->status = size ? VC_CONTAINER_SUCCESS : STREAM_STATUS(ctx);
   return module->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_reader_seek( VC_CONTAINER_T *ctx, int64_t *offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(mode);

   module->frames = *offset *
      ctx->tracks[0]->format->type->video.frame_rate_num /
      ctx->tracks[0]->format->type->video.frame_rate_den / INT64_C(1000000);
   module->block_offset = 0;

   if ((flags & VC_CONTAINER_SEEK_FLAG_FORWARD) &&
      module->frames * INT64_C(1000000) *
         ctx->tracks[0]->format->type->video.frame_rate_den /
         ctx->tracks[0]->format->type->video.frame_rate_num < *offset)
      module->frames++;

   module->frame_header = 0;

   module->status =
      SEEK(ctx, module->data_offset + module->frames *
         (module->block_size + module->frame_header_size));

   return module->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_reader_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   for (; ctx->tracks_num > 0; ctx->tracks_num--)
      vc_container_free_track(ctx, ctx->tracks[ctx->tracks_num-1]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T rawvideo_reader_open( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   bool yuv4mpeg2 = false;
   uint8_t h[10];

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Check for the YUV4MPEG2 signature */
   if (PEEK_BYTES(ctx, h, sizeof(h)) != sizeof(h))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if (!memcmp(h, "YUV4MPEG2 ", sizeof(h)))
      yuv4mpeg2 = true;

   /* Or check if the extension is supported */
   if (!yuv4mpeg2 &&
       !(extension && !strcasecmp(extension, "yuv")))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(ctx, "using raw video reader");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if (!module) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks_num = 1;
   ctx->tracks = &module->track;
   ctx->tracks[0] = vc_container_allocate_track(ctx, 0);
   if (!ctx->tracks[0])
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }
   ctx->tracks[0]->format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   ctx->tracks[0]->is_enabled = true;
   ctx->tracks[0]->format->type->video.frame_rate_num = 25;
   ctx->tracks[0]->format->type->video.frame_rate_den = 1;
   ctx->tracks[0]->format->type->video.par_num = 1;
   ctx->tracks[0]->format->type->video.par_den = 1;

   if (yuv4mpeg2)
   {
      status = read_yuv4mpeg2_file_header(ctx);
      if (status != VC_CONTAINER_SUCCESS)
         goto error;

      module->data_offset = STREAM_POSITION(ctx);

      status = read_yuv4mpeg2_frame_header(ctx);
      if (status != VC_CONTAINER_SUCCESS)
         goto error;
      module->frame_header = true;
   }
   else
   {
      VC_CONTAINER_FOURCC_T codec;
      unsigned int width, height, fr_num, fr_den, block_size;

      status = rawvideo_parse_uri(ctx, &codec, &width, &height,
         &fr_num, &fr_den, &block_size);
      if (status != VC_CONTAINER_SUCCESS)
         goto error;
      ctx->tracks[0]->format->codec = codec;
      ctx->tracks[0]->format->type->video.width = width;
      ctx->tracks[0]->format->type->video.height = height;
      if (fr_num && fr_den)
      {
         ctx->tracks[0]->format->type->video.frame_rate_num = fr_num;
         ctx->tracks[0]->format->type->video.frame_rate_den = fr_den;
      }
      module->block_size = block_size;
   }

   /*
    *  We now have all the information we really need to start playing the stream
    */

   LOG_INFO(ctx, "rawvideo %4.4s/%ix%i/fps:%i:%i/size:%i",
      (char *)&ctx->tracks[0]->format->codec,
      ctx->tracks[0]->format->type->video.width,
      ctx->tracks[0]->format->type->video.height,
      ctx->tracks[0]->format->type->video.frame_rate_num,
      ctx->tracks[0]->format->type->video.frame_rate_den, module->block_size);
   ctx->priv->pf_close = rawvideo_reader_close;
   ctx->priv->pf_read = rawvideo_reader_read;
   ctx->priv->pf_seek = rawvideo_reader_seek;
   module->yuv4mpeg2 = yuv4mpeg2;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(ctx, "rawvideo: error opening stream (%i)", status);
   rawvideo_reader_close(ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open rawvideo_reader_open
#endif
