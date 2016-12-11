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
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <retro_assert.h>
#include <retro_stat.h>

#include "dirs.h"
#include "configuration.h"
#include "command.h"
#include "defaults.h"
#include "list_special.h"
#include "file_path_special.h"
#include "msg_hash.h"
#include "paths.h"
#include "content.h"
#include "runloop.h"
#include "verbosity.h"

struct rarch_dir_list
{
   struct string_list *list;
   size_t ptr;
};

static struct rarch_dir_list dir_shader_list;

static char dir_system[PATH_MAX_LENGTH]                 = {0};
static char dir_savefile[PATH_MAX_LENGTH]               = {0};
static char current_savefile_dir[PATH_MAX_LENGTH]       = {0};
static char current_savestate_dir[PATH_MAX_LENGTH]      = {0};
static char dir_savestate[PATH_MAX_LENGTH]              = {0};

/* init functions */

bool dir_init_shader(void)
{
   unsigned i;
   struct rarch_dir_list *dir_list = (struct rarch_dir_list*)&dir_shader_list;
   settings_t           *settings  = config_get_ptr();

   if (!*settings->directory.video_shader)
      return false;

   dir_list->list = dir_list_new_special(
         settings->directory.video_shader, DIR_LIST_SHADERS, NULL);

   if (!dir_list->list || dir_list->list->size == 0)
   {
      command_event(CMD_EVENT_SHADER_DIR_DEINIT, NULL);
      return false;
   }

   dir_list->ptr  = 0;
   dir_list_sort(dir_list->list, false);

   for (i = 0; i < dir_list->list->size; i++)
      RARCH_LOG("%s \"%s\"\n",
            msg_hash_to_str(MSG_FOUND_SHADER),
            dir_list->list->elems[i].data);
   return true;
}

/* free functions */

bool dir_free_shader(void)
{
   struct rarch_dir_list *dir_list = 
      (struct rarch_dir_list*)&dir_shader_list;

   dir_list_free(dir_list->list);
   dir_list->list = NULL;
   dir_list->ptr  = 0;

   return true;
}

/* check functions */

/**
 * dir_check_shader:
 * @pressed_next         : was next shader key pressed?
 * @pressed_previous     : was previous shader key pressed?
 *
 * Checks if any one of the shader keys has been pressed for this frame:
 * a) Next shader index.
 * b) Previous shader index.
 *
 * Will also immediately apply the shader.
 **/
void dir_check_shader(bool pressed_next, bool pressed_prev)
{
   char msg[128];
   const char *shader              = NULL;
   enum rarch_shader_type type     = RARCH_SHADER_NONE;
   struct rarch_dir_list *dir_list = (struct rarch_dir_list*)&dir_shader_list;

   if (!dir_list || !dir_list->list)
      return;

   if (pressed_next)
   {
      dir_list->ptr = (dir_list->ptr + 1) %
         dir_list->list->size;
   }
   else if (pressed_prev)
   {
      if (dir_list->ptr == 0)
         dir_list->ptr = dir_list->list->size - 1;
      else
         dir_list->ptr--;
   }
   else
      return;

   shader   = dir_list->list->elems[dir_list->ptr].data;

   switch (msg_hash_to_file_type(msg_hash_calculate(
               path_get_extension(shader))))
   {
      case FILE_TYPE_SHADER_GLSL:
      case FILE_TYPE_SHADER_PRESET_GLSLP:
         type = RARCH_SHADER_GLSL;
         break;
      case FILE_TYPE_SHADER_SLANG:
      case FILE_TYPE_SHADER_PRESET_SLANGP:
         type = RARCH_SHADER_SLANG;
         break;
      case FILE_TYPE_SHADER_CG:
      case FILE_TYPE_SHADER_PRESET_CGP:
         type = RARCH_SHADER_CG;
         break;
      default:
         return;
   }

   msg[0] = '\0';

   snprintf(msg, sizeof(msg), "%s #%u: \"%s\".",
         msg_hash_to_str(MSG_SHADER),
         (unsigned)dir_list->ptr, shader);
   runloop_msg_queue_push(msg, 2, 120, true);

   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_APPLYING_SHADER),
         shader);

   if (!video_driver_set_shader(type, shader))
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_FAILED_TO_APPLY_SHADER));
}

/* empty functions */

bool dir_is_empty(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return string_is_empty(dir_savefile);
      case RARCH_DIR_SAVEFILE:
         return string_is_empty(dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return string_is_empty(current_savefile_dir);
      case RARCH_DIR_SAVESTATE:
         return string_is_empty(dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return string_is_empty(current_savestate_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return false;
}

/* get size functions */

size_t dir_get_size(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return sizeof(dir_system);
      case RARCH_DIR_SAVESTATE:
         return sizeof(dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return sizeof(current_savestate_dir);
      case RARCH_DIR_SAVEFILE:
         return sizeof(dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return sizeof(current_savefile_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return 0;
}

/* clear functions */

void dir_clear(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         *dir_savefile = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVEFILE:
         *current_savefile_dir = '\0';
         break;
      case RARCH_DIR_SAVESTATE:
         *dir_savestate = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         *current_savestate_dir = '\0';
         break;
      case RARCH_DIR_SYSTEM:
         *dir_system = '\0';
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

void dir_clear_all(void)
{
   dir_clear(RARCH_DIR_SYSTEM);
   dir_clear(RARCH_DIR_SAVEFILE);
   dir_clear(RARCH_DIR_SAVESTATE);
}

/* get ptr functions */

char *dir_get_ptr(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return current_savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return current_savestate_dir;
      case RARCH_DIR_SYSTEM:
         return dir_system;
      case RARCH_DIR_NONE:
         break;
   }

   return NULL;
}

const char *dir_get(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return current_savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return current_savestate_dir;
      case RARCH_DIR_SYSTEM:
         return dir_system;
      case RARCH_DIR_NONE:
         break;
   }

   return NULL;
}

void dir_set(enum rarch_dir_type type, const char *path)
{
   switch (type)
   {
      case RARCH_DIR_CURRENT_SAVEFILE:
         strlcpy(current_savefile_dir, path,
               sizeof(current_savefile_dir));
         break;
      case RARCH_DIR_SAVEFILE:
         strlcpy(dir_savefile, path,
               sizeof(dir_savefile));
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         strlcpy(current_savestate_dir, path,
               sizeof(current_savestate_dir));
         break;
      case RARCH_DIR_SAVESTATE:
         strlcpy(dir_savestate, path,
               sizeof(dir_savestate));
         break;
      case RARCH_DIR_SYSTEM:
         strlcpy(dir_system, path,
               sizeof(dir_system));
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

static void check_defaults_dir_create_dir(const char *path)
{
   char new_path[PATH_MAX_LENGTH];
   new_path[0] = '\0';
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
