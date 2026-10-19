#include <formats/rh265_hw.h>

typedef struct sequence_packet
{
   int64_t dts;
   const uint8_t *data;
   size_t size;
   uint32_t crc[3];
   unsigned display_index, slice_type;
} sequence_packet_t;

typedef struct sequence_case
{
   codec_case_t picture;
   const sequence_packet_t *packets;
   unsigned reordered;
} sequence_case_t;

#include "sequence_data.h"
#include "reorder_data.h"

static void sequence_access_unit(rh265_video *source, const sequence_case_t *sequence,
      const uint8_t *data, size_t size, unsigned index,
      rh265_nal_t *slices, rh265_access_unit_t *au)
{
   const sequence_packet_t *expected = &sequence->packets[index];
   assert(!rh265_video_get_access_unit(source, data, size, 4, slices, 8, au));
   assert(au->first.slice_type == (int)expected->slice_type && au->intra == (index == 0));
   if (index)
   {
      assert(au->first.poc_lsb == (int)expected->display_index);
      assert(au->first.num_negative > 0 && au->first.used_before[0]);
      if (!sequence->reordered) assert(au->first.delta_poc_before[0] == -1);
      if (!expected->slice_type)
         assert(au->first.num_positive > 0 && au->first.used_after[0]);
   }
}

static void sequence_software_pixels(rh265_video *source,
      const sequence_case_t *sequence, unsigned index)
{
   codec_case_t picture = sequence->picture;
   unsigned plane, packet;
   assert(index < VT_HEVC_TEST_BATCH);
   for (packet = 0; packet < VT_HEVC_TEST_BATCH; packet++)
      if (sequence->packets[packet].display_index == index) break;
   assert(packet < VT_HEVC_TEST_BATCH);
   memcpy(picture.crc, sequence->packets[packet].crc, sizeof(picture.crc));
   assert((unsigned)rh265_video_bit_depth(source) == picture.depth);
   for (plane = 0; plane < 3; plane++)
   {
      int stride, width, height, x, y;
      const uint8_t *data = rh265_video_plane(source, (int)plane, &stride, &width, &height);
      uint32_t crc = UINT32_MAX;
      assert(data && width == (plane ? 32 : 64) && height == width && stride >= width);
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
         {
            unsigned value = picture.depth == 10
               ? ((const uint16_t*)data)[(size_t)y * stride + x]
               : data[(size_t)y * stride + x];
            crc = codec_crc_sample(crc, value, picture.depth);
         }
      codec_check_crc(&picture, plane, crc ^ UINT32_MAX);
   }
}

static void test_sequence_software(void)
{
   unsigned which;
   unsigned singles = sizeof(sequence_cases) / sizeof(sequence_cases[0]);
   unsigned reorders = sizeof(reorder_cases) / sizeof(reorder_cases[0]);
   for (which = 0; which < singles + reorders; which++)
   {
      const sequence_case_t *sequence = which < singles ? &sequence_cases[which] : &reorder_cases[which - singles];
      rh265_video *source = rh265_video_open();
      unsigned input, output = 0;
      assert(source && !rh265_video_set_extradata(source,
               sequence->picture.hvcc, sequence->picture.hvcc_size));
      for (input = 0; input < VT_HEVC_TEST_BATCH; input++)
      {
         rh265_nal_t slices[8];
         rh265_access_unit_t au;
         int ready;
         sequence_access_unit(source, sequence, sequence->packets[input].data,
               sequence->packets[input].size, input, slices, &au);
         ready = rh265_video_decode(source, sequence->packets[input].data,
               sequence->packets[input].size);
         if (ready < 0) fprintf(stderr, "%s picture %u: software decode status %d\n",
               sequence->picture.name, input, ready);
         assert(ready >= 0 && ready <= 1);
         if (ready) sequence_software_pixels(source, sequence, output++);
      }
      while (!rh265_video_drain(source)) sequence_software_pixels(source, sequence, output++);
      assert(output == VT_HEVC_TEST_BATCH);
      rh265_video_close(source);
   }
   puts("VideoToolbox sequence software: 32 dependent Main/Main10 pictures match FFmpeg pixels");
   puts("VideoToolbox reorder software: 32 B-stream pictures drain in FFmpeg display order");
}

