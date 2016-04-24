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

#ifndef __VIDEO_SHADER_PARSE_H
#define __VIDEO_SHADER_PARSE_H

#include <boolean.h>
#include <file/config_file.h>
#include "video_state_tracker.h"
#include <retro_miscellaneous.h>

#ifdef __cplusplus
extern "C" {
#endif

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

enum rarch_shader_type
{
   RARCH_SHADER_NONE = 0,
   RARCH_SHADER_CG,
   RARCH_SHADER_HLSL,
   RARCH_SHADER_GLSL,
   RARCH_SHADER_SLANG
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
   RARCH_FILTER_NEAREST
};

enum gfx_wrap_type
{
   RARCH_WRAP_BORDER = 0, /* Kinda deprecated, but keep as default. 
                             Will be translated to EDGE in GLES. */
   RARCH_WRAP_DEFAULT = RARCH_WRAP_BORDER,
   RARCH_WRAP_EDGE,
   RARCH_WRAP_REPEAT,
   RARCH_WRAP_MIRRORED_REPEAT
};

struct gfx_fbo_scale
{
   enum gfx_scale_type type_x;
   enum gfx_scale_type type_y;
   float scale_x;
   float scale_y;
   unsigned abs_x;
   unsigned abs_y;
   bool fp_fbo;
   bool srgb_fbo;
   bool valid;
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
   unsigned filter;
   enum gfx_wrap_type wrap;
   unsigned frame_count_mod;
   bool mipmap;
};

struct video_shader_lut
{
   char id[64];
   char path[PATH_MAX_LENGTH];
   unsigned filter;
   enum gfx_wrap_type wrap;
   bool mipmap;
};

/* This is pretty big, shouldn't be put on the stack.
 * Avoid lots of allocation for convenience. */
struct video_shader
{
   enum rarch_shader_type type;

   bool modern; /* Only used for XML shaders. */
   char prefix[64];

   unsigned passes;
   struct video_shader_pass pass[GFX_MAX_SHADERS];

   unsigned luts;
   struct video_shader_lut lut[GFX_MAX_TEXTURES];

   struct video_shader_parameter parameters[GFX_MAX_PARAMETERS];
   unsigned num_parameters;

   unsigned variables;
   struct state_tracker_uniform_info variable[GFX_MAX_VARIABLES];
   char script_path[PATH_MAX_LENGTH];
   char *script; /* Dynamically allocated. Must be free'd. Only used by XML. */
   char script_class[512];

   /* If < 0, no feedback pass is used. Otherwise,
    * the FBO after pass #N is passed a texture to next frame. */
   int feedback_pass;
};

/** 
 * video_shader_read_conf_cgp:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Loads preset file and all associated state (passes,
 * textures, imports, etc). 
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_read_conf_cgp(config_file_t *conf,
      struct video_shader *shader);

/** 
 * video_shader_write_conf_cgp:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Saves preset and all associated state (passes,
 * textures, imports, etc) to disk. 
 **/
void video_shader_write_conf_cgp(config_file_t *conf,
      struct video_shader *shader);

/**
 * video_shader_resolve_relative:
 * @shader            : Shader pass handle.
 * @ref_path          : Relative shader path.
 *
 * Resolves relative shader path (@ref_path) into absolute
 * shader paths.
 **/
void video_shader_resolve_relative(struct video_shader *shader,
      const char *ref_path);

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
 * @fallback          : Fallback shader type in case no
 *                      type could be found.
 *
 * Parses type of shader.
 *
 * Returns: value of shader type on success, otherwise will return
 * user-supplied @fallback value. 
 **/
enum rarch_shader_type video_shader_parse_type(const char *path,
      enum rarch_shader_type fallback);

#ifdef __cplusplus
}
#endif

#endif

