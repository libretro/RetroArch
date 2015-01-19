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

#ifndef __DRIVER__H
#define __DRIVER__H

#include <sys/types.h>
#include <boolean.h>
#include "libretro_private.h"
#include <stdlib.h>
#include <stdint.h>
#include <compat/posix_string.h>

#include <retro_miscellaneous.h>

#include "frontend/frontend_driver.h"
#include "gfx/video_driver.h"
#include "audio/audio_driver.h"

#include "menu/menu_driver.h"
#include "osk/osk_driver.h"
#include "camera/camera_driver.h"
#include "location/location_driver.h"
#include "audio/audio_resampler_driver.h"
#include "record/record_driver.h"

#include "libretro_version_1.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_CHUNK_SIZE_BLOCKING 512

/* So we don't get complete line-noise when fast-forwarding audio. */
#define AUDIO_CHUNK_SIZE_NONBLOCKING 2048

#define AUDIO_MAX_RATIO 16

/* Specialized _POINTER that targets the full screen regardless of viewport.
 * Should not be used by a libretro implementation as coordinates returned
 * make no sense.
 *
 * It is only used internally for overlays. */
#define RARCH_DEVICE_POINTER_SCREEN (RETRO_DEVICE_POINTER | 0x10000)

/* libretro has 16 buttons from 0-15 (libretro.h)
 * Analog binds use RETRO_DEVICE_ANALOG, but we follow the same scheme
 * internally in RetroArch for simplicity, so they are mapped into [16, 23].
 */
#define RARCH_FIRST_CUSTOM_BIND 16
#define RARCH_FIRST_META_KEY RARCH_CUSTOM_BIND_LIST_END

/* RetroArch specific bind IDs. */
enum
{
   /* Custom binds that extend the scope of RETRO_DEVICE_JOYPAD for
    * RetroArch specifically.
    * Analogs (RETRO_DEVICE_ANALOG) */
   RARCH_ANALOG_LEFT_X_PLUS = RARCH_FIRST_CUSTOM_BIND,
   RARCH_ANALOG_LEFT_X_MINUS,
   RARCH_ANALOG_LEFT_Y_PLUS,
   RARCH_ANALOG_LEFT_Y_MINUS,
   RARCH_ANALOG_RIGHT_X_PLUS,
   RARCH_ANALOG_RIGHT_X_MINUS,
   RARCH_ANALOG_RIGHT_Y_PLUS,
   RARCH_ANALOG_RIGHT_Y_MINUS,

   /* Turbo */
   RARCH_TURBO_ENABLE,

   RARCH_CUSTOM_BIND_LIST_END,

   /* Command binds. Not related to game input,
    * only usable for port 0. */
   RARCH_FAST_FORWARD_KEY = RARCH_FIRST_META_KEY,
   RARCH_FAST_FORWARD_HOLD_KEY,
   RARCH_LOAD_STATE_KEY,
   RARCH_SAVE_STATE_KEY,
   RARCH_FULLSCREEN_TOGGLE_KEY,
   RARCH_QUIT_KEY,
   RARCH_STATE_SLOT_PLUS,
   RARCH_STATE_SLOT_MINUS,
   RARCH_REWIND,
   RARCH_MOVIE_RECORD_TOGGLE,
   RARCH_PAUSE_TOGGLE,
   RARCH_FRAMEADVANCE,
   RARCH_RESET,
   RARCH_SHADER_NEXT,
   RARCH_SHADER_PREV,
   RARCH_CHEAT_INDEX_PLUS,
   RARCH_CHEAT_INDEX_MINUS,
   RARCH_CHEAT_TOGGLE,
   RARCH_SCREENSHOT,
   RARCH_MUTE,
   RARCH_NETPLAY_FLIP,
   RARCH_SLOWMOTION,
   RARCH_ENABLE_HOTKEY,
   RARCH_VOLUME_UP,
   RARCH_VOLUME_DOWN,
   RARCH_OVERLAY_NEXT,
   RARCH_DISK_EJECT_TOGGLE,
   RARCH_DISK_NEXT,
   RARCH_DISK_PREV,
   RARCH_GRAB_MOUSE_TOGGLE,

   RARCH_MENU_TOGGLE,

