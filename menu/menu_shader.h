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

#include "../retroarch.h"
#include "../gfx/video_shader_parse.h"

RETRO_BEGIN_DECLS

enum auto_shader_type
{
   SHADER_PRESET_GLOBAL,
   SHADER_PRESET_CORE,
   SHADER_PRESET_PARENT,
   SHADER_PRESET_GAME
};

struct video_shader *menu_shader_get(void);

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
 * @apply                    : Whether to apply the shader or just update shader information
 *
 * Sets shader preset.
 **/
bool menu_shader_manager_set_preset(
      struct video_shader *shader,
      enum rarch_shader_type type, const char *preset_path, bool apply);

/**
 * menu_shader_manager_save_auto_preset:
 * @shader                   : shader to save
 * @type                     : type of shader preset which determines save path
 * @apply                    : immediately set preset after saving
 *
 * Save a shader as an auto-shader to it's appropriate path:
 *    SHADER_PRESET_GLOBAL: <shader dir>/presets/global
 *    SHADER_PRESET_CORE:   <shader dir>/presets/<core name>/<core name>
 *    SHADER_PRESET_PARENT: <shader dir>/presets/<core name>/<parent>
 *    SHADER_PRESET_GAME:   <shader dir>/presets/<core name>/<game name>
 * Needs to be consistent with retroarch_load_shader_preset()
 * Auto-shaders will be saved as a reference if possible
 **/
bool menu_shader_manager_save_auto_preset(const struct video_shader *shader,
      enum auto_shader_type type, bool apply);

/**
 * menu_shader_manager_save_preset:
 * @shader                   : shader to save
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
bool menu_shader_manager_save_preset(const struct video_shader *shader,
      const char *basename, bool apply);

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle
 *
 * Gets type of shader.
 *
 * Returns: type of shader.
 **/
enum rarch_shader_type menu_shader_manager_get_type(
      const struct video_shader *shader);

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(struct video_shader *shader);

int menu_shader_manager_clear_num_passes(struct video_shader *shader);

int menu_shader_manager_clear_parameter(struct video_shader *shader,
      unsigned i);

int menu_shader_manager_clear_pass_filter(struct video_shader *shader,
      unsigned i);

void menu_shader_manager_clear_pass_scale(struct video_shader *shader,
      unsigned i);

void menu_shader_manager_clear_pass_path(struct video_shader *shader,
      unsigned i);

/**
 * menu_shader_manager_remove_auto_preset:
 * @type                     : type of shader preset to delete
 *
 * Deletes an auto-shader.
 **/
bool menu_shader_manager_remove_auto_preset(enum auto_shader_type type);

bool menu_shader_manager_auto_preset_exists(enum auto_shader_type type);

void menu_shader_set_modified(bool modified);

RETRO_END_DECLS

#endif