static void sequence_hw_take(rh265_hw_state_t *state, unsigned reorder,
      bool drain, unsigned *output)
{
   int slot;
   while ((slot = rh265_hw_output_pick(state, reorder, drain)) >= 0)
   {
      uint32_t references = state->references;
      assert(*output < VT_HEVC_TEST_BATCH);
      assert(state->poc[slot] == (int32_t)*output);
      assert(rh265_hw_output_release(state, (unsigned)slot));
      assert(state->references == references);
      (*output)++;
      if (drain) break;
   }
}

static void test_sequence_hardware_state(void)
{
   struct work
   {
      rh265_hw_state_t state, before;
      rh265_hw_picture_t picture;
      rh265_video_params_t params;
   } *work = (struct work*)malloc(sizeof(*work));
   unsigned which, mode, pressure = 0;
   unsigned singles = sizeof(sequence_cases) / sizeof(sequence_cases[0]);
   unsigned reorders = sizeof(reorder_cases) / sizeof(reorder_cases[0]);
   assert(work);
   for (which = 0; which < singles + reorders; which++) for (mode = 0; mode < 2; mode++)
   {
      const sequence_case_t *sequence = which < singles ? &sequence_cases[which] : &reorder_cases[which - singles];
      rh265_video *source = rh265_video_open();
      unsigned input, output = 0;
      assert(source && !rh265_video_set_extradata(source,
               sequence->picture.hvcc, sequence->picture.hvcc_size));
      assert(rh265_hw_state_init(&work->state, 32));
      for (input = 0; input < VT_HEVC_TEST_BATCH; input++)
      {
         rh265_nal_t slices[8];
         rh265_access_unit_t au;
         int status;
         sequence_access_unit(source, sequence, sequence->packets[input].data,
               sequence->packets[input].size, input, slices, &au);
         assert(!rh265_video_get_params(source, (unsigned)au.first.pps_id, &work->params));
         for (;;)
         {
            unsigned before = output;
            memcpy(&work->before, &work->state, sizeof(work->state));
            status = rh265_hw_prepare(&work->state, &work->params, &au, &work->picture);
            assert(!memcmp(&work->before, &work->state, sizeof(work->state)));
            if (status != 1) break;
            assert(mode);
            pressure++;
            sequence_hw_take(&work->state, 0, true, &output);
            assert(output == before + 1);
         }
         if (status) fprintf(stderr, "%s hardware state picture %u: status %d\n",
               sequence->picture.name, input, status);
         assert(!status && work->picture.poc == (int32_t)sequence->packets[input].display_index);
         {
            unsigned ref, past = 0, future = 0;
            assert(work->picture.num_refs <= 15);
            for (ref = 0; ref < work->picture.num_refs; ref++)
            {
               unsigned slot = work->picture.refs[ref].slot;
               assert(slot < 32 && (work->state.references & (UINT32_C(1) << slot)));
               assert(work->state.poc[slot] == work->picture.refs[ref].poc);
               past += work->picture.refs[ref].poc < work->picture.poc;
               future += work->picture.refs[ref].poc > work->picture.poc;
            }
            if (input) assert(past);
            if (!sequence->packets[input].slice_type) assert(future);
         }
         assert(rh265_hw_commit(&work->state, &work->picture));
         memcpy(&work->before, &work->state, sizeof(work->state));
         assert(!rh265_hw_commit(&work->state, &work->picture));
         assert(!memcmp(&work->before, &work->state, sizeof(work->state)));
         if (!mode) sequence_hw_take(&work->state, (unsigned)work->params.max_num_reorder_pics, false, &output);
      }
      while (work->state.outputs) sequence_hw_take(&work->state, 0, true, &output);
      assert(output == VT_HEVC_TEST_BATCH);
      rh265_video_close(source);
   }
   assert(pressure);
   free(work);
   puts("VideoToolbox hardware state: 128 I/P/B pictures preserve display order and pressure retries");
}