   RARCH_BIND_LIST_END,
   RARCH_BIND_LIST_END_NULL
};

#define AXIS_NEG(x) (((uint32_t)(x) << 16) | UINT16_C(0xFFFF))
#define AXIS_POS(x) ((uint32_t)(x) | UINT32_C(0xFFFF0000))
#define AXIS_NONE UINT32_C(0xFFFFFFFF)
#define AXIS_DIR_NONE UINT16_C(0xFFFF)

#define AXIS_NEG_GET(x) (((uint32_t)(x) >> 16) & UINT16_C(0xFFFF))
#define AXIS_POS_GET(x) ((uint32_t)(x) & UINT16_C(0xFFFF))

#define NO_BTN UINT16_C(0xFFFF)

#define HAT_UP_SHIFT 15
#define HAT_DOWN_SHIFT 14
#define HAT_LEFT_SHIFT 13
#define HAT_RIGHT_SHIFT 12
#define HAT_UP_MASK (1 << HAT_UP_SHIFT)
#define HAT_DOWN_MASK (1 << HAT_DOWN_SHIFT)
#define HAT_LEFT_MASK (1 << HAT_LEFT_SHIFT)
#define HAT_RIGHT_MASK (1 << HAT_RIGHT_SHIFT)
#define HAT_MAP(x, hat) ((x & ((1 << 12) - 1)) | hat)

#define HAT_MASK (HAT_UP_MASK | HAT_DOWN_MASK | HAT_LEFT_MASK | HAT_RIGHT_MASK)
#define GET_HAT_DIR(x) (x & HAT_MASK)
#define GET_HAT(x) (x & (~HAT_MASK))

enum analog_dpad_mode
{
   ANALOG_DPAD_NONE = 0,
   ANALOG_DPAD_LSTICK,
   ANALOG_DPAD_RSTICK,
   ANALOG_DPAD_LAST
};

/* Flags for init_drivers/uninit_drivers */
enum
{
   DRIVER_AUDIO        = 1 << 0,
   DRIVER_VIDEO        = 1 << 1,
   DRIVER_INPUT        = 1 << 2,
   DRIVER_OSK          = 1 << 3,
   DRIVER_CAMERA       = 1 << 4,
   DRIVER_LOCATION     = 1 << 5,
   DRIVER_MENU         = 1 << 6,
   DRIVERS_VIDEO_INPUT = 1 << 7
};

/* Drivers for RARCH_CMD_DRIVERS_DEINIT and RARCH_CMD_DRIVERS_INIT */
#define DRIVERS_CMD_ALL \
      ( DRIVER_AUDIO \
      | DRIVER_VIDEO \
      | DRIVER_INPUT \
      | DRIVER_OSK \
      | DRIVER_CAMERA \
      | DRIVER_LOCATION \
      | DRIVER_MENU \
      | DRIVERS_VIDEO_INPUT )

