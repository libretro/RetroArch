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
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_io.h"

#define BUFFER_SIZE 256*1024
#define MAX_TRACKS 16
#define MAX_SEEKS 16

static int container_test_info(VC_CONTAINER_T *ctx, bool b_reader);
static int container_test_parse_cmdline(int argc, char **argv);

static const char *psz_in = 0;
static long packets_num = 0;
static long track_num = -1;
static int fps = 30;
static int margin = 5; // margin for frame interval, percent

static struct
{
   uint32_t mapping;
   uint32_t frames;
   uint32_t packets;
   uint64_t bytes;
   uint32_t frame_size;
   int64_t first_dts;
   int64_t first_pts;
   int64_t last_dts;
   int64_t last_pts;
} tracks[MAX_TRACKS];

static int32_t verbosity = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;

/*****************************************************************************/
int main(int argc, char **argv)
{
   int retval = 0;
   VC_CONTAINER_T *p_ctx = 0;
   VC_CONTAINER_STATUS_T status;
   unsigned int i;
   uint8_t *buffer = malloc(BUFFER_SIZE);
   int32_t interval;
   int64_t last_packet_pts = -1;
   int32_t max_interval = 0, max_interval_after_first = 0;
   int fail = 0;

   if(container_test_parse_cmdline(argc, argv))
      goto error_silent;

   LOG_INFO (0, "Require that no frame interval greater than 1/%d seconds (%d%% margin)", fps, margin);

   /* Set the general verbosity */
   vc_container_log_set_verbosity(0, verbosity);
   vc_container_log_set_default_verbosity(verbosity);

   p_ctx = vc_container_open_reader(psz_in, &status, 0, 0);

   if(!p_ctx)
   {
     LOG_ERROR(0, "error opening file %s (%i)", psz_in, status);
     goto error;
   }

   if (verbosity & VC_CONTAINER_LOG_DEBUG)
   {
      container_test_info(p_ctx, true);
   }
   LOG_INFO (0, "Search for video track only");

   /* Disabling tracks which are not requested and enable packetisation if requested */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = p_ctx->tracks[i];
      track->is_enabled =  (track->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO);
   }


   LOG_DEBUG(0, "TEST start reading");
   for(i = 0; !packets_num || (long)i < packets_num; i++)
   {
      VC_CONTAINER_PACKET_T packet = {0};
      int32_t frame_num = 0;

      status = vc_container_read(p_ctx, &packet, VC_CONTAINER_READ_FLAG_INFO);
      if(status != VC_CONTAINER_SUCCESS) {LOG_DEBUG(0, "TEST info status: %i", status); break;}

      if(packet.track < MAX_TRACKS)
      {
         if((packet.flags & VC_CONTAINER_PACKET_FLAG_FRAME_START))
         {
            tracks[packet.track].frames++;
            tracks[packet.track].frame_size = 0;
         }
         frame_num = tracks[packet.track].frames;
      }

      tracks[packet.track].frame_size += packet.size;

//      LOG_DEBUG(0, "packet info: track %i, size %i/%i/%i, pts %"PRId64", flags %x%s, num %i",
//                packet.track, packet.size, packet.frame_size, tracks[packet.track].frame_size, packet.pts, packet.flags,
//                (packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ? " (keyframe)" : "",
//                frame_num-1);

      if (last_packet_pts != -1)
         interval = packet.pts - last_packet_pts;
      else
         interval = 0; // packet.pts;
      last_packet_pts = packet.pts;
      max_interval = MAX (max_interval, interval);
      if (i >= 2)
         max_interval_after_first = MAX (max_interval_after_first, interval);

      /* Check if interval (in us) exceeds 1/fps, with percentage margin */
      if (interval * fps > 1e4 * (100+margin))
      {
         LOG_INFO (0, "Frame %d, interval %.3f FAILED", i, interval / 1000.0f);
         fail = 1;
      }

      LOG_DEBUG (0, "Frame %d, interval %.3f", i, interval / 1000.0f);

      if(track_num >= 0 && packet.track != (uint32_t)track_num)
      {
         status = vc_container_read(p_ctx, 0, VC_CONTAINER_READ_FLAG_SKIP);
         if(status != VC_CONTAINER_SUCCESS) {LOG_DEBUG(0, "TEST skip status: %i", status); break;}
         continue;
      }

      packet.buffer_size = BUFFER_SIZE;
      packet.data = buffer;
      packet.size = 0;
      status = vc_container_read(p_ctx, &packet, 0);
      if(status != VC_CONTAINER_SUCCESS) {LOG_DEBUG(0, "TEST read status: %i", status); break;}

      //      LOG_DEBUG(0, "packet: track %i, size %i, pts %"PRId64", flags %x", packet.track, packet.size, packet.pts, packet.flags);

      if (tracks[packet.track].packets)
      {
         tracks[packet.track].last_dts = packet.dts;
         tracks[packet.track].last_pts = packet.pts;
      } else {
         tracks[packet.track].first_dts = packet.dts;
         tracks[packet.track].first_pts = packet.pts;
      }

      if(packet.track < MAX_TRACKS)
      {
         tracks[packet.track].packets++;
         tracks[packet.track].bytes += packet.size;
      }

   }
   LOG_DEBUG(0, "TEST stop reading");

   /* Output stats */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      LOG_INFO(0, "track %u: read %u samples in %u packets for a total of %"PRIu64" bytes",
               i, tracks[i].frames, tracks[i].packets, tracks[i].bytes);
      LOG_INFO(0, "Starting at %"PRId64"us (decode at %"PRId64"us), ending at %"PRId64"us (decode at %"PRId64"us)",
               tracks[i].first_pts, tracks[i].first_dts, tracks[i].last_pts, tracks[i].last_dts);
   }

   LOG_INFO (0, "---\nMax interval = %.3f ms; max interval (after first) = %.3f ms\n", (float) max_interval / 1000.0, (float) max_interval_after_first / 1000.0);

 end:
   if(p_ctx) vc_container_close(p_ctx);
   free(buffer);

