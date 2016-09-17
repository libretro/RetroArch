/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <retro_assert.h>
#include <retro_stat.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dirs.h"

#include "runloop.h"

bool dir_is_savefile_empty(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return false;
   return string_is_empty(global->dir.savefile);
}

bool dir_is_savestate_empty(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return false;
   return string_is_empty(global->dir.savestate);
}

size_t dir_get_savestate_size(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return 0;
   return sizeof(global->dir.savestate);
}

size_t dir_get_savefile_size(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return 0;
   return sizeof(global->dir.savefile);
}

void dir_clear_savefile(void)
{
   global_t *global = global_get_ptr();

   if (global)
      *global->dir.savefile = '\0';
}

void dir_clear_savestate(void)
{
   global_t *global = global_get_ptr();

   if (global)
      *global->dir.savestate = '\0';
}

char *dir_get_savefile_ptr(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return NULL;
   return global->dir.savefile;
}

const char *dir_get_savefile(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return NULL;
   return global->dir.savefile;
}

char *dir_get_savestate_ptr(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return NULL;
   return global->dir.savestate;
}

const char *dir_get_savestate(void)
{
   global_t *global = global_get_ptr();

   if (!global)
      return NULL;
   return global->dir.savestate;
}

void dir_set_savestate(const char *path)
{
   global_t *global = global_get_ptr();

   if (global)
      strlcpy(global->dir.savestate, global->name.savefile,
            sizeof(global->dir.savestate));
}

void dir_set_savefile(const char *path)
{
   global_t *global = global_get_ptr();

   if (global)
      strlcpy(global->dir.savefile, global->name.savefile,
            sizeof(global->dir.savefile));
}

void dir_clear_all(void)
{
}
