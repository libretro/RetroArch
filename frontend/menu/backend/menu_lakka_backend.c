/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2014      - Jean-André Santoni
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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "menu_common_backend.h"
#include "../menu_navigation.h"

#include "../../../gfx/gfx_common.h"
#include "../../../driver.h"
#include "../../../file_ext.h"
#include "../../../input/input_common.h"
#include "../../../config.def.h"
#include "../../../input/keyboard_line.h"

#include "../disp/lakka.h"

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

static int menu_lakka_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];

   if (!active_category)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(rgui, false);

   switch (action)
   {
      case RGUI_ACTION_LEFT:
         if (depth == 0 && menu_active_category > 0)
         {
            menu_active_category--;
            lakka_switch_categories();
         }
         break;

      case RGUI_ACTION_RIGHT:
         if (depth == 0 && menu_active_category < num_categories-1)
         {
            menu_active_category++;
            lakka_switch_categories();
         }
         break;

      case RGUI_ACTION_DOWN:
         if (depth == 0 && active_category->active_item < active_category->num_items - 1)
         {
            active_category->active_item++;
            lakka_switch_items();
         }
         if (depth == 1 && 
               (active_category->items[active_category->active_item].active_subitem < active_category->items[active_category->active_item].num_subitems -1) &&
               (g_extern.main_is_init && !g_extern.libretro_dummy) &&
               strcmp(g_extern.fullpath, active_category->items[active_category->active_item].rom) == 0)
         {
            active_category->items[active_category->active_item].active_subitem++;
            lakka_switch_subitems();
         }
         break;

      case RGUI_ACTION_UP:
         if (depth == 0 && active_category->active_item > 0)
         {
            active_category->active_item--;
            lakka_switch_items();
         }
         if (depth == 1 && active_category->items[active_category->active_item].active_subitem > 0)
         {
            active_category->items[active_category->active_item].active_subitem--;
            lakka_switch_subitems();
         }
         break;

      case RGUI_ACTION_OK:
         if (depth == 1)
         {
            switch (active_category->items[active_category->active_item].active_subitem)
            {
               case 0:
                  if (g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, active_category->items[active_category->active_item].rom) == 0)
                  {
                     g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  }
                  else
                  {
                     strlcpy(g_extern.fullpath, active_category->items[active_category->active_item].rom, sizeof(g_extern.fullpath));
                     strlcpy(g_settings.libretro, active_category->libretro, sizeof(g_settings.libretro));

#ifdef HAVE_DYNAMIC
                     menu_update_system_info(rgui, &rgui->load_no_rom);
                     g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
#else
                     rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)g_settings.libretro);
                     rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif
                  }
                  return -1;
                  break;
               case 1:
                  rarch_save_state();
                  g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  return -1;
                  break;
               case 2:
                  rarch_load_state();
                  g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  return -1;
                  break;
               case 3:
                  rarch_take_screenshot();
                  break;
               case 4:
                  rarch_game_reset();
                  g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  return -1;
                  break;
            }
         }
         else if (depth == 0 && active_category->num_items)
         {
            lakka_open_submenu();
            depth = 1;
         }
         break;

      case RGUI_ACTION_CANCEL:
         if (depth == 1)
         {
            lakka_close_submenu();
            depth = 0;
         }
         break;
      default:
         break;
   }

   if (driver.menu_ctx && driver.menu_ctx->iterate)
      driver.menu_ctx->iterate(rgui, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render(rgui);

   return 0;
}

const menu_ctx_driver_backend_t menu_ctx_backend_lakka = {
   NULL,
   menu_lakka_iterate,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "menu_lakka",
};
