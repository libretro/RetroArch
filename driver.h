/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include <stdint.h>
#include "msvc/msvc_compat.h"
#include "input/keysym.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORK_CMD
#include "network_cmd.h"
#endif

#define AUDIO_CHUNK_SIZE_BLOCKING 64
#define AUDIO_CHUNK_SIZE_NONBLOCKING 2048 // So we don't get complete line-noise when fast-forwarding audio.
#define AUDIO_MAX_RATIO 16

// libretro has 16 buttons from 0-15 (libretro.h)
#define RARCH_FIRST_META_KEY 16
enum
{
   RARCH_FAST_FORWARD_KEY = RARCH_FIRST_META_KEY,
   RARCH_FAST_FORWARD_HOLD_KEY,
   RARCH_LOAD_STATE_KEY,
   RARCH_SAVE_STATE_KEY,
   RARCH_FULLSCREEN_TOGGLE_KEY,
   RARCH_QUIT_KEY,
   RARCH_STATE_SLOT_PLUS,
   RARCH_STATE_SLOT_MINUS,
   RARCH_AUDIO_INPUT_RATE_PLUS,
   RARCH_AUDIO_INPUT_RATE_MINUS,
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

#ifdef RARCH_CONSOLE
   RARCH_CHEAT_INPUT,
   RARCH_SRAM_WRITE_PROTECT,
#endif

   RARCH_BIND_LIST_END,
   RARCH_BIND_LIST_END_NULL
};

struct snes_keybind
{
   bool valid;
   int id;
   enum rarch_key key;

   // PC only uses lower 16-bits.
   // Full 64-bit can be used for port-specific purposes, like simplifying multiple binds, etc.
   uint64_t joykey;

   uint32_t joyaxis;
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

#define HAT_UP_MASK (1 << 15)
#define HAT_DOWN_MASK (1 << 14)
#define HAT_LEFT_MASK (1 << 13)
#define HAT_RIGHT_MASK (1 << 12)
#define HAT_MAP(x, hat) ((x & ((1 << 12) - 1)) | hat)

#define HAT_MASK (HAT_UP_MASK | HAT_DOWN_MASK | HAT_LEFT_MASK | HAT_RIGHT_MASK)
#define GET_HAT_DIR(x) (x & HAT_MASK)
#define GET_HAT(x) (x & (~HAT_MASK))

typedef struct input_driver
{
   void *(*init)(void);
   void (*poll)(void *data);
   int16_t (*input_state)(void *data, const struct snes_keybind **snes_keybinds, unsigned port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void *data, int key);
   void (*free)(void *data);
   const char *ident;
} input_driver_t;

typedef struct video_driver
{
   void *(*init)(const video_info_t *video, const input_driver_t **input, void **input_data); 
   // Should the video driver act as an input driver as well? :) The video init might preinitialize an input driver to override the settings in case the video driver relies on input driver for event handling, e.g.
   bool (*frame)(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg); // msg is for showing a message on the screen along with the video frame.
   void (*set_nonblock_state)(void *data, bool toggle); // Should we care about syncing to vblank? Fast forwarding.
   // Is the window still active?
   bool (*alive)(void *data);
   bool (*focus)(void *data); // Does the window have focus?
   bool (*xml_shader)(void *data, const char *path); // Sets XML-shader. Might not be implemented.
   void (*free)(void *data);
   const char *ident;

   // Callbacks essentially useless on PC, but useful on consoles where the drivers are used for more stuff.
#ifdef RARCH_CONSOLE
   void (*start)(void);
   void (*stop)(void);
   void (*restart)(void);
#endif

   void (*set_rotation)(void *data, unsigned rotation);
} video_driver_t;

typedef struct driver
{
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
   void *audio_data;
   void *video_data;
   void *input_data;

#ifdef HAVE_NETWORK_CMD
   network_cmd_t *network_cmd;
#endif
} driver_t;

void init_drivers(void);
void init_drivers_pre(void);
void uninit_drivers(void);

void init_video_input(void);
void uninit_video_input(void);
void init_audio(void);
void uninit_audio(void);

extern driver_t driver;

//////////////////////////////////////////////// Backends
extern const audio_driver_t audio_rsound;
extern const audio_driver_t audio_oss;
extern const audio_driver_t audio_alsa;
extern const audio_driver_t audio_roar;
extern const audio_driver_t audio_openal;
extern const audio_driver_t audio_jack;
extern const audio_driver_t audio_sdl;
extern const audio_driver_t audio_xa;
extern const audio_driver_t audio_pulse;
extern const audio_driver_t audio_ext;
extern const audio_driver_t audio_dsound;
extern const audio_driver_t audio_coreaudio;
extern const audio_driver_t audio_xenon360;
extern const audio_driver_t audio_xdk360;
extern const audio_driver_t audio_ps3;
extern const audio_driver_t audio_wii;
extern const video_driver_t video_gl;
extern const video_driver_t video_wii;
extern const video_driver_t video_xenon360;
extern const video_driver_t video_xvideo;
extern const video_driver_t video_xdk360;
extern const video_driver_t video_sdl;
extern const video_driver_t video_rpi;
extern const video_driver_t video_ext;
extern const input_driver_t input_sdl;
extern const input_driver_t input_x;
extern const input_driver_t input_ps3;
extern const input_driver_t input_xenon360;
extern const input_driver_t input_wii;
extern const input_driver_t input_xdk360;
extern const input_driver_t input_linuxraw;
////////////////////////////////////////////////

// Convenience macros.

#ifdef HAVE_GRIFFIN
#include "console/griffin/hook.h"
#else
#define audio_init_func(device, rate, latency)  driver.audio->init(device, rate, latency)
#define audio_write_func(buf, size)             driver.audio->write(driver.audio_data, buf, size)
#define audio_stop_func()                       driver.audio->stop(driver.audio_data)
#define audio_start_func()                      driver.audio->start(driver.audio_data)
#define audio_set_nonblock_state_func(state)    driver.audio->set_nonblock_state(driver.audio_data, state)
#define audio_free_func()                       driver.audio->free(driver.audio_data)
#define audio_use_float_func()                  driver.audio->use_float(driver.audio_data)
#define audio_write_avail_func()                driver.audio->write_avail(driver.audio_data)
#define audio_buffer_size_func()                driver.audio->buffer_size(driver.audio_data)

#define video_init_func(video_info, input, input_data) \
                                                driver.video->init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                driver.video->frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      driver.video->alive(driver.video_data)
#define video_focus_func()                      driver.video->focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_set_rotation_func(rotate)         driver.video->set_rotation(driver.video_data, rotate)
#define video_set_aspect_ratio_func(aspect_idx) driver.video->set_aspect_ratio(driver.video_data, aspect_idx)
#define video_free_func()                       driver.video->free(driver.video_data)

#define input_init_func()                       driver.input->init()
#define input_poll_func()                       driver.input->poll(driver.input_data)
#define input_input_state_func(snes_keybinds, port, device, index, id) \
                                                driver.input->input_state(driver.input_data, snes_keybinds, port, device, index, id)
#define input_free_func()                       driver.input->free(driver.input_data)

static inline bool input_key_pressed_func(int key)
{
   bool ret = driver.input->key_pressed(driver.input_data, key);
#ifdef HAVE_NETWORK_CMD
   if (!ret && driver.network_cmd)
      ret = network_cmd_get(driver.network_cmd, key);
#endif
   return ret;
}
#endif

#endif

