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

RETRO_BEGIN_DECLS

#ifndef GFX_MAX_SHADERS
#define GFX_MAX_SHADERS 26
#endif

#ifndef GFX_MAX_TEXTURES
#define GFX_MAX_TEXTURES 8
#endif

#ifndef GFX_MAX_VARIABLES
#define GFX_MAX_VARIABLES 64
#endif

#ifndef GFX_MAX_PARAMETERS
#define GFX_MAX_PARAMETERS 128
#endif

#ifndef GFX_MAX_FRAME_HISTORY
#define GFX_MAX_FRAME_HISTORY 128
#endif

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

struct gfx_fbo_scale
{
   enum gfx_scale_type type_x;
   enum gfx_scale_type type_y;
   float scale_x;
   float scale_y;
   bool fp_fbo;
   bool srgb_fbo;
   bool valid;
   unsigned abs_x;
   unsigned abs_y;
};

struct video_shader_parameter
{
   char id[64];
   char desc[64];
   float current;
   float minimum;
   float initial;
   float maximum;
   float step;
   int pass;
};

struct video_shader_pass
{
   struct
   {
      char path[PATH_MAX_LENGTH];
      struct
      {
         char *vertex; /* Dynamically allocated. Must be free'd. */
         char *fragment; /* Dynamically allocated. Must be free'd. */
      } string;
   } source;

   char alias[64];
   struct gfx_fbo_scale fbo;
   enum gfx_wrap_type wrap;
   bool mipmap;
   unsigned filter;
   unsigned frame_count_mod;
   bool feedback;
};

struct video_shader_lut
{
   char id[64];
   char path[PATH_MAX_LENGTH];
   enum gfx_wrap_type wrap;
   bool mipmap;
   unsigned filter;
};

/* This is pretty big, shouldn't be put on the stack.
 * Avoid lots of allocation for convenience. */
struct video_shader
{
   char prefix[64];
   char path[PATH_MAX_LENGTH];
   bool modern; /* Only used for XML shaders. */

   unsigned passes;
   unsigned luts;
   unsigned num_parameters;
   unsigned variables;
   /* If < 0, no feedback pass is used. Otherwise,
    * the FBO after pass #N is passed a texture to next frame. */
   int feedback_pass;
   int history_size;

   struct video_shader_pass pass[GFX_MAX_SHADERS];

   struct video_shader_lut lut[GFX_MAX_TEXTURES];

   struct video_shader_parameter parameters[GFX_MAX_PARAMETERS];
};

/**
 * video_shader_write_preset:
 * @path              : File to write to
 * @shader            : Shader preset to write
 * @reference         : Whether a reference preset should be written
 *
 * Writes a preset to disk. Can be written as a reference preset.
 * See: video_shader_read_preset
 **/
bool video_shader_write_preset(const char *path,
      const struct video_shader *shader, bool reference);

/**
 * video_shader_read_reference_path:
 * @path              : File to read
 *
 * Returns: the reference path of a preset if it exists,
 * otherwise returns NULL.
 *
 * The returned string needs to be freed.
 */
char *video_shader_read_reference_path(const char *path);

/**
 * video_shader_read_preset:
 * @path              : File to read
 *
 * Reads a preset from disk.
 * If the preset is a reference preset, the referenced preset
 * is loaded instead.
 *
 * Returns: the read preset as a config object.
 *
 * The returned config object needs to be freed.
 **/
config_file_t *video_shader_read_preset(const char *path);

/**
 * video_shader_read_conf_preset:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 * Loads preset file and all associated state (passes,
 * textures, imports, etc).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_read_conf_preset(config_file_t *conf,
      struct video_shader *shader);

/**
 * video_shader_write_conf_preset:
 * @conf              : Preset file to write to.
 * @shader            : Shader passes handle.
 * @preset_path       : Optional path to where the preset will be written.
 *
 * Writes preset and all associated state (passes,
 * textures, imports, etc) into @conf.
 * If @preset_path is not NULL, shader paths are saved
 * relative to it.
 **/
void video_shader_write_conf_preset(config_file_t *conf,
      const struct video_shader *shader, const char *preset_path);

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Reads the current value for all parameters from config file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_current_parameters(config_file_t *conf,
      struct video_shader *shader);

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_parameters(config_file_t *conf,
      struct video_shader *shader);

/**
 * video_shader_parse_type:
 * @path              : Shader path.
 *
 * Parses type of shader.
 *
 * Returns: value of shader type if it could be determined,
 * otherwise RARCH_SHADER_NONE.
 **/
enum rarch_shader_type video_shader_parse_type(const char *path);

enum rarch_shader_type video_shader_get_type_from_ext(const char *ext,
      bool *is_preset);

bool video_shader_is_supported(enum rarch_shader_type type);

bool video_shader_any_supported(void);

bool video_shader_check_for_changes(void);

const char *video_shader_to_str(enum rarch_shader_type type);

const char *video_shader_get_preset_extension(enum rarch_shader_type type);

RETRO_END_DECLS

#endif
