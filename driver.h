/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include "compat/msvc_compat.h"
#include "gfx/scaler/scaler.h"
#include "gfx/image/image.h"
#include "gfx/filters/softfilter.h"
#include "gfx/shader/shader_parse.h"
#include "audio/dsp_filter.h"
#include "input/overlay.h"
#include "frontend/frontend_context.h"
#ifndef _WIN32
#include "miscellaneous.h"
#endif

#include "frontend/menu/menu_driver.h"
#include "frontend/menu/backend/menu_backend.h"
#include "frontend/menu/disp/menu_display.h"
#include "audio/resamplers/resampler.h"
#include "record/ffemu.h"

#include "retro.h"

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

struct retro_keybind
{
   bool valid;
   unsigned id;
   const char *desc;
   enum retro_key key;

   /* PC only uses lower 16-bits.
    * Full 64-bit can be used for port-specific purposes,
    * like simplifying multiple binds, etc. */
   uint64_t joykey;

   /* Default key binding value - for resetting bind to default */
   uint64_t def_joykey;

   uint32_t joyaxis;
   uint32_t def_joyaxis;

   /* Used by input_{push,pop}_analog_dpad(). */
   uint32_t orig_joyaxis;
};

struct platform_bind
{
   uint64_t joykey;
   char desc[64];
};

typedef struct video_info
{
   unsigned width;
   unsigned height;
   bool fullscreen;
   bool vsync;
   bool force_aspect;
#ifdef GEKKO
   /* TODO - we can't really have driver system-specific
    * variables in here. There should be some
    * kind of publicly accessible driver implementation
    * video struct for specific things like this.
    */
   unsigned viwidth;
#endif
   bool vfilter;
   bool smooth;
   /* Maximum input size: RARCH_SCALE_BASE * input_scale */
   unsigned input_scale;
   /* Use 32bit RGBA rather than native XBGR1555. */
   bool rgb32;
} video_info_t;

typedef struct audio_driver
{
   void *(*init)(const char *device, unsigned rate, unsigned latency);
   ssize_t (*write)(void *data, const void *buf, size_t size);
   bool (*stop)(void *data);
   bool (*start)(void *data);

   /* Is the audio driver currently running? */
   bool (*alive)(void *data);

   /* Should we care about blocking in audio thread? Fast forwarding. */
   void (*set_nonblock_state)(void *data, bool toggle);
   void (*free)(void *data);

   /* Defines if driver will take standard floating point samples,
    * or int16_t samples. */
   bool (*use_float)(void *data);
   const char *ident;

   /* Optional. */
   size_t (*write_avail)(void *data);
   size_t (*buffer_size)(void *data);
} audio_driver_t;

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

typedef struct rarch_joypad_driver rarch_joypad_driver_t;

typedef struct input_driver
{
   void *(*init)(void);
   void (*poll)(void *data);
   int16_t (*input_state)(void *data,
         const struct retro_keybind **retro_keybinds,
         unsigned port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void *data, int key);
   void (*free)(void *data);
   bool (*set_sensor_state)(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate);
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);
   uint64_t (*get_capabilities)(void *data);
   const char *ident;

   void (*grab_mouse)(void *data, bool state);
   bool (*set_rumble)(void *data, unsigned port,
         enum retro_rumble_effect effect, uint16_t state);
   const rarch_joypad_driver_t *(*get_joypad_driver)(void *data);
} input_driver_t;

typedef struct input_osk_driver
{
   void *(*init)(size_t size);
   void (*free)(void *data);
   bool (*enable_key_layout)(void *data);
   void (*oskutil_create_activation_parameters)(void *data);
   void (*write_msg)(void *data, const void *msg);
   void (*write_initial_msg)(void *data, const void *msg);
   bool (*start)(void *data);
   void (*lifecycle)(void *data, uint64_t status);
   void *(*get_text_buf)(void *data);
   const char *ident;
} input_osk_driver_t;

typedef struct camera_driver
{
   /* FIXME: params for initialization - queries for resolution,
    * framerate, color format which might or might not be honored. */
   void *(*init)(const char *device, uint64_t buffer_types,
         unsigned width, unsigned height);

   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   /* Polls the camera driver.
    * Will call the appropriate callback if a new frame is ready.
    * Returns true if a new frame was handled. */
   bool (*poll)(void *data,
         retro_camera_frame_raw_framebuffer_t frame_raw_cb,
         retro_camera_frame_opengl_texture_t frame_gl_cb);

   const char *ident;
} camera_driver_t;

typedef struct location_driver
{
   void *(*init)(void);
   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   bool (*get_position)(void *data, double *lat, double *lon,
         double *horiz_accuracy, double *vert_accuracy);
   void (*set_interval)(void *data, unsigned interval_msecs,
         unsigned interval_distance);
   const char *ident;
} location_driver_t;

struct rarch_viewport;

#ifdef HAVE_OVERLAY
typedef struct video_overlay_interface
{
   void (*enable)(void *data, bool state);
   bool (*load)(void *data,
         const struct texture_image *images, unsigned num_images);
   void (*tex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*vertex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*full_screen)(void *data, bool enable);
   void (*set_alpha)(void *data, unsigned image, float mod);
} video_overlay_interface_t;
#endif

