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
#include "containers/core/containers_io.h"
#include "containers/core/containers_filters.h"
#include "containers/core/containers_loader.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_utils.h"

#define WRITER_SPACE_SAFETY_MARGIN (10*1024)
#define PACKETIZER_BUFFER_SIZE (32*1024)

/*****************************************************************************/
VC_CONTAINER_T *vc_container_open_reader_with_io( struct VC_CONTAINER_IO_T *io,
   const char *uri, VC_CONTAINER_STATUS_T *p_status,
   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pfn_progress, void *progress_userdata)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_T *p_ctx = 0;
   const char *extension;

   VC_CONTAINER_PARAM_UNUSED(pfn_progress);
   VC_CONTAINER_PARAM_UNUSED(progress_userdata);
   VC_CONTAINER_PARAM_UNUSED(uri);

   /* Sanity check the i/o */
   if (!io || !io->pf_read || !io->pf_seek)
   {
      LOG_ERROR(0, "invalid i/o instance: %p", io);
      status = VC_CONTAINER_ERROR_INVALID_ARGUMENT;
      goto error;
   }

   /* Allocate our context before trying out the different readers / writers */
   p_ctx = malloc( sizeof(*p_ctx) + sizeof(*p_ctx->priv) + sizeof(*p_ctx->drm));
   if(!p_ctx) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(p_ctx, 0, sizeof(*p_ctx) + sizeof(*p_ctx->priv) + sizeof(*p_ctx->drm));
   p_ctx->priv = (VC_CONTAINER_PRIVATE_T *)(p_ctx + 1);
   p_ctx->priv->verbosity = vc_container_log_get_default_verbosity();
   p_ctx->drm = (VC_CONTAINER_DRM_T *)(p_ctx->priv + 1);
   p_ctx->size = io->size;
   p_ctx->priv->io = io;
   p_ctx->priv->uri = io->uri_parts;

   /* If the uri has an extension, use it as a hint when loading the container */
   extension = vc_uri_path_extension(p_ctx->priv->uri);
   /* If the user has specified a container, then use that instead */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   status = vc_container_load_reader(p_ctx, extension);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   p_ctx->priv->drm_filter = vc_container_filter_open(VC_FOURCC('d','r','m',' '),
      VC_FOURCC('u','n','k','n'), p_ctx, &status);
   if (status != VC_CONTAINER_SUCCESS)
   {
      /* Some other problem occurred aside from the filter not being appropriate or
         the stream not needing it. */
      if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) goto error;

      /* Report no DRM and continue as normal */
      p_ctx->drm = NULL;
      status = VC_CONTAINER_SUCCESS;
   }

end:
   if(p_status) *p_status = status;
   return p_ctx;

error:
   if (p_ctx)
   {
      p_ctx->priv->io = NULL; /* The i/o doesn't belong to us */
      vc_container_close(p_ctx);
      p_ctx = NULL;
   }
   goto end;
}

/*****************************************************************************/
VC_CONTAINER_T *vc_container_open_reader( const char *uri, VC_CONTAINER_STATUS_T *p_status,
   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pfn_progress, void *progress_userdata)
{
   VC_CONTAINER_IO_T *io;
   VC_CONTAINER_T *ctx;

   /* Start by opening the URI */
   io = vc_container_io_open( uri, VC_CONTAINER_IO_MODE_READ, p_status );
   if (!io)
      return 0;

   ctx = vc_container_open_reader_with_io( io, uri, p_status, pfn_progress, progress_userdata);
   if (!ctx)
      vc_container_io_close(io);
   return ctx;
}

