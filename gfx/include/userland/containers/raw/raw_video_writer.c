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
#include <stdarg.h>
#include <string.h>

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

#include "raw_video_common.h"

/******************************************************************************
Defines.
******************************************************************************/

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   bool yuv4mpeg2;
   bool header_done;
   bool non_standard;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T rawvideo_writer_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_write_header( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int size;
   char line[128];
   const char *id;

   size = snprintf(line, sizeof(line), "YUV4MPEG2 W%i H%i",
      ctx->tracks[0]->format->type->video.width,
      ctx->tracks[0]->format->type->video.height);
   if (size >= sizeof(line))
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   WRITE_BYTES(ctx, line, size);

   if (ctx->tracks[0]->format->type->video.frame_rate_num &&
       ctx->tracks[0]->format->type->video.frame_rate_den)
   {
      size = snprintf(line, sizeof(line), " F%i:%i",
         ctx->tracks[0]->format->type->video.frame_rate_num,
         ctx->tracks[0]->format->type->video.frame_rate_den);
      if (size >= sizeof(line))
         return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
      WRITE_BYTES(ctx, line, size);
   }

   if (ctx->tracks[0]->format->type->video.par_num &&
       ctx->tracks[0]->format->type->video.par_den)
   {
      size = snprintf(line, sizeof(line), " A%i:%i",
         ctx->tracks[0]->format->type->video.par_num,
         ctx->tracks[0]->format->type->video.par_den);
      if (size >= sizeof(line))
         return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
      WRITE_BYTES(ctx, line, size);
   }

   if (to_yuv4mpeg2(ctx->tracks[0]->format->codec, &id, 0, 0))
   {
      size = snprintf(line, sizeof(line), " C%s", id);
   }
   else
   {
      module->non_standard = true;
      size = snprintf(line, sizeof(line), " C%4.4s",
         (char *)&ctx->tracks[0]->format->codec);
   }
   if (size >= sizeof(line))
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   WRITE_BYTES(ctx, line, size);

   _WRITE_U8(ctx, 0x0a);
   module->header_done = true;
   return STREAM_STATUS(ctx);
}

static VC_CONTAINER_STATUS_T simple_write_add_track( VC_CONTAINER_T *ctx,
   VC_CONTAINER_ES_FORMAT_T *format )
{
   VC_CONTAINER_STATUS_T status;

   /* Sanity check that we support the type of track being created */
   if (ctx->tracks_num)
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   if (format->es_type != VC_CONTAINER_ES_TYPE_VIDEO)
      return VC_CONTAINER_ERROR_TRACK_FORMAT_NOT_SUPPORTED;

   /* Allocate and initialise track data */
   ctx->tracks[0] = vc_container_allocate_track(ctx, 0);
   if (!ctx->tracks[0])
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   status = vc_container_track_allocate_extradata(ctx,
      ctx->tracks[0], format->extradata_size);
   if(status != VC_CONTAINER_SUCCESS)
      return status;

   vc_container_format_copy(ctx->tracks[0]->format, format,
      format->extradata_size);
   ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_writer_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   for (; ctx->tracks_num > 0; ctx->tracks_num--)
      vc_container_free_track(ctx, ctx->tracks[ctx->tracks_num-1]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_writer_write( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_STATUS_T status;

   if (module->yuv4mpeg2 && !module->header_done)
   {
      status = rawvideo_write_header(ctx);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   if (module->yuv4mpeg2 &&
       (packet->flags & VC_CONTAINER_PACKET_FLAG_FRAME_START))
   {
      /* Write the metadata */
      WRITE_BYTES(ctx, "FRAME", sizeof("FRAME")-1);

      /* For formats not supported by the YUV4MPEG2 spec, we prepend
       * each frame with its size */
      if (module->non_standard)
      {
         unsigned int size;
         char line[32];
         size = snprintf(line, sizeof(line), " S%i",
            packet->frame_size ? packet->frame_size : packet->size);
         if (size < sizeof(line))
            WRITE_BYTES(ctx, line, size);
      }

      _WRITE_U8(ctx, 0x0a);
   }

   /* Write the elementary stream */
   WRITE_BYTES(ctx, packet->data, packet->size);

   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rawvideo_writer_control( VC_CONTAINER_T *ctx,
   VC_CONTAINER_CONTROL_T operation, va_list args )
{
    VC_CONTAINER_MODULE_T *module = ctx->priv->module;
    VC_CONTAINER_ES_FORMAT_T *format;

   switch (operation)
   {
   case VC_CONTAINER_CONTROL_TRACK_ADD:
      format = (VC_CONTAINER_ES_FORMAT_T *)va_arg(args, VC_CONTAINER_ES_FORMAT_T *);
      return simple_write_add_track(ctx, format);

   case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      return module->yuv4mpeg2 ?
         rawvideo_write_header( ctx ) : VC_CONTAINER_SUCCESS;

   default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T rawvideo_writer_open( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module;
   bool yuv4mpeg2 = false;

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(!strcasecmp(extension, "y4m") || !strcasecmp(extension, "yuv4mpeg2"))
      yuv4mpeg2 = true;
   if(!yuv4mpeg2 && strcasecmp(extension, "yuv"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(ctx, "using rawvideo writer");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if (!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = &module->track;
   module->yuv4mpeg2 = yuv4mpeg2;

   ctx->priv->pf_close = rawvideo_writer_close;
   ctx->priv->pf_write = rawvideo_writer_write;
   ctx->priv->pf_control = rawvideo_writer_control;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(ctx, "rawvideo: error opening stream (%i)", status);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open rawvideo_writer_open
#endif
