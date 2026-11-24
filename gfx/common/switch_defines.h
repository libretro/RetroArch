#ifndef SWITCH_DEFINES_H__
#define SWITCH_DEFINES_H__

#include <switch.h>
#include <gfx/scaler/scaler.h>
#include <gfx/math/matrix_4x4.h>

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

/* An overlay image as the software driver draws it: its own copy of
 * the page's pixels in the framebuffer's channel order, and the
 * rectangles the frontend set, each x, y, w, h in 0..1. */
typedef struct
{
   uint32_t *pixels;
   /* The 2D engine's premultiplied copy, in its place when the GPU
    * draws (switch_nx_gfx.c's sw_surf_t) */
   void *surf;
   /* Per image row, the first and one past the last column a texel
    * shows in; equal for a row with nothing to draw. */
   uint16_t *spans;
   /* In VIDEO_SCALE_PACK's layout. */
   unsigned dims;
   float tex[4];
   float vert[4];
   float alpha;
} switch_overlay_t;

typedef struct
{
   /* The 2D engine and its memory (switch_nx_gfx.c's nxg_t); NULL when
    * the CPU draws into fb */
   void *gpu;
   math_matrix_4x4 mvp;
   /* The screen drawn to: the window's size, packed */
   unsigned out_dims;
   /* The menu's scissor, top-down, packed */
   unsigned scissor_pos;
   unsigned scissor_dims;
   bool scissor_on;
   bool blend;
   bool vsync_changed;
   bool vsync;
   bool rgb32;
   bool smooth; /* bilinear */
   unsigned rotation;
   struct video_viewport vp;
   switch_overlay_t *overlays;
   unsigned overlays_size;
   bool overlay_enabled;
   bool overlay_full_screen;
#ifdef HAVE_MENU
   bool in_menu;
#endif
   struct
   {
      bool enable;
      bool fullscreen;
      float alpha;

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

   /* The overlay image column each screen column samples, for the
    * image being drawn */
   uint16_t overlay_cols[1280];
   /* The source column each column of a CPU draw samples */
   uint16_t op_cols[1920];
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
