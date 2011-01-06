/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

#include <SDL/SDL.h>
#include <stdbool.h>
#include "libsnes.hpp"
#include "driver.h"
#include <samplerate.h>


///////////////// Drivers
#define VIDEO_GL 0
////////////////////////
#define AUDIO_RSOUND 1
#define AUDIO_OSS 2
#define AUDIO_ALSA 3
#define AUDIO_ROAR 4
#define AUDIO_AL 5
#define AUDIO_JACK 6
////////////////////////
#define INPUT_SDL 7
////////////////////////

#define VIDEO_DEFAULT_DRIVER VIDEO_GL
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA
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

////////////////
// Audio
////////////////

// Will enable audio or not.
static const bool audio_enable = true;

// Output samplerate
static const unsigned out_rate = 48000; 

// Input samplerate from libSNES. 
// Lower this (slightly) if you are experiencing frequent audio dropouts while vsync is enabled.
static const unsigned in_rate = 31950; 

// Audio device (e.g. hw:0,0 or /dev/audio). If NULL, will use defaults.
static const char* audio_device = NULL;

// Desired audio latency in milliseconds. Might not be honored if driver can't provide given latency.
static const int out_latency = 64;

// Will sync audio. (recommended) 
static const bool audio_sync = true;

// Defines the quality (and cpu reqirements) of samplerate conversion.
#define SAMPLERATE_QUALITY SRC_LINEAR




////////////////////
// Keybinds, Joypad
////////////////////

// Axis threshold (between 0.0 and 1.0)
// How far an axis must be tilted to result in a button press
#define AXIS_THRESHOLD 0.8

#define AXIS_NEG(x) ((uint32_t)(x << 16) | 0xFFFF)
#define AXIS_POS(x) ((uint32_t)(x) | 0xFFFF0000U)
#define AXIS_NONE ((uint32_t)0xFFFFFFFFU)

// To figure out which joypad buttons to use, check jstest or similar.
// Axes are configured using the axis number for the positive (up, right)
// direction and the number's two's-complement (~) for negative directions.
// To use the axis, set the button to -1.

// Player 1
static const struct snes_keybind snes_keybinds_1[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,          SDLK_x,          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,          SDLK_z,          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,          SDLK_s,          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,          SDLK_a,          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,          SDLK_q,          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,          SDLK_w,          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,       SDLK_LEFT,      11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,      SDLK_RIGHT,     12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_UP,        13,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_DOWN,      14,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_RETURN,     7,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_RSHIFT,     6,       AXIS_NONE },
   { SSNES_FAST_FORWARD_KEY,           SDLK_SPACE,     10,       AXIS_NONE },
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
   { SNES_DEVICE_ID_JOYPAD_UP,         SDLK_i,         13,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,       SDLK_k,         14,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_START,      SDLK_p,          6,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,     SDLK_o,          7,       AXIS_NONE },
   { -1 }
};

///// Save state
#define SAVE_STATE_KEY SDLK_F2
///// Load state
#define LOAD_STATE_KEY SDLK_F4

//// Toggles between fullscreen and windowed mode.
#define TOGGLE_FULLSCREEN SDLK_f


#endif

