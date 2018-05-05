#pragma once

#include <wiiu/gx2.h>

#include "../video_defines.h"
#include "../video_shader_parse.h"

#include "../drivers/gx2_shaders/frame.h"
#include "../drivers/gx2_shaders/tex.h"
#include "../drivers/gx2_shaders/sprite.h"
#include "../drivers/gx2_shaders/menu_shaders.h"

#undef _X
#undef _B

#define _X 0x00
#define _Y 0x01
#define _Z 0x02
#define _W 0x03
#define _R 0x00
#define _G 0x01
#define _B 0x02
#define _A 0x03
#define _0 0x04
#define _1 0x05
#define GX2_COMP_SEL(c0, c1, c2, c3) (((c0) << 24) | ((c1) << 16) | ((c2) << 8) | (c3))

#define COLOR_ARGB(r, g, b, a) (((u32)(a) << 24) | ((u32)(r) << 16) | ((u32)(g) << 8) | ((u32)(b) << 0))
#define COLOR_RGBA(r, g, b, a) (((u32)(r) << 24) | ((u32)(g) << 16) | ((u32)(b) << 8) | ((u32)(a) << 0))

typedef struct
{
   int width;
   int height;
   GX2TVRenderMode mode;
} wiiu_render_mode_t;

struct gx2_overlay_data
{
   GX2Texture tex;
   sprite_vertex_t v;
   float alpha_mod;
};

typedef struct
{
   struct
   {
      GX2Texture texture;
      int width;
      int height;
      bool enable;
      sprite_vertex_t* v;
   } menu;

#ifdef HAVE_OVERLAY
   struct gx2_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif

   GX2Sampler sampler_nearest[RARCH_WRAP_MAX];
   GX2Sampler sampler_linear[RARCH_WRAP_MAX];
   GX2Texture texture;
   frame_vertex_t* v;
   GX2_vec2* ubo_vp;
   GX2_vec2* ubo_tex;
   GX2_mat4x4* ubo_mvp;
   void* input_ring_buffer;
   u32 input_ring_buffer_size;
   void* output_ring_buffer;
   u32 output_ring_buffer_size;

   int width;
   int height;

   float* menu_shader_vbo;
   menu_shader_uniform_t* menu_shader_ubo;

   struct
   {
      sprite_vertex_t* v;
      int size;
      int current;
   } vertex_cache;
   struct
   {
      tex_shader_vertex_t* v;
      int size;
      int current;
   } vertex_cache_tex;

   void* drc_scan_buffer;
   void* tv_scan_buffer;
   GX2ColorBuffer color_buffer;
   GX2ContextState* ctx_state;
   void* cmd_buffer;
   struct video_shader* shader_preset;
   struct
   {
      GFDFile* gfd;
      float* vs_ubos[2];
      float* ps_ubos[2];
      GX2Texture texture;
      GX2ColorBuffer color_buffer;
      bool mem1;
   }pass[GFX_MAX_SHADERS];
   GX2Texture luts[GFX_MAX_TEXTURES];

   wiiu_render_mode_t render_mode;
   video_viewport_t vp;
   int frames;
   OSTime last_vsync;
   bool vsync;
   bool rgb32;
   bool smooth;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool render_msg_enabled;
} wiiu_video_t;
