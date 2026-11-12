/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_mediacodec_stream_config.h"

struct record_mediacodec_encoder
{
   const void *data[2];
   size_t bytes[2];
   bool audio;
};
static unsigned queries;
static uint8_t sps[] = {0,0,0,1,103,66,0,30};
static uint8_t pps[] = {0,0,0,1,104};
static uint8_t asc[] = {18,16};
static const uint8_t combined_avc[] = {0,0,0,1,103,66,0,30,0,0,0,1,104};
static const uint8_t avcc[] = {1,66,0,30,255,225,0,4,103,66,0,30,1,0,1,104};
/* Golden parameter-set bytes from the shared FLV mux HEVC fixture. */
static const uint8_t hevc8[] =
{
   0,0,0,1,64,1,170,0,0,0,1,66,
   1,1,17,17,17,17,17,17,17,17,17,17,
   17,17,160,16,32,97,112,0,0,1,68,1,
   170
};
static const uint8_t hevc10[] =
{
   0,0,0,1,64,1,170,0,0,0,1,66,
   1,1,17,17,17,17,17,17,17,17,17,17,
   17,17,160,16,32,97,55,0,0,1,68,1,
   170
};
static const uint8_t av1[] = {129,0,76,0,10,13,0,0,0,3,55,251,230,215,206,129,1,4,130};
bool record_mediacodec_encoder_video_config(record_mediacodec_encoder_t *encoder,
      unsigned index, const void **data, size_t *bytes)
{
   queries++;
   *data = NULL;
   *bytes = 0;
   if (!encoder || encoder->audio || index > 1 || !encoder->bytes[index])
      return false;
   *data = encoder->data[index];
   *bytes = encoder->bytes[index];
   return true;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *encoder,
      const void **data, size_t *bytes)
{
   queries++;
   *data = NULL;
   *bytes = 0;
   if (!encoder || !encoder->audio || !encoder->bytes[0])
      return false;
   *data = encoder->data[0];
   *bytes = encoder->bytes[0];
   return true;
}
static void setup(record_mediacodec_encoder_t *video, record_mediacodec_encoder_t *audio,
      record_mediacodec_stream_config_t *config)
{
   memset(video, 0, sizeof(*video));
   memset(audio, 0, sizeof(*audio));
   memset(config, 0, sizeof(*config));
   video->data[0] = sps;
   video->bytes[0] = sizeof(sps);
   video->data[1] = pps;
   video->bytes[1] = sizeof(pps);
   audio->data[0] = asc;
   audio->bytes[0] = sizeof(asc);
   audio->audio = true;
   config->max_packet_size = 1024;
   config->bit_depth = 8;
}
static void drain_setup(rflv_mux_t *mux)
{
   const rflv_span_t *spans;
   size_t count;
   unsigned actions = 0;
   while (rflv_mux_get(mux, &spans, &count))
   {
      assert(++actions < 8);
      assert(rflv_mux_release(mux));
   }
   assert(rflv_mux_step(mux) == RFLV_BUSY);
}
int main(void)
{
   record_mediacodec_encoder_t video, audio;
   record_mediacodec_stream_config_t config;
   rflv_mux_t *mux;
   rflv_tag_t tag;
   unsigned before, i;
   static const uint8_t picture[] = {0,0,1,101,136};
   setup(&video, &audio, &config);
   mux = record_mediacodec_stream_mux_new(&video, &audio, &config);
   assert(mux && rflv_mux_release(mux));
   sps[4] = pps[4] = asc[0] = 0; /* Native setup may change after constructor return. */
   assert(rflv_mux_get_tag(mux, &tag) && tag.type == 9);
   assert(tag.spans[1].size == sizeof(avcc) && !memcmp(tag.spans[1].data, avcc, sizeof(avcc)));
   assert(rflv_mux_release(mux) && rflv_mux_get_tag(mux, &tag) && tag.type == 8);
   assert(tag.spans[1].size == 2 && ((const uint8_t*)tag.spans[1].data)[0] == 18);
   drain_setup(mux);
   assert(rflv_mux_push_annexb(mux, picture, sizeof(picture), 1000000, 1000000, true) == RFLV_PROGRESS);
   for (i = 0; i < 8 && rflv_mux_step(mux) == RFLV_PROGRESS; i++) {}
   assert(i < 8 && rflv_mux_get_tag(mux, &tag) && tag.timestamp == 1000);
   assert(rflv_mux_release(mux));
   assert(rflv_mux_push(mux, 2, picture, 2, 1000000, 1000000, false) == RFLV_PROGRESS);
   assert(rflv_mux_step(mux) == RFLV_PACKET && rflv_mux_get_tag(mux, &tag));
   assert(tag.timestamp == 1000 && tag.spans[1].data == picture);
   rflv_mux_free(mux);
   sps[4] = 103;
   pps[4] = 104;
   asc[0] = 18;
   setup(&video, &audio, &config);
   audio.bytes[0] = 0;
   assert(!record_mediacodec_stream_mux_new(&video, &audio, &config));
   video.bytes[1] = 0;
   assert(!record_mediacodec_stream_mux_new(&video, NULL, &config));
   video.data[0] = combined_avc;
   video.bytes[0] = sizeof(combined_avc);
   mux = record_mediacodec_stream_mux_new(&video, NULL, &config);
   assert(mux);
   drain_setup(mux);
   rflv_mux_free(mux);
   setup(&video, &audio, &config);
   before = queries;
   config.max_packet_size = 0xffffffu;
   assert(!record_mediacodec_stream_mux_new(&video, NULL, &config) && queries == before);
   config.max_packet_size = 1024;
   config.bit_depth = 10;
   assert(!record_mediacodec_stream_mux_new(&video, NULL, &config) && queries == before);
   setup(&video, &audio, &config);
   config.codec = RECORD_MC_STREAM_HEVC;
   video.data[0] = hevc8;
   video.bytes[0] = sizeof(hevc8);
   mux = record_mediacodec_stream_mux_new(&video, NULL, &config);
   assert(mux);
   drain_setup(mux);
   rflv_mux_free(mux);
   video.data[0] = hevc10;
   video.bytes[0] = sizeof(hevc10);
   assert(!record_mediacodec_stream_mux_new(&video, NULL, &config));
   config.bit_depth = 10;
   mux = record_mediacodec_stream_mux_new(&video, &audio, &config);
   assert(mux);
   drain_setup(mux);
   rflv_mux_free(mux);
   config.codec = RECORD_MC_STREAM_AV1;
   video.data[0] = av1;
   video.bytes[0] = sizeof(av1);
   mux = record_mediacodec_stream_mux_new(&video, NULL, &config);
   assert(mux);
   drain_setup(mux);
   rflv_mux_free(mux);
   config.bit_depth = 8;
   assert(!record_mediacodec_stream_mux_new(&video, NULL, &config));
   for (i = RECORD_MC_STREAM_AVC; i <= RECORD_MC_STREAM_AV1; i++)
   {
      config.codec = (enum record_mediacodec_stream_codec)i;
      config.bit_depth = 8;
      config.max_packet_size = i == RECORD_MC_STREAM_HEVC ? 0xfffff8u : 0xfffffbu;
      before = queries;
      assert(!record_mediacodec_stream_mux_new(&video, NULL, &config) && queries == before);
   }
   puts("PASS MediaCodec AVC/HEVC/AV1/AAC setup, copied configuration, depth admission and microsecond mux clocks");
   return 0;
}
