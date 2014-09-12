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
#include "../menu_action.h"
#include "../menu_navigation.h"

#include "../../../gfx/gfx_common.h"
#include "../../../driver.h"
#include "../../../file_ext.h"
#include "../../../input/input_common.h"
#include "../../../config.def.h"
#include "../../../input/keyboard_line.h"

#include "../../../settings_data.h"

#include "../disp/lakka.h"

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

// Move the categories left or right depending on the menu_active_category variable
static void lakka_switch_categories(void)
{
   int i, j;

   // translation
   add_tween(LAKKA_DELAY, -menu_active_category * hspacing, &all_categories_x, &inOutQuad, NULL);

   // alpha tweening
   for (i = 0; i < num_categories; i++)
   {
      float ca, cz;
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      ca = (i == menu_active_category) ? 1.0 : 0.5;
      cz = (i == menu_active_category) ? c_active_zoom : c_passive_zoom;
      add_tween(LAKKA_DELAY, ca, &category->alpha, &inOutQuad, NULL);
      add_tween(LAKKA_DELAY, cz, &category->zoom,  &inOutQuad, NULL);

      for (j = 0; j < category->num_items; j++)
      {
         float ia = (i != menu_active_category     ) ? 0   : 
            (j == category->active_item) ? 1.0 : 0.5;

         add_tween(LAKKA_DELAY, ia, &category->items[j].alpha, &inOutQuad, NULL);
      }
   }
}

static void lakka_switch_items(void)
{
   int j;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];

   for (j = 0; j < active_category->num_items; j++)
   {
      float ia, iz, iy;
      menu_item_t *active_item = (menu_item_t*)&active_category->items[j];

      if (!active_item)
         continue;

      ia = (j == active_category->active_item) ? 1.0 : 0.5;
      iz = (j == active_category->active_item) ? i_active_zoom : i_passive_zoom;
      iy = (j == active_category->active_item) ? vspacing*active_item_factor :
         (j  < active_category->active_item) ? vspacing*(j - active_category->active_item + above_item_offset) :
         vspacing*(j - active_category->active_item + under_item_offset);

      add_tween(LAKKA_DELAY, ia, &active_item->alpha, &inOutQuad, NULL);
      add_tween(LAKKA_DELAY, iz, &active_item->zoom,  &inOutQuad, NULL);
      add_tween(LAKKA_DELAY, iy, &active_item->y,     &inOutQuad, NULL);
   }
}

static void lakka_switch_subitems(void)
{
   int k;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];
   menu_item_t *item = (menu_item_t*)&active_category->items[active_category->active_item];

   for (k = 0; k < item->num_subitems; k++)
   {
      menu_subitem_t *subitem = (menu_subitem_t*)&item->subitems[k];

      if (!subitem)
         continue;

      if (k < item->active_subitem)
      {
         /* Above items */
         add_tween(LAKKA_DELAY, 0.5, &subitem->alpha, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, vspacing*(k - item->active_subitem + above_subitem_offset), &subitem->y, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, i_passive_zoom, &subitem->zoom, &inOutQuad, NULL);
      }
      else if (k == item->active_subitem)
      {
         /* Active item */
         add_tween(LAKKA_DELAY, 1.0, &subitem->alpha, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, vspacing*active_item_factor, &subitem->y, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, i_active_zoom, &subitem->zoom, &inOutQuad, NULL);
      }
      else if (k > item->active_subitem)
      {
         /* Under items */
         add_tween(LAKKA_DELAY, 0.5, &subitem->alpha, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, vspacing*(k - item->active_subitem + under_item_offset), &subitem->y, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, i_passive_zoom, &subitem->zoom, &inOutQuad, NULL);
      }
   }
}

static void lakka_reset_submenu(void)
{
   int i, j, k;
   menu_category_t *active_category = (menu_category_t*)&categories[menu_active_category];

   if (!(
            g_extern.main_is_init
            && !g_extern.libretro_dummy
            && strcmp(g_extern.fullpath, active_category->items[active_category->active_item].rom) == 0))
   {

      // Keeps active submenu state (do we really want that?)
      active_category->items[active_category->active_item].active_subitem = 0;
      for (i = 0; i < num_categories; i++)
      {
         menu_category_t *category = (menu_category_t*)&categories[i];

         if (!category)
            continue;

         for (j = 0; j < category->num_items; j++)
         {
            for (k = 0; k < category->items[j].num_subitems; k++)
            {
               menu_subitem_t *subitem = (menu_subitem_t*)&category->items[j].subitems[k];

               if (!subitem)
                  continue;

               subitem->alpha = 0;
               subitem->zoom = k == category->items[j].active_subitem ? i_active_zoom : i_passive_zoom;
               subitem->y = k == 0 ? vspacing * active_item_factor : vspacing * (k + under_item_offset);
            }
         }
      }
   }
}

