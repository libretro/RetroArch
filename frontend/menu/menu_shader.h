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

#ifndef _MENU_SHADER_MANAGER_H
#define _MENU_SHADER_MANAGER_H

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#define HAVE_SHADER_MANAGER
#endif

#include "../../gfx/shader/shader_parse.h"

void menu_shader_manager_init(void *data);

void menu_shader_manager_set_preset(struct gfx_shader *shader,
      unsigned type, const char *cgp_path);

int menu_shader_manager_setting_toggle(
      unsigned id, const char *label, unsigned action);

void menu_shader_manager_save_preset(
      const char *basename, bool apply);

unsigned menu_shader_manager_get_type(
      const struct gfx_shader *shader);

void menu_shader_manager_get_str(struct gfx_shader *shader,
      char *type_str, size_t type_str_size, const char *menu_label,
      const char *label, unsigned type);

int handle_shader_pass_setting(struct gfx_shader *shader, unsigned action);

void menu_shader_manager_apply_changes(void);

#endif
