/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <string.h>

#include "video_shader_driver.h"
#include "../verbosity.h"

static const shader_backend_t *shader_ctx_drivers[] = {
#ifdef HAVE_GLSL
   &gl_glsl_backend,
#endif
#ifdef HAVE_CG
   &gl_cg_backend,
#endif
#ifdef HAVE_HLSL
   &hlsl_backend,
#endif
   &shader_null_backend,
   NULL
};

/**
 * shader_ctx_find_driver:
 * @ident                   : Identifier of shader context driver to find.
 *
 * Finds shader context driver and initializes.
 *
 * Returns: shader context driver if found, otherwise NULL.
 **/
const shader_backend_t *shader_ctx_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; shader_ctx_drivers[i]; i++)
   {
      if (!strcmp(shader_ctx_drivers[i]->ident, ident))
         return shader_ctx_drivers[i];
   }

   return NULL;
}

/**
 * shader_ctx_init_first:
 *
 * Finds first suitable shader context driver and initializes.
 *
 * Returns: shader context driver if found, otherwise NULL.
 **/
const shader_backend_t *shader_ctx_init_first(void)
{
   unsigned i;

   for (i = 0; shader_ctx_drivers[i]; i++)
      return shader_ctx_drivers[i];

   return NULL;
}

struct video_shader *video_shader_driver_get_current_shader(void)
{
   driver_t *driver = driver_get_ptr();
   if (!driver->video_poke)
      return NULL;
   if (!driver->video_data)
      return NULL;
   if (!driver->video_poke->get_current_shader)
      return NULL;
   return driver->video_poke->get_current_shader(driver->video_data);
}

void video_shader_scale(unsigned idx,
      const shader_backend_t *shader,  struct gfx_fbo_scale *scale)
{
   if (!scale || !shader)
      return;

   scale->valid = false;

   if (shader->shader_scale)
      shader->shader_scale(idx, scale);
}
