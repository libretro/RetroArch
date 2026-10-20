typedef struct hdr_codec_case
{
   codec_case_t picture;
   unsigned transfer;
} hdr_codec_case_t;
#include "hdr_codec_data.h"

static void test_hdr_codec_software(void)
{
   unsigned i, plane;
   for (i = 0; i < sizeof(hdr_codec_cases) / sizeof(hdr_codec_cases[0]); i++)
   {
      const hdr_codec_case_t *hdr = &hdr_codec_cases[i];
      const codec_case_t *fixture = &hdr->picture;
      rh265_video *source = rh265_video_open();
      rh265_video_color_t color;
      int ready;
      assert(source && !rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      assert(!rh265_video_get_color(source, 0, &color));
      assert(color.video_signal_present && color.primaries == 9 && color.matrix == 9
            && color.transfer == hdr->transfer && color.full_range == fixture->full);
      ready = rh265_video_decode(source, fixture->sample, fixture->sample_size);
      assert(ready >= 0 && ready <= 1);
      if (!ready) assert(!rh265_video_drain(source));
      assert((unsigned)rh265_video_bit_depth(source) == fixture->depth);
      for (plane = 0; plane < 3; plane++)
      {
         int stride, width, height, x, y;
         const uint8_t *data = rh265_video_plane(source, (int)plane, &stride, &width, &height);
         uint32_t crc = UINT32_MAX;
         assert(data && width == (plane ? 32 : 64) && height == width && stride >= width);
         for (y = 0; y < height; y++) for (x = 0; x < width; x++)
         {
            unsigned value = fixture->depth == 10 ? ((const uint16_t*)data)[(size_t)y * stride + x]
               : data[(size_t)y * stride + x];
            crc = codec_crc_sample(crc, value, fixture->depth);
         }
         codec_check_crc(fixture, plane, crc ^ UINT32_MAX);
      }
      rh265_video_close(source);
   }
   puts("VideoToolbox HDR software: eight coded BT.2020 PQ/HLG sources preserve VUI and FFmpeg planes");
}
