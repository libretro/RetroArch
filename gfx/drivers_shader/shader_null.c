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

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <boolean.h>
#include <gfx/math/matrix_4x4.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../video_driver.h"

typedef struct null_shader_data
{
   void *empty;
} null_shader_data_t;

static void shader_null_deinit(void *data)
{
   null_shader_data_t *null_shader = (null_shader_data_t*)data;
   if (!null_shader)
      return;

   free(null_shader);
}

static void *shader_null_init(void *data, const char *path)
{
   null_shader_data_t *null_shader = (null_shader_data_t*)
      calloc(1, sizeof(*null_shader));

   if (!null_shader)
      return NULL;

   return null_shader;
}

static void shader_null_set_uniform_parameter(
      void *data,
      struct uniform_info *param,
      void *uniform_data)
{
}

static unsigned shader_null_get_prev_textures(void *data)
{
   return 0;
}

static bool shader_null_compile_program(
      void *data,
      unsigned idx,
      void *program_data,
      struct shader_program_info *program_info)
{
   return true;
}

const shader_backend_t shader_null_backend = {
   shader_null_init,
   shader_null_deinit,
   NULL,
   shader_null_set_uniform_parameter,
   shader_null_compile_program,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   shader_null_get_prev_textures,
   NULL,
   NULL,
   NULL,

   RARCH_SHADER_NONE,
   "nullshader"
};
