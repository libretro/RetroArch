/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#define rmp4_get_track timing_track
#include "../../../libretro-common/formats/mp4/rmp4_video.c"
#undef rmp4_get_track

static rmp4_video_stream_t stream, saved;
static rmp4_track track;
static int64_t timestamps[4];

const rmp4_track *timing_track(const rmp4_t *mp4, int index)
{
   assert(mp4 == (const rmp4_t *)&track && !index);
   return &track;
}

int main(void)
{
   int64_t rounded;
   assert(rmp4_video_stream_next_end_time_ns(NULL) == -1);
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   stream.ts = timestamps;
   stream.ts_count = 4;
   timestamps[0] = 33333333;
   timestamps[1] = 50000000;
   timestamps[2] = 66666666;
   timestamps[3] = 83333333;
   saved = stream;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == 17000000);
   assert(!memcmp(&stream, &saved, sizeof(stream)));
   stream.disp_idx = 1;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == 33000000);
   stream.disp_idx = 2;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == 50000000);
   stream.disp_idx = 3;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   stream.disp_idx = -1;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   stream.disp_idx = 0;
   stream.demux = (rmp4_t *)&track;
   track.presentation_delay_ns = 500000000;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == 517000000);
   track.presentation_delay_ns = -1;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   track.presentation_delay_ns = 0;
   timestamps[1] = timestamps[0] - 1000000;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   timestamps[0] = INT64_MIN;
   timestamps[1] = INT64_MAX;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   timestamps[0] = 0;
   rounded = INT64_MAX / 1000000 * 1000000;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == rounded);
   track.presentation_delay_ns = INT64_MAX - rounded;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == INT64_MAX);
   track.presentation_delay_ns++;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   track.presentation_delay_ns = 0;
   timestamps[0] = -16666666;
   timestamps[1] = 0;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == 16000000);
   stream.ts_count = 1;
   assert(rmp4_video_stream_next_end_time_ns(&stream) == -1);
   puts("MP4 future presentation interval: PASS");
   return 0;
}
