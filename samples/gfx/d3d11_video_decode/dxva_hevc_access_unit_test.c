/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/dxva_hevc_rh265.h"
#include "dxva_hevc_access_unit_data.h"

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

typedef struct pipeline_backend
{
   uint8_t packed[512], records[40];
   size_t written;
   unsigned decodes, outputs, abandoned;
} pipeline_backend_t;

static int pipeline_decode(void *decoder, const rh265_video_params_t *params,
      const rh265_access_unit_t *au, const rh265_nal_t *slices,
      const rh265_hw_picture_t *pending)
{
   pipeline_backend_t *b = (pipeline_backend_t*)decoder;
   dxva_hevc_picture_t picture;
   uint8_t bytes[DXVA_HEVC_PP_SIZE];
   b->decodes++;
   if (b->decodes == 1) return 1;
   if (!dxva_hevc_picture_from_state(&picture, params, au, pending)
         || !dxva_hevc_pic_params(bytes, &picture, 1)
         || !dxva_hevc_pack_slices(b->packed, sizeof(b->packed), b->records,
               sizeof(b->records), slices, au->num_slices, &b->written)) return -1;
   return 0;
}

static void pipeline_abandon(void *owner, uint64_t done, uint64_t value)
{
   pipeline_backend_t *b = (pipeline_backend_t*)owner;
   CHECK(done == 99 && value == 1);
   b->abandoned++;
}

static int pipeline_output(void *decoder, unsigned slot, video_hw_frame_t *frame)
{
   pipeline_backend_t *b = (pipeline_backend_t*)decoder;
   b->outputs++;
   if (b->outputs == 1) return 1;
   frame->image = slot + 1; frame->width = frame->height = 64;
   frame->done = 99; frame->done_value = 1;
   frame->owner = b; frame->abandon = pipeline_abandon;
   return 0;
}

static void check_picture(const uint8_t *config, size_t config_size,
      const uint8_t *params, size_t params_size, const uint8_t *sample,
      size_t sample_size, int depth)
{
   static rh265_video_params_t metadata;
   static rh265_hw_session_t session;
   static rh265_hw_state_t unchanged;
   static pipeline_backend_t backend;
   rh265_hw_ops_t ops;
   video_hw_frame_t frame;
   rh265_access_unit_t au, saved;
   rh265_nal_t slices[4];

   int ret, plane;
   rh265_video *original = rh265_video_open(), *converted = rh265_video_open();
   CHECK(original && converted);
   if (!original || !converted) goto done;
   CHECK(!rh265_video_set_extradata(original, config, config_size));
   CHECK(!rh265_video_get_access_unit(original, sample, sample_size, 4, slices, 4, &au));
   CHECK(au.num_slices == 1 && au.intra && au.first.idr && au.first.first_slice_in_pic);
   CHECK(slices[0].data == sample + 4 && slices[0].size == sample_size - 4);
   saved = au;
   CHECK(!rh265_video_get_access_unit(original, sample, sample_size, 4, slices, 4, &au));
   CHECK(!memcmp(&saved, &au, sizeof(au)));
   CHECK(!rh265_video_get_params(original, (unsigned)au.first.pps_id, &metadata));
   CHECK(metadata.bit_depth == depth && metadata.width == 64 && metadata.height == 64);
   memset(&backend, 0, sizeof(backend));
   ops.decoder = &backend; ops.decode = pipeline_decode; ops.output = pipeline_output;
   CHECK(rh265_hw_session_init(&session, &ops, 32)); unchanged = session.state;
   CHECK(rh265_hw_session_submit(&session, sample, sample_size, &metadata, &au, slices) == 1);
   CHECK(!memcmp(&session.state, &unchanged, sizeof(unchanged)) && !backend.written);
   CHECK(!rh265_hw_session_submit(&session, sample, sample_size, &metadata, &au, slices));
   CHECK(backend.decodes == 2 && session.state.serial == 2 && !session.phase);
   CHECK(backend.written <= sizeof(backend.packed) && !(backend.written & 127));
   CHECK(!memcmp(backend.packed + 3, sample + 4, sample_size - 4));
   unchanged = session.state;
   CHECK(rh265_hw_session_output(&session, true, &frame) == 1);
   CHECK(!memcmp(&session.state, &unchanged, sizeof(unchanged)) && !frame.image);
   CHECK(!rh265_hw_session_output(&session, true, &frame));
   CHECK(!session.state.outputs && frame.image && !backend.abandoned);
   video_hw_frame_abandon(&frame);
   CHECK(backend.abandoned == 1);
   CHECK(!rh265_video_plane(original, 0, NULL, NULL, NULL));
   CHECK(!rh265_video_plane(converted, 0, NULL, NULL, NULL));
   ret = rh265_video_decode(original, sample, sample_size);
   CHECK(ret >= 0);
   if (!ret) CHECK(!rh265_video_drain(original));
   CHECK(!rh265_video_decode(converted, params, params_size));
   ret = rh265_video_decode(converted, backend.packed, backend.written);
   CHECK(ret >= 0);
   if (!ret) CHECK(!rh265_video_drain(converted));
   CHECK(rh265_video_bit_depth(original) == depth && rh265_video_bit_depth(converted) == depth);
   for (plane = 0; plane < 3; plane++)
   {
      int as, aw, ah, bs, bw, bh, row, bps = depth == 10 ? 2 : 1;
      const uint8_t *a = rh265_video_plane(original, plane, &as, &aw, &ah);
      const uint8_t *b = rh265_video_plane(converted, plane, &bs, &bw, &bh);
      CHECK(a && b);
      if (!a || !b) continue;
      CHECK(aw == bw && ah == bh && aw == (plane ? 32 : 64));
      if (aw != bw || ah != bh) continue;
      for (row = 0; row < ah; row++)
         CHECK(!memcmp(a + (size_t)row * as * bps, b + (size_t)row * bs * bps, (size_t)aw * bps));
   }
done:
   rh265_video_close(original); rh265_video_close(converted);
}

