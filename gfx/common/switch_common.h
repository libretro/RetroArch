#ifndef SWITCH_COMMON_H__
#define SWITCH_COMMON_H__

#include <switch.h>
#include <gfx/scaler/scaler.h>

typedef struct
{
      bool vsync;
      bool rgb32;
      bool smooth; // bilinear
      unsigned width, height;
      unsigned rotation;
      struct video_viewport vp;
      struct texture_image *overlay;
      bool overlay_enabled;
      bool in_menu;
      struct
      {
            bool enable;
            bool fullscreen;

            uint32_t *pixels;

            uint32_t width;
            uint32_t height;

            unsigned tgtw;
            unsigned tgth;

            struct scaler_ctx scaler;
      } menu_texture;

      struct {
          uint32_t width;
          uint32_t height;
          uint32_t x_offset;
      } hw_scale;

      uint32_t image[1280 * 720];
      uint32_t tmp_image[1280 * 720];
      u32 cnt;
      struct scaler_ctx scaler;
      uint32_t last_width;
      uint32_t last_height;
      bool keep_aspect;
      bool should_resize;
      bool need_clear;
      bool is_threaded;

      bool o_size;
      uint32_t o_height;
      uint32_t o_width;
} switch_video_t;

void gfx_slow_swizzling_blit(uint32_t *buffer, uint32_t *image, int w, int h, int tx, int ty, bool blend);

#endif
