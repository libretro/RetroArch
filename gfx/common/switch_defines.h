#ifndef SWITCH_DEFINES_H__
#define SWITCH_DEFINES_H__

#include <switch.h>
#include <gfx/scaler/scaler.h>

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

typedef struct
{
   bool vsync;
   bool rgb32;
   bool smooth; /* bilinear */
   unsigned rotation;
   struct video_viewport vp;
   struct texture_image *overlay;
   bool overlay_enabled;
#ifdef HAVE_MENU
   bool in_menu;
#endif
   struct
   {
      bool enable;
      bool fullscreen;

      uint32_t *pixels;

      /* The size the frame arrives at, and the size it is scaled
       * to on screen, each packed. */
      uint32_t dims;
      unsigned tgt_dims;

      struct scaler_ctx scaler;
   } menu_texture;

   struct
   {
      /* The window size the scaler asks the compositor for, packed;
       * x_offset is where the frame sits inside it. */
      uint32_t dims;
      uint32_t x_offset;
   } hw_scale;

   uint32_t image[1280 * 720];
   uint32_t tmp_image[1280 * 720];
   u32 cnt;
   struct scaler_ctx scaler;
   /* The frame size the scaler was last built for, packed. */
   uint32_t last_dims;
   bool keep_aspect;
   /* What the last frame said integer scaling should be:
    * set_aspect_ratio() runs on the video thread under the threaded
    * wrapper, and reading the setting there races the menu writing it. */
   bool frame_scale_integer;
   bool should_resize;
   bool need_clear;
   bool is_threaded;

   bool o_size;
   /* The original size o_size shows the frame at, packed. */
   uint32_t o_dims;

   NWindow *win;
   Framebuffer fb;

   /* needed for the switch font driver */
   uint32_t *out_buffer;
   uint32_t stride;
} switch_video_t;

typedef struct
{
#ifdef HAVE_EGL
    egl_ctx_data_t egl;
#endif

    float refresh_rate;
    NWindow *win;
} switch_ctx_data_t;

#endif
