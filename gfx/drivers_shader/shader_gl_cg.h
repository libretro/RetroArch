/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_CG_GL_H
#define __RARCH_CG_GL_H

#include <boolean.h>
#include <retro_common_api.h>

#include "../include/Cg/cg.h"

RETRO_BEGIN_DECLS

#ifdef HAVE_SHADERPIPELINE
/* A menu effect made from its Cg in the Cg backend's context, with the
 * parameters it reads found once; 0 where it does not read one */
typedef struct gl_cg_effect
{
   CGprogram vprg;
   CGprogram fprg;
   CGparameter mvp;
   CGparameter position;
   CGparameter ribbon;
   CGparameter time_v;
   CGparameter time_f;
   CGparameter output_size_v;
   CGparameter output_size_f;
} gl_cg_effect_t;

/* False when @shader_data is no Cg backend's, or @source does not
 * build */
bool gl_cg_effect_compile(void *shader_data, gl_cg_effect_t *fx,
      const char *source);

/* Binds @fx with its uniforms - @ribbon NULL where it reads none, the
 * output size where it reads one - and its positions from the array
 * buffer bound now, two floats at offset 0 every @stride bytes. The
 * caller draws, then calls gl_cg_effect_end(). */
void gl_cg_effect_begin(void *shader_data, const gl_cg_effect_t *fx,
      const float *mvp,
      float time, const float *ribbon, float output_w, float output_h,
      unsigned stride);

void gl_cg_effect_end(const gl_cg_effect_t *fx);

void gl_cg_effect_free(gl_cg_effect_t *fx);
#endif

RETRO_END_DECLS

#endif
