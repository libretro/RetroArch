/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include "general.h"

void rarch_playlist_load_content(content_playlist_t *playlist,
      unsigned idx)
{
   const char *path      = NULL;
   const char *core_path = NULL;

   content_playlist_get_index(playlist,
         idx, &path, &core_path, NULL);

   strlcpy(g_settings.libretro, core_path, sizeof(g_settings.libretro));

   driver.menu->load_no_content = (path) ? false : true;

   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)path);

   rarch_main_command(RARCH_CMD_LOAD_CORE);
}

void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv)
{
   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

   if (!args->no_content)
   {
      if (args->content_path)
      {
         RARCH_LOG("Using content: %s.\n", args->content_path);
         argv[(*argc)++] = strdup(args->content_path);
      }
      else
      {
         RARCH_LOG("No content, starting dummy core.\n");
         argv[(*argc)++] = strdup("--menu");
      }
   }

   if (args->sram_path)
   {
      argv[(*argc)++] = strdup("-s");
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strdup("-S");
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strdup("-c");
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strdup("-L");
      argv[(*argc)++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[(*argc)++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < *argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif
}

/* When selection is presented back, returns 0.
 * If it can make a decision right now, returns -1. */

int rarch_defer_core(core_info_list_t *core_info, const char *dir,
      const char *path, char *deferred_path, size_t sizeof_deferred_path)
{
   const core_info_t *info = NULL;
   size_t supported = 0;

   fill_pathname_join(deferred_path, dir, path, sizeof_deferred_path);

   if (path_is_compressed_file(dir))
   {
      /* In case of a compressed archive, we have to join with a hash */
      /* We are going to write at the position of dir: */
      rarch_assert(strlen(dir) < strlen(deferred_path));
      deferred_path[strlen(dir)] = '#';
   }

   if (core_info)
      core_info_list_get_supported_cores(core_info, deferred_path, &info,
            &supported);

   /* Can make a decision right now. */
   if (supported == 1)
   {
      strlcpy(g_extern.fullpath, deferred_path,
            sizeof(g_extern.fullpath));
      if (path_file_exists(info->path))
         strlcpy(g_settings.libretro, info->path,
               sizeof(g_settings.libretro));
      return -1;
   }
   return 0;
}

/* Quite intrusive and error prone.
 * Likely to have lots of small bugs.
 * Cleanly exit the main loop to ensure that all the tiny details
 * get set properly.
 *
 * This should mitigate most of the smaller bugs. */

bool rarch_replace_config(const char *path)
{
   /* If config file to be replaced is the same as the 
    * current config file, exit. */
   if (!strcmp(path, g_extern.config_path))
      return false;

   if (g_settings.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   strlcpy(g_extern.config_path, path, sizeof(g_extern.config_path));
   g_extern.block_config_read = false;
   *g_settings.libretro = '\0'; /* Load core in new config. */

   rarch_main_command(RARCH_CMD_PREPARE_DUMMY);

   return true;
}

void rarch_update_system_info(struct retro_system_info *_info,
      bool *load_no_content)
{
   const core_info_t *info = NULL;
#if defined(HAVE_DYNAMIC)
   libretro_free_system_info(_info);
   if (!(*g_settings.libretro))
      return;

   libretro_get_system_info(g_settings.libretro, _info,
         load_no_content);
#endif
   if (!g_extern.core_info)
      return;

   if (!core_info_list_get_info(g_extern.core_info,
            g_extern.core_info_current, g_settings.libretro))
      return;

   /* Keep track of info for the currently selected core. */
   info = (const core_info_t*)g_extern.core_info_current;

   if (!g_extern.verbosity)
      return;

   RARCH_LOG("[Core Info]:\n");
   if (info->display_name)
      RARCH_LOG("Display Name = %s\n", info->display_name);
   if (info->supported_extensions)
      RARCH_LOG("Supported Extensions = %s\n",
            info->supported_extensions);
   if (info->authors)
      RARCH_LOG("Authors = %s\n", info->authors);
   if (info->permissions)
      RARCH_LOG("Permissions = %s\n", info->permissions);
}
