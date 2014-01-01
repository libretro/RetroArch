/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef __APPLE_RARCH_CORE_INFO_EXT_H__
#define __APPLE_RARCH_CORE_INFO_EXT_H__

#include "core_info.h"
#include "frontend/menu/history.h"

void apple_core_info_set_core_path(const char* core_path);
void apple_core_info_set_config_path(const char* config_path);

core_info_list_t* apple_core_info_list_get(void);
const core_info_t* apple_core_info_list_get_by_id(const char* core_id);
const char* apple_core_info_get_id(const core_info_t* info, char* buffer, size_t buffer_length);

const char* apple_core_info_get_custom_config(const char* core_id, char* buffer, size_t buffer_length);
bool apple_core_info_has_custom_config(const char* core_id);


// ROM HISTORY EXTENSIONS
const char* apple_rom_history_get_path(rom_history_t* history, uint32_t index);
const char* apple_rom_history_get_core_path(rom_history_t* history, uint32_t index);
const char* apple_rom_history_get_core_name(rom_history_t* history, uint32_t index);

#endif
