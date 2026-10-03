/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __GFX_DISPLAY_H__
#define __GFX_DISPLAY_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>
#include <retro_atomic.h>

#include "../retroarch.h"
#include "../gfx/font_driver.h"

#define GFX_SHADOW_ALPHA 1.00f

/* Number of pixels corner-to-corner on a 1080p
 * display:
 * > sqrt((1920 * 1920) + (1080 * 1080))
 * Note: This is a double, so no suffix */
#define DIAGONAL_PIXELS_1080P 2202.90717008229831581901

#define COLOR_TEXT_ALPHA(color, alpha) ((color & 0xFFFFFF00) | (alpha))

#define HEX_R(hex) (((hex) >> 16) & 0xFF) * (1.0f / 255.0f)
#define HEX_G(hex) (((hex) >> 8 ) & 0xFF) * (1.0f / 255.0f)
#define HEX_B(hex) (((hex) >> 0 ) & 0xFF) * (1.0f / 255.0f)

#define COLOR_HEX_TO_FLOAT(hex, alpha) { \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha  \
}

#define gfx_display_set_alpha(color, alpha_value) (color[3] = color[7] = color[11] = color[15] = (alpha_value))

/* Returns true if an animation is still active or
 * when the display framebuffer still is dirty and
 * therefore it still needs to be rendered onscreen.
 *
 * This macro can be used for optimization purposes
 * so that we don't have to render the display graphics per-frame
 * unless a change has happened.
 * */
#define GFX_DISPLAY_GET_UPDATE_PENDING(p_anim, p_disp) (ANIM_IS_ACTIVE(p_anim) || (p_disp->flags & GFX_DISP_FLAG_FB_DIRTY))


RETRO_BEGIN_DECLS

enum gfx_display_flags
{
   GFX_DISP_FLAG_HAS_WINDOWED     = (1 << 0),
   GFX_DISP_FLAG_MSG_FORCE        = (1 << 1),
   GFX_DISP_FLAG_FB_DIRTY         = (1 << 2)
};

enum menu_driver_id_type
{
   MENU_DRIVER_ID_UNKNOWN = 0,
   MENU_DRIVER_ID_RGUI,
   MENU_DRIVER_ID_OZONE,
   MENU_DRIVER_ID_GLUI,
   MENU_DRIVER_ID_XMB
};

enum gfx_display_driver_type
{
   GFX_VIDEO_DRIVER_GENERIC = 0,
   GFX_VIDEO_DRIVER_OPENGL,
   GFX_VIDEO_DRIVER_OPENGL1,
   GFX_VIDEO_DRIVER_OPENGL_CORE,
   GFX_VIDEO_DRIVER_VULKAN,
   GFX_VIDEO_DRIVER_METAL,
   GFX_VIDEO_DRIVER_DIRECT3D8,
   GFX_VIDEO_DRIVER_DIRECT3D9_CG,
   GFX_VIDEO_DRIVER_DIRECT3D9_HLSL,
   GFX_VIDEO_DRIVER_DIRECT3D10,
   GFX_VIDEO_DRIVER_DIRECT3D11,
   GFX_VIDEO_DRIVER_DIRECT3D12,
   GFX_VIDEO_DRIVER_GXM,
   GFX_VIDEO_DRIVER_CTR,
   GFX_VIDEO_DRIVER_WIIU,
   GFX_VIDEO_DRIVER_GDI,
   GFX_VIDEO_DRIVER_SWITCH,
   GFX_VIDEO_DRIVER_RSX,
   GFX_VIDEO_DRIVER_SDL2,
   GFX_VIDEO_DRIVER_SDL3
};

typedef struct gfx_display_ctx_draw gfx_display_ctx_draw_t;
struct gfx_display_mesh;

typedef struct gfx_display gfx_display_t;

