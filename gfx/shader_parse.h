/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef SHADER_PARSE_H
#define SHADER_PARSE_H

#include "boolean.h"
#include "../conf/config_file.h"
#include "state_tracker.h"
#include "../general.h"

#define GFX_MAX_SHADERS 16
#define GFX_MAX_TEXTURES 8
#define GFX_MAX_VARIABLES 64

enum gfx_scale_type
{
   RARCH_SCALE_INPUT = 0,
   RARCH_SCALE_ABSOLUTE,
   RARCH_SCALE_VIEWPORT
};

enum gfx_filter_type
{
   RARCH_FILTER_UNSPEC = 0,
   RARCH_FILTER_LINEAR,
   RARCH_FILTER_NEAREST,
};

struct gfx_fbo_scale
{
   bool valid;
   enum gfx_scale_type type_x;
   enum gfx_scale_type type_y;
   float scale_x;
   float scale_y;
   unsigned abs_x;
   unsigned abs_y;
   bool fp_fbo;
};

struct gfx_shader_pass
{
   union
   {
      char cg[PATH_MAX];
      // Can allow for more types later.
   } source;

   struct gfx_fbo_scale fbo;
   enum gfx_filter_type filter;
   unsigned frame_count_mod;
};

struct gfx_shader_lut
{
   char id[64];
   char path[PATH_MAX];
   enum gfx_filter_type filter;
};

// This is pretty big, shouldn't be put on the stack.
// Avoid lots of allocation for convenience.
struct gfx_shader
{
   unsigned passes;
   struct gfx_shader_pass pass[GFX_MAX_SHADERS];

   unsigned luts;
   struct gfx_shader_lut lut[GFX_MAX_TEXTURES];

   unsigned variables;
   struct state_tracker_uniform_info variable[GFX_MAX_VARIABLES];
   char script_path[PATH_MAX];
   char script_class[512];
};

bool gfx_shader_read_conf_cgp(config_file_t *conf, struct gfx_shader *shader);
void gfx_shader_write_conf_cgp(config_file_t *conf, const struct gfx_shader *shader);

#endif

