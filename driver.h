/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "libretro.h"
#include <stdlib.h>
#include <stdint.h>
#include "msvc/msvc_compat.h"
#include "gfx/scaler/scaler.h"
#include "input/overlay.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
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
   // Turbo
   RARCH_TURBO_ENABLE = RARCH_FIRST_CUSTOM_BIND, 

   // Analogs (RETRO_DEVICE_ANALOG)
   RARCH_ANALOG_LEFT_X_PLUS,
   RARCH_ANALOG_LEFT_X_MINUS,
   RARCH_ANALOG_LEFT_Y_PLUS,
   RARCH_ANALOG_LEFT_Y_MINUS,
   RARCH_ANALOG_RIGHT_X_PLUS,
   RARCH_ANALOG_RIGHT_X_MINUS,
   RARCH_ANALOG_RIGHT_Y_PLUS,
   RARCH_ANALOG_RIGHT_Y_MINUS,
#ifdef RARCH_CONSOLE
   RARCH_ANALOG_LEFT_X_DPAD_LEFT,
   RARCH_ANALOG_LEFT_X_DPAD_RIGHT,
   RARCH_ANALOG_LEFT_Y_DPAD_UP,
   RARCH_ANALOG_LEFT_Y_DPAD_DOWN,
   RARCH_ANALOG_RIGHT_X_DPAD_LEFT,
   RARCH_ANALOG_RIGHT_X_DPAD_RIGHT,
   RARCH_ANALOG_RIGHT_Y_DPAD_UP,
   RARCH_ANALOG_RIGHT_Y_DPAD_DOWN,
#endif

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
   RARCH_DSP_CONFIG,
   RARCH_MUTE,
   RARCH_NETPLAY_FLIP,
   RARCH_SLOWMOTION,
   RARCH_ENABLE_HOTKEY,
   RARCH_VOLUME_UP,
   RARCH_VOLUME_DOWN,
   RARCH_OVERLAY_NEXT,
   RARCH_DISK_EJECT_TOGGLE,
   RARCH_DISK_NEXT,

   RARCH_MENU_TOGGLE,
   RARCH_MENU_QUICKMENU_TOGGLE,

   RARCH_BIND_LIST_END,
   RARCH_BIND_LIST_END_NULL
};

struct retro_keybind
{
   bool valid;
   int id;
   const char *desc;
   enum retro_key key;

   // PC only uses lower 16-bits.
   // Full 64-bit can be used for port-specific purposes, like simplifying multiple binds, etc.
   uint64_t joykey;

   // Default key binding value - for resetting bind to default
   uint64_t def_joykey;

   uint32_t joyaxis;
};

struct platform_bind
{
   uint64_t joykey;
   char desc[64];
};

enum rarch_shader_type
{
   RARCH_SHADER_CG,
   RARCH_SHADER_HLSL,
   RARCH_SHADER_GLSL,
   RARCH_SHADER_AUTO,
   RARCH_SHADER_NONE
};

enum rarch_shader_index
{
   RARCH_SHADER_INDEX_MULTIPASS = 0,
   RARCH_SHADER_INDEX_PASS0 = 1,
   RARCH_SHADER_INDEX_PASS1 = 2
};

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