static void lakka_open_submenu(void)
{
   int i, j, k;
    
   add_tween(LAKKA_DELAY, -hspacing * (menu_active_category+1), &all_categories_x, &inOutQuad, NULL);
   add_tween(LAKKA_DELAY, 1.0, &arrow_alpha, &inOutQuad, NULL);

   // Reset contextual menu style
   lakka_reset_submenu();
   
   for (i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      if (i != menu_active_category)
         add_tween(LAKKA_DELAY, 0, &category->alpha, &inOutQuad, NULL);
      else
      {
         add_tween(LAKKA_DELAY, 1.0, &category->alpha, &inOutQuad, NULL);

         for (j = 0; j < category->num_items; j++)
         {
            if (j == category->active_item)
            {
               for (k = 0; k < category->items[j].num_subitems; k++)
               {
                  menu_subitem_t *subitem = (menu_subitem_t*)&category->items[j].subitems[k];

                  if (k == category->items[j].active_subitem)
                  {
                     add_tween(LAKKA_DELAY, 1.0, &subitem->alpha, &inOutQuad, NULL);
                     add_tween(LAKKA_DELAY, i_active_zoom, &subitem->zoom, &inOutQuad, NULL);
                  }
                  else
                  {
                     add_tween(LAKKA_DELAY, 0.5, &subitem->alpha, &inOutQuad, NULL);
                     add_tween(LAKKA_DELAY, i_passive_zoom, &subitem->zoom, &inOutQuad, NULL);
                  }
               }
            }
            else
               add_tween(LAKKA_DELAY, 0, &category->items[j].alpha, &inOutQuad, NULL);
         }
      }
   }
}

static void lakka_close_submenu(void)
{
   int i, j, k;
    
   add_tween(LAKKA_DELAY, -hspacing * menu_active_category, &all_categories_x, &inOutQuad, NULL);
   add_tween(LAKKA_DELAY, 0.0, &arrow_alpha, &inOutQuad, NULL);
   
   for (i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      if (i == menu_active_category)
      {
         add_tween(LAKKA_DELAY, 1.0, &category->alpha, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, c_active_zoom, &category->zoom, &inOutQuad, NULL);

         for (j = 0; j < category->num_items; j++)
         {
            if (j == category->active_item)
            {
               add_tween(LAKKA_DELAY, 1.0, &category->items[j].alpha, &inOutQuad, NULL);

               for (k = 0; k < category->items[j].num_subitems; k++)
                  add_tween(LAKKA_DELAY, 0, &category->items[j].subitems[k].alpha, &inOutQuad, NULL);
            }
            else
               add_tween(LAKKA_DELAY, 0.5, &category->items[j].alpha, &inOutQuad, NULL);
         }
      }
      else
      {
         add_tween(LAKKA_DELAY, 0.5, &category->alpha, &inOutQuad, NULL);
         add_tween(LAKKA_DELAY, c_passive_zoom, &category->zoom, &inOutQuad, NULL);

         for (j = 0; j < category->num_items; j++)
            add_tween(LAKKA_DELAY, 0, &category->items[j].alpha, &inOutQuad, NULL);
      }
   }
}

