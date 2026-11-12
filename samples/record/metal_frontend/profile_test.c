/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <file/config_file.h>
#include <string/stdstring.h>
#include "../../../record/record_gpu_dispatch_metal.h"

static const char *profile_text;
static config_file_t *profile_new(const char *path)
{
   size_t length = strlen(profile_text);
   char *owned = (char*)malloc(length+1);
   assert(path && owned);
   memcpy(owned,profile_text,length+1);
   return config_file_new_take_string(owned,length,NULL);
}
#define config_file_new_from_path_to_string profile_new
#include "profile_fragment.c"
#undef config_file_new_from_path_to_string

static bool parse(const char *text,bool streaming)
{
   record_gpu_session_metal_config_t video;
   record_gpu_dispatch_metal_stream_config_t stream;
   bool audio = false;
   memset(&video,0,sizeof(video));
   memset(&stream,0,sizeof(stream));
   profile_text = text;
   return record_metal_config("profile",&video,&audio,streaming?&stream:NULL);
}

int main(void)
{
   record_gpu_session_metal_config_t video;
   record_gpu_dispatch_metal_stream_config_t stream;
   bool audio = false;
   memset(&video,0,sizeof(video));
   memset(&stream,0,sizeof(stream));
   profile_text = "audio_enable = true\nvcodec = hevc\nformat = flv\n"
      "acodec = aac\nsample_rate = 44100\naudio_bit_rate = 192000\n"
      "video_bit_rate = 8000000\nrtmp_legacy_commands = true\n";
   assert(record_metal_config("profile",&video,&audio,&stream));
   assert(audio && video.hevc && video.bitrate == 8000000 && stream.audio_rate == 44100
         && stream.audio_bitrate == 192000 && stream.legacy_commands);
   assert(parse("format = mp4\naudio_enable = false\n",false));
   assert(!parse("format = flv\n",false));
   assert(!parse("format = mp4\n",true));
   assert(!parse("rtmp_legacy_commands = true\n",false));
   assert(!parse("rtmp_legacy_commands = invalid\n",true));
   assert(!parse("sample_rate = 32000\n",true));
   assert(!parse("sample_rate = 48000\n",false));
   assert(!parse("audio_bit_rate = 12345\n",true));
   assert(!parse("audio_bit_rate = 128000\n",false));
   assert(!parse("acodec = ac3\n",true));
   assert(!parse("vcodec = vp9\n",true));
   assert(!parse("video_bit_rate = 0\n",true));
   assert(!parse("unrecognized = true\n",true));
   assert(parse("sample_rate = 48000\naudio_bit_rate = 96000\n",true));
   assert(parse("audio_bit_rate = 128000\n",true));
   assert(parse("audio_bit_rate = 160000\n",true));
   assert(record_metal_config(NULL,&video,&audio,&stream) && audio);
   puts("PASS Metal native FLV/AAC profiles, output clocks and strict file/stream separation");
   return 0;
}
