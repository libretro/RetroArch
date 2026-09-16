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

#ifndef __VIDEO_SHADER_PARSE_H
#define __VIDEO_SHADER_PARSE_H

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>

#include "../configuration.h"

#ifndef GFX_MAX_SHADERS
#define GFX_MAX_SHADERS 64
#endif

#ifndef GFX_MAX_TEXTURES
#define GFX_MAX_TEXTURES 64
#endif

#ifndef GFX_MAX_PARAMETERS
#define GFX_MAX_PARAMETERS 1024
#endif

#ifndef GFX_MAX_FRAME_HISTORY
#define GFX_MAX_FRAME_HISTORY 128
#endif

#define RARCH_WILDCARD_DELIMITER "$"

/**
 * video_shader_parse_type:
 * @path              : Shader path.
 *
 * Parses type of shader.
 *
 * Returns: value of shader type if it could be determined,
 * otherwise RARCH_SHADER_NONE.
 **/
#define video_shader_parse_type(path) video_shader_get_type_from_ext(path_get_extension((path)), NULL)

RETRO_BEGIN_DECLS

enum rarch_shader_type
{
   RARCH_SHADER_NONE = 0,
   RARCH_SHADER_CG,
   RARCH_SHADER_HLSL,
   RARCH_SHADER_GLSL,
   RARCH_SHADER_SLANG,
   RARCH_SHADER_METAL
};

enum gfx_scale_type
{
   RARCH_SCALE_INPUT = 0,
   RARCH_SCALE_ABSOLUTE,
   RARCH_SCALE_VIEWPORT
};

enum
{
   RARCH_FILTER_UNSPEC = 0,
   RARCH_FILTER_LINEAR,
   RARCH_FILTER_NEAREST,
   RARCH_FILTER_MAX
};

enum video_shader_flags
{
   SHDR_FLAG_MODERN    = (1 << 0), /* Only used for XML shaders. */
   /* Indicative of whether shader was modified -
    * for instance from the menus */
   SHDR_FLAG_MODIFIED  = (1 << 1),
   SHDR_FLAG_DISABLED  = (1 << 2),
   SHDR_FLAG_TEMPORARY = (1 << 3)
};

enum gfx_wrap_type
{
   RARCH_WRAP_BORDER = 0, /* Kinda deprecated, but keep as default.
                             Will be translated to EDGE in GLES. */
   RARCH_WRAP_DEFAULT = RARCH_WRAP_BORDER,
   RARCH_WRAP_EDGE,
   RARCH_WRAP_REPEAT,
   RARCH_WRAP_MIRRORED_REPEAT,
   RARCH_WRAP_MAX
};

enum gfx_fbo_scale_flags
{
   FBO_SCALE_FLAG_FP_FBO    = (1 << 0),
   FBO_SCALE_FLAG_SRGB_FBO  = (1 << 1),
   FBO_SCALE_FLAG_VALID     = (1 << 2),
   FBO_SCALE_FLAG_RGB10_FBO = (1 << 3)
};

struct gfx_fbo_scale
{
   unsigned abs_x;
   unsigned abs_y;
   float scale_x;
   float scale_y;
   enum gfx_scale_type type_x;
   enum gfx_scale_type type_y;
   uint8_t flags;
};

struct video_shader_parameter
{
   int pass;
   float current;
   float minimum;
   float initial;
   float maximum;
   float step;
   char id[64];
   char desc[64];
};

struct rarch_dir_shader_list
{
   struct string_list *shader_list;
   char *directory;
   char *failed_apply_loaded_path;
   size_t selection;
   bool shader_loaded;
   bool remember_last_preset_dir;
};

struct video_shader_pass
{
   struct gfx_fbo_scale fbo; /* unsigned alignment */
   unsigned filter;
   unsigned frame_count_mod;
   enum gfx_wrap_type wrap;
   struct
   {
      struct
      {
         char *vertex; /* Dynamically allocated. Must be free'd. */
         char *fragment; /* Dynamically allocated. Must be free'd. */
      } string;
      char path[NAME_MAX_LENGTH*2];
   } source;
   char alias[64];
   bool mipmap;
   bool feedback;
};

struct video_shader_lut
{
   unsigned filter;
   enum gfx_wrap_type wrap;
   char id[64];
   char path[NAME_MAX_LENGTH*2];
   bool mipmap;
};

/* This is pretty big, shouldn't be put on the stack.
 * Avoid lots of allocation for convenience. */
struct video_shader
{
   struct video_shader_parameter parameters[GFX_MAX_PARAMETERS]; /* int alignment */
   /* If < 0, no feedback pass is used. Otherwise,
    * the FBO after pass #N is passed a texture to next frame. */
   int feedback_pass;
   int history_size;

   struct video_shader_pass pass[GFX_MAX_SHADERS]; /* unsigned alignment */
   struct video_shader_lut lut[GFX_MAX_TEXTURES];  /* unsigned alignment */
   unsigned passes;
   unsigned luts;
   unsigned num_parameters;
   unsigned variables;

   uint8_t flags;

   char prefix[64];

   /* Path to the root preset */
   char path[PATH_MAX_LENGTH];

   /* Path to the original preset loaded, if this is a preset
    * with the #reference directive, then this will be different
    * than the path */
   char loaded_preset_path[PATH_MAX_LENGTH];

