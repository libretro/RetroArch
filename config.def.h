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

/// Config header for SSNES
//
//

#ifndef __CONFIG_DEF_H
#define __CONFIG_DEF_H

#include <stdbool.h>
#include "libsnes.hpp"
#include "driver.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SDL
#include "SDL.h"
#else
#error HAVE_SDL is not defined!
#endif

#ifdef HAVE_SRC
#include <samplerate.h>
#endif


///////////////// Drivers
#define VIDEO_GL 0
////////////////////////
#define AUDIO_RSOUND 1
#define AUDIO_OSS 2
#define AUDIO_ALSA 3
#define AUDIO_ROAR 4
#define AUDIO_AL 5
#define AUDIO_JACK 6
#define AUDIO_SDL 8
#define AUDIO_XAUDIO 9
#define AUDIO_PULSE 10
////////////////////////
#define INPUT_SDL 7
////////////////////////

#define VIDEO_DEFAULT_DRIVER VIDEO_GL

#if defined(HAVE_ALSA)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA
#elif defined(HAVE_PULSE)
#define AUDIO_DEFAULT_DRIVER AUDIO_PULSE
#elif defined(HAVE_OSS)
#define AUDIO_DEFAULT_DRIVER AUDIO_OSS
#elif defined(HAVE_JACK)
#define AUDIO_DEFAULT_DRIVER AUDIO_JACK
#elif defined(HAVE_AL)
#define AUDIO_DEFAULT_DRIVER AUDIO_AL
#elif defined(HAVE_SDL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SDL
#elif defined(HAVE_XAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_XAUDIO
#elif defined(HAVE_RSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_RSOUND
#elif defined(HAVE_ROAR)
#define AUDIO_DEFAULT_DRIVER AUDIO_ROAR
#else
#error Need at least one audio driver!
#endif

#define INPUT_DEFAULT_DRIVER INPUT_SDL


////////////////
// Video
////////////////

// Windowed
static const float xscale = 3.0; // Real x res = 296 * xscale
static const float yscale = 3.0; // Real y res = 224 * yscale

// Fullscreen
static const bool fullscreen = false;  // To start in Fullscreen or not
static const unsigned fullscreen_x = 1280;
static const unsigned fullscreen_y = 720;

// Video VSYNC (recommended)
static const bool vsync = true;

// Smooths picture
static const bool video_smooth = true;

// On resize and fullscreen, rendering area will stay 4:3
static const bool force_aspect = true; 

// Font size for on-screen messages.
static const unsigned font_size = 48;

// Offset for where messages will be placed on-screen. Values are in range [0.0, 1.0].
static const float message_pos_offset_x = 0.05;
static const float message_pos_offset_y = 0.05;

#define SNES_ASPECT_RATIO (4.0/3)

////////////////
// Audio
////////////////

// Will enable audio or not.
static const bool audio_enable = true;

// Output samplerate
static const unsigned out_rate = 48000; 

// Input samplerate from libSNES. 
// Lower this (slightly) if you are experiencing frequent audio dropouts while vsync is enabled.
static const float in_rate = 31980.0; 

// When changing input rate on-the-fly
static const float audio_rate_step = 0.25;

// Audio device (e.g. hw:0,0 or /dev/audio). If NULL, will use defaults.
static const char* audio_device = NULL;

// Desired audio latency in milliseconds. Might not be honored if driver can't provide given latency.
static const int out_latency = 64;

// Will sync audio. (recommended) 
static const bool audio_sync = true;

// Defines the quality (and cpu reqirements) of samplerate conversion.
#ifdef HAVE_SRC
#define SAMPLERATE_QUALITY SRC_LINEAR
#endif

// Enables use of rewind. This will incur some memory footprint depending on the save state buffer.
// This rewind only works when using bSNES core atm.
static const bool rewind_enable = false;

// The buffer size for the rewind buffer. This needs to be about 15-20MB per minute. Very game dependant.
static const unsigned rewind_buffer_size = 20 << 20; // 20MiB

// How many frames to rewind at a time.
static const unsigned rewind_granularity = 1;

// Pause gameplay when gameplay loses focus.
static const bool pause_nonactive = true;