/*****************************************************************************/
VC_CONTAINER_T *vc_container_open_writer( const char *uri, VC_CONTAINER_STATUS_T *p_status,
   VC_CONTAINER_PROGRESS_REPORT_FUNC_T pfn_progress, void *progress_userdata)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_T *p_ctx = 0;
   VC_CONTAINER_IO_T *io;
   const char *extension;
   VC_CONTAINER_PARAM_UNUSED(pfn_progress);
   VC_CONTAINER_PARAM_UNUSED(progress_userdata);

   /* Start by opening the URI */
   io = vc_container_io_open( uri, VC_CONTAINER_IO_MODE_WRITE, &status );
   if(!io) goto error;

   /* Make sure we have enough space available to start writing */
   if(io->max_size && io->max_size < WRITER_SPACE_SAFETY_MARGIN)
   {
      LOG_DEBUG(p_ctx, "not enough space available to open a writer");
      status = VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
      goto error;
   }

   /* Allocate our context before trying out the different readers / writers */
   p_ctx = malloc( sizeof(*p_ctx) + sizeof(*p_ctx->priv));
   if(!p_ctx) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(p_ctx, 0, sizeof(*p_ctx) + sizeof(*p_ctx->priv));
   p_ctx->priv = (VC_CONTAINER_PRIVATE_T *)(p_ctx + 1);
   p_ctx->priv->verbosity = vc_container_log_get_default_verbosity();
   p_ctx->priv->io = io;
   p_ctx->priv->uri = io->uri_parts;
   io = NULL; /* io now owned by the context */

   /* If the uri has an extension, use it as a hint when loading the container */
   extension = vc_uri_path_extension(p_ctx->priv->uri);
   /* If the user has specified a container, then use that instead */
   vc_uri_find_query(p_ctx->priv->uri, 0, "container", &extension);

   status = vc_container_load_writer(p_ctx, extension);
   if(status != VC_CONTAINER_SUCCESS) goto error;

 end:
   if(p_status) *p_status = status;
   return p_ctx;

