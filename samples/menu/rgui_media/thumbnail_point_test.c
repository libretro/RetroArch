#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned allocations;
static int tracking;
static void *thumbnail_test_calloc(size_t count, size_t size)
{
   if (tracking) allocations++;
   return calloc(count, size);
}
static void *thumbnail_test_malloc(size_t size)
{
   if (tracking) allocations++;
   return malloc(size);
}
#define calloc thumbnail_test_calloc
#define malloc thumbnail_test_malloc
#include "../../../menu/drivers/rgui.c"
#undef calloc
#undef malloc

static uint32_t source[640 * 640];
static uint16_t destination[320 * 240 + 2];

int main(void)
{
   static const unsigned cases[][6] = {
      {640, 360, 320, 240, 320, 180},
      {360, 640, 320, 240, 135, 240},
      {5, 3, 3, 2, 3, 1},
      {3, 5, 2, 3, 1, 3},
      {17, 13, 320, 240, 17, 13},
      {640, 360, 320, 240, 320, 240}
   };
   rgui_t *rgui = (rgui_t*)calloc(1, sizeof(*rgui));
   unsigned mode, dither, test, i;
   assert(rgui);
   video_state_get_ptr()->av_info.geometry.aspect_ratio = 1.0f;
   for (i = 0; i < ARRAY_SIZE(source); i++)
      source[i] = i * 2654435761u;
   for (mode = 0; mode <= ARRAY_SIZE(rgui_pixel_format_map); mode++)
   {
      /* The same converters selected by each platform's video driver. */
      argb32_to_pixel_platform_format = mode == ARRAY_SIZE(rgui_pixel_format_map) ? argb32_to_rgba4444 : rgui_pixel_format_map[mode].conv;
      argb32_to_pixel_platform_format_dither = mode == ARRAY_SIZE(rgui_pixel_format_map) ? argb32_to_rgba4444_dither : rgui_pixel_format_map[mode].conv_dither;
      for (dither = 0; dither < 2; dither++)
         for (test = 0; test < ARRAY_SIZE(cases); test++)
         {
            struct texture_image image;
            static thumbnail_t thumb;
            unsigned x, y, queue = 0;
            unsigned width = cases[test][4], height = cases[test][5];
            bool scaling = cases[test][0] > cases[test][2]
               || cases[test][1] > cases[test][3];
            memset(&image, 0, sizeof(image));
            memset(&thumb, 0, sizeof(thumb));
            image.pixels = source;
            image.width = cases[test][0]; image.height = cases[test][1];
            thumb.data = destination + 1;
            thumb.max_dims = VIDEO_SCALE_PACK(cases[test][2], cases[test][3]);
            memset(destination, 0xa5, sizeof(destination));
            rgui->savestate_thumbnail_file_path[0] = test == 5 ? 's' : '\0';
            allocations = 0;
            tracking = 1;
            rgui_process_thumbnail(rgui, &thumb, &queue, RGUI_THUMB_SCALE_POINT,
                  dither != 0, &image);
            tracking = 0;
            if (VIDEO_SCALE_W(thumb.dims) != width || VIDEO_SCALE_H(thumb.dims) != height)
               fprintf(stderr, "case %u: %ux%u expected %ux%u\n", test,
                     VIDEO_SCALE_W(thumb.dims), VIDEO_SCALE_H(thumb.dims), width, height);
            assert(thumb.is_valid && VIDEO_SCALE_W(thumb.dims) == width
                  && VIDEO_SCALE_H(thumb.dims) == height);
            assert(allocations == 0);
            assert(destination[0] == 0xa5a5 && destination[width * height + 1] == 0xa5a5);
            for (y = 0; y < height; y++)
               for (x = 0; x < width; x++)
               {
                  unsigned sx = scaling ? (x * ((image.width << 16) / width)) >> 16 : x;
                  unsigned sy = scaling ? (y * ((image.height << 16) / height)) >> 16 : y;
                  uint32_t pixel = source[sy * image.width + sx];
                  uint16_t expected = dither
                     ? argb32_to_pixel_platform_format_dither(pixel, x, y)
                     : argb32_to_pixel_platform_format(pixel);
                  assert(thumb.data[y * width + x] == expected);
               }
            assert(rgui->flags & RGUI_FLAG_FORCE_REDRAW);
         }
   }
   free(rgui);
   puts("PASS: RGUI point scaling writes native thumbnails directly, without allocation, for every platform format and dither mode");
   return 0;
}
