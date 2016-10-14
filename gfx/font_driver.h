/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __FONT_DRIVER_H__
#define __FONT_DRIVER_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum font_driver_render_api
{
   FONT_DRIVER_RENDER_DONT_CARE,
   FONT_DRIVER_RENDER_OPENGL_API,
   FONT_DRIVER_RENDER_DIRECT3D_API,
   FONT_DRIVER_RENDER_VITA2D,
   FONT_DRIVER_RENDER_CTR,
   FONT_DRIVER_RENDER_VULKAN_API
};

enum text_alignment
{
   TEXT_ALIGN_LEFT = 0,
   TEXT_ALIGN_RIGHT,
   TEXT_ALIGN_CENTER
};

/* All coordinates and offsets are top-left oriented.
 *
 * This is a texture-atlas approach which allows text to 
 * be drawn in a single draw call.
 *
 * It is up to the code using this interface to actually 
 * generate proper vertex buffers and upload the atlas texture to GPU. */

struct font_glyph
{
   unsigned width;
   unsigned height;

   /* Texel coordinate offset for top-left pixel of this glyph. */
   unsigned atlas_offset_x;
   unsigned atlas_offset_y;

   /* When drawing this glyph, apply an offset to 
    * current X/Y draw coordinate. */
   int draw_offset_x;
   int draw_offset_y;

   /* Advance X/Y draw coordinates after drawing this glyph. */
   int advance_x;
   int advance_y;
};

struct font_atlas
{
   uint8_t *buffer; /* Alpha channel. */
   unsigned width;
   unsigned height;
};

typedef struct font_params
{
   float x;
   float y;
   float scale;
   /* Drop shadow color multiplier. */
   float drop_mod;
   /* Drop shadow offset.
    * If both are 0, no drop shadow will be rendered. */
   int drop_x, drop_y;
   /* Drop shadow alpha */
   float drop_alpha;
   /* ABGR. Use the macros. */
   uint32_t color;
   bool full_screen;
   enum text_alignment text_align;
} font_params_t;

/** Don't free() it. Use font_load() and font_unref() to manage. */
typedef struct font_t font_t;

typedef struct
{
   void *(*init)(void *data, const font_t *font);
   void (*free)(void *data);
   void (*render_msg)(void *data, const char *msg,
         const void *params);
   const char *ident;

   const struct font_glyph *(*get_glyph)(void *data, uint32_t code);
   void (*bind_block)(void *data, void *block);
   void (*flush)(void *data);
   
   int (*get_message_width)(void *data, const char *msg, unsigned msg_len_full, float scale);
} font_renderer_t;

typedef struct
{
   void *(*init)(const char *font_path, float font_size);

   const struct font_atlas *(*get_atlas)(void *data);

   /* Returns NULL if no glyph for this code is found. */
   const struct font_glyph *(*get_glyph)(void *data, uint32_t code);

   void (*free)(void *data);

   const char *(*get_default_font)(void);

   const char *ident;
   
   int (*get_line_height)(void* data);
} font_backend_t;

/* font_path can be NULL for default font. */
int font_renderer_create_default(const void **driver,
      void **handle, const char *font_path, unsigned font_size);
      
/**
 * @brief Loads a font file
 *
 * The returned pointer must be freed using font_unref().
 *
 * @param filename
 * @param size
 * @return pointer to a font_t structure
 * @see font_unref()
 */
const font_t *font_load(const char *filename, float size);

/**
 * @brief Increments the font refcount
 *
 * Use this function if you want to tie the font lifetime to a certain piece of
 * code's lifetime e.g. a menu driver or a font renderer.
 *
 * All font_ref() calls must have a matching font_unref() call otherwise the
 * program leaks.
 *
 * @param font
 * @return the font itself
 */
const font_t *font_ref(const font_t *font);

/**
 * @brief Decrements the font refcount and/or deallocates it
 *
 * @param font
 * @return the font itself or NULL
 */
const font_t *font_unref(const font_t *font);

const char *font_get_filename(const font_t *font);
float font_get_size(const font_t *font);
const struct font_atlas *font_get_atlas(const font_t *font);
const struct font_glyph *font_get_glyph(const font_t *font, uint32_t codepoint);
int font_get_line_height(const font_t *font);
int font_width(const font_t *font, const char *text, unsigned len, float scale);
void font_set_api(enum font_driver_render_api api);


/**
 * @brief Instructs
 * @param font
 * @param queue
 */
void font_bind_block(const font_t *font, void *block);
void font_flush(const font_t *font);

/**
 * @brief Render text on the screen
 * @param font
 * @param video_thread whether the caller is in the video thread
 * @param text
 * @param params
 */
void font_render_full(const font_t *font, const char *text, const font_params_t *params);
void font_render(const font_t *font, const char *text, float x, float y, enum text_alignment align, uint32_t color);



/**
 * @brief Invalidates font caches
 *
 * This function should be called before video contexts or drivers get
 * deinitialized or destroyed.
 */
void font_invalidate_caches(void);

extern font_renderer_t gl_font_renderer;
extern font_renderer_t libdbg_font;
extern font_renderer_t d3d_xbox360_font;
extern font_renderer_t d3d_xdk1_font;
extern font_renderer_t d3d_win32_font;
extern font_renderer_t vita2d_vita_font;
extern font_renderer_t ctr_font;
extern font_renderer_t vulkan_font_renderer;

extern font_backend_t stb_font_backend;
extern font_backend_t freetype_font_backend;
extern font_backend_t coretext_font_backend;
extern font_backend_t bitmap_font_backend;

RETRO_END_DECLS

#endif
