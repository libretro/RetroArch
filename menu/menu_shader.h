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

#ifndef _MENU_SHADER_MANAGER_H
#define _MENU_SHADER_MANAGER_H

#include "../gfx/video_shader_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
void menu_shader_manager_init(menu_handle_t *menu);

/**
 * menu_shader_manager_set_preset:
 * @shader                   : Shader handle.   
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 *
 * Sets shader preset.
 **/
void menu_shader_manager_set_preset(
      struct video_shader *shader,
      unsigned type, const char *preset_path);

/**
 * menu_shader_manager_save_preset:
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
void menu_shader_manager_save_preset(
      const char *basename, bool apply);

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle     
 *
 * Gets type of shader.
 *
 * Returns: type of shader. 
 **/
unsigned menu_shader_manager_get_type(
      const struct video_shader *shader);

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(void);

void menu_shader_free(menu_handle_t *menu);

#ifdef __cplusplus
}
#endif

#endif
