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

#ifdef HAVE_LIBRETRO_MANAGEMENT
#include "libretro_mgmt.h"
#endif

#include "../general.h"
#include "console_settings.h"

#define IS_TIMER_NOT_EXPIRED(handle) (handle->frame_count < g_console.timer_expiration_frame_count)
#define IS_TIMER_EXPIRED(handle) 	(!(IS_TIMER_NOT_EXPIRED(handle)))
#define SET_TIMER_EXPIRATION(handle, value) (g_console.timer_expiration_frame_count = handle->frame_count + value)

/*============================================================
	VIDEO
============================================================ */

#define MIN_SCALING_FACTOR (1.0f)

#if defined(__CELLOS_LV2__)
#define MAX_SCALING_FACTOR (5.0f)
#else
#define MAX_SCALING_FACTOR (2.0f)
#endif

enum
{
   FBO_DEINIT = 0,
   FBO_INIT,
   FBO_REINIT
};

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

#define LAST_ORIENTATION (ORIENTATION_END-1)

extern char rotation_lut[ASPECT_RATIO_END][PATH_MAX];

/* ABGR color format defines */

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

struct aspect_ratio_elem
{
   char name[64];
   float value;
};

extern struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];

extern void rarch_set_auto_viewport(unsigned width, unsigned height);

#include "console_ext_input.h"

/*============================================================
	SOUND
============================================================ */

enum
{
   SOUND_MODE_NORMAL,
#ifdef HAVE_RSOUND
   SOUND_MODE_RSOUND,
#endif
#ifdef HAVE_HEADSET
   SOUND_MODE_HEADSET,
#endif
};

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

#ifdef HAVE_DEFAULT_RETROPAD_INPUT
const char *rarch_input_find_platform_key_label(uint64_t joykey);
uint64_t rarch_input_find_previous_platform_key(uint64_t joykey);
uint64_t rarch_input_find_next_platform_key(uint64_t joykey);

// Sets custom default keybind names (some systems emulated by the emulator
// will need different keybind names for buttons, etc.)
void rarch_input_set_default_keybind_names_for_emulator(void);
void rarch_input_set_default_keybinds(unsigned player);

void rarch_input_set_keybind(unsigned player, unsigned keybind_action, uint64_t default_retro_joypad_id);

void rarch_input_set_controls_default (void);
const char *rarch_input_get_default_keybind_name (unsigned id);
#endif


/*============================================================
  RetroArch
  ============================================================ */

enum {
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
#ifdef __CELLOS_LV2__
   MENU_ITEM_SCALE_FACTOR,
#endif
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
#ifdef __CELLOS_LV2__
   MENU_ITEM_RETURN_TO_MENU,
   MENU_ITEM_CHANGE_LIBRETRO,
   MENU_ITEM_RETURN_TO_MULTIMAN,
#endif
   MENU_ITEM_RETURN_TO_DASHBOARD
};

#define MENU_ITEM_LAST MENU_ITEM_RETURN_TO_DASHBOARD+1

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

void rarch_convert_char_to_wchar(wchar_t *buf, const char * str, size_t size);
const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr);

void rarch_config_create_default(const char * conf_name);
void rarch_config_load(const char * conf_name, const char * libretro_dir_path, const char * exe_ext, bool find_libretro_path);
void rarch_config_save(const char * conf_name);

#endif