typedef struct gfx_display_ctx_driver
{
   /* Draw graphics to the screen. @video_dims carries both axes of
    * the output size in one word, VIDEO_SCALE_PACK's layout. */
   void (*draw)(gfx_display_ctx_draw_t *draw,
         void *data, unsigned video_dims);
   /* Draw one of the menu pipeline shaders. */
   void (*draw_pipeline)(gfx_display_ctx_draw_t *draw,
         gfx_display_t *p_disp,
         void *data, unsigned video_dims);
   /* Start blending operation. */
   void (*blend_begin)(void *data);
   /* Finish blending operation. */
   void (*blend_end)(void *data);
   /* Get the default Model-View-Projection matrix */
   void *(*get_default_mvp)(void *data);
   /* Get the default vertices matrix */
   const float *(*get_default_vertices)(void);
   /* Get the default texture coordinates matrix */
   const float *(*get_default_tex_coords)(void);
   const struct font_renderer  *font_backend;
   enum gfx_display_driver_type type;
   const char *ident;
   bool handles_transform;
   /* Whether a draw may carry more geometry than one quad's four
    * vertices. A driver that walks coords->vertices can take a strip
    * of them in one call; one that reads a fixed four - because it
    * ends in a blit rather than a rasteriser - must be handed a quad
    * at a time. */
   bool handles_vertex_strip;
   /* Enables and disables scissoring */
   /* @video_dims and @dims: the output size and the rect's size,
    * each with both axes in one word, VIDEO_SCALE_PACK's layout. */
   void (*scissor_begin)(void *data, unsigned video_dims,
         int x, int y, unsigned dims);
   void (*scissor_end)(void *data, unsigned video_dims);
   /* Optional. Draws a mesh with a plain program from a buffer the
    * driver keeps, returning false to have it transformed and streamed
    * through draw() instead. @mvp (never NULL) takes the mesh to the
    * display's 0..1 space, as draw()'s coordinates are; @texture is
    * never 0; @tint is RGBA. */
   bool (*mesh_draw)(void *data, unsigned video_dims,
         const struct gfx_display_mesh *mesh, const float *mvp,
         uintptr_t texture, const float *tint);
} gfx_display_ctx_driver_t;

struct gfx_display_ctx_draw
{
   float *color;
   const float *vertex;
   const float *tex_coord;
   const void *backend_data;
   struct video_coords *coords;
   void *matrix_data;
   uintptr_t texture;
   size_t vertex_count;
   size_t backend_data_size;
   /* Both axes in one word, VIDEO_SCALE_PACK's layout. */
   unsigned dims;
   /* The quad's origin, one signed pair in VIDEO_POS_PACK's layout.
    * It is bottom-up: gfx_display_draw_quad and every caller that
    * builds its own descriptor pre-flip y, and the drivers flip it
    * back. Whole pixels, because that is what a display has - a
    * position that came off a scale factor or a tween rounds on its
    * way in here, once, rather than being truncated differently by
    * each driver on its way out. */
   unsigned pos;
   unsigned pipeline_id;
   float rotation;
   float scale_factor;
};

typedef struct gfx_display_ctx_coord_draw
{
   const float *ptr;
} gfx_display_ctx_coord_draw_t;

typedef struct gfx_display_ctx_datetime
{
   unsigned time_mode;
   unsigned date_separator;
} gfx_display_ctx_datetime_t;

typedef struct gfx_display_ctx_powerstate
{
   unsigned percent;
   bool battery_enabled;
   bool charging;
} gfx_display_ctx_powerstate_t;

/* Why a gathered batch of quads had to go out. */
enum gfx_display_flush_reason
{
   GFX_DISPLAY_FLUSH_TEXT = 0, /* text drawn */
   GFX_DISPLAY_FLUSH_TEXTURE,  /* quad with another texture or frame */
   GFX_DISPLAY_FLUSH_BLEND,    /* blend group begun or ended */
   GFX_DISPLAY_FLUSH_SCISSOR,  /* scissor begun or ended */
   GFX_DISPLAY_FLUSH_DRAW,     /* a draw that does not gather */
   GFX_DISPLAY_FLUSH_CAPACITY, /* batch full */
   GFX_DISPLAY_FLUSH_EXPLICIT, /* gfx_display_flush_batch() from outside */
   GFX_DISPLAY_FLUSH_LAST
};

