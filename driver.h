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
#include "boolean.h"
#include "libretro_private.h"
#include <stdlib.h>
#include <stdint.h>
#include "msvc/msvc_compat.h"
#include "gfx/scaler/scaler.h"
#include "gfx/image/image.h"
#include "gfx/filters/softfilter.h"
#include "gfx/shader_parse.h"
#include "audio/dsp_filter.h"
#include "input/overlay.h"
#include "frontend/frontend_context.h"
#ifndef _WIN32
#include "miscellaneous.h"
#endif

#include "driver_menu.h"

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
#define AUDIO_CHUNK_SIZE_NONBLOCKING 2048 // So we don't get complete line-noise when fast-forwarding audio.
#define AUDIO_MAX_RATIO 16

// Specialized _POINTER that targets the full screen regardless of viewport.
// Should not be used by a libretro implementation as coordinates returned make no sense.
// It is only used internally for overlays.
#define RARCH_DEVICE_POINTER_SCREEN (RETRO_DEVICE_POINTER | 0x10000)

// libretro has 16 buttons from 0-15 (libretro.h)
// Analog binds use RETRO_DEVICE_ANALOG, but we follow the same scheme internally
// in RetroArch for simplicity,
// so they are mapped into [16, 23].
#define RARCH_FIRST_CUSTOM_BIND 16
#define RARCH_FIRST_META_KEY RARCH_CUSTOM_BIND_LIST_END
enum // RetroArch specific bind IDs.
{
   // Custom binds that extend the scope of RETRO_DEVICE_JOYPAD for RetroArch specifically.
   // Analogs (RETRO_DEVICE_ANALOG)
   RARCH_ANALOG_LEFT_X_PLUS = RARCH_FIRST_CUSTOM_BIND,
   RARCH_ANALOG_LEFT_X_MINUS,
   RARCH_ANALOG_LEFT_Y_PLUS,
   RARCH_ANALOG_LEFT_Y_MINUS,
   RARCH_ANALOG_RIGHT_X_PLUS,
   RARCH_ANALOG_RIGHT_X_MINUS,
   RARCH_ANALOG_RIGHT_Y_PLUS,
   RARCH_ANALOG_RIGHT_Y_MINUS,

   // Turbo
   RARCH_TURBO_ENABLE,

   RARCH_CUSTOM_BIND_LIST_END,

   // Command binds. Not related to game input, only usable for port 0.
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
   RARCH_GRAB_MOUSE_TOGGLE,

   RARCH_MENU_TOGGLE,

   RARCH_BIND_LIST_END,
   RARCH_BIND_LIST_END_NULL
};

#ifdef RARCH_CONSOLE
enum // Console specific menu bind IDs.
{
   CONSOLE_MENU_FIRST      = 61,
   CONSOLE_MENU_A          = CONSOLE_MENU_FIRST,
   CONSOLE_MENU_B          = 62,
   CONSOLE_MENU_X          = 63,
   CONSOLE_MENU_Y          = 64,
   CONSOLE_MENU_START      = 65,
   CONSOLE_MENU_SELECT     = 66,
   CONSOLE_MENU_UP         = 67,
   CONSOLE_MENU_DOWN       = 68,
   CONSOLE_MENU_LEFT       = 69,
   CONSOLE_MENU_RIGHT      = 70,
   CONSOLE_MENU_L          = 71,
   CONSOLE_MENU_R          = 72,
   CONSOLE_MENU_L2         = 73,
   CONSOLE_MENU_R2         = 74,
   CONSOLE_MENU_L3         = 75,
   CONSOLE_MENU_R3         = 76,
   CONSOLE_MENU_HOME       = 77,
   CONSOLE_MENU_LAST       = CONSOLE_MENU_HOME,
};
#endif

struct retro_keybind
{
   bool valid;
   unsigned id;
   const char *desc;
   enum retro_key key;

   // PC only uses lower 16-bits.
   // Full 64-bit can be used for port-specific purposes, like simplifying multiple binds, etc.
   uint64_t joykey;

   // Default key binding value - for resetting bind to default
   uint64_t def_joykey;

