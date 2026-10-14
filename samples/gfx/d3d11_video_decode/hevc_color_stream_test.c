#define main rh265_stream_main
#include "rh265_hw_stream_test.c"
#undef main
#include "../../../libretro-common/formats/mp4/rmp4_video.c"
#include "hevc_color_data.h"
#include <math.h>

static void color_stream(const uint8_t *data, size_t size, unsigned matrix,
      unsigned transfer, unsigned range, int hdr, int gray)
{
   static uint32_t pixels[64 * 64];
   static uint16_t linear[64 * 64 * 4];
   rmp4_video_stream_t *s = rmp4_video_stream_open(data, size);
   video_hw_frame_t frame;
   unsigned before = opened, i;
   int duration;
   CHECK(s != NULL); if (!s) return;
   CHECK(s->matrix == matrix && s->transfer == transfer && s->range == range);
   CHECK(rmp4_video_stream_is_hdr(s) == hdr);
   rmp4_video_stream_set_argb(s, 1);
   if (hdr)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1) && opened == before);
      s->want_hw = 1;
      rmp4_video_stream_rewind(s);
      CHECK(!rh265_video_is_hw(s->h265) && opened == before);
      rmp4_video_stream_set_output(s, (uint32_t*)linear);
      rmp4_video_stream_set_want_fp16(s, 1);
      CHECK(rmp4_video_stream_skip(s, &duration) == 1);
      rmp4_video_stream_render(s);
      CHECK(rmp4_video_stream_is_fp16(s));
   }
   else
   {
      rmp4_video_stream_set_output(s, pixels);
      CHECK(rmp4_video_stream_skip(s, &duration) == 1);
      CHECK(rmp4_video_stream_render(s) == pixels);
      for (i = 0; i < 64 * 64; i++)
         CHECK(pixels[i] == (UINT32_C(0xff000000) | (unsigned)gray * UINT32_C(0x010101)));
      rmp4_video_stream_rewind(s);
      CHECK(rmp4_video_stream_set_hw(s, 1));
      CHECK(rmp4_video_stream_skip(s, &duration) == 1);
      CHECK(rmp4_video_stream_hw_frame(s, &frame));
      CHECK(frame.yuv == (unsigned)((matrix == 1 ? IMAGE_YUV_FLAG_BT709 : 0)
               | (range ? IMAGE_YUV_FLAG_FULL_RANGE : 0)));
      video_hw_frame_abandon(&frame);
   }
   rmp4_video_stream_close(s);
}

static int clamp_rgb(double value)
{
   int result = (int)floor(value + 0.5);
   return result < 0 ? 0 : result > 255 ? 255 : result;
}

static void color_blits(void)
{
   static uint8_t y[51], u[51], v[51];
   static uint16_t y10[51], u10[51], v10[51];
   static uint32_t pixels[57];
   unsigned matrix, range, argb, kind, row, x, i;
   for (i = 0; i < 51; i++)
   {
      y[i] = (uint8_t)(16 + i * 71); u[i] = (uint8_t)(i * 47); v[i] = (uint8_t)(255 - i * 29);
      y10[i] = (uint16_t)(y[i] << 2); u10[i] = (uint16_t)(u[i] << 2); v10[i] = (uint16_t)(v[i] << 2);
   }
   for (matrix = 1; matrix <= 9; matrix += matrix == 1 ? 5 : 3)
      for (range = 0; range < 2; range++)
         for (argb = 0; argb < 2; argb++)
            for (kind = 0; kind < 3; kind++)
            {
               double kr = matrix == 1 ? 0.2126 : matrix == 6 ? 0.299 : 0.2627;
               double kb = matrix == 1 ? 0.0722 : matrix == 6 ? 0.114 : 0.0593;
               memset(pixels, 0xa5, sizeof(pixels));
               if (!kind) rmp4_video_blit_yuv(pixels, 19, 13, 3, y, 17, u, v, 17, matrix, 1, range, argb);
               else if (kind == 1) rmp4_video_blit_yuv444(pixels, 19, 13, 3, y, 17, u, v, 17, matrix, range, argb);
               else rmp4_video_blit_yuv_hbd(pixels, 19, 13, 3, y10, 17, u10, v10, 17, 1, 1, 10, matrix, range, argb);
               for (row = 0; row < 3; row++)
               {
                  for (x = 0; x < 13; x++)
                  {
                     unsigned at = kind == 1 ? row * 17 + x : (row >> 1) * 17 + (x >> 1);
                     double yy = range ? y[row * 17 + x] : (y[row * 17 + x] - 16) * (255.0 / 219);
                     double cb = (u[at] - 128) * (range ? 1.0 : 255.0 / 224);
                     double cr = (v[at] - 128) * (range ? 1.0 : 255.0 / 224);
                     int r = clamp_rgb(yy + 2 * (1 - kr) * cr);
                     int b = clamp_rgb(yy + 2 * (1 - kb) * cb);
                     int g = clamp_rgb(yy - 2 * kb * (1 - kb) / (1 - kr - kb) * cb
                           - 2 * kr * (1 - kr) / (1 - kr - kb) * cr);
                     uint32_t pixel = pixels[row * 19 + x];
                     CHECK(abs((int)((pixel >> (argb ? 16 : 0)) & 255) - r) <= 2);
                     CHECK(abs((int)((pixel >> 8) & 255) - g) <= 2);
                     CHECK(abs((int)((pixel >> (argb ? 0 : 16)) & 255) - b) <= 2);
                  }
                  for (x = 13; x < 19; x++) CHECK(pixels[row * 19 + x] == UINT32_C(0xa5a5a5a5));
               }
            }
}

int main(void)
{
   rh265_set_hw_backend(&backend);
   color_blits();
#define COLOR(n, m, t, r, h, g) color_stream(color_##n, sizeof(color_##n), m, t, r, h, g)
   COLOR(full, 1, 1, 1, 0, 32);
   COLOR(full10, 1, 1, 1, 0, 32);
   COLOR(range, 0, 0, 1, 0, 32);
   COLOR(override, 6, 1, 0, 0, 19);
   COLOR(unspecified, 1, 1, 0, 0, 19);
   COLOR(pq, 9, 16, 0, 1, 0);
   COLOR(hlg, 9, 18, 0, 1, 0);
#undef COLOR
   rh265_set_hw_backend(NULL);
   CHECK(opened == closed && exported == abandoned);
   printf("HEVC VUI/MP4 color: %u failures\n", failures);
   return failures ? 1 : 0;
}
