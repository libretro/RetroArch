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

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_VALUE_FILE_WEBM           0x7ca00b50U
#define MENU_VALUE_FILE_F4F            0x0b886be5U
#define MENU_VALUE_FILE_F4V            0x0b886bf5U
#define MENU_VALUE_FILE_OGM            0x0b8898c8U
#define MENU_VALUE_FILE_MKV            0x0b8890d3U
#define MENU_VALUE_FILE_AVI            0x0b885f25U
#define MENU_VALUE_FILE_M4A            0x0b8889a7U
#define MENU_VALUE_FILE_3GP            0x0b87998fU
#define MENU_VALUE_FILE_MP4            0x0b889136U
#define MENU_VALUE_FILE_MP3            0x0b889135U
#define MENU_VALUE_FILE_FLAC           0x7c96d67bU
#define MENU_VALUE_FILE_OGG            0x0b8898c2U
#define MENU_VALUE_FILE_FLV            0x0b88732dU
#define MENU_VALUE_FILE_WAV            0x0b88ba13U
#define MENU_VALUE_FILE_MOV            0x0b889157U
#define MENU_VALUE_FILE_WMV            0x0b88bb9fU

#define MENU_VALUE_FILE_JPG            0x0b8884a6U
#define MENU_VALUE_FILE_JPEG           0x7c99198bU
#define MENU_VALUE_FILE_JPG_CAPS       0x0b87f846U
#define MENU_VALUE_FILE_JPEG_CAPS      0x7c87010bU
#define MENU_VALUE_FILE_PNG            0x0b889deaU
#define MENU_VALUE_FILE_PNG_CAPS       0x0b88118aU
#define MENU_VALUE_FILE_TGA            0x0b88ae01U
#define MENU_VALUE_FILE_BMP            0x0b886244U

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

   RARCH_CTL_MENU_RUNNING,

   RARCH_CTL_MENU_RUNNING_FINISHED,

   RARCH_CTL_SET_PATHS_REDIRECT,

   RARCH_CTL_SET_SRAM_ENABLE,

   RARCH_CTL_SET_FORCE_FULLSCREEN,

   RARCH_CTL_UNSET_FORCE_FULLSCREEN,

   RARCH_CTL_IS_FORCE_FULLSCREEN,

   RARCH_CTL_SET_BLOCK_CONFIG_READ,

   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,

   RARCH_CTL_IS_BLOCK_CONFIG_READ,

   RARCH_CTL_SET_ERROR_ON_INIT,

   RARCH_CTL_UNSET_ERROR_ON_INIT,

   RARCH_CTL_HAS_SET_USERNAME,

   RARCH_CTL_USERNAME_SET,

   RARCH_CTL_USERNAME_UNSET,

   RARCH_CTL_IS_ERROR_ON_INIT
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

enum rarch_content_type retroarch_path_is_media_type(const char *path);

const char *retroarch_get_current_savefile_dir(void);

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir);

void retroarch_set_pathnames(const char *path);

void retroarch_fill_pathnames(void);

/* Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly. */
bool retroarch_replace_config(char *path);

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

void retroarch_main_quit(void);

#ifdef __cplusplus
}
#endif

#endif