enum gfx_display_stat
{
   GFX_DISPLAY_STAT_QUADS = 0,   /* quads gathered */
   GFX_DISPLAY_STAT_BATCHES,     /* strips sent out */
   GFX_DISPLAY_STAT_BATCH_MAX,   /* quads in the largest strip */
   GFX_DISPLAY_STAT_TEXT_CALLS,  /* strings handed to the font driver */
   GFX_DISPLAY_STAT_TEXT_BYTES,  /* bytes of text in them */
   GFX_DISPLAY_STAT_FONT_DRAWS,  /* draws the font renderers issued */
   GFX_DISPLAY_STAT_FLUSH,       /* one per enum gfx_display_flush_reason */
   GFX_DISPLAY_STAT_LAST = GFX_DISPLAY_STAT_FLUSH + GFX_DISPLAY_FLUSH_LAST
};

typedef struct gfx_display_stats
{
   unsigned v[GFX_DISPLAY_STAT_LAST];
} gfx_display_stats_t;

struct gfx_display
{
   gfx_display_ctx_driver_t *dispctx;

   /* Pitch of the display framebuffer, and both its axes in one word
    * in VIDEO_SCALE_PACK's layout */
   size_t   framebuf_pitch;
   unsigned framebuf_dims;

   /* Height of the display header */
   unsigned header_height;

   enum menu_driver_id_type menu_driver_id;

   /* Quads waiting to go out as one strip. A quad drawn while the
    * batch holds quads of the same texture joins it; anything else
    * sends what is held first, so what is drawn stays in the order it
    * was asked for. Allocated when the first quad is gathered. */
   /* One allocation, carved into the three the coords want: they are
    * filled together and read together, so they are kept together. */
   float    *batch_mem;
   float    *batch_vertex;
   float    *batch_tex;
   float    *batch_color;
   unsigned  batch_quads;
   uintptr_t batch_texture;
   /* The first quad's own rectangle. A batch that never grew past it
    * goes out the way it would have without gathering: a driver that
    * ends in a blit rounds a rectangle and a strip differently, and
    * one quad is not worth a difference. */
   int       batch_first_x;
   int       batch_first_y;
   unsigned  batch_first_dims;
   void     *batch_userdata;
   unsigned  batch_video_dims;
   /* Whether a caller has blending on right now. A quad drawn on its
    * own turns blending on and off around itself; one gathered while
    * a caller has it on must leave it on, or the caller's group ends
    * with the batch instead of with the group. Mirrors the driver
    * rather than counting, so a path that returns between a begin and
    * its end leaves this no worse than the driver itself. */
   bool      blend_on;

   uint8_t flags;

   /* The menu effects' clock, in the step every driver used to keep
    * for itself: 0.01 an effect drawn, wrapped at 65536. Read by the
    * driver while it draws an effect, advanced once it has. */
   float     effect_time;
   /* The mesh an effect is being drawn over, for the driver's pipeline
    * code to draw from; NULL outside that draw. */
   const struct gfx_display_mesh *effect_mesh;

   /* What the batch did during the menu frame being drawn, counted
    * where it happens on the drawing thread and published as a whole
    * by gfx_display_stats_latch() once the frame is over. The
    * statistics overlay reads the published copy from the main
    * thread, so that copy is atomic and the live one is not. */
   gfx_display_stats_t stats;
   retro_atomic_int_t  stats_pub[GFX_DISPLAY_STAT_LAST];
};

void gfx_display_free(void);

void gfx_display_init(void);

void gfx_display_draw_cursor(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      bool cursor_visible,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y);

/* @dims: the area the text is placed in, both axes in one word,
 * VIDEO_SCALE_PACK's layout. */
void gfx_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, unsigned dims,
      uint32_t color, enum text_alignment text_align,
      float scale_factor, bool shadows_enable, float shadow_offset,
      bool draw_outside);

