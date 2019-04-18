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

#include "simple_common.h"

/******************************************************************************
Defines.
******************************************************************************/
#define MAX_LINE_SIZE 512
#define LINE_PADDING 3 /* 2 for newline + 1 for null */

#define MAX_TRACKS 4
#define MAX_HEADER_LINES 512

typedef enum SIMPLE_VARIANT_T
{
   VARIANT_DEFAULT = 0,
   VARIANT_MMAL,
   VARIANT_OMX
} SIMPLE_VARIANT_T;

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct SIMPLE_PACKET_STATE_T
{
   unsigned int track_num;
   unsigned int flags;

   uint64_t metadata_offset; /* Offset in metadata stream */
   uint32_t data_size;       /* Size of current data packet */
   uint32_t data_left;       /* Data left to read in current packet */

   int64_t pts;

} SIMPLE_PACKET_STATE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   SIMPLE_PACKET_STATE_T *state;
   SIMPLE_PACKET_STATE_T local_state;

   VC_CONTAINER_IO_T *io;
   uint64_t data_offset;     /* Current offset in data stream */
   char uri[MAX_LINE_SIZE+1];

   SIMPLE_VARIANT_T variant;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *tracks[MAX_TRACKS];

   char line[MAX_LINE_SIZE + LINE_PADDING];

   int64_t metadata_offset;

   /* Shared packet state. This is used when the tracks are in sync,
    * and for the track at the earliest position in the file when they are
    * not in sync */
   SIMPLE_PACKET_STATE_T state;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T simple_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T simple_read_line( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int i, bytes = PEEK_BYTES(ctx, module->line, sizeof(module->line)-1);

   if (!bytes)
      return VC_CONTAINER_ERROR_EOS;

   /* Find new-line marker */
   for (i = 0; i < bytes; i++)
      if (module->line[i] == '\n')
         break;

   /* Bail out if line is bigger than the maximum allowed */
   if (i == sizeof(module->line)-1)
   {
      LOG_ERROR(ctx, "line too big");
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   if (i < bytes)
   {
      module->line[i++] = 0;
      if (i < bytes && module->line[i] == '\r')
         i++;
   }
   module->line[i] = 0; /* Make sure the line is null terminated */

   SKIP_BYTES(ctx, i);
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T simple_read_header( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_TRACK_T *track = NULL;
   VC_CONTAINER_FOURCC_T fourcc;
   int matches, width, height, channels, samplerate, bps, blockalign, value;
   unsigned int lines = 1;

   /* Skip the signature */
   if (simple_read_line(ctx) != VC_CONTAINER_SUCCESS)
      return VC_CONTAINER_ERROR_CORRUPTED;

   while (lines++ < MAX_HEADER_LINES &&
          simple_read_line(ctx) == VC_CONTAINER_SUCCESS)
   {
      /* Our exit condition is the end signature */
      if (!memcmp(module->line, SIGNATURE_END_STRING, sizeof(SIGNATURE_STRING)-1))
      {
         if (track) ctx->tracks[ctx->tracks_num++] = track;
         return VC_CONTAINER_SUCCESS;
      }

      /* Start of track description */
      if (!memcmp(module->line, "TRACK ", sizeof("TRACK ")-1))
      {
         /* Add track we were constructing */
         if (track) ctx->tracks[ctx->tracks_num++] = track;
         track = NULL;

         if (ctx->tracks_num >= MAX_TRACKS)
         {
            LOG_ERROR(ctx, "too many tracks, ignoring: %s", module->line);
            continue;
         }
         track = vc_container_allocate_track(ctx, sizeof(*track->priv->module));
         if (!track)
            return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

         track->is_enabled = true;
         track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;

         if ((matches = sscanf(module->line,
                 "TRACK video, %4c, %i, %i",
                 (char *)&fourcc, &width, &height)) > 0)
         {
            track->format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
            track->format->codec = fourcc;
            if (matches > 1) track->format->type->video.width = width;
            if (matches > 2) track->format->type->video.height = height;
         }
         else if ((matches = sscanf(module->line,
                 "TRACK audio, %4c, %i, %i, %i, %i",
                 (char *)&fourcc, &channels, &samplerate, &bps,
                 &blockalign)) > 0)
         {
            track->format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
            track->format->codec = fourcc;
            if (matches > 1) track->format->type->audio.channels = channels;
            if (matches > 2) track->format->type->audio.sample_rate = samplerate;
            if (matches > 3) track->format->type->audio.bits_per_sample = bps;
            if (matches > 4) track->format->type->audio.block_align = blockalign;
         }
         if ((matches = sscanf(module->line,
                 "TRACK subpicture, %4c, %i",
                 (char *)&fourcc, &value)) > 0)
         {
            track->format->es_type = VC_CONTAINER_ES_TYPE_SUBPICTURE;
            track->format->codec = fourcc;
            if (matches > 1) track->format->type->subpicture.encoding = value;
         }
      }

      if (!track)
         continue; /* Nothing interesting */

      /* VARIANT of the syntax */
      if (sscanf(module->line, CONFIG_VARIANT" %i", &value) == 1)
      {
         track->priv->module->variant = value;
         LOG_FORMAT(ctx, CONFIG_VARIANT": %i", value);
      }
      /* URI for elementary stream */
      else if (sscanf(module->line, CONFIG_URI" %s", track->priv->module->uri) == 1)
         LOG_FORMAT(ctx, CONFIG_URI": %s", track->priv->module->uri);
      /* COCDEC_VARIANT of elementary stream */
      else if (sscanf(module->line, CONFIG_CODEC_VARIANT" %4c", (char *)&fourcc) == 1)
      {
         track->format->codec_variant = fourcc;
         LOG_FORMAT(ctx, CONFIG_CODEC_VARIANT": %4.4s", (char *)&fourcc);
      }
      /* BITRATE of elementary stream */
      else if (sscanf(module->line, CONFIG_BITRATE" %i", &value) == 1)
      {
         track->format->bitrate = value;
         LOG_FORMAT(ctx, CONFIG_BITRATE": %i", value);
      }
      /* UNFRAMED elementary stream */
      else if (!memcmp(module->line, CONFIG_UNFRAMED, sizeof(CONFIG_UNFRAMED)-1))
      {
         track->format->flags &= ~VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         LOG_FORMAT(ctx, CONFIG_UNFRAMED);
      }
      /* VIDEO_CROP information */
      else if (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO &&
         sscanf(module->line, CONFIG_VIDEO_CROP" %i, %i", &width, &height) == 2)
      {
         track->format->type->video.visible_width = width;
         track->format->type->video.visible_height = height;
         LOG_FORMAT(ctx, CONFIG_VIDEO_CROP": %i, %i", width, height);
      }
      /* VIDEO_ASPECT information */
      else if (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO &&
         sscanf(module->line, CONFIG_VIDEO_ASPECT" %i, %i", &width, &height) == 2)
      {
         track->format->type->video.par_num = width;
         track->format->type->video.par_den = height;
         LOG_FORMAT(ctx, CONFIG_VIDEO_ASPECT": %i, %i", width, height);
      }
   }

   if (track) vc_container_free_track(ctx, track);
   return VC_CONTAINER_ERROR_CORRUPTED;
}

static uint32_t simple_convert_packet_flags(VC_CONTAINER_T *ctx,
   unsigned int track_num, uint32_t flags)
{
   typedef struct { uint32_t from; uint32_t to; } convert_from_t;
   const convert_from_t convert_from_mmal[] =
   { {1<<1, VC_CONTAINER_PACKET_FLAG_FRAME_START},
     {1<<2, VC_CONTAINER_PACKET_FLAG_FRAME_END},
     {1<<3, VC_CONTAINER_PACKET_FLAG_KEYFRAME},
     {1<<4, VC_CONTAINER_PACKET_FLAG_DISCONTINUITY},
     {1<<5, VC_CONTAINER_PACKET_FLAG_CONFIG},
     {1<<6, VC_CONTAINER_PACKET_FLAG_ENCRYPTED},
     {0, 0} };
   const convert_from_t convert_from_omx[] =
   { {0x10, VC_CONTAINER_PACKET_FLAG_FRAME_END},
     {0x20, VC_CONTAINER_PACKET_FLAG_KEYFRAME},
     {0x80, VC_CONTAINER_PACKET_FLAG_CONFIG},
     {0, 0} };
   const convert_from_t *convert_from = NULL;
   int i;

   switch (ctx->tracks[track_num]->priv->module->variant)
   {
      case VARIANT_MMAL: convert_from = convert_from_mmal; break;
      case VARIANT_OMX: convert_from = convert_from_omx; break;
      default: break;
   }

   if (convert_from)
   {
      uint32_t new_flags = 0;
      for (i = 0; convert_from[i].from; i++)
         if (convert_from[i].from & flags)
            new_flags |= convert_from[i].to;
      return new_flags;
   }

   return flags;
}

static int64_t simple_convert_packet_pts(VC_CONTAINER_T *ctx,
   unsigned int track_num, int64_t pts, uint32_t flags)
{
   if (ctx->tracks[track_num]->priv->module->variant == VARIANT_OMX &&
       flags & 0x100)
      return VC_CONTAINER_TIME_UNKNOWN;

   return pts;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T simple_reader_read( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *track_module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   SIMPLE_PACKET_STATE_T *state;

   /* If a specific track has been selected, use the track packet state */
   if (flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
      state = ctx->tracks[packet->track]->priv->module->state;
   else
      state = &module->state;

   /* Switch to the next packet when the current one is empty */
   if (!state->data_left)
   {
      unsigned int track_num, size;
      int64_t pts;
      int flags;

      SEEK(ctx, state->metadata_offset);
      status = simple_read_line(ctx);
      if (status != VC_CONTAINER_SUCCESS)
         return status;

      if (sscanf(module->line, "%u %u %"PRIi64" %i",
             &track_num, &size, &pts, &flags) != 4 &&
           (track_num = 0, sscanf(module->line, "%u %"PRIi64" %i",
             &size, &pts, &flags)) != 3)
      {
         LOG_ERROR(ctx, "invalid metadata: %s", module->line);
         return VC_CONTAINER_ERROR_CORRUPTED;
      }
      state->metadata_offset = STREAM_POSITION(ctx);

      if (track_num >= ctx->tracks_num)
      {
         LOG_DEBUG(ctx, "skipping %i bytes for track %d/%d",
            size, track_num, ctx->tracks_num);
         return VC_CONTAINER_ERROR_CONTINUE;
      }

      /* If we are reading from the global state (i.e. normal read or forced
         read from the track on the global state), and the track we found is
         not on the global state, reconnect the two */
      if (state == &module->state &&
         ctx->tracks[track_num]->priv->module->state != &module->state)
      {
         LOG_DEBUG(ctx, "reconnect track %u to the global state", track_num);
         ctx->tracks[track_num]->priv->module->state = &module->state;
         module->state = ctx->tracks[track_num]->priv->module->local_state;
         return VC_CONTAINER_ERROR_CONTINUE;
      }

      state->data_size = state->data_left = size;
      state->track_num = track_num;
      state->flags = simple_convert_packet_flags(ctx, track_num, flags);
      state->pts = simple_convert_packet_pts(ctx, track_num, pts, flags);

      /* Discard empty packets */
      if (!state->data_size && !state->flags)
         return VC_CONTAINER_ERROR_CONTINUE;
   }

   /* If there is data from another track skip past it */
   if ((flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK) &&
       state->track_num != packet->track)
   {
      LOG_DEBUG(ctx, "skipping track %d/%d as we are ignoring it",
         state->track_num, ctx->tracks_num);

      track_module = ctx->tracks[packet->track]->priv->module;

      /* Handle disconnection from global state */
      if (state == &module->state &&
         ctx->tracks[state->track_num]->priv->module->state == &module->state)
      {
         /* Make a copy of the global state */
         LOG_DEBUG(ctx, "using local state on track %d", packet->track);
         track_module->local_state = module->state;
         track_module->state = &track_module->local_state;
      }

      track_module->state->data_left = 0;
      return VC_CONTAINER_ERROR_CONTINUE;
   }

   /*
    * From this point we know we have the packet which was requested
    */

   /* !!!! If we aren't in the right position in the file go there now. */

   track_module = ctx->tracks[state->track_num]->priv->module;
   packet->track = state->track_num;
   packet->size = state->data_left;
   packet->frame_size = (state->flags & VC_CONTAINER_PACKET_FLAG_FRAME) ?
      state->data_size : 0;
   packet->flags = state->flags;
   packet->pts = state->pts;
   packet->dts = VC_CONTAINER_TIME_UNKNOWN;
   if (state->data_left != state->data_size)
      packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_START;

   if (flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      track_module->data_offset += state->data_left;
      state->data_left = 0;
      return VC_CONTAINER_SUCCESS;
   }

   if (flags & VC_CONTAINER_READ_FLAG_INFO)
   {
      return VC_CONTAINER_SUCCESS;
   }

   /* Now try to read data into buffer */
   vc_container_io_seek(track_module->io, track_module->data_offset);

   packet->size = vc_container_io_read(track_module->io, packet->data,
      MIN(packet->buffer_size, state->data_left));
   state->data_left -= packet->size;
   track_module->data_offset += packet->size;

   if (state->data_left)
      packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_END;

   return track_module->io->status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T simple_reader_seek( VC_CONTAINER_T *ctx, int64_t *offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(offset);
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);
   return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T simple_reader_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;

   for (; ctx->tracks_num > 0; ctx->tracks_num--)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[ctx->tracks_num-1];
      if (track->priv->module->io)
         vc_container_io_close(track->priv->module->io);
      vc_container_free_track(ctx, track);
   }

   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T simple_reader_open( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   uint8_t h[sizeof(SIGNATURE_STRING)];
   unsigned int i;

   /* Check for the signature */
   if (PEEK_BYTES(ctx, h, sizeof(h)) != sizeof(h) ||
      memcmp(h, SIGNATURE_STRING, sizeof(SIGNATURE_STRING)-1))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(ctx, "using simple reader");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if (!module) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = module->tracks;

   status = simple_read_header(ctx);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   /* Open all the elementary streams */
   for (i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];
      char *uri;

      track->priv->module->io = vc_container_io_open(track->priv->module->uri,
         VC_CONTAINER_IO_MODE_READ, &status);

      /* URI might be relative to the path of the metadata file so
       * try again with that new path */
      if (!track->priv->module->io &&
          (uri = malloc(strlen(ctx->priv->io->uri) +
              strlen(track->priv->module->uri) + 1)) != NULL)
      {
         char *end;

         strcpy(uri, ctx->priv->io->uri);

         /* Find the last directory separator */
         for (end = uri + strlen(ctx->priv->io->uri) + 1; end != uri; end--)
            if (*(end-1) == '/' || *(end-1) == '\\')
               break;
         strcpy(end, track->priv->module->uri);

         track->priv->module->io = vc_container_io_open(uri,
            VC_CONTAINER_IO_MODE_READ, &status);
         if (!track->priv->module->io)
            LOG_ERROR(ctx, "could not open elementary stream: %s", uri);
         free(uri);
      }
      if (!track->priv->module->io)
      {
         LOG_ERROR(ctx, "could not open elementary stream: %s",
            track->priv->module->uri);
         goto error;
      }
   }

   /*
    *  We now have all the information we really need to start playing the stream
    */

   module->metadata_offset = STREAM_POSITION(ctx);

   /* Initialise state for all tracks */
   module->state.metadata_offset = module->metadata_offset;
   for (i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];
      track->priv->module->state = &module->state;
   }

   /* Look for the codec configuration data for each track so
    * we can store it in the track format */
   for (i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];
      VC_CONTAINER_PACKET_T packet;
      packet.track = i;
      status = VC_CONTAINER_ERROR_CONTINUE;

      while (status == VC_CONTAINER_ERROR_CONTINUE)
         status = simple_reader_read(ctx, &packet,
            VC_CONTAINER_READ_FLAG_INFO | VC_CONTAINER_READ_FLAG_FORCE_TRACK);
      if (status != VC_CONTAINER_SUCCESS)
         continue;

      status = vc_container_track_allocate_extradata(ctx, track, packet.size);
      if (status != VC_CONTAINER_SUCCESS)
         continue;

      packet.data = track->format->extradata;
      packet.buffer_size = packet.size;
      packet.size = 0;
      status = simple_reader_read(ctx, &packet,
         VC_CONTAINER_READ_FLAG_FORCE_TRACK);
      if (status != VC_CONTAINER_SUCCESS)
         continue;

      track->format->extradata_size = packet.size;
   }

   ctx->priv->pf_close = simple_reader_close;
   ctx->priv->pf_read = simple_reader_read;
   ctx->priv->pf_seek = simple_reader_seek;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_ERROR(ctx, "simple: error opening stream (%i)", status);
   simple_reader_close(ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open simple_reader_open
#endif
