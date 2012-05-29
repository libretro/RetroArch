/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _PS3_SHARED_H
#define _PS3_SHARED_H

/* ABGR color format */

#define WHITE		0xffffffffu
#define RED		0xff0000ffu
#define GREEN		0xff00ff00u
#define BLUE		0xffff0000u
#define YELLOW		0xff00ffffu
#define PURPLE		0xffff00ffu
#define CYAN		0xffffff00u
#define ORANGE		0xff0063ffu
#define SILVER		0xff8c848cu
#define LIGHTBLUE	0xFFFFE0E0U
#define LIGHTORANGE	0xFFE0EEFFu

enum
{
   EXTERN_LAUNCHER_SALAMANDER,
   EXTERN_LAUNCHER_MULTIMAN
};

enum
{
   CONFIG_FILE,
   SHADER_PRESET_FILE,
   INPUT_PRESET_FILE
};

enum
{
   SOUND_MODE_NORMAL,
   SOUND_MODE_RSOUND,
   SOUND_MODE_HEADSET
};

extern char contentInfoPath[PATH_MAX];
extern char usrDirPath[PATH_MAX];
extern char DEFAULT_PRESET_FILE[PATH_MAX];
extern char DEFAULT_BORDER_FILE[PATH_MAX];
extern char DEFAULT_MENU_BORDER_FILE[PATH_MAX];
extern char PRESETS_DIR_PATH[PATH_MAX];
extern char INPUT_PRESETS_DIR_PATH[PATH_MAX];
extern char BORDERS_DIR_PATH[PATH_MAX];
extern char SHADERS_DIR_PATH[PATH_MAX];
extern char DEFAULT_SHADER_FILE[PATH_MAX];
extern char DEFAULT_MENU_SHADER_FILE[PATH_MAX];
extern char LIBRETRO_DIR_PATH[PATH_MAX];
extern char SYS_CONFIG_FILE[PATH_MAX];
extern char MULTIMAN_EXECUTABLE[PATH_MAX];

#endif