/* As gfx_display_draw_text, but drives glyph colour at full float precision
 * (color_rgba = 4 floats R,G,B,A in 0..1) for deep-colour framebuffers.
 * Backends that do not implement the high-precision path fall back to the
 * 8-bit 'color', so supply an equivalent packed value there. */
void gfx_display_draw_text_hp(
      const font_data_t *font, const char *text,
      float x, float y, unsigned dims,
      uint32_t color, const float *color_rgba,
      enum text_alignment text_align,
      float scale_factor, bool shadows_enable, float shadow_offset,
      bool draw_outside);

void gfx_display_scissor_begin(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      int x, int y, unsigned dims);

bool gfx_display_init_first_driver(gfx_display_t *p_disp,
      bool video_is_threaded);

gfx_display_t *disp_get_ptr(void);

void gfx_display_draw_keyboard(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      uintptr_t hover_texture,
      const font_data_t *font,
      char *grid[], unsigned id,
      unsigned text_color);

/* Note: coords must outlive the call — its address is stored in
 * draw->coords and read by the caller's subsequent dispctx->draw().
 * Callers should declare coords as a stack local alongside draw. */
void gfx_display_draw_bg(
      gfx_display_t *p_disp,
      gfx_display_ctx_draw_t *draw,
      struct video_coords *coords,
      void *userdata,
      bool add_opacity, float opacity_override);

/* Sends any quads gathered by gfx_display_draw_quad() that have not
 * gone out yet. Anything that draws without going through this file -
 * text, above all - calls this first, or it lands underneath quads
 * that were asked for before it. */
/* Meshes: geometry a menu driver builds once and draws every frame,
 * with a program - plain, or one of the menu effects - and a 4x4
 * transform, so it may be 3D. gfx_display owns them. Drivers that draw
 * meshes themselves keep them on the GPU; on the others what a plain
 * program draws is transformed here and streamed through draw(). There
 * is no depth buffer: meshes draw in the order they are asked for. */
enum gfx_display_mesh_program
{
   GFX_MESH_PROGRAM_TEXTURED = 0,  /* texture times vertex colour */
   GFX_MESH_PROGRAM_COLORED,       /* vertex colour */
   GFX_MESH_PROGRAM_BLEND,         /* the driver's stock blend */
   GFX_MESH_PROGRAM_RIBBON,        /* the effects: the mesh animated */
   GFX_MESH_PROGRAM_RIBBON_SIMPLE, /* by time in the vertex stage */
   GFX_MESH_PROGRAM_SNOW_SIMPLE,   /* the effects: shaded per pixel */
   GFX_MESH_PROGRAM_SNOW,          /* over the full-screen quad */
   GFX_MESH_PROGRAM_BOKEH,
   GFX_MESH_PROGRAM_SNOWFLAKE,
   GFX_MESH_PROGRAM_LAST
};

enum gfx_display_mesh_topology
{
   GFX_MESH_TRIANGLES = 0,
   GFX_MESH_TRIANGLE_STRIP
};

enum gfx_display_mesh_flags
{
   /* Keep the positions as plain x,y pairs too, for the drivers whose
    * effect pipelines read them that way (see
    * gfx_display_effect_coords) */
   GFX_MESH_FLAG_POSITIONS = (1 << 0)
};

/* 20 bytes: position, texture coordinates in 1/65535ths, colour */
typedef struct gfx_display_mesh_vertex
{
   float    x, y, z;
   uint16_t u, v;
   uint8_t  rgba[4];
} gfx_display_mesh_vertex_t;

typedef struct gfx_display_mesh_desc
{
   const gfx_display_mesh_vertex_t *vertices;
   const uint16_t *indices;        /* NULL: drawn in vertex order */
   unsigned vertex_count;
   unsigned index_count;
   enum gfx_display_mesh_topology topology;
   unsigned flags;                 /* enum gfx_display_mesh_flags */
} gfx_display_mesh_desc_t;

