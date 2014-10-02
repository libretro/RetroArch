/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../../boolean.h"
#include <string.h>
#include "../../general.h"
#include "../../compat/strl.h"
#include "../../compat/posix_string.h"
#include "../state_tracker.h"
#include "../../dynamic.h"
#include "../../file.h"
#include "../math/matrix.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_context.h"
#include "shader_common.h"
#include <stdlib.h>

typedef struct shader_backend shader_backend_t;

static void shader_null_deinit(void) { }
static bool shader_null_init(void *data, const char *path) { return true; }

static void shader_null_set_params(void *data, unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const void *info, 
      const void *prev_info, 
      const void *fbo_info, unsigned fbo_info_cnt)
{
}

static bool shader_null_set_mvp(void *data, const math_matrix *mat)
{
   return false;
}

static bool shader_null_set_coords(const void *data)
{
   return false;
}

static void shader_null_use(void *data, unsigned index)
{
}

static unsigned shader_null_num(void)
{
   return 0;
}

static bool shader_null_filter_type(unsigned index, bool *smooth)
{
   return true;
}

static enum gfx_wrap_type shader_null_wrap_type(unsigned index)
{
   return RARCH_WRAP_BORDER;
}

static void shader_null_shader_scale(unsigned index,
      struct gfx_fbo_scale *scale)
{
}

static unsigned shader_null_get_prev_textures(void)
{
   return 0;
}

static bool shader_null_mipmap_input(unsigned index)
{
   return false;
}

static struct gfx_shader *shader_null_get_current_shader(void)
{
   return NULL;
}

void shader_null_set_get_proc_address(gfx_ctx_proc_t (*proc)(const char*))
{
}

void shader_null_set_context_type(bool core_profile,
      unsigned major, unsigned minor)
{
}

const shader_backend_t shader_null_backend = {
   shader_null_init,
   shader_null_deinit,
   shader_null_set_params,
   shader_null_use,
   shader_null_num,
   shader_null_filter_type,
   shader_null_wrap_type,
   shader_null_shader_scale,
   shader_null_set_coords,
   shader_null_set_mvp,
   shader_null_get_prev_textures,
   shader_null_mipmap_input,
   shader_null_get_current_shader,

   RARCH_SHADER_NONE,
};
