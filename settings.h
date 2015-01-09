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

#ifndef __RARCH_SETTINGS_H__
#define __RARCH_SETTINGS_H__

#ifdef __cplusplus
extern "C" {
#endif


const char *config_get_default_camera(void);

const char *config_get_default_location(void);

const char *config_get_default_osk(void);

const char *config_get_default_video(void);

const char *config_get_default_audio(void);

const char *config_get_default_audio_resampler(void);

const char *config_get_default_input(void);

const char *config_get_default_joypad(void);

#ifdef HAVE_MENU
const char *config_get_default_menu(void);
#endif

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void);

/**
 * config_save_file:
 * @path            : Path that shall be written to.
 *
 * Writes a config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 */
bool config_save_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif
