/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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
#include "../menu_display.h"
#include "../menu.h"
#include "../../general.h"

menu_ctx_driver_t menu_ctx_null = {
  NULL,  /* set_texture */
  NULL,  /* render_messagebox */
  NULL,  /* iterate */
  NULL,  /* render */
  NULL,  /* frame */
  NULL,  /* init */
  NULL,  /* free */
  NULL,  /* context_reset */
  NULL,  /* context_destroy */
  NULL,  /* populate_entries */
  NULL,  /* toggle */
  NULL,  /* navigation_clear */
  NULL,  /* navigation_decrement */
  NULL,  /* navigation_increment */
  NULL,  /* navigation_set */
  NULL,  /* navigation_set_last */
  NULL,  /* navigation_descend_alphabet */
  NULL,  /* navigation_ascend_alphabet */
  NULL,  /* lists_init */
  NULL,  /* list_insert */
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
  NULL,
  NULL,
};