error:
   if(io) vc_container_io_close(io);
   if (p_ctx) vc_container_close(p_ctx);
   p_ctx = NULL;
   goto end;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_close( VC_CONTAINER_T *p_ctx )
{
   unsigned int i;

   if(!p_ctx)
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   for(i = 0; i < p_ctx->tracks_num; i++)
      if(p_ctx->tracks[i]->priv->packetizer)
         vc_packetizer_close(p_ctx->tracks[i]->priv->packetizer);
   if(p_ctx->priv->packetizer_buffer) free(p_ctx->priv->packetizer_buffer);
   if(p_ctx->priv->drm_filter) vc_container_filter_close(p_ctx->priv->drm_filter);
   if(p_ctx->priv->pf_close) p_ctx->priv->pf_close(p_ctx);
   if(p_ctx->priv->io) vc_container_io_close(p_ctx->priv->io);
   if(p_ctx->priv->module_handle) vc_container_unload(p_ctx);
   for(i = 0; i < p_ctx->meta_num; i++) free(p_ctx->meta[i]);
   if(p_ctx->meta_num) free(p_ctx->meta);
   p_ctx->meta_num = 0;
   free(p_ctx);

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T container_read_packet( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_STATUS_T status;

   while(1)
   {
      status = p_ctx->priv->pf_read(p_ctx, p_packet, flags);
      if(status == VC_CONTAINER_ERROR_CONTINUE)
         continue;

      if(!p_packet || (flags & VC_CONTAINER_READ_FLAG_SKIP))
         return status; /* We've just been requested to skip the data */

      if(status != VC_CONTAINER_SUCCESS)
         return status;

      /* Skip data from out of bounds tracks, disabled tracks or packets that are encrypted
         and cannot be decrypted */
      if(p_packet->track >= p_ctx->tracks_num ||
         !p_ctx->tracks[p_packet->track]->is_enabled ||
         ((p_packet->flags & VC_CONTAINER_PACKET_FLAG_ENCRYPTED) && !p_ctx->priv->drm_filter))
      {
         if(flags & VC_CONTAINER_READ_FLAG_INFO)
            status = p_ctx->priv->pf_read(p_ctx, p_packet, VC_CONTAINER_READ_FLAG_SKIP);
         if(status == VC_CONTAINER_SUCCESS || status == VC_CONTAINER_ERROR_CONTINUE)
            continue;
      }
      if(status != VC_CONTAINER_SUCCESS)
         return status;

      if(p_ctx->priv->drm_filter)
         status = vc_container_filter_process(p_ctx->priv->drm_filter, p_packet);

      break;
   }
   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_read( VC_CONTAINER_T *p_ctx, VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_CONTINUE;
   VC_PACKETIZER_FLAGS_T packetizer_flags = 0;
   VC_PACKETIZER_T *packetizer;
   uint32_t force = flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK;
   unsigned int i;

   if(!p_packet && !(flags & VC_CONTAINER_READ_FLAG_SKIP))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   if(!p_packet && (flags & VC_CONTAINER_READ_FLAG_INFO))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   if(p_packet && !p_packet->data && !(flags & (VC_CONTAINER_READ_FLAG_INFO | VC_CONTAINER_READ_FLAG_SKIP)))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   if((flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK) &&
      (!p_packet || p_packet->track >= p_ctx->tracks_num || !p_ctx->tracks[p_packet->track]->is_enabled))
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   /* Always having a packet structure to work with simplifies things */
   if(!p_packet)
      p_packet = &p_ctx->priv->packetizer_packet;

   /* Simple/Fast case first */
   if(!p_ctx->priv->packetizing)
   {
      status = container_read_packet( p_ctx, p_packet, flags );
      goto end;
   }

   if(flags & VC_CONTAINER_READ_FLAG_INFO)
      packetizer_flags |= VC_PACKETIZER_FLAG_INFO;
   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
      packetizer_flags |= VC_PACKETIZER_FLAG_SKIP;

   /* Loop through all the packetized tracks first to see if we've got any
    * data to consume there */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_PACKETIZER_T *packetizer = p_ctx->tracks[i]->priv->packetizer;
      if(!p_ctx->tracks[i]->is_enabled || !packetizer ||
         (force && i != p_packet->track))
         continue;

      status = vc_packetizer_read(packetizer, p_packet, packetizer_flags);
      p_packet->track = i;
      if(status == VC_CONTAINER_SUCCESS)
         break;
   }
   if(i < p_ctx->tracks_num) /* We've got some data */
      goto end;

   /* Let's go and read some data from the actual container */
   while(1)
   {
      VC_CONTAINER_PACKET_T *tmp = &p_ctx->priv->packetizer_packet;
      tmp->track = p_packet->track;

      /* Let's check what the container has to offer */
      status = container_read_packet( p_ctx, tmp, force|VC_PACKETIZER_FLAG_INFO );
      if(status != VC_CONTAINER_SUCCESS)
         return status;

      if(!p_ctx->tracks[tmp->track]->priv->packetizer)
      {
         status = container_read_packet( p_ctx, p_packet, flags );
         break;
      }

      /* Feed data from the container onto the packetizer */
      packetizer = p_ctx->tracks[tmp->track]->priv->packetizer;

      tmp->data = p_ctx->priv->packetizer_buffer;
      tmp->buffer_size = PACKETIZER_BUFFER_SIZE;
      tmp->size = 0;
      status = container_read_packet( p_ctx, tmp, force );
      if(status != VC_CONTAINER_SUCCESS)
         return status;

      p_packet->track = tmp->track;
      vc_packetizer_push(packetizer, tmp);
      vc_packetizer_pop(packetizer, &tmp, VC_PACKETIZER_FLAG_FORCE_RELEASE_INPUT);

      status = vc_packetizer_read(packetizer, p_packet, packetizer_flags);
      if(status == VC_CONTAINER_SUCCESS)
         break;
   }

 end:
   if(status != VC_CONTAINER_SUCCESS)
      return status;

   if(p_packet && p_packet->dts > p_ctx->position)
      p_ctx->position = p_packet->dts;
   if(p_packet && p_packet->pts > p_ctx->position)
      p_ctx->position = p_packet->pts;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_write( VC_CONTAINER_T *p_ctx, VC_CONTAINER_PACKET_T *p_packet )
{
   VC_CONTAINER_STATUS_T status;
   void * p_metadata_buffer = NULL;
   uint32_t metadata_length = 0;

   /* TODO: check other similar argument errors and non-stateless errors */
   if (!p_packet || !p_packet->data || p_packet->track >= p_ctx->tracks_num)
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   /* Check for a previous error */
   if(p_ctx->priv->status != VC_CONTAINER_SUCCESS && p_ctx->priv->status != VC_CONTAINER_ERROR_NOT_READY)
      return p_ctx->priv->status;

   /* Check we have enough space to write the data */
   if(p_ctx->priv->max_size &&
      p_ctx->size + p_packet->size + WRITER_SPACE_SAFETY_MARGIN > p_ctx->priv->max_size)
   {status = VC_CONTAINER_ERROR_LIMIT_REACHED; goto end;}
   if(p_ctx->priv->io->max_size &&
      p_ctx->size + p_packet->size + WRITER_SPACE_SAFETY_MARGIN +
         (p_ctx->priv->tmp_io ? p_ctx->priv->tmp_io->offset : 0) > p_ctx->priv->io->max_size)
   {status = VC_CONTAINER_ERROR_OUT_OF_SPACE; goto end;}

   /* If a filter is created, then send the packet to the filter for encryption. */
   if(p_ctx->priv->drm_filter)
   {
      status = vc_container_filter_process(p_ctx->priv->drm_filter, p_packet);

      if(status == VC_CONTAINER_SUCCESS)
      {
         /* Get the encryption metadata and send it to the output first. */
         if(vc_container_control(p_ctx, VC_CONTAINER_CONTROL_GET_DRM_METADATA,
             &p_metadata_buffer, &metadata_length) == VC_CONTAINER_SUCCESS && metadata_length > 0)
         {
            /* Make a packet up with the metadata in the payload and write it. */
            VC_CONTAINER_PACKET_T metadata_packet;
            metadata_packet.data = p_metadata_buffer;
            metadata_packet.buffer_size = metadata_length;
            metadata_packet.size = metadata_length;
            metadata_packet.frame_size = p_packet->frame_size + metadata_length;
            metadata_packet.pts = p_packet->pts;
            metadata_packet.dts = p_packet->dts;
            metadata_packet.num = p_packet->num;
            metadata_packet.track = p_packet->track;
            /* As this packet is written first, we must transfer any frame start
               flag from the following packet. Also, this packet can never have the end flag set. */
            metadata_packet.flags = p_packet->flags & ~VC_CONTAINER_PACKET_FLAG_FRAME_END;

            p_packet->pts = p_packet->dts = VC_CONTAINER_TIME_UNKNOWN;
            p_packet->flags &= ~VC_CONTAINER_PACKET_FLAG_FRAME_START;
            if(p_ctx->priv->pf_write(p_ctx, &metadata_packet) != VC_CONTAINER_SUCCESS) goto end;
         }
      }
      else if (status != VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION)
      {
         /* Encryption was appropriate but a problem has occurred. Skip the write of data
            to the io and return the status to the caller. */
         goto end;
      }
   }

   status = p_ctx->priv->pf_write(p_ctx, p_packet);

 end:
   p_ctx->priv->status = status;
   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_seek( VC_CONTAINER_T *p_ctx, int64_t *p_offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_STATUS_T status;
   int64_t offset = *p_offset;
   unsigned int i;

   /* Reset all packetizers */
   for(i = 0; i < p_ctx->tracks_num; i++)
      if(p_ctx->tracks[i]->priv->packetizer)
         vc_packetizer_reset(p_ctx->tracks[i]->priv->packetizer);

   status = p_ctx->priv->pf_seek(p_ctx, p_offset, mode, flags);

   /* */
   if(status == VC_CONTAINER_SUCCESS && (flags & VC_CONTAINER_SEEK_FLAG_FORWARD) &&
      *p_offset < offset)
   {
      LOG_DEBUG(p_ctx, "container didn't seek forward as requested (%"PRIi64"/%"PRIi64")",
                *p_offset, offset);
      for(i = 1; i <= 5 && *p_offset < offset; i++)
      {
         *p_offset = offset + i * i * 100000;
         status = p_ctx->priv->pf_seek(p_ctx, p_offset, mode, flags);
         if(status != VC_CONTAINER_SUCCESS) break;
      }
   }

   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_control( VC_CONTAINER_T *p_ctx, VC_CONTAINER_CONTROL_T operation, ... )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   va_list args;

   va_start( args, operation );

   if(operation == VC_CONTAINER_CONTROL_ENCRYPT_TRACK)
   {
      VC_CONTAINER_FOURCC_T type = va_arg(args, VC_CONTAINER_FOURCC_T);

      uint32_t track_num = va_arg(args, uint32_t);
      void *p_drm_data = va_arg(args, void *);
      int drm_data_size = va_arg(args, uint32_t);

      if(!p_ctx->priv->drm_filter && track_num < p_ctx->tracks_num)
      {
         VC_CONTAINER_TRACK_T * p_track = p_ctx->tracks[track_num];

         if ((status = vc_container_track_allocate_drmdata(p_ctx, p_track, drm_data_size)) != VC_CONTAINER_SUCCESS)
         {
            LOG_DEBUG(p_ctx, "failed to allocate memory for 'drm data' (%d bytes)", drm_data_size);
            goto end;
         }
         memcpy(p_track->priv->drmdata, p_drm_data, drm_data_size);

         /* Track encryption enabled and no filter - create one now. */
         p_ctx->priv->drm_filter = vc_container_filter_open(VC_FOURCC('d','r','m',' '), type, p_ctx, &status);
         if(status != VC_CONTAINER_SUCCESS)
            goto end;

         status = vc_container_filter_control(p_ctx->priv->drm_filter, operation, track_num);
      }
   }
   else if(operation == VC_CONTAINER_CONTROL_GET_DRM_METADATA)
   {
      void *p_drm_data = va_arg(args, void *);
      int *drm_data_size = va_arg(args, int *);
      status = vc_container_filter_control(p_ctx->priv->drm_filter, operation, p_drm_data, drm_data_size);
   }

   if(status == VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION && p_ctx->priv->pf_control)
      status = p_ctx->priv->pf_control(p_ctx, operation, args);

   /* If the request has already been handled then we're done */
   if(status != VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION)
      goto end;

   switch(operation)
   {
   case VC_CONTAINER_CONTROL_DRM_PLAY:
      if (p_ctx->priv->drm_filter)
         status = vc_container_filter_control(p_ctx->priv->drm_filter, operation, args);
      break;

   case VC_CONTAINER_CONTROL_SET_MAXIMUM_SIZE:
      p_ctx->priv->max_size = (uint64_t)va_arg( args, uint64_t );
      if(p_ctx->priv->io->max_size &&
         p_ctx->priv->max_size > p_ctx->priv->io->max_size)
         p_ctx->priv->max_size = p_ctx->priv->io->max_size;
      status = VC_CONTAINER_SUCCESS;
      break;

   case VC_CONTAINER_CONTROL_TRACK_PACKETIZE:
      {
         unsigned int track_num = va_arg(args, unsigned int);
         VC_CONTAINER_FOURCC_T fourcc = va_arg(args, VC_CONTAINER_FOURCC_T);
         VC_CONTAINER_TRACK_T *p_track;

         if(track_num >= p_ctx->tracks_num)
         {
            status = VC_CONTAINER_ERROR_INVALID_ARGUMENT;
            break;
         }
         if(p_ctx->tracks[track_num]->priv->packetizer)
         {
            status = VC_CONTAINER_SUCCESS;
            break;
         }

         p_track = p_ctx->tracks[track_num];
         p_track->priv->packetizer = vc_packetizer_open( p_track->format, fourcc, &status );
         if(!p_track->priv->packetizer)
            break;

         if(!p_ctx->priv->packetizer_buffer)
         {
            p_ctx->priv->packetizer_buffer = malloc(PACKETIZER_BUFFER_SIZE);
            if(!p_ctx->priv->packetizer_buffer)
            {
               status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
               vc_packetizer_close(p_track->priv->packetizer);
               p_track->priv->packetizer = NULL;
               break;
            }
         }

         vc_container_format_copy(p_track->format, p_track->priv->packetizer->out,
            p_track->format->extradata_size);
         p_track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
         p_ctx->priv->packetizing = true;
      }
      break;

   default: break;
   }

   if (status == VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION)
      status = vc_container_io_control_list(p_ctx->priv->io, operation, args);

 end:
   va_end( args );
   return status;
}

/*****************************************************************************/
VC_CONTAINER_TRACK_T *vc_container_allocate_track( VC_CONTAINER_T *context, unsigned int extra_size )
{
   VC_CONTAINER_TRACK_T *p_ctx;
   unsigned int size;
   VC_CONTAINER_PARAM_UNUSED(context);

   size = sizeof(*p_ctx) + sizeof(*p_ctx->priv) + sizeof(*p_ctx->format) +
      sizeof(*p_ctx->format->type) + extra_size;

   p_ctx = malloc(size);
   if(!p_ctx) return 0;

   memset(p_ctx, 0, size);
   p_ctx->priv = (VC_CONTAINER_TRACK_PRIVATE_T *)(p_ctx + 1);
   p_ctx->format = (VC_CONTAINER_ES_FORMAT_T *)(p_ctx->priv + 1);
   p_ctx->format->type = (void *)(p_ctx->format + 1);
   p_ctx->priv->module = (struct VC_CONTAINER_TRACK_MODULE_T *)(p_ctx->format->type + 1);
   return p_ctx;
}

/*****************************************************************************/
void vc_container_free_track( VC_CONTAINER_T *context, VC_CONTAINER_TRACK_T *p_track )
{
   VC_CONTAINER_PARAM_UNUSED(context);
   if(p_track)
   {
      if(p_track->priv->extradata) free(p_track->priv->extradata);
      if(p_track->priv->drmdata) free(p_track->priv->drmdata);
      free(p_track);
   }
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_track_allocate_extradata( VC_CONTAINER_T *context,
   VC_CONTAINER_TRACK_T *p_track, unsigned int extra_size )
{
   VC_CONTAINER_PARAM_UNUSED(context);

   /* Sanity check the size of the extra data */
   if(extra_size > 100*1024) return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   /* Check if we need to allocate a buffer */
   if(extra_size > p_track->priv->extradata_size)
   {
      p_track->priv->extradata_size = 0;
      if(p_track->priv->extradata) free(p_track->priv->extradata);
      p_track->priv->extradata = malloc(extra_size);
      p_track->format->extradata = p_track->priv->extradata;
      if(!p_track->priv->extradata) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      p_track->priv->extradata_size = extra_size;
   }
   p_track->format->extradata = p_track->priv->extradata;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_track_allocate_drmdata( VC_CONTAINER_T *context,
   VC_CONTAINER_TRACK_T *p_track, unsigned int size )
{
   VC_CONTAINER_PARAM_UNUSED(context);

   /* Sanity check the size of the drm data */
   if(size > 200*1024) return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   /* Check if we need to allocate a buffer */
   if(size > p_track->priv->drmdata_size)
   {
      p_track->priv->drmdata_size = 0;
      if(p_track->priv->drmdata) free(p_track->priv->drmdata);
      p_track->priv->drmdata = malloc(size);
      if(!p_track->priv->drmdata) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      p_track->priv->drmdata_size = size;
   }

   return VC_CONTAINER_SUCCESS;
}