static int menu_lakka_iterate(unsigned action)
{
   menu_category_t *active_category;
   menu_item_t *active_item;
   menu_subitem_t * active_subitem;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot iterate menu, menu handle is not initialized.\n");
      return 0;
   }

   active_category = NULL;
   active_item = NULL;
   active_subitem = NULL;

   active_category = (menu_category_t*)&categories[menu_active_category];

   if (active_category)
      active_item = (menu_item_t*)&active_category->items[active_category->active_item];

   if (active_item)
      active_subitem = (menu_subitem_t*)&active_item->subitems[active_item->active_subitem];

   if (!active_category || !active_item)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   if (action && depth == 1 && menu_active_category == 0 
      && active_subitem->setting)
   {
      if (active_subitem->setting->type == ST_BOOL)
         menu_common_setting_set_current_boolean(
            active_subitem->setting, action);
      else if (active_subitem->setting->type == ST_UINT)
         menu_common_setting_set_current_unsigned_integer(
            active_subitem->setting, 0, action);
      else if (active_subitem->setting->type == ST_FLOAT)
         menu_common_setting_set_current_fraction(
            active_subitem->setting, action);
   }

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (depth == 0 && menu_active_category > 0)
         {
            menu_active_category--;
            lakka_switch_categories();
         }
         else if (depth == 1 && menu_active_category > 0 
            && (active_item->active_subitem == 1 
            || active_item->active_subitem == 2)
            && g_settings.state_slot > -1)
         {
            g_settings.state_slot--;
         }
         break;

      case MENU_ACTION_RIGHT:
         if (depth == 0 && menu_active_category < num_categories-1)
         {
            menu_active_category++;
            lakka_switch_categories();
         }
         else if (depth == 1 && menu_active_category > 0 
            && (active_item->active_subitem == 1
            || active_item->active_subitem == 2)
            && g_settings.state_slot < 255)
         {
            g_settings.state_slot++;
         }
         break;

      case MENU_ACTION_DOWN:
         if (depth == 0 && active_category->active_item < active_category->num_items - 1)
         {
            active_category->active_item++;
            lakka_switch_items();
         }
         if (depth == 1 // if we are on subitems level
         && active_item->active_subitem < active_item->num_subitems -1 // and we do not exceed the number of subitems
         && (menu_active_category == 0 // and we are in settings or a rom is launched
         || ((active_item->active_subitem < active_item->num_subitems -1) && (g_extern.main_is_init && !g_extern.libretro_dummy) && strcmp(g_extern.fullpath, active_item->rom) == 0)))
         {
            active_item->active_subitem++;
            lakka_switch_subitems();
         }
         break;

      case MENU_ACTION_UP:
         if (depth == 0 && active_category->active_item > 0)
         {
            active_category->active_item--;
            lakka_switch_items();
         }
         if (depth == 1 && active_item->active_subitem > 0)
         {
            active_item->active_subitem--;
            lakka_switch_subitems();
         }
         break;

      case MENU_ACTION_OK:
         if (depth == 1 && menu_active_category > 0)
         {
            switch (active_item->active_subitem)
            {
               case 0:
                  global_alpha = 0.0;
                  if (g_extern.main_is_init && !g_extern.libretro_dummy
                        && strcmp(g_extern.fullpath, active_item->rom) == 0)
                  {
                     rarch_main_command(RARCH_CMD_RESUME);
                  }
                  else
                  {
                     strlcpy(g_extern.fullpath, active_item->rom, sizeof(g_extern.fullpath));
                     strlcpy(g_settings.libretro, active_category->libretro, sizeof(g_settings.libretro));

#ifdef HAVE_DYNAMIC
                     rarch_main_command(RARCH_CMD_LOAD_CORE);
                     rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
#endif
                  }
                  return -1;
                  break;
               case 1:
                  global_alpha = 0.0;
                  rarch_main_command(RARCH_CMD_SAVE_STATE);
                  return -1;
                  break;
               case 2:
                  global_alpha = 0.0;
                  rarch_main_command(RARCH_CMD_LOAD_STATE);
                  return -1;
                  break;
               case 3:
                  rarch_main_command(RARCH_CMD_TAKE_SCREENSHOT);
                  break;
               case 4:
                  global_alpha = 0.0;
                  rarch_main_command(RARCH_CMD_RESET);
                  return -1;
                  break;
            }
         }
         else if (depth == 0 && active_item->num_subitems)
         {
            lakka_open_submenu();
            depth = 1;
         }
         else if (depth == 0 && menu_active_category == 0 && active_category->active_item == active_category->num_items-1)
         {
            add_tween(LAKKA_DELAY, 1.0, &global_alpha, &inOutQuad, NULL);
            rarch_main_set_state(RARCH_ACTION_STATE_RUNNING_FINISHED);
            return -1;
         }
         break;

      case MENU_ACTION_CANCEL:
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
      driver.menu_ctx->iterate(driver.menu, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   return 0;
}

menu_ctx_driver_backend_t menu_ctx_backend_lakka = {
   menu_lakka_iterate,
//#ifndef HAVE_SHADER_MANAGER
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
//#endif
   NULL,
   NULL,
   "menu_lakka",
};
