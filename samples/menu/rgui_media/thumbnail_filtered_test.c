#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <gfx/scaler/scaler.h>

static unsigned image_allocations, box_allocations;
static unsigned stream_contexts, full_contexts;
static int tracking;
static void *thumbnail_filtered_calloc(size_t count, size_t size)
{
   if (tracking) image_allocations++;
   return calloc(count, size);
}
static void *thumbnail_filtered_malloc(size_t size)
{
   if (tracking) box_allocations++;
   return malloc(size);
}
static bool filtered_gen_stream(struct scaler_ctx *ctx)
{
   if (tracking) stream_contexts++;
   return scaler_ctx_gen_filter_stream(ctx);
}
static bool filtered_gen_full(struct scaler_ctx *ctx)
{
   if (tracking) full_contexts++;
   return scaler_ctx_gen_filter(ctx);
}
#define scaler_ctx_gen_filter_stream filtered_gen_stream
#define scaler_ctx_gen_filter filtered_gen_full
#define calloc thumbnail_filtered_calloc
#define malloc thumbnail_filtered_malloc
#include "../../../menu/drivers/rgui.c"
#undef calloc
#undef malloc
#undef scaler_ctx_gen_filter_stream
#undef scaler_ctx_gen_filter

static uint32_t source[2048 * 1152], reference[600 * 600];
static uint16_t destination[600 * 600 + 2];

static uint32_t *reference_box(const uint32_t *src,
      unsigned src_dims, unsigned f, unsigned *out_dims)
{
   unsigned x, y, i, j;
   unsigned n  = f * f;
   unsigned sw = VIDEO_SCALE_W(src_dims);
   unsigned dw = sw / f;
   unsigned dh = VIDEO_SCALE_H(src_dims) / f;
   uint32_t *d;

   if ((dw < 1) || (dh < 1))
      return NULL;

   if (!(d = (uint32_t*)malloc((size_t)dw * dh * sizeof(uint32_t))))
      return NULL;

   for (y = 0; y < dh; y++)
   {
      for (x = 0; x < dw; x++)
      {
         unsigned a = 0, r = 0, g = 0, b = 0;

         for (j = 0; j < f; j++)
         {
            const uint32_t *row = src + (size_t)(y * f + j) * sw + x * f;

            for (i = 0; i < f; i++)
            {
               uint32_t p  = row[i];
               a          += (p >> 24) & 0xff;
               r          += (p >> 16) & 0xff;
               g          += (p >>  8) & 0xff;
               b          +=  p        & 0xff;
            }
         }

         d[(size_t)y * dw + x] =
               ((a / n) << 24) | ((r / n) << 16)
             | ((g / n) <<  8) |  (b / n);
      }
   }

   *out_dims = VIDEO_SCALE_PACK(dw, dh);
   return d;
}

/* The full-frame scaler path used before native row loans. */
static bool reference_pixels(unsigned iw, unsigned ih, unsigned ow,
      unsigned oh, unsigned type)
{
   struct scaler_ctx ctx;
   const uint32_t *input = source;
   uint32_t *box = NULL;
   unsigned f, dims;
   if (iw == ow && ih == oh)
   {
      memcpy(reference, source, (size_t)iw * ih * sizeof(*source));
      return true;
   }
   for (f = 1; iw / (f * 2) >= ow; f *= 2) {}
   if (f > 1)
   {
      box = reference_box(source, VIDEO_SCALE_PACK(iw, ih), f, &dims);
      assert(box);
      input = box; iw = VIDEO_SCALE_W(dims); ih = VIDEO_SCALE_H(dims);
   }
   memset(&ctx, 0, sizeof(ctx));
   ctx.in_width = iw; ctx.in_height = ih; ctx.in_stride = iw * 4;
   ctx.out_width = ow; ctx.out_height = oh; ctx.out_stride = ow * 4;
   ctx.in_fmt = ctx.out_fmt = SCALER_FMT_ARGB8888;
   ctx.scaler_type = type == RGUI_THUMB_SCALE_SINC ? SCALER_TYPE_SINC : SCALER_TYPE_BILINEAR;
   if (!scaler_ctx_gen_filter(&ctx))
   {
      scaler_ctx_gen_reset(&ctx);
      free(box);
      return false;
   }
   scaler_ctx_scale(&ctx, reference, input);
   scaler_ctx_gen_reset(&ctx);
   free(box);
   return true;
}

