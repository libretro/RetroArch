/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RETROARCH_H
#define __RETROARCH_H

#include <boolean.h>

#include <retro_common_api.h>

#include "core_type.h"

RETRO_BEGIN_DECLS

enum rarch_ctl_state
{
   RARCH_CTL_NONE = 0,

   /* Will teardown drivers and clears all
    * internal state of the program. */
   RARCH_CTL_DEINIT,

   /* Initialize all drivers. */
   RARCH_CTL_INIT,

   /* Deinitializes RetroArch. */
   RARCH_CTL_MAIN_DEINIT,

   RARCH_CTL_UNSET_INITED,

   RARCH_CTL_SET_INITED,

   RARCH_CTL_IS_INITED,

   RARCH_CTL_IS_PLAIN_CORE,

   RARCH_CTL_IS_DUMMY_CORE,

   RARCH_CTL_PREINIT,

   RARCH_CTL_DESTROY,

   /* Menu running? */
   RARCH_CTL_MENU_RUNNING,
   RARCH_CTL_MENU_RUNNING_FINISHED,

   RARCH_CTL_SET_PATHS_REDIRECT,

   RARCH_CTL_SET_SRAM_ENABLE,

   /* Force fullscreen */
   RARCH_CTL_SET_FORCE_FULLSCREEN,
   RARCH_CTL_UNSET_FORCE_FULLSCREEN,
   RARCH_CTL_IS_FORCE_FULLSCREEN,

   /* Block config read */
   RARCH_CTL_SET_BLOCK_CONFIG_READ,
   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,
   RARCH_CTL_IS_BLOCK_CONFIG_READ,

   /* Error */
   RARCH_CTL_SET_ERROR_ON_INIT,
   RARCH_CTL_UNSET_ERROR_ON_INIT,
   RARCH_CTL_IS_ERROR_ON_INIT,

   /* Username */
   RARCH_CTL_HAS_SET_USERNAME,
   RARCH_CTL_USERNAME_SET,
   RARCH_CTL_USERNAME_UNSET

};

enum rarch_content_type
{
   RARCH_CONTENT_NONE = 0,
   RARCH_CONTENT_MOVIE,
   RARCH_CONTENT_MUSIC,
   RARCH_CONTENT_IMAGE
};

enum rarch_capabilities
{
   RARCH_CAPABILITIES_NONE = 0,
   RARCH_CAPABILITIES_CPU,
   RARCH_CAPABILITIES_COMPILER
};

enum rarch_override_setting
{
   RARCH_OVERRIDE_SETTING_NONE = 0,
   RARCH_OVERRIDE_SETTING_LIBRETRO,
   RARCH_OVERRIDE_SETTING_VERBOSITY,
   RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY,
   RARCH_OVERRIDE_SETTING_SAVE_PATH,
   RARCH_OVERRIDE_SETTING_STATE_PATH,
   RARCH_OVERRIDE_SETTING_NETPLAY_MODE,
   RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS,
   RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT,
   RARCH_OVERRIDE_SETTING_NETPLAY_DELAY_FRAMES,
   RARCH_OVERRIDE_SETTING_UPS_PREF,
   RARCH_OVERRIDE_SETTING_BPS_PREF,
   RARCH_OVERRIDE_SETTING_IPS_PREF,
   RARCH_OVERRIDE_SETTING_LAST
};

struct rarch_main_wrap
{
   int argc;
   char **argv;
   const char *content_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   const char *libretro_path;
   bool verbose;
   bool no_content;

   bool touched;
};


bool rarch_ctl(enum rarch_ctl_state state, void *data);

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len);

void retroarch_override_setting_set(enum rarch_override_setting enum_idx);

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx);

void retroarch_override_setting_free_state(void);

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx);

enum rarch_content_type retroarch_path_is_media_type(const char *path);

const char *retroarch_get_current_savefile_dir(void);

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir);

void retroarch_set_pathnames(const char *path);

void retroarch_fill_pathnames(void);

void retroarch_set_current_core_type(enum rarch_core_type type, bool explicitly_set);

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
void retroarch_fail(int error_code, const char *error);

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * Returns: 1 (true) on success, otherwise false (0) if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[]);

RETRO_END_DECLS

#endif
