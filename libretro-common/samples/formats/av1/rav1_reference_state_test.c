/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_reference_state.h>

static rav1_reference_state_t state, saved, other;
static rav1_reference_transaction_t ticket, old, zero;
static rav1_sequence_t seq;
static rav1_frame_prefix_t frame;
static rav1_frame_setup_t setup;
static unsigned failures;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void reset(void)
{
   unsigned i;
   memset(&state, 0, sizeof(state));
   memset(&seq, 0, sizeof(seq));
   memset(&frame, 0, sizeof(frame));
   memset(&setup, 0, sizeof(setup));
   CHECK(rav1_reference_state_reset(&state));
   setup.size.width = setup.size.upscaled_width = setup.size.render_width = 320;
   setup.size.height = setup.size.render_height = 240;
   setup.size.superres_denom = 8;
   setup.size.mi_cols = 80;
   setup.size.mi_rows = 60;
   setup.valid_mask = 255;
   for (i = 0; i < 7; i++)
      setup.ref_frame_idx[i] = i;
}

static void coded(unsigned type, unsigned show, unsigned refresh, unsigned id)
{
   memset(&frame, 0, sizeof(frame));
   frame.frame_type = type;
   frame.frame_is_intra =
       type == RAV1_FRAME_KEY || type == RAV1_FRAME_INTRA_ONLY;
   frame.show_frame = show;
   frame.showable_frame = !show || type != RAV1_FRAME_KEY;
   frame.refresh_frame_flags = refresh;
   frame.current_frame_id = id;
   frame.order_hint = id;
}

static void accept(void)
{
   saved = state;
   CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
   CHECK(!memcmp(&saved, &state, sizeof(state)));
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   CHECK(!memcmp(&saved, &state, sizeof(state)));
   CHECK(rav1_reference_commit(&state, &ticket));
}

static void reject_prepare(void)
{
   saved = state;
   memset(&ticket, 255, sizeof(ticket));
   CHECK(!rav1_reference_prepare(&ticket, &state, &seq, &frame));
   CHECK(!memcmp(&saved, &state, sizeof(state)));
   CHECK(!memcmp(&ticket, &zero, sizeof(ticket)));
}

static void test_transactions(void)
{
   uint64_t epoch;
   reset();
   coded(RAV1_FRAME_KEY, 0, 255, 4);
   CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
   saved = state;
   CHECK(!rav1_reference_commit(&state, &ticket));
   CHECK(!memcmp(&saved, &state, sizeof(state)));
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   old = ticket;
   other = state;
   CHECK(!rav1_reference_commit(&other, &ticket));
   CHECK(!memcmp(&saved, &other, sizeof(other)));
   CHECK(rav1_reference_commit(&state, &ticket));
   saved = state;
   CHECK(!rav1_reference_commit(&state, &ticket));
   CHECK(!rav1_reference_stage(&old, &state, &setup));
   CHECK(!memcmp(&saved, &state, sizeof(state)));
   coded(RAV1_FRAME_INTER, 1, 2, 5);
   CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   CHECK(!rav1_reference_stage(&ticket, &state, NULL));
   CHECK(!rav1_reference_commit(&state, &ticket));
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   epoch = state.epoch;
   CHECK(rav1_reference_state_reset(&state));
   CHECK(state.epoch == epoch + 1 && !state.have_previous &&
         !state.refs[0].valid);
   CHECK(!rav1_reference_commit(&state, &ticket));
   coded(RAV1_FRAME_INTRA_ONLY, 0, 255, 0);
   reject_prepare();
   state.epoch = UINT64_MAX - 1;
   coded(RAV1_FRAME_KEY, 1, 255, 0);
   accept();
   CHECK(state.epoch == UINT64_MAX);
   saved = state;
   CHECK(!rav1_reference_state_reset(&state));
   CHECK(!memcmp(&saved, &state, sizeof(state)));
   reject_prepare();
}