// Saves non-volatile SRAM at a regular interval. It is measured in seconds. A value of 0 disables autosave.
static const unsigned autosave_interval = 0;


////////////////////
// Keybinds, Joypad
////////////////////

// Axis threshold (between 0.0 and 1.0)
// How far an axis must be tilted to result in a button press
#define AXIS_THRESHOLD 0.5

// To figure out which joypad buttons to use, check jstest or similar.
// SDL sometimes reverses the axes for some odd reason, but hey. :D

// Player 1
static const struct snes_keybind snes_keybinds_1[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,          SDLK_x,           1,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,          SDLK_z,           0,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,          SDLK_s,           3,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,          SDLK_a,           2,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,          SDLK_q,           4,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,          SDLK_w,           5,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,       SDLK_LEFT,       11,      AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,      SDLK_RIGHT,      12,      AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_UP,         13,      AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_DOWN,       14,      AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_RETURN,      7,      AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_RSHIFT,      6,      AXIS_NONE },
   { SSNES_FAST_FORWARD_KEY,           SDLK_SPACE,      10,      AXIS_NONE },
   { SSNES_SAVE_STATE_KEY,             SDLK_F2,     NO_BTN,      AXIS_NONE },
   { SSNES_LOAD_STATE_KEY,             SDLK_F4,     NO_BTN,      AXIS_NONE },
   { SSNES_FULLSCREEN_TOGGLE_KEY,      SDLK_f,      NO_BTN,      AXIS_NONE },
   { SSNES_QUIT_KEY,                   SDLK_ESCAPE, NO_BTN,      AXIS_NONE },
   { SSNES_STATE_SLOT_MINUS,           SDLK_F6,     NO_BTN,      AXIS_NONE },
   { SSNES_STATE_SLOT_PLUS,            SDLK_F7,     NO_BTN,      AXIS_NONE },
   { SSNES_AUDIO_INPUT_RATE_PLUS,      SDLK_KP_PLUS, NO_BTN,     AXIS_NONE },
   { SSNES_AUDIO_INPUT_RATE_MINUS,     SDLK_KP_MINUS, NO_BTN,    AXIS_NONE },
   { SSNES_REWIND,                     SDLK_r,      NO_BTN,      AXIS_NONE },
   { SSNES_MOVIE_RECORD_TOGGLE,        SDLK_o,      NO_BTN,      AXIS_NONE },
   { SSNES_PAUSE_TOGGLE,               SDLK_p,      NO_BTN,      AXIS_NONE },
   { -1 }
};

// Player 2
static const struct snes_keybind snes_keybinds_2[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,          SDLK_b,          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,          SDLK_v,          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,          SDLK_g,          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,          SDLK_f,          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,          SDLK_r,          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,          SDLK_t,          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,       SDLK_j,         11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,      SDLK_l,         12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_i,         13,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_k,         14,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_p,          6,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_o,          7,       AXIS_NONE },
   { -1 }
};

// Player 3
static const struct snes_keybind snes_keybinds_3[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,          SDLK_b,          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,          SDLK_v,          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,          SDLK_g,          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,          SDLK_f,          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,          SDLK_r,          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,          SDLK_t,          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,       SDLK_j,         11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,      SDLK_l,         12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_i,         13,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_k,         14,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_p,          6,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_o,          7,       AXIS_NONE },
   { -1 }
};

// Player 4
static const struct snes_keybind snes_keybinds_4[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,          SDLK_b,          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,          SDLK_v,          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,          SDLK_g,          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,          SDLK_f,          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,          SDLK_r,          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,          SDLK_t,          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,       SDLK_j,         11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,      SDLK_l,         12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_i,         13,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_k,         14,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_p,          6,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_o,          7,       AXIS_NONE },
   { -1 }
};

// Player 5
static const struct snes_keybind snes_keybinds_5[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,          SDLK_b,          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,          SDLK_v,          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,          SDLK_g,          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,          SDLK_f,          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,          SDLK_r,          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,          SDLK_t,          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,       SDLK_j,         11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,      SDLK_l,         12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_i,         13,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_k,         14,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_p,          6,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_o,          7,       AXIS_NONE },
   { -1 }
};

#endif