/* Read by the drivers that draw meshes themselves. @id names the mesh
 * for as long as the process runs and is never given to another, so a
 * driver may keep a buffer for it and let the buffer age out once the
 * mesh stops being drawn. */
struct gfx_display_mesh
{
   gfx_display_mesh_vertex_t *vertices;
   uint16_t *indices;
   unsigned vertex_count;
   unsigned index_count;
   enum gfx_display_mesh_topology topology;
   uint32_t id;
   /* x,y of each vertex, for GFX_MESH_FLAG_POSITIONS; else NULL */
   float *positions;
};
typedef struct gfx_display_mesh gfx_display_mesh_t;

typedef struct gfx_display_mesh_draw
{
   /* Column-major, model to clip space; NULL leaves the mesh in clip
    * space as it is */
   const float *mvp;
   /* Four corners' RGBA, as the quads take it: the tint of the plain
    * programs (corner 0), and what the effects draw with */
   float *color;
   uintptr_t texture;
   enum gfx_display_mesh_program program;
} gfx_display_mesh_draw_t;

gfx_display_mesh_t *gfx_display_mesh_create(const gfx_display_mesh_desc_t *desc);
void gfx_display_mesh_free(gfx_display_mesh_t *mesh);
/* The quad covering the screen, which the per-pixel effects draw over.
 * An effect is drawn over this or over a mesh made with
 * GFX_MESH_FLAG_POSITIONS, and not drawn over any other. */
const gfx_display_mesh_t *gfx_display_mesh_fullscreen(void);
/* While a driver's pipeline code draws an effect: the effect mesh's
 * positions as coordinates, for drivers that stream them; NULL when
 * the mesh keeps none. Drawing thread only. */
struct video_coords *gfx_display_effect_coords(gfx_display_t *p_disp);
void gfx_display_mesh_draw(gfx_display_t *p_disp, void *userdata,
      unsigned video_dims, const gfx_display_mesh_t *mesh,
      const gfx_display_mesh_draw_t *draw);

void gfx_display_flush_batch(gfx_display_t *p_disp);

/* Publishes the counts of the menu frame just drawn and starts the
 * next; called by the drawing thread once the menu frame is over. */
void gfx_display_stats_latch(gfx_display_t *p_disp);

/* The counts of the last published menu frame, safe from any thread. */
void gfx_display_stats_get(gfx_display_stats_t *out);

/* Blending, counted, so that what is gathered knows whether it is
 * inside a group that has already turned blending on. Every caller
 * goes through these rather than the driver's own. */
void gfx_display_blend_begin(gfx_display_ctx_driver_t *dispctx,
      void *userdata);
void gfx_display_blend_end(gfx_display_ctx_driver_t *dispctx,
      void *userdata);

void gfx_display_draw(gfx_display_ctx_driver_t *dispctx,
      gfx_display_ctx_draw_t *draw, void *userdata,
      unsigned video_dims);

void gfx_display_draw_quad(
      gfx_display_t *p_disp,
      void *data,
      unsigned video_dims,
      int x, int y, unsigned dims,
      unsigned ref_dims,
      float *color,
      uintptr_t *texture);

/* @video_dims, @src_dims, @dst_dims and @dims: the output size, the
 * texture's size, the size to draw it at and the area it is placed in,
 * each with both axes in one word, VIDEO_SCALE_PACK's layout. */
void gfx_display_draw_texture_slice(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_dims,
      int x, int y, unsigned src_dims,
      unsigned dst_dims,
      unsigned dims,
      float *color, unsigned offset, float scale_factor, uintptr_t texture,
      math_matrix_4x4 *mymat);

void gfx_display_rotate_z(gfx_display_t *p_disp,
      math_matrix_4x4 *matrix, float cosine, float sine, void *data);

font_data_t *gfx_display_font_file(gfx_display_t *p_disp,
      char* fontpath, float font_size, bool is_threaded);

bool gfx_display_reset_textures_list(
      const char *texture_path,
      const char *iconpath,
      uintptr_t *item,
      enum texture_filter_type filter_type,
      unsigned *dims);