static void test_existing(void)
{
   unsigned i;
   uint64_t identity;
   reset();
   coded(RAV1_FRAME_KEY, 0, 3, 4);
   accept();
   identity = state.picture_id[0];
   CHECK(identity && identity == state.picture_id[1]);
   memset(&frame, 0, sizeof(frame));
   frame.show_existing = 1;
   frame.frame_to_show = 1;
   CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
   CHECK(ticket.ready && ticket.type == RAV1_FRAME_KEY &&
         ticket.refresh_mask == 255);
   CHECK(ticket.picture_id == identity && ticket.current.frame_id == 4);
   CHECK(rav1_reference_commit(&state, &ticket));
   for (i = 0; i < 8; i++)
   {
      CHECK(state.refs[i].valid && state.picture_id[i] == identity &&
            !state.showable[i]);
      frame.frame_to_show = i;
      reject_prepare();
   }
   coded(RAV1_FRAME_INTER, 1, 5, 6);
   accept();
   memset(&frame, 0, sizeof(frame));
   frame.show_existing = 1;
   frame.frame_to_show = 2;
   for (i = 0; i < 2; i++)
   {
      CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
      CHECK(ticket.type == RAV1_FRAME_INTER && !ticket.refresh_mask);
      CHECK(rav1_reference_commit(&state, &ticket));
      CHECK(state.previous_frame_id == 6 && state.showable[2]);
   }
   frame.frame_to_show = 8;
   reject_prepare();
   frame.frame_to_show = 2;
   state.refs[2].valid = 0;
   reject_prepare();
}

static void test_ids(void)
{
   reset();
   seq.frame_id_numbers_present = 1;
   seq.frame_id_length = 5;
   coded(RAV1_FRAME_KEY, 0, 255, 28);
   accept();
   coded(RAV1_FRAME_INTER, 1, 255, 31);
   accept();
   coded(RAV1_FRAME_INTER, 1, 255, 1);
   accept();
   reject_prepare();
   frame.current_frame_id = 0;
   reject_prepare();
   frame.current_frame_id = 17;
   reject_prepare();
   frame.current_frame_id = 32;
   reject_prepare();
   coded(RAV1_FRAME_KEY, 1, 255, 17);
   setup.valid_mask = 0;
   accept();
   coded(RAV1_FRAME_INTER, 1, 1, 18);
   setup.valid_mask = 255;
   accept();
   memset(&frame, 0, sizeof(frame));
   frame.show_existing = 1;
   frame.display_frame_id = 17;
   reject_prepare();
   frame.display_frame_id = 18;
   CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
   CHECK(rav1_reference_commit(&state, &ticket));
   seq.frame_id_length = 0;
   reject_prepare();
}

static void test_scaling(void)
{
   reset();
   coded(RAV1_FRAME_KEY, 1, 255, 0);
   accept();
   coded(RAV1_FRAME_INTER, 1, 1, 1);
   CHECK(rav1_reference_prepare(&ticket, &state, &seq, &frame));
   state.refs[6].size.upscaled_width = 640;
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.upscaled_width = 641;
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
   CHECK(!ticket.ready);
   state.refs[6].size.upscaled_width = 20;
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.upscaled_width = 19;
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.upscaled_width = 320;
   state.refs[6].size.height = 480;
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.height = 481;
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.height = 15;
   CHECK(rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.height = 14;
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
   state.refs[6].size.height = 240;
   setup.ref_frame_idx[6] = 8;
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
   setup.ref_frame_idx[6] = 6;
   setup.valid_mask &= ~(1u << 6);
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
   setup.valid_mask = 255;
   setup.size.width = 65537;
   CHECK(!rav1_reference_stage(&ticket, &state, &setup));
}

int main(void)
{
   test_transactions();
   test_existing();
   test_ids();
   test_scaling();
   printf("AV1 reference transactions: %u failures\n", failures);
   return failures ? 1 : 0;
}
