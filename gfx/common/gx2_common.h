#include <wiiu/gx2.h>

#include "wiiu/tex_shader.h"

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

#define COLOR_ABGR(r, g, b, a) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(g) << 8) | ((u32)(r) << 0))
#define COLOR_RGBA(r, g, b, a) (((u32)(a) << 24) | ((u32)(r) << 16) | ((u32)(g) << 8) | ((u32)(b) << 0))
#define COLOR_RGBA(r, g, b, a) (((u32)(r) << 24) | ((u32)(g) << 16) | ((u32)(b) << 8) | ((u32)(a) << 0))

//#define GX2_CAN_ACCESS_DATA_SECTION

typedef struct
{
   int width;
   int height;
   GX2TVRenderMode mode;
} wiiu_render_mode_t;

typedef struct
{
   float x;
   float y;
} position_t;

typedef struct
{
   float u;
   float v;
} tex_coord_t;

struct gx2_overlay_data
{
   GX2Texture tex;
   float tex_coord[8];
   float vertex_coord[8];
   u32 color[4];
   float alpha_mod;
};

typedef struct
{
   tex_shader_t* shader;
   struct
   {
      GX2Texture texture;
      int width;
      int height;
      bool enable;
      position_t* position;
      tex_coord_t* tex_coord;
      u32* color;
   } menu;

#ifdef HAVE_OVERLAY
   struct gx2_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif

   GX2Sampler sampler_nearest;
   GX2Sampler sampler_linear;
   GX2Texture texture;
   position_t* position;
   tex_coord_t* tex_coord;
   u32* color;
   int width;
   int height;

   struct
   {
      position_t* positions;
      tex_coord_t* tex_coords;
      u32* colors;
      int size;
      int current;
   } vertex_cache;

   void* drc_scan_buffer;
   void* tv_scan_buffer;
   GX2ColorBuffer color_buffer;
   GX2ContextState* ctx_state;
   void* cmd_buffer;

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