   uint32_t joyaxis;
   uint32_t def_joyaxis;

   uint32_t orig_joyaxis; // Used by input_{push,pop}_analog_dpad().
};

struct platform_bind
{
   uint64_t joykey;
   char desc[64];
};

#if defined(_XBOX360)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_HLSL
#elif defined(__PSL1GHT__)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(__CELLOS_LV2__)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#elif defined(HAVE_OPENGLES2)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#else
#define DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

typedef struct video_info
{
   unsigned width;
   unsigned height;
   bool fullscreen;
   bool vsync;
   bool force_aspect;
   bool smooth;
   unsigned input_scale; // Maximum input size: RARCH_SCALE_BASE * input_scale
   bool rgb32; // Use 32-bit RGBA rather than native XBGR1555.
} video_info_t;

typedef struct audio_driver
{
   void *(*init)(const char *device, unsigned rate, unsigned latency);
   ssize_t (*write)(void *data, const void *buf, size_t size);
   bool (*stop)(void *data);
   bool (*start)(void *data);
   void (*set_nonblock_state)(void *data, bool toggle); // Should we care about blocking in audio thread? Fast forwarding.
   void (*free)(void *data);
   bool (*use_float)(void *data); // Defines if driver will take standard floating point samples, or int16_t samples.
   const char *ident;

   size_t (*write_avail)(void *data); // Optional
   size_t (*buffer_size)(void *data); // Optional
} audio_driver_t;

#define AXIS_NEG(x) (((uint32_t)(x) << 16) | UINT16_C(0xFFFF))
#define AXIS_POS(x) ((uint32_t)(x) | UINT32_C(0xFFFF0000))
#define AXIS_NONE UINT32_C(0xFFFFFFFF)
#define AXIS_DIR_NONE UINT16_C(0xFFFF)

#define AXIS_NEG_GET(x) (((uint32_t)(x) >> 16) & UINT16_C(0xFFFF))
#define AXIS_POS_GET(x) ((uint32_t)(x) & UINT16_C(0xFFFF))

#define NO_BTN UINT16_C(0xFFFF) // I hope no joypad will ever have this many buttons ... ;)

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
   ANALOG_DPAD_DUALANALOG,
   ANALOG_DPAD_LAST
};

enum keybind_set_id
{
   KEYBINDS_ACTION_NONE = 0,
   KEYBINDS_ACTION_SET_DEFAULT_BIND,
   KEYBINDS_ACTION_SET_DEFAULT_BINDS,
   KEYBINDS_ACTION_SET_ANALOG_DPAD_NONE,
   KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK,
   KEYBINDS_ACTION_SET_ANALOG_DPAD_RSTICK,
   KEYBINDS_ACTION_GET_BIND_LABEL,
   KEYBINDS_ACTION_LAST
};

typedef struct rarch_joypad_driver rarch_joypad_driver_t;

typedef struct input_driver
{
   void *(*init)(void);
   void (*poll)(void *data);
   int16_t (*input_state)(void *data, const struct retro_keybind **retro_keybinds,
         unsigned port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void *data, int key);
   void (*free)(void *data);
   void (*set_keybinds)(void *data, unsigned device, unsigned port, unsigned id, unsigned keybind_action);
   bool (*set_sensor_state)(void *data, unsigned port, enum retro_sensor_action action, unsigned rate);
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);
   uint64_t (*get_capabilities)(void *data);
   unsigned (*devices_size)(void *data);
   const char *ident;

   void (*grab_mouse)(void *data, bool state);
   bool (*set_rumble)(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t state);
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
   // FIXME: params for initialization - queries for resolution, framerate, color format
   // which might or might not be honored
   void *(*init)(const char *device, uint64_t buffer_types, unsigned width, unsigned height);
   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   // Polls the camera driver.
   // Will call the appropriate callback if a new frame is ready.
   // Returns true if a new frame was handled.
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

   bool (*get_position)(void *data, double *lat, double *lon, double *horiz_accuracy, double *vert_accuracy);
   void (*set_interval)(void *data, unsigned interval_msecs, unsigned interval_distance);
   const char *ident;
} location_driver_t;