struct font_params
{
   float x;
   float y;
   float scale;
   /* Drop shadow color multiplier. */
   float drop_mod;
   /* Drop shadow offset.
    * If both are 0, no drop shadow will be rendered. */
   int drop_x, drop_y;
   /* ABGR. Use the macros. */
   uint32_t color;
   bool full_screen;
};
#define FONT_COLOR_RGBA(r, g, b, a) (((r) << 0) | ((g) << 8) | ((b) << 16) | ((a) << 24))
#define FONT_COLOR_GET_RED(col)   (((col) >>  0) & 0xff)
#define FONT_COLOR_GET_GREEN(col) (((col) >>  8) & 0xff)
#define FONT_COLOR_GET_BLUE(col)  (((col) >> 16) & 0xff)
#define FONT_COLOR_GET_ALPHA(col) (((col) >> 24) & 0xff)

/* Optionally implemented interface to poke more
 * deeply into video driver. */

typedef struct video_poke_interface
{
   void (*set_filtering)(void *data, unsigned index, bool smooth);
#ifdef HAVE_FBO
   uintptr_t (*get_current_framebuffer)(void *data);
   retro_proc_address_t (*get_proc_address)(void *data, const char *sym);
#endif
   void (*set_aspect_ratio)(void *data, unsigned aspectratio_index);
   void (*apply_state_changes)(void *data);

#ifdef HAVE_MENU
   /* Update texture. */
   void (*set_texture_frame)(void *data, const void *frame, bool rgb32,
         unsigned width, unsigned height, float alpha);

   /* Enable or disable rendering. */
   void (*set_texture_enable)(void *data, bool enable, bool full_screen);
#endif
   void (*set_osd_msg)(void *data, const char *msg,
         const struct font_params *params);

   void (*show_mouse)(void *data, bool state);
   void (*grab_mouse_toggle)(void *data);

   struct gfx_shader *(*get_current_shader)(void *data);
} video_poke_interface_t;

typedef struct video_driver
{
   /* Should the video driver act as an input driver as well?
    * The video initialization might preinitialize an input driver 
    * to override the settings in case the video driver relies on 
    * input driver for event handling. */
   void *(*init)(const video_info_t *video, const input_driver_t **input,
         void **input_data); 

   /* msg is for showing a message on the screen along with the video frame. */
   bool (*frame)(void *data, const void *frame, unsigned width,
         unsigned height, unsigned pitch, const char *msg);

   /* Should we care about syncing to vblank? Fast forwarding. */
   void (*set_nonblock_state)(void *data, bool toggle);

   /* Is the window still active? */
   bool (*alive)(void *data);

   /* Does the window have focus? */
   bool (*focus)(void *data);

   /* Does the graphics conext support windowed mode? */
   bool (*has_windowed)(void *data);

   /* Sets shader. Might not be implemented. Will be moved to
    * poke_interface later. */
   bool (*set_shader)(void *data, enum rarch_shader_type type,
         const char *path);

   void (*free)(void *data);
   const char *ident;

   void (*set_rotation)(void *data, unsigned rotation);
   void (*viewport_info)(void *data, struct rarch_viewport *vp);

   /* Reads out in BGR byte order (24bpp). */
   bool (*read_viewport)(void *data, uint8_t *buffer);

#ifdef HAVE_OVERLAY
   void (*overlay_interface)(void *data, const video_overlay_interface_t **iface);
#endif
   void (*poke_interface)(void *data, const video_poke_interface_t **iface);
   unsigned (*wrap_type_to_enum)(enum gfx_wrap_type type);
} video_driver_t;

enum rarch_display_type
{
   /* Non-bindable types like consoles, KMS, VideoCore, etc. */
   RARCH_DISPLAY_NONE = 0,
   /* video_display => Display*, video_window => Window */
   RARCH_DISPLAY_X11,
   /* video_display => N/A, video_window => HWND */
   RARCH_DISPLAY_WIN32,
   RARCH_DISPLAY_OSX
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
   DRIVERS_VIDEO_INPUT = 1 << 7 /* note multiple drivers */
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

void init_drivers(int flags);
void init_drivers_pre(void);
void uninit_drivers(int flags);

void find_prev_driver(const char *label, char *str, size_t sizeof_str);
void find_next_driver(const char *label, char *str, size_t sizeof_str);

void find_prev_resampler_driver(void);
void find_next_resampler_driver(void);

void driver_set_monitor_refresh_rate(float hz);
bool driver_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points);
void driver_set_nonblock_state(bool nonblock);

/* Used by RETRO_ENVIRONMENT_SET_HW_RENDER. */
uintptr_t driver_get_current_framebuffer(void);

retro_proc_address_t driver_get_proc_address(const char *sym);

/* Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE */
bool driver_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

/* Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE */
bool driver_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate);

float driver_sensor_get_input(unsigned port, unsigned action);

/* Use this if you need the real video driver and driver data pointers */
void *driver_video_resolve(const video_driver_t **drv);

/* Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE. */
bool driver_camera_start(void);
void driver_camera_stop(void);
void driver_camera_poll(void);

/* Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE. */
bool driver_location_start(void);
void driver_location_stop(void);
bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);
void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance);

/* Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO. */
bool driver_update_system_av_info(const struct retro_system_av_info *info);

extern driver_t driver;

/* Backends */
extern audio_driver_t audio_rsound;
extern audio_driver_t audio_oss;
extern audio_driver_t audio_alsa;
extern audio_driver_t audio_alsathread;
extern audio_driver_t audio_roar;
extern audio_driver_t audio_openal;
extern audio_driver_t audio_opensl;
extern audio_driver_t audio_jack;
extern audio_driver_t audio_sdl;
extern audio_driver_t audio_xa;
extern audio_driver_t audio_pulse;
extern audio_driver_t audio_dsound;
extern audio_driver_t audio_coreaudio;
extern audio_driver_t audio_xenon360;
extern audio_driver_t audio_ps3;
extern audio_driver_t audio_gx;
extern audio_driver_t audio_psp1;
extern audio_driver_t audio_rwebaudio;
extern audio_driver_t audio_null;

extern video_driver_t video_gl;
extern video_driver_t video_psp1;
extern video_driver_t video_vita;
extern video_driver_t video_d3d;
extern video_driver_t video_gx;
extern video_driver_t video_xenon360;
extern video_driver_t video_xvideo;
extern video_driver_t video_xdk_d3d;
extern video_driver_t video_sdl;
extern video_driver_t video_sdl2;
extern video_driver_t video_vg;
extern video_driver_t video_omap;
extern video_driver_t video_exynos;
extern video_driver_t video_null;

extern input_driver_t input_android;
extern input_driver_t input_sdl;
extern input_driver_t input_dinput;
extern input_driver_t input_x;
extern input_driver_t input_wayland;
extern input_driver_t input_ps3;
extern input_driver_t input_psp;
extern input_driver_t input_xenon360;
extern input_driver_t input_gx;
extern input_driver_t input_xinput;
extern input_driver_t input_linuxraw;
extern input_driver_t input_udev;
extern input_driver_t input_apple;
extern input_driver_t input_qnx;
extern input_driver_t input_rwebinput;
extern input_driver_t input_null;

extern camera_driver_t camera_v4l2;
extern camera_driver_t camera_android;
extern camera_driver_t camera_rwebcam;
extern camera_driver_t camera_ios;
extern camera_driver_t camera_null;

extern location_driver_t location_apple;
extern location_driver_t location_android;
extern location_driver_t location_null;

extern input_osk_driver_t input_ps3_osk;
extern input_osk_driver_t input_null_osk;

extern menu_ctx_driver_t menu_ctx_rmenu;
extern menu_ctx_driver_t menu_ctx_rmenu_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_glui;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_lakka;

extern menu_ctx_driver_backend_t menu_ctx_backend_common;
extern menu_ctx_driver_backend_t menu_ctx_backend_lakka;

extern rarch_joypad_driver_t *joypad_drivers[];

#define check_overlay_func(input, old_input) rarch_check_overlay(BIT64_GET(input, RARCH_OVERLAY_NEXT), BIT64_GET(old_input, RARCH_OVERLAY_NEXT))
#define check_oneshot_func(trigger_input) (check_is_oneshot(BIT64_GET(trigger_input, RARCH_FRAMEADVANCE), BIT64_GET(trigger_input, RARCH_REWIND)))
#define check_slowmotion_func(input) check_slowmotion(BIT64_GET(input, RARCH_SLOWMOTION))
#define check_shader_dir_func(trigger_input) check_shader_dir(BIT64_GET(trigger_input, RARCH_SHADER_NEXT), BIT64_GET(trigger_input, RARCH_SHADER_PREV))
#define check_enter_menu_func(input) BIT64_GET(input, RARCH_MENU_TOGGLE)
#define check_mute_func(input, old_input) check_mute(BIT64_GET(input, RARCH_MUTE), BIT64_GET(old_input, RARCH_MUTE))
#define check_fast_forward_button_func(input, old_input, trigger_input) check_fast_forward_button(BIT64_GET(trigger_input, RARCH_FAST_FORWARD_KEY), BIT64_GET(input, RARCH_FAST_FORWARD_HOLD_KEY), BIT64_GET(old_input, RARCH_FAST_FORWARD_HOLD_KEY))
#define check_rewind_func(input) check_rewind(BIT64_GET(input, RARCH_REWIND))
#define check_stateslots_func(trigger_input) check_stateslots(BIT64_GET(trigger_input, RARCH_STATE_SLOT_PLUS), BIT64_GET(trigger_input, RARCH_STATE_SLOT_MINUS))
#define check_pause_func(input) check_pause(BIT64_GET(input, RARCH_PAUSE_TOGGLE), BIT64_GET(input, RARCH_FRAMEADVANCE))
#define check_quit_key_func(input) BIT64_GET(input, RARCH_QUIT_KEY)

#ifdef __cplusplus
}
#endif

#endif

