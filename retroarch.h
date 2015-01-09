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

#ifdef __cplusplus
extern "C" {
#endif

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

void rarch_main_state_new(void);

void rarch_main_state_free(void);

void rarch_main_set_state(unsigned action);

bool rarch_main_command(unsigned action);

int rarch_main_init(int argc, char *argv[]);

void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv);

void rarch_main_deinit(void);

void rarch_render_cached_frame(void);

void rarch_disk_control_set_eject(bool state, bool log);

void rarch_disk_control_set_index(unsigned index);

void rarch_disk_control_append_image(const char *path);

void rarch_recording_dump_frame(const void *data, unsigned width,
      unsigned height, size_t pitch);

bool rarch_replace_config(const char *path);

void rarch_playlist_load_content(content_playlist_t *playlist,
      unsigned index);

int rarch_defer_core(core_info_list_t *data,
      const char *dir, const char *path, const char *menu_label,
      char *deferred_path, size_t sizeof_deferred_path);

void rarch_update_system_info(struct retro_system_info *info,
      bool *load_no_content);

#ifdef __cplusplus
}
#endif

#endif
