/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rh265_hw.h>
#include <formats/image.h>
#include <formats/rmp4_video.h>
#include "dxva_hevc_access_unit_data.h"
/* Expose owner state for DPB-pressure fault injection. */
#include "../../../libretro-common/formats/h265/rh265.c"

static unsigned failures, opened, closed, accepted, calls, abandoned, exported;
static int decode_busy, output_busy, refuse, require_sets;
static int enable_drain, drain_busy, drain_error;
static unsigned drains;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

typedef struct mock_decoder
{
   unsigned width, height, bit_depth;
} mock_decoder_t;

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &abandoned && done == 99 && value != 0);
   abandoned++;
}

static int decode(void *decoder, const rh265_video_params_t *params,
      const rh265_access_unit_t *au, const rh265_nal_t *slices,
      const rh265_hw_picture_t *picture)
{
   CHECK(decoder && params && au && slices && picture && au->num_slices);
   CHECK(slices[0].data && slices[0].size && picture->serial);
   calls++;
   if (decode_busy) { decode_busy--; return 1; }
   accepted++;
   return 0;
}

static int output(void *decoder, unsigned slot, video_hw_frame_t *frame)
{
   mock_decoder_t *d = (mock_decoder_t*)decoder;
   if (output_busy) { output_busy--; return 1; }
   frame->image = slot + 1;
   frame->pixfmt = d->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   frame->width = d->width; frame->height = d->height;
   frame->done = 99; frame->done_value = ++exported;
   frame->owner = &abandoned; frame->abandon = abandon;
   return 0;
}

static int drain_backend(void *decoder)
{
   CHECK(decoder != NULL);
   drains++;
   if (drain_error)
      return -1;
   if (drain_busy)
   {
      drain_busy--;
      return 1;
   }
   return 0;
}

static bool open_backend(void *user, const rh265_video *source, const rh265_video_params_t *params,
      unsigned slots, rh265_hw_ops_t *ops)
{
   mock_decoder_t *d;
   CHECK(source != NULL);
   CHECK(user == &opened && slots == (unsigned)params->max_dec_pic_buffering + 1);
   if (require_sets)
   {
      rh265_nal_t sets[3];
      unsigned i;
      CHECK(!rh265_video_get_parameter_sets(source, (unsigned)params->pps_id, sets));
      for (i = 0; i < 3; i++)
         CHECK(sets[i].data && sets[i].size > 2 && ((sets[i].data[0] >> 1) & 63u) == 32u + i);
   }
   if (refuse) return false;
   d = (mock_decoder_t*)calloc(1, sizeof(*d));
   if (!d) return false;
   d->width = params->width - params->crop_left - params->crop_right;
   d->height = params->height - params->crop_top - params->crop_bottom;
   d->bit_depth = params->bit_depth;
   ops->decoder = d; ops->decode = decode; ops->output = output;
   ops->drain = enable_drain ? drain_backend : NULL;
   opened++;
   return true;
}

static void close_backend(rh265_hw_ops_t *ops)
{
   free(ops->decoder);
   memset(ops, 0, sizeof(*ops));
   closed++;
}

static const rh265_hw_backend_t backend = { &opened, open_backend, close_backend };

static rh265_video *start(void)
{
   rh265_video *v = rh265_video_open();
   CHECK(v != NULL);
   if (!v) return NULL;
   CHECK(!rh265_video_set_hw(v, 1));
   CHECK(!rh265_video_set_extradata(v, hevc_config8, sizeof(hevc_config8)));
   CHECK(rh265_video_set_hw(v, 1) && rh265_video_is_hw(v));
   return v;
}

static void check_decoder(void)
{
   rh265_video *v;
   video_hw_frame_t frame;
   uint8_t other[sizeof(hevc_sample8)];
   unsigned before = accepted, prior_abandoned = abandoned;
   int ret;
   rh265_set_hw_backend(&backend);
   v = start(); if (!v) return;
   CHECK(rh265_video_set_extradata(v, hevc_config8, sizeof(hevc_config8)) < 0);
   decode_busy = output_busy = 1;
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 2);
   memcpy(other, hevc_sample8, sizeof(other));
   CHECK(rh265_video_decode(v, other, sizeof(other)) < 0);
   CHECK(accepted == before);
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 2);
   CHECK(accepted == before + 1);
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 1);
   CHECK(accepted == before + 1 && !rh265_video_plane(v, 0, NULL, NULL, NULL));
   CHECK(!rh265_video_hw_frame(v, &frame) && frame.image);
   CHECK(rh265_video_hw_frame(v, &frame) < 0);
   rh265_set_hw_backend(NULL);
   rh265_video_close(v);
   CHECK(abandoned == prior_abandoned);
   video_hw_frame_abandon(&frame);
   CHECK(abandoned == prior_abandoned + 1);

   rh265_set_hw_backend(&backend);
   v = start(); if (!v) return;
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 1);
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 1);
   CHECK(abandoned == prior_abandoned + 2);
   rh265_video_close(v);
   CHECK(abandoned == prior_abandoned + 3);

   refuse = 1;
   v = rh265_video_open(); CHECK(v != NULL);
   CHECK(!rh265_video_set_extradata(v, hevc_config8, sizeof(hevc_config8)));
   CHECK(!rh265_video_set_hw(v, 1) && !rh265_video_is_hw(v));
   ret = rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8));
   if (!ret) CHECK(!rh265_video_drain(v));
   CHECK(ret >= 0 && rh265_video_plane(v, 0, NULL, NULL, NULL));
   rh265_video_close(v);
   refuse = 0;
}

static void check_pressure(void)
{
   rh265_video *v = start();
   rh265_hw_t *hw;
   video_hw_frame_t frame;
   unsigned i, count, before = accepted;
   int ret;
   if (!v) return;
   hw = v->hw;
   decode_busy = 1;
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 2);
   CHECK(rh265_hw_session_cancel(&hw->session));
   count = (unsigned)hw->params.max_dec_pic_buffering;
   hw->session.state.started = 1;
   hw->session.state.width = hw->params.width;
   hw->session.state.height = hw->params.height;
   hw->session.state.bit_depth = hw->params.bit_depth;
   hw->session.state.poc_log2 = hw->params.log2_max_poc_lsb;
   hw->session.state.outputs = (UINT32_C(1) << count) - 1;
   for (i = 0; i < count; i++)
   {
      hw->session.state.poc[i] = (int32_t)i;
      hw->session.state.output_epoch[i] = 1;
   }
   hw->au.first.no_output_of_prior_pics = 0;
   output_busy = 1;
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 2);
   CHECK(accepted == before && hw->session.pressure);
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 3);
   CHECK(accepted == before && hw->pending == 1);
   CHECK(!rh265_video_hw_frame(v, &frame)); video_hw_frame_abandon(&frame);
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 1);
   CHECK(accepted == before + 1 && !hw->pending);
   CHECK(!rh265_video_hw_frame(v, &frame)); video_hw_frame_abandon(&frame);
   output_busy = 1;
   CHECK(rh265_video_drain(v) == 2);
   for (i = 0; i < count; i++)
   {
      ret = rh265_video_drain(v);
      if (ret < 0) break;
      CHECK(ret == 0 && !rh265_video_hw_frame(v, &frame));
      video_hw_frame_abandon(&frame);
   }
   CHECK(i == count - 1);
   rh265_video_close(v);
}

