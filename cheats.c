/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "cheats.h"
#include "general.h"
#include "dynamic.h"
#include <compat/strl.h>
#include <compat/posix_string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

void cheat_manager_apply_cheats(cheat_manager_t *handle)
{
   unsigned i;
   unsigned idx = 0;

   pretro_cheat_reset();
   for (i = 0; i < handle->size; i++)
   {
      if (handle->cheats[i].state)
         pretro_cheat_set(idx++, true, handle->cheats[i].code);
   }
}

cheat_manager_t *cheat_manager_new(void)
{
   unsigned i;
   cheat_manager_t *handle = NULL;
   handle = (cheat_manager_t*)calloc(1, sizeof(struct cheat_manager));
   if (!handle)
      return NULL;

   handle->buf_size = handle->size = 0;
   handle->cheats = (struct item_cheat*)
      calloc(handle->buf_size, sizeof(struct item_cheat));

   if (!handle->cheats)
   {
      handle->buf_size = 0;
      handle->size = 0;
      handle->cheats = NULL;
      return handle;
   }

   for (i = 0; i < handle->size; i++)
   {
      handle->cheats[i].desc   = NULL;
      handle->cheats[i].code   = NULL;
      handle->cheats[i].state  = false;
   }

   return handle;
}

bool cheat_manager_realloc(cheat_manager_t *handle, unsigned new_size)
{
   unsigned i;

   if (!handle->cheats)
      handle->cheats = (struct item_cheat*)
         calloc(new_size, sizeof(struct item_cheat));
   else
      handle->cheats = (struct item_cheat*)
         realloc(handle->cheats, new_size * sizeof(struct item_cheat));

   if (!handle->cheats)
   {
      handle->buf_size = handle->size = 0;
      handle->cheats = NULL;
      return false;
   }

   handle->buf_size = new_size;
   handle->size     = new_size;

   for (i = 0; i < handle->size; i++)
   {
      handle->cheats[i].desc    = NULL;
      handle->cheats[i].code    = NULL;
      handle->cheats[i].state   = false;
   }

   return true;
}

void cheat_manager_free(cheat_manager_t *handle)
{
   unsigned i;
   if (!handle)
      return;

   if (handle->cheats)
   {
      for (i = 0; i < handle->size; i++)
      {
         free(handle->cheats[i].desc);
         free(handle->cheats[i].code);
      }

      free(handle->cheats);
   }

   free(handle);
}

static void cheat_manager_update(cheat_manager_t *handle)
{
   msg_queue_clear(g_extern.msg_queue);
   char msg[256];
   snprintf(msg, sizeof(msg), "Cheat: #%u [%s]: %s",
         handle->ptr, handle->cheats[handle->ptr].state ? "ON" : "OFF",
         handle->cheats[handle->ptr].desc);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);
}


void cheat_manager_toggle(cheat_manager_t *handle)
{
   if (!handle)
      return;

   handle->cheats[handle->ptr].state ^= true;
   cheat_manager_apply_cheats(handle);
   cheat_manager_update(handle);
}

void cheat_manager_index_next(cheat_manager_t *handle)
{
   if (!handle)
      return;
   handle->ptr = (handle->ptr + 1) % handle->size;
   cheat_manager_update(handle);
}

void cheat_manager_index_prev(cheat_manager_t *handle)
{
   if (!handle)
      return;

   if (handle->ptr == 0)
      handle->ptr = handle->size - 1;
   else
      handle->ptr--;

   cheat_manager_update(handle);
}
