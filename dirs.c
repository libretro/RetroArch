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

#include "dirs.h"
#include "defaults.h"

static char dir_osk_overlay[PATH_MAX_LENGTH] = {0};
static char dir_system[PATH_MAX_LENGTH]      = {0};
static char dir_savefile[PATH_MAX_LENGTH]    = {0};
static char dir_savestate[PATH_MAX_LENGTH]   = {0};

/* empty functions */

bool dir_is_system_empty(void)
{
   return string_is_empty(dir_savefile);
}

bool dir_is_savefile_empty(void)
{
   return string_is_empty(dir_savefile);
}

bool dir_is_savestate_empty(void)
{
   return string_is_empty(dir_savestate);
}

bool dir_is_osk_overlay_empty(void)
{
   return string_is_empty(dir_osk_overlay);
}

/* get size functions */

size_t dir_get_system_size(void)
{
   return sizeof(dir_system);
}

size_t dir_get_savestate_size(void)
{
   return sizeof(dir_savestate);
}

size_t dir_get_savefile_size(void)
{
   return sizeof(dir_savefile);
}

size_t dir_get_osk_overlay_size(void)
{
   return sizeof(dir_osk_overlay);
}

/* clear functions */

void dir_clear_system(void)
{
   *dir_system = '\0';
}

void dir_clear_savefile(void)
{
   *dir_savefile = '\0';
}

void dir_clear_savestate(void)
{
   *dir_savestate = '\0';
}

void dir_clear_osk_overlay(void)
{
   *dir_osk_overlay = '\0';
}

void dir_clear_all(void)
{
   dir_clear_system();
   dir_clear_osk_overlay();
   dir_clear_savefile();
   dir_clear_savestate();
}

/* get ptr functions */

char *dir_get_osk_overlay_ptr(void)
{
   return dir_osk_overlay;
}

char *dir_get_savefile_ptr(void)
{
   return dir_savefile;
}

char *dir_get_system_ptr(void)
{
   return dir_system;
}

char *dir_get_savestate_ptr(void)
{
   return dir_savestate;
}

/* get functions */

const char *dir_get_osk_overlay(void)
{
   return dir_osk_overlay;
}

const char *dir_get_system(void)
{
   return dir_system;
}

const char *dir_get_savefile(void)
{
   return dir_savefile;
}

const char *dir_get_savestate(void)
{
   return dir_savestate;
}

/* set functions */

void dir_set_osk_overlay(const char *path)
{
   strlcpy(dir_osk_overlay, path,
         sizeof(dir_osk_overlay));
}

void dir_set_system(const char *path)
{
   strlcpy(dir_system, path,
         sizeof(dir_system));
}

void dir_set_savestate(const char *path)
{
   strlcpy(dir_savestate, path,
         sizeof(dir_savestate));
}

void dir_set_savefile(const char *path)
{
   strlcpy(dir_savefile, path,
         sizeof(dir_savefile));
}

static void check_defaults_dir_create_dir(const char *path)
{
   char new_path[PATH_MAX_LENGTH] = {0};
   fill_pathname_expand_special(new_path,
         path, sizeof(new_path));

   if (path_is_directory(new_path))
      return;
   path_mkdir(new_path);
}

void dir_check_defaults(void)
{
   /* early return for people with a custom folder setup
      so it doesn't create unnecessary directories
    */
   if (path_file_exists("custom.ini"))
      return;

   if (!string_is_empty(g_defaults.dir.core_assets))
      check_defaults_dir_create_dir(g_defaults.dir.core_assets);
   if (!string_is_empty(g_defaults.dir.remap))
      check_defaults_dir_create_dir(g_defaults.dir.remap);
   if (!string_is_empty(g_defaults.dir.screenshot))
      check_defaults_dir_create_dir(g_defaults.dir.screenshot);
   if (!string_is_empty(g_defaults.dir.core))
      check_defaults_dir_create_dir(g_defaults.dir.core);
   if (!string_is_empty(g_defaults.dir.autoconfig))
      check_defaults_dir_create_dir(g_defaults.dir.autoconfig);
   if (!string_is_empty(g_defaults.dir.audio_filter))
      check_defaults_dir_create_dir(g_defaults.dir.audio_filter);
   if (!string_is_empty(g_defaults.dir.video_filter))
      check_defaults_dir_create_dir(g_defaults.dir.video_filter);
   if (!string_is_empty(g_defaults.dir.assets))
      check_defaults_dir_create_dir(g_defaults.dir.assets);
   if (!string_is_empty(g_defaults.dir.playlist))
      check_defaults_dir_create_dir(g_defaults.dir.playlist);
   if (!string_is_empty(g_defaults.dir.core))
      check_defaults_dir_create_dir(g_defaults.dir.core);
   if (!string_is_empty(g_defaults.dir.core_info))
      check_defaults_dir_create_dir(g_defaults.dir.core_info);
   if (!string_is_empty(g_defaults.dir.overlay))
      check_defaults_dir_create_dir(g_defaults.dir.overlay);
   if (!string_is_empty(g_defaults.dir.port))
      check_defaults_dir_create_dir(g_defaults.dir.port);
   if (!string_is_empty(g_defaults.dir.shader))
      check_defaults_dir_create_dir(g_defaults.dir.shader);
   if (!string_is_empty(g_defaults.dir.savestate))
      check_defaults_dir_create_dir(g_defaults.dir.savestate);
   if (!string_is_empty(g_defaults.dir.sram))
      check_defaults_dir_create_dir(g_defaults.dir.sram);
   if (!string_is_empty(g_defaults.dir.system))
      check_defaults_dir_create_dir(g_defaults.dir.system);
   if (!string_is_empty(g_defaults.dir.resampler))
      check_defaults_dir_create_dir(g_defaults.dir.resampler);
   if (!string_is_empty(g_defaults.dir.menu_config))
      check_defaults_dir_create_dir(g_defaults.dir.menu_config);
   if (!string_is_empty(g_defaults.dir.content_history))
      check_defaults_dir_create_dir(g_defaults.dir.content_history);
   if (!string_is_empty(g_defaults.dir.cache))
      check_defaults_dir_create_dir(g_defaults.dir.cache);
   if (!string_is_empty(g_defaults.dir.database))
      check_defaults_dir_create_dir(g_defaults.dir.database);
   if (!string_is_empty(g_defaults.dir.cursor))
      check_defaults_dir_create_dir(g_defaults.dir.cursor);
   if (!string_is_empty(g_defaults.dir.cheats))
      check_defaults_dir_create_dir(g_defaults.dir.cheats);
   if (!string_is_empty(g_defaults.dir.thumbnails))
      check_defaults_dir_create_dir(g_defaults.dir.thumbnails);
}