static void check_reuse_and_invalidation(rgui_t *rgui)
{
   static thumbnail_t thumb;
   struct texture_image image;
   void *previous_frame = NULL, *previous_filters = NULL;
   unsigned pass, i, queue = 0;
   scaler_ctx_gen_reset(&rgui->image_scaler);
   rgui->savestate_thumbnail_file_path[0] = '\0';
   for (pass = 0; pass < 6; pass++)
   {
      unsigned width = pass == 3 ? 280 : 300;
      unsigned height = pass == 3 ? 157 : 168;
      unsigned type = pass == 2 || pass == 3 ? RGUI_THUMB_SCALE_BILINEAR : RGUI_THUMB_SCALE_SINC;
      unsigned mode = pass % ARRAY_SIZE(rgui_pixel_format_map);
      bool reuse = pass == 1 || pass == 5;
      memset(&image, 0, sizeof(image)); memset(&thumb, 0, sizeof(thumb));
      image.width = pass == 4 ? 1280 : 640;
      image.height = pass == 4 ? 720 : 360;
      image.pixels = source;
      for (i = 0; i < image.width * image.height; i++) source[i] = i * 2654435761u + pass * 7919;
      assert(reference_pixels(image.width, image.height, width, height, type));
      argb32_to_pixel_platform_format = rgui_pixel_format_map[mode].conv;
      argb32_to_pixel_platform_format_dither = rgui_pixel_format_map[mode].conv_dither;
      thumb.data = destination + 1;
      thumb.max_dims = VIDEO_SCALE_PACK(width, 240);
      image_allocations = box_allocations = stream_contexts = full_contexts = 0;
      tracking = 1;
      rgui_process_thumbnail(rgui, &thumb, &queue, type, (pass & 1) != 0, &image);
      tracking = 0;
      assert(thumb.is_valid && stream_contexts == (reuse ? 0u : 1u));
      assert(!full_contexts && !image_allocations && !box_allocations);
      assert(rgui->image_scaler.streaming && rgui->image_scaler.frame_arena && rgui->image_scaler.filter_arena);
      if (reuse) assert(previous_frame == rgui->image_scaler.frame_arena && previous_filters == rgui->image_scaler.filter_arena);
      previous_frame = rgui->image_scaler.frame_arena;
      previous_filters = rgui->image_scaler.filter_arena;
      for (i = 0; i < width * height; i++)
      {
         uint16_t expected = pass & 1
            ? argb32_to_pixel_platform_format_dither(reference[i], i % width, i / width)
            : argb32_to_pixel_platform_format(reference[i]);
         assert(thumb.data[i] == expected);
      }
   }
   /* Failed generation must discard the old key/arenas. */
   image.width = 5; image.height = 3; thumb.max_dims = VIDEO_SCALE_PACK(3, 2);
   thumb.is_valid = false;
   rgui_process_thumbnail(rgui, &thumb, &queue, RGUI_THUMB_SCALE_SINC, false, &image);
   assert(!thumb.is_valid && !rgui->image_scaler.streaming
         && !rgui->image_scaler.frame_arena && !rgui->image_scaler.filter_arena);
   image.width = 640; image.height = 360; thumb.max_dims = VIDEO_SCALE_PACK(300, 240);
   stream_contexts = full_contexts = 0; tracking = 1;
   rgui_process_thumbnail(rgui, &thumb, &queue, RGUI_THUMB_SCALE_SINC, false, &image);
   tracking = 0;
   assert(thumb.is_valid && stream_contexts == 1 && !full_contexts);
   rgui_process_thumbnail(rgui, &thumb, &queue, RGUI_THUMB_SCALE_POINT, false, &image);
   assert(thumb.is_valid && !rgui->image_scaler.streaming
         && !rgui->image_scaler.frame_arena && !rgui->image_scaler.filter_arena);
   /* Leave a retained cache for the real menu destructor to release. */
   rgui_process_thumbnail(rgui, &thumb, &queue, RGUI_THUMB_SCALE_SINC, false, &image);
   assert(thumb.is_valid && rgui->image_scaler.streaming);
}