struct rarch_viewport;

#ifdef HAVE_OVERLAY
typedef struct video_overlay_interface
{
   void (*enable)(void *data, bool state);
   bool (*load)(void *data, const struct texture_image *images, unsigned num_images);
   void (*tex_geom)(void *data, unsigned image, float x, float y, float w, float h);
   void (*vertex_geom)(void *data, unsigned image, float x, float y, float w, float h);
   void (*full_screen)(void *data, bool enable);
   void (*set_alpha)(void *data, unsigned image, float mod);
} video_overlay_interface_t;
#endif

struct font_params
{
   float x;
   float y;
   float scale;
   float drop_mod; // Drop shadow color multiplier.
   int drop_x, drop_y; // Drop shadow offset. If both are 0, no drop shadow will be rendered.
   uint32_t color; // ABGR. Use the macros.
   bool full_screen;
};
#define FONT_COLOR_RGBA(r, g, b, a) (((r) << 0) | ((g) << 8) | ((b) << 16) | ((a) << 24))
#define FONT_COLOR_GET_RED(col)   (((col) >>  0) & 0xff)
#define FONT_COLOR_GET_GREEN(col) (((col) >>  8) & 0xff)
#define FONT_COLOR_GET_BLUE(col)  (((col) >> 16) & 0xff)
#define FONT_COLOR_GET_ALPHA(col) (((col) >> 24) & 0xff)

// Optionally implemented interface to poke more deeply into video driver.
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
   void (*set_texture_frame)(void *data, const void *frame, bool rgb32, unsigned width, unsigned height, float alpha); // Update texture.
   void (*set_texture_enable)(void *data, bool enable, bool full_screen); // Enable/disable rendering.
#endif
   void (*set_osd_msg)(void *data, const char *msg, const struct font_params *params);

   void (*show_mouse)(void *data, bool state);
   void (*grab_mouse_toggle)(void *data);

   struct gfx_shader *(*get_current_shader)(void *data);
} video_poke_interface_t;

typedef struct video_driver
{
   void *(*init)(const video_info_t *video, const input_driver_t **input, void **input_data); 
   // Should the video driver act as an input driver as well? :)
   // The video initialization might preinitialize an input driver to override the settings in case the video driver relies on input driver for event handling, e.g.
   bool (*frame)(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg); // msg is for showing a message on the screen along with the video frame.
   void (*set_nonblock_state)(void *data, bool toggle); // Should we care about syncing to vblank? Fast forwarding.
   // Is the window still active?
   bool (*alive)(void *data);
   bool (*focus)(void *data); // Does the window have focus?
   bool (*set_shader)(void *data, enum rarch_shader_type type, const char *path); // Sets shader. Might not be implemented. Will be moved to poke_interface later.
   void (*free)(void *data);
   const char *ident;

   void (*set_rotation)(void *data, unsigned rotation);
   void (*viewport_info)(void *data, struct rarch_viewport *vp);

   // Reads out in BGR byte order (24bpp).
   bool (*read_viewport)(void *data, uint8_t *buffer);

#ifdef HAVE_OVERLAY
   void (*overlay_interface)(void *data, const video_overlay_interface_t **iface);
#endif
   void (*poke_interface)(void *data, const video_poke_interface_t **iface);
} video_driver_t;

enum rarch_display_type
{
   RARCH_DISPLAY_NONE = 0, // Non-bindable types like consoles, KMS, VideoCore, etc.
   RARCH_DISPLAY_X11, // video_display => Display*, video_window => Window
   RARCH_DISPLAY_WIN32, // video_display => N/A, video_window => HWND
   RARCH_DISPLAY_OSX // ?!
};

