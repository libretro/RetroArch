/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "core_info.h"
#include "command_event.h"

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
   RARCH_ACTION_STATE_NONE = 0,

   RARCH_ACTION_STATE_LOAD_CONTENT,

#ifdef HAVE_FFMPEG
   RARCH_ACTION_STATE_LOAD_CONTENT_FFMPEG,
#endif

   RARCH_ACTION_STATE_LOAD_CONTENT_IMAGEVIEWER,

   RARCH_ACTION_STATE_MENU_RUNNING,

   RARCH_ACTION_STATE_MENU_RUNNING_FINISHED,

   /* Replaces currently loaded configuration file with
    * another one. Will load a dummy core to flush state
    * properly. */
   RARCH_ACTION_STATE_REPLACE_CONFIG,

   RARCH_ACTION_STATE_QUIT,

   RARCH_ACTION_STATE_FORCE_QUIT,

   /* Compare libretro core API version against API version
    * used by RetroArch.
    *
    * TODO - when libretro v2 gets added, allow for switching
    * between libretro version backend dynamically.
    */
   RARCH_ACTION_STATE_VERIFY_API_VERSION,

      /* Validates CPU features for given processor architecture.
       *
       * Make sure we haven't compiled for something we cannot run.
       * Ideally, code would get swapped out depending on CPU support,
       * but this will do for now. */
   RARCH_ACTION_STATE_VALIDATE_CPU_FEATURES,

   RARCH_ACTION_STATE_FILL_PATHNAMES
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

void rarch_main_alloc(void);

void rarch_main_new(void);

void rarch_main_free(void);

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
 * rarch_main_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv);

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void);

/**
 * rarch_playlist_load_content:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
void rarch_playlist_load_content(void *data, unsigned index);

/**
 * rarch_defer_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @s                    : Deferred core path. Will be filled in
 *                         by function.
 * @len                  : Size of @s.
 *
 * Gets deferred core.
 *
 * Returns: 0 if there are multiple deferred cores and a
 * selection needs to be made from a list, otherwise
 * returns -1 and fills in @s with path to core.
 **/
int rarch_defer_core(core_info_list_t *data,
      const char *dir, const char *path, const char *menu_label,
      char *s, size_t len);

/**
 * rarch_init_system_av_info:
 *
 * Initialize system A/V information by calling the libretro core's
 * get_system_av_info function.
 **/
void rarch_init_system_av_info(void);

void rarch_set_paths(const char *path);

void set_paths_redirect(const char *path);

int rarch_info_get_capabilities(enum rarch_capabilities type, char *s, size_t len);

enum rarch_content_type rarch_path_is_media_type(const char *path);

const char *rarch_get_current_savefile_dir(void);

#ifdef __cplusplus
}
#endif

#endif
