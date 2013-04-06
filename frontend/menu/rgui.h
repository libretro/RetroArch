/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#ifndef RGUI_H__
#define RGUI_H__

#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#else
extern "C" {
#endif

typedef enum
{
   RGUI_FILE_PLAIN,
   RGUI_FILE_DIRECTORY,
   RGUI_FILE_DEVICE,
   RGUI_SETTINGS,

   // settings options are done here too
   RGUI_SETTINGS_OPEN_FILEBROWSER,
   RGUI_SETTINGS_CORE_OPTIONS,
   RGUI_SETTINGS_REWIND_ENABLE,
   RGUI_SETTINGS_REWIND_GRANULARITY,
   RGUI_SETTINGS_SAVESTATE_SAVE,
   RGUI_SETTINGS_SAVESTATE_LOAD,
#ifdef HAVE_SCREENSHOTS
   RGUI_SETTINGS_SCREENSHOT,
#endif
   RGUI_SETTINGS_RESTART_GAME,
   RGUI_SETTINGS_VIDEO_FILTER,
   RGUI_SETTINGS_VIDEO_SOFT_FILTER,
#ifdef GEKKO
   RGUI_SETTINGS_VIDEO_RESOLUTION,
#endif
   RGUI_SETTINGS_VIDEO_GAMMA,
   RGUI_SETTINGS_VIDEO_ASPECT_RATIO,
   RGUI_SETTINGS_CUSTOM_VIEWPORT,
   RGUI_SETTINGS_CUSTOM_VIEWPORT_2,
   RGUI_SETTINGS_VIDEO_ROTATION,
   RGUI_SETTINGS_AUDIO_MUTE,
   RGUI_SETTINGS_AUDIO_CONTROL_RATE,
   RGUI_SETTINGS_RESAMPLER_TYPE,
   RGUI_SETTINGS_ZIP_EXTRACT,
   RGUI_SETTINGS_SRAM_DIR,
   RGUI_SETTINGS_STATE_DIR,
   RGUI_SETTINGS_CORE,
   RGUI_SETTINGS_CONTROLLER_1,
   RGUI_SETTINGS_CONTROLLER_2,
   RGUI_SETTINGS_CONTROLLER_3,
   RGUI_SETTINGS_CONTROLLER_4,
   RGUI_SETTINGS_DEBUG_TEXT,
   RGUI_SETTINGS_RESTART_EMULATOR,
   RGUI_SETTINGS_RESUME_GAME,
   RGUI_SETTINGS_QUIT_RARCH,

   RGUI_SETTINGS_BIND_DEVICE,
   RGUI_SETTINGS_BIND_DPAD_EMULATION,
   RGUI_SETTINGS_BIND_UP,
   RGUI_SETTINGS_BIND_DOWN,
   RGUI_SETTINGS_BIND_LEFT,
   RGUI_SETTINGS_BIND_RIGHT,
   RGUI_SETTINGS_BIND_A,
   RGUI_SETTINGS_BIND_B,
   RGUI_SETTINGS_BIND_X,
   RGUI_SETTINGS_BIND_Y,
   RGUI_SETTINGS_BIND_START,
   RGUI_SETTINGS_BIND_SELECT,
   RGUI_SETTINGS_BIND_L,
   RGUI_SETTINGS_BIND_R,
   RGUI_SETTINGS_BIND_L2,
   RGUI_SETTINGS_BIND_R2,
   RGUI_SETTINGS_BIND_L3,
   RGUI_SETTINGS_BIND_R3,

   RGUI_SETTINGS_CORE_OPTION_NONE = 0xffff,
   RGUI_SETTINGS_CORE_OPTION_START = 0x10000
} rgui_file_type_t;

typedef enum
{
   RGUI_ACTION_UP,
   RGUI_ACTION_DOWN,
   RGUI_ACTION_LEFT,
   RGUI_ACTION_RIGHT,
   RGUI_ACTION_OK,
   RGUI_ACTION_CANCEL,
   RGUI_ACTION_REFRESH,
   RGUI_ACTION_SETTINGS,
   RGUI_ACTION_START,
   RGUI_ACTION_MESSAGE,
   RGUI_ACTION_NOOP
} rgui_action_t;

typedef struct rgui_handle rgui_handle_t;

typedef void (*rgui_file_enum_cb_t)(void *ctx,
      const char *path, unsigned file_type, size_t directory_ptr);
typedef bool (*rgui_folder_enum_cb_t)(const char *directory,
      rgui_file_enum_cb_t file_cb, void *userdata, void *ctx);

extern unsigned RGUI_WIDTH;
extern unsigned RGUI_HEIGHT;

rgui_handle_t *rgui_init(const char *base_path,
      uint16_t *framebuf, size_t framebuf_pitch,
      const uint8_t *font_bmp_buf, const uint8_t *font_bin_buf);

int rgui_iterate(rgui_handle_t *rgui, rgui_action_t action);

void rgui_free(rgui_handle_t *rgui);

void menu_init(void);
bool menu_iterate(void);
void menu_free(void);

#ifdef __cplusplus
}
#endif
#endif