typedef struct driver
{
   const frontend_ctx_driver_t *frontend_ctx;
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
#ifdef HAVE_OSK
   const input_osk_driver_t *osk;
   void *osk_data;
#endif
#ifdef HAVE_CAMERA
   const camera_driver_t *camera;
   void *camera_data;
#endif
#ifdef HAVE_LOCATION
   const location_driver_t *location;
   void *location_data;
#endif
   void *audio_data;
   void *video_data;
   void *input_data;
#ifdef HAVE_MENU
   menu_handle_t *menu;
   const menu_ctx_driver_t *menu_ctx;
#endif

   bool threaded_video;

   // If set during context deinit, the driver should keep
   // graphics context alive to avoid having to reset all context state.
   bool video_cache_context;
   bool video_cache_context_ack; // Set to true by driver if context caching succeeded.

   // Set this to true if the platform in question needs to 'own' the respective
   // handle and therefore skip regular RetroArch driver teardown/reiniting procedure.
   // If set to true, the 'free' function will get skipped. It is then up to the
   // driver implementation to properly handle 'reiniting' inside the 'init' function
   // and make sure it returns the existing handle instead of allocating and returning
   // a pointer to a new handle.
   //
   // Typically, if a driver intends to make use of this, it should set this to true
   // at the end of its 'init' function.
   bool video_data_own;
   bool audio_data_own;
   bool input_data_own;
#ifdef HAVE_CAMERA
   bool camera_data_own;
#endif
#ifdef HAVE_LOCATION
   bool location_data_own;
#endif
#ifdef HAVE_OSK
   bool osk_data_own;
#endif
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
   bool nonblock_state;

   // Opaque handles to currently running window.
   // Used by e.g. input drivers which bind to a window.
   // Drivers are responsible for setting these if an input driver
   // could potentially make use of this.
   uintptr_t video_display;
   uintptr_t video_window;
   enum rarch_display_type display_type;

   // Used for 15-bit -> 16-bit conversions that take place before being passed to video driver.
   struct scaler_ctx scaler;
   void *scaler_out;

   // Graphics driver requires RGBA byte order data (ABGR on little-endian) for 32-bit.
   // This takes effect for overlay and shader cores that wants to load data into graphics driver.
   // Kinda hackish to place it here, it is only used for GLES.
   // TODO: Refactor this better.
   bool gfx_use_rgba;

#ifdef HAVE_OVERLAY
   input_overlay_t *overlay;
   input_overlay_state_t overlay_state;
#endif

   // Interface for "poking".
   const video_poke_interface_t *video_poke;

   // last message given to the video driver
   const char *current_msg;
} driver_t;

void init_drivers(void);
void init_drivers_pre(void);
void uninit_drivers(void);

void init_video_input(void);
void uninit_video_input(void);
void init_audio(void);
void uninit_audio(void);

void find_prev_resampler_driver(void);
void find_prev_video_driver(void);
void find_prev_audio_driver(void);
void find_prev_input_driver(void);
void find_next_video_driver(void);
void find_next_audio_driver(void);
void find_next_input_driver(void);
void find_next_resampler_driver(void);

#ifdef HAVE_CAMERA
void init_camera(void);
void uninit_camera(void);
void find_prev_camera_driver(void);
void find_next_camera_driver(void);
#endif

#ifdef HAVE_LOCATION
void init_location(void);
void uninit_location(void);
void find_prev_location_driver(void);
void find_next_location_driver(void);
#endif

void driver_set_monitor_refresh_rate(float hz);
bool driver_monitor_fps_statistics(double *refresh_rate, double *deviation, unsigned *sample_points);
void driver_set_nonblock_state(bool nonblock);

// Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
uintptr_t driver_get_current_framebuffer(void);
retro_proc_address_t driver_get_proc_address(const char *sym);

// Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE
bool driver_set_rumble_state(unsigned port, enum retro_rumble_effect effect, uint16_t strength);
// Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE
bool driver_set_sensor_state(unsigned port, enum retro_sensor_action action, unsigned rate);
float driver_sensor_get_input(unsigned port, unsigned action);

#ifdef HAVE_DYLIB
void rarch_deinit_filter(void);
void rarch_init_filter(enum retro_pixel_format);
#endif

#ifdef HAVE_FILTERS_BUILTIN
unsigned dspfilter_get_last_idx(void);
#endif

const char *rarch_dspfilter_get_name(void *data);

// Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE
#ifdef HAVE_CAMERA
bool driver_camera_start(void);
void driver_camera_stop(void);
void driver_camera_poll(void);
#endif

// Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE
#ifdef HAVE_LOCATION
bool driver_location_start(void);
void driver_location_stop(void);
bool driver_location_get_position(double *lat, double *lon, double *horiz_accuracy, double *vert_accuracy);
void driver_location_set_interval(unsigned interval_msecs, unsigned interval_distance);
#endif

#ifdef HAVE_MENU
const void *menu_ctx_find_driver(const char *ident); // Finds driver with ident. Does not initialize.
void find_prev_menu_driver(void);
void find_next_menu_driver(void);
void find_menu_driver(void);
#endif

// Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO
bool driver_update_system_av_info(const struct retro_system_av_info *info);

extern driver_t driver;

//////////////////////////////////////////////// Backends
extern const audio_driver_t audio_rsound;
extern const audio_driver_t audio_oss;
extern const audio_driver_t audio_alsa;
extern const audio_driver_t audio_alsathread;
extern const audio_driver_t audio_roar;
extern const audio_driver_t audio_openal;
extern const audio_driver_t audio_opensl;
extern const audio_driver_t audio_jack;
extern const audio_driver_t audio_sdl;
extern const audio_driver_t audio_xa;
extern const audio_driver_t audio_pulse;
extern const audio_driver_t audio_dsound;
extern const audio_driver_t audio_coreaudio;
extern const audio_driver_t audio_xenon360;
extern const audio_driver_t audio_ps3;
extern const audio_driver_t audio_gx;
extern const audio_driver_t audio_psp1;
extern const audio_driver_t audio_rwebaudio;
extern const audio_driver_t audio_null;
extern const video_driver_t video_gl;
extern const video_driver_t video_psp1;
extern const video_driver_t video_vita;
extern const video_driver_t video_d3d;
extern const video_driver_t video_gx;
extern const video_driver_t video_xenon360;
extern const video_driver_t video_xvideo;
extern const video_driver_t video_xdk_d3d;
extern const video_driver_t video_sdl;
extern const video_driver_t video_vg;
extern const video_driver_t video_null;
extern const video_driver_t video_omap;
extern const video_driver_t video_exynos;
extern const input_driver_t input_android;
extern const input_driver_t input_sdl;
extern const input_driver_t input_dinput;
extern const input_driver_t input_x;
extern const input_driver_t input_ps3;
extern const input_driver_t input_psp;
extern const input_driver_t input_xenon360;
extern const input_driver_t input_gx;
extern const input_driver_t input_xinput;
extern const input_driver_t input_linuxraw;
extern const input_driver_t input_udev;
extern const input_driver_t input_apple;
extern const input_driver_t input_qnx;
extern const input_driver_t input_rwebinput;
extern const input_driver_t input_null;
extern const camera_driver_t camera_v4l2;
extern const camera_driver_t camera_android;
extern const camera_driver_t camera_rwebcam;
extern const camera_driver_t camera_ios;
extern const location_driver_t location_apple;
extern const location_driver_t location_android;
extern const input_osk_driver_t input_ps3_osk;

extern const menu_ctx_driver_t menu_ctx_rmenu;
extern const menu_ctx_driver_t menu_ctx_rmenu_xui;
extern const menu_ctx_driver_t menu_ctx_rgui;
extern const menu_ctx_driver_t menu_ctx_lakka;

extern const menu_ctx_driver_backend_t menu_ctx_backend_common;
extern const menu_ctx_driver_backend_t menu_ctx_backend_lakka;

#ifdef HAVE_FILTERS_BUILTIN
extern const struct softfilter_implementation *blargg_ntsc_snes_rf_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *blargg_ntsc_snes_composite_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *blargg_ntsc_snes_svideo_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *blargg_ntsc_snes_rgb_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *lq2x_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *phosphor2x_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *twoxbr_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *epx_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *twoxsai_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *supereagle_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *supertwoxsai_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *twoxbr_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *darken_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *scale2x_get_implementation(softfilter_simd_mask_t simd);
#endif

#include "driver_funcs.h"

#ifdef __cplusplus
}
#endif

#endif

