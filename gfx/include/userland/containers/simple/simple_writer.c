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

#include "simple_common.h"

/******************************************************************************
Defines.
******************************************************************************/
#define MAX_TRACKS 4
#define MAX_LINE_SIZE 512

#define ES_SUFFIX "%s.%2.2i.%4.4s"
#define ES_SUFFIX_SIZE 8

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   VC_CONTAINER_IO_T *io;
   char *uri;

   bool config_done;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   char line[MAX_LINE_SIZE + 1];

   VC_CONTAINER_TRACK_T *tracks[MAX_TRACKS];
   bool header_done;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T simple_writer_open( VC_CONTAINER_T * );
static VC_CONTAINER_STATUS_T simple_writer_write( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T simple_write_line( VC_CONTAINER_T *ctx,
   const char *format, ...)
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   va_list args;
   int result;

   va_start(args, format);
   result = vsnprintf(module->line, sizeof(module->line), format, args);
   va_end(args);

   if (result >= (int)sizeof(module->line))
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

   WRITE_BYTES(ctx, module->line, result);
   _WRITE_U8(ctx, '\n');
   return STREAM_STATUS(ctx);
}

static VC_CONTAINER_STATUS_T simple_write_header( VC_CONTAINER_T *ctx )
{
   unsigned int i;

   simple_write_line(ctx, SIGNATURE_STRING);

   for (i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];

      if (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
      {
         simple_write_line(ctx, "TRACK video, %4.4s, %i, %i",
            (char *)&track->format->codec,
            (int)track->format->type->video.width,
            (int)track->format->type->video.height);
         if ((track->format->type->video.visible_width &&
              track->format->type->video.visible_width !=
                 track->format->type->video.width) ||
             (track->format->type->video.visible_height &&
              track->format->type->video.visible_height !=
                 track->format->type->video.height))
            simple_write_line(ctx, CONFIG_VIDEO_CROP" %i, %i",
               track->format->type->video.visible_width,
               track->format->type->video.visible_height);
         if (track->format->type->video.par_num &&
             track->format->type->video.par_den)
            simple_write_line(ctx, CONFIG_VIDEO_ASPECT" %i, %i",
               track->format->type->video.par_num,
               track->format->type->video.par_den);
      }
      else if (track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      {
         simple_write_line(ctx, "TRACK audio, %4.4s, %i, %i, %i, %i",
            (char *)&track->format->codec,
            (int)track->format->type->audio.channels,
            (int)track->format->type->audio.sample_rate,
            (int)track->format->type->audio.bits_per_sample,
            (int)track->format->type->audio.block_align);
      }
      else if (track->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
      {
         simple_write_line(ctx, "TRACK subpicture, %4.4s, %i",
            (char *)&track->format->codec,
            (int)track->format->type->subpicture.encoding);
      }
      else
      {
         simple_write_line(ctx, "TRACK unknown, %4.4s",
            (char *)&track->format->codec);
      }

      simple_write_line(ctx, CONFIG_URI" %s", track->priv->module->io->uri);
      if (track->format->codec_variant)
         simple_write_line(ctx, CONFIG_CODEC_VARIANT" %4.4s",
            (char *)&track->format->codec_variant);
      if (track->format->bitrate)
         simple_write_line(ctx, CONFIG_BITRATE" %i", track->format->bitrate);
      if (!(track->format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
         simple_write_line(ctx, CONFIG_UNFRAMED);
   }

   simple_write_line(ctx, SIGNATURE_END_STRING);

   ctx->priv->module->header_done = true;
   return STREAM_STATUS(ctx);
}

static VC_CONTAINER_STATUS_T simple_write_config( VC_CONTAINER_T *ctx,
   unsigned int track_num, VC_CONTAINER_PACKET_T *pkt)
{
   VC_CONTAINER_TRACK_T *track = ctx->tracks[track_num];
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PACKET_T packet;

   track->priv->module->config_done = true;

   if (track->format->extradata_size)
   {
      packet.size = track->format->extradata_size;
      packet.data = track->format->extradata;
      packet.track = track_num;
      packet.pts = pkt ? pkt->pts : VC_CONTAINER_TIME_UNKNOWN;
      packet.flags = 0;
      packet.flags |= VC_CONTAINER_PACKET_FLAG_CONFIG;

      status = simple_writer_write(ctx, &packet);
   }

   return status;
}

static VC_CONTAINER_STATUS_T simple_write_add_track( VC_CONTAINER_T *ctx,
   VC_CONTAINER_ES_FORMAT_T *format )
{
   VC_CONTAINER_TRACK_T *track = NULL;
   VC_CONTAINER_STATUS_T status;
   const char *uri = vc_uri_path(ctx->priv->uri);
   unsigned int uri_size = strlen(uri);

   /* Allocate and initialise track data */
   if (ctx->tracks_num >= MAX_TRACKS)
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

   ctx->tracks[ctx->tracks_num] = track =
      vc_container_allocate_track(ctx, sizeof(VC_CONTAINER_TRACK_MODULE_T) +
         uri_size + ES_SUFFIX_SIZE + 1);
   if (!track)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   if (format->extradata_size)
   {
      status = vc_container_track_allocate_extradata(ctx, track, format->extradata_size);
      if (status != VC_CONTAINER_SUCCESS)
         goto error;
   }
   vc_container_format_copy(track->format, format, format->extradata_size);

   track->priv->module->uri = (char *)&track->priv->module[1];
   snprintf(track->priv->module->uri, uri_size + ES_SUFFIX_SIZE + 1,
      ES_SUFFIX, uri, ctx->tracks_num, (char *)&track->format->codec);

   LOG_DEBUG(ctx, "opening elementary stream: %s", track->priv->module->uri);
   track->priv->module->io = vc_container_io_open(track->priv->module->uri,
      VC_CONTAINER_IO_MODE_WRITE, &status);
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_ERROR(ctx, "error opening elementary stream: %s",
         track->priv->module->uri);
      goto error;
   }

   ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;

 error:
   if (track)
      vc_container_free_track(ctx, track);
   return status;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T simple_writer_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   for (; ctx->tracks_num > 0; ctx->tracks_num--)
   {
      vc_container_io_close(ctx->tracks[ctx->tracks_num-1]->priv->module->io);
      vc_container_free_track(ctx, ctx->tracks[ctx->tracks_num-1]);
   }
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T simple_writer_write( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet )
{
   VC_CONTAINER_STATUS_T status;

   if (!ctx->priv->module->header_done)
   {
      status = simple_write_header(ctx);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   if (!ctx->tracks[packet->track]->priv->module->config_done)
   {
      status = simple_write_config(ctx, packet->track, packet);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   /* Write the metadata */
   status = simple_write_line(ctx, "%i %i %"PRIi64" 0x%x",
      (int)packet->track, (int)packet->size, packet->pts, packet->flags);
   if (status != VC_CONTAINER_SUCCESS)
      return status;

   /* Write the elementary stream */
   vc_container_io_write(ctx->tracks[packet->track]->priv->module->io,
      packet->data, packet->size);

   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T simple_writer_control( VC_CONTAINER_T *ctx,
   VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_ES_FORMAT_T *format;

   switch (operation)
   {
   case VC_CONTAINER_CONTROL_TRACK_ADD:
      format = (VC_CONTAINER_ES_FORMAT_T *)va_arg(args, VC_CONTAINER_ES_FORMAT_T *);
      return simple_write_add_track(ctx, format);

   case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      simple_write_header( ctx );
      return VC_CONTAINER_SUCCESS;

   default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T simple_writer_open( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module;

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "smpl") && strcasecmp(extension, "simple"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(ctx, "using simple writer");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if (!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = module->tracks;

   ctx->priv->pf_close = simple_writer_close;
   ctx->priv->pf_write = simple_writer_write;
   ctx->priv->pf_control = simple_writer_control;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(ctx, "simple: error opening stream (%i)", status);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open simple_writer_open
#endif
