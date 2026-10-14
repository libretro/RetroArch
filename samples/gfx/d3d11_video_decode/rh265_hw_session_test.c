/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <formats/rh265_hw.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
typedef struct mock_backend
{
   unsigned decodes, outputs, abandoned;
   int decode_busy, output_busy, decode_error, output_error, invalid_frame, last_busy;
   rh265_hw_picture_t seen;
} mock_backend_t;
static mock_backend_t mock;
static rh265_hw_session_t session;
static rh265_video_params_t params;
static rh265_access_unit_t au;
static uint8_t packet[] = {0,0,1,0x26,1,0x80};
static rh265_nal_t slices[1];

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   mock_backend_t *m = (mock_backend_t*)owner;
   CHECK(done == 77 && value == 1);
   m->abandoned++;
}
static int decode(void *decoder, const rh265_video_params_t *p,
      const rh265_access_unit_t *a, const rh265_nal_t *n, const rh265_hw_picture_t *picture)
{
   mock_backend_t *m = (mock_backend_t*)decoder;
   CHECK(p == &params && a == &au && n == slices && n->data == packet + 3);
   m->decodes++;
   if (m->last_busy) CHECK(!memcmp(&m->seen, picture, sizeof(*picture)));
   m->seen = *picture;
   if (m->decode_error) return -1;
   if (m->decode_busy) { m->decode_busy--; m->last_busy = 1; return 1; }
   m->last_busy = 0;
   return 0;
}
static int output(void *decoder, unsigned slot, video_hw_frame_t *out)
{
   mock_backend_t *m = (mock_backend_t*)decoder;
   m->outputs++;
   if (m->output_error) return -1;
   if (m->output_busy) { m->output_busy--; return 1; }
   out->image = m->invalid_frame ? 0 : slot + 1;
   out->width = out->height = 64;
   out->done = 77; out->done_value = 1; out->owner = m; out->abandon = abandon;
   return 0;
}
static void setup(unsigned slots)
{
   rh265_hw_ops_t ops;
   memset(&mock, 0, sizeof(mock)); memset(&params, 0, sizeof(params)); memset(&au, 0, sizeof(au));
   ops.decoder = &mock; ops.decode = decode; ops.output = output;
   CHECK(rh265_hw_session_init(&session, &ops, slots));
   params.width = params.height = 64; params.bit_depth = 8; params.chroma_format_idc = 1;
   params.log2_max_poc_lsb = 4; params.max_dec_pic_buffering = (int)slots;
   au.num_slices = 1; au.intra = 1; au.first.first_slice_in_pic = 1;
   au.first.nal_type = 19; au.first.slice_type = 2; au.first.pic_output_flag = 1;
   slices[0].data = packet + 3; slices[0].size = 3;
}
static int submit(void)
{
   return rh265_hw_session_submit(&session, packet, sizeof(packet), &params, &au, slices);
}
static void next_frame(void)
{
   au.intra = 0; au.first.nal_type = 1; au.first.slice_type = 1;
   au.first.poc_lsb++;
}
static void test_retry_and_lease(void)
{
   rh265_hw_state_t saved;
   rh265_hw_picture_t pending;
   video_hw_frame_t frame;
   uint8_t other[sizeof(packet)];
   setup(4); saved = session.state; mock.decode_busy = 2;
   CHECK(submit() == 1 && mock.decodes == 1); pending = session.pending;
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)));
   CHECK(rh265_hw_session_submit(&session, other, sizeof(other), &params, &au, slices) == -1);
   CHECK(rh265_hw_session_submit(&session, packet, sizeof(packet)-1, &params, &au, slices) == -1);
   CHECK(mock.decodes == 1 && !memcmp(&pending, &session.pending, sizeof(pending)));
   CHECK(submit() == 1 && mock.decodes == 2);
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)));
   CHECK(!submit() && mock.decodes == 3 && session.state.serial == 2 && !session.phase);
   CHECK(!session.packet && !session.params && !session.au && !session.slices);
   mock.output_busy = 1; saved = session.state;
   CHECK(rh265_hw_session_output(&session, false, &frame) == 1 && !frame.image);
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)) && !mock.abandoned);
   CHECK(!rh265_hw_session_output(&session, false, &frame));
   CHECK(!session.state.outputs && frame.image == 1 && !mock.abandoned);
   video_hw_frame_abandon(&frame);
   CHECK(mock.abandoned == 1);
   CHECK(rh265_hw_session_output(&session, true, &frame) == 1 && !frame.image);
}
static void test_pressure_and_cancel(void)
{
   rh265_hw_state_t saved;
   video_hw_frame_t frame;
   setup(1); CHECK(!submit()); next_frame(); saved = session.state;
   CHECK(submit() == 1 && session.pressure && mock.decodes == 1);
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)));
   CHECK(!rh265_hw_session_output(&session, false, &frame)); video_hw_frame_abandon(&frame);
   CHECK(!submit() && mock.decodes == 2);
   CHECK(!rh265_hw_session_output(&session, true, &frame)); video_hw_frame_abandon(&frame);
   next_frame(); saved = session.state; mock.decode_busy = 1;
   CHECK(submit() == 1);
   CHECK(rh265_hw_session_cancel(&session));
   CHECK(!session.packet && !session.phase && !memcmp(&saved, &session.state, sizeof(saved)));
   CHECK(!rh265_hw_session_cancel(&session));
   CHECK(!submit() && session.state.poc[0] == 2);
}
static void test_errors(void)
{
   rh265_hw_state_t saved;
   video_hw_frame_t frame;
   setup(4); mock.decode_error = 1; saved = session.state;
   CHECK(submit() == -1 && session.broken && !session.packet);
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)));
   CHECK(submit() == -1 && mock.decodes == 1);
   setup(4); CHECK(!submit()); mock.output_error = 1; saved = session.state;
   CHECK(rh265_hw_session_output(&session, true, &frame) == -1 && session.broken && !frame.image);
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)) && !mock.abandoned);
   setup(4); CHECK(!submit()); mock.invalid_frame = 1;
   CHECK(rh265_hw_session_output(&session, true, &frame) == -1 && session.broken);
   CHECK(mock.abandoned == 1 && !frame.image);
   setup(4); CHECK(!submit()); next_frame(); mock.decode_busy = 1;
   CHECK(submit() == 1 && session.packet);
   mock.output_error = 1; saved = session.state;
   CHECK(rh265_hw_session_output(&session, true, &frame) == -1 && session.broken);
   CHECK(!session.packet && !session.phase && !memcmp(&saved, &session.state, sizeof(saved)));
   setup(4); CHECK(!submit()); next_frame(); au.first.nal_type = 9;
   CHECK(!submit() && mock.decodes == 1 && !session.phase);
   setup(4); au.num_slices = 0;
   CHECK(!submit() && !mock.decodes && !session.state.started);
   CHECK(!rh265_hw_session_init(NULL, NULL, 1));
   CHECK(rh265_hw_session_submit(NULL, packet, sizeof(packet), &params, &au, slices) == -1);
   CHECK(rh265_hw_session_output(NULL, false, &frame) == -1);
}
int main(void)
{
   test_retry_and_lease(); test_pressure_and_cancel(); test_errors();
   printf("HEVC hardware callback session: %u failures\n", failures);
   return failures ? 1 : 0;
}
