/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __DRIVER__H
#define __DRIVER__H

#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

enum
{
   SSNES_FAST_FORWARD_KEY = 0x666, // Hurr, durr
   SSNES_LOAD_STATE_KEY,
   SSNES_SAVE_STATE_KEY,
   SSNES_FULLSCREEN_TOGGLE_KEY,
   SSNES_QUIT_KEY,
   SSNES_STATE_SLOT_PLUS,
   SSNES_STATE_SLOT_MINUS,
   SSNES_AUDIO_INPUT_RATE_PLUS,
   SSNES_AUDIO_INPUT_RATE_MINUS,
   SSNES_REWIND,
   SSNES_MOVIE_RECORD_TOGGLE,
   SSNES_PAUSE_TOGGLE,
};


struct snes_keybind
{
   int id;
   uint16_t key;
   uint16_t joykey;
   uint32_t joyaxis;
};

typedef struct video_info
{
   int width;
   int height;
   bool fullscreen;
   bool vsync;
   bool force_aspect;
   bool smooth;
   int input_scale; // HQ2X => 2, HQ4X => 4, None => 1
   bool rgb32; // Use 32-bit RGBA rather than native XBGR1555.
} video_info_t;

typedef struct audio_driver
{
   void* (*init)(const char* device, int rate, int latency);
   ssize_t (*write)(void* data, const void* buf, size_t size);
   bool (*stop)(void* data);
   bool (*start)(void* data);
   void (*set_nonblock_state)(void* data, bool toggle); // Should we care about blocking in audio thread? Fast forwarding.
   void (*free)(void* data);
   bool (*use_float)(void *data); // Defines if driver will take standard floating point samples, or int16_t samples.
   const char *ident;
} audio_driver_t;

#define AXIS_NEG(x) (((uint32_t)(x) << 16) | 0xFFFFU)
#define AXIS_POS(x) ((uint32_t)(x) | 0xFFFF0000U)
#define AXIS_NONE (0xFFFFFFFFU)

#define AXIS_NEG_GET(x) (((uint32_t)(x) >> 16) & 0xFFFFU)
#define AXIS_POS_GET(x) ((uint32_t)(x) & 0xFFFFU)

#define NO_BTN ((uint16_t)0xFFFFU) // I hope no joypad will ever have this many buttons ... ;)

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
   void* (*init)(void);
   void (*poll)(void* data);
   int16_t (*input_state)(void* data, const struct snes_keybind **snes_keybinds, bool port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void* data, int key);
   void (*free)(void* data);
   const char *ident;
} input_driver_t;

typedef struct video_driver
{
   void* (*init)(video_info_t *video, const input_driver_t **input, void **input_data); 
   // Should the video driver act as an input driver as well? :) The video init might preinitialize an input driver to override the settings in case the video driver relies on input driver for event handling, e.g.
   bool (*frame)(void* data, const void* frame, unsigned width, unsigned height, unsigned pitch, const char *msg); // msg is for showing a message on the screen along with the video frame.
   void (*set_nonblock_state)(void* data, bool toggle); // Should we care about syncing to vblank? Fast forwarding.
   // Is the window still active?
   bool (*alive)(void *data);
   bool (*focus)(void *data); // Does the window have focus?
   void (*free)(void* data);
   const char *ident;
} video_driver_t;


typedef struct driver
{
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
   void *audio_data;
   void *video_data;
   void *input_data;
} driver_t;

void init_drivers(void);
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
extern const video_driver_t video_gl;
extern const video_driver_t video_xvideo;
extern const input_driver_t input_sdl;
extern const input_driver_t input_x;
////////////////////////////////////////////////

#endif
