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

// Move the categories left or right depending on the menu_active_category variable
static void lakka_switch_categories(void)
{
   int i, j;

   // translation
   add_tween(DELAY, -menu_active_category * HSPACING, &all_categories_x, &inOutQuad, NULL);

   // alpha tweening
   for (i = 0; i < num_categories; i++)
   {
      float ca, cz;
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      ca = (i == menu_active_category) ? 1.0 : 0.5;
      cz = (i == menu_active_category) ? C_ACTIVE_ZOOM : C_PASSIVE_ZOOM;
      add_tween(DELAY, ca, &category->alpha, &inOutQuad, NULL);
      add_tween(DELAY, cz, &category->zoom,  &inOutQuad, NULL);

      for (j = 0; j < category->num_items; j++)
      {
         float ia = (i != menu_active_category     ) ? 0   : 
            (j == category->active_item) ? 1.0 : 0.5;

         add_tween(DELAY, ia, &category->items[j].alpha, &inOutQuad, NULL);
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
      iz = (j == active_category->active_item) ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
      iy = (j == active_category->active_item) ? VSPACING*ACTIVE_ITEM_FACTOR :
         (j  < active_category->active_item) ? VSPACING*(j - active_category->active_item + ABOVE_ITEM_OFFSET) :
         VSPACING*(j - active_category->active_item + UNDER_ITEM_OFFSET);

      add_tween(DELAY, ia, &active_item->alpha, &inOutQuad, NULL);
      add_tween(DELAY, iz, &active_item->zoom,  &inOutQuad, NULL);
      add_tween(DELAY, iy, &active_item->y,     &inOutQuad, NULL);
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
         // Above items
         add_tween(DELAY, 0.5, &subitem->alpha, &inOutQuad, NULL);
         add_tween(DELAY, VSPACING*(k - item->active_subitem + ABOVE_SUBITEM_OFFSET), &subitem->y, &inOutQuad, NULL);
         add_tween(DELAY, I_PASSIVE_ZOOM, &subitem->zoom, &inOutQuad, NULL);
      }
      else if (k == item->active_subitem)
      {
         // Active item
         add_tween(DELAY, 1.0, &subitem->alpha, &inOutQuad, NULL);
         add_tween(DELAY, VSPACING*ACTIVE_ITEM_FACTOR, &subitem->y, &inOutQuad, NULL);
         add_tween(DELAY, I_ACTIVE_ZOOM, &subitem->zoom, &inOutQuad, NULL);
      }
      else if (k > item->active_subitem)
      {
         // Under items
         add_tween(DELAY, 0.5, &subitem->alpha, &inOutQuad, NULL);
         add_tween(DELAY, VSPACING*(k - item->active_subitem + UNDER_ITEM_OFFSET), &subitem->y, &inOutQuad, NULL);
         add_tween(DELAY, I_PASSIVE_ZOOM, &subitem->zoom, &inOutQuad, NULL);
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
               subitem->zoom = k == category->items[j].active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
               subitem->y = k == 0 ? VSPACING * ACTIVE_ITEM_FACTOR : VSPACING * (3+k);
            }
         }
      }
   }
}

static void lakka_open_submenu(void)
{
   int i, j, k;
   add_tween(DELAY, -HSPACING * (menu_active_category+1), &all_categories_x, &inOutQuad, NULL);

   // Reset contextual menu style
   lakka_reset_submenu();
   
   for (i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      if (i != menu_active_category)
         add_tween(DELAY, 0, &category->alpha, &inOutQuad, NULL);
      else
      {
         add_tween(DELAY, 1.0, &category->alpha, &inOutQuad, NULL);

         for (j = 0; j < category->num_items; j++)
         {
            if (j == category->active_item)
            {
               for (k = 0; k < category->items[j].num_subitems; k++)
               {
                  menu_subitem_t *subitem = (menu_subitem_t*)&category->items[j].subitems[k];

                  if (k == category->items[j].active_subitem)
                  {
                     add_tween(DELAY, 1.0, &subitem->alpha, &inOutQuad, NULL);
                     add_tween(DELAY, I_ACTIVE_ZOOM, &subitem->zoom, &inOutQuad, NULL);
                  }
                  else
                  {
                     add_tween(DELAY, 0.5, &subitem->alpha, &inOutQuad, NULL);
                     add_tween(DELAY, I_PASSIVE_ZOOM, &subitem->zoom, &inOutQuad, NULL);
                  }
               }
            }
            else
               add_tween(DELAY, 0, &category->items[j].alpha, &inOutQuad, NULL);
         }
      }
   }
}

