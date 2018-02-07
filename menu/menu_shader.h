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

#ifndef _MENU_SHADER_MANAGER_H
#define _MENU_SHADER_MANAGER_H

#include <retro_common_api.h>

#include "../gfx/video_driver.h"

RETRO_BEGIN_DECLS

struct video_shader *menu_shader_get();

struct video_shader_parameter *menu_shader_manager_get_parameters(unsigned i);

struct video_shader_pass *menu_shader_manager_get_pass(unsigned i);

unsigned menu_shader_manager_get_amount_passes(void);

void menu_shader_manager_free(void);

/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
bool menu_shader_manager_init(void);

/**
 * menu_shader_manager_set_preset:
 * @shader                   : Shader handle.
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 *
 * Sets shader preset.
 **/
bool menu_shader_manager_set_preset(
      void *data, unsigned type, const char *preset_path);

/**
 * menu_shader_manager_save_preset:
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
bool menu_shader_manager_save_preset(
      const char *basename, bool apply, bool fullpath);

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle
 *
 * Gets type of shader.
 *
 * Returns: type of shader.
 **/
unsigned menu_shader_manager_get_type(const void *data);

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(void);

int menu_shader_manager_clear_num_passes(void);

int menu_shader_manager_clear_parameter(unsigned i);

int menu_shader_manager_clear_pass_filter(unsigned i);

void menu_shader_manager_clear_pass_scale(unsigned i);

void menu_shader_manager_clear_pass_path(unsigned i);

void menu_shader_manager_decrement_amount_passes(void);

void menu_shader_manager_increment_amount_passes(void);

RETRO_END_DECLS

#endif
