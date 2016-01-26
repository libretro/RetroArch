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
#include <retro_miscellaneous.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_VALUE_FILE_WEBM                                                   0x7ca00b50U
#define MENU_VALUE_FILE_F4F                                                    0x0b886be5U
#define MENU_VALUE_FILE_F4V                                                    0x0b886bf5U
#define MENU_VALUE_FILE_OGM                                                    0x0b8898c8U
#define MENU_VALUE_FILE_MKV                                                    0x0b8890d3U
#define MENU_VALUE_FILE_AVI                                                    0x0b885f25U
#define MENU_VALUE_FILE_M4A                                                    0x0b8889a7U
#define MENU_VALUE_FILE_3GP                                                    0x0b87998fU
#define MENU_VALUE_FILE_MP4                                                    0x0b889136U
#define MENU_VALUE_FILE_MP3                                                    0x0b889135U
#define MENU_VALUE_FILE_FLAC                                                   0x7c96d67bU
#define MENU_VALUE_FILE_OGG                                                    0x0b8898c2U
#define MENU_VALUE_FILE_FLV                                                    0x0b88732dU
#define MENU_VALUE_FILE_WAV                                                    0x0b88ba13U
#define MENU_VALUE_FILE_MOV                                                    0x0b889157U
#define MENU_VALUE_FILE_WMV                                                    0x0b88bb9fU

#define MENU_VALUE_FILE_JPG                                                    0x0b8884a6U
#define MENU_VALUE_FILE_JPEG                                                   0x7c99198bU
#define MENU_VALUE_FILE_JPG_CAPS                                               0x0b87f846U
#define MENU_VALUE_FILE_JPEG_CAPS                                              0x7c87010bU
#define MENU_VALUE_FILE_PNG                                                    0x0b889deaU
#define MENU_VALUE_FILE_PNG_CAPS                                               0x0b88118aU
#define MENU_VALUE_FILE_TGA                                                    0x0b88ae01U
#define MENU_VALUE_FILE_BMP                                                    0x0b886244U

enum rarch_ctl_state
{
   RARCH_CTL_NONE = 0,

   /* Will teardown drivers and clears all
    * internal state of the program. */
   RARCH_CTL_DEINIT,

   /* Initialize all drivers. */
   RARCH_CTL_INIT,

   RARCH_CTL_UNSET_INITED,

   RARCH_CTL_SET_INITED,

   RARCH_CTL_IS_INITED,

   RARCH_CTL_IS_PLAIN_CORE,

   RARCH_CTL_IS_DUMMY_CORE,

   RARCH_CTL_PREINIT,

   RARCH_CTL_DESTROY,

   RARCH_CTL_LOAD_CONTENT,

   RARCH_CTL_LOAD_CONTENT_FFMPEG,

   RARCH_CTL_LOAD_CONTENT_IMAGEVIEWER,

   RARCH_CTL_MENU_RUNNING,

   RARCH_CTL_MENU_RUNNING_FINISHED,

   /* Replaces currently loaded configuration file with
    * another one. Will load a dummy core to flush state
    * properly. */
   RARCH_CTL_REPLACE_CONFIG,

   RARCH_CTL_QUIT,

   RARCH_CTL_FORCE_QUIT,

   /* Compare libretro core API version against API version
    * used by RetroArch.
    *
    * TODO - when libretro v2 gets added, allow for switching
    * between libretro version backend dynamically.
    */
   RARCH_CTL_VERIFY_API_VERSION,

   /* Validates CPU features for given processor architecture.
    * Make sure we haven't compiled for something we cannot run.
    * Ideally, code would get swapped out depending on CPU support,
    * but this will do for now. */
   RARCH_CTL_VALIDATE_CPU_FEATURES,

   /**
    * Initialize system A/V information by calling the libretro core's
    * get_system_av_info function.
    **/
   RARCH_CTL_INIT_SYSTEM_AV_INFO,

   RARCH_CTL_FILL_PATHNAMES,

   RARCH_CTL_SET_PATHS_REDIRECT,

   RARCH_CTL_SET_FORCE_FULLSCREEN,

   RARCH_CTL_UNSET_FORCE_FULLSCREEN,

   RARCH_CTL_IS_FORCE_FULLSCREEN,

   RARCH_CTL_SET_BLOCK_CONFIG_READ,

   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,

   RARCH_CTL_IS_BLOCK_CONFIG_READ,

   RARCH_CTL_SET_ERROR_ON_INIT,

   RARCH_CTL_UNSET_ERROR_ON_INIT,

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

/**
 * rarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes RetroArch.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
int rarch_main_init(int argc, char *argv[]);

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void);

void rarch_set_paths(const char *path);

int rarch_info_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len);

enum rarch_content_type rarch_path_is_media_type(const char *path);

const char *rarch_get_current_savefile_dir(void);

bool rarch_option_create(char *path, size_t len);

#ifdef __cplusplus
}
#endif

#endif