static void lakka_close_submenu(void)
{
   int i, j, k;
   add_tween(DELAY, -HSPACING * menu_active_category, &all_categories_x, &inOutQuad, NULL);
   
   for (i = 0; i < num_categories; i++)
   {
      menu_category_t *category = (menu_category_t*)&categories[i];

      if (!category)
         continue;

      if (i == menu_active_category)
      {
         add_tween(DELAY, 1.0, &category->alpha, &inOutQuad, NULL);
         add_tween(DELAY, C_ACTIVE_ZOOM, &category->zoom, &inOutQuad, NULL);

         for (j = 0; j < category->num_items; j++)
         {
            if (j == category->active_item)
            {
               add_tween(DELAY, 1.0, &category->items[j].alpha, &inOutQuad, NULL);

               for (k = 0; k < category->items[j].num_subitems; k++)
                  add_tween(DELAY, 0, &category->items[j].subitems[k].alpha, &inOutQuad, NULL);
            }
            else
               add_tween(DELAY, 0.5, &category->items[j].alpha, &inOutQuad, NULL);
         }
      }
      else
      {
         add_tween(DELAY, 0.5, &category->alpha, &inOutQuad, NULL);
         add_tween(DELAY, C_PASSIVE_ZOOM, &category->zoom, &inOutQuad, NULL);

         for (j = 0; j < category->num_items; j++)
            add_tween(DELAY, 0, &category->items[j].alpha, &inOutQuad, NULL);
      }
   }
}

static int menu_lakka_iterate(unsigned action)
{
   menu_category_t *active_category;
   menu_item_t *active_item;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot iterate menu, menu handle is not initialized.\n");
      return 0;
   }

   active_category = NULL;
   active_item = NULL;

   active_category = (menu_category_t*)&categories[menu_active_category];

   if (active_category)
      active_item = (menu_item_t*)&active_category->items[active_category->active_item];

   if (!active_category || !active_item)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (depth == 0 && menu_active_category > 0)
         {
            menu_active_category--;
            lakka_switch_categories();
         }
         break;

      case MENU_ACTION_RIGHT:
         if (depth == 0 && menu_active_category < num_categories-1)
         {
            menu_active_category++;
            lakka_switch_categories();
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
         || ((active_item->active_subitem < active_item->num_subitems -1) && (g_extern.main_is_init && !g_extern.libretro_dummy) && strcmp(g_extern.fullpath, &active_item->rom) == 0)))
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
         if (depth == 1)
         {
            switch (active_item->active_subitem)
            {
               case 0:
                  global_alpha = 0.0;
                  if (g_extern.main_is_init && !g_extern.libretro_dummy
                        && strcmp(g_extern.fullpath, active_item->rom) == 0)
                     g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  else
                  {
                     strlcpy(g_extern.fullpath, active_item->rom, sizeof(g_extern.fullpath));
                     strlcpy(g_settings.libretro, active_category->libretro, sizeof(g_settings.libretro));

#ifdef HAVE_DYNAMIC
                     rarch_main_command(RARCH_CMD_LOAD_CORE);
                     g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
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
         else if (depth == 0 && menu_active_category == 0 && active_item->active_subitem == 1) // Hardcoded "Quit" item index
         {
            printf("EXIT\n");
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
