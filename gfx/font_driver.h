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

#ifndef __FONT_DRIVER_H__
#define __FONT_DRIVER_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include "../retroarch.h"

#include "video_defines.h"

RETRO_BEGIN_DECLS

typedef struct font_renderer
{
   void *(*init)(void *data, const char *font_path,
         float font_size, bool is_threaded);
   void (*free)(void *data, bool is_threaded);
   void (*render_msg)(void *userdata,
         void *data, const char *msg, size_t msg_len,
         const struct font_params *params);
   const char *ident;

   const struct font_glyph *(*get_glyph)(void *data, uint32_t code);
   void (*bind_block)(void *data, void *block);
   void (*flush)(unsigned width, unsigned height, void *data);

   int (*get_message_width)(void *data, const char *msg, size_t msg_len, float scale);
   bool (*get_line_metrics)(void* data, struct font_line_metrics **metrics);
} font_renderer_t;

/* NOTE: All functions are required to be implemented for font_renderer_driver */

typedef struct font_renderer_driver
{
   /* font_data/font_data_len carry the contents of font_path, already
    * read by font_renderer_create_default(). Renderers do no file I/O
    * of their own: a NULL font_data means no usable file was found,
    * and the renderer should fall back to whatever internal or system
    * source it has (stb's built-in glyphs, the WiiU shared font,
    * fontconfig). On success the renderer takes ownership of
    * font_data and frees it in free(). */
   /* font_data/font_data_len are the bytes of the chosen font, read by
    * font_renderer_create_default(); face_index selects the face
    * within a collection. Renderers do no file I/O and are not given a
    * path: a NULL font_data means nothing usable was found and the
    * renderer should fall back to whatever internal or system source
    * it has. On success a renderer with borrows_font_data set only
    * reads font_data and never frees it; one without takes
    * ownership, as the field below explains. */
   void *(*init)(uint8_t *font_data, size_t font_data_len,
         unsigned face_index,
         float font_size, enum font_atlas_format fmt);

   struct font_atlas *(*get_atlas)(void *data);

   /* Returns NULL if no glyph for this code is found. */
   const struct font_glyph *(*get_glyph)(void *data, uint32_t code);

   void (*free)(void *data);

   /* NULL-terminated list of candidate paths, best first. Returning
    * the list rather than a chosen path keeps path_is_valid() - and so
    * all file I/O - out of the renderers. An empty first entry means
    * "no file needed", for renderers with an internal source. */
   /* Candidate paths for the requested font, best first,
    * NULL-terminated. requested is what the caller asked for, or NULL;
    * a renderer may ignore it or resolve against it - freetype asks
    * fontconfig, which needs the request and the user's locale.
    * face_index is written with the face to use within whichever
    * candidate is taken. An empty entry means "no file needed", for a
    * renderer with an internal source. */
   const char * const *(*get_default_fonts)(const char *requested,
         unsigned *face_index);

   const char *ident;

   void (*get_line_metrics)(void* data, struct font_line_metrics **metrics);

   /* True when the renderer only reads the bytes it is handed and
    * never frees them.  font_renderer_create_default() can then give
    * the same buffer to every font built from one path and free it
    * once the last of them is gone - which is what turns nine reads
    * of a menu face into one.
    *
    * False means the renderer takes the bytes and disposes of them on
    * a schedule of its own, so it gets a private copy.  coretext is
    * the case: it hands the buffer to CGDataProviderCreateWithData
    * and CoreGraphics calls the release callback when it is finished,
    * which is not necessarily when the font is freed. */
   bool borrows_font_data;
} font_renderer_driver_t;

