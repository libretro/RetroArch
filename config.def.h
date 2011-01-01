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

#ifndef __CONFIG_H
#define __CONFIG_H

#include <GL/glfw.h>
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

#define VIDEO_DEFAULT_DRIVER VIDEO_GL
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA


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
   { SNES_DEVICE_ID_JOYPAD_A,             'X',          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,             'Z',          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,             'S',          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,             'A',          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,             'Q',          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,             'W',          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,    GLFW_KEY_LEFT,     11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,   GLFW_KEY_RIGHT,    12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,      GLFW_KEY_UP,       13,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,    GLFW_KEY_DOWN,     14,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_START,   GLFW_KEY_ENTER,     7,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,  GLFW_KEY_RSHIFT,    6,       AXIS_NONE },
   { SNES_FAST_FORWARD_KEY,         GLFW_KEY_SPACE,    10,       AXIS_NONE },
   { -1 }
};

// Player 2
static const struct snes_keybind snes_keybinds_2[] = {
   // SNES button                 |   keyboard key   | js btn | js axis |
   { SNES_DEVICE_ID_JOYPAD_A,             'B',          1,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_B,             'V',          0,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_X,             'G',          3,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_Y,             'F',          2,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_L,             'R',          4,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_R,             'T',          5,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_LEFT,          'J',         11,       AXIS_NEG(0) },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,         'L',         12,       AXIS_POS(0) },
   { SNES_DEVICE_ID_JOYPAD_UP,            'I',         13,       AXIS_POS(1) },
   { SNES_DEVICE_ID_JOYPAD_DOWN,          'K',         14,       AXIS_NEG(1) },
   { SNES_DEVICE_ID_JOYPAD_START,         'P',          6,       AXIS_NONE },
   { SNES_DEVICE_ID_JOYPAD_SELECT,        'O',          7,       AXIS_NONE },
   { -1 }
};

///// Save state
#define SAVE_STATE_KEY GLFW_KEY_F2
///// Load state
#define LOAD_STATE_KEY GLFW_KEY_F4

//// Toggles between fullscreen and windowed mode.
#define TOGGLE_FULLSCREEN 'F'



#endif

