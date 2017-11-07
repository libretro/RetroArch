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

#ifndef __GL2_RENDER_CHAIN_H
#define __GL2_RENDER_CHAIN_H

#include <retro_common_api.h>
#include <libretro.h>

#include "../video_driver.h"
#include "../video_shader_parse.h"

RETRO_BEGIN_DECLS

void gl2_renderchain_deinit_fbo(void *data);

void context_bind_hw_render(bool enable);

RETRO_END_DECLS

#endif

