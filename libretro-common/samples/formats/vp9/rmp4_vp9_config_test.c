/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "../../../formats/mp4/rmp4_video.c"
#include "rvp9_mp4_fixture.h"

int main(void)
{
   static const uint8_t hdr[12] = {1, 0, 0, 0, 2, 0, 0xa3, 9, 16, 9, 0, 0};
   static const uint8_t sdr[12] = {1, 0, 0, 0, 0, 0, 0x82, 1, 13, 1, 0, 0};
   rmp4_video_stream_t *stream;
   uint8_t invalid[12];
   fixture(hdr, 0);
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && stream->vp9_have_config && stream->vp9);
   assert(rmp4_video_stream_is_hdr(stream) && stream->transfer == 16);
   assert(stream->primaries == 9 && stream->matrix == 9 && stream->range == 1);
   assert(stream->max_cll == 1000 && stream->max_fall == 400
         && stream->mastering.luminance_max == 10000000);
   memset(clip + config_at, 0, 12);
   rmp4_video_stream_rewind(stream);
   stream->vp9->hd.profile = 2;
   stream->vp9->hd.bit_depth = 10;
   stream->vp9->hd.subsampling_x = stream->vp9->hd.subsampling_y = 1;
   stream->vp9->hd.color_space = 5;
   stream->vp9->hd.color_range = 1;
   assert(rmp4_video_vp9_color(stream));
   stream->vp9->hd.color_range = 0;
   assert(!rmp4_video_vp9_color(stream));
   stream->vp9->hd.color_range = 1;
   stream->vp9->hd.profile = 0;
   assert(!rmp4_video_vp9_color(stream));
   rmp4_video_stream_close(stream);
   fixture(sdr, 1);
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && !rmp4_video_stream_is_hdr(stream) && stream->transfer == 13);
   rmp4_video_stream_close(stream);
   fixture(hdr, 1);
   assert(!rmp4_video_stream_open(clip, clip_size));
   fixture(sdr, 1);
   clip[colr_at + 6] = 0x80;
   assert(!rmp4_video_stream_open(clip, clip_size));
   memcpy(invalid, hdr, sizeof(invalid));
   invalid[0] = 0;
   fixture(invalid, 0);
   assert(!rmp4_video_stream_open(clip, clip_size));
   memcpy(invalid, sdr, sizeof(invalid));
   invalid[8] = 16;
   fixture(invalid, 0);
   assert(!rmp4_video_stream_open(clip, clip_size));
   fixture(NULL, 0);
   stream = rmp4_video_stream_open(clip, clip_size);
   assert(stream && !stream->vp9_have_config && !rmp4_video_stream_is_hdr(stream));
   stream->vp9->hd.profile = 0;
   stream->vp9->hd.bit_depth = 8;
   stream->vp9->hd.subsampling_x = stream->vp9->hd.subsampling_y = 1;
   stream->vp9->hd.color_space = 2;
   stream->vp9->hd.color_range = 1;
   assert(rmp4_video_vp9_color(stream));
   assert(stream->primaries == 1 && stream->matrix == 1 && stream->range == 1
         && !stream->transfer);
   rmp4_video_stream_close(stream);
   puts("PASS: MP4 VP9 vpcC HDR/SDR metadata, coded consistency, ownership and legacy absence; no pixel decode claim");
   return 0;
}