/* Returns the texture filter type used when uploading menu/UI
 * images (icons, thumbnails, wallpapers).  Mip-mapped filtering
 * keeps images smooth when drawn below their native size at the
 * cost of extra video memory; plain linear filtering is cheaper
 * but aliases under heavy minification.  Controlled by the
 * 'menu_texture_mipmapping' setting. */
enum texture_filter_type gfx_display_texture_filter(void);
/* The latched variant, for texture loads issued off the main
 * thread; see gfx_display.c. */
enum texture_filter_type gfx_display_texture_filter_latched(void);

/* Load @ti into the texture the caller owns at @item. Under threaded
 * video, on the main thread, the upload is queued rather than waited
 * for, and @item is written when it completes - a frame later, 0 until
 * then - with the pixels taken from @ti (left NULL, so the caller's
 * image_texture_free() skips them). Otherwise as
 * video_driver_texture_load(). */
bool gfx_display_texture_load(struct texture_image *ti,
      enum texture_filter_type filter, uintptr_t *item);

/* Before a texture slot in [@base, @base + @len) is reset or freed:
 * a load still in flight into it unloads its texture on completion
 * instead of writing the slot. */
void gfx_display_texture_loads_cancel(const void *base, size_t len);

bool gfx_display_reset_icon_texture(
      const char *texture_path,
      uintptr_t *item, enum texture_filter_type filter_type);

/* Platform-adaptive icon/texture loading.
 *
 * On platforms where async task-based image loading is detrimental
 * to performance (e.g. Android with SAF I/O overhead), falls back
 * to synchronous loading identical to the pre-async behavior.
 *
 * All menu drivers and gfx_widgets should call this instead of
 * task_push_icon_load() directly so that adding a new platform
 * to the synchronous path requires changing only one place.
 *
 * |generation| / |generation_ptr| are only used on the async path
 * to guard against stale callbacks; on the synchronous path they
 * are ignored (the load completes before the function returns). */
bool gfx_display_load_icon(
      const char *fullpath,
      bool supports_rgba,
      uintptr_t *target_texture,
      uint64_t generation,
      uint64_t *generation_ptr);

bool gfx_display_reset_textures_list_buffer(
        uintptr_t *item,
        enum texture_filter_type filter_type,
        void* buffer,
        unsigned buffer_len,
        enum image_type_enum image_type,
        unsigned *dims);

/* Returns the OSK key at a given position */
int gfx_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned dims);

/* @dims: both axes in one word, VIDEO_SCALE_PACK's layout. */
float gfx_display_get_dpi_scale(
      gfx_display_t *p_disp,
      void *settings_data,
      unsigned dims,
      bool fullscreen,
      bool is_widget);

void gfx_display_deinit_white_texture(void);

void gfx_display_init_white_texture(void);

bool gfx_display_init_first_driver(gfx_display_t *p_disp,
      bool video_is_threaded);

extern gfx_display_ctx_driver_t gfx_display_ctx_gl;
extern gfx_display_ctx_driver_t gfx_display_ctx_gl3;
extern gfx_display_ctx_driver_t gfx_display_ctx_gl1;
extern gfx_display_ctx_driver_t gfx_display_ctx_vulkan;
extern gfx_display_ctx_driver_t gfx_display_ctx_metal;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d8;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d9_cg;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d9_hlsl;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d10;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d11;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d12;
extern gfx_display_ctx_driver_t gfx_display_ctx_gxm;
extern gfx_display_ctx_driver_t gfx_display_ctx_ctr;
extern gfx_display_ctx_driver_t gfx_display_ctx_wiiu;
extern gfx_display_ctx_driver_t gfx_display_ctx_gdi;
extern gfx_display_ctx_driver_t gfx_display_ctx_switch;
extern gfx_display_ctx_driver_t gfx_display_ctx_rsx;
extern gfx_display_ctx_driver_t gfx_display_ctx_sdl2;
extern gfx_display_ctx_driver_t gfx_display_ctx_sdl3;

RETRO_END_DECLS

#endif