   /* Identifies the pass sources 'parameters' was resolved from: one
    * entry per pass that has a source, in pass order. Appended rather
    * than placed by alignment, so that it leaves every offset ahead of
    * it where it was. */
   int64_t  param_src_mtime[GFX_MAX_SHADERS];
   int64_t  param_src_size[GFX_MAX_SHADERS];
   uint32_t param_src_hash[GFX_MAX_SHADERS];
   unsigned param_src_count;
};

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders
 * from the #pragma parameter lines in the shader for each pass.
 *
 * The sources are walked when the set of pass sources, or any of the
 * files behind it, differs from the one the parameters currently held
 * came from. Otherwise those parameters stand, reset to their initial
 * values as a walk would leave them.
 **/
void video_shader_resolve_parameters(struct video_shader *shader);

/**
 * video_shader_load_current_parameter_values:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Reads the current value for all parameters from config file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_load_current_parameter_values(config_file_t *conf, struct video_shader *shader);

/**
 * video_shader_load_preset_into_shader:
 * @path              : Path to preset file, could be a Simple Preset (including a #reference) or Full Preset
 * @shader            : Shader
 *
 * Loads preset file to a shader including passes, textures and parameters
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
/* Struct copy of a driver's loaded shader for menu use: everything
 * but the driver-owned pass source strings, which are cleared in
 * the copy. Replaces a full re-parse of the preset chain when the
 * driver has already done it. */
void video_shader_copy_for_menu(struct video_shader *dst,
      const struct video_shader *src);

bool video_shader_load_preset_into_shader(const char *path, struct video_shader *shader);

/**
 * video_shader_write_preset:
 * @path              : File to write to
 * @shader            : Shader to write
 * @reference         : Whether a simple preset should be written with the #reference to another preset in it
 *
 * Writes a preset to disk. Can be written as a simple preset (With the #reference directive in it) or a full preset.
 **/
bool video_shader_write_preset(const char *path,
      const struct video_shader *shader,
      bool reference);

enum rarch_shader_type video_shader_get_type_from_ext(const char *ext, bool *is_preset);

enum display_flags video_shader_type_to_flag(enum rarch_shader_type type);

bool video_shader_check_for_changes(void);

const char *video_shader_type_to_str(enum rarch_shader_type type);

void video_shader_dir_free_shader(
      struct rarch_dir_shader_list *dir_list,
      bool shader_remember_last_dir);

/**
 * video_shader_dir_check_shader:
 * @pressed_next         : Was next shader key pressed?
 * @pressed_prev         : Was previous shader key pressed?
 *
 * Checks if any one of the shader keys has been pressed for this frame:
 * a) Next shader index.
 * b) Previous shader index.
 *
 * Will also immediately apply the shader.
 **/
void video_shader_dir_check_shader(
      void *menu_driver_data_,
      settings_t *settings,
      struct rarch_dir_shader_list *dir_list,
      bool pressed_next,
      bool pressed_prev);

bool video_shader_combine_preset_and_apply(
      enum rarch_shader_type type,
      struct video_shader *menu_shader,
      const char *preset_path,
      const char *temp_dir,
      bool prepend,
      bool message);

/**
 * video_shader_get_display_name:
 * @preset_path          : Path to a shader preset
 * @shader_dir           : Video shaders directory
 *
 * Returns: path of @preset_path relative to @shader_dir if it lies
 * inside it, otherwise its file name, or NULL if @preset_path is empty.
 **/
const char *video_shader_get_display_name(const char *preset_path,
      const char *shader_dir);

bool video_shader_apply_shader(
      settings_t *settings,
      enum rarch_shader_type type,
      const char *preset_path, bool message);

const char *video_shader_get_preset_extension(enum rarch_shader_type type);

void video_shader_toggle(settings_t *settings, bool write);

/**
 * video_shader_source_read:
 * @ident  : what names the source - a path, as presets carry them
 * @buf    : receives the bytes, NUL terminated, for the caller to free
 * @len    : receives their length, not counting the terminator
 *
 * Hands a shader driver the bytes it is to compile. The drivers under
 * gfx/drivers_shader ask for a source by name and are given it; where
 * those bytes live, and how the name resolves, is decided here and not
 * by them. The caller owns what comes back, as it did when it read the
 * file itself.
 *
 * Returns: true if the source was found and read.
 **/
bool video_shader_source_read(const char *ident, char **buf, int64_t *len);

/**
 * video_shader_source_resolve:
 * @parent : what named the source doing the referring, or NULL
 * @name   : the reference, as it was written in the source
 * @s      : receives what to ask for with video_shader_source_read()
 * @len    : size of @s
 *
 * Turns a reference inside one source into a name for another. A
 * shader driver hands back what it read out of an #include line and
 * gets a name it can ask for; how that resolves - relative to the
 * referring file, or otherwise - is decided here.
 *
 * Returns: true when the reference resolved.
 **/
bool video_shader_source_resolve(const char *parent, const char *name,
      char *s, size_t len);

/**
 * video_shader_source_ident_name:
 * @ident : a name video_shader_source_read() would take
 *
 * The short name of a source, for the #line directives a preprocessor
 * writes into what it hands the compiler. Points into @ident.
 **/
const char *video_shader_source_ident_name(const char *ident);

/**
 * video_shader_source_ident_is_slang:
 * @ident : a name video_shader_source_read() would take
 *
 * Whether a source is a slang one, which a preprocessor checks the
 * #version line of.
 **/
bool video_shader_source_ident_is_slang(const char *ident);

RETRO_END_DECLS

#endif