#ifdef _MSC_VER
   getchar();
#endif

   retval = fail;
   if (fail)
   {
      LOG_INFO (0, "TEST FAILED: highest frame interval = %.3f ms", max_interval / 1000.0f);
   }
   else
      LOG_INFO (0, "TEST PASSED");
   return retval;

 error:
   LOG_ERROR(0, "TEST FAILED TO RUN");
 error_silent:
   retval = -1;
   goto end;
}

static int container_test_parse_cmdline(int argc, char **argv)
{
   int i, j, k;
   int32_t *p_verbosity;

   /* Parse the command line arguments */
   for(i = 1; i < argc; i++)
   {
      if(!argv[i]) continue;

      if(argv[i][0] != '-')
      {
         /* Not an option argument so will be the input URI */
         psz_in = argv[i];
         continue;
      }

      /* We are now dealing with command line options */
      switch(argv[i][1])
      {
      case 'v':
         j = 2;
         p_verbosity = &verbosity;
         *p_verbosity = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;
         for(k = 0; k < 2 && argv[i][j+k] == 'v'; k++)
            *p_verbosity = (*p_verbosity << 1) | 1 ;
         break;
      case 'f':
         if(i+1 == argc || !argv[i+1]) goto invalid_option;
         fps = strtol(argv[++i], 0, 0);
         break;
      case 'h': goto usage;
      default: goto invalid_option;
      }
      continue;
   }

   /* Sanity check that we have at least an input uri */
   if(!psz_in)
   {
     LOG_ERROR(0, "missing uri argument");
     goto usage;
   }

   return 0;

 invalid_option:
   LOG_ERROR(0, "invalid command line option (%s)", argv[i]);

 usage:
   psz_in = strrchr(argv[0], '\\'); if(psz_in) psz_in++;
   if(!psz_in) {psz_in = strrchr(argv[0], '/'); if(psz_in) psz_in++;}
   if(!psz_in) psz_in = argv[0];
   LOG_INFO(0, "");
   LOG_INFO(0, "usage: %s [options] uri", psz_in);
   LOG_INFO(0, "options list:");
   LOG_INFO(0, " -vxx  : general verbosity level (replace xx with a number of \'v\')");
   LOG_INFO(0, " -f    : required frame rate/second (frame interval must not exceed 1/f)");
   LOG_INFO(0, " -h    : help");
   return 1;
}

static int container_test_info(VC_CONTAINER_T *ctx, bool b_reader)
{
   const char *name_type;
   unsigned int i;

   LOG_INFO(0, "");
   if(b_reader) LOG_INFO(0, "----Reader Information----");
   else LOG_INFO(0, "----Writer Information----");

   LOG_INFO(0, "duration: %2.2fs, size: %"PRId64, ctx->duration/1000000.0, ctx->size);
   LOG_INFO(0, "capabilities: %x", ctx->capabilities);
   LOG_INFO(0, "");

   for(i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];

      switch(track->format->es_type)
      {
      case VC_CONTAINER_ES_TYPE_AUDIO: name_type = "audio"; break;
      case VC_CONTAINER_ES_TYPE_VIDEO: name_type = "video"; break;
      case VC_CONTAINER_ES_TYPE_SUBPICTURE: name_type = "subpicture"; break;
      default: name_type = "unknown"; break;
      }

      if (!strcmp (name_type, "video"))
      {
         LOG_INFO(0, "track: %i, type: %s, fourcc: %4.4s", i, name_type, (char *)&track->format->codec);
         LOG_INFO(0, " bitrate: %i, framed: %i, enabled: %i", track->format->bitrate,
                  !!(track->format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED), track->is_enabled);
         LOG_INFO(0, " extra data: %i, %p", track->format->extradata_size, track->format->extradata);
         switch(track->format->es_type)
         {
         case VC_CONTAINER_ES_TYPE_VIDEO:
            LOG_INFO(0, " width: %i, height: %i, (%i,%i,%i,%i)",
                     track->format->type->video.width, track->format->type->video.height,
                     track->format->type->video.x_offset, track->format->type->video.y_offset,
                     track->format->type->video.visible_width, track->format->type->video.visible_height);
            LOG_INFO(0, " pixel aspect ratio: %i/%i, frame rate: %i/%i",
                     track->format->type->video.par_num, track->format->type->video.par_den,
                     track->format->type->video.frame_rate_num, track->format->type->video.frame_rate_den);
            break;

         default: break;
         }
      }
   }

   for (i = 0; i < ctx->meta_num; ++i)
   {
      const char *name, *value;
      if (i == 0) LOG_INFO(0, "");
      name = vc_container_metadata_id_to_string(ctx->meta[i]->key);
      value = ctx->meta[i]->value;
      if(!name) continue;
      LOG_INFO(0, "metadata(%i) : %s : %s", i, name, value);
   }

   LOG_INFO(0, "--------------------------");
   LOG_INFO(0, "");

   return 0;
}


