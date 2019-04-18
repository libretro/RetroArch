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

/* Client i/o wrapper */
static VC_CONTAINER_IO_T *client_io_open(const char *, VC_CONTAINER_STATUS_T *);

static bool b_info = 0, b_seek = 0, b_dump = 0;
static bool b_audio = 1, b_video = 1, b_subs = 1, b_errorcode = 1;
static const char *psz_in = 0, *psz_out = 0;
static long packets_num = 0;
static long track_num = -1;
static FILE *dump_file = 0;
static bool b_client_io = 0;
static bool b_packetize = 0;

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

static unsigned int seeks = 0;
static long seek_offsets[MAX_SEEKS];
static long seek_flags[MAX_SEEKS];
static int32_t verbosity = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;
static int32_t verbosity_input = -1, verbosity_output = -1;

/*****************************************************************************/
int main(int argc, char **argv)
{
   int retval = 0;
   VC_CONTAINER_T *p_ctx = 0, *p_writer_ctx = 0;
   VC_CONTAINER_STATUS_T status;
   unsigned int i, j;
   uint8_t *buffer = malloc(BUFFER_SIZE);
   int64_t seek_time;

   if(container_test_parse_cmdline(argc, argv))
      goto error_silent;

   /* Set the general verbosity */
   vc_container_log_set_verbosity(0, verbosity);

   if(verbosity_input < 0) verbosity_input = verbosity;
   if(verbosity_output < 0) verbosity_output = verbosity;

   /* Open a dump file if it was requested */
   if(b_dump)
   {
      char *psz_dump;

      if (psz_out)
      {
         psz_dump = vcos_strdup(psz_out);
      } else {
         psz_dump = strrchr(psz_in, '\\'); if(psz_dump) psz_dump++;
         if(!psz_dump) {psz_dump = strrchr(psz_in, '/'); if(psz_dump) psz_dump++;}
         if(!psz_dump) psz_dump = vcos_strdup(psz_in);
         else psz_dump = vcos_strdup(psz_dump);
         psz_dump[strlen(psz_dump)-1] = '1';
      }
      dump_file = fopen(psz_dump, "wb");
      if(!dump_file) LOG_ERROR(0, "error opening dump file %s", psz_dump);
      else LOG_INFO(0, "data packets will dumped to %s", psz_dump);
      free(psz_dump);
      if(!dump_file) goto error;
   }

   /* Open a writer if an output was requested */
   if(psz_out && !b_dump)
   {
      vc_container_log_set_default_verbosity(verbosity_output);
      p_writer_ctx = vc_container_open_writer(psz_out, &status, 0, 0);
      if(!p_writer_ctx)
      {
         LOG_ERROR(0, "error opening file %s (%i)", psz_out, status);
         goto error;
      }
   }

   vc_container_log_set_default_verbosity(verbosity_input);

   /* Open the container */
   if(b_client_io)
   {
      VC_CONTAINER_IO_T *p_io;

      LOG_INFO(0, "Using client I/O for %s", psz_in);
      p_io = client_io_open(psz_in, &status);
      if(!p_io)
      {
        LOG_ERROR(0, "error creating io for %s (%i)", psz_in, status);
        goto error;
      }

      p_ctx = vc_container_open_reader_with_io(p_io, psz_in, &status, 0, 0);
      if(!p_ctx)
         vc_container_io_close(p_io);
   }
   else
   {
      p_ctx = vc_container_open_reader(psz_in, &status, 0, 0);
   }

   if(!p_ctx)
   {
     LOG_ERROR(0, "error opening file %s (%i)", psz_in, status);
     goto error;
   }

   /* Disabling tracks which are not requested and enable packetisation if requested */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = p_ctx->tracks[i];
      unsigned int disable = 0;

      switch(track->format->es_type)
      {
      case VC_CONTAINER_ES_TYPE_VIDEO: if(!b_video) disable = 1; break;
      case VC_CONTAINER_ES_TYPE_AUDIO: if(!b_audio) disable = 1; break;
      case VC_CONTAINER_ES_TYPE_SUBPICTURE: if(!b_subs) disable = 1; break;
      default: break;
      }
      if(disable)
      {
         track->is_enabled = 0;
         LOG_INFO(0, "disabling track: %i, fourcc: %4.4s", i, (char *)&track->format->codec);
      }

      if(track->is_enabled && b_packetize && !(track->format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
      {
         status = vc_container_control(p_ctx, VC_CONTAINER_CONTROL_TRACK_PACKETIZE, i, track->format->codec_variant);
         if(status != VC_CONTAINER_SUCCESS)
         {
            LOG_ERROR(0, "packetization not supported on track: %i, fourcc: %4.4s", i, (char *)&track->format->codec);
            track->is_enabled = 0;
         }
      }
   }

   container_test_info(p_ctx, true);
   if(b_info) goto end;

   if(p_writer_ctx)
   {
      LOG_INFO(0, "----Writer Information----");
      for(i = 0; i < p_ctx->tracks_num; i++)
      {
         VC_CONTAINER_TRACK_T *track = p_ctx->tracks[i];
         if(!track->is_enabled) continue;
         tracks[p_writer_ctx->tracks_num].mapping = i;
         LOG_INFO(0, "adding track: %i, fourcc: %4.4s", i, (char *)&track->format->codec);
         status = vc_container_control(p_writer_ctx, VC_CONTAINER_CONTROL_TRACK_ADD, track->format);
         if(status)
         {
            LOG_INFO(0, "unsupported track type (%i, %i)", status, i);
            track->is_enabled = 0; /* disable track */
         }
      }
      if(p_writer_ctx->tracks_num)
      {
         status = vc_container_control(p_writer_ctx, VC_CONTAINER_CONTROL_TRACK_ADD_DONE);
         if(status) LOG_INFO(0, "could not add tracks (%i)", status);
      }
      LOG_INFO(0, "--------------------------");
      LOG_INFO(0, "");
   }

   for(i = 0; i < seeks; i++)
   {
      LOG_DEBUG(0, "TEST seek to %ims", seek_offsets[i]);
      seek_time = ((int64_t)(seek_offsets[i])) * 1000;
      status = vc_container_seek(p_ctx, &seek_time, VC_CONTAINER_SEEK_MODE_TIME, seek_flags[i]);
      LOG_DEBUG(0, "TEST seek done (%i) to %ims", status, (int)(seek_time/1000));
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

      LOG_DEBUG(0, "packet info: track %i, size %i/%i/%i, pts %"PRId64", flags %x%s, num %i",
                packet.track, packet.size, packet.frame_size, tracks[packet.track].frame_size, packet.pts, packet.flags,
                (packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ? " (keyframe)" : "",
                frame_num-1);

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

      LOG_DEBUG(0, "packet: track %i, size %i, pts %"PRId64", flags %x", packet.track, packet.size, packet.pts, packet.flags);

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

      if(dump_file) fwrite(packet.data, packet.size, 1, dump_file);

      if(p_writer_ctx)
      {
         packet.track = tracks[packet.track].mapping;
         status = vc_container_write(p_writer_ctx, &packet);
         if(status != VC_CONTAINER_SUCCESS) {LOG_DEBUG(0, "TEST write status: %i", status); break;}
      }
   }
   LOG_DEBUG(0, "TEST stop reading");

   if(b_seek)
   {
      LOG_DEBUG(0, "TEST start seeking");
      for(j = 0, seek_time = 100; j < 20; j++)
      {
         LOG_DEBUG(0, "seeking to %ims", (int)(seek_time/1000));
         status = vc_container_seek(p_ctx, &seek_time, VC_CONTAINER_SEEK_MODE_TIME, VC_CONTAINER_SEEK_FLAG_FORWARD);
         LOG_DEBUG(0, "seek done (%i) to %ims", status, (int)(seek_time/1000));

         for(i = 0; i < 1; i++)
         {
            VC_CONTAINER_PACKET_T packet = {0};
            packet.buffer_size = BUFFER_SIZE;
            packet.data = buffer;

            status = vc_container_read(p_ctx, &packet, 0);
            if(status) LOG_DEBUG(0, "TEST read status: %i", status);
            if(status == VC_CONTAINER_ERROR_EOS) break;
            if(status == VC_CONTAINER_ERROR_CORRUPTED) break;
            if(status == VC_CONTAINER_ERROR_FORMAT_INVALID) break;
            seek_time = packet.pts + 800000;
         }
      }
      LOG_DEBUG(0, "TEST stop seeking");
   }

   /* Output stats */
   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      LOG_INFO(0, "track %u: read %u samples in %u packets for a total of %"PRIu64" bytes",
               i, tracks[i].frames, tracks[i].packets, tracks[i].bytes);
      LOG_INFO(0, "Starting at %"PRId64"us (decode at %"PRId64"us), ending at %"PRId64"us (decode at %"PRId64"us)",
               tracks[i].first_pts, tracks[i].first_dts, tracks[i].last_pts, tracks[i].last_dts);
   }

 end:
   if(p_ctx) vc_container_close(p_ctx);
   if(p_writer_ctx)
   {
      container_test_info(p_writer_ctx, false);
      vc_container_close(p_writer_ctx);
   }
   if(dump_file) fclose(dump_file);
   free(buffer);

#ifdef _MSC_VER
   getchar();
#endif

   LOG_ERROR(0, "TEST ENDED (%i)", retval);
   return b_errorcode ? retval : 0;

 error:
   LOG_ERROR(0, "TEST FAILED");
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
      case 'i': b_info = 1; break;
      case 'S': b_seek = 1; break;
      case 'd': b_dump = 1; break;
      case 'c': b_client_io = 1; break;
      case 'v':
         if(argv[i][2] == 'i') {j = 3; p_verbosity = &verbosity_input;}
         else if(argv[i][2] == 'o') {j = 3; p_verbosity = &verbosity_output;}
         else {j = 2; p_verbosity = &verbosity;}
         *p_verbosity = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;
         for(k = 0; k < 2 && argv[i][j+k] == 'v'; k++)
            *p_verbosity = (*p_verbosity << 1) | 1 ;
         break;
      case 's':
         if(i+1 == argc || !argv[i+1]) goto invalid_option;
         if(seeks >= MAX_SEEKS) goto invalid_option;
         seek_flags[seeks] = argv[i][2] == 'f' ? VC_CONTAINER_SEEK_FLAG_FORWARD : 0;
         seek_offsets[seeks] = strtol(argv[++i], 0, 0);
         if(seek_offsets[seeks] < 0 || seek_offsets[seeks] == LONG_MAX) goto invalid_option;
         seeks++;
         break;
      case 'n':
         if(argv[i][2] == 'a') b_audio = 0;
         else if(argv[i][2] == 'v') b_video = 0;
         else if(argv[i][2] == 's') b_subs = 0;
         else if(argv[i][2] == 'r') b_errorcode = 0;
         else goto invalid_option;
         break;
      case 'e':
         if(argv[i][2] == 'p') b_packetize = 1;
         else goto invalid_option;
         break;
      case 'o':
         if(i+1 == argc || !argv[i+1] || argv[i+1][0] == '-') goto invalid_option;
         psz_out = argv[++i];
         break;
      case 'p':
         if(i+1 == argc || !argv[i+1]) goto invalid_option;
         packets_num = strtol(argv[++i], 0, 0);
         if(packets_num < 0 || packets_num == LONG_MAX) goto invalid_option;
         break;
      case 't':
         if(i+1 == argc || !argv[i+1]) goto invalid_option;
         track_num = strtol(argv[++i], 0, 0);
         if(track_num == LONG_MIN || track_num == LONG_MAX) goto invalid_option;
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
   LOG_INFO(0, " -i    : only print information on the container");
   LOG_INFO(0, " -p X  : read only X packets from the container");
   LOG_INFO(0, " -t X  : read only packets from track X");
   LOG_INFO(0, " -s X  : seek to X milliseconds before starting reading");
   LOG_INFO(0, " -sf X : seek forward to X milliseconds before starting reading");
   LOG_INFO(0, " -S    : do seek testing");
   LOG_INFO(0, " -d    : dump the data read from the container to files (-o to name file)");
   LOG_INFO(0, " -o uri: output to another uri (i.e. re-muxing)");
   LOG_INFO(0, " -na   : disable audio");
   LOG_INFO(0, " -nv   : disable video");
   LOG_INFO(0, " -ns   : disable subtitles");
   LOG_INFO(0, " -nr   : always return an error code of 0 (even in case of failure)");
   LOG_INFO(0, " -ep   : enable packetization if data is not already packetized");
   LOG_INFO(0, " -c    : use the client i/o functions");
   LOG_INFO(0, " -vxx  : general verbosity level (replace xx with a number of \'v\')");
   LOG_INFO(0, " -vixx : verbosity specific to the input container");
   LOG_INFO(0, " -voxx : verbosity specific to the output container");
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

      LOG_INFO(0, "track: %i, type: %s, fourcc: %4.4s", i, name_type, (char *)&track->format->codec);
      LOG_INFO(0, " bitrate: %i, framed: %i, enabled: %i", track->format->bitrate,
               !!(track->format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED), track->is_enabled);
      LOG_INFO(0, " extra data: %i, %p", track->format->extradata_size, track->format->extradata);
      switch(track->format->es_type)
      {
      case VC_CONTAINER_ES_TYPE_AUDIO:
         LOG_INFO(0, " samplerate: %i, channels: %i, bps: %i, block align: %i",
                  track->format->type->audio.sample_rate, track->format->type->audio.channels,
                  track->format->type->audio.bits_per_sample, track->format->type->audio.block_align);
         LOG_INFO(0, " gapless delay: %i gapless padding: %i",
                  track->format->type->audio.gap_delay, track->format->type->audio.gap_padding);
         LOG_INFO(0, " language: %4.4s", track->format->language);
         break;

      case VC_CONTAINER_ES_TYPE_VIDEO:
         LOG_INFO(0, " width: %i, height: %i, (%i,%i,%i,%i)",
                  track->format->type->video.width, track->format->type->video.height,
                  track->format->type->video.x_offset, track->format->type->video.y_offset,
                  track->format->type->video.visible_width, track->format->type->video.visible_height);
         LOG_INFO(0, " pixel aspect ratio: %i/%i, frame rate: %i/%i",
                  track->format->type->video.par_num, track->format->type->video.par_den,
                  track->format->type->video.frame_rate_num, track->format->type->video.frame_rate_den);
         break;

      case VC_CONTAINER_ES_TYPE_SUBPICTURE:
         LOG_INFO(0, " language: %4.4s, encoding: %i", track->format->language,
                  track->format->type->subpicture.encoding);
         break;

      default: break;
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

/*****************************************************************************
 * Client I/O wrapper. Only used when the right cmd line option is passed.
 *****************************************************************************/
static VC_CONTAINER_STATUS_T client_io_close( VC_CONTAINER_IO_T *p_ctx )
{
   FILE *fd = (FILE *)p_ctx->module;
   fclose(fd);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t client_io_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   FILE *fd = (FILE *)p_ctx->module;
   size_t ret = fread(buffer, 1, size, fd);
   if(ret != size)
   {
      /* Sanity check return value. Some platforms (e.g. Android) can return -1 */
      if( ((int)ret) < 0 ) ret = 0;

      if( feof(fd) ) p_ctx->status = VC_CONTAINER_ERROR_EOS;
      else p_ctx->status = VC_CONTAINER_ERROR_FAILED;
   }
   LOG_DEBUG( 0, "read: %i", ret );
   return ret;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T client_io_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   FILE *fd = (FILE *)p_ctx->module;
   int ret;

   //FIXME: no large file support
   if (offset > (int64_t)UINT_MAX)
   {
      LOG_ERROR( 0, "no large file support");
      p_ctx->status = VC_CONTAINER_ERROR_EOS;
      return VC_CONTAINER_ERROR_EOS;
   }

   ret = fseek(fd, (long)offset, SEEK_SET);
   if(ret)
   {
      if( feof(fd) ) status = VC_CONTAINER_ERROR_EOS;
      else status = VC_CONTAINER_ERROR_FAILED;
   }

   p_ctx->status = status;
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_IO_T *client_io_open( const char *psz_uri, VC_CONTAINER_STATUS_T *status )
{
   VC_CONTAINER_IO_T *p_io;
   VC_CONTAINER_IO_CAPABILITIES_T capabilities = VC_CONTAINER_IO_CAPS_NO_CACHING;
   FILE *fd;

   fd = fopen(psz_uri, "rb");
   if (!fd)
   {
      *status = VC_CONTAINER_ERROR_URI_OPEN_FAILED;
      return NULL;
   }

   p_io = vc_container_io_create( psz_uri, 0, capabilities, status );
   if(!p_io)
   {
     LOG_ERROR(0, "error creating io (%i)", *status);
     fclose(fd);
     return NULL;
   }

   p_io->module = (struct VC_CONTAINER_IO_MODULE_T *)fd;
   p_io->pf_close = client_io_close;
   p_io->pf_read = client_io_read;
   p_io->pf_seek = client_io_seek;

   //FIXME: no large file support
   fseek(fd, 0, SEEK_END);
   p_io->size = ftell(fd);
   fseek(fd, 0, SEEK_SET);

   *status = VC_CONTAINER_SUCCESS;
   return p_io;
}
