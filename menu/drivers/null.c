/*  RetroArch - A frontend for libretro.
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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../menu_driver.h"

static bool g_menu_on = false;

static void* null_init(void **userdata, bool video_is_threaded)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   return menu;
}

static void null_free(void *data)
{
}

static void null_toggle(void *userdata, bool menu_on)
{
   (void)userdata;

   g_menu_on = menu_on;
}

static int null_menu_iterate(menu_handle_t *menu, void *userdata, enum menu_action action)
{
   (void)menu;
   (void)userdata;
   (void)action;

   return 1;
}

menu_ctx_driver_t menu_ctx_null = {
  NULL,  /* set_texture */
  NULL,  /* render_messagebox */
  null_menu_iterate,
  NULL,  /* render */
  NULL,  /* frame */
  null_init,
  null_free,
  NULL,  /* context_reset */
  NULL,  /* context_destroy */
  NULL,  /* populate_entries */
  null_toggle,
  NULL,  /* navigation_clear */
  NULL,  /* navigation_decrement */
  NULL,  /* navigation_increment */
  NULL,  /* navigation_set */
  NULL,  /* navigation_set_last */
  NULL,  /* navigation_descend_alphabet */
  NULL,  /* navigation_ascend_alphabet */
  NULL,  /* lists_init */
  NULL,  /* list_insert */
  NULL,  /* list_prepend */
  NULL,  /* list_delete */
  NULL,  /* list_clear */
  NULL,  /* list_cache */
  NULL,  /* list_push */
  NULL,  /* list_get_selection */
  NULL,  /* list_get_size */
  NULL,  /* list_get_entry */
  NULL,  /* list_set_selection */
  NULL,  /* bind_init */
  NULL,  /* load_image */
  "null",
  NULL,  /* environ */
  NULL,  /* pointer_tap */
  NULL,  /* update_thumbnail_path */
  NULL,  /* update_thumbnail_image */
  NULL,  /* refresh_thumbnail_image */
  NULL,  /* set_thumbnail_system */
  NULL,  /* get_thumbnail_system */
  NULL,  /* set_thumbnail_content */
  NULL,  /* osk_ptr_at_pos */
  NULL,  /* update_savestate_thumbnail_path */
  NULL,  /* update_savestate_thumbnail_image */
  NULL,  /* pointer_down */
  NULL,  /* pointer_up   */
  NULL,  /* get_load_content_animation_data */
};