enum input_devices
{
#if defined(ANDROID)
   DEVICE_NONE = 0,
   DEVICE_LOGITECH_RUMBLEPAD2,
   DEVICE_LOGITECH_DUAL_ACTION,
   DEVICE_ICONTROLPAD_BLUEZ_IME,
   DEVICE_TTT_THT_ARCADE,
   DEVICE_TOMMO_NEOGEOX_ARCADE,
   DEVICE_MADCATZ_PC_USB_STICK,
   DEVICE_LOGICOOL_RUMBLEPAD2,
   DEVICE_IDROID_X360,
   DEVICE_ZEEMOTE_STEELSERIES,
   DEVICE_HUIJIA_USB_SNES,
   DEVICE_SUPER_SMARTJOY,
   DEVICE_SAITEK_RUMBLE_P480,
   DEVICE_MS_SIDEWINDER_DUAL_STRIKE,
   DEVICE_MS_SIDEWINDER,
   DEVICE_MS_XBOX,
   DEVICE_WISEGROUP_PLAYSTATION2,
   DEVICE_JCPS102_PLAYSTATION2,
   DEVICE_GENERIC_PLAYSTATION2_CONVERTER,
   DEVICE_PSMOVE_NAVI,
   DEVICE_JXD_S7300B,
   DEVICE_IDROID_CON,
   DEVICE_GENIUS_MAXFIRE_G08XU,
   DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD,
   DEVICE_BUFFALO_BGC_FC801,
   DEVICE_RETROUSB_RETROPAD,
   DEVICE_RETROUSB_SNES_RETROPORT,
   DEVICE_CYPRESS_USB,
   DEVICE_MAYFLASH_WII_CLASSIC,
   DEVICE_SZMY_POWER_DUAL_BOX_WII,
   DEVICE_ARCHOS_GAMEPAD,
   DEVICE_JXD_S5110,
   DEVICE_XPERIA_PLAY,
   DEVICE_BROADCOM_BLUETOOTH_HID,
   DEVICE_THRUST_PREDATOR,
   DEVICE_DRAGONRISE,
   DEVICE_PLAYSTATION3_VERSION1,
   DEVICE_PLAYSTATION3_VERSION2,
   DEVICE_MOGA_IME,
   DEVICE_NYKO_PLAYPAD_PRO,
   DEVICE_TOODLES_2008_CHIMP,
   DEVICE_MOGA,
   DEVICE_CCPCREATIONS_WIIUSE_IME,
   DEVICE_KEYBOARD_RETROPAD,
#elif defined(GEKKO)
   DEVICE_GAMECUBE = 0,
#ifdef HW_RVL
   DEVICE_WIIMOTE,
   DEVICE_NUNCHUK,
   DEVICE_CLASSIC,
#endif
#elif defined(_XBOX)
   DEVICE_XBOX_PAD = 0,
#elif defined(__CELLOS_LV2__)
   DEVICE_SIXAXIS = 0,
#endif
   DEVICE_LAST
};

enum analog_dpad_bind_enums
{
   ANALOG_DPAD_NONE = 0,
   ANALOG_DPAD_LSTICK,
   ANALOG_DPAD_RSTICK,
   ANALOG_DPAD_LAST
};

enum keybind_set_id
{
   KEYBINDS_ACTION_NONE = 0,
   KEYBINDS_ACTION_DECREMENT_BIND,
   KEYBINDS_ACTION_INCREMENT_BIND,
   KEYBINDS_ACTION_SET_DEFAULT_BIND,
   KEYBINDS_ACTION_SET_DEFAULT_BINDS,
   KEYBINDS_ACTION_SET_ANALOG_DPAD_NONE,
   KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK,
   KEYBINDS_ACTION_SET_ANALOG_DPAD_RSTICK,
   KEYBINDS_ACTION_GET_BIND_LABEL,
   KEYBINDS_ACTION_LAST
};

typedef struct input_driver
{
   void *(*init)(void);
   void (*poll)(void *data);
   int16_t (*input_state)(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void *data, int key);
   void (*free)(void *data);
   void (*set_keybinds)(void *data, unsigned device, unsigned port, unsigned id, unsigned keybind_action);
   const char *ident;
} input_driver_t;

struct rarch_viewport;

#ifdef HAVE_OVERLAY
typedef struct video_overlay_interface
{
   void (*enable)(void *data, bool state);
   bool (*load)(void *data, const uint32_t *image, unsigned width, unsigned height);
   void (*tex_geom)(void *data, float x, float y, float w, float h);
   void (*vertex_geom)(void *data, float x, float y, float w, float h);
   void (*full_screen)(void *data, bool enable);
   void (*set_alpha)(void *data, float mod);
} video_overlay_interface_t;
#endif