int main(void)
{
   static const unsigned cases[][6] = {
      {640, 360, 320, 240, 320, 180},
      {360, 640, 320, 240, 135, 240},
      {640, 360, 300, 240, 300, 168},
      {5, 3, 3, 2, 3, 1},
      {3, 5, 2, 3, 1, 3},
      {17, 13, 320, 240, 17, 13},
      {640, 360, 320, 240, 320, 240},
      {640, 360, 600, 400, 600, 337},
      {640, 640, 160, 160, 160, 160},
      {640, 360, 80, 60, 80, 45},
      {640, 360, 150, 100, 150, 84},
      {640, 360, 70, 50, 70, 39},
      {360, 640, 75, 100, 56, 100},
      {2048, 1152, 600, 400, 600, 337}
   };
   rgui_t *rgui = (rgui_t*)calloc(1, sizeof(*rgui));
   unsigned mode, dither, test, i, filter;
   assert(rgui);
   video_state_get_ptr()->av_info.geometry.aspect_ratio = 1.0f;
   for (i = 0; i < ARRAY_SIZE(source); i++) source[i] = i * 2654435761u;
   for (filter = RGUI_THUMB_SCALE_BILINEAR; filter <= RGUI_THUMB_SCALE_SINC; filter++)
      for (test = 0; test < ARRAY_SIZE(cases); test++)
      {
         unsigned width = cases[test][4], height = cases[test][5];
         bool valid = reference_pixels(cases[test][0], cases[test][1], width, height, filter);
         for (mode = 0; mode <= ARRAY_SIZE(rgui_pixel_format_map); mode++)
         {
            argb32_to_pixel_platform_format = mode == ARRAY_SIZE(rgui_pixel_format_map)
               ? argb32_to_rgba4444 : rgui_pixel_format_map[mode].conv;
            argb32_to_pixel_platform_format_dither = mode == ARRAY_SIZE(rgui_pixel_format_map)
               ? argb32_to_rgba4444_dither : rgui_pixel_format_map[mode].conv_dither;
            for (dither = 0; dither < 2; dither++)
            {
               struct texture_image image;
               static thumbnail_t thumb;
               unsigned x, y, f, queue = 0;
               void *previous_frame = rgui->image_scaler.frame_arena;
               void *previous_filters = rgui->image_scaler.filter_arena;
               bool previous_stream = rgui->image_scaler.streaming;
               memset(&image, 0, sizeof(image)); memset(&thumb, 0, sizeof(thumb));
               image.pixels = source; image.width = cases[test][0]; image.height = cases[test][1];
               thumb.data = destination + 1;
               thumb.max_dims = VIDEO_SCALE_PACK(cases[test][2], cases[test][3]);
               memset(destination, 0xa5, sizeof(destination));
               rgui->savestate_thumbnail_file_path[0] = test == 6 ? 's' : '\0';
               rgui->flags = 0;
               image_allocations = box_allocations = 0;
               stream_contexts = full_contexts = 0; tracking = 1;
               rgui_process_thumbnail(rgui, &thumb, &queue, filter, dither != 0, &image);
               tracking = 0;
               for (f = 1; image.width / (f * 2) >= width; f *= 2) {}
               assert(box_allocations == (f > 1 && width > RGUI_MAX_FB_WIDTH
                        && (image.width / f != width || image.height / f != height) ? 1u : 0u));
               if (!valid)
               {
                  assert(!thumb.is_valid && !image_allocations);
                  continue;
               }
               if ((image.width != width || image.height != height) &&
                     !(f > 1 && image.width / f == width && image.height / f == height))
               {
                  if (width <= RGUI_MAX_FB_WIDTH)
                  {
                     assert(stream_contexts <= 1 && !full_contexts);
                     assert(rgui->image_scaler.streaming && rgui->image_scaler.frame_arena
                           && rgui->image_scaler.filter_arena);
                     if (!stream_contexts)
                        assert(previous_stream && previous_frame == rgui->image_scaler.frame_arena
                              && previous_filters == rgui->image_scaler.filter_arena);
                  }
                  else
                     assert(!stream_contexts && full_contexts == 1
                           && !rgui->image_scaler.frame_arena && !rgui->image_scaler.filter_arena);
               }
               else
                  assert(!stream_contexts && !full_contexts);
               assert(thumb.is_valid && VIDEO_SCALE_W(thumb.dims) == width
                     && VIDEO_SCALE_H(thumb.dims) == height);
               assert(image_allocations == (width > RGUI_MAX_FB_WIDTH ? 1u : 0u));
               assert(destination[0] == 0xa5a5 && destination[width * height + 1] == 0xa5a5);
               for (y = 0; y < height; y++)
                  for (x = 0; x < width; x++)
                  {
                     uint32_t pixel = reference[y * width + x];
                     uint16_t expected = dither
                        ? argb32_to_pixel_platform_format_dither(pixel, x, y)
                        : argb32_to_pixel_platform_format(pixel);
                     assert(thumb.data[y * width + x] == expected);
                  }
               assert(rgui->flags & RGUI_FLAG_FORCE_REDRAW);
            }
         }
      }
   check_reuse_and_invalidation(rgui);
   rgui_free(rgui);
   assert(!rgui->image_scaler.frame_arena && !rgui->image_scaler.filter_arena);
   free(rgui);
   puts("PASS: bilinear/sinc cached source/output row loans match changing full-frame pixels on every platform/dither mode; oversized rows keep the fallback");
   return 0;
}
