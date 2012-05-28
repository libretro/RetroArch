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

#ifndef CONSOLE_EXT_H__
#define CONSOLE_EXT_H__

#define IS_TIMER_NOT_EXPIRED(handle) (handle->frame_count < g_console.timer_expiration_frame_count)
#define IS_TIMER_EXPIRED(handle) 	(!(IS_TIMER_NOT_EXPIRED(handle)))
#define SET_TIMER_EXPIRATION(handle, value) (g_console.timer_expiration_frame_count = handle->frame_count + value)

enum aspect_ratio
{
   ASPECT_RATIO_1_1 = 0,
   ASPECT_RATIO_2_1,
   ASPECT_RATIO_3_2,
   ASPECT_RATIO_3_4,
   ASPECT_RATIO_4_1,
   ASPECT_RATIO_4_3,
   ASPECT_RATIO_4_4,
   ASPECT_RATIO_5_4,
   ASPECT_RATIO_6_5,
   ASPECT_RATIO_7_9,
   ASPECT_RATIO_8_3,
   ASPECT_RATIO_8_7,
   ASPECT_RATIO_16_9,
   ASPECT_RATIO_16_10,
   ASPECT_RATIO_16_15,
   ASPECT_RATIO_19_12,
   ASPECT_RATIO_19_14,
   ASPECT_RATIO_30_17,
   ASPECT_RATIO_32_9,
   ASPECT_RATIO_AUTO,
   ASPECT_RATIO_CUSTOM,

   ASPECT_RATIO_END,
};

#define LAST_ASPECT_RATIO ASPECT_RATIO_CUSTOM

enum rotation
{
   ORIENTATION_NORMAL = 0,
   ORIENTATION_VERTICAL,
   ORIENTATION_FLIPPED,
   ORIENTATION_FLIPPED_ROTATED,
   ORIENTATION_END
};

struct aspect_ratio_elem
{
   char name[64];
   float value;
};

extern struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];

extern void rarch_set_auto_viewport(unsigned width, unsigned height);

#include "console_ext_input.h"

/*============================================================
	ROM EXTENSIONS
============================================================ */

// Get rom extensions for current library.
// Returns NULL if library doesn't have any preferences in particular.
const char *rarch_console_get_rom_ext(void);

// Transforms a library id to a name suitable as a pathname.
void rarch_console_name_from_id(char *name, size_t size);

#ifdef HAVE_ZLIB
int rarch_extract_zipfile(const char *zip_path);
#endif

/*============================================================
	INPUT EXTENSIONS
============================================================ */

const char *rarch_input_find_platform_key_label(uint64_t joykey);
uint64_t rarch_input_find_previous_platform_key(uint64_t joykey);
uint64_t rarch_input_find_next_platform_key(uint64_t joykey);

// Sets custom default keybind names (some systems emulated by the emulator
// will need different keybind names for buttons, etc.)
void rarch_input_set_default_keybind_names_for_emulator(void);

void rarch_input_set_keybind(unsigned player, unsigned keybind_action, uint64_t default_retro_joypad_id);

#ifdef HAVE_LIBRETRO_MANAGEMENT
bool rarch_manage_libretro_core(const char *full_path, const char *path, const char *exe_ext);
#endif

/*============================================================
  RetroArch
  ============================================================ */

#ifdef HAVE_RARCH_MAIN_WRAP

struct rarch_main_wrap
{
   const char *rom_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   bool verbose;
};

int rarch_main_init_wrap(const struct rarch_main_wrap *args);
void rarch_startup (const char * config_path);
#endif

#ifdef HAVE_RARCH_EXEC
void rarch_exec (void);
#endif

#ifdef HAVE_RSOUND
bool rarch_console_rsound_start(const char *ip);
void rarch_console_rsound_stop(void);
#endif

#ifdef _XBOX
wchar_t * rarch_convert_char_to_wchar(const char * str);
#endif

const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr);


#endif
