typedef struct sequence_packet
{
   const uint8_t *data;
   size_t size;
   uint32_t crc[3];
} sequence_packet_t;

typedef struct sequence_case
{
   codec_case_t picture;
   const sequence_packet_t *packets;
} sequence_case_t;

#include "sequence_data.h"

static void sequence_access_unit(rh265_video *source, const uint8_t *data,
      size_t size, unsigned index, rh265_nal_t *slices, rh265_access_unit_t *au)
{
   assert(!rh265_video_get_access_unit(source, data, size, 4, slices, 8, au));
   assert(au->first.slice_type == (index ? 1 : 2) && au->intra == (index == 0));
   if (index)
      assert(au->first.num_negative > 0 && au->first.used_before[0]
            && au->first.delta_poc_before[0] == -1);
}

static void sequence_software_pixels(rh265_video *source,
      const sequence_case_t *sequence, unsigned index)
{
   codec_case_t picture = sequence->picture;
   unsigned plane;
   assert(index < VT_HEVC_TEST_BATCH);
   memcpy(picture.crc, sequence->packets[index].crc, sizeof(picture.crc));
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
   for (which = 0; which < sizeof(sequence_cases) / sizeof(sequence_cases[0]); which++)
   {
      const sequence_case_t *sequence = &sequence_cases[which];
      rh265_video *source = rh265_video_open();
      unsigned input, output = 0;
      assert(source && !rh265_video_set_extradata(source,
               sequence->picture.hvcc, sequence->picture.hvcc_size));
      for (input = 0; input < VT_HEVC_TEST_BATCH; input++)
      {
         rh265_nal_t slices[8];
         rh265_access_unit_t au;
         int ready;
         sequence_access_unit(source, sequence->packets[input].data,
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
}
