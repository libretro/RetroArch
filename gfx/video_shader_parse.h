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
   SHDR_FLAG_DISABLED  = (1 << 2)
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
   FBO_SCALE_FLAG_FP_FBO   = (1 << 0),
   FBO_SCALE_FLAG_SRGB_FBO = (1 << 1),
   FBO_SCALE_FLAG_VALID    = (1 << 2)
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
};

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders
 * from the #pragma parameter lines in the shader for each pass.
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

bool video_shader_is_supported(enum rarch_shader_type type);

bool video_shader_any_supported(void);

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
      settings_t *settings,
      enum rarch_shader_type type,
      struct video_shader *menu_shader,
      const char *preset_path,
      const char *temp_dir,
      bool prepend,
      bool message);

bool video_shader_apply_shader(
      settings_t *settings,
      enum rarch_shader_type type,
      const char *preset_path, bool message);

const char *video_shader_get_preset_extension(enum rarch_shader_type type);

void video_shader_toggle(settings_t *settings);

RETRO_END_DECLS

#endif
