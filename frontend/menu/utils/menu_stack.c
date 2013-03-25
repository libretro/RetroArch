/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "menu_stack.h"

static unsigned char menu_stack_enum_array[10];
static unsigned stack_idx = 0;
static bool need_refresh = false;

static void menu_stack_pop(void)
{
   if(stack_idx > 1)
   {
      stack_idx--;
      need_refresh = true;
   }
}

static void menu_stack_force_refresh(void)
{
   need_refresh = true;
}

static void menu_stack_push(unsigned menu_id)
{
   menu_stack_enum_array[++stack_idx] = menu_id;
   need_refresh = true;
}

static void menu_stack_get_current_ptr(menu *current_menu)
{
   if(!need_refresh)
      return;

   unsigned menu_id = menu_stack_enum_array[stack_idx];

   switch(menu_id)
   {
      case INGAME_MENU:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_INGAME_MENU;
         current_menu->entry = ingame_menu;
         break;
      case INGAME_MENU_RESIZE:
         current_menu->enum_id = INGAME_MENU_RESIZE;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_INGAME_MENU;
         current_menu->entry = ingame_menu_resize;
         break;
      case INGAME_MENU_SCREENSHOT:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_INGAME_MENU;
         current_menu->entry = ingame_menu_screenshot;
         break;
      case FILE_BROWSER_MENU:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_rom;
         break;
      case LIBRETRO_CHOICE:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_file;
         break;
      case PRESET_CHOICE:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_file;
         break;
      case INPUT_PRESET_CHOICE:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_file;
         break;
      case SHADER_CHOICE:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_file;
         break;
      case BORDER_CHOICE:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_file;
         break;
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         current_menu->enum_id = menu_id;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         current_menu->entry = select_directory;
         break;
      case GENERAL_VIDEO_MENU:
         current_menu->enum_id = GENERAL_VIDEO_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      case GENERAL_AUDIO_MENU:
         current_menu->enum_id = GENERAL_AUDIO_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      case EMU_GENERAL_MENU:
         current_menu->enum_id = EMU_GENERAL_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      case EMU_VIDEO_MENU:
         current_menu->enum_id = EMU_VIDEO_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      case EMU_AUDIO_MENU:
         current_menu->enum_id = EMU_AUDIO_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      case PATH_MENU:
         current_menu->enum_id = PATH_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      case CONTROLS_MENU:
         current_menu->enum_id = CONTROLS_MENU;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_SETTINGS;
         current_menu->entry = select_setting;
         break;
      default:
         break;
   }

   need_refresh = false;
}