static void check_long_term(void)
{
   static rh265_video_params_t params;
   static rh265_hw_state_t state;
   static rh265_hw_picture_t pending;
   static rh265_access_unit_t au;
   dxva_hevc_picture_t picture;
   uint8_t bytes[DXVA_HEVC_PP_SIZE];
   rh265_video *v = rh265_video_open();
   CHECK(v && !rh265_video_set_extradata(v, hevc_config8, sizeof(hevc_config8)));
   if (!v) return;
   CHECK(!rh265_video_get_params(v, 0, &params));
   params.long_term_ref_pics_present = 1;
   CHECK(rh265_hw_state_init(&state, 8));
   memset(&au, 0, sizeof(au)); au.num_slices = 1; au.intra = 1;
   au.first.nal_type = 19; au.first.slice_type = 2; au.first.first_slice_in_pic = 1;
   au.first.pic_output_flag = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &pending) && rh265_hw_commit(&state, &pending));
   au.intra = 0; au.first.nal_type = 1; au.first.slice_type = 1; au.first.poc_lsb = 1;
   au.first.short_term_rps_sps_idx = -1; au.first.short_term_rps_bits = 2;
   au.first.long_term.count = 1; au.first.long_term.used[0] = 1;
   au.first.long_term.msb_present[0] = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &pending));
   CHECK(dxva_hevc_picture_from_state(&picture, &params, &au, &pending));
   CHECK(picture.num_longterm == 1 && !picture.num_before && !picture.num_after);
   CHECK(dxva_hevc_pic_params(bytes, &picture, 1));
   CHECK(bytes[124] == 0x80 && bytes[216] == 0 && bytes[217] == 0xff);
   au.first.first_slice_in_pic = 0;
   CHECK(dxva_hevc_slice_from_rh265(&picture, &au.first));
   au.first.long_term.poc_lsb[0] = 3;
   CHECK(!dxva_hevc_slice_from_rh265(&picture, &au.first));
   rh265_video_close(v);
}

int main(void)
{
   check_picture(hevc_config8, sizeof(hevc_config8), hevc_params8, sizeof(hevc_params8),
         hevc_sample8, sizeof(hevc_sample8), 8);
   check_picture(hevc_config10, sizeof(hevc_config10), hevc_params10, sizeof(hevc_params10),
         hevc_sample10, sizeof(hevc_sample10), 10);
   check_long_term();
   printf("HEVC access-unit pipeline: %u failures\n", failures);
   return failures ? 1 : 0;
}
