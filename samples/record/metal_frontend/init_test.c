/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual frontend admission and worker configuration, without native GPU I/O. */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include "../../../configuration.h"
#include "../../../gfx/video_driver.h"
#include "../../../audio/audio_driver.h"
#include "../../../audio/audio_upmix.h"
#include "../../../record/record_driver.h"
#include "../../../record/record_gpu_dispatch_metal.h"
#include "../../../record/record_vt_packet.h"

static settings_t settings;
static recording_state_t recording;
static struct retro_hdr_metadata metadata;
static record_gpu_session_metal_config_t offered_video;
static record_gpu_dispatch_metal_stream_config_t offered_stream;
static unsigned stream_calls, file_calls;
static char device_token, job_token;
static const char *profile_text;

settings_t *config_get_ptr(void)
{
   return &settings;
}
recording_state_t *recording_state_get_ptr(void)
{
   return &recording;
}
bool audio_driver_has_callback(void)
{
   return false;
}
unsigned audio_layout_channels(uint32_t layout)
{
   return layout == AUDIO_LAYOUT_STEREO ? 2u
      : layout == AUDIO_SPEAKER_FRONT_CENTER ? 1u : 0u;
}
bool video_driver_get_device(video_device_info_t *info)
{
   memset(info,0,sizeof(*info));
   info->api = VIDEO_DEVICE_METAL;
   info->device = &device_token;
   return true;
}
bool video_driver_get_display_metadata(unsigned command, void *data)
{
   assert(command == RETRO_ENVIRONMENT_GET_HDR_METADATA);
   *(struct retro_hdr_metadata*)data = metadata;
   return true;
}
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new_stream(void *device,
      const record_gpu_session_metal_config_t *video,
      const record_gpu_dispatch_metal_stream_config_t *stream,const char *url)
{
   assert(device == &device_token && url && !strncmp(url,"rtmp",4));
   stream_calls++;
   offered_video = *video;
   offered_stream = *stream;
   /* This is the reader's real admission bound, independent of the caller. */
   if (!stream->max_packet_size || stream->max_packet_size > RECORD_VT_PACKET_MAX_SIZE)
      return NULL;
   return (record_gpu_dispatch_metal_t*)&job_token;
}
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new(void *device,
      const record_gpu_session_metal_config_t *video,
      const record_mux_avfoundation_config_t *output,size_t capacity,const char *path)
{
   assert(device == &device_token && video && output && capacity && path);
   file_calls++;
   return (record_gpu_dispatch_metal_t*)&job_token;
}
static config_file_t *profile_new(const char *path)
{
   size_t length = strlen(profile_text);
   char *owned = (char*)malloc(length+1);
   assert(path && owned);
   memcpy(owned,profile_text,length+1);
   return config_file_new_take_string(owned,length,NULL);
}
#define config_file_new_from_path_to_string profile_new
#define RARCH_ERR(message) ((void)0)
#define RARCH_LOG(message,a,b,c,d) ((void)0)
#include "init_fragment.c"
#undef config_file_new_from_path_to_string

static void reset(struct record_params *params)
{
   memset(&settings,0,sizeof(settings));
   memset(&recording,0,sizeof(recording));
   memset(&metadata,0,sizeof(metadata));
   memset(params,0,sizeof(*params));
   metadata.version = 1;
   metadata.size = sizeof(metadata);
   recording.streaming_enable = true;
   recording.layout = AUDIO_LAYOUT_STEREO;
   params->filename = "rtmp://localhost/live/test";
   params->video_gpu_record = true;
   params->out_dims = VIDEO_SCALE_PACK(960,576);
   params->fps = 60000.0/1001.0;
   params->samplerate = 48000;
   params->channels = 2;
   stream_calls = file_calls = 0;
   assert(!record_metal_active && !record_metal_retired && !record_metal_start_requested);
}
static record_metal_context_t *start(struct record_params *params)
{
   record_metal_context_t *ctx = (record_metal_context_t*)record_metal_init(params);
   assert(ctx && ctx == record_metal_active && ctx->streaming && !ctx->path);
   assert(stream_calls == 1 && !file_calls);
   assert(offered_stream.max_packet_size == RECORD_VT_PACKET_MAX_SIZE);
   assert(offered_video.timescale == 60000 && ctx->duration == 1001);
   assert(offered_video.width == 960 && offered_video.height == 576);
   return ctx;
}
static void retire(record_metal_context_t *ctx)
{
   record_metal_active = NULL;
   free(ctx);
}
int main(void)
{
   struct record_params params;
   record_metal_context_t *ctx;
   reset(&params);
   ctx = start(&params);
   assert(!offered_video.hevc && offered_video.bit_depth == 8);
   assert(offered_stream.channels == 2 && offered_stream.audio_rate == 48000);
   assert(offered_stream.color.primaries == 1 && offered_stream.color.transfer == 1);
   retire(ctx);

   reset(&params);
   params.samplerate = 32040.5;
   ctx = start(&params);
   assert(offered_stream.source_rate == 32040.5 && ctx->audio_rate == 32040.5);
   assert(offered_stream.audio_rate == 48000);
   retire(ctx);

   reset(&params);
   params.samplerate = 44100;
   params.channels = 1;
   recording.layout = AUDIO_SPEAKER_FRONT_CENTER;
   ctx = start(&params);
   assert(offered_stream.channels == 1 && offered_stream.audio_rate == 44100);
   retire(ctx);

   reset(&params);
   profile_text = "audio_enable = false\nformat = flv\n";
   params.config = "profile";
   ctx = start(&params);
   assert(!offered_stream.channels && !offered_stream.audio_rate && !ctx->audio_enabled);
   retire(ctx);

   reset(&params);
   metadata.output_mode = 1;
   metadata.color_primaries = 9;
   metadata.transfer_function = 16;
   ctx = start(&params);
   assert(offered_video.hevc && offered_video.bit_depth == 10);
   assert(offered_stream.color.primaries == 9 && offered_stream.color.transfer == 16);
   retire(ctx);

   reset(&params);
   metadata.output_mode = 2;
   metadata.color_primaries = 1;
   metadata.transfer_function = 8;
   ctx = start(&params);
   assert(offered_video.mode == 2 && offered_video.hevc && offered_video.bit_depth == 10);
   assert(offered_stream.color.primaries == 9 && offered_stream.color.transfer == 16);
   retire(ctx);

   reset(&params);
   recording.streaming_enable = false;
   params.filename = "recording.mp4";
   ctx = (record_metal_context_t*)record_metal_init(&params);
   assert(ctx && !ctx->streaming && ctx->path != params.filename);
   assert(!strcmp(ctx->path,params.filename) && file_calls == 1 && !stream_calls);
   retire(ctx);

   reset(&params);
   params.filename = "https://localhost/live/test";
   assert(!record_metal_init(&params) && !stream_calls);
   reset(&params);
   params.out_dims = VIDEO_SCALE_PACK(959,576);
   assert(!record_metal_init(&params) && !stream_calls);
   puts("PASS actual Metal frontend stream admission, packet bounds, clocks, video-only and HDR configuration");
   return 0;
}