typedef struct driver
{
   const frontend_ctx_driver_t *frontend_ctx;
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
   const input_osk_driver_t *osk;
   const camera_driver_t *camera;
   const location_driver_t *location;
   const rarch_resampler_t *resampler;
   const ffemu_backend_t *recording;
   struct retro_callbacks retro_ctx;

   void *audio_data;
   void *video_data;
   void *video_context_data;
   void *video_shader_data;
   void *input_data;
   void *osk_data;
   void *camera_data;
   void *location_data;
   void *resampler_data;
   void *recording_data;
#ifdef HAVE_NETPLAY
   void *netplay_data;
#endif

   bool audio_active;
   bool video_active;
   bool camera_active;
   bool location_active;
   bool osk_active;

#ifdef HAVE_MENU
   menu_handle_t *menu;
   const menu_ctx_driver_t *menu_ctx;
#endif
   bool threaded_video;

   /* If set during context deinit, the driver should keep
    * graphics context alive to avoid having to reset all 
    * context state. */
   bool video_cache_context;

   /* Set to true by driver if context caching succeeded. */
   bool video_cache_context_ack;

   /* Set this to true if the platform in question needs to 'own' 
    * the respective handle and therefore skip regular RetroArch 
    * driver teardown/reiniting procedure.
    *
    * If set to true, the 'free' function will get skipped. It is 
    * then up to the driver implementation to properly handle 
    * 'reiniting' inside the 'init' function and make sure it 
    * returns the existing handle instead of allocating and 
    * returning a pointer to a new handle.
    *
    * Typically, if a driver intends to make use of this, it should 
    * set this to true at the end of its 'init' function. */
   bool video_data_own;
   bool audio_data_own;
   bool input_data_own;
   bool camera_data_own;
   bool location_data_own;
   bool osk_data_own;
#ifdef HAVE_MENU
   bool menu_data_own;
#endif

#ifdef HAVE_COMMAND
   rarch_cmd_t *command;
#endif
   bool stdin_claimed;
   bool block_hotkey;
   bool block_input;
   bool block_libretro_input;
   bool flushing_input;
   bool nonblock_state;

   /* Opaque handles to currently running window.
    * Used by e.g. input drivers which bind to a window.
    * Drivers are responsible for setting these if an input driver
    * could potentially make use of this. */
   uintptr_t video_display;
   uintptr_t video_window;
   enum rarch_display_type display_type;

   /* Used for 15-bit -> 16-bit conversions that take place before
    * being passed to video driver. */
   struct scaler_ctx scaler;
   void *scaler_out;

   /* Graphics driver requires RGBA byte order data (ABGR on little-endian)
    * for 32-bit.
    * This takes effect for overlay and shader cores that wants to load
    * data into graphics driver. Kinda hackish to place it here, it is only
    * used for GLES.
    * TODO: Refactor this better. */
   bool gfx_use_rgba;

#ifdef HAVE_OVERLAY
   input_overlay_t *overlay;
   input_overlay_state_t overlay_state;
#endif

   /* Interface for "poking". */
   const video_poke_interface_t *video_poke;

   /* Last message given to the video driver */
   const char *current_msg;
} driver_t;

/**
 * init_drivers:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
void init_drivers(int flags);

/**
 * init_drivers_pre:
 *
 * Attempts to find a default driver for 
 * all driver types.
 *
 * Should be run before init_drivers().
 **/
void init_drivers_pre(void);

/**
 * uninit_drivers:
 * @flags              : Bitmask of drivers to deinitialize.
 *
 * Deinitializes drivers.
 * @flags determines which drivers get deinitialized.
 **/
void uninit_drivers(int flags);

/**
 * find_prev_driver:
 * @label              : string of driver type to be found.
 * @str                : identifier of driver to be found.
 * @sizeof_str         : size of @str.
 *
 * Find previous driver in driver array.
 **/
void find_prev_driver(const char *label, char *str, size_t sizeof_str);

/**
 * find_next_driver:
 * @label              : string of driver type to be found.
 * @str                : identifier of driver to be found.
 * @sizeof_str         : size of @str.
 *
 * Find next driver in driver array.
 **/
void find_next_driver(const char *label, char *str, size_t sizeof_str);

/**
 * driver_set_nonblock_state:
 * @enable             : Enable nonblock state?
 *
 * Sets audio and video drivers to nonblock state.
 *
 * If @enable is false, sets blocking state for both
 * audio and video drivers instead.
 **/
void driver_set_nonblock_state(bool enable);

/**
 * driver_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value by calling
 * video_monitor_set_refresh_rate(). Subsequently
 * calls audio_monitor_set_refresh_rate().
 **/
void driver_set_refresh_rate(float hz);

/**
 * driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
uintptr_t driver_get_current_framebuffer(void);

retro_proc_address_t driver_get_proc_address(const char *sym);

/**
 * driver_update_system_av_info:
 * @info               : pointer to new A/V info
 *
 * Update the system Audio/Video information. 
 * Will reinitialize audio/video drivers.
 * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool driver_update_system_av_info(const struct retro_system_av_info *info);

extern driver_t driver;

/**
 * find_driver_index:
 * @label              : string of driver type to be found.
 * @drv                : identifier of driver to be found.
 *
 * Find index of the driver, based on @label.
 *
 * Returns: -1 if no driver based on @label and @drv found, otherwise
 * index number of the driver found in the array.
 **/
int find_driver_index(const char * label, const char *drv);
  
#ifdef __cplusplus
}
#endif

#endif