typedef struct font_data
{
   const font_renderer_t *renderer;
   void *renderer_data;
   float size;
   /* How this font was created, so it can be rebuilt in place when the
    * file behind it should change - switching menu language picks a
    * different TTF. Rebuilding keeps this font_data_t at the same
    * address, so every holder of the pointer stays valid. */
   struct font_data *next;          /* list of live fonts */
   void *video_data;
   char *path;                      /* NULL when the renderer chose */
   /* Set for a font whose file depends on the menu language. Kept so
    * the path can be worked out again rather than merely re-read: a
    * language change wants a different face, not the same one. */
   char *lang_pkg_dir;
   char *lang_default_path;
   bool is_threaded;
   /* Line metrics, read from the renderer once when the font is
    * created. Renderers fill these at init and never change them, so
    * callers can use them directly instead of asking again - which
    * some did per frame, from inside ticker animations. Scale them
    * yourself if you are drawing at other than 1.0. */
   struct font_line_metrics metrics;
} font_data_t;

/* This structure holds all objects + metadata
 * corresponding to a particular font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block; /* ptr alignment */
   unsigned glyph_width;
   unsigned wideglyph_width;
   int line_height;
   int line_ascender;
   int line_centre_offset;
   /* The string wideglyph_width is measured from, kept so the derived
    * values above can be recomputed without the driver's help. */
   const char *wideglyph_str;
   /* font_driver generation these were computed at. When the font is
    * rebuilt underneath - switching menu language picks a different
    * face - the generation moves and they are recomputed. */
   uint32_t metrics_generation;
} font_data_impl_t;

void font_driver_bind_block(void *font_data, void *block);

/* font_path can be NULL for default font.
 *
 * @fmt is the glyph coverage precision the caller wants in the atlas.
 * Video drivers producing HDR output ask for FONT_ATLAS_FORMAT_A16;
 * everything else passes FONT_ATLAS_FORMAT_A8. Renderers without
 * 16-bit support ignore it. */

int font_renderer_create_default(
      const font_renderer_driver_t **drv,
      void **handle,
      const char *font_path, unsigned font_size,
      enum font_atlas_format fmt);

void font_driver_render_msg(void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params, void *font_data);

int font_driver_get_message_width(void *font_data, const char *msg, size_t len, float scale);

void font_driver_free(font_data_t *font);

/* Rebuild every live font from its current path, in place. Used when
 * the menu language changes and the fonts must follow, without tearing
 * down the video driver to do it. Fonts whose path is unchanged are
 * left alone. Returns the number rebuilt. */
unsigned font_driver_reload_fonts(void);

/* The font file the current menu language needs, relative to the
 * assets pkg directory, or NULL when it has no special requirement
 * and the caller's own default should be used.
 *
 * One copy of this decision. It was four - ozone, materialui,
 * gfx_widgets and file_path_special each carried the same switch -
 * which is three places to forget when a language is added. */
const char *font_driver_language_font_file(void);

/* Declare that this font's file follows the menu language, and give
 * the two things needed to work it out again: the assets pkg
 * directory the language fonts live in, and the path to use when the
 * language needs no special font. font_driver_reload_fonts() then
 * re-resolves instead of re-reading. */
void font_driver_set_language_font(font_data_t *font,
      const char *pkg_dir, const char *default_path);

/* Returns a monotonic counter incremented on every font free;
 * see font_driver.c for rationale. Used to validate externally
 * cached per-font derived data */
uint32_t font_driver_get_generation(void);

/* Recompute the derived metrics above if the font has been rebuilt
 * since they were last worked out. Cheap when nothing has changed:
 * one integer compare. */
void font_driver_sync_impl(font_data_impl_t *font_data);

void font_flush(
      unsigned video_width,
      unsigned video_height,
      font_data_impl_t *font_data);

font_data_t *font_driver_init_first(
      void *video_data,
      const char *font_path,
      float font_size,
      bool threading_hint,
      bool is_threaded,
      const font_renderer_t *backend);

void font_driver_init_osd(
      void *video_data,
      const video_info_t *video_info,
      bool is_threaded,
      const font_renderer_t *backend);

/* Frees the OSD font only if it was built against @video_data. Use
 * this from any teardown that may run out of order with respect to the
 * next init. */
void font_driver_free_osd_for(void *video_data);

extern font_renderer_driver_t stb_font_renderer;
extern font_renderer_driver_t freetype_font_renderer;
extern font_renderer_driver_t coretext_font_renderer;

RETRO_END_DECLS

#endif