static void check_mp4(void)
{
   static uint32_t pixels[64 * 48];
   FILE *f = fopen("../../formats/video_hdr_fp16/fixtures/sdr_hevc.mp4", "rb");
   uint8_t *bytes;
   long size;
   rmp4_video_stream_t *stream;
   video_hw_frame_t frame;
   unsigned attempt, i, count = 0, waits = 0, prior_opened = opened;
   unsigned prior_accepted = accepted;
   int ret, duration;
   CHECK(f != NULL); if (!f) return;
   CHECK(!fseek(f, 0, SEEK_END)); size = ftell(f); rewind(f);
   CHECK(size > 0); if (size <= 0) { fclose(f); return; }
   bytes = (uint8_t*)malloc((size_t)size);
   CHECK(bytes != NULL); if (!bytes) { fclose(f); return; }
   CHECK(fread(bytes, 1, (size_t)size, f) == (size_t)size); fclose(f);
   require_sets = 1;
   stream = rmp4_video_stream_open(bytes, (size_t)size);
   CHECK(stream != NULL); if (!stream) { free(bytes); return; }
   CHECK(rmp4_video_stream_set_hw(stream, 1));
   memset(pixels, 0xa5, sizeof(pixels));
   rmp4_video_stream_set_output(stream, pixels);
   decode_busy = output_busy = 1;
   for (attempt = 0; attempt < 20; attempt++)
   {
      ret = rmp4_video_stream_skip(stream, &duration);
      if (ret == 2) { CHECK(rmp4_video_stream_waiting(stream)
            && rmp4_video_stream_wait_reason(stream) == RMP4_VIDEO_WAIT_DECODER); waits++; continue; }
      if (ret < 0) break;
      CHECK(ret == 1 && !rmp4_video_stream_waiting(stream));
      CHECK(rmp4_video_stream_hw_frame(stream, &frame));
      CHECK(frame.width == 64 && frame.height == 48);
      CHECK(rmp4_video_stream_render(stream) == pixels);
      for (i = 0; i < 64 * 48; i++) CHECK(pixels[i] == UINT32_C(0xa5a5a5a5));
      video_hw_frame_abandon(&frame);
      count++;
      if (count == 1) output_busy = 1;
   }
   CHECK(count == 3 && waits == 3 && attempt < 20 && accepted == prior_accepted + 3);
   rmp4_video_stream_rewind(stream);
   CHECK(opened == prior_opened + 2);
   CHECK(rmp4_video_stream_skip(stream, &duration) == 1);
   /* Leave this export untaken: rewind owns its abandonment. */
   rmp4_video_stream_rewind(stream);
   rmp4_video_stream_close(stream);
   refuse = 1;
   stream = rmp4_video_stream_open(bytes, (size_t)size);
   CHECK(stream != NULL);
   if (stream)
   {
      CHECK(!rmp4_video_stream_set_hw(stream, 1));
      rmp4_video_stream_set_output(stream, pixels);
      CHECK(rmp4_video_stream_skip(stream, &duration) == 1);
      CHECK(!rmp4_video_stream_hw_frame(stream, &frame));
      CHECK(rmp4_video_stream_render(stream) == pixels);
      rmp4_video_stream_close(stream);
   }
   refuse = require_sets = 0;
   free(bytes);
}

static void check_eof(void)
{
   rh265_video *v;
   video_hw_frame_t frame;
   unsigned before = drains;
   memset(&frame, 0, sizeof(frame));
   enable_drain = 1;
   rh265_set_hw_backend(&backend);
   v = start();
   if (!v)
      return;
   output_busy = 1;
   CHECK(rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)) == 2);
   CHECK(drains == before);
   CHECK(rh265_video_drain(v) == 2 && drains == before);
   v->hw->session.max_reorder = 1;
   CHECK(!rh265_video_decode(v, hevc_sample8, sizeof(hevc_sample8)));
   CHECK(drains == before && v->hw->session.state.outputs);
   drain_busy = 1;
   CHECK(rh265_video_drain(v) == 2 && drains == before + 1);
   CHECK(!rh265_video_drain(v) && drains == before + 2);
   CHECK(!rh265_video_hw_frame(v, &frame));
   video_hw_frame_abandon(&frame);
   CHECK(rh265_video_drain(v) == -1 && drains == before + 3);
   rh265_video_close(v);

   v = start();
   if (!v)
      return;
   before = drains;
   drain_error = 1;
   CHECK(rh265_video_drain(v) == -2 && drains == before + 1);
   CHECK(rh265_video_drain(v) == -2 && drains == before + 1);
   rh265_video_close(v);
   enable_drain = drain_error = 0;
}

int main(void)
{
   check_decoder();
   check_pressure();
   check_eof();
   check_mp4();
   rh265_set_hw_backend(NULL);
   CHECK(opened == closed && exported == abandoned);
   printf("HEVC hardware decoder/MP4 stream: %u failures\n", failures);
   return failures ? 1 : 0;
}