// Optionally implemented interface to poke more deeply into video driver.
// Only used by RGUI atm.
typedef struct video_poke_interface
{
   void (*set_blend)(void *data, bool enable);
   void (*set_filtering)(void *data, unsigned index, bool smooth);
#ifdef HAVE_FBO
   void (*set_fbo_state)(void *data, unsigned state);
   unsigned (*get_fbo_state)(void *data);
   uintptr_t (*get_current_framebuffer)(void *data);
   retro_proc_address_t (*get_proc_address)(void *data, const char *sym);
#endif
   void (*set_aspect_ratio)(void *data, unsigned aspectratio_index);
   void (*apply_state_changes)(void *data);

   // Set to NULL if RGUI texture is not supposed to be rendered.
#ifdef HAVE_RGUI
   void (*set_rgui_texture)(void *data, const void *frame);
#endif
   void (*set_osd_msg)(void *data, const char *msg, void *userdata);
} video_poke_interface_t;

typedef struct video_driver
{
   void *(*init)(const video_info_t *video, const input_driver_t **input, void **input_data); 
   // Should the video driver act as an input driver as well? :)
   // The video init might preinitialize an input driver to override the settings in case the video driver relies on input driver for event handling, e.g.
   bool (*frame)(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg); // msg is for showing a message on the screen along with the video frame.
   void (*set_nonblock_state)(void *data, bool toggle); // Should we care about syncing to vblank? Fast forwarding.
   // Is the window still active?
   bool (*alive)(void *data);
   bool (*focus)(void *data); // Does the window have focus?
   bool (*set_shader)(void *data, enum rarch_shader_type type, const char *path, unsigned index); // Sets shader. Might not be implemented.
   void (*free)(void *data);
   const char *ident;

#if defined(HAVE_RMENU) || defined(HAVE_RGUI)
   void (*start)(void);
   void (*stop)(void);
   void (*restart)(void);
#endif

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
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
   void *audio_data;
   void *video_data;
   void *input_data;

   bool threaded_video;

   // Set if the respective handles are owned by RetroArch driver core.
   // Consoles upper logic will generally intialize the drivers before
   // the driver core initializes. It will then be up to upper logic
   // to finally free() up the driver handles.
   // Driver core will still call init() and free(), but in this case
   // these calls should be seen as "reinit() + ref_count++" and "ref_count--"
   // respectively.
   bool video_data_own;
   bool audio_data_own;
   bool input_data_own;

#ifdef HAVE_COMMAND
   rarch_cmd_t *command;
#endif
   bool stdin_claimed;
   bool block_hotkey;

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
   uint64_t overlay_state;
#endif

   // Interface for "poking".
   const video_poke_interface_t *video_poke;

   // last message given to the video driver
   const char *current_msg;
} driver_t;

void init_drivers(void);
void init_drivers_pre(void);
void uninit_drivers(void);

void global_init_drivers(void);
void global_uninit_drivers(void);

void init_video_input(void);
void uninit_video_input(void);
void init_audio(void);
void uninit_audio(void);

void driver_set_monitor_refresh_rate(float hz);

// Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
uintptr_t driver_get_current_framebuffer(void);
retro_proc_address_t driver_get_proc_address(const char *sym);

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
extern const audio_driver_t audio_null;
extern const video_driver_t video_gl;
extern const video_driver_t video_psp1;
extern const video_driver_t video_vita;
extern const video_driver_t video_d3d9;
extern const video_driver_t video_gx;
extern const video_driver_t video_xenon360;
extern const video_driver_t video_xvideo;
extern const video_driver_t video_xdk_d3d;
extern const video_driver_t video_sdl;
extern const video_driver_t video_vg;
extern const video_driver_t video_null;
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
extern const input_driver_t input_ios;
extern const input_driver_t input_null;

#include "driver_funcs.h"

#endif

