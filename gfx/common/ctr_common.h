/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouahl
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CTR_COMMON_H__
#define CTR_COMMON_H__

#include <3ds.h>
#include <retro_inline.h>

#define COLOR_ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((b) << 16) | ((g) << 8) | ((r) << 0))

#define CTR_TOP_FRAMEBUFFER_WIDTH      400
#define CTR_TOP_FRAMEBUFFER_HEIGHT     240
#define CTR_BOTTOM_FRAMEBUFFER_WIDTH   320
#define CTR_BOTTOM_FRAMEBUFFER_HEIGHT  240
#define CTR_STATE_DATE_SIZE            11

#ifdef USE_CTRULIB_2
extern u8* gfxTopLeftFramebuffers[2];
extern u8* gfxTopRightFramebuffers[2];
extern u8* gfxBottomFramebuffers[2];
#endif

#ifdef CONSOLE_LOG
extern PrintConsole* ctrConsole;
#endif 

extern const u8 ctr_sprite_shbin[];
extern const u32 ctr_sprite_shbin_size;

static const bool bottom_font_enable;
static const int bottom_font_color_red;
static const int bottom_font_color_green;
static const int bottom_font_color_blue;
static const int bottom_font_color_opacity;

typedef struct
{
   float v;
   float u;
   float y;
   float x;
} ctr_scale_vector_t;

typedef struct
{
   s16 x0, y0, x1, y1;
   s16 u0, v0, u1, v1;
} ctr_vertex_t;

typedef enum
{
   CTR_VIDEO_MODE_3D = 0,
   CTR_VIDEO_MODE_2D,
   CTR_VIDEO_MODE_2D_400X240,
   CTR_VIDEO_MODE_2D_800X240,
   CTR_VIDEO_MODE_LAST
} ctr_video_mode_enum;

typedef enum
{
   CTR_BOTTOM_MENU_NOT_AVAILABLE = 0,
   CTR_BOTTOM_MENU_DEFAULT,
   CTR_BOTTOM_MENU_SELECT,
} ctr_bottom_menu;

typedef struct ctr_video
{
   struct
   {
      struct
      {
         void* left;
         void* right;
      }top;
      void* bottom;
   }drawbuffers;
   void* depthbuffer;

   struct
   {
      uint32_t* display_list;
      int display_list_size;
      void* texture_linear;
      void* texture_swizzled;
      int texture_width;
      int texture_height;
      ctr_scale_vector_t scale_vector;
      ctr_vertex_t* frame_coords;
   }menu;

   uint32_t* display_list;
   int display_list_size;
   void* texture_linear;
   void* texture_swizzled;
   int texture_width;
   int texture_height;

   ctr_scale_vector_t scale_vector;
   ctr_vertex_t* frame_coords;

   DVLB_s*         dvlb;
   shaderProgram_s shader;

   video_viewport_t vp;

   bool rgb32;
   bool vsync;
   bool smooth;
   bool menu_texture_enable;
   bool menu_texture_frame_enable;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool msg_rendering_enabled;
   bool supports_parallax_disable;
   bool enable_3d;

#ifdef HAVE_OVERLAY
   struct ctr_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enabled;
   bool overlay_full_screen;
#endif

   aptHookCookie lcd_aptHook;
   ctr_video_mode_enum video_mode;
   int current_buffer_top;
   int current_buffer_bottom;

   bool p3d_event_pending;
   bool ppf_event_pending;
   volatile bool vsync_event_pending;

   struct
   {
      ctr_vertex_t* buffer;
      ctr_vertex_t* current;
      int size;
   }vertex_cache;

   bool init_bottom_menu;
   bool refresh_bottom_menu;
   bool render_font_bottom;
   bool render_state_from_png_file;
   bool state_data_exist;
   char state_date[CTR_STATE_DATE_SIZE];
   int state_slot;
   bool bottom_check_idle;
   bool bottom_is_idle;
   bool bottom_is_fading;
   u64  idle_timestamp;
   ctr_bottom_menu bottom_menu;
   ctr_bottom_menu prev_bottom_menu;
   struct ctr_bottom_texture_data *bottom_textures;
} ctr_video_t;

typedef struct ctr_texture
{
   int width;
   int height;
   int active_width;
   int active_height;

   enum texture_filter_type type;
   void* data;
} ctr_texture_t;

#ifdef HAVE_OVERLAY
struct ctr_overlay_data
{
   ctr_texture_t texture;
   ctr_vertex_t* frame_coords;
   ctr_scale_vector_t scale_vector;
   float alpha_mod;
};
#endif

struct ctr_bottom_texture_data
{
   uintptr_t texture;
   ctr_vertex_t* frame_coords;
   ctr_scale_vector_t scale_vector;
};

static INLINE void ctr_set_scale_vector(ctr_scale_vector_t* vec,
      int viewport_width, int viewport_height,
      int texture_width, int texture_height)
{
   vec->x = -2.0f / viewport_width;
   vec->y = -2.0f / viewport_height;
   vec->u =  1.0f / texture_width;
   vec->v = -1.0f / texture_height;
}

#endif /* CTR_COMMON_H__ */
